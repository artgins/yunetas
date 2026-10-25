/****************************************************************************
 *          test_replace_string.c
 *
 *          replace_string() (gobj-c), written again after 7.28.4 on gbmem
 *          (it was the str_replace() of Laird Shaw, on libc's malloc/free):
 *          every occurrence replaced left to right without overlaps, the
 *          ends of the string, a longer and a shorter replacement, no
 *          occurrence, and NULL (logged) for a missing or empty argument.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>

#define APP "test_replace_string"

PRIVATE int global_result = 0;

PRIVATE void ok_or_fail(int cond, const char *name)
{
    if(cond) {
        printf("ok   %s\n", name);
    } else {
        printf("FAIL %s\n", name);
        global_result += -1;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void check(const char *str, const char *old, const char *snew, const char *expected)
{
    char name[256];
    snprintf(name, sizeof(name), "replace_string(\"%s\", \"%s\", \"%s\") is \"%s\"",
        str, old, snew, expected
    );
    char *s = replace_string(str, old, snew);
    ok_or_fail(s && strcmp(s, expected) == 0, name);
    if(s) {
        gbmem_free(s);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_replace(void)
{
    check("a-b-c", "-", "+", "a+b+c");
    check("-a-", "-", "", "a");
    check("abc", "x", "yy", "abc");
    check("", "x", "y", "");
    check("$1 and $1", "$1", "value", "value and value");
    check("aaaa", "aa", "b", "bb");
    check("aaa", "aa", "b", "ba");
    check("{{name}}", "{{name}}", "yuneta", "yuneta");
    check("xyz", "xyz", "", "");
    check("ab", "b", "bbb", "abbb");
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_bad_arguments(void)
{
    printf("     (four ERRORs follow, on purpose)\n");
    ok_or_fail(replace_string(NULL, "a", "b") == NULL, "a NULL str answers NULL");
    ok_or_fail(replace_string("abc", NULL, "b") == NULL, "a NULL old answers NULL");
    ok_or_fail(replace_string("abc", "", "b") == NULL, "an empty old answers NULL");
    ok_or_fail(replace_string("abc", "a", NULL) == NULL, "a NULL snew answers NULL");
}

/***************************************************************************
 *
 ***************************************************************************/
int main(int argc, char *argv[])
{
    gbmem_setup(
        0,          // mem_max_block, the default
        0,          // mem_max_system_memory, the default
        FALSE,      // use_own_system_memory
        0,          // mem_min_block
        0           // mem_superblock
    );

    sys_malloc_fn_t malloc_func;
    sys_free_fn_t free_func;

    gbmem_get_json_allocators(&malloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    gobj_start_up(
        argc,
        argv,
        NULL,   // jn_global_settings
        NULL,   // persistent_attrs
        NULL,   // global_command_parser
        NULL,   // global_stats_parser
        NULL,   // global_authz_checker
        NULL    // global_authentication_parser
    );

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    test_replace();
    test_bad_arguments();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}
