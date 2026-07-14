/****************************************************************************
 *          test_c_tranger.c
 *
 *  Regression coverage for the C_TRANGER cursor-pagination command surface
 *  added on top of the timeranger2 iterator primitives:
 *
 *      - topics         -> names, or (expanded=1) a desc per topic
 *      - list-keys      -> keys of a topic with their record counts + the
 *                          time span of each key on BOTH axes
 *      - open-iterator  -> stateful per-key iterator (index only), including
 *                          the t (persistence) and tm (message) match
 *                          conditions, which are independent and combine
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
#include <testing.h>

#define APP         "test_c_tranger"

#define DATABASE    "tr_c_tranger_cmd"
#define TOPIC_NAME  "topic_cmd"
#define TOPIC_NAME2 "topic_cmd2"    // closed under its open handles (UAF regression)
#define BASE_T      946684800   // 2000-01-01T00:00:00+0000

/*  key "A" gets 5 records, key "B" gets 3 records  */
#define KEY_A       "A"
#define KEY_A_ROWS  5
#define KEY_B       "B"
#define KEY_B_ROWS  3

/*  key "C" gets 4 records whose two timestamps DIVERGE: t (persistence) at
 *  BASE_T+j, tm (message time, the record's tkey field) TM_SKEW seconds
 *  later. A backfill looks like this — and it is the only fixture that can
 *  tell a from_t filter from a from_tm one.  */
#define KEY_C       "C"
#define KEY_C_ROWS  4
#define TM_SKEW     1000

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

PRIVATE void check_bool(const char *name, BOOL got, BOOL expected)
{
    if((got?1:0) != (expected?1:0)) {
        printf("FAIL %-40s got %s expected %s\n",
            name, got?"TRUE":"FALSE", expected?"TRUE":"FALSE");
        global_result += -1;
    } else {
        printf("ok   %-40s (%s)\n", name, got?"TRUE":"FALSE");
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

/*  Look up one field of one key's row in the list-keys data array (records,
 *  fr_t, to_t, fr_tm, to_tm). -1 if the key is absent.  */
PRIVATE json_int_t find_key_field(json_t *jn_data, const char *key, const char *field)
{
    int idx; json_t *entry;
    json_array_foreach(jn_data, idx, entry) {
        const char *k = json_string_value(json_object_get(entry, "key"));
        if(k && strcmp(k, key)==0) {
            return json_integer_value(json_object_get(entry, field));
        }
    }
    return -1;
}

PRIVATE json_int_t find_key_records(json_t *jn_data, const char *key)
{
    return find_key_field(jn_data, key, "records");
}

/*  The system_flag a topic reports in `topics expanded=1`. -1 if absent.  */
PRIVATE json_int_t find_topic_system_flag(json_t *jn_data, const char *topic_name)
{
    int idx; json_t *entry;
    json_array_foreach(jn_data, idx, entry) {
        const char *name = json_string_value(json_object_get(entry, "topic_name"));
        if(name && strcmp(name, topic_name)==0) {
            return json_integer_value(json_object_get(entry, "system_flag"));
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

/*  Append `n` records whose t and tm diverge: t = BASE_T+j (the append time
 *  passed to timeranger2) and tm = BASE_T+TM_SKEW+j (the tkey field of the
 *  record itself).  */
PRIVATE int append_key_skewed(json_t *tranger, const char *key, int n)
{
    for(int j=0; j<n; j++) {
        json_t *jn_record = json_pack("{s:s, s:I, s:s}",
            "id", key,
            "tm", (json_int_t)(BASE_T + TM_SKEW + j),
            "content", "backfill"
        );
        md2_record_ex_t md = {0};
        if(tranger2_append_record(tranger, TOPIC_NAME, BASE_T + j, 0, &md, jn_record) < 0) {
            return -1;
        }
    }
    return 0;
}

/*  A SECOND topic, closed later under its open handles (the use-after-free
 *  regression at the end of the test).  */
PRIVATE int create_topic2(json_t *tranger)
{
    json_t *topic = tranger2_create_topic(
        tranger,
        TOPIC_NAME2,
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

PRIVATE int append_key2(json_t *tranger, const char *key, int n)
{
    for(int j=0; j<n; j++) {
        json_t *jn_record = json_pack("{s:s, s:I, s:s}",
            "id", key,
            "tm", (json_int_t)(BASE_T + j),
            "content", "payload"
        );
        md2_record_ex_t md = {0};
        if(tranger2_append_record(tranger, TOPIC_NAME2, BASE_T + j, 0, &md, jn_record) < 0) {
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

/*  Per-feed tally: a record must reach each open feed EXACTLY once, and the
 *  `rt_id` of the publish is what says which feed produced it.  */
PRIVATE int g_rt_keyed = 0;     /*  publishes carrying rt_id "rtKEYED"   */
PRIVATE int g_rt_all = 0;       /*  publishes carrying rt_id "rtALL"     */

GOBJ_DEFINE_GCLASS(C_RTPROBE);

PRIVATE int ac_rt_added(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    g_rt_count++;

    const char *rt_id = kw_get_str(gobj, kw, "rt_id", "", 0);
    if(strcmp(rt_id, "rtKEYED")==0) {
        g_rt_keyed++;
    } else if(strcmp(rt_id, "rtALL")==0) {
        g_rt_all++;
    }

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
       append_key(tranger, KEY_B, KEY_B_ROWS) < 0 ||
       append_key_skewed(tranger, KEY_C, KEY_C_ROWS) < 0) {
        printf("%s: FAIL (append)\n", APP);
        return -1;
    }

    json_t *r;
    json_t *data;

    /*-------------------------------------------------*
     *      topics: names by default
     *-------------------------------------------------*/
    r = gobj_command(yuno, "topics", json_pack("{}"), yuno);
    check_int("topics result", kw_get_int(0, r, "result", -999, 0), 0);
    data = kw_get_list(0, r, "data", 0, 0);
    check_int("topics count", json_array_size(data), 1);
    check_str("topics name", json_string_value(json_array_get(data, 0)), TOPIC_NAME);
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      topics expanded=1: a desc per topic. system_flag is what tells
     *      a client whether t/tm are seconds or milliseconds.
     *-------------------------------------------------*/
    r = gobj_command(yuno, "topics", json_pack("{s:b}", "expanded", 1), yuno);
    check_int("topics expanded result", kw_get_int(0, r, "result", -999, 0), 0);
    data = kw_get_list(0, r, "data", 0, 0);
    check_int("topics expanded count", json_array_size(data), 1);
    check_int("topics expanded system_flag",
        find_topic_system_flag(data, TOPIC_NAME), sf_string_key);
    check_str("topics expanded tkey",
        kw_get_str(0, json_array_get(data, 0), "tkey", "", 0), "tm");
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      list-keys: the keys with their counts
     *-------------------------------------------------*/
    r = gobj_command(yuno, "list-keys",
        json_pack("{s:s}", "topic_name", TOPIC_NAME), yuno);
    check_int("list-keys result", kw_get_int(0, r, "result", -999, 0), 0);
    data = kw_get_list(0, r, "data", 0, 0);
    check_int("list-keys count", json_array_size(data), 3);
    check_int("list-keys A records", find_key_records(data, KEY_A), KEY_A_ROWS);
    check_int("list-keys B records", find_key_records(data, KEY_B), KEY_B_ROWS);
    check_int("list-keys C records", find_key_records(data, KEY_C), KEY_C_ROWS);

    /*  ...and the time span of each key, both axes. Key A was appended with
     *  t == tm; key C is the skewed one, and its two spans must differ by
     *  exactly TM_SKEW — that is the span a client bounds its pickers to.  */
    check_int("list-keys A fr_t",  find_key_field(data, KEY_A, "fr_t"),  BASE_T);
    check_int("list-keys A to_t",  find_key_field(data, KEY_A, "to_t"),  BASE_T + KEY_A_ROWS - 1);
    check_int("list-keys A fr_tm", find_key_field(data, KEY_A, "fr_tm"), BASE_T);
    check_int("list-keys A to_tm", find_key_field(data, KEY_A, "to_tm"), BASE_T + KEY_A_ROWS - 1);

    check_int("list-keys C fr_t",  find_key_field(data, KEY_C, "fr_t"),  BASE_T);
    check_int("list-keys C to_t",  find_key_field(data, KEY_C, "to_t"),  BASE_T + KEY_C_ROWS - 1);
    check_int("list-keys C fr_tm", find_key_field(data, KEY_C, "fr_tm"), BASE_T + TM_SKEW);
    check_int("list-keys C to_tm", find_key_field(data, KEY_C, "to_tm"),
        BASE_T + TM_SKEW + KEY_C_ROWS - 1);
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      list-keys rkey: filter the keys in the SERVER.
     *
     *      Without it a topic with a hundred thousand keys is answered in
     *      full and the client filters what it was handed.
     *-------------------------------------------------*/
    r = gobj_command(yuno, "list-keys",
        json_pack("{s:s, s:s}", "topic_name", TOPIC_NAME, "rkey", "^B$"), yuno);
    check_int("list-keys rkey result", kw_get_int(0, r, "result", -999, 0), 0);
    data = kw_get_list(0, r, "data", 0, 0);
    check_int("list-keys rkey count", json_array_size(data), 1);
    check_int("list-keys rkey B records", find_key_records(data, KEY_B), KEY_B_ROWS);
    check_int("list-keys rkey A absent", find_key_records(data, KEY_A), -1);
    JSON_DECREF(r)

    /*  A regex matching several keys, and one matching none.  */
    r = gobj_command(yuno, "list-keys",
        json_pack("{s:s, s:s}", "topic_name", TOPIC_NAME, "rkey", "A|C"), yuno);
    data = kw_get_list(0, r, "data", 0, 0);
    check_int("list-keys rkey A|C count", json_array_size(data), 2);
    JSON_DECREF(r)

    r = gobj_command(yuno, "list-keys",
        json_pack("{s:s, s:s}", "topic_name", TOPIC_NAME, "rkey", "^nope$"), yuno);
    check_int("list-keys rkey no match result", kw_get_int(0, r, "result", -999, 0), 0);
    check_int("list-keys rkey no match count",
        json_array_size(kw_get_list(0, r, "data", 0, 0)), 0);
    JSON_DECREF(r)

    /*  A malformed regex is a client error, not an empty answer: it must be
     *  told apart from "no key matched".  */
    r = gobj_command(yuno, "list-keys",
        json_pack("{s:s, s:s}", "topic_name", TOPIC_NAME, "rkey", "a[b"), yuno);
    check_bool("list-keys bad rkey fails", kw_get_int(0, r, "result", 0, 0) < 0, TRUE);
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      list-keys paging + order.
     *
     *      With `limit` the answer is the same envelope get-page uses
     *      ({total_rows, pages, data}), so a client pages KEYS exactly as it
     *      pages records — and total_rows counts the MATCHING set, not the
     *      page. Sorting has to happen server-side: a client holding 2 of 3
     *      keys cannot order what it was not given.
     *-------------------------------------------------*/
    r = gobj_command(yuno, "list-keys",
        json_pack("{s:s, s:s, s:I, s:I}",
            "topic_name", TOPIC_NAME,
            "order", "records",
            "from", (json_int_t)1,
            "limit", (json_int_t)2
        ), yuno);
    check_int("list-keys page result", kw_get_int(0, r, "result", -999, 0), 0);
    json_t *page = kw_get_dict(0, r, "data", 0, 0);
    check_int("list-keys page total_rows", kw_get_int(0, page, "total_rows", -1, 0), 3);
    check_int("list-keys page pages", kw_get_int(0, page, "pages", -1, 0), 2);
    data = kw_get_list(0, page, "data", 0, 0);
    check_int("list-keys page size", json_array_size(data), 2);
    /*  order=records ascending: B(3), C(4), A(5) -> the page is B, C  */
    check_str("list-keys page[0] by records",
        kw_get_str(0, json_array_get(data, 0), "key", "", 0), KEY_B);
    check_str("list-keys page[1] by records",
        kw_get_str(0, json_array_get(data, 1), "key", "", 0), KEY_C);
    /*  the page still carries the time span of each key it returns  */
    check_int("list-keys page keeps the span",
        find_key_field(data, KEY_B, "fr_t"), BASE_T);
    JSON_DECREF(r)

    /*  The second page: the remainder, and desc order flips the sort.  */
    r = gobj_command(yuno, "list-keys",
        json_pack("{s:s, s:s, s:b, s:I, s:I}",
            "topic_name", TOPIC_NAME,
            "order", "records",
            "desc", 1,
            "from", (json_int_t)1,
            "limit", (json_int_t)2
        ), yuno);
    data = kw_get_list(0, kw_get_dict(0, r, "data", 0, 0), "data", 0, 0);
    /*  desc: A(5), C(4), B(3) -> the page is A, C  */
    check_str("list-keys desc page[0]",
        kw_get_str(0, json_array_get(data, 0), "key", "", 0), KEY_A);
    check_str("list-keys desc page[1]",
        kw_get_str(0, json_array_get(data, 1), "key", "", 0), KEY_C);
    JSON_DECREF(r)

    /*  `from` past the end is an empty page, not an error.  */
    r = gobj_command(yuno, "list-keys",
        json_pack("{s:s, s:I, s:I}",
            "topic_name", TOPIC_NAME,
            "from", (json_int_t)10,
            "limit", (json_int_t)2
        ), yuno);
    check_int("list-keys page past the end result",
        kw_get_int(0, r, "result", -999, 0), 0);
    page = kw_get_dict(0, r, "data", 0, 0);
    check_int("list-keys page past the end total",
        kw_get_int(0, page, "total_rows", -1, 0), 3);
    check_int("list-keys page past the end size",
        json_array_size(kw_get_list(0, page, "data", 0, 0)), 0);
    JSON_DECREF(r)

    /*  An unknown order is refused, not silently ignored.  */
    r = gobj_command(yuno, "list-keys",
        json_pack("{s:s, s:s}", "topic_name", TOPIC_NAME, "order", "colour"), yuno);
    check_bool("list-keys bad order fails", kw_get_int(0, r, "result", 0, 0) < 0, TRUE);
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      open-iterator on key C filtered by tm: the last 2 records
     *      (tm >= BASE_T+TM_SKEW+2). total_rows reflects the filter.
     *-------------------------------------------------*/
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s, s:I}",
            "iterator_id", "itC_tm",
            "topic_name", TOPIC_NAME,
            "key", KEY_C,
            "from_tm", (json_int_t)(BASE_T + TM_SKEW + 2)
        ), yuno);
    check_int("open-iterator C from_tm result", kw_get_int(0, r, "result", -999, 0), 0);
    data = kw_get_dict(0, r, "data", 0, 0);
    check_int("open-iterator C from_tm total_rows",
        kw_get_int(0, data, "total_rows", -1, 0), KEY_C_ROWS - 2);
    JSON_DECREF(r)

    /*  ...and get-page returns exactly those records: the page is a window
     *  over the MATCHING rows, so from_rowid=1 is the first match (global
     *  rowid 3), never the first record of the key.  */
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itC_tm",
            "from_rowid", 1,
            "limit", 10
        ), yuno);
    check_int("get-page C from_tm result", kw_get_int(0, r, "result", -999, 0), 0);
    data = kw_get_dict(0, r, "data", 0, 0);
    check_int("get-page C from_tm total_rows",
        kw_get_int(0, data, "total_rows", -1, 0), KEY_C_ROWS - 2);
    check_int("get-page C from_tm pages", kw_get_int(0, data, "pages", -1, 0), 1);
    {
        json_t *page = kw_get_list(0, data, "data", 0, 0);
        check_int("get-page C from_tm len", json_array_size(page), KEY_C_ROWS - 2);
        check_int("get-page C from_tm rowid[0]", record_rowid(json_array_get(page, 0)), 3);
        check_int("get-page C from_tm rowid[1]", record_rowid(json_array_get(page, 1)), 4);
    }
    JSON_DECREF(r)

    r = gobj_command(yuno, "close-iterator",
        json_pack("{s:s}", "iterator_id", "itC_tm"), yuno);
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      The SAME bound on the t axis matches NOTHING (key C's t never
     *      reaches BASE_T+TM_SKEW): t and tm are two independent axes, and
     *      the iterator does not confuse them.
     *-------------------------------------------------*/
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s, s:I}",
            "iterator_id", "itC_t",
            "topic_name", TOPIC_NAME,
            "key", KEY_C,
            "from_t", (json_int_t)(BASE_T + TM_SKEW + 2)
        ), yuno);
    check_int("open-iterator C from_t result", kw_get_int(0, r, "result", -999, 0), 0);
    data = kw_get_dict(0, r, "data", 0, 0);
    check_int("open-iterator C from_t total_rows",
        kw_get_int(0, data, "total_rows", -1, 0), 0);
    JSON_DECREF(r)

    r = gobj_command(yuno, "close-iterator",
        json_pack("{s:s}", "iterator_id", "itC_t"), yuno);
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      Both axes at once: the iterator ANDs them. t bounds the first 3
     *      records, tm bounds the last 3 -> their intersection is 2.
     *-------------------------------------------------*/
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s, s:I, s:I}",
            "iterator_id", "itC_both",
            "topic_name", TOPIC_NAME,
            "key", KEY_C,
            "to_t", (json_int_t)(BASE_T + 2),
            "from_tm", (json_int_t)(BASE_T + TM_SKEW + 1)
        ), yuno);
    check_int("open-iterator C both result", kw_get_int(0, r, "result", -999, 0), 0);
    data = kw_get_dict(0, r, "data", 0, 0);
    check_int("open-iterator C both total_rows",
        kw_get_int(0, data, "total_rows", -1, 0), 2);
    JSON_DECREF(r)

    r = gobj_command(yuno, "close-iterator",
        json_pack("{s:s}", "iterator_id", "itC_both"), yuno);
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
     *      open-list with rkey: a LIVE list on a SUBSET of the keys.
     *
     *      This is the one that used to be a lie. rkey was honoured by the
     *      one-shot read (return_data=1) and REFUSED by the live list,
     *      because the realtime feed did not know about it: a list filtered
     *      on load and unfiltered on append would have fed the caller the
     *      records of every key it had asked to exclude.
     *-------------------------------------------------*/
    r = gobj_command(yuno, "open-list",
        json_pack("{s:s, s:s, s:s}",
            "list_id", "lstA",
            "topic_name", TOPIC_NAME,
            "rkey", "^A$"
        ), yuno);
    check_int("open-list rkey result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    /*  The DISK half: only key A's records were loaded.  */
    r = gobj_command(yuno, "get-list-data",
        json_pack("{s:s}", "list_id", "lstA"), yuno);
    data = kw_get_list(0, r, "data", 0, 0);
    check_int("open-list rkey loaded only A", json_array_size(data), KEY_A_ROWS + 2);
    JSON_DECREF(r)

    /*  The REALTIME half, which is the whole point: an append on A reaches the
     *  list, and an append on a key the rkey excludes does NOT.  */
    g_rt_count = 0;
    append_one(tranger, KEY_A, BASE_T + 2000);
    check_int("open-list rkey live publish on A", g_rt_count, 1);

    append_one(tranger, KEY_B, BASE_T + 2001);
    check_int("open-list rkey no live publish on B", g_rt_count, 1);

    r = gobj_command(yuno, "close-list",
        json_pack("{s:s}", "list_id", "lstA"), yuno);
    check_int("close-list rkey result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    /*  A malformed rkey REFUSES the list: degrading it to "every key" would
     *  push the caller exactly the records it asked not to receive.  */
    r = gobj_command(yuno, "open-list",
        json_pack("{s:s, s:s, s:s}",
            "list_id", "lstBad",
            "topic_name", TOPIC_NAME,
            "rkey", "a[b"
        ), yuno);
    check_bool("open-list bad rkey fails", kw_get_int(0, r, "result", 0, 0) < 0, TRUE);
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      Command-LINE parameter form: `open-list ... return_data=1` with
     *      every parameter inline, the way ycommand sends it. A kw dict is
     *      merged into the parsed kw_cmd afterwards (update_missing), so it
     *      cannot catch a parameter missing from pm_open_list — this can:
     *      the parser refuses any inline key not in the table ("command has
     *      no option '...'"). return_data was once dropped from the table by
     *      an edit that replaced the wrong line, and only this form broke.
     *-------------------------------------------------*/
    r = gobj_command(yuno,
        "open-list list_id=lstInline topic_name=" TOPIC_NAME " key=" KEY_A " return_data=1",
        json_object(), yuno);
    check_int("open-list inline return_data result", kw_get_int(0, r, "result", -999, 0), 0);
    data = kw_get_list(0, r, "data", 0, 0);
    check_int("open-list inline return_data rows", json_array_size(data), KEY_A_ROWS + 3);
    JSON_DECREF(r)

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

    /*-------------------------------------------------*
     *      A CLOSED TOPIC takes its handles with it.
     *
     *  A topic OWNS the iterators / rt feeds opened on it: closing the topic
     *  closes and FREES them all. C_TRANGER registers what a remote client
     *  opened, and used to keep the raw pointer — so after a close-topic its
     *  registry pointed at freed memory, and the next close (at the latest,
     *  its own mt_destroy) was a use-after-free. It crashed a yuno with
     *  SIGSEGV on every shutdown that had a Rows card open (close-treedb
     *  frees the topics, THEN the C_TRANGER gobj is destroyed).
     *
     *  Open a fresh pair on a second topic, close the topic under them, and
     *  ask C_TRANGER to close them: it must answer that they are already
     *  gone instead of dereferencing dead memory. mt_destroy then sweeps the
     *  registry of the FIRST topic (iterator "B" + rtLeak, still open), so
     *  both paths run in one test.
     *-------------------------------------------------*/
    if(create_topic2(tranger) < 0) {
        printf("%s: FAIL (create_topic2)\n", APP);
        return -1;
    }
    if(append_key2(tranger, KEY_A, KEY_A_ROWS) < 0) {
        printf("%s: FAIL (append topic2)\n", APP);
        return -1;
    }

    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itDead",
            "topic_name", TOPIC_NAME2,
            "key", KEY_A
        ), yuno);
    check_int("open-iterator on topic2", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    r = gobj_command(yuno, "open-rt",
        json_pack("{s:s, s:s, s:s}",
            "rt_id", "rtDead",
            "topic_name", TOPIC_NAME2,
            "key", KEY_A
        ), yuno);
    check_int("open-rt on topic2", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    /*  The topic goes away UNDER the open handles (this is what
     *  close-treedb does to a treedb's topics at shutdown).  */
    if(tranger2_close_topic(tranger, TOPIC_NAME2) < 0) {
        printf("%s: FAIL (close_topic2)\n", APP);
        global_result += -1;
    }

    /*  From here on, touching one of those handles must be SILENT: the guard
     *  sees the topic is gone and drops the entry. Without it, the code
     *  dereferences freed memory — which does not reliably crash (the block
     *  may still read back fine), but it ALWAYS goes through the tranger and
     *  logs ("Cannot open topic", "iterators not found"). So assert on the
     *  logs: zero errors is the invariant, and it fails without the fix.  */
    set_expected_results(
        "handles of a closed topic are dropped, not dereferenced",
        NULL,   // no errors expected
        NULL, NULL, 1
    );

    /*  Reaching into the registry now must NOT touch the freed handles.  */
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itDead",
            "from_rowid", 1,
            "limit", 10
        ), yuno);
    check_int("get-page on a dead topic", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)

    r = gobj_command(yuno, "close-iterator",
        json_pack("{s:s}", "iterator_id", "itDead"), yuno);
    check_int("close-iterator on a dead topic", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    global_result += test_json(NULL);

    /*  `rtDead` is left REGISTERED on the closed topic on purpose: it is the
     *  crash as it happened in production — mt_destroy sweeping a registry
     *  entry whose handle the tranger already freed. It must drop the entry
     *  without touching it (gobj_end() runs it, and the leak check that
     *  follows would catch a handle left behind).  */

    /*-------------------------------------------------*
     *      TWO feeds over the SAME key: one opened on the key, one on the
     *      WHOLE topic (empty key). An append on that key belongs to both,
     *      and each of them must receive it EXACTLY ONCE.
     *
     *      This is what a browser does with a per-key Live card and a
     *      "Live topic" card open at the same time; the rows appeared
     *      DUPLICATED in both.
     *-------------------------------------------------*/
    r = gobj_command(yuno, "open-rt",
        json_pack("{s:s, s:s, s:s}",
            "rt_id", "rtKEYED",
            "topic_name", TOPIC_NAME,
            "key", KEY_A
        ), yuno);
    check_int("open-rt keyed result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    r = gobj_command(yuno, "open-rt",
        json_pack("{s:s, s:s, s:s}",
            "rt_id", "rtALL",
            "topic_name", TOPIC_NAME,
            "key", ""           /*  empty = every key of the topic  */
        ), yuno);
    check_int("open-rt keyless result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    g_rt_count = 0;
    g_rt_keyed = 0;
    g_rt_all = 0;
    append_one(tranger, KEY_A, BASE_T + 1003);
    check_int("shared key: keyed feed gets it once",   g_rt_keyed, 1);
    check_int("shared key: keyless feed gets it once", g_rt_all,   1);

    /*  a key only the KEYLESS feed wants: it must arrive once, to it alone  */
    g_rt_count = 0;
    g_rt_keyed = 0;
    g_rt_all = 0;
    append_one(tranger, KEY_B, BASE_T + 1004);
    check_int("other key: keyless feed gets it once", g_rt_all,   1);
    check_int("other key: keyed feed untouched",      g_rt_keyed, 0);

    r = gobj_command(yuno, "close-rt",
        json_pack("{s:s}", "rt_id", "rtKEYED"), yuno);
    check_int("close-rt keyed result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-rt",
        json_pack("{s:s}", "rt_id", "rtALL"), yuno);
    check_int("close-rt keyless result", kw_get_int(0, r, "result", -999, 0), 0);
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

    /*  Captures ERROR logs so a phase can assert it emitted NONE (see the
     *  closed-topic phase: touching a freed handle always goes through the
     *  tranger and logs an error, so silence is the proof it was not touched).
     *  Errors only: this test's yuno IS the C_TRANGER, so it has no
     *  `yuno_role_plus_name` attr and every command response that names the
     *  yuno emits a harmless warning no real yuno emits.  */
    gobj_log_register_handler(
        "testing", 0, capture_log_write, 0
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_ERROR, 0);

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
