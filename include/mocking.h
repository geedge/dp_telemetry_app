#pragma once
#include <librdkafka/rdkafka.h>
#include <zlog.h>

int kafka_dump_to_log(zlog_category_t * logger, const void * payload, size_t len);