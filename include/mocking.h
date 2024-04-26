#pragma once
#include <librdkafka/rdkafka.h>

int kafka_dump_to_log(rd_kafka_topic_t * rkt, const void * payload, size_t len);