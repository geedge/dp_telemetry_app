#include "maat.h"
#include "config.h"

#include <MESA/maat.h>
#include <cjson/cJSON.h>

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

    dzlog_info("maat defere load:%u", conf->deferred_load_on);
    if (conf->deferred_load_on)
    {
        ret = maat_options_set_deferred_load_on(opts);
        if (ret != 0)
        {
            dzlog_warn("maat_options_set_deferred_load_on function execution failed.");
        }
    }

    maat_options_set_caller_thread_number(opts, 0);
    switch (conf->maat_input_mode)
    {
    case MAAT_INPUT_JSON:
        ret = maat_options_set_json_file(opts, conf->json_cfg_file);
        if (ret != 0)
        {
            dzlog_error("maat_options_set_json_file function execution failed.");
            goto end;
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
            goto end;
        }
        maat_options_set_redis(opts, conf->redis_server, redis_port_select, conf->redis_db_idx);
        break;
    default:
        dzlog_error("Invalid MAAT Input Mode: %d.", conf->maat_input_mode);
        goto end;
        break;
    }

    target = maat_new(opts, conf->table_schema);
    if (!target)
    {
        dzlog_error("maat_new function execution failed.");
        goto end;
    }

end:
    if (opts != NULL)
    {
        maat_options_free(opts);
    }
    return target;
}

void dp_trace_maat_init()
{
    dzlog_info("data path trace maat init start...");
    struct maat * target = dp_trace_maat_instance_create();
    DP_TRACE_VERIFY(target, "create maat instance failed.");

    int ret = maat_plugin_table_ex_schema_register(target, "DATAPATH_TELEMETRY_JOB", telemetry_job_add_cb,
                                                   telemetry_job_del_cb, NULL, 0, NULL);
    DP_TRACE_VERIFY(ret == 0, "failed at register callback of DATAPATH_TELEMETRY_JOB.");
    dzlog_info("data path trace maat init end");
}

bool maat_rule_parse(const char * table_line, struct dp_trace_telemetry_desc * telemetry_desc)
{
    struct dp_trace_job_desc * job_desc = &telemetry_desc->job_desc;
    bool valid_rule = false;

    const struct config * conf = global_config_get();

    char uuid[40] = {};
    unsigned int pkt_cnt_max = 0;
    char bpf_expr[MR_BPF_EXPRESSION_MAX] = "";
    unsigned int timeout = 0;
    unsigned int sampling = 0;
    unsigned int snaplen = 0;
    unsigned int with_packet_capture = 0;
    char device_group[4096] = "";
    char traffic_link_id[128] = "";
    unsigned int is_valid = 1;

    cJSON * device_group_json = NULL;
    cJSON * traffic_json = NULL;
    int ret = 0;

    dzlog_info("telemetry add maat parse config rule:%s", table_line);
    ret = sscanf(table_line, "%s\t%u\t%127s\t%u\t%u\t%u\t%u\t%4095s\t%127s\t%u", uuid, &pkt_cnt_max, bpf_expr, &timeout,
                 &sampling, &snaplen, &with_packet_capture, device_group, traffic_link_id, &is_valid);
    if (ret != 10)
    {
        dzlog_warn("maat parse config failed. Not enough fields:%s", table_line);
        goto end;
    }

    bool device_group_match = false;
    if (strcasecmp(device_group, "{}") == 0)
    {
        dzlog_info("When device_group is empty, all devices are matched");
        device_group_match = true;
        goto device_group_match_end;
    }

    device_group_json = cJSON_Parse(device_group);
    if (device_group_json == NULL)
    {
        dzlog_error("parse to json failed:%s", device_group);
        goto device_group_match_end;
    }

    cJSON * tag_sets_item = cJSON_GetObjectItem(device_group_json, "tag_sets");
    if (!cJSON_IsArray(tag_sets_item))
    {
        dzlog_warn("tag_sets value is not array");
        goto device_group_match_end;
    }

    int tag_sets_array_size = cJSON_GetArraySize(tag_sets_item);
    for (int i = 0; i < tag_sets_array_size; i++)
    {
        cJSON * tag_sets_item_i = cJSON_GetArrayItem(tag_sets_item, i);
        if (!cJSON_IsArray(tag_sets_item_i))
        {
            dzlog_warn("tag_sets value %d is not array", i);
            goto device_group_match_end;
        }

        int tag_sets_item_i_array_size = cJSON_GetArraySize(tag_sets_item_i);
        for (int j = 0; j < tag_sets_item_i_array_size; j++)
        {
            cJSON * inner_item = cJSON_GetArrayItem(tag_sets_item_i, j);
            cJSON * inner_item_value = cJSON_GetObjectItem(inner_item, "value");
            if (!cJSON_IsArray(inner_item_value))
            {
                dzlog_warn("tag_sets %d-%d 'value' is not array", i, j);
                goto device_group_match_end;
            }

            int value_array_size = cJSON_GetArraySize(inner_item_value);
            for (int k = 0; k < value_array_size; k++)
            {
                cJSON * value_item_k = cJSON_GetArrayItem(inner_item_value, k);
                if (cJSON_IsString(value_item_k))
                {
                    if (strcasecmp(conf->device_group, value_item_k->valuestring) == 0)
                    {
                        device_group_match = true;
                        goto device_group_match_end;
                    }
                }
            }
        }
    }

device_group_match_end:
    if (device_group_match == false)
    {
        dzlog_info("This rule does not apply to the current device(%s)", conf->device_group);
        goto end;
    }

    if (is_valid == 0)
    {
        dzlog_warn("rule is not valid:%s", table_line);
        goto end;
    }

    ret = uuid_parse(uuid, telemetry_desc->uuid);
    if (ret != 0)
    {
        dzlog_error("uuid parsing failed: %s", uuid);
        goto end;
    }

    backspace_remove(bpf_expr, job_desc->bpf_expr);

    traffic_json = cJSON_Parse(traffic_link_id);
    if (traffic_json == NULL || !cJSON_IsArray(traffic_json))
    {
        dzlog_error("parse traffic link id string to json failed. the string is:%s", traffic_link_id);
        goto end;
    }

    int traffic_obj_array_size = cJSON_GetArraySize(traffic_json);
    int id_num_max = (traffic_obj_array_size < DP_TRACE_TRAFFIC_LINK_ID_ARRAY_SIZE_MAX)
                         ? traffic_obj_array_size
                         : DP_TRACE_TRAFFIC_LINK_ID_ARRAY_SIZE_MAX;
    for (int i = 0; i < id_num_max; i++)
    {
        cJSON * traffic_obj_item_i = cJSON_GetArrayItem(traffic_json, i);
        if (traffic_obj_item_i != NULL && cJSON_IsNumber(traffic_obj_item_i))
        {
            uint16_t value = (uint16_t)traffic_obj_item_i->valueint;
            job_desc->traffic_link_ids[job_desc->traffic_link_id_cnt++] = value;
        }
    }

    job_desc->pkt_cnt_max = pkt_cnt_max;
    job_desc->sampling = sampling;
    job_desc->snaplen = snaplen;
    job_desc->measurement_type = DP_TRACE_MEASUREMENT_TYPE_TELEMETRY;

    valid_rule = true;

end:
    if (device_group_json)
    {
        cJSON_Delete(device_group_json);
    }
    if (traffic_json)
    {
        cJSON_Delete(traffic_json);
    }
    return valid_rule;
}