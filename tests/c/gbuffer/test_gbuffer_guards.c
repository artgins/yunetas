/****************************************************************************
 *          test_gbuffer_guards.c
 *
 *          Regression test for the central NULL guards on the gbuffer
 *          pointer accessors (gbuffer_cur_rd_pointer / _cur_wr_pointer /
 *          _head_pointer). A NULL gbuffer must return NULL, not crash
 *          (hardens the content64/base64 NULL-deref family at the source).
 *          Also asserts the valid path is unchanged.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>

#define APP "test_gbuffer_guards"

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
 *  NULL gbuffer must yield NULL from every pointer accessor (no SIGSEGV)
 ***************************************************************************/
PRIVATE void test_null_guards(void)
{
    ok_or_fail(gbuffer_cur_rd_pointer(NULL) == NULL, "cur_rd_pointer(NULL) == NULL");
    ok_or_fail(gbuffer_cur_wr_pointer(NULL) == NULL, "cur_wr_pointer(NULL) == NULL");
    ok_or_fail(gbuffer_head_pointer(NULL)   == NULL, "head_pointer(NULL) == NULL");
}

/***************************************************************************
 *  The content64/base64-style sink stays graceful: feeding the NULL
 *  result to json_string() yields a NULL json, not a crash.
 ***************************************************************************/
PRIVATE void test_sink_graceful(void)
{
    json_t *jn = json_string(gbuffer_cur_rd_pointer(NULL));
    ok_or_fail(jn == NULL, "json_string(cur_rd_pointer(NULL)) == NULL");
    JSON_DECREF(jn);
}

/***************************************************************************
 *  Wrap guard: a data_size whose +1 sentinel allocation overflows
 *  (SIZE_MAX) must be rejected at creation, not turned into a
 *  non-NULL ~0-byte buffer with data_size==SIZE_MAX. This is the
 *  integer-overflow boundary the __max_block__ guard alone misses
 *  (it catches every merely-large value but not the exact wrap).
 ***************************************************************************/
PRIVATE void test_wrap_guard(void)
{
    gbuffer_t *gbuf = gbuffer_create((size_t)-1, (size_t)-1);
    ok_or_fail(gbuf == NULL, "gbuffer_create(SIZE_MAX,...) == NULL");
    GBUFFER_DECREF(gbuf);
}

/***************************************************************************
 *  Valid gbuffer: accessors still return real, usable data.
 ***************************************************************************/
PRIVATE void test_valid_path(void)
{
    gbuffer_t *gbuf = gbuffer_create(32, 32);
    ok_or_fail(gbuf != NULL, "gbuffer_create");
    if(!gbuf) {
        return;
    }

    gbuffer_append_string(gbuf, "hello");

    char *rd = gbuffer_cur_rd_pointer(gbuf);
    ok_or_fail(rd != NULL && strcmp(rd, "hello") == 0, "cur_rd_pointer returns data");

    char *head = gbuffer_head_pointer(gbuf);
    ok_or_fail(head != NULL && strcmp(head, "hello") == 0, "head_pointer returns data");

    char *wr = gbuffer_cur_wr_pointer(gbuf);
    ok_or_fail(wr != NULL && wr == head + 5, "cur_wr_pointer at tail");

    GBUFFER_DECREF(gbuf);
}

/***************************************************************************
 *      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*----------------------------------*
     *      Startup gobj system
     *----------------------------------*/
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    unsigned long memory_check_list[] = {0}; // WARNING: list ended with 0
    set_memory_check_list(memory_check_list);

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

    /*----------------------------------*
     *      Tests
     *----------------------------------*/
    test_null_guards();
    test_sink_graceful();
    test_wrap_guard();
    test_valid_path();

    gobj_end();

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result;
}
