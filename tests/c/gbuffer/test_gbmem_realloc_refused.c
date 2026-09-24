/****************************************************************************
 *          test_gbmem_realloc_refused.c
 *
 *          A gbmem_realloc() that is refused (the new size is larger than
 *          the largest block) answers NULL and leaves the old block as it
 *          was: valid, with its content, and TRACKED.
 *
 *          With CONFIG_DEBUG_TRACK_MEMORY, 7.25.4 took the block out of the
 *          tracking list and took its size off the memory counter BEFORE
 *          the size check. The caller keeps the old block, as it must, and
 *          its free then logged "Wrong dl_item_t, WITHOUT links" and took
 *          the size off a second time: the counter wrapped.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>

#define APP "test_gbmem_realloc_refused"

#define MAX_BLOCK   4096

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
 *  A refused realloc keeps the old block, and the tracking with it
 ***************************************************************************/
PRIVATE void test_refused_realloc(void)
{
    size_t before = get_cur_system_memory();
    char *p = gbmem_malloc(100);
    memcpy(p, "still here", sizeof("still here"));
    size_t with_block = get_cur_system_memory();

    char *q = gbmem_realloc(p, MAX_BLOCK * 2);

    ok_or_fail(q == NULL, "a realloc larger than the largest block answers NULL");
    ok_or_fail(strstr(gobj_log_last_message(), "SIZE GREATER THAN MAX_BLOCK") != NULL,
        "the refusal is logged");
    ok_or_fail(strcmp(p, "still here") == 0, "the old block keeps its content");
    ok_or_fail(get_cur_system_memory() == with_block,
        "the memory counter still has the old block");

    /*
     *  The old block still grows within the limit
     */
    q = gbmem_realloc(p, 200);
    ok_or_fail(q != NULL && strcmp(q, "still here") == 0,
        "the old block can still be reallocated");
    if(q) {
        p = q;
    }

    gbmem_free(p);
    ok_or_fail(get_cur_system_memory() == before,
        "the free takes off what the block had, once");
}

/***************************************************************************
 *
 ***************************************************************************/
int main(int argc, char *argv[])
{
    gbmem_setup(
        MAX_BLOCK,  // mem_max_block
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

    /*
     *  No capture handler: it keeps the logs in memory, and the test
     *  counts the memory
     */
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    test_refused_realloc();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}
