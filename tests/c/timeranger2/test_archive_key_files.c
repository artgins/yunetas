/****************************************************************************
 *          test_archive_key_files.c
 *
 *  Retention that keeps what it takes: tranger2_archive_key_files() is the
 *  purge of tranger2_delete_key_files(), and the files it takes out of the
 *  topic are hard-linked into a BATCH of an archive directory
 *  (DESIGN-retention.md §12), for another process to send or read.
 *
 *  What is checked:
 *
 *      - a purge makes one batch: the content and the md2 of every file
 *        taken, the schema of the topic, and a manifest that says from
 *        which g_rowid; the topic keeps its numbering, as with a delete;
 *      - a second purge makes a second batch, whose manifest starts after
 *        the first;
 *      - the guards: no archive_directory, a relative one, one that does
 *        not exist, a file, one inside the tranger, one that holds it, and
 *        one on another filesystem (when the machine has one) are refused,
 *        with nothing deleted;
 *      - a base that cannot be written removes the batch, and deletes
 *        nothing; the next purge goes through;
 *      - a batch left half built (`.new-*`, a crash) is removed by the next
 *        archive of the key;
 *      - a batch with the name of this one is refused, nothing deleted;
 *      - a reopen of the master and a replica read the numbering of the
 *        base.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP "test_archive_key_files"

#define DATABASE    "tr_archive_key_files"
#define ARCHIVE     "archive_key_files"
#define TOPIC_NAME  "topic_archive_key_files"
#define KEY         "0000000000000000001"
#define KEY2        "0000000000000000002"   // a base that cannot be written
#define KEY3        "0000000000000000003"   // a batch left half built
#define KEY4        "0000000000000000004"   // a batch that exists already
#define DAY1        946684800   // 2000-01-01
#define DAY2        (DAY1 + 86400)
#define DAY3        (DAY2 + 86400)
#define DAY4        (DAY3 + 86400)

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE char path_root[PATH_MAX];
PRIVATE char path_database[PATH_MAX];
PRIVATE char path_archive[PATH_MAX];

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int expect(const char *what, const char *got, const char *expected);
PRIVATE int expect_int(const char *what, json_int_t got, json_int_t expected);

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE json_t *startup_tranger(BOOL master)
{
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i, s:s, s:i, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", master?1:0,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y",
        "xpermission" , 02770,
        "rpermission", 0600
    );
    return tranger2_startup(0, jn_tranger, yev_loop);
}

PRIVATE json_t *create_topic(json_t *tranger)
{
    return tranger2_create_topic(
        tranger, TOPIC_NAME, "id", "tm",
        json_pack("{s:i, s:s, s:i, s:i}",
            "on_critical_error", 4,
            "filename_mask", "%Y-%m-%d",
            "xpermission" , 02700,
            "rpermission", 0600
        ),
        sf_int_key,
        json_pack("{s:s, s:I, s:s}",
            "id", "",
            "tm", (json_int_t)0,
            "content", ""
        ),
        0
    );
}

/*
 *  Append one record of key `id`, return its g_rowid (-1 refused)
 */
PRIVATE json_int_t append_one(json_t *tranger, json_int_t id, uint64_t t, const char *content)
{
    json_t *record = json_pack("{s:I, s:I, s:s}",
        "id", id,
        "tm", (json_int_t)t,
        "content", content
    );
    md2_record_ex_t md = {0};
    if(tranger2_append_record(tranger, TOPIC_NAME, t, 0, &md, record) < 0) {
        return -1;
    }
    return (json_int_t)md.g_rowid;
}

PRIVATE void drain(int turns)
{
    for(int i = 0; i < turns; i++) {
        yev_loop_run_once(yev_loop);
    }
}

/*
 *  "<content>:<g_rowid> ..." of what an iterator of `key` loads
 */
PRIVATE void served(json_t *tranger, const char *key, char *bf, size_t bfsize)
{
    json_t *data = json_array();
    json_t *it = tranger2_open_iterator(
        tranger, TOPIC_NAME, key, json_object(), NULL, "served", "", data, NULL
    );
    bf[0] = 0;
    int idx; json_t *record;
    json_array_foreach(data, idx, record) {
        char item[64];
        snprintf(item, sizeof(item), "%s%s:%ld",
            bf[0]? " ": "",
            kw_get_str(0, record, "content", "?", 0),
            (long)kw_get_int(0, record, "__md_tranger__`g_rowid", -1, 0)
        );
        strncat(bf, item, bfsize - strlen(bf) - 1);
    }
    if(it) {
        tranger2_close_iterator(tranger, it);
    }
    JSON_DECREF(data)
}

PRIVATE BOOL key_file_exists(const char *key, const char *filename)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), path_database, TOPIC_NAME, "keys", key, filename, NULL);
    return access(path, F_OK) == 0;
}

PRIVATE void key_archive_path(const char *key, char *bf, size_t bfsize)
{
    build_path(bf, bfsize, path_archive, DATABASE, TOPIC_NAME, key, NULL);
}

/*
 *  How many ready marks name a batch of `key`
 */
PRIVATE int count_ready_marks(const char *key)
{
    char ready_dir[PATH_MAX];
    char prefix[NAME_MAX + 1];
    build_path(ready_dir, sizeof(ready_dir), path_archive, ".ready", NULL);
    snprintf(prefix, sizeof(prefix), "%s%%2F%s%%2F%s%%2F", DATABASE, TOPIC_NAME, key);
    int n = 0;
    DIR *dir = opendir(ready_dir);
    struct dirent *entry;
    while(dir && (entry = readdir(dir)) != NULL) {
        if(strncmp(entry->d_name, prefix, strlen(prefix)) == 0) {
            n++;
        }
    }
    if(dir) {
        closedir(dir);
    }
    return n;
}

/*
 *  The ready mark of a batch (its path), is it there?
 */
PRIVATE BOOL ready_mark_exists(const char *key, const char *batch_path)
{
    const char *batch_name = strrchr(batch_path, '/');
    batch_name = batch_name? batch_name + 1 : batch_path;
    char mark[NAME_MAX + 1];
    char mark_path[PATH_MAX];
    snprintf(mark, sizeof(mark), "%s%%2F%s%%2F%s%%2F%s", DATABASE, TOPIC_NAME, key, batch_name);
    build_path(mark_path, sizeof(mark_path), path_archive, ".ready", mark, NULL);
    return access(mark_path, F_OK) == 0;
}

/*
 *  How many batches a key has, and how many entries that start with '.'
 */
PRIVATE int count_batches(const char *key, int *dot_entries)
{
    char dir_path[PATH_MAX];
    key_archive_path(key, dir_path, sizeof(dir_path));
    *dot_entries = 0;
    int n = 0;
    DIR *dir = opendir(dir_path);
    struct dirent *entry;
    while(dir && (entry = readdir(dir)) != NULL) {
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if(entry->d_name[0] == '.') {
            (*dot_entries)++;
            continue;
        }
        n++;
    }
    if(dir) {
        closedir(dir);
    }
    return n;
}

/*
 *  The manifest of the batch of `key` whose name starts with `upto@`
 */
PRIVATE json_t *read_manifest(const char *key, const char *upto, char *batch, size_t batchsize)
{
    char dir_path[PATH_MAX];
    key_archive_path(key, dir_path, sizeof(dir_path));
    batch[0] = 0;
    DIR *dir = opendir(dir_path);
    struct dirent *entry;
    size_t len = strlen(upto);
    while(dir && (entry = readdir(dir)) != NULL) {
        if(strncmp(entry->d_name, upto, len) == 0 && entry->d_name[len] == '@') {
            build_path(batch, batchsize, dir_path, entry->d_name, NULL);
            break;
        }
    }
    if(dir) {
        closedir(dir);
    }
    if(!batch[0]) {
        return NULL;
    }
    char path[PATH_MAX];
    build_path(path, sizeof(path), batch, "manifest.json", NULL);
    return json_load_file(path, 0, NULL);
}

PRIVATE BOOL batch_file_exists(const char *batch, const char *filename)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), batch, filename, NULL);
    return access(path, F_OK) == 0;
}

/*
 *  Does a file of a batch hold `text`?
 */
PRIVATE BOOL batch_file_holds(const char *batch, const char *filename, const char *text)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), batch, filename, NULL);
    FILE *f = fopen(path, "r");
    if(!f) {
        return FALSE;
    }
    char content[4096];
    size_t n = fread(content, 1, sizeof(content), f);
    fclose(f);
    return memmem(content, n, text, strlen(text)) != NULL;   // the records are NUL-separated
}

PRIVATE int expect(const char *what, const char *got, const char *expected)
{
    if(strcmp(got, expected) != 0) {
        printf("%sERROR%s --> %s: '%s', expected '%s'\n",
            On_Red BWhite, Color_Off, what, got, expected);
        return -1;
    }
    return 0;
}

PRIVATE int expect_int(const char *what, json_int_t got, json_int_t expected)
{
    if(got != expected) {
        printf("%sERROR%s --> %s: %ld, expected %ld\n",
            On_Red BWhite, Color_Off, what, (long)got, (long)expected);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    char bf[512];
    char batch[PATH_MAX];
    int dots;

    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    build_path(path_archive, sizeof(path_archive), path_root, ARCHIVE, NULL);
    rmrdir(path_database);
    rmrdir(path_archive);
    mkrdir(path_archive, 02770);

    /*-------------------------------------*
     *  Three days of four keys
     *-------------------------------------*/
    set_expected_results(
        "archive key files: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tm = startup_tranger(TRUE);
    if(!tm || !create_topic(tm)) {
        if(tm) {
            tranger2_shutdown(tm);
        }
        return -1;
    }
    for(int d = 0; d < 3; d++) {
        for(int r = 0; r < 3; r++) {
            char content[8];
            snprintf(content, sizeof(content), "%c%d", 'A' + d, r + 1);
            append_one(tm, 1, DAY1 + (uint64_t)d*86400 + (uint64_t)r, content);
        }
        for(json_int_t k = 2; k <= 4; k++) {
            for(int r = 0; r < 2; r++) {
                char content[8];
                snprintf(content, sizeof(content), "%c%d", (char)('P' + d), r + 1);
                append_one(tm, k, DAY1 + (uint64_t)d*86400 + (uint64_t)r, content);
            }
        }
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  A purge makes a batch, and the
     *  topic keeps its numbering
     *-------------------------------------*/
    set_expected_results("archive key files: a batch", NULL, NULL, NULL, 1);
    result += expect_int("rows archived", tranger2_archive_key_files(tm, TOPIC_NAME, KEY, DAY2, path_archive), 3);
    result += expect_int("first day's md2 out of the topic", key_file_exists(KEY, "2000-01-01.md2"), 0);
    result += expect_int("first day's content out of the topic", key_file_exists(KEY, "2000-01-01.json"), 0);
    result += expect_int("base written", key_file_exists(KEY, ".purged"), 1);
    served(tm, KEY, bf, sizeof(bf));
    result += expect("served after the archive", bf, "B1:4 B2:5 B3:6 C1:7 C2:8 C3:9");

    result += expect_int("one batch", count_batches(KEY, &dots), 1);
    result += expect_int("nothing half built", dots, 0);
    json_t *manifest = read_manifest(KEY, "2000-01-01", batch, sizeof(batch));
    result += expect_int("manifest: rows", kw_get_int(0, manifest, "rows", -1, 0), 3);
    result += expect_int("manifest: first_g_rowid", kw_get_int(0, manifest, "first_g_rowid", -1, 0), 1);
    result += expect_int("manifest: last_g_rowid", kw_get_int(0, manifest, "last_g_rowid", -1, 0), 3);
    result += expect("manifest: upto", kw_get_str(0, manifest, "upto", "", 0), "2000-01-01");
    result += expect("manifest: key", kw_get_str(0, manifest, "key", "", 0), KEY);
    result += expect("manifest: topic", kw_get_str(0, manifest, "topic", "", 0), TOPIC_NAME);
    result += expect("manifest: database", kw_get_str(0, manifest, "database", "", 0), DATABASE);
    result += expect_int("manifest: files", (json_int_t)json_array_size(kw_get_list(0, manifest, "files", 0, 0)), 1);
    JSON_DECREF(manifest)
    result += expect_int("batch: the md2", batch_file_exists(batch, "2000-01-01.md2"), 1);
    result += expect_int("batch: the content", batch_file_exists(batch, "2000-01-01.json"), 1);
    result += expect_int("batch: the content holds the records", batch_file_holds(batch, "2000-01-01.json", "\"A3\""), 1);
    result += expect_int("batch: topic_desc.json", batch_file_exists(batch, "topic_desc.json"), 1);
    result += expect_int("batch: topic_cols.json", batch_file_exists(batch, "topic_cols.json"), 1);
    result += expect_int("batch: its ready mark", ready_mark_exists(KEY, batch), 1);
    result += expect_int("one ready mark", count_ready_marks(KEY), 1);
    result += expect_int("next append", append_one(tm, 1, DAY3 + 100, "D1"), 10);
    result += test_json(NULL);

    /*-------------------------------------*
     *  A second purge, a second batch
     *  that starts after the first
     *-------------------------------------*/
    set_expected_results("archive key files: a second batch", NULL, NULL, NULL, 1);
    result += expect_int("rows archived", tranger2_archive_key_files(tm, TOPIC_NAME, KEY, DAY3, path_archive), 3);
    result += expect_int("two batches", count_batches(KEY, &dots), 2);
    manifest = read_manifest(KEY, "2000-01-02", batch, sizeof(batch));
    result += expect_int("second manifest: first_g_rowid", kw_get_int(0, manifest, "first_g_rowid", -1, 0), 4);
    result += expect_int("second manifest: last_g_rowid", kw_get_int(0, manifest, "last_g_rowid", -1, 0), 6);
    JSON_DECREF(manifest)
    result += expect_int("batch: the content holds the records", batch_file_holds(batch, "2000-01-02.json", "\"B1\""), 1);
    result += expect_int("second batch: its ready mark", ready_mark_exists(KEY, batch), 1);
    result += expect_int("two ready marks", count_ready_marks(KEY), 2);
    result += expect_int("nothing more to archive", tranger2_archive_key_files(tm, TOPIC_NAME, KEY, DAY3, path_archive), 0);
    result += expect_int("still two batches", count_batches(KEY, &dots), 2);
    served(tm, KEY, bf, sizeof(bf));
    result += expect("served after the second archive", bf, "C1:7 C2:8 C3:9 D1:10");
    result += test_json(NULL);

    /*-------------------------------------*
     *  The guards: nothing deleted
     *-------------------------------------*/
    char a_file[PATH_MAX];
    build_path(a_file, sizeof(a_file), path_root, "archive_key_files_a_file", NULL);
    {
        FILE *f = fopen(a_file, "w");
        if(f) {
            fclose(f);
        }
    }
    char missing[PATH_MAX];
    build_path(missing, sizeof(missing), path_root, "archive_key_files_missing", NULL);
    rmrdir(missing);
    char inside[PATH_MAX];
    build_path(inside, sizeof(inside), path_database, TOPIC_NAME, NULL);

    set_expected_results(
        "archive key files: the guards",
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Cannot archive the old files of a key, what archive_directory?",
            "msg", "Cannot archive the old files of a key, archive_directory is not an absolute path",
            "msg", "Cannot archive the old files of a key, archive_directory cannot be resolved: it must exist",
            "msg", "Cannot archive the old files of a key, archive_directory is not a directory",
            "msg", "Cannot archive the old files of a key, archive_directory is inside the directory of the tranger, or holds it",
            "msg", "Cannot archive the old files of a key, archive_directory is inside the directory of the tranger, or holds it"
        ),
        NULL, NULL, 1
    );
    result += expect_int("no archive_directory", tranger2_archive_key_files(tm, TOPIC_NAME, KEY2, DAY2, ""), -1);
    result += expect_int("a relative one", tranger2_archive_key_files(tm, TOPIC_NAME, KEY2, DAY2, "tests_yuneta"), -1);
    result += expect_int("one that does not exist", tranger2_archive_key_files(tm, TOPIC_NAME, KEY2, DAY2, missing), -1);
    result += expect_int("a file", tranger2_archive_key_files(tm, TOPIC_NAME, KEY2, DAY2, a_file), -1);
    result += expect_int("inside the tranger", tranger2_archive_key_files(tm, TOPIC_NAME, KEY2, DAY2, inside), -1);
    result += expect_int("holding the tranger", tranger2_archive_key_files(tm, TOPIC_NAME, KEY2, DAY2, path_root), -1);
    result += expect_int("nothing deleted", (json_int_t)tranger2_topic_key_size(tm, TOPIC_NAME, KEY2), 6);
    result += expect_int("no base", key_file_exists(KEY2, ".purged"), 0);
    result += test_json(NULL);
    unlink(a_file);

    /*
     *  Another filesystem, when the machine has one at hand
     */
    struct stat st_shm;
    struct stat st_db;
    if(stat("/dev/shm", &st_shm) == 0 && S_ISDIR(st_shm.st_mode) &&
            stat(path_database, &st_db) == 0 && st_shm.st_dev != st_db.st_dev &&
            access("/dev/shm", W_OK) == 0) {
        set_expected_results(
            "archive key files: another filesystem",
            json_pack("[{s:s}]",
                "msg", "Cannot archive the old files of a key, archive_directory is on another filesystem than the topic: a hard link cannot cross it"
            ),
            NULL, NULL, 1
        );
        result += expect_int("another filesystem", tranger2_archive_key_files(tm, TOPIC_NAME, KEY2, DAY2, "/dev/shm"), -1);
        result += expect_int("nothing deleted", (json_int_t)tranger2_topic_key_size(tm, TOPIC_NAME, KEY2), 6);
        result += test_json(NULL);
    }

    /*-------------------------------------*
     *  A base that cannot be written: the
     *  batch goes, nothing deleted
     *-------------------------------------*/
    if(geteuid() != 0) {
        set_expected_results(
            "archive key files: a base that cannot be written",
            json_pack("[{s:s}]",
                "msg", "Cannot replace .purged, cannot create the temporary file"
            ),
            NULL, NULL, 1
        );
        char key_dir[PATH_MAX];
        build_path(key_dir, sizeof(key_dir), path_database, TOPIC_NAME, "keys", KEY2, NULL);
        chmod(key_dir, 0550);
        result += expect_int("the archive fails", tranger2_archive_key_files(tm, TOPIC_NAME, KEY2, DAY2, path_archive), -1);
        chmod(key_dir, 0770);
        result += expect_int("no batch", count_batches(KEY2, &dots), 0);
        result += expect_int("no ready mark", count_ready_marks(KEY2), 0);
        result += expect_int("nothing half built", dots, 0);
        result += expect_int("nothing deleted", (json_int_t)tranger2_topic_key_size(tm, TOPIC_NAME, KEY2), 6);
        result += expect_int("the md2 still in the topic", key_file_exists(KEY2, "2000-01-01.md2"), 1);
        result += test_json(NULL);
    }
    set_expected_results("archive key files: the next purge goes through", NULL, NULL, NULL, 1);
    result += expect_int("rows archived", tranger2_archive_key_files(tm, TOPIC_NAME, KEY2, DAY2, path_archive), 2);
    result += expect_int("one batch", count_batches(KEY2, &dots), 1);
    result += expect_int("one ready mark", count_ready_marks(KEY2), 1);
    served(tm, KEY2, bf, sizeof(bf));
    result += expect("served", bf, "Q1:3 Q2:4 R1:5 R2:6");
    result += test_json(NULL);

    /*-------------------------------------*
     *  A batch left half built by a crash
     *-------------------------------------*/
    char left[PATH_MAX];
    key_archive_path(KEY3, bf, sizeof(bf));
    build_path(left, sizeof(left), bf, ".new-2000-01-01@1", NULL);
    mkrdir(left, 02770);
    {
        char junk[PATH_MAX];
        build_path(junk, sizeof(junk), left, "2000-01-01.md2", NULL);
        FILE *f = fopen(junk, "w");
        if(f) {
            fclose(f);
        }
    }
    set_expected_results(
        "archive key files: a batch left half built",
        json_pack("[{s:s}]",
            "msg", "Batch of an archive that was not finished removed: its purge did not happen"
        ),
        NULL, NULL, 1
    );
    result += expect_int("rows archived", tranger2_archive_key_files(tm, TOPIC_NAME, KEY3, DAY2, path_archive), 2);
    result += expect_int("one batch", count_batches(KEY3, &dots), 1);
    result += expect_int("the half built one is gone", dots, 0);
    result += test_json(NULL);

    /*-------------------------------------*
     *  A batch with the name of this one
     *  (a crash and a purge in the same
     *  second) is refused, nothing deleted
     *-------------------------------------*/
    key_archive_path(KEY4, bf, sizeof(bf));
    uint64_t now = time_in_seconds();
    for(uint64_t t = now; t <= now + 2; t++) {
        char name[NAME_MAX];
        char taken[PATH_MAX];
        snprintf(name, sizeof(name), "2000-01-01@%" PRIu64, t);
        build_path(taken, sizeof(taken), bf, name, NULL);
        mkrdir(taken, 02770);
    }
    set_expected_results(
        "archive key files: a batch that exists already",
        json_pack("[{s:s}]",
            "msg", "Cannot archive the old files of a key, its batch already exists"
        ),
        NULL, NULL, 1
    );
    result += expect_int("the archive fails", tranger2_archive_key_files(tm, TOPIC_NAME, KEY4, DAY2, path_archive), -1);
    result += expect_int("nothing deleted", (json_int_t)tranger2_topic_key_size(tm, TOPIC_NAME, KEY4), 6);
    result += expect_int("no base", key_file_exists(KEY4, ".purged"), 0);
    result += test_json(NULL);

    /*-------------------------------------*
     *  A reopen and a replica read the
     *  numbering of the base
     *-------------------------------------*/
    set_expected_results("archive key files: reopen and replica", NULL, NULL, NULL, 1);
    tranger2_shutdown(tm);
    drain(10);
    json_t *tr = startup_tranger(FALSE);
    if(!tr || !tranger2_open_topic(tr, TOPIC_NAME, TRUE)) {
        printf("%sERROR%s --> cannot open the replica\n", On_Red BWhite, Color_Off);
        if(tr) {
            tranger2_shutdown(tr);
        }
        return -1;
    }
    served(tr, KEY, bf, sizeof(bf));
    result += expect("replica", bf, "C1:7 C2:8 C3:9 D1:10");
    tranger2_shutdown(tr);
    drain(10);
    tm = startup_tranger(TRUE);
    if(!tm || !tranger2_open_topic(tm, TOPIC_NAME, TRUE)) {
        printf("%sERROR%s --> cannot reopen the master\n", On_Red BWhite, Color_Off);
        if(tm) {
            tranger2_shutdown(tm);
        }
        return -1;
    }
    served(tm, KEY, bf, sizeof(bf));
    result += expect("master", bf, "C1:7 C2:8 C3:9 D1:10");
    result += expect_int("next append", append_one(tm, 1, DAY4, "E1"), 11);
    result += test_json(NULL);

    set_expected_results("archive key files: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tm);
    drain(10);
    result += test_json(NULL);

    rmrdir(path_archive);
    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_free_fn_t free_func;
    gbmem_get_json_allocators(&malloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("testing", 0, capture_log_write, 0);
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    yev_loop_create(0, 2024, 10, NULL, &yev_loop);

    int result = do_test();

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

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
