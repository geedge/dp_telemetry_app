#include "logger.h"
#include "config.h"

unsigned int zlog_env_is_init = 0;

void logger_init()
{
    const struct config * conf = global_config_get();

    int ret = dzlog_init(conf->zlog_config_path, "default_zlog_category");
    if (ret != 0)
    {
        char * zlog_profile_error = getenv("ZLOG_PROFILE_ERROR");
        printf("fail in dzlog_int.\n");
        if (zlog_profile_error != NULL)
        {
            printf("The dzlog_init function error infomation is recorded in:%s\n", zlog_profile_error);
        }
        exit(EXIT_FAILURE);
    }
    zlog_env_is_init = 1;

    if (conf->kafka_dump_to_log)
    {
        ret = zlog_init(conf->zlog_config_path);
        DP_TRACE_VERIFY(ret != 0, "zlog init failed.");
    }
}

zlog_category_t * kafka_dump_logger_get()
{
    const struct config * conf = global_config_get();

    if (conf->kafka_dump_to_log)
    {
        zlog_category_t * logger = zlog_get_category("kafka_dump");
        DP_TRACE_VERIFY(logger != NULL, "kafka_dump logger is null.");
        return logger;
    }

    return NULL;
}