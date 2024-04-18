#include "monit.h"
#include "config.h"
#include <cjson/cJSON.h>
#include <pthread.h>

static struct monit * monit = NULL;

static void monit_dump();
static void * monit_loop(void * args);

void monit_init(unsigned int thread_num)
{
    monit = calloc(1, sizeof(struct monit) + thread_num * sizeof(struct record_saving_stat));
    DP_TRACE_VERIFY(monit != NULL, "calloc failed.");
    monit->nr_stat = thread_num;

    pthread_t tid;
    int ret = pthread_create(&tid, NULL, monit_loop, NULL);
    DP_TRACE_VERIFY(ret == 0, "failed to create thread for monit_loop.return value:%d", ret);
}

struct record_saving_stat * record_saving_stat_point_get(unsigned int thread_id)
{
    assert(thread_id < monit->nr_stat);
    return &monit->savint_stats[thread_id];
}

static void monit_dump()
{
    const struct config * conf = global_config_get();

    struct record_saving_stat total_stat = {};
    for (unsigned int i = 0; i < monit->nr_stat; i++)
    {
        total_stat.recv_success += monit->savint_stats[i].recv_success;
        total_stat.save_failed_at_job_deleted += monit->savint_stats[i].save_failed_at_job_deleted;
        total_stat.save_failed_at_mutex_lock += monit->savint_stats[i].save_failed_at_mutex_lock;

        total_stat.save_to_kafka_failed_at_decode_messagepack +=
            monit->savint_stats[i].save_to_kafka_failed_at_decode_messagepack;
        total_stat.save_to_kafka_failed_at_send += monit->savint_stats[i].save_to_kafka_failed_at_send;
        total_stat.save_to_kafka_success += monit->savint_stats[i].save_to_kafka_success;

        total_stat.save_to_file_failed_at_decode_to_str += monit->savint_stats[i].save_to_file_failed_at_decode_to_str;
        total_stat.save_to_file_failed_at_pcapng_open += monit->savint_stats[i].save_to_file_failed_at_pcapng_open;
        total_stat.save_to_file_failed_at_pcapng_format += monit->savint_stats[i].save_to_file_failed_at_pcapng_format;
        total_stat.save_to_file_failed_at_write_to_disk += monit->savint_stats[i].save_to_file_failed_at_write_to_disk;
        total_stat.save_to_file_success += monit->savint_stats[i].save_to_file_success;
    }

    struct cJSON * json_root = cJSON_CreateObject();
    cJSON_AddNumberToObject(json_root, "recv_success", total_stat.recv_success);
    cJSON_AddNumberToObject(json_root, "save_failed_at_job_deleted", total_stat.save_failed_at_job_deleted);
    cJSON_AddNumberToObject(json_root, "save_failed_at_mutex_lock", total_stat.save_failed_at_mutex_lock);

    cJSON_AddNumberToObject(json_root, "save_to_kafka_failed_at_decode_messagepack",
                            total_stat.save_to_kafka_failed_at_decode_messagepack);
    cJSON_AddNumberToObject(json_root, "save_to_kafka_failed_at_send", total_stat.save_to_kafka_failed_at_send);
    cJSON_AddNumberToObject(json_root, "save_to_kafka_success", total_stat.save_to_kafka_success);

    cJSON_AddNumberToObject(json_root, "save_to_file_failed_at_decode_to_str",
                            total_stat.save_to_file_failed_at_decode_to_str);
    cJSON_AddNumberToObject(json_root, "save_to_file_failed_at_pcapng_open",
                            total_stat.save_to_file_failed_at_pcapng_open);
    cJSON_AddNumberToObject(json_root, "save_to_file_failed_at_pcapng_format",
                            total_stat.save_to_file_failed_at_pcapng_format);
    cJSON_AddNumberToObject(json_root, "save_to_file_failed_at_write_to_disk",
                            total_stat.save_to_file_failed_at_write_to_disk);
    cJSON_AddNumberToObject(json_root, "save_to_file_success", total_stat.save_to_file_success);

    char * str_json_print = cJSON_Print(json_root);
    FILE * fp_monit = fopen(conf->monit_file_path, "w");
    if (fp_monit == NULL)
    {
        dzlog_warn("monit file %s open failed, cannot dump program stat info : %s", conf->monit_file_path,
                   strerror(errno));
        return;
    }

    fprintf(fp_monit, "%s", str_json_print);
    cJSON_Delete(json_root);
    free(str_json_print);
    fclose(fp_monit);
    return;
}

static void * monit_loop(void * args)
{
    pthread_detach(pthread_self());
    while (1)
    {
        monit_dump();
        sleep(1);
    }
}