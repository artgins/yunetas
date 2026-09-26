/****************************************************************************
 *          test_switchs.c
 *
 *          The string switch of helpers.h (SWITCHS / CASES / ICASES /
 *          CASES_RE / DEFAULTS / SWITCHS_END): what it matches, and that
 *          LEAVING it from inside a case -- a `return` -- costs nothing.
 *
 *          Up to 7.25.6 SWITCHS compiled a regex on entry and only
 *          SWITCHS_END freed it, so every `return` from a case lost glibc's
 *          compiled automaton, ~1 KB. It was allocated by regcomp(3), OUTSIDE
 *          gbmem: the gbmem audit and cur_system_memory never saw it, which
 *          is why this test measures the libc heap (mallinfo2) instead. It
 *          was C_MQIOGATE's send path, one leak per message: a gate_central
 *          grew to 1 GB in 25 minutes in yunovatios' stress test.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <yunetas.h>

#define APP "test_switchs"

#define LOOPS       100000
#define MAX_GROWTH  (256*1024)      // the old macro grew ~100 MB over LOOPS

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
 *  A switch that is left with `return` from its cases, the shape of
 *  C_MQIOGATE's ac_send_message
 ***************************************************************************/
PRIVATE int classify(const char *s)
{
    SWITCHS(s) {
        CASES("foo")
        CASES("bar")
            return 1;

        ICASES("pi")
            return 2;

        CASES_RE("^D.*", 0)
            return 3;

        CASES_RE("^E.*", REG_ICASE)
            return 4;

        CASES("1")
            // fall through to "2", as a C switch does

        CASES("2")
            return 5;

        DEFAULTS
            return 0;
    } SWITCHS_END

    return -1;  // not reached: every case returns
}

/***************************************************************************
 *  The same switch, left with `break`, the way SWITCHS_END is reached
 ***************************************************************************/
PRIVATE int classify_break(const char *s)
{
    int ret = -1;
    SWITCHS(s) {
        CASES("foo")
            ret = 1;
            break;

        CASES_RE("^D.*", 0)
            ret = 3;
            break;

        DEFAULTS
            ret = 0;
            break;
    } SWITCHS_END

    return ret;
}

/***************************************************************************
 *  What the switch matches, unchanged
 ***************************************************************************/
PRIVATE void test_matching(void)
{
    ok_or_fail(classify("foo") == 1, "CASES matches exactly");
    ok_or_fail(classify("bar") == 1, "two CASES stacked share one body");
    ok_or_fail(classify("PI") == 2 && classify("pI") == 2, "ICASES ignores case");
    ok_or_fail(classify("Dog") == 3, "CASES_RE matches a regex");
    ok_or_fail(classify("dog") == 0, "CASES_RE without REG_ICASE is case sensitive");
    ok_or_fail(classify("elephant") == 4, "CASES_RE with REG_ICASE is not");
    ok_or_fail(classify("1") == 5, "a case without return falls through to the next");
    ok_or_fail(classify("zzz") == 0, "DEFAULTS takes what nothing matched");
    ok_or_fail(classify_break("foo") == 1 && classify_break("Dx") == 3 &&
        classify_break("zzz") == 0, "leaving by break reaches SWITCHS_END");
}

/***************************************************************************
 *  Leaving the switch from a case must not grow the heap
 ***************************************************************************/
PRIVATE void test_return_does_not_leak(void)
{
    /*
     *  A warm-up, so that whatever glibc keeps once (regex tables, locale)
     *  is not counted as growth
     */
    for(int i=0; i<100; i++) {
        classify("foo");
        classify("Dog");
    }

    struct mallinfo2 before = mallinfo2();
    size_t sum = 0;
    for(int i=0; i<LOOPS; i++) {
        sum += (size_t)classify("foo");     // return from a plain case
        sum += (size_t)classify("zzz");     // return from DEFAULTS
        sum += (size_t)classify("Dog");     // return from a CASES_RE
    }
    struct mallinfo2 after = mallinfo2();

    long growth = (long)after.uordblks - (long)before.uordblks;
    printf("     libc heap in use: %zu -> %zu bytes (%+ld) over %d x 3 switches\n",
        before.uordblks, after.uordblks, growth, LOOPS);
    ok_or_fail(sum == (size_t)LOOPS * (1 + 0 + 3), "the switches answered");
    ok_or_fail(growth < MAX_GROWTH, "returning from inside a case does not grow the heap");
}

/***************************************************************************
 *  A pattern that does not compile matches nothing, and says so
 ***************************************************************************/
PRIVATE void test_bad_pattern(void)
{
    gobj_log_set_last_message("%s", "");
    BOOL m = str_match_regex("abc", "a(", REG_EXTENDED);
    ok_or_fail(m == FALSE, "a pattern that does not compile does not match");
    ok_or_fail(strstr(gobj_log_last_message(), "regcomp() FAILED") != NULL,
        "and the failure is logged");

    ok_or_fail(str_match_regex("abc", "^a.c$", REG_EXTENDED) == TRUE,
        "str_match_regex() matches");
    ok_or_fail(str_match_regex("abd", "^a.c$", REG_EXTENDED) == FALSE,
        "str_match_regex() does not match what it should not");
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
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
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

    test_matching();
    test_return_does_not_leak();
    test_bad_pattern();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}
