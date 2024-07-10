#include "kafka.h"
#include "common.h"
#include <errno.h>
#include <mpack.h>

rd_kafka_t * kafka_handle_create(const char * brokerlist, const char * sasl_username, const char * sasl_passwd,
                                 uint32_t kafka_queue_size)
{
    int ret;
    char kafka_errstr[1024] = {0};
    rd_kafka_t * handle = NULL;
    rd_kafka_conf_t * rconf = NULL;

    char kafka_queue_size_str[10];
    snprintf(kafka_queue_size_str, sizeof(kafka_queue_size_str), "%u", kafka_queue_size);

    rconf = rd_kafka_conf_new();

    ret = rd_kafka_conf_set(rconf, "queue.buffering.max.messages", kafka_queue_size_str, kafka_errstr,
                            sizeof(kafka_errstr));
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
    exit(EXIT_FAILURE);
}

rd_kafka_topic_t * kafka_topic_new(rd_kafka_t * rk, const char * topic, rd_kafka_topic_conf_t * conf)
{
    rd_kafka_topic_t * ret = rd_kafka_topic_new(rk, topic, conf);
    if (ret == NULL)
    {
        rd_kafka_resp_err_t err = rd_kafka_last_error();
        dzlog_error("rd_kafka_topic_new failed:%s", rd_kafka_err2str(err));
        exit(EXIT_FAILURE);
    }
    return ret;
}

int kafka_produce(rd_kafka_topic_t * rkt, void * payload, size_t len)
{
    // Automatically release payload
    // Even if the function fails to execute and returns -1, the payload will be released.
    int ret = rd_kafka_produce(rkt, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_FREE, (void *)payload, len, NULL, 0, NULL);
#if 0
// Avoid having too many same logs
    if (ret != 0)
    {
        rd_kafka_resp_err_t err = rd_kafka_last_error();
        dzlog_error("rd_kafka_topic_new failed:%s", rd_kafka_err2str(err));
    }
#endif
    return ret;
}

int kafka_dump_to_log(zlog_category_t * logger, const void * payload, size_t len)
{
    struct measurements
    {
        int32_t tv_sec;
        int32_t tv_nsec;
        const char * app;
        uint32_t app_len;
        const char * comments;
        uint32_t comments_len;
    };

    struct dp_trace_message_pack
    {
        int64_t microseconds;
        char job_id_str[MR_SYMBOL_MAX];
        char sled_ip[INET6_ADDRSTRLEN];
        char device_group[MR_SYMBOL_MAX];
        int32_t traffic_link_id;
        char source_ip[INET6_ADDRSTRLEN];
        int32_t source_port;
        char server_ip[INET6_ADDRSTRLEN];
        int32_t server_port;
        int32_t egress_action;
        const char * packet;
        int32_t packet_length;
        int32_t measurements_num;
        struct measurements record[128];
    };

    struct dp_trace_message_pack packet = {};

    mpack_tree_t tree;
    mpack_tree_init_data(&tree, payload, len);
    mpack_tree_parse(&tree);

    mpack_node_t root = mpack_tree_root(&tree);
    packet.microseconds = mpack_node_i64(mpack_node_map_cstr(root, "timestamp_us"));

    mpack_node_copy_cstr(mpack_node_map_cstr(root, "job_id"), packet.job_id_str, sizeof(packet.job_id_str));

    mpack_node_copy_cstr(mpack_node_map_cstr(root, "sled_ip"), packet.sled_ip, sizeof(packet.sled_ip));

    mpack_node_copy_cstr(mpack_node_map_cstr(root, "device_group"), packet.device_group, sizeof(packet.device_group));

    mpack_node_t traffic_link_id_node = mpack_node_map_cstr(root, "traffic_link_id");
    if (!mpack_node_is_nil(traffic_link_id_node))
    {
        packet.traffic_link_id = mpack_node_i32(traffic_link_id_node);
    }

    mpack_node_t source_ip_node = mpack_node_map_cstr(root, "source_ip");
    if (!mpack_node_is_nil(source_ip_node))
    {
        mpack_node_copy_cstr(source_ip_node, packet.source_ip, sizeof(packet.source_ip));
    }

    mpack_node_t source_port_node = mpack_node_map_cstr(root, "source_port");
    if (!mpack_node_is_nil(source_port_node))
    {
        packet.source_port = mpack_node_i32(source_port_node);
    }

    mpack_node_t destination_ip_node = mpack_node_map_cstr(root, "destination_ip");
    if (!mpack_node_is_nil(destination_ip_node))
    {
        mpack_node_copy_cstr(destination_ip_node, packet.server_ip, sizeof(packet.server_ip));
    }

    mpack_node_t destination_port_node = mpack_node_map_cstr(root, "destination_port");
    if (!mpack_node_is_nil(destination_port_node))
    {
        packet.server_port = mpack_node_i32(destination_port_node);
    }

    packet.egress_action = mpack_node_i32(mpack_node_map_cstr(root, "egress_action"));

    packet.packet = mpack_node_bin_data(mpack_node_map_cstr(root, "packet"));

    packet.packet_length = mpack_node_i32(mpack_node_map_cstr(root, "packet_length"));

    mpack_node_t measurements_val = mpack_node_map_cstr(root, "measurements");
    packet.measurements_num = mpack_node_array_length(mpack_node_map_cstr(root, "measurements"));
    for (int i = 0; i < packet.measurements_num; i++)
    {
        if (i >= 128)
        {
            zlog_debug(logger, "too many measurements...");
            continue;
        }
        mpack_node_t measurement = mpack_node_array_at(measurements_val, i);
        packet.record[i].tv_sec = mpack_node_i32(mpack_node_map_cstr(measurement, "tv_sec"));
        packet.record[i].tv_nsec = mpack_node_i32(mpack_node_map_cstr(measurement, "tv_nsec"));
        packet.record[i].app = mpack_node_str(mpack_node_map_cstr(measurement, "app"));
        packet.record[i].app_len = mpack_node_strlen(mpack_node_map_cstr(measurement, "app"));
        packet.record[i].comments = mpack_node_str(mpack_node_map_cstr(measurement, "comments"));
        packet.record[i].comments_len = mpack_node_strlen(mpack_node_map_cstr(measurement, "comments"));
    }

    // print
    zlog_debug(logger, "microseconds %ld", packet.microseconds);
    zlog_debug(logger, "job_id %s", packet.job_id_str);
    zlog_debug(logger, "sled_ip %s", packet.sled_ip);
    zlog_debug(logger, "device_group %s", packet.device_group);
    zlog_debug(logger, "traffic_link_id %d", packet.traffic_link_id);

    if (strlen(packet.source_ip) == 0)
    {
        zlog_debug(logger, "source_ip is empty");
    }
    else
    {
        zlog_debug(logger, "source_ip %s", packet.source_ip);
    }

    zlog_debug(logger, "source_port %d", packet.source_port);

    if (strlen(packet.server_ip) == 0)
    {
        zlog_debug(logger, "server_ip is empty");
    }
    else
    {
        zlog_debug(logger, "server_ip %s", packet.server_ip);
    }

    zlog_debug(logger, "server_port %d", packet.server_port);

    zlog_debug(logger, "egress_action %d", packet.egress_action);

    zlog_debug(logger, "packet_length %d", packet.packet_length);

    if (packet.measurements_num == 0)
    {
        zlog_debug(logger, "measurements num is zero");
    }

    for (int i = 0; i < packet.measurements_num; i++)
    {
        zlog_debug(logger, "record %u:", i);
        zlog_debug(logger, "tv_sec %d", packet.record[i].tv_sec);
        zlog_debug(logger, "tv_nsec %d", packet.record[i].tv_nsec);
        zlog_debug(logger, "app %.*s", packet.record[i].app_len, packet.record[i].app);
        zlog_debug(logger, "comments %.*s", packet.record[i].comments_len, packet.record[i].comments);
    }

    return 0;
}