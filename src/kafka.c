#include "kafka.h"
#include "common.h"

rd_kafka_t * kafka_handle_create(const char * brokerlist, const char * sasl_username, const char * sasl_passwd)
{
    int ret;
    char kafka_errstr[1024] = {0};
    rd_kafka_t * handle = NULL;
    rd_kafka_conf_t * rconf = NULL;

    rconf = rd_kafka_conf_new();

    ret = rd_kafka_conf_set(rconf, "queue.buffering.max.messages", "1000", kafka_errstr, sizeof(kafka_errstr));
    if (ret != RD_KAFKA_CONF_OK)
    {
        dzlog_error("Error to set kafka \"queue.buffering.max.messages\", %s.", kafka_errstr);
        goto error;
    }
    ret = rd_kafka_conf_set(rconf, "topic.metadata.refresh.interval.ms", "600000", kafka_errstr, sizeof(kafka_errstr));
    if (ret != RD_KAFKA_CONF_OK)
    {
        dzlog_error("Error to set kafka \"topic.metadata.refresh.interval.ms\", %s.", kafka_errstr);
        goto error;
    }
    ret = rd_kafka_conf_set(rconf, "security.protocol", "plaintext", kafka_errstr, sizeof(kafka_errstr));
    if (ret != RD_KAFKA_CONF_OK)
    {
        dzlog_error("Error to set kafka \"security.protocol\", %s.", kafka_errstr);
        goto error;
    }

    if (strlen(sasl_username) > 0 && strlen(sasl_passwd) > 0)
    {

        ret = rd_kafka_conf_set(rconf, "security.protocol", "sasl_plaintext", kafka_errstr, sizeof(kafka_errstr));
        if (ret != RD_KAFKA_CONF_OK)
        {
            dzlog_error("Error to set kafka \"security.protocol\", %s.", kafka_errstr);
            goto error;
        }

        ret = rd_kafka_conf_set(rconf, "sasl.mechanisms", "PLAIN", kafka_errstr, sizeof(kafka_errstr));
        if (ret != RD_KAFKA_CONF_OK)
        {
            dzlog_error("Error to set kafka \"sasl.mechanisms\", %s.", kafka_errstr);
            goto error;
        }

        ret = rd_kafka_conf_set(rconf, "sasl.username", sasl_username, kafka_errstr, sizeof(kafka_errstr));
        if (ret != RD_KAFKA_CONF_OK)
        {
            dzlog_error("Error to set kafka \"sasl.username\", %s.", kafka_errstr);
            goto error;
        }
        ret = rd_kafka_conf_set(rconf, "sasl.password", sasl_passwd, kafka_errstr, sizeof(kafka_errstr));
        if (ret != RD_KAFKA_CONF_OK)
        {
            dzlog_error("Error to set kafka \"sasl.password\", %s.", kafka_errstr);
            goto error;
        }
    }

    // The conf object is freed by this function and must not be used or destroyed by the application sub-sequently.
    handle = rd_kafka_new(RD_KAFKA_PRODUCER, rconf, kafka_errstr, sizeof(kafka_errstr));
    rconf = NULL;
    if (handle == NULL)
    {
        dzlog_error("Error to new kafka, %s.", kafka_errstr);
        goto error;
    }

    if (rd_kafka_brokers_add(handle, brokerlist) == 0)
    {
        dzlog_error("Error to add kakfa bokers.");
        goto error;
    }

    return handle;

error:
    if (rconf != NULL)
    {
        rd_kafka_conf_destroy(rconf);
    }
    if (handle != NULL)
    {
        rd_kafka_destroy(handle);
    }
    return NULL;
}

rd_kafka_topic_t * kafka_topic_new(rd_kafka_t * rk, const char * topic, rd_kafka_topic_conf_t * conf)
{
    return rd_kafka_topic_new(rk, topic, conf);
}

int kafka_produce(rd_kafka_topic_t * rkt, void * payload, size_t len)
{
    return rd_kafka_produce(rkt, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_FREE, (void *)payload, len, NULL, 0, NULL);
}