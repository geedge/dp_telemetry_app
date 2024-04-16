#pragma once
#include "common.h"

#include <event.h>
#include <librdkafka/rdkafka.h>

#include <limits.h>
#include <sched.h>
#include <stdint.h>

struct config
{
    char absolute_path[PATH_MAX];

    char config_path[PATH_MAX];
    char dy_config_path[PATH_MAX];
    char zlog_config_path[PATH_MAX];

    cpu_set_t cpu_set_io;

    // device Information
    char * sled_ip;
    char device_group[MR_SYMBOL_MAX];

    // kafka
    char topic_name[MR_SYMBOL_MAX];
    char broker_list[1024];
    char sasl_username[MR_SYMBOL_MAX];
    char sasl_password[MR_SYMBOL_MAX];

    // maat
    unsigned int maat_log_level;
    unsigned int maat_input_mode;
    char table_schema[PATH_MAX];
    char json_cfg_file[PATH_MAX];

    char redis_server[MR_SYMBOL_MAX];
    char redis_port_range[MR_SYMBOL_MAX];
    int redis_db_idx;

    // dp trace
    char dp_trace_dir[PATH_MAX];
    unsigned int dp_trace_file_max_size_in_KB;
    unsigned int dp_trace_merge_timeout;
    struct dp_trace_job_desc desc[DP_TRACE_JOB_NUM_MAX];
};

const struct config * config_create(const char * config_path, const char * dy_config_path);
const struct config * global_config_get();
void global_config_destroy();
void config_load();
void dynamic_config_load_and_apply();