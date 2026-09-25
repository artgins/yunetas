/****************************************************************************
 *          test_tr_dt_unknown.c
 *
 *  Regression coverage for a filesystem that gives readdir() no d_type
 *  (XFS with ftype=0, NFS, FUSE, overlay).
 *
 *  find_keys_in_disk() then asks the inode with stat(), and it joined the
 *  entry to the TOPIC's directory instead of its keys/: <topic>/<key> did
 *  not exist, no key was a directory, and the topic opened with an EMPTY
 *  cache over files that were all there -- reads answered 0 rows, and the
 *  first append started a cell {rows:1} over a file that held N.
 *
 *  ext4 and tmpfs always fill d_type, and a static binary cannot be
 *  LD_PRELOADed, so the test links with -Wl,--wrap=readdir: every readdir()
 *  of the libraries goes through __wrap_readdir() below, which can hide the
 *  type as such a filesystem does.
 *
 *      1. write a topic with two keys and five records, and close it
 *      2. reopen it with d_type hidden: both keys and all five records
 *      3. reopen it with d_type hidden and the lstat() of one key failing
 *         (EIO, by __wrap_lstat() below): the topic does not open, logged.
 *         Up to this fix the key was taken as "not a directory" and left
 *         out of the cache with no log -- the topic opened without it, and
 *         a treedb accepted a create of its id. Only ENOENT (the key went
 *         away between the readdir() and the lstat()) leaves a key out.
 *      4. a symbolic link in keys/ is not a key without d_type either (it
 *         was asked with stat(), which follows it: a third key, with the
 *         records of the key it points to)
 *      5. a key directory of mode r-- (read, not searched) is flagged the
 *         same without d_type as with it (it loaded EMPTY and unflagged:
 *         the lstat() of each md2 file failed with EACCES, and the file
 *         was skipped with no log)
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include <gobj.h>
#include <kwid.h>
#include <helpers.h>
#include <timeranger2.h>
#include <testing.h>

#define APP         "test_tr_dt_unknown"
#define DATABASE    "tr_dt_unknown"
#define TOPIC_NAME  "topic_dt_unknown"
#define BASE_T      946684800   // 2000-01-01T00:00:00+0000

/***************************************************************
 *              The readdir() of a filesystem without d_type
 ***************************************************************/
struct dirent *__real_readdir(DIR *dirp);
struct dirent *__wrap_readdir(DIR *dirp);

PRIVATE BOOL hide_d_type = FALSE;
PRIVATE int entries_without_type = 0;

struct dirent *__wrap_readdir(DIR *dirp)
{
    struct dirent *entry = __real_readdir(dirp);
    if(entry && hide_d_type) {
        entry->d_type = DT_UNKNOWN;
        entries_without_type++;
    }
    return entry;
}

/***************************************************************
 *              An lstat() that fails (EIO) for one path
 ***************************************************************/
int __real_lstat(const char *path, struct stat *st);
int __wrap_lstat(const char *path, struct stat *st);

PRIVATE char failing_stat[PATH_MAX] = "";
PRIVATE int stat_failures = 0;

int __wrap_lstat(const char *path, struct stat *st)
{
    if(failing_stat[0] && strcmp(path, failing_stat) == 0) {
        stat_failures++;
        errno = EIO;
        return -1;
    }
    return __real_lstat(path, st);
}

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE json_t *startup_tranger(const char *path_root)
{
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    );
    return tranger2_startup(0, jn_tranger, 0);
}

PRIVATE int append_key(json_t *tranger, const char *key, int n)
{
    for(int j = 0; j < n; j++) {
        md2_record_ex_t md = {0};
        json_t *jn_record = json_pack("{s:s, s:I, s:s}",
            "id", key,
            "tm", (json_int_t)(BASE_T + j),
            "content", "payload"
        );
        if(tranger2_append_record(tranger, TOPIC_NAME, BASE_T + j, 0, &md, jn_record) < 0) {
            return -1;
        }
    }
    return 0;
}

PRIVATE int expect_int(const char *what, long long got, long long expected)
{
    if(got != expected) {
        printf("%sERROR%s --> %s: got %lld, expected %lld\n",
            On_Red BWhite, Color_Off, what, got, expected);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Open the topic, with d_type shown or hidden: its keys, records and the
 *  md2 files of key A flagged unreadable (-1 when it does not open)
 ***************************************************************************/
PRIVATE int open_and_count(
    const char *path_root,
    BOOL hidden,
    int *keys_found,
    int *records_found,
    int *unreadable_a
)
{
    *keys_found = -1;
    *records_found = -1;
    *unreadable_a = -1;
    hide_d_type = hidden;
    json_t *tranger = startup_tranger(path_root);
    json_t *topic = tranger? tranger2_open_topic(tranger, TOPIC_NAME, FALSE): NULL;
    hide_d_type = FALSE;
    if(topic) {
        json_t *keys = tranger2_list_keys(tranger, TOPIC_NAME);
        *keys_found = (int)json_array_size(keys);
        JSON_DECREF(keys)
        *records_found = (int)tranger2_topic_size(tranger, TOPIC_NAME);
        json_t *key_cache = json_object_get(json_object_get(topic, "cache"), "A");
        *unreadable_a = (int)json_array_size(json_object_get(key_cache, "unreadable"));
    }
    if(tranger) {
        tranger2_shutdown(tranger);
    }
    return topic? 0: -1;
}

/***************************************************************************
 *  4. A symbolic link in keys/ is not a key, with d_type or without it.
 *  With d_type it is DT_LNK and skipped; without it the key was asked with
 *  stat(), which follows the link: a key directory whose name nobody
 *  wrote, with the records of another key (and a link that answered ELOOP
 *  or EACCES failed the whole open, only there). Now lstat(), as the entry
 *  type says.
 ***************************************************************************/
PRIVATE int test_symlink_key(const char *path_root, const char *path_database)
{
    int result = 0;
    char link_path[PATH_MAX];
    build_path(link_path, sizeof(link_path), path_database, TOPIC_NAME, "keys", "L", NULL);
    unlink(link_path);
    if(symlink("A", link_path) < 0) {
        printf("%sERROR%s --> cannot make the link %s\n", On_Red BWhite, Color_Off, link_path);
        return -1;
    }

    int keys_found, records_found, unreadable_a;
    set_expected_results("a link in keys/ is not a key, with d_type", NULL, NULL, NULL, 1);
    result += open_and_count(path_root, FALSE, &keys_found, &records_found, &unreadable_a);
    result += expect_int("keys with d_type, a link in keys/", keys_found, 2);
    result += expect_int("records with d_type, a link in keys/", records_found, 5);
    result += test_json(NULL);

    set_expected_results("a link in keys/ is not a key, without d_type", NULL, NULL, NULL, 1);
    entries_without_type = 0;
    result += open_and_count(path_root, TRUE, &keys_found, &records_found, &unreadable_a);
    if(entries_without_type == 0) {
        printf("%sERROR%s --> no entry without d_type: the test proves nothing\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_int("keys without d_type, a link in keys/", keys_found, 2);
    result += expect_int("records without d_type, a link in keys/", records_found, 5);
    result += test_json(NULL);

    unlink(link_path);
    return result;
}

/***************************************************************************
 *  5. A key directory that can be read and not searched (r-- : its
 *  entries are named, none can be asked or opened). With d_type its md2
 *  files are listed, their open fails, and the key is flagged unreadable:
 *  every load of it says load_failed. Without d_type the lstat() of each
 *  entry failed with EACCES and the entry was skipped with no log, so the
 *  key loaded EMPTY and unflagged. Now the entry is listed, as the entry
 *  type lists it, and both paths flag the key the same way.
 *  Skipped as root (the mode does not stop it).
 ***************************************************************************/
PRIVATE int test_unsearchable_key(const char *path_root, const char *path_database)
{
    if(geteuid() == 0) {
        printf("skip unsearchable key: as root a directory of mode r-- can be searched\n");
        return 0;
    }
    int result = 0;
    char key_dir[PATH_MAX];
    build_path(key_dir, sizeof(key_dir), path_database, TOPIC_NAME, "keys", "A", NULL);
    chmod(key_dir, 0440);   // read, not searched

    int keys_d, records_d, unreadable_d;
    set_expected_results("a key of mode r--, with d_type", NULL, NULL, NULL, 0);
    result += open_and_count(path_root, FALSE, &keys_d, &records_d, &unreadable_d);
    test_json(NULL);    // what the open logs is checked below, against the other path
    result += expect_int("with d_type, the key of mode r-- is flagged", unreadable_d > 0, 1);

    int keys_u, records_u, unreadable_u;
    set_expected_results("a key of mode r--, without d_type", NULL, NULL, NULL, 0);
    result += open_and_count(path_root, TRUE, &keys_u, &records_u, &unreadable_u);
    test_json(NULL);
    result += expect_int("without d_type, the keys are the same", keys_u, keys_d);
    result += expect_int("without d_type, the records are the same", records_u, records_d);
    result += expect_int("without d_type, the key of mode r-- is flagged the same",
        unreadable_u, unreadable_d);

    chmod(key_dir, 02770);
    return result;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];

    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    char key_a[PATH_MAX];
    build_path(key_a, sizeof(key_a), path_database, TOPIC_NAME, "keys", "A", NULL);
    chmod(key_a, 02770);    // a run that died left it r--
    rmrdir(path_database);

    /*-------------------------------------*
     *  1. Write the topic and close it
     *-------------------------------------*/
    set_expected_results(
        "write a topic with two keys",
        json_pack("[{s:s}, {s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = startup_tranger(path_root);
    if(!tranger) {
        return -1;
    }
    json_t *topic = tranger2_create_topic(
        tranger,
        TOPIC_NAME,
        "id",   // pkey
        "tm",   // tkey
        NULL,
        sf_string_key,
        json_pack("{s:s, s:I, s:s}",
            "id", "",
            "tm", (json_int_t)0,
            "content", ""
        ),
        0
    );
    if(!topic || append_key(tranger, "A", 3) < 0 || append_key(tranger, "B", 2) < 0) {
        printf("%sERROR%s --> cannot write the topic\n", On_Red BWhite, Color_Off);
        tranger2_shutdown(tranger);
        return -1;
    }
    result += expect_int("records written", (long long)tranger2_topic_size(tranger, TOPIC_NAME), 5);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    /*-------------------------------------*
     *  2. Reopen it with d_type hidden
     *-------------------------------------*/
    set_expected_results("reopen it on a filesystem without d_type", NULL, NULL, NULL, 1);
    hide_d_type = TRUE;
    tranger = startup_tranger(path_root);
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> cannot reopen the topic\n", On_Red BWhite, Color_Off);
        hide_d_type = FALSE;
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    if(entries_without_type == 0) {
        printf("%sERROR%s --> the reopen read no entry without d_type: the test proves nothing\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_int("records found without d_type",
        (long long)tranger2_topic_size(tranger, TOPIC_NAME), 5);
    json_t *keys = tranger2_list_keys(tranger, TOPIC_NAME);
    result += expect_int("keys found without d_type", (long long)json_array_size(keys), 2);
    JSON_DECREF(keys)
    tranger2_shutdown(tranger);
    hide_d_type = FALSE;
    result += test_json(NULL);

    /*-------------------------------------*
     *  3. The stat() of a key fails
     *-------------------------------------*/
    set_expected_results(
        "a key that cannot be stat'ed fails the listing",
        json_pack("[{s:s}, {s:s}]",
            "msg", "Cannot list the keys of the topic, stat() FAILED",
            "msg", "Cannot open topic: its keys cannot be listed"
        ),
        NULL, NULL, 1
    );
    build_path(failing_stat, sizeof(failing_stat), path_database, TOPIC_NAME, "keys", "B", NULL);
    stat_failures = 0;
    hide_d_type = TRUE;
    tranger = startup_tranger(path_root);
    json_t *opened = tranger? tranger2_open_topic(tranger, TOPIC_NAME, FALSE): NULL;
    hide_d_type = FALSE;
    failing_stat[0] = 0;
    if(stat_failures == 0) {
        printf("%sERROR%s --> no stat() of the key failed: the test proves nothing\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(opened) {
        printf("%sERROR%s --> the topic opened with a key it could not stat, %d records\n",
            On_Red BWhite, Color_Off, (int)tranger2_topic_size(tranger, TOPIC_NAME));
        result += -1;
    }
    if(tranger) {
        tranger2_shutdown(tranger);
    }
    result += test_json(NULL);

    result += test_symlink_key(path_root, path_database);
    result += test_unsearchable_key(path_root, path_database);

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
    } else {
        printf("<-- %sTEST OK%s: %s\n", On_Green BWhite, Color_Off, APP);
    }
    return result < 0? -1 : 0;
}
