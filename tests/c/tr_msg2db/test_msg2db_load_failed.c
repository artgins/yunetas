/****************************************************************************
 *          test_msg2db_load_failed.c
 *
 *  msg2db keeps, per id and pkey2, the LAST message it loaded: it loads its
 *  topic forward, oldest first, and each message replaces the one before.
 *  A forward load that stops half way leaves the last message it read, an
 *  OLD one, in the place of the current (independent review of the fourth
 *  fix round, repro indep4_A/r_msg2db_stale):
 *
 *      1. A file of dev1 whose only append was never acknowledged (a md2
 *         of 0 rows, its content not empty) between the file of its old
 *         message and the file of its new one. It was taken for damage,
 *         the load stopped there, and the old message was served. It is
 *         ignored with a warning: the new message is served.
 *      2. The file of dev1's new message really damaged (a md2 that cannot
 *         be read, mode 000): the load of dev1 stops before it.
 *         What it read is not served as current: msg2db reloads dev1
 *         BACKWARD, the load stops at once at the damaged newest file, and
 *         dev1 is not in memory; that is logged. dev2, whole, is served.
 *         The damaged file is the file of the current period, where the
 *         next message goes: msg2db says so at the open, and the next
 *         message of dev1 is REFUSED (the append into a flagged file is),
 *         so it is not served either. The doc said it was.
 *      3. dev1 with two alarms and the damage in the MIDDLE of its history:
 *         X (old in the first file, new in the last) and Y (old in the
 *         first file, newer in the damaged one). X's newest message is
 *         readable, and it is served; Y's newest message is not, and Y is
 *         absent -- its old message is not served as current. The whole
 *         key was dropped before the fix of the fifth fix round: X, whose
 *         current state was on disk and readable, was absent too, and the
 *         alarms of the projects (db_history) announced it again as new.
 *      4. The store of 3 with a record of dev1 whose pkey2 is empty in its
 *         first file (read by the forward load) and another in its last
 *         file (read only by the backward reload). Both are dropped, and
 *         "Records NOT loaded, 'pkey2' empty" says 2: it said 1, because
 *         the count of the forward load was put back after the reload.
 *      5. The md2 of dev1's new message ends in a part of a row (a power
 *         cut during the write of a row). That is an append that was never
 *         acknowledged, not damage: the md2 is cut back with one warning,
 *         dev1 is whole, and its next message is stored and served. Since
 *         7.25.4 (unreleased work) it was damage, and every new message of
 *         dev1 was refused until the period changed -- a year for the
 *         alarms of the projects ("filename_mask": "%Y").
 *
 *  Cases 2, 3 and 4 make a md2 unreadable with mode 000: they are skipped
 *  as root, who reads it anyway.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <yunetas.h>

#define APP             "test_msg2db_load_failed"
#define DATABASE        "tr_msg2db_load_failed"
#define MSG2DB_NAME     "msg2db_test"
#define TOPIC_NAME      "alarms"

#define MSG_UNCOMMITTED "md2 file of the key with no rows and a content file that is not empty: an append that was never acknowledged, the file is ignored"
#define MSG_CURRENT_DAMAGED "msg2db: the damaged file of the key is the file of the current period: every new message of the key is REFUSED until the file is repaired or the period changes"
#define MSG_CUT         "md2 file of the key ends in a part of a row: an append that was never acknowledged was cut back"
#define MSG_NOT_SERVED  "msg2db: a key whose history did not load whole: only the messages newer than the damage are served, a pkey2 whose newest message was not read is ABSENT and its state unknown (msg2db_id_incomplete)"

PRIVATE yev_loop_h yev_loop;
PRIVATE char path_root[PATH_MAX];
PRIVATE char path_database[PATH_MAX];

PRIVATE char msg2db_schema[]= "\
{                                                                   \n\
    'id': '"MSG2DB_NAME"',                                          \n\
    'schema_version': '1',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': '"TOPIC_NAME"',                                   \n\
            'pkey': 'id',                                           \n\
            'tkey': 'tm',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'pkey2': 'alarm',                                       \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Device Id',                          \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'tm': {                                             \n\
                    'header': 'Time',                               \n\
                    'type': 'integer',                              \n\
                    'flag': ['persistent','required','time']        \n\
                },                                                  \n\
                'alarm': {                                          \n\
                    'header': 'Alarm',                              \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'description': {                                    \n\
                    'header': 'Description',                        \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************************
 *  The turns of the loop that free the fs watcher of a master (see
 *  test_pkey2_empty.c)
 ***************************************************************************/
PRIVATE void drain_loop(void)
{
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
}

PRIVATE json_t *open_all(void)
{
    json_t *tranger = tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", 0
    ), yev_loop);
    json_t *jn_schema = legalstring2json(msg2db_schema, TRUE);
    msg2db_open_db(tranger, MSG2DB_NAME, jn_schema, "");
    return tranger;
}

PRIVATE void close_all(json_t *tranger)
{
    msg2db_close_db(tranger, MSG2DB_NAME);
    tranger2_shutdown(tranger);
    drain_loop();
}

PRIVATE void put_alarm(json_t *tranger, const char *id, const char *alarm, const char *description)
{
    msg2db_append_message(tranger, MSG2DB_NAME, TOPIC_NAME,
        json_pack("{s:s, s:I, s:s, s:s}",
            "id", id,
            "tm", (json_int_t)time(0),
            "alarm", alarm,
            "description", description
        ),
        ""
    );
}

PRIVATE void put(json_t *tranger, const char *id, const char *description)
{
    put_alarm(tranger, id, "X", description);
}

PRIVATE int expect_alarm(json_t *tranger, const char *what, const char *id, const char *alarm,
    const char *expected)
{
    json_t *msg = msg2db_get_message(tranger, MSG2DB_NAME, TOPIC_NAME, id, alarm);
    const char *found = msg? kw_get_str(0, msg, "description", "?", 0) : "(absent)";
    if(strcmp(found, expected) != 0) {
        printf("%sERROR%s --> %s: %s/%s is [%s], expected [%s]\n",
            On_Red BWhite, Color_Off, what, id, alarm, found, expected);
        return -1;
    }
    return 0;
}

PRIVATE int expect_description(json_t *tranger, const char *what, const char *id, const char *expected)
{
    return expect_alarm(tranger, what, id, "X", expected);
}

PRIVATE void key_dir(char *bf, size_t bfsize, const char *id)
{
    build_path(bf, bfsize, path_database, TOPIC_NAME, "keys", id, NULL);
}

/*
 *  Give the files of `id` written today (not named 2000-*) the name of `day`
 */
PRIVATE int move_today_files(const char *id, const char *day)
{
    char dir[PATH_MAX];
    key_dir(dir, sizeof(dir), id);
    const char *exts[] = {"md2", "json", NULL};
    int moved = 0;
    for(int e = 0; exts[e]; e++) {
        char pattern[32];
        snprintf(pattern, sizeof(pattern), ".*\\.%s", exts[e]);
        dir_array_t da;
        get_ordered_filename_array(0, dir, pattern, WD_MATCH_REGULAR_FILE, &da);
        for(int i = 0; i < da.count; i++) {
            if(strstr(da.items[i], "/2000-")) {
                continue;
            }
            char name[NAME_MAX];
            char dst[PATH_MAX];
            snprintf(name, sizeof(name), "%s.%s", day, exts[e]);
            build_path(dst, sizeof(dst), dir, name, NULL);
            if(rename(da.items[i], dst) == 0) {
                moved++;
            }
        }
        dir_array_free(&da);
    }
    return moved;
}

/*
 *  A md2 that cannot be read: mode 000. Return 1 when done, 0 when it
 *  cannot be done (root reads a file of mode 000), -1 on error.
 */
PRIVATE int make_unreadable(const char *case_name, const char *path)
{
    if(geteuid() == 0) {
        printf("%s: SKIPPED, running as root: a md2 of mode 000 is still read\n", case_name);
        return 0;
    }
    if(chmod(path, 0) < 0) {
        printf("%sERROR%s --> cannot make %s unreadable\n", On_Red BWhite, Color_Off, path);
        return -1;
    }
    return 1;
}

/*
 *  The store: dev1 with OLD in the file 2000-01-01 and NEW in today's,
 *  dev2 with one message
 */
PRIVATE int build_store(void)
{
    rmrdir(path_database);
    set_expected_results("build the store", NULL, NULL, NULL, FALSE);

    json_t *tranger = open_all();
    put(tranger, "dev1", "OLD");
    close_all(tranger);

    char dir[PATH_MAX];
    key_dir(dir, sizeof(dir), "dev1");
    const char *exts[] = {"md2", "json", NULL};
    int moved = 0;
    for(int e = 0; exts[e]; e++) {
        char pattern[32];
        snprintf(pattern, sizeof(pattern), ".*\\.%s", exts[e]);
        dir_array_t da;
        get_ordered_filename_array(0, dir, pattern, WD_MATCH_REGULAR_FILE, &da);
        for(int i = 0; i < da.count; i++) {
            char name[NAME_MAX];
            char dst[PATH_MAX];
            snprintf(name, sizeof(name), "2000-01-01.%s", exts[e]);
            build_path(dst, sizeof(dst), dir, name, NULL);
            if(rename(da.items[i], dst) == 0) {
                moved++;
            }
        }
        dir_array_free(&da);
    }

    tranger = open_all();
    put(tranger, "dev1", "NEW");
    put(tranger, "dev2", "WHOLE");
    close_all(tranger);
    test_json(NULL);    // the setup logs are not what is tested

    if(moved != 2) {
        printf("%sERROR%s --> cannot move the old message of dev1\n", On_Red BWhite, Color_Off);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  1. An uncommitted file between the old message and the new one
 ***************************************************************************/
PRIVATE int test_uncommitted(void)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }

    char dir[PATH_MAX];
    char src[PATH_MAX];
    char dst[PATH_MAX];
    key_dir(dir, sizeof(dir), "dev1");
    build_path(src, sizeof(src), dir, "2000-01-01.json", NULL);
    build_path(dst, sizeof(dst), dir, "2001-01-01.json", NULL);
    int ret = copyfile(src, dst, 0660, TRUE);
    build_path(dst, sizeof(dst), dir, "2001-01-01.md2", NULL);
    FILE *f = fopen(dst, "w");
    if(ret < 0 || !f) {
        printf("%sERROR%s --> 1: cannot leave the uncommitted file\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(f) {
        fclose(f);
    }

    set_expected_results("1. an uncommitted file between the old and the new message",
        json_pack("[{s:s}]", "msg", MSG_UNCOMMITTED), NULL, NULL, TRUE
    );
    json_t *tranger = open_all();
    result += expect_description(tranger, "1", "dev1", "NEW");
    result += expect_description(tranger, "1", "dev2", "WHOLE");
    close_all(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  2. The file of the new message damaged: dev1 is not served
 ***************************************************************************/
PRIVATE int test_damaged(void)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }

    /*
     *  The md2 of today's file cannot be read
     */
    char dir[PATH_MAX];
    key_dir(dir, sizeof(dir), "dev1");
    dir_array_t da;
    get_ordered_filename_array(0, dir, ".*\\.md2", WD_MATCH_REGULAR_FILE, &da);
    int damaged = 0;
    for(int i = 0; i < da.count; i++) {
        if(strstr(da.items[i], "2000-01-01.md2")) {
            continue;
        }
        int ret = make_unreadable("2", da.items[i]);
        if(ret == 0) {
            dir_array_free(&da);
            return 0;   // skipped
        }
        if(ret > 0) {
            damaged++;
        }
    }
    dir_array_free(&da);
    if(damaged != 1) {
        printf("%sERROR%s --> 2: cannot damage the md2 of dev1's new message\n",
            On_Red BWhite, Color_Off);
        return -1;
    }

    set_expected_results("2. the file of the new message damaged",
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Cannot open md2 file",
            "msg", "md2 file of the key unreadable when its cache was built: every load of the key says load_failed",
            "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built",
            "msg", "Cannot load the whole history of a key of the list: the records read before the failure "
                   "were handed, the list goes on with the next key",
            "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built",
            "msg", MSG_NOT_SERVED,
            "msg", MSG_CURRENT_DAMAGED
        ), NULL, NULL, TRUE
    );
    json_t *tranger = open_all();
    result += expect_description(tranger, "2", "dev1", "(absent)");
    result += expect_description(tranger, "2", "dev2", "WHOLE");
    result += test_json(NULL);

    /*
     *  The next message of dev1 goes to the damaged file: it is refused
     */
    set_expected_results("2. the next message of dev1 goes to the damaged file",
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot open md2 file",
            "msg", "Cannot append record, its file is flagged unreadable: its row would follow rows no cell counts"
        ), NULL, NULL, TRUE
    );
    put(tranger, "dev1", "NEXT");
    result += expect_description(tranger, "2", "dev1", "(absent)");
    close_all(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  3. The damage in the middle of dev1's history: what was newer is served
 ***************************************************************************/
PRIVATE int test_damaged_middle(void)
{
    int result = 0;
    rmrdir(path_database);
    set_expected_results("3. build the store", NULL, NULL, NULL, FALSE);

    json_t *tranger = open_all();
    put_alarm(tranger, "dev1", "X", "OLD_X");
    put_alarm(tranger, "dev1", "Y", "OLD_Y");
    close_all(tranger);
    int moved = move_today_files("dev1", "2000-01-01");

    tranger = open_all();
    put_alarm(tranger, "dev1", "Y", "MID_Y");
    close_all(tranger);
    moved += move_today_files("dev1", "2000-01-02");

    tranger = open_all();
    put_alarm(tranger, "dev1", "X", "NEW_X");
    put_alarm(tranger, "dev2", "X", "WHOLE");
    close_all(tranger);
    test_json(NULL);    // the setup logs are not what is tested

    if(moved != 4) {
        printf("%sERROR%s --> 3: cannot move the files of dev1\n", On_Red BWhite, Color_Off);
        return -1;
    }

    /*
     *  The md2 of the middle file cannot be read
     */
    char dir[PATH_MAX];
    char md2[PATH_MAX];
    key_dir(dir, sizeof(dir), "dev1");
    build_path(md2, sizeof(md2), dir, "2000-01-02.md2", NULL);
    int ret = make_unreadable("3", md2);
    if(ret == 0) {
        return 0;   // skipped
    }
    if(ret < 0) {
        result += -1;
    }

    set_expected_results("3. the damage in the middle of dev1's history",
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Cannot open md2 file",
            "msg", "md2 file of the key unreadable when its cache was built: every load of the key says load_failed",
            "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built",
            "msg", "Cannot load the whole history of a key of the list: the records read before the failure "
                   "were handed, the list goes on with the next key",
            "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built",
            "msg", MSG_NOT_SERVED
        ), NULL, NULL, TRUE
    );
    tranger = open_all();
    result += expect_alarm(tranger, "3", "dev1", "X", "NEW_X");
    result += expect_alarm(tranger, "3", "dev1", "Y", "(absent)");
    result += expect_alarm(tranger, "3", "dev2", "X", "WHOLE");
    result += test_json(NULL);

    /*
     *  An absent message of an incomplete id is UNKNOWN, not "never was":
     *  the id says so
     */
    set_expected_results("3. the ids that are incomplete", NULL, NULL, NULL, TRUE);
    if(!msg2db_id_incomplete(tranger, MSG2DB_NAME, TOPIC_NAME, "dev1")) {
        printf("%sERROR%s --> 3: dev1 is not incomplete\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(msg2db_id_incomplete(tranger, MSG2DB_NAME, TOPIC_NAME, "dev2")) {
        printf("%sERROR%s --> 3: dev2 is incomplete\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    /*
     *  The next message of Y is its current one; dev1 stays incomplete (its
     *  other alarms may still be unknown) until the store is repaired
     */
    put_alarm(tranger, "dev1", "Y", "NEXT_Y");
    result += expect_alarm(tranger, "3", "dev1", "Y", "NEXT_Y");
    if(!msg2db_id_incomplete(tranger, MSG2DB_NAME, TOPIC_NAME, "dev1")) {
        printf("%sERROR%s --> 3: dev1 is not incomplete after a message\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    close_all(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  4. The records with an empty pkey2 are counted in both loads
 ***************************************************************************/
PRIVATE void put_raw_empty_pkey2(json_t *tranger, const char *id)
{
    /*
     *  msg2db_append_message() refuses an empty pkey2: an old store may
     *  hold them, so they are written with tranger2 directly
     */
    md2_record_ex_t md = {0};
    tranger2_append_record(tranger, TOPIC_NAME, 0, 0, &md,
        json_pack("{s:s, s:I, s:s, s:s}",
            "id", id,
            "tm", (json_int_t)time(0),
            "alarm", "",
            "description", "EMPTY"
        )
    );
}

PRIVATE int test_empty_pkey2_both_loads(void)
{
    int result = 0;
    rmrdir(path_database);
    set_expected_results("4. build the store", NULL, NULL, NULL, FALSE);

    json_t *tranger = open_all();
    put_alarm(tranger, "dev1", "X", "OLD_X");
    put_raw_empty_pkey2(tranger, "dev1");
    close_all(tranger);
    int moved = move_today_files("dev1", "2000-01-01");

    tranger = open_all();
    put_alarm(tranger, "dev1", "Y", "MID_Y");
    close_all(tranger);
    moved += move_today_files("dev1", "2000-01-02");

    tranger = open_all();
    put_alarm(tranger, "dev1", "X", "NEW_X");
    put_raw_empty_pkey2(tranger, "dev1");
    close_all(tranger);
    test_json(NULL);    // the setup logs are not what is tested

    if(moved != 4) {
        printf("%sERROR%s --> 4: cannot move the files of dev1\n", On_Red BWhite, Color_Off);
        return -1;
    }

    char dir[PATH_MAX];
    char md2[PATH_MAX];
    key_dir(dir, sizeof(dir), "dev1");
    build_path(md2, sizeof(md2), dir, "2000-01-02.md2", NULL);
    int ret = make_unreadable("4", md2);
    if(ret == 0) {
        return 0;   // skipped
    }
    if(ret < 0) {
        result += -1;
    }

    set_expected_results("4. an empty pkey2 on each side of the damage",
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s, s:i}]",
            "msg", "Cannot open md2 file",
            "msg", "md2 file of the key unreadable when its cache was built: every load of the key says load_failed",
            "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built",
            "msg", "Field 'pkey2' required, record NOT loaded (first of the topic)",
            "msg", "Cannot load the whole history of a key of the list: the records read before the failure "
                   "were handed, the list goes on with the next key",
            "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built",
            "msg", MSG_NOT_SERVED,
            "msg", "Records NOT loaded, 'pkey2' empty",
            "dropped", 2
        ), NULL, NULL, TRUE
    );
    tranger = open_all();
    result += expect_alarm(tranger, "4", "dev1", "X", "NEW_X");
    result += expect_alarm(tranger, "4", "dev1", "Y", "(absent)");
    close_all(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  5. The md2 of today's file ends in a part of a row: an append that was
 *     never acknowledged. It is cut back, dev1 is whole, and its next
 *     message is stored and served.
 ***************************************************************************/
PRIVATE int test_torn_tail(void)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }

    char dir[PATH_MAX];
    key_dir(dir, sizeof(dir), "dev1");
    dir_array_t da;
    get_ordered_filename_array(0, dir, ".*\\.md2", WD_MATCH_REGULAR_FILE, &da);
    char md2[PATH_MAX] = {0};
    for(int i = 0; i < da.count; i++) {
        if(!strstr(da.items[i], "2000-01-01.md2")) {
            snprintf(md2, sizeof(md2), "%s", da.items[i]);
        }
    }
    dir_array_free(&da);
    off_t size = filesize(md2);
    if(!md2[0] || size != 32 || truncate(md2, size + 13) < 0) {
        printf("%sERROR%s --> 5: cannot tear the md2 of dev1's new message\n",
            On_Red BWhite, Color_Off);
        return -1;
    }

    set_expected_results("5. the md2 of today's file ends in a part of a row",
        json_pack("[{s:s, s:s, s:i, s:i}]",
            "msg", MSG_CUT,
            "key", "dev1",
            "old_size", 32 + 13,
            "new_size", 32
        ), NULL, NULL, TRUE
    );
    json_t *tranger = open_all();
    result += expect_description(tranger, "5", "dev1", "NEW");
    result += expect_description(tranger, "5", "dev2", "WHOLE");
    if(msg2db_id_incomplete(tranger, MSG2DB_NAME, TOPIC_NAME, "dev1")) {
        printf("%sERROR%s --> 5: dev1 is incomplete\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("5. the next message of dev1 is stored and served", NULL, NULL, NULL, TRUE);
    put(tranger, "dev1", "NEXT");
    result += expect_description(tranger, "5", "dev1", "NEXT");
    close_all(tranger);
    if(filesize(md2) != 64) {
        printf("%sERROR%s --> 5: md2 size %ld, expected 64\n",
            On_Red BWhite, Color_Off, (long)filesize(md2));
        result += -1;
    }
    tranger = open_all();
    result += expect_description(tranger, "5", "dev1", "NEXT");
    if(msg2db_id_incomplete(tranger, MSG2DB_NAME, TOPIC_NAME, "dev1")) {
        printf("%sERROR%s --> 5: dev1 is incomplete after the restart\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    close_all(tranger);
    result += test_json(NULL);

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
    helper_quote2doublequote(msg2db_schema);

    result += test_uncommitted();
    result += test_damaged();
    result += test_damaged_middle();
    result += test_empty_pkey2_both_loads();
    result += test_torn_tail();

    return result;
}

/***************************************************************************
 *
 ***************************************************************************/
int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    unsigned long memory_check_list[] = {0}; // WARNING: list ended with 0
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);
    signal(SIGPIPE, SIG_IGN);

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
    return result < 0 ? -1 : 0;
}
