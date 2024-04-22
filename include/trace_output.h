#pragma once
#include "common.h"

struct record_saving_stat
{
    uint64_t recv_success;
    uint64_t save_failed_at_job_deleted;
    uint64_t save_failed_at_mutex_lock;

    uint64_t save_to_kafka_failed_at_decode_messagepack;
    uint64_t save_to_kafka_failed_at_send;
    uint64_t save_to_kafka_success;

    uint64_t save_to_file_failed_at_decode_to_str;
    uint64_t save_to_file_failed_at_pcapng_open;
    uint64_t save_to_file_failed_at_pcapng_format;
    uint64_t save_to_file_failed_at_write_to_disk;
    uint64_t save_to_file_success;
};

void dp_trace_output_init();
void * dp_trace_process_thread(void * arg);
void dp_trace_pcapng_merger(job_bitmap_t job_id);