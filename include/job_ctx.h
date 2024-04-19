#pragma once
#include "marsio.h"
#include <uuid/uuid.h>

struct dp_trace_telemetry_desc
{
    uuid_t uuid;
    struct dp_trace_job_desc job_desc;
};

void job_rule_apply(struct dp_trace_job_desc desc[], unsigned int nr_desc, uint8_t role);
int job_id_to_index(job_bitmap_t job_id);
job_bitmap_t index_to_job_id(unsigned int index);
int is_job_id_used(job_bitmap_t job_id);
uint8_t job_id_role_get(job_bitmap_t job_id);

void telemetry_job_add_cb(const char * table_name, int table_id, const char * key, const char * table_line, void ** ad,
                          long argl, void * argp);
void telemetry_job_del_cb(int table_id, void ** ad, long argl, void * argp);
void telemetry_job_uuid_get(job_bitmap_t job_id, unsigned char * uu);