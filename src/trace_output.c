#include "trace_output.h"
#include "common.h"
#include "config.h"
#include "job_ctx.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#define BURST_MAX 64

int dp_trace_file_mutex_lock(job_bitmap_t job_id);
int dp_trace_file_mutex_unlock(job_bitmap_t job_id);
bool dp_trace_file_reach_max_size(job_bitmap_t job_id);
void cli_job_mbufs_write_process(marsio_buff_t * mbufs[], int nr_mbufs, job_bitmap_t job_id);
void dp_trace_file_rollbak(job_bitmap_t job_id);

static rd_kafka_t * kafka_handle_create(const char * brokerlist, const char * sasl_username, const char * sasl_passwd);

struct dp_trace_output
{
    char * file_path;
    char * file_bak_path;
    char * file_middle_path;
    pcapng_file_t * pcapng;
    pthread_mutex_t file_mutex;
};

struct dp_trace_output dp_trace_output[DP_TRACE_JOB_NUM_MAX] = {};

void dp_trace_output_init()
{
    // pcapng file path init
    char * dp_trace_dir = g_conf->dp_trace_dir;
    int ret = mkdir(dp_trace_dir, 0755);
    if (ret != 0 && errno != EEXIST)
    {
        syslog(LOG_ERR, "Failed to create directory:%s.%s", dp_trace_dir, strerror(errno));
        exit(EXIT_FAILURE);
    }

    for (unsigned int i = 0; i < TELEMETRY_DIM(dp_trace_output); i++)
    {
        asprintf(&dp_trace_output[i].file_path, "%s/dp_trace_%u.pcapng", dp_trace_dir, i);
        asprintf(&dp_trace_output[i].file_bak_path, "%s/dp_trace_%u.pcapng.1", dp_trace_dir, i);
        asprintf(&dp_trace_output[i].file_middle_path, "%s/dp_trace_%u.pcapng.2", dp_trace_dir, i);
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    for (unsigned int i = 0; i < TELEMETRY_DIM(dp_trace_output); i++)
    {
        ret = pthread_mutex_init(&dp_trace_output[i].file_mutex, &attr);
        if (ret != 0)
        {
            syslog(LOG_ERR, "pthread_mutex_init failed(ret=%d)", ret);
            exit(EXIT_FAILURE);
        }
    }

    // kafka init
#if 0
    g_conf->kafka_handle = kafka_handle_create(g_conf->broker_list, g_conf->sasl_password, g_conf->sasl_username);
    g_conf->kafka_topic = rd_kafka_topic_new(g_conf->kafka_handle, g_conf->topic_name, NULL);
#endif
}

void * dp_trace_process_thread(void * arg)
{
    uintptr_t qid = (uintptr_t)arg;

    marsio_buff_t * rx_buff[BURST_MAX];
    unsigned int nr_burst = TELEMETRY_DIM(rx_buff);
    unsigned int nr_class_mbufs = nr_burst * DP_TRACE_JOB_NUM_MAX;
    marsio_buff_t * class_mbufs[nr_class_mbufs];
    int nr_jobs_mbufs[DP_TRACE_JOB_NUM_MAX];

    marsio_thread_init(mr_instance);

    while (1)
    {
        int nr_recv = marsio_dp_trace_recv_burst(mr_instance, qid, rx_buff, nr_burst);
        if (nr_recv == 0)
        {
            // todo: use epoll+eventfd
            sleep(1);
            continue;
        }

        marsio_dp_trace_classification(mr_instance, rx_buff, nr_recv, class_mbufs, nr_class_mbufs, nr_jobs_mbufs);

        for (unsigned int i = 0; i < 8; i++)
        {
            unsigned int nr_mbufs = nr_jobs_mbufs[i];
            if (nr_mbufs == 0)
            {
                continue;
            }

            for (unsigned int j = 0; j < nr_mbufs; j++)
            {
                rx_buff[j] = class_mbufs[i * nr_recv + j];
            }
            job_bitmap_t job_id = index_to_job_id(i);
            cli_job_mbufs_write_process(rx_buff, nr_mbufs, job_id);
        }

        for (unsigned int i = 8; i < 16; i++)
        {
            unsigned int nr_mbufs = nr_jobs_mbufs[i];
            if (nr_mbufs == 0)
            {
                continue;
            }

            for (unsigned int j = 0; j < nr_mbufs; j++)
            {
                rx_buff[j] = class_mbufs[i * nr_recv + j];
            }
            for (unsigned int j = 0; j < nr_mbufs; j++)
            {
                char * data;
                size_t size;
                marsio_dp_trace_decode_to_message_pack(mr_instance, rx_buff[j], &data, &size);
#if 0
                rd_kafka_produce(g_conf->kafka_topic, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_FREE, (void *)data, size,
                                 NULL, 0, NULL);
#endif
            }
        }
    }
}

void cli_job_mbufs_write_process(marsio_buff_t * mbufs[], int nr_mbufs, job_bitmap_t job_id)
{
    if (is_job_id_used(job_id) == 0)
    {
        // After the job stops, the remaining data packets in the ring are discarded.
        goto end;
    }

    if (dp_trace_file_mutex_lock(job_id) < 0)
    {
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

        pcapng_file_t * pcapng = marsio_dp_trace_pcapng_fopen(mr_instance, dp_trace_output[index].file_path);
        if (pcapng == NULL)
        {
            goto unlock;
        }
        dp_trace_output[index].pcapng = pcapng;
    }

    marsio_dp_trace_pcapng_write(mr_instance, dp_trace_output[index].pcapng, mbufs, nr_mbufs);

    if (dp_trace_file_reach_max_size(job_id))
    {
        dp_trace_file_rollbak(job_id);
    }

unlock:
    dp_trace_file_mutex_unlock(job_id);

end:
    marsio_dp_trace_free(mr_instance, mbufs, nr_mbufs);
    return;
}

bool dp_trace_file_reach_max_size(job_bitmap_t job_id)
{
    // max_size == 0 : unlimit write
    bool ret = false;
    unsigned int index = job_id_to_index(job_id);

    dp_trace_file_mutex_lock(job_id);

    unsigned int max_size = g_conf->dp_trace_file_max_size_in_KB / 2;

    if (max_size == 0)
    {
        ret = false;
        goto end;
    }

    struct stat file_stat;
    if (unlikely(stat(dp_trace_output[index].file_path, &file_stat) == -1))
    {
        syslog(LOG_ERR, "Failed to obtain data path trace file status.");
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

    marsio_dp_trace_pcapng_fclose(mr_instance, dp_trace_output[index].pcapng);
    dp_trace_output[index].pcapng = NULL;

    const char * file_path = dp_trace_output[index].file_path;
    const char * file_bak_path = dp_trace_output[index].file_bak_path;
    if (rename(file_path, file_bak_path) < 0)
    {
        syslog(LOG_ERR, "rename %s to %s failed. error info: %s", file_path, file_bak_path, strerror(errno));
    }

    pcapng_file_t * pcapng = marsio_dp_trace_pcapng_fopen(mr_instance, dp_trace_output[index].file_path);
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

    marsio_dp_trace_pcapng_fclose(mr_instance, dp_trace_output[index].pcapng);
    dp_trace_output[index].pcapng = NULL;

    const char * file_path = dp_trace_output[index].file_path;
    const char * file_middle_path = dp_trace_output[index].file_middle_path;
    const char * file_bak_path = dp_trace_output[index].file_bak_path;

    if (rename(file_path, file_middle_path) < 0)
    {
        syslog(LOG_ERR, "rename %s to %s failed. error info: %s", file_path, file_middle_path, strerror(errno));
        goto end;
    }

    char command[2 * PATH_MAX];
    snprintf(command, sizeof(command), "timeout -v %us mergecap -w %s %s %s 2>&1", g_conf->dp_trace_merge_timeout,
             file_path, file_middle_path, file_bak_path);
    syslog(LOG_INFO, "merge trace file: %s", command);

    FILE * fp;
    char buffer[1024];
    fp = popen(command, "r");
    if (fp == NULL)
    {
        syslog(LOG_ERR, "open pipe failed: %s", strerror(errno));
        goto end;
    }

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        syslog(LOG_ERR, "merge trace file output: %s", buffer);
    }

    pclose(fp);

    if (remove(file_middle_path) < 0)
    {
        syslog(LOG_ERR, "remove %s failed. error info: %s", file_middle_path, strerror(errno));
    }

    if (remove(file_bak_path) < 0)
    {
        syslog(LOG_ERR, "remove %s failed. error info: %s", file_bak_path, strerror(errno));
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
            syslog(LOG_ERR, "EOWNERDEAD -> job ctx unlock failed");
            return -1;
        }
    }
    else if (ret != 0)
    {
        syslog(LOG_ERR, "job ctx lock failed");
        return -1;
    }
    return 0;
}

int dp_trace_file_mutex_unlock(job_bitmap_t job_id)
{
    unsigned int index = job_id_to_index(job_id);
    return pthread_mutex_unlock(&dp_trace_output[index].file_mutex);
}

static rd_kafka_t * kafka_handle_create(const char * brokerlist, const char * sasl_username, const char * sasl_passwd)
{
    int ret;
    char kafka_errstr[1024] = {0};
    rd_kafka_t * handle = NULL;
    rd_kafka_conf_t * rconf = NULL;

    rconf = rd_kafka_conf_new();

    ret = rd_kafka_conf_set(rconf, "queue.buffering.max.messages", "1000000", kafka_errstr, sizeof(kafka_errstr));
    if (ret != RD_KAFKA_CONF_OK)
    {
        syslog(LOG_ERR, "Error to set kafka \"queue.buffering.max.messages\", %s.", kafka_errstr);
        goto error;
    }
    ret = rd_kafka_conf_set(rconf, "topic.metadata.refresh.interval.ms", "600000", kafka_errstr, sizeof(kafka_errstr));
    if (ret != RD_KAFKA_CONF_OK)
    {
        syslog(LOG_ERR, "Error to set kafka \"topic.metadata.refresh.interval.ms\", %s.", kafka_errstr);
        goto error;
    }

#if 0
    assert(strlen(sasl_username) > 0);
    assert(strlen(sasl_passwd) > 0);
#endif

    rd_kafka_conf_set(rconf, "security.protocol", "sasl_plaintext", kafka_errstr, sizeof(kafka_errstr));
    rd_kafka_conf_set(rconf, "sasl.mechanisms", "PLAIN", kafka_errstr, sizeof(kafka_errstr));
    ret = rd_kafka_conf_set(rconf, "sasl.username", sasl_username, kafka_errstr, sizeof(kafka_errstr));
    if (ret != RD_KAFKA_CONF_OK)
    {
        syslog(LOG_ERR, "Error to set kafka \"sasl.username\", %s.", kafka_errstr);
        goto error;
    }
    ret = rd_kafka_conf_set(rconf, "sasl.password", sasl_passwd, kafka_errstr, sizeof(kafka_errstr));
    if (ret != RD_KAFKA_CONF_OK)
    {
        syslog(LOG_ERR, "Error to set kafka \"sasl.password\", %s.", kafka_errstr);
        goto error;
    }

    // The conf object is freed by this function and must not be used or destroyed by the application sub-sequently.
    handle = rd_kafka_new(RD_KAFKA_PRODUCER, rconf, kafka_errstr, sizeof(kafka_errstr));
    rconf = NULL;
    if (handle == NULL)
    {
        syslog(LOG_ERR, "Error to new kafka, %s.", kafka_errstr);
        goto error;
    }

    if (rd_kafka_brokers_add(handle, brokerlist) == 0)
    {
        syslog(LOG_ERR, "Error to add kakfa bokers.");
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