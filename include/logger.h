#pragma once
#include "common.h"

extern unsigned int zlog_env_is_init;
void logger_init();
zlog_category_t * kafka_dump_logger_get();