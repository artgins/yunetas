/****************************************************************************
 *          test_c_tranger_replica.c
 *
 *  The commands of a C_TRANGER that is a REPLICA (master=0) of a store that
 *  a master in the same process purges (retention, tranger2_delete_key_files()).
 *  The replica runs no loop: it hears no signal, and finds each purge before
 *  its next read of the key, as a follower that missed it.
 *
 *      - a FILTERED multi-key iterator whose key the master purged after the
 *        open: the replica applies the purge inside the open of the part,
 *        and the page closes that part before it answers that the iterator
 *        was closed (a part left open stayed in the topic's iterators);
 *      - an UNFILTERED multi-key iterator over a key the replica had
 *        numbered short (a md2 it could not read when it built the key's
 *        cache): the purge renumbers the key (`renumbered`), and the
 *        positions of the page do not move with it;
 *      - delete-key-files on a replica is answered READ-ONLY, before the
 *        library;
 *      - iterators with ABSOLUTE rowid bounds (from_rowid=3, to_rowid=4)
 *        across a renumber that closes no part (the master purges only rows
 *        the replica never counted): a one-key one and a multi-key one page
 *        the same rows, under their new g_rowids. The parts of a multi-key
 *        iterator are opened again for every page, with the bounds of the
 *        open moved by the key's `renumbered` since then.
 *
 *  The C_TRANGER gobj is the yuno; the master is a plain tranger of the same
 *  store, started first so it holds the lock.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

#include <yunetas.h>
#include <testing.h>

#define APP         "test_c_tranger_replica"

#define DATABASE    "tr_c_tranger_replica"
#define TOPIC_NAME  "topic_replica"
#define TOPIC_BOUNDS "topic_bounds"
#define BASE_T      946684800   // 2000-01-01T00:00:00+0000

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int global_result = 0;

PRIVATE BOOL test_authz_checker(hgobj gobj, const char *authz, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return TRUE;
}

/***************************************************************
 *              Check helpers
 ***************************************************************/
PRIVATE void check_int(const char *name, json_int_t got, json_int_t expected)
{
    if(got != expected) {
        printf("FAIL %-50s got %lld expected %lld\n",
            name, (long long)got, (long long)expected);
        global_result += -1;
    } else {
        printf("ok   %-50s (%lld)\n", name, (long long)got);
    }
}

PRIVATE void check_str(const char *name, const char *got, const char *expected)
{
    if(!got || strcmp(got, expected) != 0) {
        printf("FAIL %-50s got '%s' expected '%s'\n",
            name, got? got : "(null)", expected);
        global_result += -1;
    } else {
        printf("ok   %-50s ('%s')\n", name, got);
    }
}

PRIVATE void check_bool(const char *name, BOOL got, BOOL expected)
{
    if((got?1:0) != (expected?1:0)) {
        printf("FAIL %-50s got %s expected %s\n",
            name, got?"TRUE":"FALSE", expected?"TRUE":"FALSE");
        global_result += -1;
    } else {
        printf("ok   %-50s (%s)\n", name, got?"TRUE":"FALSE");
    }
}

/*
 *  The contents of the records of a page, "<content> ..."
 */
PRIVATE void page_contents(json_t *r, char *bf, size_t bfsize)
{
    bf[0] = 0;
    int idx; json_t *record;
    json_array_foreach(kw_get_list(0, r, "data`data", 0, 0), idx, record) {
        if(bf[0]) {
            strncat(bf, " ", bfsize - strlen(bf) - 1);
        }
        strncat(bf, kw_get_str(0, record, "content", "?", 0), bfsize - strlen(bf) - 1);
    }
}

/*
 *  The records of a page with their g_rowid, "<content>:<g_rowid> ..."
 */
PRIVATE void page_rowids(json_t *r, char *bf, size_t bfsize)
{
    bf[0] = 0;
    int idx; json_t *record;
    json_array_foreach(kw_get_list(0, r, "data`data", 0, 0), idx, record) {
        char item[64];
        snprintf(item, sizeof(item), "%s%s:%lld",
            bf[0]? " " : "",
            kw_get_str(0, record, "content", "?", 0),
            (long long)kw_get_int(0, record, "__md_tranger__`g_rowid", -1, 0)
        );
        strncat(bf, item, bfsize - strlen(bf) - 1);
    }
}

PRIVATE size_t topic_iterators(json_t *tranger)
{
    return json_array_size(json_object_get(tranger2_topic(tranger, TOPIC_NAME), "iterators"));
}

/***************************************************************************
 *              Test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    global_result = 0;
    char bf[256];
    json_t *r;

    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    if(register_c_tranger() != 0) {
        printf("%s: FAIL (register gclasses)\n", APP);
        return -1;
    }

    /*-------------------------------------------------*
     *      The master: keys Q and R, three days of two rows
     *-------------------------------------------------*/
    set_expected_results("replica: setup", NULL, NULL, NULL, 1);
    json_t *tm = tranger2_startup(
        0,
        json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", LOG_OPT_TRACE_STACK
        ),
        0
    );
    if(!tm || !tranger2_create_topic(
            tm, TOPIC_NAME, "id", "tm",
            json_pack("{s:s}", "filename_mask", "%Y-%m-%d"),
            sf_string_key,
            json_pack("{s:s, s:I, s:s}", "id", "", "tm", (json_int_t)0, "content", ""),
            0)) {
        printf("%s: FAIL (master)\n", APP);
        if(tm) {
            tranger2_shutdown(tm);
        }
        return -1;
    }
    const char *keys[] = {"Q", "R"};
    for(int k = 0; k < 2; k++) {
        for(int d = 0; d < 3; d++) {
            for(int j = 0; j < 2; j++) {
                uint64_t t = BASE_T + (uint64_t)d*86400 + (uint64_t)j;
                char content[8];
                snprintf(content, sizeof(content), "%s%c%d", keys[k], 'a' + d, j + 1);
                md2_record_ex_t md = {0};
                tranger2_append_record(tm, TOPIC_NAME, t, 0, &md,
                    json_pack("{s:s, s:I, s:s}", "id", keys[k], "tm", (json_int_t)t, "content", content));
            }
        }
    }
    global_result += test_json(NULL);

    /*
     *  The first day of Q cannot be read when the replica builds the key's
     *  cache (not as root, who reads it all the same): the replica numbers
     *  Q from its second day, 2 rows short of the master. The file stays
     *  unreadable until the master purges it
     */
    BOOL flagged = (geteuid() != 0)? TRUE : FALSE;
    char md2_qa[PATH_MAX];
    build_path(md2_qa, sizeof(md2_qa), path_database, TOPIC_NAME, "keys", "Q", "2000-01-01.md2", NULL);
    if(flagged) {
        chmod(md2_qa, 0);
    }

    /*-------------------------------------------------*
     *      The replica, and its two iterators of Q and R
     *-------------------------------------------------*/
    if(flagged) {
        set_expected_results(
            "replica: open, a file of Q flagged",
            json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
                "msg", "Cannot open md2 file",
                "msg", "md2 file of the key unreadable when its cache was built: every load of the key says load_failed",
                /*  the opens of Q's part: itU, itF, and the page of itU  */
                "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built",
                "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built",
                "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built"
            ),
            NULL, NULL, 1
        );
    } else {
        set_expected_results("replica: open", NULL, NULL, NULL, 1);
    }
    hgobj yuno = gobj_create_yuno(
        "tranger",
        C_TRANGER,
        json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 0,
            "on_critical_error", LOG_OPT_TRACE_STACK
        )
    );
    if(!yuno) {
        printf("%s: FAIL (gobj_create_yuno)\n", APP);
        tranger2_shutdown(tm);
        return -1;
    }
    json_t *tr = gobj_read_pointer_attr(yuno, "tranger");
    check_bool("a replica", gobj_read_bool_attr(yuno, "master"), FALSE);

    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "topic_name", TOPIC_NAME, "rkey", "^(Q|R)$", "iterator_id", "itU"), yuno);
    check_int("open-iterator Q|R", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s, s:I}",
            "topic_name", TOPIC_NAME, "rkey", "^(Q|R)$", "iterator_id", "itF",
            "from_t", (json_int_t)BASE_T), yuno);
    check_int("open-iterator Q|R filtered", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}", "iterator_id", "itU", "from_rowid", 1, "limit", 100), yuno);
    check_int("Q's positions, then R's", kw_get_int(0, r, "data`total_rows", -1, 0), flagged? 10 : 12);
    JSON_DECREF(r)
    global_result += test_json(NULL);

    /*-------------------------------------------------*
     *      The master purges the first two days of Q: the replica hears
     *      nothing (no loop)
     *-------------------------------------------------*/
    set_expected_results("replica: the master purges", NULL, NULL, NULL, 1);
    check_int("master purges Q", tranger2_delete_key_files(tm, TOPIC_NAME, "Q", BASE_T + 2*86400), 4);
    global_result += test_json(NULL);

    /*-------------------------------------------------*
     *      The filtered iterator: its part of Q applies the purge when it
     *      opens, and is closed with the iterator
     *-------------------------------------------------*/
    set_expected_results("replica: a filtered part purged under it", NULL, NULL, NULL, 1);
    size_t iterators_before = topic_iterators(tr);
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}", "iterator_id", "itF", "from_rowid", 1, "limit", 4), yuno);
    check_int("a filtered multi-key page after the purge", kw_get_int(0, r, "result", -999, 0), -1);
    check_bool("it says why",
        strstr(kw_get_str(0, r, "comment", "", 0), "were purged since it opened") != NULL, TRUE);
    JSON_DECREF(r)
    check_int("no part left open in the topic", (json_int_t)topic_iterators(tr), (json_int_t)iterators_before);
    json_t *range = tranger2_topic_key_range(tr, TOPIC_NAME, "Q");
    check_int("the replica took the master's base", kw_get_int(0, range, "purged", -1, 0), 4);
    check_int("and moved its numbering by the rows it had not counted",
        kw_get_int(0, range, "renumbered", 0, 0), flagged? 2 : 0);
    JSON_DECREF(range)
    global_result += test_json(NULL);

    /*-------------------------------------------------*
     *      The unfiltered iterator: its positions stay where they were at
     *      its open, renumbered or not
     *-------------------------------------------------*/
    set_expected_results("replica: an unfiltered part renumbered under it", NULL, NULL, NULL, 1);
    json_int_t q_positions = flagged? 4 : 6;    // Q's positions at the open
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:I, s:i}", "iterator_id", "itU",
            "from_rowid", (json_int_t)(q_positions - 1), "limit", 2), yuno);
    check_int("an unfiltered multi-key page after the purge", kw_get_int(0, r, "result", -999, 0), 0);
    page_contents(r, bf, sizeof(bf));
    check_str("Q's last positions: its last day", bf, "Qc1 Qc2");
    check_int("its total: the positions of the open", kw_get_int(0, r, "data`total_rows", -1, 0),
        q_positions + 6);
    JSON_DECREF(r)
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:I, s:i}", "iterator_id", "itU",
            "from_rowid", (json_int_t)(q_positions + 1), "limit", 2), yuno);
    page_contents(r, bf, sizeof(bf));
    check_str("R's first positions", bf, "Ra1 Ra2");
    JSON_DECREF(r)
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}", "iterator_id", "itU", "from_rowid", 1, "limit", 2), yuno);
    page_contents(r, bf, sizeof(bf));
    check_str("Q's first positions: purged, empty", bf, "");
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-iterator", json_pack("{s:s}", "iterator_id", "itU"), yuno);
    JSON_DECREF(r)
    global_result += test_json(NULL);

    /*-------------------------------------------------*
     *      delete-key-files on a replica: READ-ONLY, before the library
     *-------------------------------------------------*/
    set_expected_results("replica: delete-key-files", NULL, NULL, NULL, 1);
    r = gobj_command(yuno, "delete-key-files",
        json_pack("{s:s, s:s, s:I}",
            "topic_name", TOPIC_NAME, "key", "R", "before", (json_int_t)(BASE_T + 86400)), yuno);
    check_int("delete-key-files on a replica", kw_get_int(0, r, "result", -999, 0), -1);
    check_bool("it says READ-ONLY",
        strstr(kw_get_str(0, r, "comment", "", 0), "READ-ONLY") != NULL, TRUE);
    JSON_DECREF(r)
    check_int("R kept every row", (json_int_t)tranger2_topic_key_size(tm, TOPIC_NAME, "R"), 6);
    global_result += test_json(NULL);

    /*-------------------------------------------------*
     *      Absolute rowid bounds across a renumber: a second topic, keys Q
     *      and R of four days of two rows, the first day of Q unreadable to
     *      the replica. The master purges only that day: rows the replica
     *      never counted, so no filtered part is closed, and the numbering
     *      of Q moves by 2. The rows an iterator opened with
     *      from_rowid=3, to_rowid=4 are the same rows after it, under their
     *      new g_rowids -- a multi-key one too, whose parts are opened again
     *      for every page
     *-------------------------------------------------*/
    if(flagged) {
        set_expected_results("replica: bounds, setup", NULL, NULL, NULL, 1);
        if(!tranger2_create_topic(
                tm, TOPIC_BOUNDS, "id", "tm",
                json_pack("{s:s}", "filename_mask", "%Y-%m-%d"),
                sf_string_key,
                json_pack("{s:s, s:I, s:s}", "id", "", "tm", (json_int_t)0, "content", ""),
                0)) {
            printf("%s: FAIL (topic %s)\n", APP, TOPIC_BOUNDS);
            global_result += -1;
        }
        for(int k = 0; k < 2; k++) {
            for(int d = 0; d < 4; d++) {
                for(int j = 0; j < 2; j++) {
                    uint64_t t = BASE_T + (uint64_t)d*86400 + (uint64_t)j;
                    char content[8];
                    snprintf(content, sizeof(content), "%s%c%d", keys[k], 'a' + d, j + 1);
                    md2_record_ex_t md = {0};
                    tranger2_append_record(tm, TOPIC_BOUNDS, t, 0, &md,
                        json_pack("{s:s, s:I, s:s}", "id", keys[k], "tm", (json_int_t)t, "content", content));
                }
            }
        }
        char md2_bounds_qa[PATH_MAX];
        build_path(md2_bounds_qa, sizeof(md2_bounds_qa),
            path_database, TOPIC_BOUNDS, "keys", "Q", "2000-01-01.md2", NULL);
        chmod(md2_bounds_qa, 0);
        global_result += test_json(NULL);

        set_expected_results(
            "replica: bounds, open",
            json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
                "msg", "Cannot open md2 file",
                "msg", "md2 file of the key unreadable when its cache was built: every load of the key says load_failed",
                /*  the opens of Q: itB, itS, itN, and the parts of Q the pages of itB and itN open  */
                "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built",
                "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built",
                "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built",
                "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built",
                "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built"
            ),
            NULL, NULL, 1
        );
        r = gobj_command(yuno, "open-iterator",
            json_pack("{s:s, s:s, s:s, s:i, s:i}",
                "topic_name", TOPIC_BOUNDS, "rkey", "^(Q|R)$", "iterator_id", "itB",
                "from_rowid", 3, "to_rowid", 4), yuno);
        check_int("open-iterator Q|R rowid 3..4", kw_get_int(0, r, "result", -999, 0), 0);
        JSON_DECREF(r)
        r = gobj_command(yuno, "open-iterator",
            json_pack("{s:s, s:s, s:s, s:i, s:i}",
                "topic_name", TOPIC_BOUNDS, "key", "Q", "iterator_id", "itS",
                "from_rowid", 3, "to_rowid", 4), yuno);
        check_int("open-iterator Q rowid 3..4", kw_get_int(0, r, "result", -999, 0), 0);
        JSON_DECREF(r)
        r = gobj_command(yuno, "open-iterator",
            json_pack("{s:s, s:s, s:s, s:i}",
                "topic_name", TOPIC_BOUNDS, "rkey", "^(Q|R)$", "iterator_id", "itN",
                "from_rowid", -2), yuno);
        check_int("open-iterator Q|R rowid -2", kw_get_int(0, r, "result", -999, 0), 0);
        JSON_DECREF(r)

        r = gobj_command(yuno, "get-page",
            json_pack("{s:s, s:i, s:i}", "iterator_id", "itB", "from_rowid", 1, "limit", 10), yuno);
        page_rowids(r, bf, sizeof(bf));
        check_str("multi-key 3..4 at the open", bf, "Qc1:3 Qc2:4 Rb1:3 Rb2:4");
        JSON_DECREF(r)
        r = gobj_command(yuno, "get-page",
            json_pack("{s:s, s:i, s:i}", "iterator_id", "itS", "from_rowid", 1, "limit", 10), yuno);
        page_rowids(r, bf, sizeof(bf));
        check_str("one key 3..4 at the open", bf, "Qc1:3 Qc2:4");
        JSON_DECREF(r)
        r = gobj_command(yuno, "get-page",
            json_pack("{s:s, s:i, s:i}", "iterator_id", "itN", "from_rowid", 1, "limit", 10), yuno);
        page_rowids(r, bf, sizeof(bf));
        check_str("multi-key -2 at the open", bf, "Qd1:5 Qd2:6 Rd1:7 Rd2:8");
        JSON_DECREF(r)
        global_result += test_json(NULL);

        set_expected_results("replica: bounds, the master purges", NULL, NULL, NULL, 1);
        check_int("master purges the first day of Q",
            tranger2_delete_key_files(tm, TOPIC_BOUNDS, "Q", BASE_T + 86400), 2);
        global_result += test_json(NULL);

        set_expected_results("replica: bounds, renumbered", NULL, NULL, NULL, 1);
        r = gobj_command(yuno, "get-page",
            json_pack("{s:s, s:i, s:i}", "iterator_id", "itB", "from_rowid", 1, "limit", 10), yuno);
        check_int("multi-key 3..4 after the renumber", kw_get_int(0, r, "result", -999, 0), 0);
        page_rowids(r, bf, sizeof(bf));
        check_str("the same rows, Q's under its new g_rowids", bf, "Qc1:5 Qc2:6 Rb1:3 Rb2:4");
        JSON_DECREF(r)
        range = tranger2_topic_key_range(tr, TOPIC_BOUNDS, "Q");
        check_int("Q renumbered by the rows it had not counted",
            kw_get_int(0, range, "renumbered", 0, 0), 2);
        JSON_DECREF(range)
        r = gobj_command(yuno, "get-page",
            json_pack("{s:s, s:i, s:i}", "iterator_id", "itB", "from_rowid", 2, "limit", 2), yuno);
        page_rowids(r, bf, sizeof(bf));
        check_str("multi-key 3..4, a page across the two keys", bf, "Qc2:6 Rb1:3");
        JSON_DECREF(r)
        r = gobj_command(yuno, "get-page",
            json_pack("{s:s, s:i, s:i}", "iterator_id", "itS", "from_rowid", 1, "limit", 10), yuno);
        page_rowids(r, bf, sizeof(bf));
        check_str("one key 3..4 after the renumber", bf, "Qc1:5 Qc2:6");
        JSON_DECREF(r)
        r = gobj_command(yuno, "get-page",
            json_pack("{s:s, s:i, s:i}", "iterator_id", "itN", "from_rowid", 1, "limit", 10), yuno);
        page_rowids(r, bf, sizeof(bf));
        check_str("multi-key -2 after the renumber", bf, "Qd1:7 Qd2:8 Rd1:7 Rd2:8");
        JSON_DECREF(r)
        const char *its[] = {"itB", "itS", "itN"};
        for(int i = 0; i < 3; i++) {
            r = gobj_command(yuno, "close-iterator", json_pack("{s:s}", "iterator_id", its[i]), yuno);
            JSON_DECREF(r)
        }
        global_result += test_json(NULL);
    } else {
        printf("skip rowid bounds across a renumber: root reads every md2, nothing is renumbered\n");
    }

    set_expected_results("replica: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tm);
    global_result += test_json(NULL);

    return global_result;
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

    unsigned long memory_check_list[] = {0}; // WARNING: list ended with 0
    set_memory_check_list(memory_check_list);

    gobj_start_up(
        argc, argv,
        NULL,               // jn_global_settings
        NULL,               // persistent_attrs
        command_parser,     // global_command_parser (C_TRANGER has no mt_command_parser)
        NULL,               // global_stats_parser
        test_authz_checker, // global_authz_checker: every permission
        NULL                // global_authentication_parser
    );
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    /*
     *  Errors only: this test's yuno IS the C_TRANGER, so every answer that
     *  names the yuno warns that it has no `yuno_role_plus_name` (see
     *  test_c_tranger)
     */
    gobj_log_register_handler("testing", 0, capture_log_write, 0);
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_ERROR, 0);

    int result = do_test();

    gobj_end();

    size_t leaked = get_cur_system_memory();
    if(leaked != 0) {
        printf("%s: FAIL system memory not free: %zu\n", APP, leaked);
        print_track_mem();
        result += -1;
    }

    printf("\n%s: %s\n", APP, result==0? "PASS" : "FAIL");
    return result<0? -1 : 0;
}
