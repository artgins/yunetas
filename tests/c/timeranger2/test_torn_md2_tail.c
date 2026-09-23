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
 *         warning, and writes its row there. Up to 7.25.4 the row went
 *         after the torn bytes and the whole file was left out of the key.
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
 *     10. a md2 as 7.25.4 left it after a torn row: 3 whole rows, 13 bytes
 *         of a row whose append was refused, and 2 rows acknowledged after
 *         them, on no row boundary (7.25.4 wrote them at the end of the
 *         file). Its last 32 bytes are a whole row that ends the content
 *         file. A MASTER does not cut it (the cut would remove the last 13
 *         bytes of an acknowledged row): a CRITICAL names the shape, the
 *         file is flagged, the load says load_failed, an append into the
 *         file is refused, and the md2 bytes do not change.
 *     11. a REPLICA opens the same shape: the same CRITICAL, the file is
 *         flagged, and it does not read the rows on the row boundaries.
 *     12. a torn md2 whose last whole row is not a valid row (its content
 *         goes past the end of the content file): the tail is not an
 *         append that was never acknowledged after a good row. It is not
 *         cut: a CRITICAL, and the file is flagged.
 *     13. a RUNNING master finds the shape of case 10 at the next append:
 *         the append is refused, its content is cut back, the md2 bytes do
 *         not change, and the file is flagged in memory, as the open flags
 *         it.
 *     14. the shape of case 10, for every size of the torn part (1 to 31
 *         bytes), in a topic with no tkey and records of 256 bytes, with
 *         and without content after the last row in the content file (an
 *         append killed between its two writes; 7.25.4 did not cut it
 *         back). With __tm__ 0 and every __offset__ a multiple of 256, a
 *         row moved by 1 byte names a range INSIDE the content file, and
 *         a check of the range only cut such a file by 1 byte. A master
 *         does not cut it: the last 32 bytes are a row whose content is a
 *         whole record.
 *     15. the same shape, with content after the last row, at the append
 *         of a running master: refused, not cut, and the file flagged in
 *         memory -- the list of the same process says load_failed, and
 *         the next append into the file is refused as a flagged one.
 *     16. a torn row, its content and more content after it, for every
 *         size of the torn part: the master cuts it back, the key loads.
 *     17. a torn md2 whose last whole row names a range inside the content
 *         file that is not a record: not cut, flagged.
 *     18. a torn md2 whose last whole row names content before the content
 *         of the row before it: not cut, flagged.
 *     19. a torn md2 whose last whole row is a deleted instance with its
 *         content zeroed: a good row, and the master cuts the tail.
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
#include <endian.h>

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
#define TOPIC_NT    "topic_torn_notkey"
#define JUNK        50          // bytes of content after the last row

#define MSG_CUT         "md2 file of the key ends in a part of a row: an append that was never acknowledged was cut back"
#define MSG_NO_ROWS     "md2 file of the key with no rows and a content file that is not empty: an append that was never acknowledged, the file is ignored"
#define MSG_FLAG        "md2 file of the key unreadable when its cache was built: every load of the key says load_failed"
#define MSG_UNFLAG      "md2 file of the key readable again: it is counted, and the key is not flagged for it"
#define MSG_725         "md2 file of the key ends in a whole row that is not on a row boundary: written by 7.25.4 after a torn row; not cut, repair it by hand"
#define MSG_LAST_BAD    "md2 file of the key ends in a part of a row after a last whole row that is not valid: not cut, repair it by hand"
#define MSG_APPEND_NOT_CUT "Cannot append record, its md2 file ends in a part of a row that must not be cut back: the append is refused"
#define MSG_FLAG_APPEND "md2 file of the key flagged unreadable at an append: every load of the key says load_failed"
#define MSG_APPEND_FLAGGED "Cannot append record, its file is flagged unreadable: its row would follow rows no cell counts"
#define MSG_ITER        "The history of the key is not whole: a md2 file of it could not be read when its cache was built"
#define MSG_LIST        "Cannot load the whole history of a key of the list: the records read before the failure were handed, the list goes on with the next key"

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

/*
 *  The keyless list forward of a store with A's file of day 1 flagged: it
 *  stops at the file, and says load_failed
 */
PRIVATE int check_list_flagged(json_t *tranger, const char *what)
{
    int result = 0;
    got[0] = 0;
    set_expected_results(what,
        json_pack("[{s:s},{s:s}]",
            "msg", MSG_ITER,
            "msg", MSG_LIST
        ), NULL, NULL, 1
    );
    json_t *match_cond = json_pack("{s:I, s:b, s:I}",
        "to_rowid", (json_int_t)1000000,   // no realtime
        "backward", 0,
        "load_record_callback", (json_int_t)(uintptr_t)on_record
    );
    json_t *list = tranger2_open_list(tranger, TOPIC_NAME, match_cond, json_object(), "", FALSE, "");
    if(!list) {
        printf("%sERROR%s --> %s: list REFUSED\n", On_Red BWhite, Color_Off, what);
        test_json(NULL);
        return -1;
    }
    result += expect(what, got, "A@1 B@1");
    if(!json_is_true(json_object_get(list, "load_failed"))) {
        printf("%sERROR%s --> %s: no load_failed\n", On_Red BWhite, Color_Off, what);
        result += -1;
    }
    tranger2_close_list(tranger, list);
    result += test_json(NULL);
    return result;
}

/*
 *  The whole md2 of A's file of `day`, to compare it after
 */
PRIVATE ssize_t read_md2(const char *day, char *bf, size_t bfsize)
{
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), day, "md2");
    int fd = open(path, O_RDONLY|O_CLOEXEC);
    if(fd < 0) {
        printf("%sERROR%s --> cannot open %s\n", On_Red BWhite, Color_Off, path);
        return -1;
    }
    ssize_t ln = pread(fd, bf, bfsize, 0);
    close(fd);
    return ln;
}

PRIVATE int expect_md2_unchanged(const char *what, const char *day, const char *before, ssize_t before_ln)
{
    char now[16*ROW];
    ssize_t ln = read_md2(day, now, sizeof(now));
    if(ln != before_ln || memcmp(now, before, (size_t)ln) != 0) {
        printf("%sERROR%s --> %s: the md2 changed, %ld bytes, expected %ld\n",
            On_Red BWhite, Color_Off, what, (long)ln, (long)before_ln);
        return -1;
    }
    return 0;
}

/*
 *  The md2 of A's file of day 1 with 6 rows (v 2..7), rewritten as 7.25.4
 *  left it: rows 1-3, 13 bytes of row 4 (an append refused after a torn
 *  write), then rows 5 and 6, acknowledged, at 109 and 141. The content
 *  file keeps the 6 records. `saved` gets the md2 as written.
 */
PRIVATE ssize_t make_725_shape(char *saved, size_t saved_size)
{
    char rows[6*ROW];
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), "2000-01-02", "md2");
    if(read_md2("2000-01-02", rows, sizeof(rows)) != (ssize_t)sizeof(rows)) {
        printf("%sERROR%s --> the md2 has not 6 rows\n", On_Red BWhite, Color_Off);
        return -1;
    }
    int fd = open(path, O_WRONLY|O_CLOEXEC);
    if(fd < 0 ||
            ftruncate(fd, 0) < 0 ||
            pwrite(fd, rows, 3*ROW, 0) != 3*ROW ||
            pwrite(fd, rows + 3*ROW, TORN, 3*ROW) != TORN ||
            pwrite(fd, rows + 4*ROW, 2*ROW, 3*ROW + TORN) != 2*ROW) {
        printf("%sERROR%s --> cannot rewrite %s\n", On_Red BWhite, Color_Off, path);
        if(fd >= 0) {
            close(fd);
        }
        return -1;
    }
    close(fd);
    return read_md2("2000-01-02", saved, saved_size);
}

PRIVATE off_t content_size_of_a(const char *day)
{
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), day, "json");
    return filesize(path);
}

/*
 *  build_store(), then rows v 5, 6, 7 in A's file of day 1
 */
PRIVATE int build_store_6_rows(void)
{
    if(build_store() < 0) {
        return -1;
    }
    set_expected_results("6 rows: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup(TRUE);
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> cannot open the store\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    append(tranger, "A", 1, 5);
    append(tranger, "A", 1, 6);
    append(tranger, "A", 1, 7);
    tranger2_shutdown(tranger);
    test_json(NULL);    // the setup logs are not what is tested
    return 0;
}

/***************************************************************************
 *  10, 11. The shape 7.25.4 left: a master and a replica do not cut it
 ***************************************************************************/
PRIVATE int test_725_shape(BOOL master)
{
    int result = 0;
    const char *case_name = master? "10. a master": "11. a replica";
    char test[256];
    char before[16*ROW];
    if(build_store_6_rows() < 0) {
        return -1;
    }
    ssize_t before_ln = make_725_shape(before, sizeof(before));
    if(before_ln != 3*ROW + TORN + 2*ROW) {
        return -1;
    }
    off_t content_size = content_size_of_a("2000-01-02");

    snprintf(test, sizeof(test), "%s opens the shape 7.25.4 left: not cut, flagged", case_name);
    set_expected_results(test,
        json_pack("[{s:s, s:s, s:s, s:I, s:I, s:I},{s:s, s:s, s:s}]",
            "msg", MSG_725,
            "key", "A",
            "file_id", "2000-01-02",
            "md2_size", (json_int_t)(3*ROW + TORN + 2*ROW),
            "content_size", (json_int_t)content_size,
            "row_at", (json_int_t)(3*ROW + TORN + ROW),
            "msg", MSG_FLAG,
            "key", "A",
            "file_id", "2000-01-02"
        ), NULL, NULL, 1
    );
    json_t *tranger = startup(master);
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> %s: cannot open the store\n", On_Red BWhite, Color_Off, case_name);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += test_json(NULL);
    snprintf(test, sizeof(test), "%s: after the open", case_name);
    result += expect_md2_unchanged(test, "2000-01-02", before, before_ln);

    snprintf(test, sizeof(test), "%s: the list", case_name);
    result += check_list_flagged(tranger, test);

    if(master) {
        snprintf(test, sizeof(test), "%s: an append into the file is refused", case_name);
        set_expected_results(test,
            json_pack("[{s:s},{s:s, s:s, s:s}]",
                "msg", MSG_725,
                "msg", MSG_APPEND_FLAGGED,
                "key", "A",
                "file_id", "2000-01-02"
            ), NULL, NULL, 1
        );
        if(append(tranger, "A", 1, 8) == 0) {
            printf("%sERROR%s --> %s: the append was taken\n", On_Red BWhite, Color_Off, case_name);
            result += -1;
        }
        result += test_json(NULL);
        snprintf(test, sizeof(test), "%s: after the append", case_name);
        result += expect_md2_unchanged(test, "2000-01-02", before, before_ln);
    }

    set_expected_results("the shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    snprintf(test, sizeof(test), "%s: after the shutdown", case_name);
    result += expect_md2_unchanged(test, "2000-01-02", before, before_ln);

    return result;
}

/***************************************************************************
 *  12. A torn md2 whose last whole row is not valid: not cut
 ***************************************************************************/
PRIVATE int test_last_row_not_valid(void)
{
    int result = 0;
    char before[16*ROW];
    if(build_store() < 0 || tear_md2("2000-01-02", 3*ROW + TORN) < 0) {
        return -1;
    }
    /*
     *  The content of the last whole row goes past the end of the content
     */
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), "2000-01-02", "json");
    off_t content_size = filesize(path) - 5;
    if(truncate(path, content_size) < 0) {
        printf("%sERROR%s --> 12: cannot cut %s\n", On_Red BWhite, Color_Off, path);
        return -1;
    }
    ssize_t before_ln = read_md2("2000-01-02", before, sizeof(before));

    set_expected_results("12. a master: the last whole row is not valid, not cut, flagged",
        json_pack("[{s:s, s:s, s:s, s:I, s:I, s:I},{s:s, s:s, s:s}]",
            "msg", MSG_LAST_BAD,
            "key", "A",
            "file_id", "2000-01-02",
            "md2_size", (json_int_t)(3*ROW + TORN),
            "content_size", (json_int_t)content_size,
            "row_at", (json_int_t)(2*ROW),
            "msg", MSG_FLAG,
            "key", "A",
            "file_id", "2000-01-02"
        ), NULL, NULL, 1
    );
    json_t *tranger = startup(TRUE);
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> 12: cannot open the store\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += test_json(NULL);
    result += expect_md2_unchanged("12. after the open", "2000-01-02", before, before_ln);
    result += check_list_flagged(tranger, "12. the list");

    set_expected_results("12. the shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  13. A running master finds the shape 7.25.4 left at the next append
 ***************************************************************************/
PRIVATE int test_725_shape_at_append(void)
{
    int result = 0;
    char before[16*ROW];
    if(build_store_6_rows() < 0) {
        return -1;
    }

    set_expected_results("13. a running master", NULL, NULL, NULL, 1);
    json_t *tranger = startup(TRUE);
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> 13: cannot open the store\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += check_rows(tranger, "13. the range", 7);
    result += test_json(NULL);

    ssize_t before_ln = make_725_shape(before, sizeof(before));
    if(before_ln != 3*ROW + TORN + 2*ROW) {
        tranger2_shutdown(tranger);
        return -1;
    }
    off_t content_size = content_size_of_a("2000-01-02");

    set_expected_results("13. the append is refused, the md2 is not cut, the file is flagged",
        json_pack("[{s:s, s:s, s:s, s:I, s:I},{s:s, s:s, s:s},{s:s, s:s, s:s, s:I}]",
            "msg", MSG_725,
            "key", "A",
            "file_id", "2000-01-02",
            "md2_size", (json_int_t)before_ln,
            "content_size", (json_int_t)content_size,
            "msg", MSG_FLAG_APPEND,
            "key", "A",
            "file_id", "2000-01-02",
            "msg", MSG_APPEND_NOT_CUT,
            "key", "A",
            "file_id", "2000-01-02",
            "md2_size", (json_int_t)before_ln
        ), NULL, NULL, 1
    );
    if(append(tranger, "A", 1, 8) == 0) {
        printf("%sERROR%s --> 13: the append was taken\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);
    result += expect_md2_unchanged("13. after the append", "2000-01-02", before, before_ln);
    if(content_size_of_a("2000-01-02") != content_size) {
        printf("%sERROR%s --> 13: the content of the refused append was not cut back\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    set_expected_results("13. the shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  The store of cases 14 to 16: a topic with no tkey (its __tm__ is 0, as
 *  in a treedb topic) and records of 256 bytes each, so every __offset__
 *  is a multiple of 256. Key A: rows v 1..6 in the file of day 0.
 ***************************************************************************/
PRIVATE json_t *record_256(int v)
{
    json_t *record = json_pack("{s:s, s:i, s:s}", "id", "A", "v", v, "pad", "");
    char *s = json_dumps(record, JSON_COMPACT|JSON_ENCODE_ANY);
    size_t base = strlen(s);
    gbmem_free(s);
    char pad[256];
    memset(pad, 'x', sizeof(pad));
    pad[255 - base] = 0;        // 255 bytes of text and the NUL: 256
    json_object_set_new(record, "pad", json_string(pad));
    return record;
}

PRIVATE int append_nt(json_t *tranger, int v)
{
    md2_record_ex_t md = {0};
    return tranger2_append_record(tranger, TOPIC_NT, (uint64_t)(DAY1 + v), 0, &md,
        record_256(v)
    );
}

PRIVATE void file_of_nt(char *bf, size_t bfsize, const char *ext)
{
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "2000-01-01.%s", ext);
    build_path(bf, bfsize, path_database, TOPIC_NT, "keys", "A", name, NULL);
}

PRIVATE json_t *open_nt(BOOL master)
{
    json_t *tranger = startup(master);
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NT, FALSE)) {
        printf("%sERROR%s --> cannot open the store of %s\n", On_Red BWhite, Color_Off, TOPIC_NT);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return NULL;
    }
    return tranger;
}

/*
 *  A running master with rows v 1..6 of key A. Its md2 as written goes to
 *  `rows` (6 rows).
 */
PRIVATE json_t *build_store_nt(char *rows)
{
    rmrdir(path_database);
    set_expected_results("256-byte rows: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup(TRUE);
    if(!tranger || !tranger2_create_topic(
            tranger, TOPIC_NT, "id", "", NULL, sf_string_key,
            json_pack("{s:s, s:I, s:s}", "id", "", "v", (json_int_t)0, "pad", ""),
            0)) {
        printf("%sERROR%s --> cannot create %s\n", On_Red BWhite, Color_Off, TOPIC_NT);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return NULL;
    }
    for(int v = 1; v <= 6; v++) {
        append_nt(tranger, v);
    }
    test_json(NULL);    // the setup logs are not what is tested

    char path[PATH_MAX];
    file_of_nt(path, sizeof(path), "md2");
    int fd = open(path, O_RDONLY|O_CLOEXEC);
    if(fd < 0 || pread(fd, rows, 6*ROW, 0) != 6*ROW) {
        printf("%sERROR%s --> the md2 of %s has not 6 rows\n", On_Red BWhite, Color_Off, TOPIC_NT);
        if(fd >= 0) {
            close(fd);
        }
        tranger2_shutdown(tranger);
        return NULL;
    }
    close(fd);
    return tranger;
}

/*
 *  Write `ln` bytes of `bf` as the md2 of key A, and add `junk` bytes to
 *  the end of its content file: content that no row names, as a process
 *  killed between the content and the md2 row of an append leaves it
 *  (7.25.4 did not cut it back). `saved` gets the md2 as written.
 */
PRIVATE ssize_t write_md2_nt(const char *bf, size_t ln, int junk, char *saved, size_t saved_size)
{
    char path[PATH_MAX];
    file_of_nt(path, sizeof(path), "md2");
    int fd = open(path, O_WRONLY|O_CLOEXEC);
    if(fd < 0 || ftruncate(fd, 0) < 0 || pwrite(fd, bf, ln, 0) != (ssize_t)ln) {
        printf("%sERROR%s --> cannot rewrite %s\n", On_Red BWhite, Color_Off, path);
        if(fd >= 0) {
            close(fd);
        }
        return -1;
    }
    close(fd);
    if(junk > 0) {
        char content[PATH_MAX];
        file_of_nt(content, sizeof(content), "json");
        char j[256];
        memset(j, 'j', sizeof(j));
        fd = open(content, O_WRONLY|O_APPEND|O_CLOEXEC);
        if(fd < 0 || write(fd, j, (size_t)junk) != junk) {
            printf("%sERROR%s --> cannot add content to %s\n", On_Red BWhite, Color_Off, content);
            if(fd >= 0) {
                close(fd);
            }
            return -1;
        }
        close(fd);
    }
    fd = open(path, O_RDONLY|O_CLOEXEC);
    if(fd < 0) {
        return -1;
    }
    ssize_t r = pread(fd, saved, saved_size, 0);
    close(fd);
    return r;
}

/*
 *  The shape 7.25.4 left, of `k` torn bytes: rows 1-3, k bytes of row 4,
 *  rows 5 and 6
 */
PRIVATE ssize_t make_725_shape_nt(const char *rows, int k, int junk, char *saved, size_t saved_size)
{
    char bf[6*ROW];
    memcpy(bf, rows, 3*ROW);
    memcpy(bf + 3*ROW, rows + 3*ROW, (size_t)k);
    memcpy(bf + 3*ROW + k, rows + 4*ROW, 2*ROW);
    return write_md2_nt(bf, (size_t)(5*ROW + k), junk, saved, saved_size);
}

PRIVATE int expect_md2_nt_unchanged(const char *what, const char *before, ssize_t before_ln)
{
    char path[PATH_MAX];
    char now[16*ROW];
    file_of_nt(path, sizeof(path), "md2");
    int fd = open(path, O_RDONLY|O_CLOEXEC);
    ssize_t ln = fd < 0? -1: pread(fd, now, sizeof(now), 0);
    if(fd >= 0) {
        close(fd);
    }
    if(ln != before_ln || memcmp(now, before, (size_t)ln) != 0) {
        printf("%sERROR%s --> %s: the md2 changed, %ld bytes, expected %ld\n",
            On_Red BWhite, Color_Off, what, (long)ln, (long)before_ln);
        return -1;
    }
    return 0;
}

PRIVATE off_t content_size_nt(void)
{
    char path[PATH_MAX];
    file_of_nt(path, sizeof(path), "json");
    return filesize(path);
}

/*
 *  The keyless list forward of TOPIC_NT: what it hands, and whether it
 *  says load_failed. The expected logs are the caller's.
 */
PRIVATE int check_list_nt(json_t *tranger, const char *what, const char *expected, BOOL failed)
{
    int result = 0;
    got[0] = 0;
    json_t *match_cond = json_pack("{s:I, s:b, s:I}",
        "to_rowid", (json_int_t)1000000,   // no realtime
        "backward", 0,
        "load_record_callback", (json_int_t)(uintptr_t)on_record
    );
    json_t *list = tranger2_open_list(tranger, TOPIC_NT, match_cond, json_object(), "", FALSE, "");
    if(!list) {
        printf("%sERROR%s --> %s: list REFUSED\n", On_Red BWhite, Color_Off, what);
        return -1;
    }
    result += expect(what, got, expected);
    if(json_is_true(json_object_get(list, "load_failed")) != failed) {
        printf("%sERROR%s --> %s: load_failed is %s, expected %s\n", On_Red BWhite, Color_Off,
            what, failed? "false": "true", failed? "true": "false");
        result += -1;
    }
    tranger2_close_list(tranger, list);
    return result;
}

/***************************************************************************
 *  14. The shape 7.25.4 left, with content after the last row, at the
 *      open: for every size of the torn part, with and without that
 *      content, a master does not cut it
 ***************************************************************************/
PRIVATE int test_725_shape_trailing_content(void)
{
    int result = 0;
    int junks[] = {0, JUNK};
    for(size_t j = 0; j < sizeof(junks)/sizeof(junks[0]); j++) {
        for(int k = 1; k < ROW; k++) {
            char test[256];
            char rows[6*ROW];
            char before[16*ROW];
            json_t *tranger = build_store_nt(rows);
            if(!tranger) {
                return -1;
            }
            tranger2_shutdown(tranger);
            ssize_t before_ln = make_725_shape_nt(rows, k, junks[j], before, sizeof(before));
            if(before_ln != 5*ROW + k) {
                return -1;
            }
            off_t content_size = content_size_nt();

            snprintf(test, sizeof(test),
                "14. the shape 7.25.4 left, %d torn bytes, %d bytes of content after the last row: not cut",
                k, junks[j]);
            set_expected_results(test,
                json_pack("[{s:s, s:s, s:s, s:I, s:I, s:I},{s:s, s:s, s:s},{s:s},{s:s}]",
                    "msg", MSG_725,
                    "key", "A",
                    "file_id", "2000-01-01",
                    "md2_size", (json_int_t)(5*ROW + k),
                    "content_size", (json_int_t)content_size,
                    "row_at", (json_int_t)(4*ROW + k),
                    "msg", MSG_FLAG,
                    "key", "A",
                    "file_id", "2000-01-01",
                    "msg", MSG_ITER,
                    "msg", MSG_LIST
                ), NULL, NULL, 1
            );
            tranger = open_nt(TRUE);
            if(!tranger) {
                test_json(NULL);
                return -1;
            }
            result += check_list_nt(tranger, test, "", TRUE);
            tranger2_shutdown(tranger);
            result += test_json(NULL);
            result += expect_md2_nt_unchanged(test, before, before_ln);
        }
    }
    return result;
}

/***************************************************************************
 *  15. The same shape, with content after the last row, at an append of a
 *      running master: the append is refused, the md2 is not cut, and the
 *      file is flagged in memory as the open flags it
 ***************************************************************************/
PRIVATE int test_725_shape_trailing_content_at_append(void)
{
    int result = 0;
    char rows[6*ROW];
    char before[16*ROW];
    json_t *tranger = build_store_nt(rows);
    if(!tranger) {
        return -1;
    }
    ssize_t before_ln = make_725_shape_nt(rows, 1, JUNK, before, sizeof(before));
    if(before_ln != 5*ROW + 1) {
        tranger2_shutdown(tranger);
        return -1;
    }
    off_t content_size = content_size_nt();

    set_expected_results("15. the append is refused, the md2 is not cut, the file is flagged",
        json_pack("[{s:s, s:s, s:s, s:I, s:I, s:I},{s:s, s:s, s:s},{s:s, s:s, s:s, s:I}]",
            "msg", MSG_725,
            "key", "A",
            "file_id", "2000-01-01",
            "md2_size", (json_int_t)before_ln,
            "content_size", (json_int_t)content_size,
            "row_at", (json_int_t)(4*ROW + 1),
            "msg", MSG_FLAG_APPEND,
            "key", "A",
            "file_id", "2000-01-01",
            "msg", MSG_APPEND_NOT_CUT,
            "key", "A",
            "file_id", "2000-01-01",
            "md2_size", (json_int_t)before_ln
        ), NULL, NULL, 1
    );
    if(append_nt(tranger, 7) == 0) {
        printf("%sERROR%s --> 15: the append was taken\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);
    result += expect_md2_nt_unchanged("15. after the append", before, before_ln);
    if(content_size_nt() != content_size) {
        printf("%sERROR%s --> 15: the content of the refused append was not cut back\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    set_expected_results("15. the list of the same process says load_failed",
        json_pack("[{s:s},{s:s}]",
            "msg", MSG_ITER,
            "msg", MSG_LIST
        ), NULL, NULL, 1
    );
    result += check_list_nt(tranger, "15. the list", "", TRUE);
    result += test_json(NULL);

    set_expected_results("15. the next append is refused: the file is flagged",
        json_pack("[{s:s},{s:s, s:s, s:s}]",
            "msg", MSG_725,
            "msg", MSG_APPEND_FLAGGED,
            "key", "A",
            "file_id", "2000-01-01"
        ), NULL, NULL, 1
    );
    if(append_nt(tranger, 8) == 0) {
        printf("%sERROR%s --> 15: the second append was taken\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);
    result += expect_md2_nt_unchanged("15. after the second append", before, before_ln);

    set_expected_results("15. the shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    result += expect_md2_nt_unchanged("15. after the shutdown", before, before_ln);

    return result;
}

/***************************************************************************
 *  16. A torn row, with its content and more content after it: for every
 *      size of the torn part, the master cuts it back and the key loads
 ***************************************************************************/
PRIVATE int test_torn_row_trailing_content(void)
{
    int result = 0;
    for(int k = 1; k < ROW; k++) {
        char test[256];
        char rows[6*ROW];
        char before[16*ROW];
        json_t *tranger = build_store_nt(rows);
        if(!tranger) {
            return -1;
        }
        tranger2_shutdown(tranger);
        if(write_md2_nt(rows, (size_t)(5*ROW + k), JUNK, before, sizeof(before)) != 5*ROW + k) {
            return -1;
        }

        snprintf(test, sizeof(test),
            "16. a torn row of %d bytes, its content and %d bytes more: cut back", k, JUNK);
        set_expected_results(test,
            json_pack("[{s:s, s:s, s:s, s:i, s:i}]",
                "msg", MSG_CUT,
                "key", "A",
                "file_id", "2000-01-01",
                "old_size", 5*ROW + k,
                "new_size", 5*ROW
            ), NULL, NULL, 1
        );
        tranger = open_nt(TRUE);
        if(!tranger) {
            test_json(NULL);
            return -1;
        }
        result += check_list_nt(tranger, test, "A@1 A@2 A@3 A@4 A@5", FALSE);
        tranger2_shutdown(tranger);
        result += test_json(NULL);
    }
    return result;
}

/*
 *  Rewrite the __offset__ and __size__ of the row at `at` of A's md2 of
 *  day 1 (big endian, as on disk)
 */
PRIVATE int patch_row(off_t at, int64_t add_offset, int64_t add_size)
{
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), "2000-01-02", "md2");
    int fd = open(path, O_RDWR|O_CLOEXEC);
    uint64_t f[2];
    if(fd < 0 || pread(fd, f, sizeof(f), at + 16) != sizeof(f)) {
        printf("%sERROR%s --> cannot read a row of %s\n", On_Red BWhite, Color_Off, path);
        if(fd >= 0) {
            close(fd);
        }
        return -1;
    }
    f[0] = htobe64(be64toh(f[0]) + (uint64_t)add_offset);
    f[1] = htobe64(be64toh(f[1]) + (uint64_t)add_size);
    if(pwrite(fd, f, sizeof(f), at + 16) != sizeof(f)) {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

/*
 *  A torn md2 of A's day 1 (3 rows and TORN bytes) that a master opens
 *  and does not cut: MSG_LAST_BAD with `cause` at `row_at`, the file
 *  flagged, the md2 unchanged
 */
PRIVATE int expect_last_bad(const char *what, const char *cause, off_t row_at)
{
    int result = 0;
    char before[16*ROW];
    char test[256];
    ssize_t before_ln = read_md2("2000-01-02", before, sizeof(before));

    set_expected_results(what,
        json_pack("[{s:s, s:s, s:s, s:s, s:I, s:I},{s:s, s:s, s:s}]",
            "msg", MSG_LAST_BAD,
            "cause", cause,
            "key", "A",
            "file_id", "2000-01-02",
            "md2_size", (json_int_t)(3*ROW + TORN),
            "row_at", (json_int_t)row_at,
            "msg", MSG_FLAG,
            "key", "A",
            "file_id", "2000-01-02"
        ), NULL, NULL, 1
    );
    json_t *tranger = startup(TRUE);
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> %s: cannot open the store\n", On_Red BWhite, Color_Off, what);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += test_json(NULL);
    snprintf(test, sizeof(test), "%s: after the open", what);
    result += expect_md2_unchanged(test, "2000-01-02", before, before_ln);
    snprintf(test, sizeof(test), "%s: the list", what);
    result += check_list_flagged(tranger, test);

    set_expected_results("the shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  17. A torn md2 whose last whole row names content inside the content
 *      file that is not a record (the end of a record): not cut
 ***************************************************************************/
PRIVATE int test_last_row_not_a_record(void)
{
    if(build_store() < 0 || tear_md2("2000-01-02", 3*ROW + TORN) < 0 ||
            patch_row(2*ROW, 1, -1) < 0) {
        return -1;
    }
    return expect_last_bad(
        "17. a master: the last whole row does not name a record, not cut, flagged",
        "its content is not a record", 2*ROW
    );
}

/***************************************************************************
 *  18. A torn md2 whose last whole row names content before the content
 *      of the row before it: not cut
 ***************************************************************************/
PRIVATE int test_last_row_before_previous(void)
{
    if(build_store() < 0 || tear_md2("2000-01-02", 3*ROW + TORN) < 0) {
        return -1;
    }
    char rows[3*ROW];
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), "2000-01-02", "md2");
    if(read_md2("2000-01-02", rows, sizeof(rows)) != (ssize_t)sizeof(rows)) {
        return -1;
    }
    int fd = open(path, O_WRONLY|O_CLOEXEC);
    if(fd < 0 ||
            pwrite(fd, rows + 2*ROW, ROW, ROW) != ROW ||
            pwrite(fd, rows + ROW, ROW, 2*ROW) != ROW) {
        printf("%sERROR%s --> 18: cannot swap the rows of %s\n", On_Red BWhite, Color_Off, path);
        if(fd >= 0) {
            close(fd);
        }
        return -1;
    }
    close(fd);
    return expect_last_bad(
        "18. a master: the last whole row names content before the row before it, not cut, flagged",
        "its content starts before the end of the content of the row before it", 2*ROW
    );
}

/***************************************************************************
 *  19. A torn md2 whose last whole row is a deleted instance with its
 *      content zeroed: it is a valid row, and the master cuts the tail
 ***************************************************************************/
PRIVATE int test_last_row_zeroed_instance(void)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }
    set_expected_results("19. delete the last instance of day 1, content zeroed", NULL, NULL, NULL, 1);
    json_t *tranger = startup(TRUE);
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE) ||
            tranger2_delete_instance(tranger, TOPIC_NAME, "A", DAY1 + DAY + 4, 3, TRUE) < 0) {
        printf("%sERROR%s --> 19: cannot delete the instance\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    if(tear_md2("2000-01-02", 3*ROW + TORN) < 0) {
        return -1;
    }

    set_expected_results("19. a master cuts the torn row after a zeroed instance",
        json_pack("[{s:s, s:s, s:s, s:i, s:i}]",
            "msg", MSG_CUT,
            "key", "A",
            "file_id", "2000-01-02",
            "old_size", 3*ROW + TORN,
            "new_size", 3*ROW
        ), NULL, NULL, 1
    );
    tranger = startup(TRUE);
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> 19: cannot open the store\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += check_list(tranger, "19. the list", "A@1 A@2 A@3 B@1");
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    result += expect_size("19. after the open", md2_size("2000-01-02"), 3*ROW);
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
    result += test_725_shape(TRUE);
    result += test_725_shape(FALSE);
    result += test_last_row_not_valid();
    result += test_725_shape_at_append();
    result += test_725_shape_trailing_content();
    result += test_725_shape_trailing_content_at_append();
    result += test_torn_row_trailing_content();
    result += test_last_row_not_a_record();
    result += test_last_row_before_previous();
    result += test_last_row_zeroed_instance();

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
