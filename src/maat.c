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
    char device_group[4096] = "";

    cJSON * table_json = NULL;
    int ret = 0;

    dzlog_info("telemetry parse config rule:%s", table_line);

    table_json = cJSON_Parse(table_line);
    if (table_json == NULL)
    {
        dzlog_error("Parse to json failed. table_json fields are missing.");
        valid_rule = false;
        goto end;
    }

    cJSON * uuid_item = cJSON_GetObjectItem(table_json, "job_id");
    if (uuid_item == NULL)
    {
        dzlog_error("Parse to json failed. job_id fields are missing.");
        valid_rule = false;
        goto end;
    }
    snprintf(uuid, sizeof(uuid), "%s", uuid_item->valuestring);

    cJSON * max_packet_item = cJSON_GetObjectItem(table_json, "max_packet");
    if (max_packet_item == NULL)
    {
        dzlog_error("Parse to json failed. max_packet fields are missing.");
        valid_rule = false;
        goto end;
    }
    pkt_cnt_max = max_packet_item->valueint;

    cJSON * filter_rule_item = cJSON_GetObjectItem(table_json, "filter_rule");
    if (filter_rule_item == NULL)
    {
        dzlog_error("Parse to json failed. filter_rule fields are missing.");
        valid_rule = false;
        goto end;
    }
    snprintf(bpf_expr, sizeof(bpf_expr), "%s", filter_rule_item->valuestring);

    cJSON * sampling_item = cJSON_GetObjectItem(table_json, "sampling");
    if (sampling_item == NULL)
    {
        dzlog_error("Parse to json failed. sampling fields are missing.");
        valid_rule = false;
        goto end;
    }
    sampling = sampling_item->valueint;

    cJSON * snaplen_item = cJSON_GetObjectItem(table_json, "snaplen");
    if (snaplen_item == NULL)
    {
        dzlog_error("Parse to json failed. snaplen fields are missing.");
        valid_rule = false;
        goto end;
    }
    snaplen = snaplen_item->valueint;

    cJSON * device_group_item = cJSON_GetObjectItem(table_json, "device_group");
    if (device_group_item == NULL)
    {
        dzlog_error("Parse to json failed. device_group fields are missing.");
        valid_rule = false;
        goto end;
    }

    bool device_group_or_date_center_match = false;
    if (cJSON_GetArraySize(device_group_item) == 0)
    {
        dzlog_info("When device_group is empty, all devices are matched");
        device_group_or_date_center_match = true;
        goto device_group_or_date_center_match_end;
    }

    cJSON * tag_sets_item = cJSON_GetObjectItem(device_group_item, "tag_sets");
    if (!cJSON_IsArray(tag_sets_item))
    {
        dzlog_warn("tag_sets value is not array");
        goto device_group_or_date_center_match_end;
    }

    int tag_sets_array_size = cJSON_GetArraySize(tag_sets_item);
    for (int i = 0; i < tag_sets_array_size; i++)
    {
        cJSON * tag_set_item = cJSON_GetArrayItem(tag_sets_item, i);
        if (!cJSON_IsArray(tag_set_item))
        {
            dzlog_warn("tag_sets value %d is not array", i);
            goto device_group_or_date_center_match_end;
        }

        int tag_set_item_array_size = cJSON_GetArraySize(tag_set_item);
        for (int j = 0; j < tag_set_item_array_size; j++)
        {
            cJSON * tag_set_j_item = cJSON_GetArrayItem(tag_set_item, j);

            cJSON * tag_item = cJSON_GetObjectItem(tag_set_j_item, "tag");
            if (tag_item == NULL)
            {
                dzlog_warn("tag_sets %d no 'tag'", i);
                continue;
            }

            const char * tag_key_str = cJSON_GetStringValue(tag_item);
            const char * tag_val_str = NULL;
            if (strcasecmp(tag_key_str, "device_group") == 0)
            {
                tag_val_str = conf->device_group;
            }
            else if (strcasecmp(tag_key_str, "data_center") == 0)
            {
                tag_val_str = conf->data_center;
            }

            cJSON * valuer_item = cJSON_GetObjectItem(tag_set_j_item, "value");
            if (!cJSON_IsArray(valuer_item))
            {
                dzlog_warn("tag_sets %d-%d 'value' is not array", i, j);
                continue;
            }

            int value_array_size = cJSON_GetArraySize(valuer_item);
            for (int k = 0; k < value_array_size; k++)
            {
                cJSON * value_item_k = cJSON_GetArrayItem(valuer_item, k);
                if (cJSON_IsString(value_item_k))
                {
                    if (strcasecmp(tag_val_str, value_item_k->valuestring) == 0)
                    {
                        device_group_or_date_center_match = true;
                        goto device_group_or_date_center_match_end;
                    }
                }
            }
        }
    }

device_group_or_date_center_match_end:
    if (device_group_or_date_center_match == false)
    {
        dzlog_info("This rule does not apply to the current center(%s) or device(%s)", conf->data_center,
                   conf->device_group);
        goto end;
    }

    ret = uuid_parse(uuid, telemetry_desc->uuid);
    if (ret != 0)
    {
        dzlog_error("uuid parsing failed: %s", uuid);
        goto end;
    }

    bpf_str_unescape_for_cm(bpf_expr, job_desc->bpf_expr);

    cJSON * traffic_link_id_item = cJSON_GetObjectItem(table_json, "traffic_link_id");
    if (traffic_link_id_item == NULL || !cJSON_IsArray(traffic_link_id_item))
    {
        dzlog_error("parse traffic_link_id field failed.");
        goto end;
    }

    int traffic_obj_array_size = cJSON_GetArraySize(traffic_link_id_item);
    int id_num_max = (traffic_obj_array_size < DP_TRACE_TRAFFIC_LINK_ID_ARRAY_SIZE_MAX)
                         ? traffic_obj_array_size
                         : DP_TRACE_TRAFFIC_LINK_ID_ARRAY_SIZE_MAX;
    for (int i = 0; i < id_num_max; i++)
    {
        cJSON * traffic_obj_item_i = cJSON_GetArrayItem(traffic_link_id_item, i);
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
    cJSON_Delete(table_json);
    return valid_rule;
}