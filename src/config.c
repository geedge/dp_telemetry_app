#include "config.h"
#include "job_ctx.h"
#include <MESA/MESA_prof_load.h>

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <unistd.h>

#define MAX_LCORE 128

extern struct mr_instance * mr_instance;
struct config * g_conf = NULL;

struct config * config_create()
{
    struct config * conf = calloc(1, sizeof(struct config));
    CHECK_OR_EXIT(conf, "calloc failed: %s", strerror(errno));

    g_conf = conf;
    return conf;
}

void config_load()
{
    if (access(g_conf->config_path, R_OK) != 0)
    {
        syslog(LOG_ERR, "Configure File %s is not existed. Failed. ", g_conf->config_path);
        exit(EXIT_FAILURE);
    }

    CPU_ZERO(&g_conf->cpu_set_io);
    unsigned int io_cores[MAX_LCORE] = {};
    int nr_io_cores = 0;
    nr_io_cores = MESA_load_profile_uint_range(g_conf->config_path, "global", "iocore",
                                               sizeof(io_cores) / sizeof(io_cores[0]), io_cores);
    for (unsigned int i = 0; i < nr_io_cores; i++)
    {
        CPU_SET(io_cores[i], &g_conf->cpu_set_io);
    }

    unsigned int loglevel = LOG_ERR;
    MESA_load_profile_uint_def(g_conf->config_path, "global", "loglevel", &loglevel, loglevel);
    if (!(loglevel <= LOG_DEBUG && loglevel >= LOG_EMERG))
    {
        syslog(LOG_ERR, "global loglevel Must between LOG_DEBUG(7) and LOG_EMERG(0)");
        exit(EXIT_FAILURE);
    }
    g_conf->log_level = loglevel;
}

void dynamic_config_load()
{
    if (access(g_conf->dy_config_path, R_OK) != 0)
    {
        syslog(LOG_ERR, "Dynamic configure file %s is not existed. Do not load dynamic configuration files. ",
               g_conf->dy_config_path);
        return;
    }

    syslog(LOG_INFO, "Loading data path trace configuration file...");

    int ret = 0;

    char dp_trace_dir[PATH_MAX] = "/tmp";
    MESA_load_profile_string_nodef(g_conf->dy_config_path, "dp_trace_rule", "dp_trace_dir", dp_trace_dir,
                                   sizeof(dp_trace_dir));
    if (dp_trace_dir[strlen(dp_trace_dir) - 1] == '/')
    {
        dp_trace_dir[strlen(dp_trace_dir) - 1] = '\0';
    }
    snprintf(g_conf->dp_trace_dir, sizeof(g_conf->dp_trace_dir), "%s", dp_trace_dir);

    unsigned int dp_trace_file_max_size_in_KB = 256 * 1024;
    MESA_load_profile_uint_nodef(g_conf->dy_config_path, "dp_trace_rule", "dp_trace_file_max_size_in_KB",
                                 &dp_trace_file_max_size_in_KB);
    g_conf->dp_trace_file_max_size_in_KB = dp_trace_file_max_size_in_KB;

    unsigned int dp_trace_merge_timeout = 30;
    MESA_load_profile_uint_nodef(g_conf->dy_config_path, "dp_trace_rule", "dp_trace_merge_timeout",
                                 &dp_trace_merge_timeout);
    g_conf->dp_trace_merge_timeout = dp_trace_merge_timeout;

    memset(g_conf->desc, 0, sizeof(g_conf->desc));
    for (int rule_index = 0; rule_index < DP_TRACE_JOB_NUM_MAX; rule_index++)
    {
        unsigned int enable = 1;
        char bpf_expr[MR_BPF_EXPRESSION_MAX] = {};
        unsigned int pkt_cnt_max = 0;
        unsigned int sampling = 1;
        unsigned int snaplen = UINT32_MAX;

        char dp_trace_section[MR_SYMBOL_MAX] = {};
        snprintf(dp_trace_section, sizeof(dp_trace_section), "dp_trace_rule:%d", rule_index);

        MESA_load_profile_uint_nodef(g_conf->dy_config_path, dp_trace_section, "enable", &enable);

        ret = MESA_load_profile_string_nodef(g_conf->dy_config_path, dp_trace_section, "bpf_expr", bpf_expr,
                                             sizeof(bpf_expr));
        if (ret < 0)
        {
            continue;
        }

        MESA_load_profile_uint_nodef(g_conf->dy_config_path, dp_trace_section, "pkt_cnt_max", &pkt_cnt_max);
        MESA_load_profile_uint_nodef(g_conf->dy_config_path, dp_trace_section, "sampling", &sampling);
        MESA_load_profile_uint_nodef(g_conf->dy_config_path, dp_trace_section, "snaplen", &snaplen);

        struct dp_trace_job_desc * desc = (struct dp_trace_job_desc *)&g_conf->desc;
        struct dp_trace_job_desc * desc_i = &desc[rule_index];
        desc_i->enable = (enable == 0) ? false : true;
        desc_i->rule_index = rule_index;

        snprintf(desc_i->bpf_expr, sizeof(desc_i->bpf_expr), "%s", bpf_expr);

        desc_i->pkt_cnt_max = pkt_cnt_max;
        desc_i->sampling = (sampling == 0) ? 1 : sampling;
        desc_i->snaplen = (snaplen == 0) ? UINT32_MAX : snaplen;
    }

    syslog(LOG_INFO, "Loading data path trace configuration file is completed.");
}

void dynamic_config_apply()
{
    job_rule_apply(g_conf->desc, TELEMETRY_DIM(g_conf->desc));
}

static void signal_handler(evutil_socket_t fd, short what, void * arg)
{

    switch (fd)
    {
    case SIGHUP:
        dynamic_config_load();
        dynamic_config_apply();
    default:
        syslog(LOG_WARNING, "Received unexpected signal: %d", fd);
    }
}

static void * dynamic_config_reload_thread_dispatch(void * arg)
{
    struct event_base * base = (struct event_base *)arg;
    event_base_dispatch(base);
    return NULL;
}

void dynamic_config_reload_init()
{
    static int signals[] = {SIGHUP};

    g_conf->evbase = event_base_new();

    CHECK_OR_EXIT(g_conf->evbase, "event_base_new failed");

    for (unsigned int i = 0; i < TELEMETRY_DIM(signals); i++)
    {
        g_conf->evsignal[i] = evsignal_new(g_conf->evbase, signals[i], signal_handler, NULL);
        CHECK_OR_EXIT(g_conf->evsignal[i], "create signal event failed. signal is:%d", signals[i]);
        evsignal_add(g_conf->evsignal[i], NULL);
    }

    pthread_t tid;
    if (pthread_create(&tid, NULL, dynamic_config_reload_thread_dispatch, g_conf->evbase) != 0)
    {
        syslog(LOG_ERR, "failed to create thread for event_base dispatch");
        exit(EXIT_FAILURE);
    }
}
