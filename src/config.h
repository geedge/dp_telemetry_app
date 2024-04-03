#pragma once
#include "common.h"
#include <limits.h>
#include <sched.h>
#include <stdint.h>

struct config
{
    char config_path[PATH_MAX];
    char dy_config_path[PATH_MAX];

    cpu_set_t cpu_set_io;
    uint8_t log_level;

    char dp_trace_dir[PATH_MAX];
    unsigned int dp_trace_file_max_size_in_KB;
    unsigned int dp_trace_merge_timeout;
    struct dp_trace_job_desc desc[DP_TRACE_JOB_NUM_MAX];
};

struct config * config_create();
void config_load();
void dynamic_config_load();
void dynamic_config_apply();
void dynamic_config_reload_thread_create();