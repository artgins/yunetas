/****************************************************************************
 *          test_unlisted_relist_once.c
 *
 *  A key directory that OPENS and cannot be LISTED (readdir() fails: EIO,
 *  ENOMEM) at the open of the topic is flagged `unlisted`, as one that
 *  cannot be opened. Every load of the key says load_failed.
 *
 *  A load tries the flag again only when its cause may be gone: the
 *  directory changed since it was flagged (its ctime: its mode, its
 *  entries), or it could not be opened then and opens now. Up to this fix
 *  a directory that opened was listed again at EVERY load -- and its
 *  failure logged again at every load -- while the doc said the load logs
 *  nothing more.
 *
 *      1. the open flags key A: the listing failure is logged, once
 *      2. three loads of A: load_failed, and nothing more in the log than
 *         what a load of a flagged key says
 *      3. the listing works again, the directory unchanged: a load still
 *         says load_failed, and lists nothing
 *      4. the directory changes (chmod): the next load lists the key
 *         again, and reads it whole
 *
 *  The failure of readdir() is made by __wrap_readdir() below, for the
 *  directory of key A (seen at its opendir(), forgotten at its closedir()).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <testing.h>

#define APP         "test_unlisted_relist_once"
#define DATABASE    "tr_unlisted_relist_once"
#define TOPIC_NAME  "topic_relist_once"
#define DAY1        946684800   // 2000-01-01
#define DAY         86400

#define MSG_READDIR     "Cannot list directory, readdir() FAILED"
#define MSG_UNLISTED    "key directory cannot be listed when its cache was built: every load of the key says load_failed"
#define MSG_ITER        "The history of the key is not whole: its directory could not be listed when its cache was built"
#define MSG_RELISTED    "key directory listed again: its files are counted, and the key is not flagged"

/***************************************************************
 *              The readdir() of a failing disk
 ***************************************************************/
DIR *__real_opendir(const char *name);
DIR *__wrap_opendir(const char *name);
struct dirent *__real_readdir(DIR *dirp);
struct dirent *__wrap_readdir(DIR *dirp);
int __real_closedir(DIR *dirp);
int __wrap_closedir(DIR *dirp);

PRIVATE char failing_dir[PATH_MAX] = "";   // the directory whose readdir() fails
PRIVATE DIR *failing_dirp = NULL;
PRIVATE int readdir_failures = 0;

DIR *__wrap_opendir(const char *name)
{
    DIR *dirp = __real_opendir(name);
    if(dirp && failing_dir[0] && strcmp(name, failing_dir) == 0) {
        failing_dirp = dirp;
    }
    return dirp;
}

struct dirent *__wrap_readdir(DIR *dirp)
{
    if(dirp && dirp == failing_dirp) {
        failing_dirp = NULL;
        readdir_failures++;
        errno = EIO;
        return NULL;
    }
    return __real_readdir(dirp);
}

int __wrap_closedir(DIR *dirp)
{
    if(dirp && dirp == failing_dirp) {
        failing_dirp = NULL;    // opened and closed without a readdir()
    }
    return __real_closedir(dirp);
}

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE char path_root[PATH_MAX];
PRIVATE char path_database[PATH_MAX];
PRIVATE char got[512];

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE int on_record(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *record
)
{
    size_t ln = strlen(got);
    snprintf(got + ln, sizeof(got) - ln, "%s%s@%d", ln? " ": "", key,
        (int)kw_get_int(0, record, "v", 0, 0));
    JSON_DECREF(record)
    return 0;
}

PRIVATE json_t *startup(void)
{
    return tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i, s:s}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y-%m-%d"
    ), 0);
}

PRIVATE int append(json_t *tranger, const char *key, int day, int v)
{
    md2_record_ex_t md = {0};
    return tranger2_append_record(tranger, TOPIC_NAME, (uint64_t)(DAY1 + day*DAY), 0, &md,
        json_pack("{s:s, s:I, s:i}", "id", key, "tm", (json_int_t)(100 + v), "v", v)
    );
}

/*
 *  A load of the key: what it loads, and whether it says load_failed
 */
PRIVATE int check_load(
    json_t *tranger,
    const char *what,
    const char *key,
    const char *expected,
    BOOL expect_failed
)
{
    got[0] = 0;
    json_t *it = tranger2_open_iterator(
        tranger, TOPIC_NAME, key, NULL, on_record, "it", "test", NULL, NULL
    );
    if(!it) {
        printf("%sERROR%s --> %s: no iterator\n", On_Red BWhite, Color_Off, what);
        return -1;
    }
    int result = 0;
    if(strcmp(got, expected) != 0) {
        printf("%sERROR%s --> %s: [%s], expected [%s]\n",
            On_Red BWhite, Color_Off, what, got, expected);
        result += -1;
    }
    BOOL failed = json_is_true(json_object_get(it, "load_failed"));
    if(failed != expect_failed) {
        printf("%sERROR%s --> %s: load_failed %d, expected %d\n",
            On_Red BWhite, Color_Off, what, failed, expect_failed);
        result += -1;
    }
    tranger2_close_iterator(tranger, it);
    return result;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;

    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    set_expected_results(
        "setup: key A with 3 rows",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = startup();
    tranger2_create_topic(
        tranger, TOPIC_NAME, "id", "tm", NULL, sf_string_key,
        json_pack("{s:s, s:I, s:I}", "id", "", "tm", (json_int_t)0, "v", (json_int_t)0),
        0
    );
    for(int i = 1; i <= 3; i++) {
        append(tranger, "A", i, i);
    }
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    /*
     *  1. The directory of A opens, and cannot be listed
     */
    build_path(failing_dir, sizeof(failing_dir), path_database, TOPIC_NAME, "keys", "A", NULL);
    set_expected_results(
        "1. the open flags key A, and logs it once",
        json_pack("[{s:s},{s:s}]",
            "msg", MSG_READDIR,
            "msg", MSG_UNLISTED
        ),
        NULL, NULL, 1
    );
    tranger = startup();
    tranger2_open_topic(tranger, TOPIC_NAME, TRUE);
    if(readdir_failures != 1) {
        printf("%sERROR%s --> 1. %d failures of readdir(), expected 1\n",
            On_Red BWhite, Color_Off, readdir_failures);
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  2. Three loads, with the listing still failing: load_failed, and
     *  not listed again (the directory did not change)
     */
    readdir_failures = 0;
    set_expected_results(
        "2. a load of the flagged key does not list it again",
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", MSG_ITER,
            "msg", MSG_ITER,
            "msg", MSG_ITER
        ),
        NULL, NULL, 1
    );
    for(int i = 0; i < 3; i++) {
        result += check_load(tranger, "2. a load of A", "A", "", TRUE);
    }
    if(readdir_failures != 0) {
        printf("%sERROR%s --> 2. the key was listed again %d time(s)\n",
            On_Red BWhite, Color_Off, readdir_failures);
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  3. The listing works again, the directory unchanged: still flagged
     */
    failing_dir[0] = 0;
    set_expected_results(
        "3. the directory unchanged: still flagged",
        json_pack("[{s:s}]",
            "msg", MSG_ITER
        ),
        NULL, NULL, 1
    );
    result += check_load(tranger, "3. a load of A", "A", "", TRUE);
    result += test_json(NULL);

    /*
     *  4. The directory changes: the next load lists it, and reads it whole
     */
    char key_dir[PATH_MAX];
    build_path(key_dir, sizeof(key_dir), path_database, TOPIC_NAME, "keys", "A", NULL);
    struct stat st;
    stat(key_dir, &st);
    chmod(key_dir, st.st_mode & 07777);     // the same mode: only its ctime moves
    set_expected_results(
        "4. the directory changed: the load lists the key again",
        json_pack("[{s:s}]",
            "msg", MSG_RELISTED
        ),
        NULL, NULL, 1
    );
    result += check_load(tranger, "4. a load of A", "A", "A@1 A@2 A@3", FALSE);
    result += test_json(NULL);

    set_expected_results("shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    rmrdir(path_database);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;
    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("testing", 0, capture_log_write, 0);
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    int result = do_test();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result < 0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    }
    return result < 0? -1 : 0;
}
