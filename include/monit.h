#pragma once
#include "common.h"
#include "trace_output.h"

struct monit
{
    unsigned nr_stat;
    struct record_saving_stat savint_stats[0];
};

void monit_init(unsigned int thread_num);
struct record_saving_stat * record_saving_stat_point_get(unsigned int thread_id);