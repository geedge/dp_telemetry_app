#pragma once
#include "job_ctx.h"

#define MAAT_INPUT_JSON 0
#define MAAT_INPUT_REDIS 1

void dp_trace_maat_init();
bool maat_rule_parse(const char * table_line, struct dp_trace_telemetry_desc * telemetry_desc);