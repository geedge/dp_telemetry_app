#pragma once
#include "marsio.h"
#include <uuid/uuid.h>

#define DP_TRACE_TELEMETRY_TABLE "DP_TRACE_TELEMETRY_TABLE"

struct dp_trace_telemetry_desc
{
    uuid_t uuid;
    job_bitmap_t job_id;
    struct dp_trace_job_desc;
};