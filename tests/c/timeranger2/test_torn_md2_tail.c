/****************************************************************************
 *          test_torn_md2_tail.c
 *
 *  A md2 file whose size is not a whole number of rows: its last row is
 *  torn. A power cut during the 32-byte write of a md2 row leaves this
 *  shape. The md2 row is the commit point of an append (the content is
 *  written first, the row after), so a torn last row is an append that was
 *  never acknowledged. It is not damage.
 *
 *  Up to 7.25.4 the cache build logged a CRITICAL ("Cannot read last
 *  record, md2 file corrupted") and left the whole file out of the key, on
 *  a master and on a replica: its acknowledged rows were missing from
 *  every load, and nothing failed.
 *
 *  The cases:
 *      1. a MASTER opens a store whose last md2 of a key has 3 whole
 *         rows and 13 bytes more: the md2 is cut back to the 3 rows, with
 *         ONE warning. The key loads whole, the next append goes into the
 *         same file and is read back, and the md2 size is a whole number
 *         of rows again. A restart finds nothing to say.
 *      2. a REPLICA opens the same damaged store: it writes nothing (the
 *         md2 keeps its 13 bytes), it reads the 3 whole rows, and it logs
 *         nothing. A replica can see a row of a live master half written.
 *         Then a master opens the store and cuts the md2 back (case 1).
 *      3. a md2 of 13 bytes only (its first row is torn): the master cuts
 *         it to 0 bytes, and a md2 of 0 rows beside a content file that is
 *         not empty is ignored with its own warning (test_uncommitted_
 *         append.c). The next append into the file is its row 1.
 *      4. a RUNNING master finds its md2 torn at the next append (a row
 *         written in part whose cut back failed leaves this shape): the
 *         append cuts the md2 back to its whole rows first, with the same
 *         warning, and writes its row there. Before the fix the row went
 *         at offset 141, where no read finds it: the list said load_failed,
 *         and the cut of the next restart removed bytes of that
 *         acknowledged row.
 *      5. a md2 flagged unreadable (mode 000) that also ends torn: the
 *         append that finds it readable again counts it, cuts it back,
 *         unflags it, and writes its row as the file's next row.
 *      6. a delete of the key that cannot remove the key directory reads
 *         the key again from the disk: the master cuts a torn md2 there too.
 *      7. a REPLICA with a realtime disk feed on a torn store: a master
 *         opens it (cut) and appends two rows; the feed hands each new row
 *         once, and the two agree on the rows.
 *      8. a REPLICA feed that sees a row half written at a notification
 *         (first a row of a file it counts, then the first row of a new
 *         file, which has no cell yet): nothing is handed, nothing is
 *         logged, and each row is handed once when the next notification
 *         comes.
 *      9. a REPLICA opens a md2 of 13 bytes only: it has no whole row, so
 *         it is a md2 of 0 rows beside a content that is not empty, and
 *         the replica logs the warning of case 3. It does not cut.
 *
 *      Cases 5 and 6 need a mode that stops a read or a remove: they are
 *      SKIPPED, and say so, when the test runs as root.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP         "test_torn_md2_tail"
#define DATABASE    "tr_torn_md2_tail"
#define TOPIC_NAME  "topic_torn"
#define DAY1        946684800   // 2000-01-01
#define DAY         86400
#define ROW         32          // sizeof(md2_record_t)
#define TORN        13          // the bytes of the torn row

#define MSG_CUT         "md2 file of the key ends in a part of a row: an append that was never acknowledged was cut back"
#define MSG_NO_ROWS     "md2 file of the key with no rows and a content file that is not empty: an append that was never acknowledged, the file is ignored"
#define MSG_FLAG        "md2 file of the key unreadable when its cache was built: every load of the key says load_failed"
#define MSG_UNFLAG      "md2 file of the key readable again: it is counted, and the key is not flagged for it"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
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
    if(!record) {
        snprintf(got + ln, sizeof(got) - ln, "%s%s@NULL", ln? " ": "", key);
    } else {
        snprintf(got + ln, sizeof(got) - ln, "%s%s@%d", ln? " ": "", key,
            (int)kw_get_int(0, record, "v", 0, 0));
    }
    JSON_DECREF(record)
    return 0;
}

PRIVATE json_t *startup(BOOL master)
{
    return tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i, s:s}",
        "path", path_root,
        "database", DATABASE,
        "master", master,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y-%m-%d"
    ), 0);
}

/*
 *  With the event loop: a replica's realtime disk feed needs it
 */
PRIVATE json_t *startup_rt(BOOL master)
{
    return tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i, s:s}",
        "path", path_root,
        "database", DATABASE,
        "master", master,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y-%m-%d"
    ), yev_loop);
}

PRIVATE void drain(void)
{
    for(int i = 0; i < 30; i++) {
        yev_loop_run_once(yev_loop);
    }
}

PRIVATE json_t *create_topic(json_t *tranger)
{
    return tranger2_create_topic(
        tranger, TOPIC_NAME, "id", "tm", NULL, sf_string_key,
        json_pack("{s:s, s:I, s:I}", "id", "", "tm", (json_int_t)0, "v", (json_int_t)0),
        0
    );
}

PRIVATE int append(json_t *tranger, const char *key, int day, int v)
{
    md2_record_ex_t md = {0};
    return tranger2_append_record(tranger, TOPIC_NAME, (uint64_t)(DAY1 + day*DAY + v), 0, &md,
        json_pack("{s:s, s:I, s:i}", "id", key, "tm", (json_int_t)(DAY1 + day*DAY + v), "v", v)
    );
}

/*
 *  A store with A: row 1 in the file of day 0 and rows 2, 3, 4 in the
 *  file of day 1, and B: row 1
 */
PRIVATE int build_store(void)
{
    rmrdir(path_database);
    set_expected_results("torn md2 tail: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup(TRUE);
    if(!tranger || !create_topic(tranger)) {
        printf("%sERROR%s --> cannot create the store\n", On_Red BWhite, Color_Off);
        return -1;
    }
    append(tranger, "A", 0, 1);
    append(tranger, "A", 1, 2);
    append(tranger, "A", 1, 3);
    append(tranger, "A", 1, 4);
    append(tranger, "B", 0, 1);
    tranger2_shutdown(tranger);
    test_json(NULL);    // the setup logs are not what is tested
    return 0;
}

PRIVATE void file_of_a(char *bf, size_t bfsize, const char *day, const char *ext)
{
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "%s.%s", day, ext);
    build_path(bf, bfsize, path_database, TOPIC_NAME, "keys", "A", name, NULL);
}

/*
 *  The md2 of A's file of `day` with `size` bytes, as a power cut leaves
 *  it, with no tranger running
 */
PRIVATE int tear_md2(const char *day, off_t size)
{
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), day, "md2");
    if(truncate(path, size) < 0) {
        printf("%sERROR%s --> cannot tear %s\n", On_Red BWhite, Color_Off, path);
        return -1;
    }
    return 0;
}

PRIVATE int chmod_md2(const char *day, mode_t mode)
{
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), day, "md2");
    if(chmod(path, mode) < 0) {
        printf("%sERROR%s --> cannot chmod %s\n", On_Red BWhite, Color_Off, path);
        return -1;
    }
    return 0;
}

PRIVATE off_t md2_size(const char *day)
{
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), day, "md2");
    return filesize(path);
}

PRIVATE int expect(const char *what, const char *found, const char *expected)
{
    if(strcmp(found, expected) != 0) {
        printf("%sERROR%s --> %s: [%s], expected [%s]\n",
            On_Red BWhite, Color_Off, what, found, expected);
        return -1;
    }
    return 0;
}

PRIVATE int expect_size(const char *what, off_t found, off_t expected)
{
    if(found != expected) {
        printf("%sERROR%s --> %s: md2 size %ld, expected %ld\n",
            On_Red BWhite, Color_Off, what, (long)found, (long)expected);
        return -1;
    }
    return 0;
}

/*
 *  The keyless list forward: what it hands, and that it is whole
 */
PRIVATE int check_list(json_t *tranger, const char *what, const char *expected)
{
    int result = 0;
    got[0] = 0;
    json_t *match_cond = json_pack("{s:I, s:b, s:I}",
        "to_rowid", (json_int_t)1000000,   // no realtime
        "backward", 0,
        "load_record_callback", (json_int_t)(uintptr_t)on_record
    );
    json_t *list = tranger2_open_list(tranger, TOPIC_NAME, match_cond, json_object(), "", FALSE, "");
    if(!list) {
        printf("%sERROR%s --> %s: list REFUSED\n", On_Red BWhite, Color_Off, what);
        return -1;
    }
    result += expect(what, got, expected);
    if(json_is_true(json_object_get(list, "load_failed"))) {
        printf("%sERROR%s --> %s: load_failed\n", On_Red BWhite, Color_Off, what);
        result += -1;
    }
    tranger2_close_list(tranger, list);
    return result;
}

PRIVATE int check_rows(json_t *tranger, const char *what, int expected)
{
    json_t *range = tranger2_topic_key_range(tranger, TOPIC_NAME, "A");
    int rows = (int)kw_get_int(0, range, "rows", 0, 0);
    JSON_DECREF(range)
    if(rows != expected) {
        printf("%sERROR%s --> %s: A has %d rows, expected %d\n",
            On_Red BWhite, Color_Off, what, rows, expected);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  1. A master cuts a torn tail back
 ***************************************************************************/
PRIVATE int test_master_cuts(void)
{
    int result = 0;
    if(build_store() < 0 || tear_md2("2000-01-02", 3*ROW + TORN) < 0) {
        return -1;
    }

    set_expected_results("1. a master opens a torn md2: one warning",
        json_pack("[{s:s, s:s, s:s, s:i, s:i}]",
            "msg", MSG_CUT,
            "key", "A",
            "file_id", "2000-01-02",
            "old_size", 3*ROW + TORN,
            "new_size", 3*ROW
        ), NULL, NULL, 1
    );
    json_t *tranger = startup(TRUE);
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> 1: cannot open the store\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += test_json(NULL);
    result += expect_size("1. after the open", md2_size("2000-01-02"), 3*ROW);

    set_expected_results("1. the key loads whole, the next append goes in", NULL, NULL, NULL, 1);
    result += check_list(tranger, "1. the list after the cut", "A@1 A@2 A@3 A@4 B@1");
    result += check_rows(tranger, "1. the range after the cut", 4);
    if(append(tranger, "A", 1, 5) < 0) {
        printf("%sERROR%s --> 1: the append into the cut file was refused\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_size("1. after the append", md2_size("2000-01-02"), 4*ROW);
    result += check_list(tranger, "1. the list after the append", "A@1 A@2 A@3 A@4 A@5 B@1");
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    set_expected_results("1. the restart: nothing to say", NULL, NULL, NULL, 1);
    tranger = startup(TRUE);
    tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    result += check_list(tranger, "1. the list after the restart", "A@1 A@2 A@3 A@4 A@5 B@1");
    result += check_rows(tranger, "1. the range after the restart", 5);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  2. A replica reads the whole rows and writes nothing
 ***************************************************************************/
PRIVATE int test_replica_reads(void)
{
    int result = 0;
    if(build_store() < 0 || tear_md2("2000-01-02", 3*ROW + TORN) < 0) {
        return -1;
    }

    set_expected_results("2. a replica opens a torn md2: nothing to say", NULL, NULL, NULL, 1);
    json_t *tranger = startup(FALSE);
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> 2: cannot open the store as a replica\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += expect_size("2. the replica writes nothing", md2_size("2000-01-02"), 3*ROW + TORN);
    result += check_list(tranger, "2. the replica's list", "A@1 A@2 A@3 A@4 B@1");
    result += check_rows(tranger, "2. the replica's range", 4);
    tranger2_shutdown(tranger);
    result += expect_size("2. after the replica", md2_size("2000-01-02"), 3*ROW + TORN);
    result += test_json(NULL);

    set_expected_results("2. then a master cuts it back",
        json_pack("[{s:s}]", "msg", MSG_CUT), NULL, NULL, 1
    );
    tranger = startup(TRUE);
    tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    result += expect_size("2. after the master", md2_size("2000-01-02"), 3*ROW);
    result += check_list(tranger, "2. the master's list", "A@1 A@2 A@3 A@4 B@1");
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  3. The first row torn: the md2 is cut to 0 bytes, and ignored
 ***************************************************************************/
PRIVATE int test_first_row_torn(void)
{
    int result = 0;
    if(build_store() < 0 || tear_md2("2000-01-02", TORN) < 0) {
        return -1;
    }

    set_expected_results("3. the only row torn: cut to 0 bytes, then ignored",
        json_pack("[{s:s, s:s, s:i, s:i},{s:s, s:s}]",
            "msg", MSG_CUT,
            "file_id", "2000-01-02",
            "old_size", TORN,
            "new_size", 0,
            "msg", MSG_NO_ROWS,
            "file_id", "2000-01-02"
        ), NULL, NULL, 1
    );
    json_t *tranger = startup(TRUE);
    tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    result += test_json(NULL);
    result += expect_size("3. after the open", md2_size("2000-01-02"), 0);

    set_expected_results("3. the next append is the file's row 1", NULL, NULL, NULL, 1);
    result += check_list(tranger, "3. the list after the cut", "A@1 B@1");
    if(append(tranger, "A", 1, 6) < 0) {
        printf("%sERROR%s --> 3: the append into the cut file was refused\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_size("3. after the append", md2_size("2000-01-02"), ROW);
    result += check_list(tranger, "3. the list after the append", "A@1 A@6 B@1");
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  4. A running master finds its md2 torn at the next append
 ***************************************************************************/
PRIVATE int test_running_master_append(void)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }

    set_expected_results("4. a running master appends", NULL, NULL, NULL, 1);
    json_t *tranger = startup(TRUE);
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> 4: cannot open the store\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    if(append(tranger, "A", 1, 5) < 0) {
        printf("%sERROR%s --> 4: the first append was refused\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  A row written in part whose cut back failed leaves the md2 of the
     *  running master torn, with its write fd open
     */
    if(tear_md2("2000-01-02", 4*ROW + TORN) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }

    set_expected_results("4. the next append cuts the torn row back first",
        json_pack("[{s:s, s:s, s:s, s:i, s:i}]",
            "msg", MSG_CUT,
            "key", "A",
            "file_id", "2000-01-02",
            "old_size", 4*ROW + TORN,
            "new_size", 4*ROW
        ), NULL, NULL, 1
    );
    if(append(tranger, "A", 1, 6) < 0) {
        printf("%sERROR%s --> 4: the append after the torn row was refused\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("4. the row is where a read finds it", NULL, NULL, NULL, 1);
    result += expect_size("4. after the append", md2_size("2000-01-02"), 5*ROW);
    result += check_list(tranger, "4. the list after the append", "A@1 A@2 A@3 A@4 A@5 A@6 B@1");
    result += check_rows(tranger, "4. the range after the append", 6);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    set_expected_results("4. the restart: nothing to cut", NULL, NULL, NULL, 1);
    tranger = startup(TRUE);
    tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    result += expect_size("4. after the restart", md2_size("2000-01-02"), 5*ROW);
    result += check_list(tranger, "4. the list after the restart", "A@1 A@2 A@3 A@4 A@5 A@6 B@1");
    result += check_rows(tranger, "4. the range after the restart", 6);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  5. A flagged file that ends torn: the append counts it again
 ***************************************************************************/
PRIVATE int test_flagged_and_torn(void)
{
    int result = 0;
    if(geteuid() == 0) {
        printf("5. SKIPPED: running as root, a md2 of mode 000 is still read\n");
        return 0;
    }
    if(build_store() < 0 || tear_md2("2000-01-02", 3*ROW + TORN) < 0 ||
            chmod_md2("2000-01-02", 0) < 0) {
        return -1;
    }

    set_expected_results("5. a md2 that cannot be read is flagged",
        json_pack("[{s:s},{s:s, s:s, s:s}]",
            "msg", "Cannot open md2 file",
            "msg", MSG_FLAG,
            "key", "A",
            "file_id", "2000-01-02"
        ), NULL, NULL, 1
    );
    json_t *tranger = startup(TRUE);
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> 5: cannot open the store\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += test_json(NULL);

    if(chmod_md2("2000-01-02", 0660) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }
    set_expected_results("5. the append counts it again: cut back, unflagged",
        json_pack("[{s:s, s:s, s:i, s:i},{s:s, s:s, s:s}]",
            "msg", MSG_CUT,
            "file_id", "2000-01-02",
            "old_size", 3*ROW + TORN,
            "new_size", 3*ROW,
            "msg", MSG_UNFLAG,
            "key", "A",
            "file_id", "2000-01-02"
        ), NULL, NULL, 1
    );
    if(append(tranger, "A", 1, 5) < 0) {
        printf("%sERROR%s --> 5: the append into the file was refused\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("5. the row is the file's next row", NULL, NULL, NULL, 1);
    result += expect_size("5. after the append", md2_size("2000-01-02"), 4*ROW);
    result += check_list(tranger, "5. the list after the append", "A@1 A@2 A@3 A@4 A@5 B@1");
    result += check_rows(tranger, "5. the range after the append", 5);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  6. A delete that cannot remove the key reads it again: the cut
 ***************************************************************************/
PRIVATE int test_delete_key_reload(void)
{
    int result = 0;
    if(geteuid() == 0) {
        printf("6. SKIPPED: running as root, a directory of mode 0500 does not stop a remove\n");
        return 0;
    }
    if(build_store() < 0) {
        return -1;
    }

    set_expected_results("6. a running master", NULL, NULL, NULL, 1);
    json_t *tranger = startup(TRUE);
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> 6: cannot open the store\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += test_json(NULL);

    char key_dir[PATH_MAX];
    build_path(key_dir, sizeof(key_dir), path_database, TOPIC_NAME, "keys", "A", NULL);
    struct stat st;
    if(tear_md2("2000-01-02", 3*ROW + TORN) < 0 || stat(key_dir, &st) < 0 ||
            chmod(key_dir, 0500) < 0) {
        printf("%sERROR%s --> 6: cannot prepare %s\n", On_Red BWhite, Color_Off, key_dir);
        tranger2_shutdown(tranger);
        return -1;
    }

    set_expected_results("6. the delete fails, the key is read again and cut back",
        json_pack("[{s:s},{s:s},{s:s, s:s, s:s, s:i, s:i}]",
            "msg", "remove() FAILED",
            "msg", "Cannot delete subdir key. rmrdir() FAILED",
            "msg", MSG_CUT,
            "key", "A",
            "file_id", "2000-01-02",
            "old_size", 3*ROW + TORN,
            "new_size", 3*ROW
        ), NULL, NULL, 1
    );
    if(tranger2_delete_key(tranger, TOPIC_NAME, "A") == 0) {
        printf("%sERROR%s --> 6: the delete answered done\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);
    chmod(key_dir, st.st_mode & 07777);

    set_expected_results("6. the key is whole", NULL, NULL, NULL, 1);
    result += expect_size("6. after the delete", md2_size("2000-01-02"), 3*ROW);
    result += check_list(tranger, "6. the list after the delete", "A@1 A@2 A@3 A@4 B@1");
    result += check_rows(tranger, "6. the range after the delete", 4);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  7. A replica feed on a torn store: the master cuts and appends
 ***************************************************************************/
PRIVATE int test_replica_feed_converges(void)
{
    int result = 0;
    if(build_store() < 0 || tear_md2("2000-01-02", 3*ROW + TORN) < 0) {
        return -1;
    }

    set_expected_results("7. a replica and its feed open a torn store", NULL, NULL, NULL, 1);
    json_t *replica = startup_rt(FALSE);
    if(!replica || !tranger2_open_topic(replica, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> 7: cannot open the store as a replica\n", On_Red BWhite, Color_Off);
        if(replica) {
            tranger2_shutdown(replica);
        }
        test_json(NULL);
        return -1;
    }
    got[0] = 0;
    json_t *rt = tranger2_open_rt_disk(
        replica, TOPIC_NAME, "A", NULL, on_record, "rt_torn7", "", NULL
    );
    if(!rt) {
        printf("%sERROR%s --> 7: cannot open the feed\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += check_rows(replica, "7. the replica's range", 4);
    result += test_json(NULL);

    set_expected_results("7. a master opens it: the cut",
        json_pack("[{s:s, s:s, s:i, s:i}]",
            "msg", MSG_CUT,
            "file_id", "2000-01-02",
            "old_size", 3*ROW + TORN,
            "new_size", 3*ROW
        ), NULL, NULL, 1
    );
    json_t *master = startup_rt(TRUE);
    tranger2_open_topic(master, TOPIC_NAME, FALSE);
    drain();
    result += test_json(NULL);
    result += expect("7. the cut hands nothing", got, "");

    set_expected_results("7. two appends: each is handed once", NULL, NULL, NULL, 1);
    append(master, "A", 1, 5);
    drain();
    append(master, "A", 1, 6);
    drain();
    result += expect("7. the replica's feed", got, "A@5 A@6");
    result += check_rows(replica, "7. the replica's range after the appends", 6);
    result += check_rows(master, "7. the master's range after the appends", 6);
    if(rt) {
        tranger2_close_rt_disk(replica, rt);
    }
    tranger2_shutdown(replica);
    tranger2_shutdown(master);
    drain();
    result += test_json(NULL);

    return result;
}

/*
 *  Tear the md2 of A's file of `day` to `size` bytes and keep the bytes
 *  cut, to write them back as the master finishing its row
 */
PRIVATE int tear_and_keep(const char *day, off_t size, char *kept, size_t kept_size)
{
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), day, "md2");
    int fd = open(path, O_RDONLY|O_CLOEXEC);
    if(fd < 0 || pread(fd, kept, kept_size, size) != (ssize_t)kept_size) {
        printf("%sERROR%s --> cannot keep the tail of %s\n", On_Red BWhite, Color_Off, path);
        if(fd >= 0) {
            close(fd);
        }
        return -1;
    }
    close(fd);
    return tear_md2(day, size);
}

PRIVATE int finish_row(const char *day, off_t offset, const char *kept, size_t kept_size)
{
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), day, "md2");
    int fd = open(path, O_WRONLY|O_CLOEXEC);
    if(fd < 0 || pwrite(fd, kept, kept_size, offset) != (ssize_t)kept_size) {
        printf("%sERROR%s --> cannot finish the row of %s\n", On_Red BWhite, Color_Off, path);
        if(fd >= 0) {
            close(fd);
        }
        return -1;
    }
    close(fd);
    return 0;
}

/***************************************************************************
 *  8. A replica feed sees a row half written at a notification
 ***************************************************************************/
PRIVATE int test_replica_feed_half_written(void)
{
    int result = 0;
    char kept[ROW];
    if(build_store() < 0) {
        return -1;
    }

    set_expected_results("8. a master, a replica and its feed", NULL, NULL, NULL, 1);
    json_t *master = startup_rt(TRUE);
    json_t *replica = startup_rt(FALSE);
    if(!master || !replica ||
            !tranger2_open_topic(master, TOPIC_NAME, FALSE) ||
            !tranger2_open_topic(replica, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> 8: cannot open the store\n", On_Red BWhite, Color_Off);
        if(replica) {
            tranger2_shutdown(replica);
        }
        if(master) {
            tranger2_shutdown(master);
        }
        test_json(NULL);
        return -1;
    }
    json_t *rt = tranger2_open_rt_disk(
        replica, TOPIC_NAME, "A", NULL, on_record, "rt_torn8", "", NULL
    );
    if(!rt) {
        printf("%sERROR%s --> 8: cannot open the feed\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    drain();
    got[0] = 0;
    result += test_json(NULL);

    set_expected_results("8. half written rows: nothing handed, nothing said", NULL, NULL, NULL, 1);

    /*
     *  A row of a file the replica counts
     */
    append(master, "A", 1, 5);
    if(tear_and_keep("2000-01-02", 3*ROW + TORN, kept, ROW - TORN) < 0) {
        result += -1;
    }
    drain();
    result += expect("8. a torn row of a counted file is not handed", got, "");
    result += check_rows(replica, "8. the replica's range with a torn row", 4);
    result += finish_row("2000-01-02", 3*ROW + TORN, kept, ROW - TORN);
    append(master, "A", 1, 6);
    drain();
    result += expect("8. the rows of the counted file, each once", got, "A@5 A@6");

    /*
     *  The first row of a new file: no cell yet
     */
    append(master, "A", 2, 7);
    if(tear_and_keep("2000-01-03", TORN, kept, ROW - TORN) < 0) {
        result += -1;
    }
    drain();
    result += expect("8. a torn first row of a new file is not handed", got, "A@5 A@6");
    result += check_rows(replica, "8. the replica's range with a torn new file", 6);
    result += finish_row("2000-01-03", TORN, kept, ROW - TORN);
    append(master, "A", 2, 8);
    drain();
    result += expect("8. the rows of the new file, each once", got, "A@5 A@6 A@7 A@8");
    result += check_rows(replica, "8. the replica's range at the end", 8);
    result += check_rows(master, "8. the master's range at the end", 8);

    if(rt) {
        tranger2_close_rt_disk(replica, rt);
    }
    tranger2_shutdown(replica);
    tranger2_shutdown(master);
    drain();
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  9. A replica opens a md2 whose only row is torn
 ***************************************************************************/
PRIVATE int test_replica_only_row_torn(void)
{
    int result = 0;
    if(build_store() < 0 || tear_md2("2000-01-02", TORN) < 0) {
        return -1;
    }

    set_expected_results("9. a replica and a md2 with no whole row: the 0-rows warning",
        json_pack("[{s:s, s:s, s:s}]",
            "msg", MSG_NO_ROWS,
            "key", "A",
            "file_id", "2000-01-02"
        ), NULL, NULL, 1
    );
    json_t *tranger = startup(FALSE);
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> 9: cannot open the store as a replica\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += test_json(NULL);

    set_expected_results("9. the replica reads the other files, and writes nothing", NULL, NULL, NULL, 1);
    result += expect_size("9. the replica does not cut", md2_size("2000-01-02"), TORN);
    result += check_list(tranger, "9. the replica's list", "A@1 B@1");
    result += check_rows(tranger, "9. the replica's range", 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);

    result += test_master_cuts();
    result += test_replica_reads();
    result += test_first_row_torn();
    result += test_running_master_append();
    result += test_flagged_and_torn();
    result += test_delete_key_reload();
    result += test_replica_feed_converges();
    result += test_replica_feed_half_written();
    result += test_replica_only_row_torn();

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
