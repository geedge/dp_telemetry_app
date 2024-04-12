#include "config.h"
#include "job_ctx.h"
#include <MESA/MESA_prof_load.h>

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LCORE 128

extern struct mr_instance * mr_instance;
static struct config * g_conf = NULL;

const struct config * config_create(const char * config_path, const char * dy_config_path)
{
    struct config * conf = calloc(1, sizeof(struct config));
    CHECK_OR_EXIT(conf, "calloc failed: %s", strerror(errno));

    snprintf(conf->config_path, sizeof(conf->config_path), "%s", config_path);
    snprintf(conf->dy_config_path, sizeof(conf->dy_config_path), "%s", dy_config_path);

    g_conf = conf;
    return conf;
}

const struct config * global_config_get()
{
    CHECK_OR_EXIT(g_conf, "global config is null.");
    return g_conf;
}

void global_config_destroy()
{
    free(g_conf);
}

void config_load()
{
    if (access(g_conf->config_path, R_OK) != 0)
    {
        dzlog_error("Configure File %s is not existed. Failed. ", g_conf->config_path);
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

    MESA_load_profile_string_nodef(g_conf->config_path, "global", "zlog_config_path", g_conf->zlog_config_path,
                                   sizeof(g_conf->zlog_config_path));

    char dp_trace_dir[PATH_MAX] = "/tmp";
    MESA_load_profile_string_nodef(g_conf->config_path, "global", "dp_trace_dir", dp_trace_dir, sizeof(dp_trace_dir));
    if (dp_trace_dir[strlen(dp_trace_dir) - 1] == '/')
    {
        dp_trace_dir[strlen(dp_trace_dir) - 1] = '\0';
    }
    snprintf(g_conf->dp_trace_dir, sizeof(g_conf->dp_trace_dir), "%s", dp_trace_dir);

    MESA_load_profile_string_def(g_conf->device_group, "global", "device_group", g_conf->device_group,
                                 sizeof(g_conf->device_group), "unknow");

    g_conf->sled_ip = getenv("SLED_IP");
    if (g_conf->sled_ip == NULL)
    {
        dzlog_warn("SLED_IP environment variable does not exist.");
    }

    MESA_load_profile_string_def(g_conf->config_path, "kafka", "borker_list", g_conf->broker_list,
                                 sizeof(g_conf->broker_list), "");
    MESA_load_profile_string_def(g_conf->config_path, "kafka", "topic_name", g_conf->topic_name,
                                 sizeof(g_conf->topic_name), "");
    MESA_load_profile_string_def(g_conf->config_path, "kafka", "sasl_username", g_conf->sasl_username,
                                 sizeof(g_conf->sasl_username), "");
    MESA_load_profile_string_def(g_conf->config_path, "kafka", "sasl_password", g_conf->sasl_password,
                                 sizeof(g_conf->sasl_password), "");
}

void dynamic_config_load()
{
    if (access(g_conf->dy_config_path, R_OK) != 0)
    {
        dzlog_error("Dynamic configure file %s is not existed. Do not load dynamic configuration files. ",
                    g_conf->dy_config_path);
        return;
    }

    dzlog_info("Loading data path trace configuration file...");

    int ret = 0;

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

    dzlog_info("Loading data path trace configuration file is completed.");
}

void dynamic_config_load_and_apply()
{
    dynamic_config_load();
    job_rule_apply(g_conf->desc, TELEMETRY_DIM(g_conf->desc), DP_TRACE_ROLE);
}
