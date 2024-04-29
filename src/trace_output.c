#include "trace_output.h"
#include "common.h"
#include "config.h"
#include "job_ctx.h"
#include "kafka.h"
#include "logger.h"
#include "mocking.h"
#include "monit.h"
#include "pcapng.h"

#include <mpack.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

struct pkt_inner_ip_port
{
    char src_addr_str[INET6_ADDRSTRLEN];
    char dst_addr_str[INET6_ADDRSTRLEN];
    int32_t src_port;
    int32_t dst_port;
};

#define BURST_MAX 64

static rd_kafka_t * kafka_handle = NULL;
static rd_kafka_topic_t * kafka_topic = NULL;
__thread struct record_saving_stat * saving_stat = NULL;

int dp_trace_file_mutex_lock(job_bitmap_t job_id);
int dp_trace_file_mutex_unlock(job_bitmap_t job_id);
bool dp_trace_file_reach_max_size(job_bitmap_t job_id);
void cli_job_mbufs_write_process(marsio_buff_t * mbufs[], int nr_mbufs, job_bitmap_t job_id);
void dp_trace_file_rollbak(job_bitmap_t job_id);
int dp_trace_classification(struct mr_instance * instance, marsio_buff_t * mbufs[], int nr_mbufs,
                            marsio_buff_t * class_mbufs[DP_TRACE_JOB_NUM_MAX][nr_mbufs],
                            int nr_jobs_mbufs[DP_TRACE_JOB_NUM_MAX]);
static void dp_trace_decode_to_message_pack(marsio_buff_t * mr_mbuf, char ** data, size_t * size, job_bitmap_t job_id);
static int dp_trace_record_decode_to_str(marsio_buff_t * mr_mbuf, char * data, unsigned int size);
int marsio_pkt_inner_ip_port_get(const marsio_buff_t * mbuf, struct pkt_inner_ip_port * info);
static void thread_id_to_ring_id_calculate(unsigned int nr_thread, unsigned int nr_ring, unsigned int thread_id,
                                           unsigned int qids[], unsigned int * nr_qids);

struct dp_trace_output
{
    char * file_path;
    char * file_bak_path;
    char * file_middle_path;
    struct pcapng_t * pcapng;
    pthread_mutex_t file_mutex;
    unsigned int comment_max_size;
    char * comment;
    struct pcapng_enhance_packet_block * epbs[BURST_MAX];
};

static struct dp_trace_output dp_trace_output[DP_TRACE_JOB_NUM_MAX] = {};

void dp_trace_output_init()
{
    const struct config * conf = global_config_get();

    // pcapng file path init
    const char * dp_trace_dir = conf->dp_trace_dir;
    int ret = mkdir(dp_trace_dir, 0755);
    DP_TRACE_VERIFY(ret == 0 || errno == EEXIST, "Failed to create directory:%s.%s", dp_trace_dir, strerror(errno));

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    for (unsigned int i = 0; i < TELEMETRY_DIM(dp_trace_output); i++)
    {
        asprintf(&dp_trace_output[i].file_path, "%s/dp_trace_%u.pcapng", dp_trace_dir, i);
        asprintf(&dp_trace_output[i].file_bak_path, "%s/dp_trace_%u.pcapng.1", dp_trace_dir, i);
        asprintf(&dp_trace_output[i].file_middle_path, "%s/dp_trace_%u.pcapng.2", dp_trace_dir, i);

        ret = pthread_mutex_init(&dp_trace_output[i].file_mutex, &attr);
        DP_TRACE_VERIFY(ret == 0, "pthread_mutex_init failed(ret = % d)", ret);
    }

    kafka_handle = kafka_handle_create(conf->broker_list, conf->sasl_username, conf->sasl_password);
    kafka_topic = kafka_topic_new(kafka_handle, conf->topic_name, NULL);

    for (unsigned int i = 0; i < TELEMETRY_DIM(dp_trace_output); i++)
    {
        dp_trace_output[i].comment_max_size = 8192;
        dp_trace_output[i].comment = malloc(dp_trace_output[i].comment_max_size);
        DP_TRACE_VERIFY(dp_trace_output[i].comment != NULL, "malloc failed in dp_trace_output_init:%s",
                        strerror(errno));
    }
}

void * dp_trace_process_thread(void * arg)
{
    uintptr_t thread_id = (uintptr_t)arg;
    const struct config * conf = global_config_get();
    saving_stat = record_saving_stat_point_get(thread_id);

    unsigned int nr_thread = CPU_COUNT(&conf->cpu_set_io);
    unsigned int nr_ring = DP_TRACE_RING_NUM;

    unsigned int qids[nr_ring];
    unsigned int nr_qids = 0;
    thread_id_to_ring_id_calculate(nr_thread, nr_ring, thread_id, qids, &nr_qids);
    for (unsigned int i = 0; i < nr_qids; i++)
    {
        dzlog_info("thread %u receive ring %u packet", thread_id, qids[i]);
    }

    marsio_buff_t * rx_buff[BURST_MAX];
    marsio_buff_t * tx_buff[BURST_MAX];
    marsio_buff_t * class_mbufs[DP_TRACE_JOB_NUM_MAX][BURST_MAX];
    int nr_jobs_mbufs[DP_TRACE_JOB_NUM_MAX];

    marsio_thread_init(mr_instance);

    zlog_category_t * kafak_dump_logger = kafka_dump_logger_get();

    unsigned int no_pkt_recv_cnt = 0;
    for (unsigned int qid_index = 0;; qid_index++)
    {
        qid_index = qid_index % nr_qids;

        unsigned int nr_recv =
            marsio_dp_trace_mbuf_recv_burst(mr_instance, qids[qid_index], rx_buff, TELEMETRY_DIM(rx_buff));
        if (nr_recv == 0)
        {
            no_pkt_recv_cnt++;
            if (no_pkt_recv_cnt == nr_qids)
            {
                no_pkt_recv_cnt = 0;
                sleep(1);
            }
            continue;
        }

        no_pkt_recv_cnt = 0;
        saving_stat->recv_success += nr_recv;

        dp_trace_classification(mr_instance, rx_buff, nr_recv, class_mbufs, nr_jobs_mbufs);

        for (unsigned int i = 0; i < DP_TRACE_JOB_NUM_MAX; i++)
        {
            const unsigned int nr_mbufs = nr_jobs_mbufs[i];
            if (nr_mbufs == 0)
            {
                continue;
            }

            uint8_t role = job_id_role_get(index_to_job_id(i));
            if (role == DP_TRACE_ROLE)
            {
                for (unsigned int j = 0; j < nr_mbufs; j++)
                {
                    tx_buff[j] = class_mbufs[i][j];
                }
                job_bitmap_t job_id = index_to_job_id(i);
                cli_job_mbufs_write_process(tx_buff, nr_mbufs, job_id);
            }
            else if (role == DP_TELEMETRY_ROLE)
            {
                for (unsigned int j = 0; j < nr_mbufs; j++)
                {
                    char * data = NULL;
                    size_t size;
                    job_bitmap_t job_id = index_to_job_id(i);
                    dp_trace_decode_to_message_pack(class_mbufs[i][j], &data, &size, job_id);
                    if (data == NULL)
                    {
                        saving_stat->save_to_kafka_failed_at_decode_messagepack++;
                        continue;
                    }

                    if (conf->kafka_dump_to_log)
                    {
                        kafka_dump_to_log(kafak_dump_logger, (void *)data, size);
                    }

                    int ret = kafka_produce(kafka_topic, (void *)data, size);
                    if (ret != 0)
                    {
                        saving_stat->save_to_kafka_failed_at_send++;
                    }
                    else
                    {
                        saving_stat->save_to_kafka_success++;
                    }
                }
            }
            else
            {
                saving_stat->save_failed_at_job_deleted += nr_mbufs;
                // dzlog_info("The job has been deleted. The trace content corresponding to the job has been
                // discarded.");
            }

            marsio_dp_trace_mbuf_free(mr_instance, class_mbufs[i], nr_mbufs);
        }
    }
}

int dp_trace_classification(struct mr_instance * instance, marsio_buff_t * mbufs[], int nr_mbufs,
                            marsio_buff_t * class_mbufs[DP_TRACE_JOB_NUM_MAX][BURST_MAX],
                            int nr_jobs_mbufs[DP_TRACE_JOB_NUM_MAX])
{
    memset((void *)nr_jobs_mbufs, 0, DP_TRACE_JOB_NUM_MAX * sizeof(int));

    for (unsigned int i = 0; i < nr_mbufs; i++)
    {
        struct dp_trace_buffer_telemetry info;
        marsio_dp_trace_buffer_info_get((struct rte_mbuf *)mbufs[i], &info);

        int refcnt = -1;

        job_bitmap_t jobs_id = info.jobs_id;
        refcnt += __builtin_popcount(jobs_id & UINT16_MAX);
        if (refcnt > 0)
        {
            marsio_dp_trace_mbuf_refcnt_update(mbufs[i], refcnt);
        }

        for (unsigned int j = 0; j < DP_TRACE_JOB_NUM_MAX; j++)
        {
            uint16_t job_id_mask = index_to_job_id(j);
            uint16_t job_id = jobs_id & job_id_mask;
            if (job_id == 0)
            {
                continue;
            }

            unsigned int nr_job = nr_jobs_mbufs[j];
            class_mbufs[j][nr_job] = mbufs[i];
            nr_jobs_mbufs[j]++;
        }
    }

    return 0;
}

void cli_job_mbufs_write_process(marsio_buff_t * mbufs[], int nr_mbufs, job_bitmap_t job_id)
{
    int ret = 0;
    unsigned int nb_epb = 0;
    struct pcapng_enhance_packet_block * epbs[nr_mbufs];

    if (dp_trace_file_mutex_lock(job_id) < 0)
    {
        saving_stat->save_failed_at_mutex_lock;
        goto end;
    }

    if (is_job_id_used(job_id) == 0)
    {
        // After the job stops, the remaining data packets in the ring are discarded.
        saving_stat->save_failed_at_job_deleted += nr_mbufs;
        goto end;
    }

    unsigned int index = job_id_to_index(job_id);
    if (dp_trace_output[index].pcapng == NULL)
    {
        if (is_file_exists(dp_trace_output[index].file_path))
        {
            remove(dp_trace_output[index].file_path);
        }

        if (is_file_exists(dp_trace_output[index].file_bak_path))
        {
            remove(dp_trace_output[index].file_bak_path);
        }

        struct pcapng_t * pcapng = pcapng_open(dp_trace_output[index].file_path);
        if (pcapng == NULL)
        {
            saving_stat->save_to_file_failed_at_pcapng_open += nr_mbufs;
            goto unlock;
        }
        dp_trace_output[index].pcapng = pcapng;
    }

    for (unsigned int i = 0; i < nr_mbufs; i++)
    {
        char * comment = dp_trace_output[index].comment;
        unsigned int comment_max_size = dp_trace_output[index].comment_max_size;
        ret = dp_trace_record_decode_to_str(mbufs[i], comment, comment_max_size);
        if (ret < 0)
        {
            saving_stat->save_to_file_failed_at_decode_to_str++;
            continue;
        }

        struct dp_trace_buffer_telemetry trace_buff_info;
        marsio_dp_trace_buffer_info_get(mbufs[i], &trace_buff_info);
        ret = pcapng_copy(mbufs[i], trace_buff_info.snaplen, comment, &epbs[nb_epb]);
        if (ret >= 0)
        {
            nb_epb++;
        }
        else
        {
            saving_stat->save_to_file_failed_at_pcapng_format++;
        }
    }

    ret = pcapng_write_packets(dp_trace_output[index].pcapng, epbs, nb_epb);
    saving_stat->save_to_file_success += ret;
    saving_stat->save_to_file_failed_at_write_to_disk += (nb_epb - ret);

    if (dp_trace_file_reach_max_size(job_id))
    {
        dp_trace_file_rollbak(job_id);
    }

unlock:
    dp_trace_file_mutex_unlock(job_id);

end:
    for (unsigned int i = 0; i < nb_epb; i++)
    {
        free(epbs[i]);
    }
    return;
}

bool dp_trace_file_reach_max_size(job_bitmap_t job_id)
{
    // max_size == 0 : unlimit write
    bool ret = false;
    unsigned int index = job_id_to_index(job_id);

    dp_trace_file_mutex_lock(job_id);

    unsigned int max_size = global_config_get()->dp_trace_file_max_size_in_KB / 2;

    if (max_size == 0)
    {
        ret = false;
        goto end;
    }

    struct stat file_stat;
    if (unlikely(stat(dp_trace_output[index].file_path, &file_stat) == -1))
    {
        dzlog_error("Failed to obtain data path trace file status.");
        ret = true;
        goto end;
    }

    // unit is B -> KB
    if ((file_stat.st_size >> 10) >= max_size)
    {
        ret = true;
        goto end;
    }

end:
    dp_trace_file_mutex_unlock(job_id);
    return ret;
}

void dp_trace_file_rollbak(job_bitmap_t job_id)
{
    dp_trace_file_mutex_lock(job_id);

    unsigned int index = job_id_to_index(job_id);

    pcapng_close(dp_trace_output[index].pcapng);
    dp_trace_output[index].pcapng = NULL;

    const char * file_path = dp_trace_output[index].file_path;
    const char * file_bak_path = dp_trace_output[index].file_bak_path;
    if (rename(file_path, file_bak_path) < 0)
    {
        dzlog_error("rename %s to %s failed. error info: %s", file_path, file_bak_path, strerror(errno));
    }

    struct pcapng_t * pcapng = pcapng_open(file_path);

    if (pcapng == NULL)
    {
        goto unlock;
    }
    dp_trace_output[index].pcapng = pcapng;

unlock:
    dp_trace_file_mutex_unlock(job_id);
}

void dp_trace_pcapng_merger(job_bitmap_t job_id)
{
    dp_trace_file_mutex_lock(job_id);

    unsigned int index = job_id_to_index(job_id);

    if (!is_file_exists(dp_trace_output[index].file_bak_path))
    {
        // Only one file, no need to merge
        goto end;
    }

    pcapng_close(dp_trace_output[index].pcapng);
    dp_trace_output[index].pcapng = NULL;

    const char * file_path = dp_trace_output[index].file_path;
    const char * file_middle_path = dp_trace_output[index].file_middle_path;
    const char * file_bak_path = dp_trace_output[index].file_bak_path;

    if (rename(file_path, file_middle_path) < 0)
    {
        dzlog_error("rename %s to %s failed. error info: %s", file_path, file_middle_path, strerror(errno));
        goto end;
    }

    char command[2 * PATH_MAX];
    snprintf(command, sizeof(command), "timeout -v %us mergecap -w %s %s %s 2>&1",
             global_config_get()->dp_trace_merge_timeout, file_path, file_middle_path, file_bak_path);
    dzlog_info("merge trace file: %s", command);

    FILE * fp;
    char buffer[1024];
    fp = popen(command, "r");
    if (fp == NULL)
    {
        dzlog_error("open pipe failed: %s", strerror(errno));
        goto end;
    }

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        dzlog_error("merge trace file output: %s", buffer);
    }

    pclose(fp);

    if (remove(file_middle_path) < 0)
    {
        dzlog_error("remove %s failed. error info: %s", file_middle_path, strerror(errno));
    }

    if (remove(file_bak_path) < 0)
    {
        dzlog_error("remove %s failed. error info: %s", file_bak_path, strerror(errno));
    }

end:
    dp_trace_file_mutex_unlock(job_id);
}

int dp_trace_file_mutex_lock(job_bitmap_t job_id)
{
    unsigned int index = job_id_to_index(job_id);
    int ret = pthread_mutex_lock(&dp_trace_output[index].file_mutex);
    if (ret == EOWNERDEAD)
    {
        ret = pthread_mutex_consistent(&dp_trace_output[index].file_mutex);
        ret = pthread_mutex_unlock(&dp_trace_output[index].file_mutex);
        if (ret != 0)
        {
            dzlog_error("EOWNERDEAD -> job ctx unlock failed");
            return -1;
        }
    }
    else if (ret != 0)
    {
        dzlog_error("job ctx lock failed");
        return -1;
    }
    return 0;
}

int dp_trace_file_mutex_unlock(job_bitmap_t job_id)
{
    unsigned int index = job_id_to_index(job_id);
    return pthread_mutex_unlock(&dp_trace_output[index].file_mutex);
}

static void dp_trace_decode_to_message_pack(marsio_buff_t * mr_mbuf, char ** data, size_t * size, job_bitmap_t job_id)
{
    struct dp_trace_buffer_telemetry trace_buff_info;
    marsio_dp_trace_buffer_info_get(mr_mbuf, &trace_buff_info);

    unsigned int position = 0;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t microseconds = (uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;

    struct pkt_inner_ip_port pkt_info;
    marsio_pkt_inner_ip_port_get(mr_mbuf, &pkt_info);

    mpack_writer_t writer;
    mpack_writer_init_growable(&writer, data, size);
    mpack_build_map(&writer);

    mpack_write_cstr(&writer, "timestamp_us");
    mpack_write_i64(&writer, microseconds);

    uuid_t uuid;
    telemetry_job_uuid_get(job_id, uuid);
    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);
    mpack_write_cstr(&writer, "job_id");
    mpack_write_cstr(&writer, uuid_str);

    mpack_write_cstr(&writer, "sled_ip");
    if (global_config_get()->sled_ip != NULL)
    {
        mpack_write_cstr(&writer, global_config_get()->sled_ip);
    }
    else
    {
        mpack_write_cstr(&writer, "0.0.0.0");
    }

    mpack_write_cstr(&writer, "device_group");
    mpack_write_cstr(&writer, global_config_get()->device_group);

    mpack_write_cstr(&writer, "source_ip");
    mpack_write_cstr(&writer, pkt_info.src_addr_str);

    mpack_write_cstr(&writer, "source_port");
    mpack_write_i32(&writer, pkt_info.src_port);

    mpack_write_cstr(&writer, "destination_ip");
    mpack_write_cstr(&writer, pkt_info.dst_addr_str);

    mpack_write_cstr(&writer, "destination_port");
    mpack_write_i32(&writer, pkt_info.dst_port);

    mpack_write_cstr(&writer, "packet");
    mpack_write_bin(&writer, marsio_buff_mtod(mr_mbuf), marsio_buff_datalen(mr_mbuf));

    mpack_write_cstr(&writer, "packet_length");
    mpack_write_i32(&writer, marsio_buff_datalen(mr_mbuf));

    mpack_write_cstr(&writer, "measurements");
    mpack_build_array(&writer);
    while (position < trace_buff_info.buffer_used)
    {
        char * cur = trace_buff_info.buffer + position;

        const struct dp_trace_record_header * record_header = (struct dp_trace_record_header *)(cur);
        const char * comment = cur + sizeof(struct dp_trace_record_header);
        const unsigned int comment_len = record_header->recode_len;

        if ((record_header->measurement_type & DP_TRACE_MEASUREMENT_TYPE_TELEMETRY))
        {
            mpack_start_map(&writer, 4);

            mpack_write_cstr(&writer, "tv_sec");
            mpack_write_i32(&writer, record_header->ts.tv_sec);

            mpack_write_cstr(&writer, "tv_nsec");
            mpack_write_i32(&writer, record_header->ts.tv_nsec);

            mpack_write_cstr(&writer, "app");
            mpack_write_cstr(&writer, record_header->appsym);

            mpack_write_cstr(&writer, "comments");
            mpack_write_str(&writer, comment, comment_len);

            mpack_finish_map(&writer);
        }

        position += sizeof(struct dp_trace_record_header) + comment_len;
    }
    mpack_complete_array(&writer);

    mpack_complete_map(&writer);

    /* finish writing */
    if (mpack_writer_destroy(&writer) != mpack_ok)
    {
        dzlog_error("An error occurred during the data path decode to message pack!");
    }
}

static int dp_trace_record_decode_to_str(marsio_buff_t * mr_mbuf, char * data, unsigned int size)
{
    struct dp_trace_buffer_telemetry trace_buff_info;
    marsio_dp_trace_buffer_info_get(mr_mbuf, &trace_buff_info);
    unsigned int position = 0;

#ifndef NDEBUG
    unsigned int comment_cnt = 0;
    unsigned int trace_comment_cnt = 0;
#endif

    while (position < trace_buff_info.buffer_used)
    {
        char * cur = trace_buff_info.buffer + position;

        const struct dp_trace_record_header * record_header = (struct dp_trace_record_header *)(cur);
        const char * str = cur + sizeof(struct dp_trace_record_header);
        const unsigned int str_len = record_header->recode_len;

        if (record_header->measurement_type & DP_TRACE_MEASUREMENT_TYPE_TRACE)
        {
            int n = snprintf(data, size, "[%s:%s:] %ld.%ld ", record_header->appsym, record_header->module,
                             record_header->ts.tv_sec, record_header->ts.tv_nsec);
            if (unlikely(n < 0 || n >= size))
                return -1;
            size -= n;
            data += n;

            if (unlikely(size - 2 < str_len))
                return -1;
            memcpy(data, str, str_len);
            size -= str_len;
            data += str_len;

            *data = '\n';
            size--;
            data++;
#ifndef NDEBUG
            trace_comment_cnt++;
#endif
        }

        position += sizeof(struct dp_trace_record_header) + str_len;
#ifndef NDEBUG
        comment_cnt++;
#endif
    }

    if (size < 1)
        return -1;

#ifndef NDEBUG
    uint16_t avali = trace_buff_info.buffer_len - trace_buff_info.buffer_used;
    snprintf(data, size, "used: %u, avali: %u, all comment: %u, trace comment %u", trace_buff_info.buffer_used, avali,
             comment_cnt, trace_comment_cnt);
#else
    *data = '\0';
#endif

    return 0;
}

int marsio_pkt_inner_ip_port_get(const marsio_buff_t * mbuf, struct pkt_inner_ip_port * info)
{
    memset(info, 0, sizeof(struct pkt_inner_ip_port));

    // get inner ip
    void * ipv4_ptr = NULL;
    void * ipv6_ptr = NULL;
    uint16_t l3_layer_type_id = 0;

    ipv4_ptr = marsio_pkt_jump_to_innermost_layer(mbuf, LAYER_TYPE_ID_IPV4);
    ipv6_ptr = marsio_pkt_jump_to_innermost_layer(mbuf, LAYER_TYPE_ID_IPV6);

    if (ipv4_ptr > ipv6_ptr)
    {
        l3_layer_type_id = LAYER_TYPE_ID_IPV4;
    }
    else if (ipv6_ptr > ipv4_ptr)
    {
        l3_layer_type_id = LAYER_TYPE_ID_IPV6;
    }

    if (l3_layer_type_id == LAYER_TYPE_ID_IPV4)
    {
        struct iphdr * ipv4_hdr = (struct iphdr *)ipv4_ptr;
        inet_ntop(AF_INET, &ipv4_hdr->saddr, info->src_addr_str, sizeof(info->src_addr_str));
        inet_ntop(AF_INET, &ipv4_hdr->daddr, info->dst_addr_str, sizeof(info->dst_addr_str));
    }
    else if (l3_layer_type_id == LAYER_TYPE_ID_IPV6)
    {
        struct ip6_hdr * ipv6_hdr = (struct ip6_hdr *)ipv6_ptr;
        inet_ntop(AF_INET6, &ipv6_hdr->ip6_src, info->src_addr_str, sizeof(info->src_addr_str));
        inet_ntop(AF_INET6, &ipv6_hdr->ip6_dst, info->dst_addr_str, sizeof(info->dst_addr_str));
    }

    // get inner tcp or udp port
    void * tcp_ptr = NULL;
    void * udp_ptr = NULL;
    uint16_t l4_layer_type_id = 0;

    tcp_ptr = marsio_pkt_jump_to_innermost_layer(mbuf, LAYER_TYPE_ID_TCP);
    udp_ptr = marsio_pkt_jump_to_innermost_layer(mbuf, LAYER_TYPE_ID_UDP);

    if (tcp_ptr > udp_ptr)
    {
        l4_layer_type_id = LAYER_TYPE_ID_TCP;
    }
    else if (udp_ptr > tcp_ptr)
    {
        l4_layer_type_id = LAYER_TYPE_ID_UDP;
    }

    if (l4_layer_type_id == LAYER_TYPE_ID_TCP)
    {
        struct tcphdr * tcp_hdr = (struct tcphdr *)tcp_ptr;
        info->src_port = tcp_hdr->th_sport;
        info->dst_port = tcp_hdr->th_dport;
    }
    else if (l4_layer_type_id == LAYER_TYPE_ID_UDP)
    {
        struct udphdr * udp_hdr = (struct udphdr *)udp_ptr;
        info->src_port = udp_hdr->uh_sport;
        info->dst_port = udp_hdr->uh_dport;
    }

    return 0;
}

static void thread_id_to_ring_id_calculate(unsigned int nr_thread, unsigned int nr_ring, unsigned int thread_id,
                                           unsigned int qids[], unsigned int * nr_qids)
{
    // nr_thread threads; nr_ring rings; The current thread id is thread_id
    // Calculate the ring subscript that the current thread needs to process
    *nr_qids = 0;

    if (nr_thread >= nr_ring)
    {
        qids[0] = thread_id % nr_ring;
        *nr_qids = 1;
        return;
    }

    unsigned int numbers[nr_thread];
    int avg = nr_ring / nr_thread;
    int remainder = nr_ring % nr_thread;
    for (unsigned i = 0; i < nr_thread; i++)
    {
        numbers[i] = avg;
        if (remainder > 0)
        {
            numbers[i]++;
            remainder--;
        }
    }

    int prefix_index = -1;
    for (int i = 0; i < thread_id; i++)
    {
        prefix_index += numbers[i];
    }

    for (int i = 0; i < numbers[thread_id]; i++)
    {
        qids[i] = (++prefix_index);
        (*nr_qids)++;
    }
}