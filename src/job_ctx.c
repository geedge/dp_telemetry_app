#include "job_ctx.h"
#include "common.h"
#include "config.h"
#include "maat.h"
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
static void job_desc_dump(struct dp_trace_job_desc * desc);

void job_rule_apply(struct dp_trace_job_desc desc[], unsigned int nr_desc)
{
    int ret = 0;
    for (unsigned int i = 0; i < nr_desc; i++)
    {
        job_desc_dump(&desc[i]);

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
                dp_trace_job_occupy[rule_index].role = desc[i].measurement_type;
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

                if (tmp_role == DP_TRACE_MEASUREMENT_TYPE_TRACE)
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
    struct dp_trace_telemetry_desc telemetry_desc = {};
    struct dp_trace_job_desc * job_desc = &telemetry_desc.job_desc;

    bool valid_rule = maat_rule_parse(table_line, &telemetry_desc);
    if (valid_rule == false)
    {
        dzlog_info("not a valid rule.");
        return;
    }

    int index = telemetry_unused_job_index_get();
    if (index < 0)
    {
        dzlog_warn("no enough job for current rule:%s", table_line);
        return;
    }

    job_desc->rule_index = index;
    job_desc->enable = true;
    memcpy(&telemetry_descs[index], &telemetry_desc, sizeof(struct dp_trace_telemetry_desc));

    job_rule_apply(&telemetry_descs[index].job_desc, 1);

    *ad = &telemetry_descs[index];
}

void telemetry_job_del_cb(int table_id, void ** ad, long argl, void * argp)
{
    struct dp_trace_telemetry_desc * telemetry_desc = *ad;
    struct dp_trace_job_desc * job_desc = &telemetry_desc->job_desc;
    job_desc->enable = false;
    job_rule_apply(job_desc, 1);
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

static void job_desc_dump(struct dp_trace_job_desc * desc)
{
    dzlog_info("dp trace job desc dump");
    dzlog_info("rule_index:%u", desc->rule_index);
    dzlog_info("enable:%u", desc->enable);
    dzlog_info("measurement_type:%u", desc->measurement_type);
    dzlog_info("bpf_expr:%s", desc->bpf_expr);
    dzlog_info("pkt_cnt_max:%u", desc->pkt_cnt_max);
    dzlog_info("sampling:%u", desc->sampling);
    dzlog_info("snaplen:%u", desc->snaplen);

    dzlog_info("traffic_link_id_cnt:%u", desc->traffic_link_id_cnt);
    char traffic_link_id_str[512];
    int len = snprintf(traffic_link_id_str, sizeof(traffic_link_id_str), "%s", "[");
    for (unsigned int i = 0; i < desc->traffic_link_id_cnt; i++)
    {
        char * normal_format = "%u,";
        char * last_format = "%u";

        char * fromat = normal_format;
        if (i == desc->traffic_link_id_cnt - 1)
        {
            fromat = last_format;
        }
        len +=
            snprintf(traffic_link_id_str + len, sizeof(traffic_link_id_str) - len, fromat, desc->traffic_link_ids[i]);
    }
    len += snprintf(traffic_link_id_str + len, sizeof(traffic_link_id_str) - len, "%s", "]");
    dzlog_info("traffic_link_ids:%s", traffic_link_id_str);
}

#if 0
int traffic_link_id_extract(char * traffic_str, uint16_t traffic_link_ids[], unsigned int size)
{
    unsigned int traffic_link_id_cnt = 0;
    const char * delim = "[], ";
    char * token = strtok(traffic_str, delim);

    for (unsigned int i = 0; i < size; i++)
    {
        if (token != NULL)
        {
            char * endptr;
            uint16_t id = (uint16_t)strtol(token, &endptr, 10);
            if (endptr != token)
            {
                traffic_link_ids[traffic_link_id_cnt++] = id;
            }
        }
        token = strtok(NULL, delim);
    }
    return traffic_link_id_cnt;
}
#endif