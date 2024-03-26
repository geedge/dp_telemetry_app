#include "job_ctx.h"
#include <MESA/maat.h>

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

int maat_register_hook()
{
}