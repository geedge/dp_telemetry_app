#include "job_ctx.h"
#include "common.h"
#include "trace_output.h"

#include <MESA/maat.h>
#include <stdlib.h>
#include <string.h>

struct dp_trace_job_occupy
{
    uint8_t uesd;
    uint8_t role;
};

static struct dp_trace_job_occupy dp_trace_job_occupy[DP_TRACE_JOB_NUM_MAX] = {};

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

void job_rule_apply(struct dp_trace_job_desc desc[], unsigned int nr_desc, uint8_t role)
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
        uint8_t rule_index = desc[i].rule_index;
        job_bitmap_t cur_job_id = index_to_job_id(rule_index);
        unsigned int cur_job_uesd = cur_job_id & jobs_id_used;

        // The job that was not originally in the system is now included in the configuration.
        if (cur_job_uesd == 0 && desc[i].enable && strlen(desc[i].bpf_expr) != 0)
        {
            marsio_dp_trace_job_add(mr_instance, &desc[i]);
            dp_trace_job_occupy[rule_index].uesd = 1;
            dp_trace_job_occupy[rule_index].role = role;

#if 1
            // just for test
            if (rule_index == 10)
            {
                dp_trace_job_occupy[rule_index].role = DP_TELEMETRY_ROLE;
            }
#endif
            continue;
        }

        // The job that originally existed in the system is not in the configuration now.
        if (cur_job_uesd == 1 && (desc[i].enable == false || strlen(desc[i].bpf_expr) == 0))
        {
            marsio_dp_trace_job_del(mr_instance, cur_job_id);
            uint8_t tmp_role = dp_trace_job_occupy[rule_index].role;
            dp_trace_job_occupy[rule_index].uesd = 0;
            dp_trace_job_occupy[rule_index].role = DP_TRACE_NO_ROLE;

            if (dp_trace_job_occupy[rule_index].role == tmp_role)
            {
                dp_trace_pcapng_merger(cur_job_id);
            }
            continue;
        }
    }
}

int is_job_id_used(job_bitmap_t job_id)
{
    unsigned int index = job_id_to_index(job_id);
    return dp_trace_job_occupy[index].uesd;
}

uint8_t job_id_role_get(job_bitmap_t job_id)
{
    unsigned int index = job_id_to_index(job_id);
    return dp_trace_job_occupy[index].role;
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