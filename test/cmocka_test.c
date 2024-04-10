#include "common.h"
#include "config.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <cmocka.h>

struct mr_instance * mr_instance = NULL;

int setup(void ** state)
{
    mr_instance = marsio_create();
    return 0;
}

int teardown(void ** state)
{
    if (g_conf != NULL)
    {
        free(g_conf);
        g_conf == NULL;
        printf("free g_conf\n");
    }
    return 0;
}

void config_create_test(void ** state)
{
    config_create();
    assert_non_null(g_conf);
}

int main(int argc, char * argv[])
{
    const struct CMUnitTest config_tests[] = {
        cmocka_unit_test_setup_teardown(config_create_test, setup, teardown),
    };
    cmocka_run_group_tests(config_tests, NULL, NULL);
    return 0;
}