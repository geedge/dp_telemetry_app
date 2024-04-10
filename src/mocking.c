#include "mocking.h"
#include "common.h"
#include <mpack.h>

rd_kafka_t * __wrap_kafka_handle_create(const char * brokerlist, const char * sasl_username, const char * sasl_passwd)
{
    return NULL;
}

rd_kafka_topic_t * __wrap_kafka_topic_new(rd_kafka_t * rk, const char * topic, rd_kafka_topic_conf_t * conf)
{
    return NULL;
}

int __wrap_kafka_produce(rd_kafka_topic_t * rkt, void * payload, size_t len)
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
        char source_ip[INET6_ADDRSTRLEN];
        int32_t source_port;
        char server_ip[INET6_ADDRSTRLEN];
        int32_t server_port;
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
    packet.microseconds = mpack_node_i64(mpack_node_map_cstr(root, "timestamp"));

    mpack_node_copy_cstr(mpack_node_map_cstr(root, "job_id"), packet.job_id_str, sizeof(packet.job_id_str));

    mpack_node_copy_cstr(mpack_node_map_cstr(root, "sled_ip"), packet.sled_ip, sizeof(packet.sled_ip));

    mpack_node_copy_cstr(mpack_node_map_cstr(root, "device_group"), packet.device_group, sizeof(packet.device_group));

    mpack_node_copy_cstr(mpack_node_map_cstr(root, "source_ip"), packet.source_ip, sizeof(packet.source_ip));

    packet.source_port = mpack_node_i32(mpack_node_map_cstr(root, "source_port"));

    mpack_node_copy_cstr(mpack_node_map_cstr(root, "server_ip"), packet.server_ip, sizeof(packet.server_ip));

    packet.server_port = mpack_node_i32(mpack_node_map_cstr(root, "server_port"));

    packet.packet = mpack_node_bin_data(mpack_node_map_cstr(root, "packet"));

    packet.packet_length = mpack_node_i32(mpack_node_map_cstr(root, "packet_length"));

    mpack_node_t measurements_val = mpack_node_map_cstr(root, "measurements");
    packet.measurements_num = mpack_node_array_length(mpack_node_map_cstr(root, "measurements"));
    for (unsigned int i = 0; i < packet.measurements_num; i++)
    {
        if (i >= 128)
        {
            printf("too many measurements...");
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
    printf("microseconds %ld\n", packet.microseconds);
    printf("job_id %s\n", packet.job_id_str);
    printf("sled_ip %s\n", packet.sled_ip);
    printf("device_group %s\n", packet.device_group);
    printf("source_ip %s\n", packet.source_ip);
    printf("source_port %d\n", packet.source_port);
    printf("server_ip %s\n", packet.server_ip);
    printf("server_port %d\n", packet.server_port);
    printf("packet_length %d\n", packet.packet_length);
    for (unsigned int i = 0; i < packet.measurements_num; i++)
    {
        printf("record %u:\n", i);
        printf("tv_sec %d\n", packet.record[i].tv_sec);
        printf("tv_nsec %d\n", packet.record[i].tv_nsec);
        printf("app %.*s\n", packet.record[i].app_len, packet.record[i].app);
        printf("comments %.*s\n", packet.record[i].comments_len, packet.record[i].comments);
    }

    free(payload);
    return 0;
}