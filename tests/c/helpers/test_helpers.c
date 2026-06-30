/****************************************************************************
 *          test_helpers.c
 *
 *          Unit tests for split2() / split_free2() (string split helper).
 *          Includes a reentrancy regression: split2() must NOT clobber a
 *          caller's in-progress strtok() parse (the strtok -> strtok_r fix).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <yunetas.h>

#define APP "test_helpers"

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE int global_result = 0;

/***************************************************************************
 *  Compare a split2() result against a NULL-terminated expected array,
 *  also checking the reported size. Frees the list with split_free2().
 ***************************************************************************/
PRIVATE void check_split(
    const char *str,
    const char *delim,
    const char **expected,   // NULL-terminated
    const char *name
)
{
    int expected_size = 0;
    while(expected[expected_size]) {
        expected_size++;
    }

    int list_size = 0;
    const char **list = split2(str, delim, &list_size);

    int ok = 1;
    if(list_size != expected_size) {
        ok = 0;
    } else {
        for(int i=0; i<expected_size; i++) {
            if(strcmp(list[i], expected[i]) != 0) {
                ok = 0;
                break;
            }
        }
    }

    if(ok) {
        printf("ok   %-40s size=%d\n", name, list_size);
    } else {
        printf("FAIL %-40s got size=%d\n", name, list_size);
        global_result += -1;
    }

    split_free2(list);
}

/***************************************************************************
 *  Basic splitting
 ***************************************************************************/
PRIVATE void test_split_basic(void)
{
    check_split("a,b,c", ",", (const char*[]){"a","b","c",NULL}, "basic comma split");
    check_split("abc", ",", (const char*[]){"abc",NULL}, "single token, no delim");
    check_split("a:b;c", ":;", (const char*[]){"a","b","c",NULL}, "multi-char delim set");
}

/***************************************************************************
 *  Empty strings are dropped (documented HACK in split2)
 ***************************************************************************/
PRIVATE void test_split_empties_excluded(void)
{
    check_split("a,,b", ",", (const char*[]){"a","b",NULL}, "drop empty between delims");
    check_split(",a,b,", ",", (const char*[]){"a","b",NULL}, "drop leading/trailing empty");
    check_split("", ",", (const char*[]){NULL}, "empty input -> empty list");
    check_split(",,,", ",", (const char*[]){NULL}, "all-delims -> empty list");
}

/***************************************************************************
 *  plist_size NULL must not crash
 ***************************************************************************/
PRIVATE void test_split_null_size_arg(void)
{
    const char **list = split2("a,b", ",", NULL);
    if(list && list[0] && strcmp(list[0],"a")==0 && list[1] && strcmp(list[1],"b")==0) {
        printf("ok   %-40s\n", "NULL list_size arg");
    } else {
        printf("FAIL %-40s\n", "NULL list_size arg");
        global_result += -1;
    }
    split_free2(list);
}

/***************************************************************************
 *  Reentrancy regression (the strtok -> strtok_r fix):
 *  a caller that is mid-strtok() must keep its parse intact across a
 *  split2() call. The OLD split2() used the global-state strtok()
 *  internally, so calling it between two strtok(NULL,...) steps clobbered
 *  the caller's parse pointer (left dangling into split2's freed scratch
 *  buffer). With strtok_r inside split2(), the caller's strtok() is untouched.
 *
 *  NOTE: this test uses the global strtok() ON PURPOSE to play the role of
 *  such a caller; it is not production code.
 ***************************************************************************/
PRIVATE void test_split_reentrancy(void)
{
    char outer[] = "x1,x2,x3";

    char *t1 = strtok(outer, ",");          // caller starts a GLOBAL strtok parse
    int n = 0;
    const char **inner = split2("a;b;c", ";", &n);  // ... and calls split2 mid-parse
    split_free2(inner);
    char *t2 = strtok(NULL, ",");           // must still yield "x2"
    char *t3 = strtok(NULL, ",");           // must still yield "x3"

    if(t1 && strcmp(t1,"x1")==0 &&
       t2 && strcmp(t2,"x2")==0 &&
       t3 && strcmp(t3,"x3")==0) {
        printf("ok   %-40s\n", "outer strtok survives split2");
    } else {
        printf("FAIL %-40s t1=%s t2=%s t3=%s\n", "outer strtok survives split2",
            t1?t1:"(null)", t2?t2:"(null)", t3?t3:"(null)");
        global_result += -1;
    }
}

/***************************************************************************
 *              Test
 *  HACK: return -1 to fail, 0 to ok
 ***************************************************************************/
PRIVATE int do_test(void)
{
    test_split_basic();
    test_split_empties_excluded();
    test_split_null_size_arg();
    test_split_reentrancy();

    return global_result;
}

/***************************************************************************
 *                      Main
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

    gbmem_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

    unsigned long memory_check_list[] = {0}; // WARNING: list ended with 0
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

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

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_create(
        0,
        2024,
        10,
        NULL,
        &yev_loop
    );

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    int result = do_test();

    /*--------------------------------*
     *  Stop the event loop
     *--------------------------------*/
    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory()!=0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }
    if(result<0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    } else {
        printf("\n%s: PASS\n", APP);
    }
    return result<0?-1:0;
}
