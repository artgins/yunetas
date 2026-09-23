/****************************************************************************
 *          test_rotatory.c
 *
 *          Retention of a rotatory directory: rotatory_remove_old_files()
 *          removes ONLY the old files of its own mask (and their .OLD),
 *          never the current file, a symbolic link, a directory, or a file
 *          of another name. And the rotation calls the newfile callback,
 *          which is where a user (the agent's audit) applies it.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <yunetas.h>

#define APP "test_rotatory"

#define BASE        "/tmp/test_rotatory_retention"
#define AUDIT_DIR   BASE "/audit"
#define OUTSIDE     BASE "/outside"
#define MASK        "ZZZ-DD_MM_CCYY.log"

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE int global_result = 0;
PRIVATE int s_errors = 0;
PRIVATE int s_newfile_calls = 0;
PRIVATE int s_removed_at_rotation = 0;

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int count_errors(void *h, int priority, const char *bf, size_t len)
{
    s_errors++;
    return 0;
}

PRIVATE void check(BOOL ok, const char *name)
{
    if(ok) {
        printf("ok   %s\n", name);
    } else {
        printf("FAIL %s\n", name);
        global_result += -1;
    }
}

PRIVATE BOOL exists_no_follow(const char *path)
{
    struct stat st;
    return lstat(path, &st) == 0;
}

/***************************************************************************
 *  Set the mtime of the path itself (a link is not followed)
 ***************************************************************************/
PRIVATE void set_age(const char *path, int days_ago)
{
    struct timespec times[2];
    times[0].tv_sec = time(NULL) - (time_t)days_ago*24*60*60;
    times[0].tv_nsec = 0;
    times[1] = times[0];
    if(utimensat(AT_FDCWD, path, times, AT_SYMLINK_NOFOLLOW) != 0) {
        printf("FAIL cannot set the age of %s\n", path);
        global_result += -1;
    }
}

/***************************************************************************
 *  Create a file of `size` bytes and set its mtime `days_ago` days back
 ***************************************************************************/
PRIVATE void make_file(const char *path, size_t size, int days_ago)
{
    FILE *f = fopen(path, "w");
    if(!f) {
        printf("FAIL cannot create %s\n", path);
        global_result += -1;
        return;
    }
    for(size_t i=0; i<size; i++) {
        fputc('x', f);
    }
    fclose(f);
    set_age(path, days_ago);
}

/***************************************************************************
 *  The newfile callback: where the agent applies its retention
 ***************************************************************************/
PRIVATE int newfile_cb(void *user_data, const char *old_filename, const char *new_filename)
{
    hrotatory_h hr = user_data;
    s_newfile_calls++;
    int removed = rotatory_remove_old_files(hr, 7, NULL, NULL);
    if(removed > 0) {
        s_removed_at_rotation += removed;
    }
    return 0;
}

/***************************************************************************
 *  Only the old files of the mask go
 ***************************************************************************/
PRIVATE void test_retention(void)
{
    rmrdir(BASE);
    mkrdir(AUDIT_DIR, 02775);
    mkrdir(OUTSIDE, 02775);

    hrotatory_h hr = rotatory_open(
        AUDIT_DIR "/" MASK,
        0,          // default buffer
        1,          // 1 mega: for the size rotation below
        1,          // min free disk percentage: do not stop on a full /tmp
        02775,
        0660,
        FALSE
    );
    if(!hr) {
        printf("FAIL rotatory_open()\n");
        global_result += -1;
        return;
    }

    const char *current = rotatory_path(hr);
    char current_old[PATH_MAX];
    snprintf(current_old, sizeof(current_old), "%s.OLD", current);

    /*
     *  Candidates (old files of the mask, and their .OLD)
     */
    make_file(AUDIT_DIR "/001-01_01_2026.log", 1000, 10);
    make_file(AUDIT_DIR "/001-01_01_2026.log.OLD", 2000, 10);

    /*
     *  Everything else
     */
    make_file(AUDIT_DIR "/002-02_01_2026.log", 10, 1);          // recent
    make_file(AUDIT_DIR "/notes.txt", 10, 30);                  // another name
    make_file(AUDIT_DIR "/001-01_01_2026.logx", 10, 30);        // another name
    make_file(AUDIT_DIR "/0a1-01_01_2026.log", 10, 30);         // a letter for a digit
    make_file(AUDIT_DIR "/001-01_01_2026.OLD", 10, 30);         // .OLD of no file shape
    make_file(OUTSIDE "/keep.log", 10, 30);
    if(symlink(OUTSIDE "/keep.log", AUDIT_DIR "/003-03_01_2026.log") != 0) {
        printf("FAIL symlink\n");
        global_result += -1;
    }
    set_age(AUDIT_DIR "/003-03_01_2026.log", 30);               // an old LINK with the shape
    mkrdir(AUDIT_DIR "/004-04_01_2026.log", 02775);             // a DIRECTORY with the shape
    set_age(AUDIT_DIR "/004-04_01_2026.log", 30);
    set_age(current, 30);                                       // the current file, old
    make_file(current_old, 10, 30);                             // its .OLD, old

    /*
     *  keep_days 0: nothing
     */
    int errors_before = s_errors;
    json_t *jn_removed = json_array();
    uint64_t removed_bytes = 99;
    int removed = rotatory_remove_old_files(hr, 0, jn_removed, &removed_bytes);
    check(removed == 0 && json_array_size(jn_removed) == 0 && removed_bytes == 0 &&
        exists_no_follow(AUDIT_DIR "/001-01_01_2026.log"),
        "keep_days 0 removes nothing"
    );

    /*
     *  keep_days 7
     */
    removed = rotatory_remove_old_files(hr, 7, jn_removed, &removed_bytes);
    check(removed == 2 && json_array_size(jn_removed) == 2 && removed_bytes == 3000,
        "keep_days 7 removes the two old files of the mask"
    );
    check(!exists_no_follow(AUDIT_DIR "/001-01_01_2026.log") &&
        !exists_no_follow(AUDIT_DIR "/001-01_01_2026.log.OLD"),
        "  the old file and its .OLD are gone"
    );
    check(exists_no_follow(AUDIT_DIR "/002-02_01_2026.log"), "  a recent file stays");
    check(exists_no_follow(AUDIT_DIR "/notes.txt") &&
        exists_no_follow(AUDIT_DIR "/001-01_01_2026.logx") &&
        exists_no_follow(AUDIT_DIR "/0a1-01_01_2026.log") &&
        exists_no_follow(AUDIT_DIR "/001-01_01_2026.OLD"),
        "  files of other names stay"
    );
    check(exists_no_follow(AUDIT_DIR "/003-03_01_2026.log") &&
        exists_no_follow(OUTSIDE "/keep.log"),
        "  an old symbolic link stays, and its target"
    );
    check(is_directory(AUDIT_DIR "/004-04_01_2026.log"), "  a directory with the shape stays");
    check(exists_no_follow(current) && exists_no_follow(current_old),
        "  the current file and its .OLD stay, even old"
    );
    check(s_errors == errors_before, "  no error logged");
    JSON_DECREF(jn_removed)

    /*
     *  A NULL handle is an error, logged
     */
    errors_before = s_errors;
    removed = rotatory_remove_old_files(NULL, 7, NULL, NULL);
    check(removed == -1 && s_errors - errors_before == 1, "NULL handle: -1, logged");

    /*
     *  At rotation: the size limit (1 mega) starts a new file, the newfile
     *  callback runs, and it removes the old file created before
     */
    make_file(AUDIT_DIR "/005-05_01_2026.log", 10, 10);
    rotatory_subscribe2newfile(hr, newfile_cb, hr);

    char line[1024];
    memset(line, 'a', sizeof(line)-1);
    line[sizeof(line)-1] = 0;
    for(int i=0; i<2200; i++) {    // > 2 megas: siz/1M > 1
        rotatory_write(hr, LOG_AUDIT, line, strlen(line));
    }
    check(s_newfile_calls >= 1 && s_removed_at_rotation == 1 &&
        !exists_no_follow(AUDIT_DIR "/005-05_01_2026.log"),
        "a rotation calls newfile, which applies the retention"
    );

    /*
     *  What a first sweep of a big directory costs (at start only)
     */
    for(int i=0; i<400; i++) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%03d-01_01_2025.log", AUDIT_DIR, i % 366 + 1);
        if(i >= 366) {
            snprintf(path, sizeof(path), "%s/%03d-01_01_2025.log.OLD", AUDIT_DIR, i - 365);
        }
        make_file(path, 100, 60);
    }
    uint64_t t0 = time_in_milliseconds_monotonic();
    removed = rotatory_remove_old_files(hr, 7, NULL, NULL);
    uint64_t t1 = time_in_milliseconds_monotonic();
    check(removed == 400, "a first sweep of 400 old files removes all of them");
    printf("     (400 files swept in %" PRIu64 " ms)\n", t1 - t0);

    rotatory_close(hr);
    rmrdir(BASE);
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
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
    rotatory_start_up();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("count_errors", 0, count_errors, 0);
    gobj_log_add_handler("count_errors", "count_errors", LOG_OPT_UP_ERROR, 0);

    test_retention();

    rotatory_end();
    gobj_end();

    int result = global_result;
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
