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
static struct dp_trace_telemetry_desc telemetry_descs[DP_TRACE_JOB_NUM_MAX] = {};

static int telemetry_unused_job_index_get();

void job_rule_apply(struct dp_trace_job_desc desc[], unsigned int nr_desc, uint8_t role)
{
    job_bitmap_t jobs_id_used;
    int ret = marsio_dp_trace_job_id_uesd_get(mr_instance, &jobs_id_used);
    if (ret < 0)
    {
        dzlog_error("marsio_dp_trace_job_id_uesd_get faild");
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

void telemetry_job_add_cb(const char * table_name, int table_id, const char * key, const char * table_line, void ** ad,
                          long argl, void * argp)
{
    int ret = 0;
    size_t offset = 0;
    size_t len = 0;
    char * json_str = NULL;
    cJSON * json = NULL;

    ret = maat_helper_read_column(table_line, 2, &offset, &len);
    if (ret < 0)
    {
        dzlog_error("fail to get data path rule in maat.");
        goto out;
    }

    json_str = calloc(sizeof(char), len + 1);
    memcpy(json_str, table_line + offset, len);
    json = cJSON_Parse(json_str);
    if (json == NULL)
    {
        dzlog_error("Invalid decryption parameter: %s", table_line);
        goto out;
    }

    struct dp_trace_telemetry_desc telemetry_desc = {};
    struct dp_trace_job_desc * job_desc = &telemetry_desc.job_desc;

    cJSON * uuid_obj = cJSON_GetObjectItem(json, "job_id");
    DP_TRACE_VERIFY(cJSON_IsString(uuid_obj), "uuid is not string");
    ret = uuid_parse(uuid_obj->valuestring, telemetry_desc.uuid);
    if (ret != 0)
    {
        dzlog_error("uuid parsing failed: %s", uuid_obj->valuestring);
        goto out;
    }

    cJSON * bpf_obj = cJSON_GetObjectItem(json, "bpf_expr");
    DP_TRACE_VERIFY(cJSON_IsString(bpf_obj), "bpf expr is not string");
    snprintf(job_desc->bpf_expr, sizeof(job_desc->bpf_expr), "%s", bpf_obj->valuestring);

    int index = telemetry_unused_job_index_get();
    if (index < 0)
    {
        dzlog_warn("no enough job for bpf_expr:", job_desc->bpf_expr);
        goto out;
    }

    job_desc->rule_index = index;
    job_desc->sampling = 1;
    job_desc->enable = true;

    memcpy(&telemetry_descs[index], &telemetry_desc, sizeof(struct dp_trace_telemetry_desc));

    job_rule_apply(&telemetry_descs[index].job_desc, 1, DP_TELEMETRY_ROLE);

    *ad = &telemetry_descs[index];

out:
    if (json_str)
        cJSON_Delete(json);
    if (json_str)
        free(json_str);
    return;
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