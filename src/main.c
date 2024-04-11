#include "common.h"
#include "config.h"
#include "job_ctx.h"
#include "trace_output.h"

#include <getopt.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/signalfd.h>

static char appsym[64] = "dp_trace_telemetry";

struct mr_instance * mr_instance = NULL;

static void signal_handler(evutil_socket_t fd, short what, void * arg)
{

    switch (fd)
    {
    case SIGHUP:
        dynamic_config_load_and_apply();
    default:
        syslog(LOG_WARNING, "Received unexpected signal: %d", fd);
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

    CHECK_OR_EXIT(evbase, "event_base_new failed");

    for (unsigned int i = 0; i < TELEMETRY_DIM(signals); i++)
    {
        evsignal[i] = evsignal_new(evbase, signals[i], signal_handler, NULL);
        CHECK_OR_EXIT(evsignal[i], "create signal event failed. signal is:%d", signals[i]);
        evsignal_add(evsignal[i], NULL);
    }

    pthread_t tid;
    if (pthread_create(&tid, NULL, signal_event_thread_dispatch, evbase) != 0)
    {
        syslog(LOG_ERR, "failed to create thread for event_base dispatch");
        exit(EXIT_FAILURE);
    }
}

void usage()
{
    fprintf(stdout, "Usage:\n"
                    "\n"
                    "   dp_trace_telemetry [option]\n"
                    "\n"
                    "Options:\n"
                    "\n"
                    "   -c config file path\n"
                    "   -d dynamic config file path\n"
                    "   -h\n"
                    "\n");
    exit(0);
}

int main(int argc, char * argv[])
{
    openlog("dp_trace_telemetry", LOG_CONS, 0);
    syslog(LOG_DEBUG, "dp_trace_telemetry start.");

    // Specify configuration file location
    struct config * conf = config_create();
    int opt = 0;
    while ((opt = getopt(argc, argv, "c:d:h")) != -1)
    {
        char * endptr = NULL;
        switch (opt)
        {
        case 'h': {
            usage();
            break;
        }
        case 'c': {
            snprintf(conf->config_path, sizeof(conf->config_path), "%s", optarg);
            break;
        }
        case 'd': {
            snprintf(conf->dy_config_path, sizeof(conf->dy_config_path), "%s", optarg);
            break;
        }
        default:
            usage();
            break;
        }
    }

    // Load configuration file
    config_load();
    dp_trace_output_init();

    setlogmask(LOG_UPTO(conf->log_level));

    mr_instance = marsio_create();

    marsio_option_set(mr_instance, MARSIO_OPT_THREAD_MASK_IN_CPUSET, &conf->cpu_set_io, sizeof(conf->cpu_set_io));

    marsio_init(mr_instance, appsym);

    dynamic_config_load_and_apply();

    signal_event_init();

    unsigned int nr_thread = CPU_COUNT(&conf->cpu_set_io);

    if (nr_thread != 4)
    {
        syslog(LOG_EMERG, "Currently, four threads must be created to read the data. This restriction will be removed "
                          "later");
        return 0;
    }

    syslog(LOG_INFO, "Thread Count = %d\n", nr_thread);
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
    fprintf(stdout, "L2FWD is terminated. ");
}