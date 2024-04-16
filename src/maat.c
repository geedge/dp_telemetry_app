#include "maat.h"
#include "config.h"
#include "job_ctx.h"

#include <MESA/maat.h>
#include <cjson/cJSON.h>
#include <uuid/uuid.h>

struct dp_trace_telemetry_desc
{
    uuid_t uuid;
    char job_name[128];
    struct dp_trace_job_desc job_desc;
};

static struct dp_trace_telemetry_desc telemetry_descs[DP_TRACE_JOB_NUM_MAX];

static int dp_trace_telemetry_unused_job_index_get()
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

static struct maat * dp_trace_maat_instance_create()
{
    int ret = 0;
    int redis_port_begin = 0;
    int redis_port_end = 0;
    int redis_port_select = 0;
    struct maat * target = NULL;

    const struct config * conf = global_config_get();

    struct maat_options * opts = maat_options_new();
    maat_options_set_logger(opts, "log/maat.log", (enum log_level)conf->maat_log_level);
    maat_options_set_instance_name(opts, "dp_trace_telemetry");
    maat_options_set_caller_thread_number(opts, 0);

    switch (conf->maat_input_mode)
    {
    case MAAT_INPUT_JSON:
        ret = maat_options_set_json_file(opts, conf->json_cfg_file);
        if (ret != 0)
        {
            dzlog_error("maat_options_set_json_file function execution failed.");
            goto error_out;
        }
        break;
    case MAAT_INPUT_REDIS:
        ret = sscanf(conf->redis_port_range, "%d-%d", &redis_port_begin, &redis_port_end);
        if (ret == 1)
        {
            redis_port_select = redis_port_begin;
        }
        else if (ret == 2)
        {
            srand(time(NULL));
            redis_port_select = redis_port_begin + rand() % (redis_port_end - redis_port_begin);
        }
        else
        {
            dzlog_error("Invalid redis port range %s, MAAT init failed.", conf->redis_port_range);
            goto error_out;
        }
        maat_options_set_redis(opts, conf->redis_server, redis_port_select, conf->redis_db_idx);
        break;
    default:
        dzlog_error("Invalid MAAT Input Mode: %d.", conf->maat_input_mode);
        goto error_out;
        break;
    }

    target = maat_new(opts, conf->table_schema);
    if (!target)
    {
        dzlog_error("maat_new function execution failed.");
        goto error_out;
    }

    maat_options_free(opts);
    return target;

error_out:
    maat_options_free(opts);
    return NULL;
}

static void telemetry_job_add_cb(const char * table_name, int table_id, const char * key, const char * table_line,
                                 void ** ad, long argl, void * argp)
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

    struct dp_trace_telemetry_desc telemetry_desc;
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

    int index = dp_trace_telemetry_unused_job_index_get();
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

static void telemetry_job_del_cb(int table_id, void ** ad, long argl, void * argp)
{
    struct dp_trace_telemetry_desc * telemetry_desc = *ad;
    struct dp_trace_job_desc * job_desc = &telemetry_desc->job_desc;
    job_desc->enable = false;
    job_rule_apply(job_desc, 1, DP_TELEMETRY_ROLE);
    return;
}

void dp_trace_maat_init()
{
    dzlog_info("data path trace maat init start...");
    struct maat * target = dp_trace_maat_instance_create();
    DP_TRACE_VERIFY(target, "create maat instance failed.");

    int ret = maat_plugin_table_ex_schema_register(target, "DP_TRACE_TELEMETRY_TABLE", telemetry_job_add_cb,
                                                   telemetry_job_del_cb, NULL, 0, NULL);
    DP_TRACE_VERIFY(ret == 0, "failed at register callback of DP_TRACE_TELEMETRY_TABLE.");
    dzlog_info("data path trace maat init end");
}