/****************************************************************************
 *          test_c_tranger.c
 *
 *  Regression coverage for the C_TRANGER cursor-pagination command surface
 *  added on top of the timeranger2 iterator primitives:
 *
 *      - list-keys      -> keys of a topic with their record counts
 *      - open-iterator  -> stateful per-key iterator (index only)
 *      - get-page       -> {total_rows, pages, data} page of records
 *      - close-iterator -> close + deregister
 *
 *  The C_TRANGER gobj is created as a master yuno; its own tranger handle is
 *  borrowed to create a topic and append records, then the commands are
 *  driven through gobj_command() (the same path command-yuno takes remotely).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <yunetas.h>

#define APP         "test_c_tranger"

#define DATABASE    "tr_c_tranger_cmd"
#define TOPIC_NAME  "topic_cmd"
#define BASE_T      946684800   // 2000-01-01T00:00:00+0000

/*  key "A" gets 5 records, key "B" gets 3 records  */
#define KEY_A       "A"
#define KEY_A_ROWS  5
#define KEY_B       "B"
#define KEY_B_ROWS  3

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int global_result = 0;

/***************************************************************
 *              Check helpers
 ***************************************************************/
PRIVATE void check_int(const char *name, json_int_t got, json_int_t expected)
{
    if(got != expected) {
        printf("FAIL %-40s got %lld expected %lld\n",
            name, (long long)got, (long long)expected);
        global_result += -1;
    } else {
        printf("ok   %-40s (%lld)\n", name, (long long)got);
    }
}

PRIVATE void check_str(const char *name, const char *got, const char *expected)
{
    if(!got || strcmp(got, expected)!=0) {
        printf("FAIL %-40s got '%s' expected '%s'\n",
            name, got?got:"(null)", expected);
        global_result += -1;
    } else {
        printf("ok   %-40s ('%s')\n", name, got);
    }
}

/*  global rowid of a record, read from its __md_tranger__ metadata
 *  (md2json emits "g_rowid" for the global rowid; "i_rowid" is per-file).  */
PRIVATE json_int_t record_rowid(json_t *record)
{
    json_t *md = json_object_get(record, "__md_tranger__");
    return json_integer_value(json_object_get(md, "g_rowid"));
}

/*  Look up one key's record count in the list-keys data array. -1 if absent. */
PRIVATE json_int_t find_key_records(json_t *jn_data, const char *key)
{
    int idx; json_t *entry;
    json_array_foreach(jn_data, idx, entry) {
        const char *k = json_string_value(json_object_get(entry, "key"));
        if(k && strcmp(k, key)==0) {
            return json_integer_value(json_object_get(entry, "records"));
        }
    }
    return -1;
}

/***************************************************************
 *              Fixture: topic + records on the gobj's tranger
 ***************************************************************/
PRIVATE int create_topic(json_t *tranger)
{
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
    return topic? 0 : -1;
}

PRIVATE int append_key(json_t *tranger, const char *key, int n)
{
    for(int j=0; j<n; j++) {
        json_t *jn_record = json_pack("{s:s, s:I, s:s}",
            "id", key,
            "tm", (json_int_t)(BASE_T + j),
            "content", "payload"
        );
        md2_record_ex_t md = {0};
        if(tranger2_append_record(tranger, TOPIC_NAME, BASE_T + j, 0, &md, jn_record) < 0) {
            return -1;
        }
    }
    return 0;
}

/*  Append one record to `key` with an explicit time (used to trigger the
 *  realtime feed with a fresh append after the fixture is loaded).  */
PRIVATE int append_one(json_t *tranger, const char *key, uint64_t t)
{
    json_t *jn_record = json_pack("{s:s, s:I, s:s}",
        "id", key,
        "tm", (json_int_t)t,
        "content", "live"
    );
    md2_record_ex_t md = {0};
    return tranger2_append_record(tranger, TOPIC_NAME, t, 0, &md, jn_record);
}

/***************************************************************
 *  Probe gclass: counts EV_TRANGER_RECORD_ADDED so the test can assert
 *  the realtime feed actually publishes on a new append.
 ***************************************************************/
PRIVATE int g_rt_count = 0;

GOBJ_DEFINE_GCLASS(C_RTPROBE);

PRIVATE int ac_rt_added(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    g_rt_count++;
    KW_DECREF(kw)
    return 0;
}

PRIVATE sdata_desc_t rtprobe_attrs[] = {
SDATA_END()
};

/*  File-scope: gclass_create keeps a POINTER to the GMETHODS, so it must
 *  outlive register_rtprobe() (a local would dangle and crash gobj_create).  */
PRIVATE const GMETHODS rtprobe_gmt = {0};

PRIVATE int register_rtprobe(void)
{
    ev_action_t st_idle[] = {
        {EV_TRANGER_RECORD_ADDED, ac_rt_added, 0},
        {0, 0, 0}
    };
    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };
    event_type_t event_types[] = {
        {EV_TRANGER_RECORD_ADDED, EVF_PUBLIC_EVENT},
        {0, 0}
    };
    hgclass gc = gclass_create(
        C_RTPROBE, event_types, states, &rtprobe_gmt,
        0, rtprobe_attrs, 0, 0, 0, 0, 0
    );
    return gc ? 0 : -1;
}

/***************************************************************************
 *              Test
 *  HACK: return -1 to fail, 0 to ok
 ***************************************************************************/
PRIVATE int do_test(void)
{
    global_result = 0;

    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    /*-------------------------------------------------*
     *      Create the C_TRANGER gobj as master yuno
     *-------------------------------------------------*/
    if(register_c_tranger() != 0 || register_rtprobe() != 0) {
        printf("%s: FAIL (register gclasses)\n", APP);
        return -1;
    }
    hgobj yuno = gobj_create_yuno(
        "tranger",
        C_TRANGER,
        json_pack("{s:s, s:s, s:b}",
            "path", path_root,
            "database", DATABASE,
            "master", 1
        )
    );
    if(!yuno) {
        printf("%s: FAIL (gobj_create_yuno)\n", APP);
        return -1;
    }

    /*  A probe subscribed to the tranger's realtime record event.  */
    hgobj probe = gobj_create("rtprobe", C_RTPROBE, 0, yuno);
    if(!probe) {
        printf("%s: FAIL (probe create)\n", APP);
        return -1;
    }
    gobj_subscribe_event(yuno, EV_TRANGER_RECORD_ADDED, 0, probe);

    /*-------------------------------------------------*
     *      Fill the topic through the gobj's tranger
     *-------------------------------------------------*/
    json_t *tranger = gobj_read_pointer_attr(yuno, "tranger");
    if(!tranger) {
        printf("%s: FAIL (no tranger handle)\n", APP);
        return -1;
    }
    if(create_topic(tranger) < 0) {
        printf("%s: FAIL (create_topic)\n", APP);
        return -1;
    }
    if(append_key(tranger, KEY_A, KEY_A_ROWS) < 0 ||
       append_key(tranger, KEY_B, KEY_B_ROWS) < 0) {
        printf("%s: FAIL (append)\n", APP);
        return -1;
    }

    json_t *r;
    json_t *data;

    /*-------------------------------------------------*
     *      list-keys: both keys with their counts
     *-------------------------------------------------*/
    r = gobj_command(yuno, "list-keys",
        json_pack("{s:s}", "topic_name", TOPIC_NAME), yuno);
    check_int("list-keys result", kw_get_int(0, r, "result", -999, 0), 0);
    data = kw_get_list(0, r, "data", 0, 0);
    check_int("list-keys count", json_array_size(data), 2);
    check_int("list-keys A records", find_key_records(data, KEY_A), KEY_A_ROWS);
    check_int("list-keys B records", find_key_records(data, KEY_B), KEY_B_ROWS);
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      open-iterator on key A -> total_rows 5
     *-------------------------------------------------*/
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itA",
            "topic_name", TOPIC_NAME,
            "key", KEY_A
        ), yuno);
    check_int("open-iterator result", kw_get_int(0, r, "result", -999, 0), 0);
    data = kw_get_dict(0, r, "data", 0, 0);
    check_str("open-iterator id", kw_get_str(0, data, "iterator_id", "", 0), "itA");
    check_int("open-iterator total_rows", kw_get_int(0, data, "total_rows", -1, 0), KEY_A_ROWS);
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      get-page: page 1 (rowids 1,2)
     *-------------------------------------------------*/
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itA",
            "from_rowid", 1,
            "limit", 2
        ), yuno);
    check_int("get-page p1 result", kw_get_int(0, r, "result", -999, 0), 0);
    data = kw_get_dict(0, r, "data", 0, 0);
    check_int("get-page p1 total_rows", kw_get_int(0, data, "total_rows", -1, 0), KEY_A_ROWS);
    check_int("get-page p1 pages", kw_get_int(0, data, "pages", -1, 0), 3);  // ceil(5/2)
    {
        json_t *page = kw_get_list(0, data, "data", 0, 0);
        check_int("get-page p1 len", json_array_size(page), 2);
        check_int("get-page p1 rowid[0]", record_rowid(json_array_get(page, 0)), 1);
        check_int("get-page p1 rowid[1]", record_rowid(json_array_get(page, 1)), 2);
    }
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      get-page: page 2 (rowids 3,4)
     *-------------------------------------------------*/
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itA",
            "from_rowid", 3,
            "limit", 2
        ), yuno);
    data = kw_get_dict(0, r, "data", 0, 0);
    {
        json_t *page = kw_get_list(0, data, "data", 0, 0);
        check_int("get-page p2 len", json_array_size(page), 2);
        check_int("get-page p2 rowid[0]", record_rowid(json_array_get(page, 0)), 3);
        check_int("get-page p2 rowid[1]", record_rowid(json_array_get(page, 1)), 4);
    }
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      get-page: page 3 (rowid 5, short page)
     *-------------------------------------------------*/
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itA",
            "from_rowid", 5,
            "limit", 2
        ), yuno);
    data = kw_get_dict(0, r, "data", 0, 0);
    {
        json_t *page = kw_get_list(0, data, "data", 0, 0);
        check_int("get-page p3 len", json_array_size(page), 1);
        check_int("get-page p3 rowid[0]", record_rowid(json_array_get(page, 0)), 5);
    }
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      get-page: out-of-range -> empty page, no error
     *-------------------------------------------------*/
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itA",
            "from_rowid", 99,
            "limit", 2
        ), yuno);
    check_int("get-page oob result", kw_get_int(0, r, "result", -999, 0), 0);
    data = kw_get_dict(0, r, "data", 0, 0);
    check_int("get-page oob len", json_array_size(kw_get_list(0, data, "data", 0, 0)), 0);
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      open-iterator again same id -> already open (0)
     *-------------------------------------------------*/
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itA",
            "topic_name", TOPIC_NAME,
            "key", KEY_A
        ), yuno);
    check_int("open-iterator dup result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      close-iterator -> 0, then get-page 404
     *-------------------------------------------------*/
    r = gobj_command(yuno, "close-iterator",
        json_pack("{s:s}", "iterator_id", "itA"), yuno);
    check_int("close-iterator result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itA",
            "from_rowid", 1,
            "limit", 2
        ), yuno);
    check_int("get-page after close result", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      Negative: open-iterator without key
     *-------------------------------------------------*/
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s}", "topic_name", TOPIC_NAME), yuno);
    check_int("open-iterator no-key result", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      Negative: list-keys on missing topic
     *-------------------------------------------------*/
    r = gobj_command(yuno, "list-keys",
        json_pack("{s:s}", "topic_name", "does_not_exist"), yuno);
    check_int("list-keys bad-topic result", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      open-iterator on key B (default id = key)
     *-------------------------------------------------*/
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s}",
            "topic_name", TOPIC_NAME,
            "key", KEY_B
        ), yuno);
    check_int("open-iterator B result", kw_get_int(0, r, "result", -999, 0), 0);
    data = kw_get_dict(0, r, "data", 0, 0);
    check_str("open-iterator B id", kw_get_str(0, data, "iterator_id", "", 0), KEY_B);
    check_int("open-iterator B total_rows", kw_get_int(0, data, "total_rows", -1, 0), KEY_B_ROWS);
    JSON_DECREF(r)
    /*  left open on purpose: mt_destroy must close it without leaking  */

    /*-------------------------------------------------*
     *      Realtime feed: open-rt, append, expect a publish
     *-------------------------------------------------*/
    g_rt_count = 0;
    r = gobj_command(yuno, "open-rt",
        json_pack("{s:s, s:s, s:s}",
            "rt_id", "rtA",
            "topic_name", TOPIC_NAME,
            "key", KEY_A
        ), yuno);
    check_int("open-rt result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    /*  a fresh append on key A must fire the rt_mem feed -> publish  */
    append_one(tranger, KEY_A, BASE_T + 1000);
    check_int("rt publish on append", g_rt_count, 1);

    /*  an append on a DIFFERENT key must not reach this key's feed  */
    append_one(tranger, KEY_B, BASE_T + 1001);
    check_int("rt no cross-key publish", g_rt_count, 1);

    /*-------------------------------------------------*
     *      open-rt dup -> already open (0)
     *-------------------------------------------------*/
    r = gobj_command(yuno, "open-rt",
        json_pack("{s:s, s:s, s:s}",
            "rt_id", "rtA",
            "topic_name", TOPIC_NAME,
            "key", KEY_A
        ), yuno);
    check_int("open-rt dup result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      close-rt -> after it, appends no longer publish
     *-------------------------------------------------*/
    r = gobj_command(yuno, "close-rt",
        json_pack("{s:s}", "rt_id", "rtA"), yuno);
    check_int("close-rt result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    g_rt_count = 0;
    append_one(tranger, KEY_A, BASE_T + 1002);
    check_int("no publish after close-rt", g_rt_count, 0);

    /*-------------------------------------------------*
     *      Negatives: open-rt without rt_id; close-rt unknown
     *-------------------------------------------------*/
    r = gobj_command(yuno, "open-rt",
        json_pack("{s:s}", "topic_name", TOPIC_NAME), yuno);
    check_int("open-rt no-id result", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)

    r = gobj_command(yuno, "close-rt",
        json_pack("{s:s}", "rt_id", "nope"), yuno);
    check_int("close-rt unknown result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    /*  leave one rt open on purpose: mt_destroy must close it, no leak  */
    r = gobj_command(yuno, "open-rt",
        json_pack("{s:s, s:s, s:s}",
            "rt_id", "rtLeak",
            "topic_name", TOPIC_NAME,
            "key", KEY_B
        ), yuno);
    JSON_DECREF(r)

    return global_result;
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

    unsigned long memory_check_list[] = {0}; // WARNING: list ended with 0
    set_memory_check_list(memory_check_list);

    gobj_start_up(
        argc, argv,
        NULL,               // jn_global_settings
        NULL,               // persistent_attrs
        command_parser,     // global_command_parser (C_TRANGER has no mt_command_parser)
        NULL,               // global_stats_parser
        NULL,               // global_authz_checker (NULL => gobj_user_has_authz TRUE)
        NULL                // global_authentication_parser
    );
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    int result = do_test();

    /*
     *  gobj_end() destroys the yuno tree (C_TRANGER mt_destroy closes the
     *  still-open iterator and shuts the tranger down), freeing the startup
     *  baseline FIRST — then assert zero residual.
     */
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
