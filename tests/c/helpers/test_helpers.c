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
#include <inttypes.h>
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
 *  A version has to compare as a version, and the arithmetic has to fit.
 *
 *  This was an `int` accumulating up to 1000^4, so a four-segment release
 *  with its revision -- "1.9.0.0-2" -- overflowed and came out NEGATIVE. The
 *  agent compares releases with this to decide which binary is the newest,
 *  and on a client node it read 1.9.0.0-2 as SMALLER than 1.7.1.0-2 and
 *  promoted the OLD one, re-appending it on every restart for eleven days.
 *  The store showed it plainly: 1.9.0.0 written at 13:46:25, 1.7.1.0 written
 *  back two seconds later.
 *
 *  The cases below are that node's real version chain.
 ***************************************************************************/
PRIVATE void test_version_cmp(void)
{
    /*
     *  Ordered pairs: the right one must compare NEWER than the left one.
     */
    struct { const char *older; const char *newer; const char *why; } pairs[] = {
        /*  The pair that cost eleven days on a client node  */
        {"1.7.1.0-2",   "1.9.0.0-2",    "four segments plus a revision"},
        /*  The rest of that same chain, in order  */
        {"1.5.3.0-2",   "1.5.4.0-2",    "chain"},
        {"1.5.4.0-2",   "1.6.5.0-2",    "chain"},
        {"1.6.8.0-2",   "1.7.0.0-2",    "chain"},
        {"1.7.0.0-2",   "1.7.1.0-2",    "chain"},
        {"1.8.0.0-2",   "1.8.1.0-2",    "chain"},
        {"1.8.1.0-2",   "1.9.0.0-2",    "chain"},
        /*  The revision orders within one release  */
        {"1.9.0.0-1",   "1.9.0.0-2",    "revision"},
        /*  Numbers, not strings: 10 comes after 9  */
        {"7.9.11",      "7.10.0",       "numeric, not lexicographic"},
        {"7.10.0-1",    "7.11.0-1",     "SDK chain"},
        /*  A segment over 999 -- ANY packing that weighs segments by 1000
            gets this backwards, whatever the width of the accumulator  */
        {"2.0.0",       "2.1000.0",     "segment above the packing base"},
        {"1.2000.0",    "2.0.0",        "a big minor is still a minor"},
        /*  Segments far past what an int64 packing could hold  */
        {"1.2.3.4.5.6.7.8",  "1.2.3.4.5.6.7.9",  "eight segments"},
        {0, 0, 0}
    };

    int failed = 0;
    for(int i = 0; pairs[i].older; i++) {
        if(!(version_cmp(pairs[i].newer, pairs[i].older) > 0)) {
            printf("FAIL %-40s %s should be NEWER than %s (%s)\n",
                "version_cmp ordering",
                pairs[i].newer, pairs[i].older, pairs[i].why
            );
            failed++;
        }
        /*  and the other way round, so it is a comparison and not a coin  */
        if(!(version_cmp(pairs[i].older, pairs[i].newer) < 0)) {
            printf("FAIL %-40s %s should be OLDER than %s (%s)\n",
                "version_cmp is symmetric",
                pairs[i].older, pairs[i].newer, pairs[i].why
            );
            failed++;
        }
    }

    /*
     *  Equal is equal, and a missing segment is a zero.
     */
    struct { const char *a; const char *b; const char *why; } equals[] = {
        {"1.9.0.0-2",   "1.9.0.0-2",    "identical"},
        {"7.11",        "7.11.0",       "missing segment counts as zero"},
        {"7.11.0",      "7.11.0.0.0",   "trailing zeros do not matter"},
        {"",            "",             "two empty versions"},
        {0, 0, 0}
    };
    for(int i = 0; equals[i].a; i++) {
        if(version_cmp(equals[i].a, equals[i].b) != 0) {
            printf("FAIL %-40s '%s' vs '%s' (%s)\n",
                "version_cmp equality", equals[i].a, equals[i].b, equals[i].why
            );
            failed++;
        }
    }

    /*
     *  An absent version is the oldest thing there is, and NULL must not
     *  crash: the agent reads these straight out of a treedb record.
     */
    if(!(version_cmp("1.0.0", "") > 0) || !(version_cmp("", "1.0.0") < 0)) {
        printf("FAIL %-40s\n", "version_cmp against an empty version");
        failed++;
    }
    if(!(version_cmp("1.0.0", 0) > 0) || !(version_cmp(0, "1.0.0") < 0) ||
            version_cmp(0, 0) != 0) {
        printf("FAIL %-40s\n", "version_cmp against NULL");
        failed++;
    }

    if(!failed) {
        printf("ok   %-40s\n", "version_cmp orders releases");
    } else {
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
    test_version_cmp();

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
