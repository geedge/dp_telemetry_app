#pragma once
#include "marsio.h"

#include <zlog.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef MR_SYMBOL_MAX
#define MR_SYMBOL_MAX 128
#endif

#define TELEMETRY_INDEX_START 8
#define TELEMETRY_INDEX_END DP_TRACE_JOB_NUM_MAX

#define TRACE_INDEX_START 0
#define TRACE_INDEX_END TELEMETRY_INDEX_START

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif /* likely */

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif /* unlikely */

#define TELEMETRY_DIM(a) (sizeof(a) / sizeof((a)[0]))

extern unsigned int zlog_env_is_init;
#define DP_TRACE_VERIFY(condition, fmt, ...)                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            if (zlog_env_is_init == 1)                                                                                 \
            {                                                                                                          \
                dzlog_error(fmt, ##__VA_ARGS__);                                                                       \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                printf(fmt, ##__VA_ARGS__);                                                                            \
            }                                                                                                          \
            exit(EXIT_FAILURE);                                                                                        \
        }                                                                                                              \
    } while (0)

#define DP_TRACE_NO_ROLE 0

extern struct mr_instance * mr_instance;

static inline bool is_directory_exists(const char * path)
{
    struct stat info;
    if (stat(path, &info) != 0)
        return false;
    return S_ISDIR(info.st_mode) == 1;
}

static inline bool is_file_exists(const char * path)
{
    return access(path, F_OK) == 0;
}

// Combine relative paths and absolute paths into a new absolute path
static inline char * paths_combine(const char * restrict prog_absolute_path, const char * restrict file_relative_path,
                                   char * restrict resolve_path, size_t resolve_path_size)
{
    int prog_len = strlen(prog_absolute_path);
    int file_len = strlen(file_relative_path);
    if (prog_len + file_len + 1 > resolve_path_size)
    {
        return NULL;
    }

    if (file_relative_path[0] == '/')
    {
        strcpy(resolve_path, file_relative_path);
        return resolve_path;
    }

    char combine_path[PATH_MAX];
    strcpy(combine_path, prog_absolute_path);

    char * last_slash = strrchr(combine_path, '/');
    if (last_slash == NULL)
    {
        return NULL;
    }
    *last_slash = '\0';

    strcat(combine_path, "/");
    strcat(combine_path, file_relative_path);

    realpath(combine_path, resolve_path);

    return resolve_path;
}

// Special character conversion adapted to geedge CM.
// unescape only for bpf.
// Full string escaping is confusing and difficult to read.
// pcap-filter support backslash (\). but no '\b' in bpf.
static inline void bpf_str_unescape_for_cm(const char * input, char * output)
{
    int i, j;

    for (i = 0, j = 0; i < strlen(input) - 1; i++)
    {
        if (input[i] == '\\' && input[i + 1] == '\\')
        {
            i += 1;
            output[j++] = '\\';
        }
        else if (input[i] == '\\' && input[i + 1] == 'b')
        {
            i += 1;
            output[j++] = ' ';
        }
        else if (input[i] == '\\' && input[i + 1] == '&')
        {
            i += 1;
            output[j++] = '&';
        }
        else if (input[i] == '\\' && input[i + 1] == '^')
        {
            i += 1;
            output[j++] = '^';
        }
        else if (input[i] == '\\' && input[i + 1] == '$')
        {
            i += 1;
            output[j++] = '$';
        }
        else if (input[i] == '\\' && input[i + 1] == '|')
        {
            i += 1;
            output[j++] = '|';
        }
        else if (input[i] == '\\' && input[i + 1] == '(')
        {
            i += 1;
            output[j++] = '(';
        }
        else if (input[i] == '\\' && input[i + 1] == ')')
        {
            i += 1;
            output[j++] = ')';
        }
        else
        {
            output[j++] = input[i];
        }
    }
    output[j++] = input[i];

    output[j] = '\0';
}