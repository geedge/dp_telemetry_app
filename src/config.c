#include "config.h"
#include "job_ctx.h"
#include <MESA/MESA_prof_load.h>
#include <MESA/maat.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LCORE 128

extern struct mr_instance * mr_instance;
static struct config * g_conf = NULL;

static void main_process_realpath_get(char * absolute_path, size_t size);
static char * paths_combine(const char * restrict absolute_path, const char * restrict relative_path,
                            char * restrict resolve_path);

const struct config * config_create(const char * config_path, const char * dy_config_path)
{
    struct config * conf = calloc(1, sizeof(struct config));
    DP_TRACE_VERIFY(conf, "calloc failed: %s", strerror(errno));

    snprintf(conf->config_path, sizeof(conf->config_path), "%s", config_path);
    snprintf(conf->dy_config_path, sizeof(conf->dy_config_path), "%s", dy_config_path);
    main_process_realpath_get(conf->absolute_path, sizeof(conf->absolute_path));
    printf("process absolute path %s\n", conf->absolute_path);

    g_conf = conf;
    return conf;
}

const struct config * global_config_get()
{
    DP_TRACE_VERIFY(g_conf, "global config is null.");
    return g_conf;
}

void global_config_destroy()
{
    free(g_conf);
}

void config_load()
{
    char tmp_path[PATH_MAX];

    const char * config_path = g_conf->config_path;
    if (access(config_path, R_OK) != 0)
    {
        printf("Configure File(%s) load failed:%s \n", config_path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    CPU_ZERO(&g_conf->cpu_set_io);
    unsigned int io_cores[MAX_LCORE] = {};
    int nr_io_cores = 0;
    nr_io_cores =
        MESA_load_profile_uint_range(config_path, "global", "iocore", sizeof(io_cores) / sizeof(io_cores[0]), io_cores);
    for (unsigned int i = 0; i < nr_io_cores; i++)
    {
        CPU_SET(io_cores[i], &g_conf->cpu_set_io);
    }

    MESA_load_profile_string_nodef(config_path, "global", "zlog_config_path", tmp_path, sizeof(tmp_path));
    paths_combine(g_conf->absolute_path, tmp_path, g_conf->zlog_config_path);
    printf("zlog config path: %s\n", g_conf->zlog_config_path);

    MESA_load_profile_string_def(config_path, "global", "dp_trace_dir", tmp_path, sizeof(tmp_path), "/tmp");
    if (tmp_path[strlen(tmp_path) - 1] == '/')
    {
        tmp_path[strlen(tmp_path) - 1] = '\0';
    }
    snprintf(g_conf->dp_trace_dir, sizeof(g_conf->dp_trace_dir), "%s", tmp_path);

    MESA_load_profile_string_def(g_conf->device_group, "global", "device_group", g_conf->device_group,
                                 sizeof(g_conf->device_group), "unknow");

    g_conf->sled_ip = getenv("SLED_IP");
    if (g_conf->sled_ip == NULL)
    {
        printf("SLED_IP environment variable does not exist.\n");
    }

    MESA_load_profile_string_def(config_path, "kafka", "borker_list", g_conf->broker_list, sizeof(g_conf->broker_list),
                                 "");
    MESA_load_profile_string_def(config_path, "kafka", "topic_name", g_conf->topic_name, sizeof(g_conf->topic_name),
                                 "");
    MESA_load_profile_string_def(config_path, "kafka", "sasl_username", g_conf->sasl_username,
                                 sizeof(g_conf->sasl_username), "");
    MESA_load_profile_string_def(config_path, "kafka", "sasl_password", g_conf->sasl_password,
                                 sizeof(g_conf->sasl_password), "");

    MESA_load_profile_int_def(config_path, "maat", "maat_log_level", &(g_conf->maat_log_level), LOG_LEVEL_FATAL);
    MESA_load_profile_int_def(config_path, "maat", "maat_input_mode", &(g_conf->maat_input_mode), 0);

    MESA_load_profile_string_def(config_path, "maat", "table_schema", tmp_path, sizeof(tmp_path), "");
    paths_combine(g_conf->absolute_path, tmp_path, g_conf->table_schema);
    printf("maat table_schema path: %s\n", g_conf->table_schema);

    MESA_load_profile_string_def(config_path, "maat", "json_cfg_file", tmp_path, sizeof(tmp_path), "");
    paths_combine(g_conf->absolute_path, tmp_path, g_conf->json_cfg_file);
    printf("maat json config file path: %s\n", g_conf->json_cfg_file);

    MESA_load_profile_string_def(config_path, "maat", "maat_redis_server", g_conf->redis_server,
                                 sizeof(g_conf->redis_server), "");
    MESA_load_profile_string_def(config_path, "maat", "maat_redis_port_range", g_conf->redis_port_range,
                                 sizeof(g_conf->redis_server), "6379");
    MESA_load_profile_int_def(config_path, "maat", "maat_redis_db_index", &(g_conf->redis_db_idx), 0);
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

//////////////////////// helper function /////////////////////////
static void main_process_realpath_get(char * absolute_path, size_t size)
{
    // readlink requires "/proc/self/exe" on Linux and "/proc/curproc/file" on FreeBSD
    // https://stackoverflow.com/questions/933850/how-do-i-find-the-location-of-the-executable-in-c
    // this function only for linux
    ssize_t len = readlink("/proc/self/exe", absolute_path, size - 1);
    DP_TRACE_VERIFY(len != -1, "get main process realpath failed:%s", strerror(errno));
    absolute_path[len] = '\0';
}

// Combine relative paths and absolute paths into a new absolute path
static char * paths_combine(const char * restrict absolute_path, const char * restrict relative_path,
                            char * restrict resolve_path)
{
    if (relative_path[0] == '/')
    {
        strcpy(resolve_path, relative_path);
        return resolve_path;
    }

    char combine_path[PATH_MAX];
    strcpy(combine_path, absolute_path);

    char * last_slash = strrchr(combine_path, '/');
    DP_TRACE_VERIFY(last_slash != NULL, "Invalid absolute path.");
    *last_slash = '\0';

    strcat(combine_path, "/");
    strcat(combine_path, relative_path);

    realpath(combine_path, resolve_path);

    return resolve_path;
}