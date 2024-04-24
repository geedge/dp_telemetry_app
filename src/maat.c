#include "maat.h"
#include "config.h"
#include "job_ctx.h"

#include <MESA/maat.h>

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

#if 0
    if (conf->deferred_load_on)
    {
        maat_options_set_deferred_load_on(opts);
    }
    dzlog_info("maat defere load on:%u", conf->deferred_load_on);
#endif

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