#pragma once
#include "logger.h"
#include <librdkafka/rdkafka.h>

rd_kafka_t * kafka_handle_create(const char * brokerlist, const char * sasl_username, const char * sasl_passwd,
                                 uint32_t kafka_queue_size);
rd_kafka_topic_t * kafka_topic_new(rd_kafka_t * rk, const char * topic, rd_kafka_topic_conf_t * conf);
int kafka_produce(rd_kafka_topic_t * rkt, void * payload, size_t len);
int kafka_dump_to_log(zlog_category_t * logger, const void * payload, size_t len);