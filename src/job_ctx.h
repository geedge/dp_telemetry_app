#pragma once
#include "marsio.h"
#include <uuid/uuid.h>

#define DP_TRACE_TELEMETRY_TABLE "DP_TRACE_TELEMETRY_TABLE"
#define DP_TRACE_TELEMETRY_NUM 8

struct dp_trace_telemetry_desc
{
    uint8_t used;
    uuid_t uuid;
    char job_name[128];
    job_bitmap_t job_id;
    struct dp_trace_job_desc job_desc;
};

void job_rule_apply(struct dp_trace_job_desc desc[], unsigned int nr_desc);
int job_id_to_index(job_bitmap_t job_id);
job_bitmap_t index_to_job_id(unsigned int index);
int is_job_id_used(job_bitmap_t job_id);