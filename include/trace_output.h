#pragma once
#include "common.h"

void dp_trace_output_init();
void * dp_trace_process_thread(void * arg);
void dp_trace_pcapng_merger(job_bitmap_t job_id);