/****************************************************************************
 *          test_dir_read_error.c
 *
 *          A directory that opens and then cannot be READ (readdir() fails
 *          with EIO, as on a disk error or a stale NFS handle) is a listing
 *          that fails, never a short one:
 *
 *              1. find_files_with_suffix_array() answers -1, empty, logged
 *              2. walk_dir_array() answers -1 when its root cannot be read
 *              3. and when a SUBdirectory cannot be read
 *              4. walk_dir_tree() answers -1
 *
 *          Up to 7.25.4 the failure was taken as the end of the directory:
 *          the listing answered 0 without the entries not read yet, and
 *          timeranger2 read a key without one of its md2 files, with no
 *          flag set.
 *
 *          And two more of the same functions:
 *
 *              5. walk_dir_array() and get_ordered_filename_array() with
 *                 `re` NULL list every entry, as documented. Up to 7.25.4
 *                 they crashed in regcomp().
 *              6. walk_dir_tree() of a root that exists and cannot be
 *                 opened (mode 0) answers -1 AND logs it: its callers say
 *                 "Error already logged". Up to 7.25.4 nothing was logged.
 *
 *          The failure of readdir() is made by __wrap_readdir() below, for
 *          the directory named by `failing_dir` (seen at its opendir()).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <yunetas.h>

#define APP "test_dir_read_error"

#define BASE    "/tmp/test_dir_read_error"
#define SUB     BASE "/sub"

/***************************************************************
 *              The readdir() of a failing disk
 ***************************************************************/
DIR *__real_opendir(const char *name);
DIR *__wrap_opendir(const char *name);
struct dirent *__real_readdir(DIR *dirp);
struct dirent *__wrap_readdir(DIR *dirp);

PRIVATE const char *failing_dir = NULL;    // the directory whose readdir() fails
PRIVATE int entries_before_failure = 0;    // entries it gives before failing
PRIVATE DIR *failing_dirp = NULL;
PRIVATE int entries_given = 0;
PRIVATE int failures = 0;

DIR *__wrap_opendir(const char *name)
{
    DIR *dirp = __real_opendir(name);
    if(dirp && failing_dir && strcmp(name, failing_dir) == 0) {
        failing_dirp = dirp;
        entries_given = 0;
    }
    return dirp;
}

struct dirent *__wrap_readdir(DIR *dirp)
{
    if(dirp && dirp == failing_dirp) {
        if(entries_given >= entries_before_failure) {
            failing_dirp = NULL;
            failures++;
            errno = EIO;
            return NULL;
        }
        entries_given++;
    }
    return __real_readdir(dirp);
}

PRIVATE void fail_readdir_of(const char *directory, int entries)
{
    failing_dir = directory;
    entries_before_failure = entries;
    failing_dirp = NULL;
    failures = 0;
}

/***************************************************************
 *              Checks
 ***************************************************************/
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

PRIVATE int entries = 0;
PRIVATE BOOL count_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,
    char *fullpath,
    const char *directory,
    char *name,
    int level,
    wd_option opt
)
{
    entries++;
    return TRUE;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void make_tree(void)
{
    rmrdir(BASE);
    mkrdir(SUB, 02770);
    const char *files[] = {BASE "/a.md2", BASE "/b.md2", BASE "/c.md2", SUB "/d.log"};
    for(size_t i = 0; i < ARRAY_SIZE(files); i++) {
        int fd = newfile(files[i], 0660, FALSE);
        if(fd >= 0) {
            close(fd);
        }
    }
}

/***************************************************************************
 *  1-4. A directory that cannot be read
 ***************************************************************************/
PRIVATE void test_read_error(void)
{
    dir_array_t da;
    int ret;

    /*
     *  1.
     */
    fail_readdir_of(BASE, 4);   // at least one .md2 is listed before: the listing would be short
    gobj_log_set_last_message("%s", "");
    ret = find_files_with_suffix_array(0, BASE, ".md2", &da);
    ok_or_fail(failures == 1, "1. the readdir() of the directory failed");
    ok_or_fail(ret == -1, "1. find_files_with_suffix_array() answers -1 when readdir() fails");
    ok_or_fail(da.count == 0 && da.items == NULL, "1. and the listing is empty, not short");
    ok_or_fail(strstr(gobj_log_last_message(), "readdir() FAILED") != NULL, "1. and it is logged");
    dir_array_free(&da);

    /*
     *  2.
     */
    fail_readdir_of(BASE, 4);
    gobj_log_set_last_message("%s", "");
    ret = walk_dir_array(0, BASE, ".*", WD_RECURSIVE|WD_MATCH_REGULAR_FILE, &da);
    ok_or_fail(failures == 1, "2. the readdir() of the root failed");
    ok_or_fail(ret == -1, "2. walk_dir_array() answers -1 when the root cannot be read");
    ok_or_fail(da.count == 0 && da.items == NULL, "2. and the listing is empty, not short");
    ok_or_fail(strstr(gobj_log_last_message(), "Cannot list directory tree") != NULL,
        "2. and it is logged");
    dir_array_free(&da);

    /*
     *  3.
     */
    fail_readdir_of(SUB, 0);
    ret = walk_dir_array(0, BASE, ".*", WD_RECURSIVE|WD_MATCH_REGULAR_FILE, &da);
    ok_or_fail(failures == 1, "3. the readdir() of the subdirectory failed");
    ok_or_fail(ret == -1, "3. walk_dir_array() answers -1 when a subdirectory cannot be read");
    ok_or_fail(da.count == 0 && da.items == NULL, "3. and the listing is empty, not short");
    dir_array_free(&da);

    /*
     *  4.
     */
    fail_readdir_of(BASE, 1);
    entries = 0;
    gobj_log_set_last_message("%s", "");
    ret = walk_dir_tree(0, BASE, ".*", WD_RECURSIVE|WD_MATCH_REGULAR_FILE, count_cb, NULL);
    ok_or_fail(failures == 1, "4. the readdir() of the root failed");
    ok_or_fail(ret == -1, "4. walk_dir_tree() answers -1 when the root cannot be read");
    ok_or_fail(strstr(gobj_log_last_message(), "readdir() FAILED") != NULL, "4. and it is logged");

    fail_readdir_of(NULL, 0);

    /*
     *  Without failures, the same calls list the whole tree
     */
    ret = find_files_with_suffix_array(0, BASE, ".md2", &da);
    ok_or_fail(ret == 0 && da.count == 3, "a directory that can be read lists all its entries");
    dir_array_free(&da);
    ret = walk_dir_array(0, BASE, ".*", WD_RECURSIVE|WD_MATCH_REGULAR_FILE, &da);
    ok_or_fail(ret == 0 && da.count == 4, "a tree that can be read lists all its entries");
    dir_array_free(&da);
}

/***************************************************************************
 *  5. `re` NULL is every entry
 ***************************************************************************/
PRIVATE void test_re_null(void)
{
    dir_array_t da;
    int ret = walk_dir_array(0, BASE, NULL, WD_RECURSIVE|WD_MATCH_REGULAR_FILE, &da);
    ok_or_fail(ret == 0 && da.count == 4, "5. walk_dir_array() with re NULL lists every entry");
    dir_array_free(&da);

    ret = get_ordered_filename_array(0, BASE, NULL, WD_MATCH_REGULAR_FILE, &da);
    ok_or_fail(ret == 0 && da.count == 3, "5. get_ordered_filename_array() with re NULL too");
    dir_array_free(&da);
}

/***************************************************************************
 *  6. A root that cannot be opened is logged. Skipped as root, which
 *  opens it.
 ***************************************************************************/
PRIVATE void test_root_cannot_be_opened(void)
{
    if(geteuid() == 0) {
        printf("skip 6. as root every directory opens\n");
        return;
    }
    chmod(BASE, 0);
    gobj_log_set_last_message("%s", "");
    int ret = walk_dir_tree(0, BASE, ".*", WD_MATCH_REGULAR_FILE, count_cb, NULL);
    chmod(BASE, 02770);
    ok_or_fail(ret == -1, "6. walk_dir_tree() of a root that cannot be opened answers -1");
    ok_or_fail(strstr(gobj_log_last_message(), "Cannot open directory") != NULL,
        "6. and it is logged, as its callers say");
}

/***************************************************************************
 *
 ***************************************************************************/
int main(int argc, char *argv[])
{
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

    make_tree();
    test_read_error();
    test_root_cannot_be_opened();
    test_re_null();     // LAST: it crashed up to 7.25.4
    rmrdir(BASE);

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}
