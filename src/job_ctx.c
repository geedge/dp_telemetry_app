#include "job_ctx.h"
#include "common.h"
#include "trace_output.h"

#include <MESA/maat.h>
#include <stdlib.h>
#include <string.h>

extern struct mr_instance * mr_instance;
uint8_t dp_trace_job_id_uesd[DP_TRACE_JOB_NUM_MAX];

#if 0
static void telemetry_job_add_cb(const char * table_name, int table_id, const char * key, const char * table_line,
                                 void ** ad, long argl, void * argp)
{
    return;
}

static void telemetry_job_del_cb(const char * table_name, int table_id, const char * key, const char * table_line,
                                 void ** ad, long argl, void * argp)
{
    return;
}

int maat_register()
{
    struct maat_options * opts = maat_options_new();
    struct maat * maat_instance = maat_new(opts, "");
    maat_get_table_id(maat_instance, DP_TRACE_TELEMETRY_TABLE);

    int ret = maat_plugin_table_ex_schema_register(maat_instance, DP_TRACE_TELEMETRY_TABLE, telemetry_job_add_cb,
                                                   telemetry_job_del_cb, NULL, 0, NULL);

    if (ret != 0)
    {
        MR_ERROR("failed at register callback of DP_TRACE_TELEMETRY_TABLE, ret = %d", ret);
        return -1;
    }

    return 1;
}
#endif

void job_rule_apply(struct dp_trace_job_desc desc[], unsigned int nr_desc)
{
    job_bitmap_t jobs_id_used;
    int ret = marsio_dp_trace_job_id_uesd_get(mr_instance, &jobs_id_used);
    if (ret < 0)
    {
        syslog(LOG_ERR, "marsio_dp_trace_job_id_uesd_get faild");
        return;
    }

    for (unsigned int i = 0; i < nr_desc; i++)
    {
        job_bitmap_t cur_job_id = 1 << i;
        unsigned int cur_job_uesd = cur_job_id & jobs_id_used;

        // The job that was not originally in the system is now included in the configuration.
        if (cur_job_uesd == 0 && desc[i].enable && strlen(desc[i].bpf_expr) != 0)
        {
            marsio_dp_trace_job_add(mr_instance, &desc[i]);
            dp_trace_job_id_uesd[i] = 1;
            continue;
        }

        // The job that originally existed in the system is not in the configuration now.
        if (cur_job_uesd == 1 && (desc[i].enable == false || strlen(desc[i].bpf_expr) == 0))
        {
            marsio_dp_trace_job_del(mr_instance, cur_job_id);
            dp_trace_job_id_uesd[i] = 0;
            dp_trace_pcapng_merger(cur_job_id);
            continue;
        }
    }
}

int is_job_id_used(job_bitmap_t job_id)
{
    unsigned int index = job_id_to_index(job_id);
    return dp_trace_job_id_uesd[index];
}

int job_id_to_index(job_bitmap_t job_id)
{
    assert(__builtin_popcount(job_id) == 1);
    unsigned int index = __builtin_ctz(job_id);

    assert(index < DP_TRACE_JOB_NUM_MAX);
    return index;
}

job_bitmap_t index_to_job_id(unsigned int index)
{
    assert(index < DP_TRACE_JOB_NUM_MAX);
    return 1 << index;
}