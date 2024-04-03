#include "common.h"
#include "config.h"
#include "job_ctx.h"
#include "trace_output.h"

#include <getopt.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>

static char appsym[64] = "dp_trace_telemetry";

struct mr_instance * mr_instance = NULL;

int usage(const char * command)
{
    fprintf(stdout,
            "Usage:\n"
            "\n"
            "   %s [option]\n"
            "\n"
            "Options;\n"
            "\n"
            "   -c config file path\n"
            "   -d dynamic config file path\n"
            "   -h\n"
            "\n",
            command);
    return 0;
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
            usage("dp_telemetry");
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
            usage("dp_telemetry");
            break;
        }
    }

    // Load configuration file
    config_load();

    setlogmask(LOG_UPTO(conf->log_level));

    mr_instance = marsio_create();

    marsio_option_set(mr_instance, MARSIO_OPT_THREAD_MASK_IN_CPUSET, &conf->cpu_set_io, sizeof(conf->cpu_set_io));

    marsio_init(mr_instance, appsym);

    dynamic_config_load();
    dp_trace_output_init();
    dynamic_config_apply();
    dynamic_config_reload_thread_create();

    unsigned int nr_thread = CPU_COUNT(&conf->cpu_set_io);

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