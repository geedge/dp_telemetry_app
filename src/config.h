#pragma once
#include "common.h"

#include <event.h>
#include <librdkafka/rdkafka.h>

#include <limits.h>
#include <sched.h>
#include <stdint.h>

struct config
{
    char config_path[PATH_MAX];
    char dy_config_path[PATH_MAX];

    cpu_set_t cpu_set_io;
    uint8_t log_level;

    // signal event
    struct event_base * evbase;
    struct event * evsignal[8];

    // kafka
    char topic_name[MR_SYMBOL_MAX];
    char broker_list[1024];
    char sasl_username[MR_SYMBOL_MAX];
    char sasl_password[MR_SYMBOL_MAX];
    rd_kafka_t * kafka_handle;
    rd_kafka_topic_t * kafka_topic;

    // dp trace
    char dp_trace_dir[PATH_MAX];
    unsigned int dp_trace_file_max_size_in_KB;
    unsigned int dp_trace_merge_timeout;
    struct dp_trace_job_desc desc[DP_TRACE_JOB_NUM_MAX];
};

struct config * config_create();
void config_load();
void dynamic_config_load();
void dynamic_config_apply();
void dynamic_config_reload_init();