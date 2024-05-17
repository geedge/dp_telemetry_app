#pragma once
#include "common.h"

struct record_saving_stat
{
    uint64_t recv_success;
    uint64_t init_old_packet_drop;
    uint64_t ctrlbuf_drop;

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

struct monit
{
    unsigned nr_stat;
    struct record_saving_stat savint_stats[0];
};

void monit_init(unsigned int thread_num);
struct record_saving_stat * record_saving_stat_point_get(unsigned int thread_id);