#pragma once
#include "marsio.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

#ifndef MR_SYMBOL_MAX
#define MR_SYMBOL_MAX 64
#endif

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif /* likely */

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif /* unlikely */

#define TELEMETRY_DIM(a) (sizeof(a) / sizeof((a)[0]))

#define CHECK_OR_EXIT(condition, fmt, ...)                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            syslog(LOG_ERR, fmt, ##__VA_ARGS__);                                                                       \
            exit(EXIT_FAILURE);                                                                                        \
        }                                                                                                              \
    } while (0)

extern struct mr_instance * mr_instance;
extern struct config * g_conf;

static bool is_directory_exists(const char * path)
{
    struct stat info;
    if (stat(path, &info) != 0)
        return false;
    return S_ISDIR(info.st_mode) == 1;
}

static bool is_file_exists(const char * path)
{
    return access(path, F_OK) == 0;
}