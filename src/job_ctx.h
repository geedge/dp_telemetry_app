#pragma once
#include "marsio.h"

void job_rule_apply(struct dp_trace_job_desc desc[], unsigned int nr_desc, uint8_t role);
int job_id_to_index(job_bitmap_t job_id);
job_bitmap_t index_to_job_id(unsigned int index);
int is_job_id_used(job_bitmap_t job_id);
uint8_t job_id_role_get(job_bitmap_t job_id);