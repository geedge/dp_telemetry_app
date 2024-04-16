#pragma once
#include <librdkafka/rdkafka.h>

rd_kafka_t * __wrap_kafka_handle_create(const char * brokerlist, const char * sasl_username, const char * sasl_passwd);
rd_kafka_topic_t * __wrap_kafka_topic_new(rd_kafka_t * rk, const char * topic, rd_kafka_topic_conf_t * conf);
int __wrap_kafka_produce(rd_kafka_topic_t * rkt, void * payload, size_t len);