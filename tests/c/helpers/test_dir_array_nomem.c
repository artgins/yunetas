/****************************************************************************
 *          test_dir_array_nomem.c
 *
 *          A directory listing that cannot keep an entry (no memory) FAILS:
 *          find_files_with_suffix_array() and walk_dir_array() answer -1,
 *          with the listing empty, and the failure is logged.
 *
 *          Up to 7.25.4 the entry was dropped and the listing answered 0:
 *          timeranger2 read a key without the md2 file the listing lost,
 *          a shorter key that nothing flagged.
 *
 *          The memory is refused with a small largest block (the array of
 *          entries starts at 1024 pointers, 8 KB, over the 4 KB limit).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <yunetas.h>

#define APP "test_dir_array_nomem"

#define MAX_BLOCK   4096
#define BASE        "/tmp/test_dir_array_nomem"

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
 *  A listing that cannot keep an entry fails, and says so
 ***************************************************************************/
PRIVATE void test_listing_refused(void)
{
    rmrdir(BASE);
    mkrdir(BASE, 02770);
    int fd = newfile(BASE "/a.md2", 0660, FALSE);
    if(fd >= 0) {
        close(fd);
    }

    dir_array_t da;
    gobj_log_set_last_message("%s", "");
    int ret = find_files_with_suffix_array(0, BASE, ".md2", &da);
    ok_or_fail(ret == -1, "find_files_with_suffix_array() answers -1 when it cannot keep an entry");
    ok_or_fail(da.count == 0 && da.items == NULL, "and the listing is empty, not short");
    ok_or_fail(strstr(gobj_log_last_message(), "Cannot list directory") != NULL,
        "the failed listing is logged");
    dir_array_free(&da);

    gobj_log_set_last_message("%s", "");
    ret = walk_dir_array(0, BASE, ".*", WD_MATCH_REGULAR_FILE, &da);
    ok_or_fail(ret == -1, "walk_dir_array() answers -1 when it cannot keep an entry");
    ok_or_fail(da.count == 0 && da.items == NULL, "and the listing is empty, not short");
    ok_or_fail(strstr(gobj_log_last_message(), "Cannot list directory tree") != NULL,
        "the failed walk is logged");
    dir_array_free(&da);

    ret = get_ordered_filename_array(0, BASE, ".*", WD_MATCH_REGULAR_FILE, &da);
    ok_or_fail(ret == -1, "get_ordered_filename_array() answers -1 too");
    dir_array_free(&da);

    /*
     *  An empty directory needs no memory: it lists, 0 entries
     */
    unlink(BASE "/a.md2");
    ret = find_files_with_suffix_array(0, BASE, ".md2", &da);
    ok_or_fail(ret == 0 && da.count == 0, "an empty directory still lists");
    dir_array_free(&da);

    rmrdir(BASE);
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

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    test_listing_refused();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}
