#include "common.h"
#include "config.h"
#include "http_serv.h"
#include "logger.h"
#include "maat.h"
#include "monit.h"
#include "trace_output.h"

#include <getopt.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/signalfd.h>

static const char appsym[64] = "dp_trace_telemetry";

struct mr_instance * mr_instance = NULL;

static void signal_handler(evutil_socket_t fd, short what, void * arg)
{
    switch (fd)
    {
    case SIGHUP:
        dynamic_config_load_and_apply();
        break;
    default:
        dzlog_warn("Received unexpected signal: %d", fd);
    }
}

static void * signal_event_thread_dispatch(void * arg)
{
    struct event_base * base = (struct event_base *)arg;
    event_base_dispatch(base);
    return NULL;
}

void signal_event_init()
{
    static int signals[] = {SIGHUP};
    static struct event_base * evbase;
    static struct event * evsignal[8];

    evbase = event_base_new();

    DP_TRACE_VERIFY(evbase != NULL, "event_base_new failed");

    for (unsigned int i = 0; i < TELEMETRY_DIM(signals); i++)
    {
        evsignal[i] = evsignal_new(evbase, signals[i], signal_handler, NULL);
        DP_TRACE_VERIFY(evsignal[i], "create signal event failed. signal is:%d", signals[i]);
        evsignal_add(evsignal[i], NULL);
    }

    pthread_t tid;
    int ret = pthread_create(&tid, NULL, signal_event_thread_dispatch, evbase);
    DP_TRACE_VERIFY(ret == 0, "failed to create thread for event_base dispatch.return value:%d", ret);
}

void usage(const char * program_name)
{
    fprintf(stdout,
            "Usage:\n"
            "\n"
            "   %s [option]\n"
            "\n"
            "Options:\n"
            "\n"
            "   -c config file path\n"
            "   -d dynamic config file path\n"
            "   -v show current version\n"
            "   -h \n"
            "\n",
            program_name);
    exit(0);
}

int main(int argc, char * argv[])
{
    int ret = 0;
    char * arg0_dup = strdup(argv[0]);
    const char * program_name = basename(arg0_dup);

    char config_path[PATH_MAX];
    char dy_config_path[PATH_MAX];
    int opt = 0;
    while ((opt = getopt(argc, argv, "c:d:hv")) != -1)
    {
        char * endptr = NULL;
        switch (opt)
        {
        case 'h': {
            usage(program_name);
            break;
        }
        case 'c': {
            snprintf(config_path, sizeof(config_path), "%s", optarg);
            break;
        }
        case 'd': {
            snprintf(dy_config_path, sizeof(dy_config_path), "%s", optarg);
            break;
        }
        case 'v': {
            printf("dp_trace_telemetry current version: %s\n", DP_TRACE_VERSION);
            exit(0);
        }
        default:
            usage(program_name);
            break;
        }
    }

    config_create(config_path, dy_config_path);

    // Load configuration file
    config_load();

    const struct config * conf = global_config_get();

    logger_init();

    http_serv_init();

    dp_trace_output_init();

    mr_instance = marsio_create();

    cpu_set_t cpu_set_io = conf->cpu_set_io;
    ret = marsio_option_set(mr_instance, MARSIO_OPT_THREAD_MASK_IN_CPUSET, &cpu_set_io, sizeof(cpu_set_io));
    DP_TRACE_VERIFY(ret >= 0, "marsio_option_set failed.");

    ret = marsio_init(mr_instance, appsym);
    DP_TRACE_VERIFY(ret >= 0, "marsio init failed.");

    dynamic_config_load_and_apply();

    dp_trace_maat_init();

    signal_event_init();

    unsigned int nr_thread = CPU_COUNT(&conf->cpu_set_io);
    unsigned int ring_num = DP_TRACE_RING_NUM;
    dzlog_info("thread count = %u", nr_thread);
    dzlog_info("ring num = %u", ring_num);

    monit_init(nr_thread);

    pthread_t tmp_pid[nr_thread];

    for (int i = 0; i < nr_thread; i++)
    {
        pthread_create(&tmp_pid[i], NULL, dp_trace_process_thread, (void *)(uintptr_t)i);
    }

    for (int i = 0; i < nr_thread; i++)
    {
        pthread_join(tmp_pid[i], NULL);
    }

    marsio_destory(mr_instance);
    free(arg0_dup);
    dzlog_info("%s is terminated.", program_name);
}