#include "job_ctx.h"
#include "common.h"
#include "trace_output.h"

#include <MESA/maat.h>
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <string.h>

struct dp_trace_job_occupy
{
    uint8_t uesd;
    uint8_t role;
};

static struct dp_trace_job_occupy dp_trace_job_occupy[DP_TRACE_JOB_NUM_MAX] = {};
static struct dp_trace_telemetry_desc telemetry_descs[DP_TRACE_JOB_NUM_MAX] = {};

static int telemetry_unused_job_index_get();

void job_rule_apply(struct dp_trace_job_desc desc[], unsigned int nr_desc, uint8_t role)
{
    int ret = 0;
    for (unsigned int i = 0; i < nr_desc; i++)
    {
        uint8_t rule_index = desc[i].rule_index;
        job_bitmap_t cur_job_id = index_to_job_id(rule_index);
        uint8_t cur_job_uesd = is_job_id_used(cur_job_id);

        // The job that was not originally in the system is now included in the configuration.
        if (cur_job_uesd == 0 && desc[i].enable && strlen(desc[i].bpf_expr) != 0)
        {
            ret = marsio_dp_trace_job_add(mr_instance, &desc[i]);
            if (ret == 0)
            {
                dzlog_error("add rule %u failed. bpf_expr is: %s", rule_index, desc[i].bpf_expr);
            }
            else
            {
                dp_trace_job_occupy[rule_index].uesd = 1;
                dp_trace_job_occupy[rule_index].role = role;
                dzlog_info("add rule %u successfully. bpf_expr is: %s", rule_index, desc[i].bpf_expr);
            }
            continue;
        }

        // The job that originally existed in the system is not in the configuration now.
        if (cur_job_uesd == 1 && (desc[i].enable == false || strlen(desc[i].bpf_expr) == 0))
        {
            ret = marsio_dp_trace_job_del(mr_instance, cur_job_id);
            if (ret < 0)
            {
                dzlog_error("del rule %u failed. bpf_expr is: %s", rule_index, desc[i].bpf_expr);
            }
            else
            {
                uint8_t tmp_role = dp_trace_job_occupy[rule_index].role;
                dp_trace_job_occupy[rule_index].uesd = 0;
                dp_trace_job_occupy[rule_index].role = DP_TRACE_NO_ROLE;

                if (tmp_role == DP_TRACE_ROLE)
                {
                    dp_trace_pcapng_merger(cur_job_id);
                }
                dzlog_info("del rule %u successfully. bpf_expr is: %s", rule_index, desc[i].bpf_expr);
            }
            continue;
        }
    }
}

void telemetry_job_add_cb(const char * table_name, int table_id, const char * key, const char * table_line, void ** ad,
                          long argl, void * argp)
{
    int ret = 0;
    struct dp_trace_telemetry_desc telemetry_desc = {};
    struct dp_trace_job_desc * job_desc = &telemetry_desc.job_desc;

    char uuid[40] = {};
    unsigned int pkt_cnt_max = 0;
    char bpf_expr[MR_BPF_EXPRESSION_MAX];
    unsigned int timeout = 0;
    unsigned int sampling = 0;
    unsigned int snaplen = 0;
    unsigned int with_packet_capture = 0;
    char device_group[512];
    char traffic_link_id[1024];
    unsigned int is_valid = 0;

    dzlog_debug("telemetry add maat parse config rule:%s", table_line);
    ret = sscanf(table_line, "%s\t%u\t%s\t%u\t%u\t%u\t%u\t%512s\t%1024s\t%u", uuid, &pkt_cnt_max, bpf_expr, &timeout,
                 &sampling, &snaplen, &with_packet_capture, device_group, traffic_link_id, &is_valid);
    if (ret != 10)
    {
        dzlog_warn("maat parse config failed. Not enough fields:%s", table_line);
        // return;
    }

    if (is_valid == 0)
    {
        dzlog_warn("rule is not valid:%s", table_line);
        // return;
    }

    ret = uuid_parse(uuid, telemetry_desc.uuid);
    if (ret != 0)
    {
        dzlog_error("uuid parsing failed: %s", uuid);
        return;
    }

    backspace_remove(bpf_expr, job_desc->bpf_expr);

    job_desc->pkt_cnt_max = pkt_cnt_max;
    job_desc->sampling = sampling;
    job_desc->snaplen = snaplen;
    job_desc->measurement_type = DP_TRACE_MEASUREMENT_TYPE_TELEMETRY;

    int index = telemetry_unused_job_index_get();
    if (index < 0)
    {
        dzlog_warn("no enough job for bpf_expr:", job_desc->bpf_expr);
        return;
    }

    job_desc->rule_index = index;
    job_desc->enable = true;

    memcpy(&telemetry_descs[index], &telemetry_desc, sizeof(struct dp_trace_telemetry_desc));

    job_rule_apply(&telemetry_descs[index].job_desc, 1, DP_TELEMETRY_ROLE);

    *ad = &telemetry_descs[index];
}

void telemetry_job_del_cb(int table_id, void ** ad, long argl, void * argp)
{
    struct dp_trace_telemetry_desc * telemetry_desc = *ad;
    struct dp_trace_job_desc * job_desc = &telemetry_desc->job_desc;
    job_desc->enable = false;
    job_rule_apply(job_desc, 1, DP_TELEMETRY_ROLE);
    return;
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

void telemetry_job_uuid_get(job_bitmap_t job_id, unsigned char * uu)
{
    unsigned int index = job_id_to_index(job_id);
    uuid_copy(uu, telemetry_descs[index].uuid);
}

static int telemetry_unused_job_index_get()
{
#define TELEMETRY_INDEX_START 8
#define TELEMETRY_INDEX_END DP_TRACE_JOB_NUM_MAX

    int ret = -1;
    for (unsigned int i = TELEMETRY_INDEX_START; i < TELEMETRY_INDEX_END; i++)
    {
        job_bitmap_t job_id = index_to_job_id(i);
        if (is_job_id_used(job_id))
        {
            continue;
        }
        ret = i;
        break;
    }

    return ret;
}