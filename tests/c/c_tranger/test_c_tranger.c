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
 *      - open-iterator rkey -> several keys concatenated in key order
 *      - get-page       -> {total_rows, pages, data} page of records
 *      - close-iterator -> close + deregister
 *      - delete-key     -> delete a whole key, guarded by force
 *      - add-record     -> append a record: `write`, master only, validated
 *                          (it was a stub that answered "Pending to review")
 *      - the pushes of a live list carry its id as `rt_id`, the id a
 *        subscriber filters on (they carried none)
 *      - a handle of one session is refused to another session: close-rt,
 *        close-iterator, close-list, get-page and get-list-data answer -403
 *        (any session could close or read another's by its id); through
 *        the agent's one link (a C_IEVENT_CLI) it is refused to another
 *        user (every relayed command was trusted)
 *      - add-record asks `write`, and answers -403 on a refusal (a
 *        counting authz checker, installed at the start up)
 *      - the data of a peer is refused with a WARNING and -1 before the
 *        tranger logs its ERROR: an unknown topic or a name with '/' or
 *        '..', a system topic to delete, a create-topic of a wrong shape
 *        (unknown system_flag, two key types, no pkey, jn_cols not a dict,
 *        a name longer than NAME_MAX), a record without its pkey or with
 *        a pkey that is a path, a __t__ that is no date; a replica answers
 *        READ-ONLY to delete-key, delete-topic and create-topic
 *      - more of it (REV-CLOUD V20): a topic named as a file of the
 *        database, a new topic_version over a system topic, a record that
 *        is not a dict, a key that is a path (open-iterator, open-list,
 *        open-rt), a time bound that is a dict (and an integer one, taken),
 *        a print-tranger path that is not there, of any length, answered
 *        with no log (and a string at the end, answered); a one-shot
 *        open-list named as the service reads a key the service has an
 *        iterator open on; check-json asks `read`, a -403 names the yuno
 *        first, and it ANSWERS what it finds, logging nothing; a new
 *        topic_version of a topic that exists without its jn_cols (its
 *        columns were written empty) or over a topic of a treedb (its schema
 *        is the treedb's), while a plain topic with its jn_cols still takes
 *        it; a __t__ or a tkey time of more than 44 bits (what an md2 row
 *        keeps); a print-tranger path of a wrong type is sent as a peer's
 *        (the parser refuses a C caller's with an ERROR)
 *      - the data of a peer is sent as a peer's (`__username__`): its
 *        refusals are WARNINGs, a C caller's an ERROR with a stack (an
 *        unknown topic from C); a peer writes nothing into a topic of a
 *        treedb (add-record, delete-key, delete-topic) nor makes a new topic
 *        in its store, while a C caller's add-record there is taken; a new
 *        topic named as what a store keeps (`.hidden`, `__x`, `x.json`,
 *        `x.bak`); a jn_var of what the tranger keeps or types
 *        (system_topic, last_rowid_id "x", directory, topic_version "2");
 *        a topic with no columns by design takes a new topic_version with
 *        an empty jn_cols, and without jn_cols it is refused
 *      - add-record of a purged period is refused before the tranger for a
 *        key flagged unlisted too (its base unreadable at the open)
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
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

#include <yunetas.h>
#include <testing.h>

#define APP         "test_c_tranger"

#define DATABASE    "tr_c_tranger_cmd"
#define TOPIC_NAME  "topic_cmd"
#define TOPIC_NAME2 "topic_cmd2"    // closed under its open handles (UAF regression)
#define TOPIC_NAME3 "topic_cmd3"    // deleted by command with force=1 as an integer
#define TOPIC_NAME4 "topic_cmd4"    // a no_rt list outlives its topic (leak regression)
#define TOPIC_NAME5 "topic_cmd5"    // one file a day: delete-key-files
#define TOPIC_NAME6 "topic_cmd6"    // a file name mask that does not sort as time
#define TOPIC_NAME7 "topic_cmd7"    // a key flagged unlisted: add-record of a purged period
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

/*  key "D" exists only to be deleted: nothing else reads it, so delete-key
 *  cannot disturb the iterators / feeds the other keys leave open.  */
#define KEY_D       "D"
#define KEY_D_ROWS  6

/*  key "E" is paged BACKWARD while it grows; key "F" is deleted and born
 *  again under a multi-key iterator.  */
#define KEY_E       "E"
#define KEY_F       "F"

/*  key "G" is paged backward by a MULTI-key iterator while it grows; key
 *  "H" is deleted beside it, and is none of that iterator's business.  */
#define KEY_G       "G"
#define KEY_H       "H"

/*  keys "I" and "J" pin what a multi-key iterator does not promise: a
 *  filtered key's count is the open's, and a forward page shifts after an
 *  append to an earlier key.  */
#define KEY_I       "I"
#define KEY_J       "J"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int global_result = 0;

/*
 *  The authz checker of the test: every permission is granted, except the
 *  one named by g_deny_authz. It remembers the last permission asked.
 */
PRIVATE const char *g_deny_authz = NULL;
PRIVATE char g_last_authz[64] = "";

PRIVATE BOOL test_authz_checker(hgobj gobj, const char *authz, json_t *kw, hgobj src)
{
    snprintf(g_last_authz, sizeof(g_last_authz), "%s", authz? authz: "");
    BOOL allow = (g_deny_authz && authz && strcmp(authz, g_deny_authz) == 0)? FALSE: TRUE;
    KW_DECREF(kw)
    return allow;
}

/*
 *  A handle of one session asked by another is refused with a WARNING
 */
PRIVATE int g_not_owner_said = 0;
PRIVATE int count_not_owner(void *h, int priority, const char *bf, size_t len)
{
    if(strstr(bf, "Handle of another session, refused")) {
        g_not_owner_said++;
    }
    return 0;
}

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

/*  A refusal of a peer's data: its WARNING, then the one the answer's
 *  gobj_yuno_role_plus_name() logs in this test, whose yuno is the
 *  C_TRANGER (see main()). The phase that expects them captures warnings.  */
PRIVATE void expect_refusal(json_t *list, const char *msg)
{
    json_array_append_new(list, json_pack("{s:s, s:s}", "msg", msg, "__priority__", "WARNING"));
    json_array_append_new(list, json_pack("{s:s, s:s}",
        "msg", "GClass Attribute NOT FOUND", "__priority__", "WARNING"));
}

/*  `n` answers that name the yuno, each one the warning of
 *  gobj_yuno_role_plus_name() in this test (see expect_refusal()), for a
 *  phase that captures warnings  */
PRIVATE json_t *expect_role_warnings(int n)
{
    json_t *list = json_array();
    for(int i = 0; i < n; i++) {
        json_array_append_new(list, json_pack("{s:s, s:s}",
            "msg", "GClass Attribute NOT FOUND", "__priority__", "WARNING"));
    }
    return list;
}

/*  The kw of a command as C_IEVENT_SRV delivers it from a peer: with the
 *  `__username__` it stamps. Its refusals are WARNINGs; without it the
 *  caller is C code, and they are ERRORs with a stack (REV-CLOUD V20 #17)  */
#define PEER_USER   "peer@test"

PRIVATE json_t *as_peer(json_t *kw)  // kw owned, returned
{
    json_object_set_new(kw, "__username__", json_string(PEER_USER));
    return kw;
}

/*  The command answers -1 and says why  */
PRIVATE void check_refused(const char *name, json_t *r)
{
    check_int(name, kw_get_int(0, r, "result", -999, 0), -1);
    check_bool(name, !empty_string(kw_get_str(0, r, "comment", "", 0)), TRUE);
    JSON_DECREF(r)
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
PRIVATE int g_rt_watch_id = 0;  /*  publishes carrying rt_id "itW^__keys__"  */
PRIVATE int g_rt_list = 0;      /*  publishes carrying rt_id "lstA" (a live list)  */

/*  The probe is also the PARENT of the fake session below: a C_IEVENT_SRV
 *  subscribes its parent to everything it publishes (CHILD model), so the
 *  parent must declare EV_ON_CLOSE. Counting it is the assertion that the
 *  session's close was published once.  */
PRIVATE int g_session_closed = 0;

GOBJ_DEFINE_GCLASS(C_RTPROBE);

PRIVATE int ac_session_closed(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    g_session_closed++;
    KW_DECREF(kw)
    return 0;
}

PRIVATE int ac_rt_added(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    g_rt_count++;

    const char *rt_id = kw_get_str(gobj, kw, "rt_id", "", 0);
    if(strcmp(rt_id, "rtKEYED")==0) {
        g_rt_keyed++;
    } else if(strcmp(rt_id, "rtALL")==0) {
        g_rt_all++;
    } else if(strcmp(rt_id, "itW^__keys__")==0) {
        g_rt_watch_id++;
    } else if(strcmp(rt_id, "lstA")==0) {
        g_rt_list++;
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
        {EV_ON_CLOSE, ac_session_closed, 0},
        {0, 0, 0}
    };
    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };
    event_type_t event_types[] = {
        {EV_TRANGER_RECORD_ADDED, EVF_PUBLIC_EVENT},
        {EV_ON_CLOSE, 0},
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
    if(register_c_tranger() != 0 || register_rtprobe() != 0 ||
            register_c_timer() != 0 || register_c_ievent_srv() != 0 ||
            register_c_ievent_cli() != 0) {
        printf("%s: FAIL (register gclasses)\n", APP);
        return -1;
    }
    hgobj yuno = gobj_create_yuno(
        "tranger",
        C_TRANGER,
        json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            /*
             *  Not the gclass default (2 = exit(0)): a critical must FAIL
             *  this test, and an exit(0) in the middle of it reads as a pass.
             */
            "on_critical_error", LOG_OPT_TRACE_STACK
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
        as_peer(json_pack("{s:s, s:s}", "topic_name", TOPIC_NAME, "rkey", "a[b")), yuno);
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

    /*  The direction given at the OPEN is the pages' default: without it,
     *  `open-iterator backward=1` does nothing, and a get-page that does
     *  not say reads the key oldest first.  */
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s, s:b}",
            "iterator_id", "itC_back",
            "topic_name", TOPIC_NAME,
            "key", KEY_C,
            "backward", 1
        ), yuno);
    check_int("open-iterator C backward result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itC_back",
            "from_rowid", 1,
            "limit", 1
        ), yuno);
    {
        json_t *page = kw_get_list(0, kw_get_dict(0, r, "data", 0, 0), "data", 0, 0);
        check_int("get-page of an iterator opened backward: newest first",
            record_rowid(json_array_get(page, 0)), KEY_C_ROWS);
    }
    JSON_DECREF(r)
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i, s:b}",
            "iterator_id", "itC_back",
            "from_rowid", 1,
            "limit", 1,
            "backward", 0
        ), yuno);
    {
        json_t *page = kw_get_list(0, kw_get_dict(0, r, "data", 0, 0), "data", 0, 0);
        check_int("get-page that says forward reads forward",
            record_rowid(json_array_get(page, 0)), 1);
    }
    JSON_DECREF(r)
    /*  An UNFILTERED key backward counts from the END, like a filtered or a
     *  multi-key one: page 2 of 2 rows is rows 2 and 1 from the end.  */
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i, s:b}",
            "iterator_id", "itC_back",
            "from_rowid", 3,
            "limit", 2,
            "backward", 1
        ), yuno);
    {
        json_t *page = kw_get_list(0, kw_get_dict(0, r, "data", 0, 0), "data", 0, 0);
        check_int("backward page 2 len", json_array_size(page), 2);
        check_int("backward page 2 rowid[0]", record_rowid(json_array_get(page, 0)), 2);
        check_int("backward page 2 rowid[1]", record_rowid(json_array_get(page, 1)), 1);
    }
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-iterator",
        json_pack("{s:s}", "iterator_id", "itC_back"), yuno);
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      open-iterator with rkey: keys A, B, C laid end to end in key
     *      order (5 + 3 + 4 = 12). A page that straddles two keys comes
     *      back in that order, each record naming its key.
     *-------------------------------------------------*/
    /*  The parts are not held: a multi-key iterator keeps a row count per
     *  key, and opens a part only while a page reads it. One full tranger2
     *  iterator per key for its whole life cost 1000 keys x 60 daily files,
     *  +147 MB for one whole-topic card.  */
    size_t iterators_before = json_array_size(
        kw_get_list(0, tranger2_topic(tranger, TOPIC_NAME), "iterators", 0, 0));
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itABC",
            "topic_name", TOPIC_NAME,
            "rkey", "^[ABC]$"
        ), yuno);
    check_int("open-iterator rkey result", kw_get_int(0, r, "result", -999, 0), 0);
    check_int("open-iterator rkey holds no tranger2 iterator",
        (int)json_array_size(kw_get_list(0, tranger2_topic(tranger, TOPIC_NAME), "iterators", 0, 0)),
        (int)iterators_before);
    data = kw_get_dict(0, r, "data", 0, 0);
    check_int("open-iterator rkey total_rows", kw_get_int(0, data, "total_rows", -1, 0),
        KEY_A_ROWS + KEY_B_ROWS + KEY_C_ROWS);
    check_int("open-iterator rkey keys", kw_get_int(0, data, "keys", -1, 0), 3);
    JSON_DECREF(r)

    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itABC",
            "from_rowid", 4,
            "limit", 4
        ), yuno);
    check_int("get-page rkey result", kw_get_int(0, r, "result", -999, 0), 0);
    data = kw_get_dict(0, r, "data", 0, 0);
    check_int("get-page rkey total_rows", kw_get_int(0, data, "total_rows", -1, 0),
        KEY_A_ROWS + KEY_B_ROWS + KEY_C_ROWS);
    check_int("get-page rkey pages", kw_get_int(0, data, "pages", -1, 0), 3);
    check_int("get-page rkey leaves no tranger2 iterator",
        (int)json_array_size(kw_get_list(0, tranger2_topic(tranger, TOPIC_NAME), "iterators", 0, 0)),
        (int)iterators_before);
    {
        json_t *page = kw_get_list(0, data, "data", 0, 0);
        static const char *keys[] = {KEY_A, KEY_A, KEY_B, KEY_B};
        static const json_int_t rowids[] = {4, 5, 1, 2};
        check_int("get-page rkey len", json_array_size(page), 4);
        for(int i = 0; i < 4; i++) {
            json_t *rec = json_array_get(page, (size_t)i);
            check_str("get-page rkey key",
                kw_get_str(0, rec, "__md_tranger__`key", "", 0), keys[i]);
            check_int("get-page rkey rowid", record_rowid(rec), rowids[i]);
        }
    }
    JSON_DECREF(r)

    /*  Backward counts from the END of the concatenation: key C, newest
     *  first.  */
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i, s:b}",
            "iterator_id", "itABC",
            "from_rowid", 1,
            "limit", 2,
            "backward", 1
        ), yuno);
    data = kw_get_dict(0, r, "data", 0, 0);
    {
        json_t *page = kw_get_list(0, data, "data", 0, 0);
        check_int("get-page rkey backward len", json_array_size(page), 2);
        check_str("get-page rkey backward key[0]",
            kw_get_str(0, json_array_get(page, 0), "__md_tranger__`key", "", 0), KEY_C);
        check_int("get-page rkey backward rowid[0]",
            record_rowid(json_array_get(page, 0)), KEY_C_ROWS);
        check_int("get-page rkey backward rowid[1]",
            record_rowid(json_array_get(page, 1)), KEY_C_ROWS - 1);
    }
    JSON_DECREF(r)

    r = gobj_command(yuno, "close-iterator",
        json_pack("{s:s}", "iterator_id", "itABC"), yuno);
    check_int("close-iterator rkey result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    /*  The match conditions bound EVERY key alike: t <= BASE_T+1 keeps the
     *  first 2 records of each.  */
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s, s:I}",
            "iterator_id", "itABC_t",
            "topic_name", TOPIC_NAME,
            "rkey", "^[ABC]$",
            "to_t", (json_int_t)(BASE_T + 1)
        ), yuno);
    data = kw_get_dict(0, r, "data", 0, 0);
    check_int("open-iterator rkey to_t total_rows", kw_get_int(0, data, "total_rows", -1, 0), 6);
    JSON_DECREF(r)

    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itABC_t",
            "from_rowid", 3,
            "limit", 2
        ), yuno);
    data = kw_get_dict(0, r, "data", 0, 0);
    {
        json_t *page = kw_get_list(0, data, "data", 0, 0);
        check_str("get-page rkey to_t key[0]",
            kw_get_str(0, json_array_get(page, 0), "__md_tranger__`key", "", 0), KEY_B);
        check_int("get-page rkey to_t rowid[1]", record_rowid(json_array_get(page, 1)), 2);
    }
    JSON_DECREF(r)

    r = gobj_command(yuno, "close-iterator",
        json_pack("{s:s}", "iterator_id", "itABC_t"), yuno);
    JSON_DECREF(r)

    /*  key and rkey are exclusive; a malformed rkey is refused.  */
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s, s:s}",
            "iterator_id", "itBoth",
            "topic_name", TOPIC_NAME,
            "key", KEY_A,
            "rkey", ".*"
        ), yuno);
    check_bool("open-iterator key+rkey fails", kw_get_int(0, r, "result", 0, 0) < 0, TRUE);
    JSON_DECREF(r)

    r = gobj_command(yuno, "open-iterator",
        as_peer(json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itBad",
            "topic_name", TOPIC_NAME,
            "rkey", "a[b"
        )), yuno);
    check_bool("open-iterator bad rkey fails", kw_get_int(0, r, "result", 0, 0) < 0, TRUE);
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
     *      open-iterator again same id -> already open (-1: an open that
     *      opens nothing is not a success, since the 2026-09-22 lows)
     *-------------------------------------------------*/
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itA",
            "topic_name", TOPIC_NAME,
            "key", KEY_A
        ), yuno);
    check_int("open-iterator dup result", kw_get_int(0, r, "result", -999, 0), -1);
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
        as_peer(json_pack("{s:s}", "topic_name", "does_not_exist")), yuno);
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
     *      open-rt dup -> already open (-1)
     *-------------------------------------------------*/
    r = gobj_command(yuno, "open-rt",
        json_pack("{s:s, s:s, s:s}",
            "rt_id", "rtA",
            "topic_name", TOPIC_NAME,
            "key", KEY_A
        ), yuno);
    check_int("open-rt dup result", kw_get_int(0, r, "result", -999, 0), -1);
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
    g_rt_list = 0;
    append_one(tranger, KEY_A, BASE_T + 2000);
    check_int("open-list rkey live publish on A", g_rt_count, 1);
    check_int("the live list's push carries its id as rt_id", g_rt_list, 1);

    append_one(tranger, KEY_B, BASE_T + 2001);
    check_int("open-list rkey no live publish on B", g_rt_count, 1);

    r = gobj_command(yuno, "close-list",
        json_pack("{s:s}", "list_id", "lstA"), yuno);
    check_int("close-list rkey result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    /*  A malformed rkey REFUSES the list: degrading it to "every key" would
     *  push the caller exactly the records it asked not to receive.  */
    r = gobj_command(yuno, "open-list",
        as_peer(json_pack("{s:s, s:s, s:s}",
            "list_id", "lstBad",
            "topic_name", TOPIC_NAME,
            "rkey", "a[b"
        )), yuno);
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
     *      A negative bound over several keys is relative to EACH key's
     *      last record: A ends at BASE_T+2000, C at BASE_T+3, and
     *      from_t=-1 takes the last row of each. The keys shared one
     *      match_cond, and the first key's resolved bound was the other's:
     *      1 row (A first) or 6 (C first).
     *-------------------------------------------------*/
    r = gobj_command(yuno, "open-list",
        json_pack("{s:s, s:s, s:s, s:s, s:b}",
            "list_id", "lstNeg",
            "topic_name", TOPIC_NAME,
            "rkey", "^(A|C)$",
            "from_t", "-1",
            "return_data", 1
        ), yuno);
    check_int("open-list negative from_t per key result", kw_get_int(0, r, "result", -999, 0), 0);
    data = kw_get_list(0, r, "data", 0, 0);
    check_int("open-list negative from_t: the last row of each key", json_array_size(data), 2);
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
     *      delete-key: the write list-keys had no counterpart for.
     *
     *  A key of its own (nothing before this point has ever looked at it), so
     *  the delete cannot disturb the iterators and feeds left open above.
     *-------------------------------------------------*/
    if(append_key(tranger, KEY_D, KEY_D_ROWS) < 0) {
        printf("%s: FAIL (append KEY_D)\n", APP);
        return -1;
    }

    r = gobj_command(yuno, "delete-key",
        json_pack("{s:s}", "topic_name", TOPIC_NAME), yuno);
    check_int("delete-key no-key result", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)

    r = gobj_command(yuno, "delete-key",
        json_pack("{s:s, s:s}",
            "topic_name", TOPIC_NAME,
            "key", "never_existed"
        ), yuno);
    check_int("delete-key missing-key result", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)

    /*  A key with records refuses without force: the delete is irrecoverable  */
    r = gobj_command(yuno, "delete-key",
        json_pack("{s:s, s:s}",
            "topic_name", TOPIC_NAME,
            "key", KEY_D
        ), yuno);
    check_int("delete-key no-force result", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)

    r = gobj_command(yuno, "list-keys",
        json_pack("{s:s}", "topic_name", TOPIC_NAME), yuno);
    data = kw_get_list(0, r, "data", 0, 0);
    check_int("delete-key refused, D still there", find_key_records(data, KEY_D), KEY_D_ROWS);
    JSON_DECREF(r)

    /*  Iterators open on D when it is deleted: a filtered one (its own row
     *  index, whose next page opened a file that is gone -- a critical, and
     *  exit(0) with the gclass default), an unfiltered one (it answered a
     *  short page with the old total_rows), and a multi-key one. One on A is
     *  the control: another key's delete does not touch it.  */
    const char *it_on_d[] = {"itD_filtered", "itD_plain", "itD_multi", NULL};
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s, s:i}",
            "topic_name", TOPIC_NAME,
            "key", KEY_D,
            "iterator_id", "itD_filtered",
            "from_rowid", 2
        ), yuno);
    check_int("open-iterator D filtered result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "topic_name", TOPIC_NAME,
            "key", KEY_D,
            "iterator_id", "itD_plain"
        ), yuno);
    check_int("open-iterator D plain result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "topic_name", TOPIC_NAME,
            "rkey", "^D$",
            "iterator_id", "itD_multi"
        ), yuno);
    check_int("open-iterator D multi result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s, s:i}",
            "topic_name", TOPIC_NAME,
            "key", KEY_A,
            "iterator_id", "itA_control",
            "from_rowid", 2
        ), yuno);
    check_int("open-iterator A control result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    r = gobj_command(yuno, "delete-key",
        json_pack("{s:s, s:s, s:b}",
            "topic_name", TOPIC_NAME,
            "key", KEY_D,
            "force", 1
        ), yuno);
    check_int("delete-key force result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    for(int i = 0; it_on_d[i]; i++) {
        char name[NAME_MAX];
        r = gobj_command(yuno, "get-page",
            json_pack("{s:s, s:i, s:i}",
                "iterator_id", it_on_d[i],
                "from_rowid", 1,
                "limit", 10
            ), yuno);
        snprintf(name, sizeof(name), "get-page %s after delete-key result", it_on_d[i]);
        check_int(name, kw_get_int(0, r, "result", -999, 0), -1);
        snprintf(name, sizeof(name), "get-page %s after delete-key says why", it_on_d[i]);
        check_bool(name,
            strstr(kw_get_str(0, r, "comment", "", 0), "was deleted") != NULL, TRUE);
        JSON_DECREF(r)

        /*  ...and it is closed: asked again, it is not there  */
        r = gobj_command(yuno, "get-page",
            json_pack("{s:s}", "iterator_id", it_on_d[i]), yuno);
        snprintf(name, sizeof(name), "get-page %s closed", it_on_d[i]);
        check_bool(name,
            strstr(kw_get_str(0, r, "comment", "", 0), "Iterator not found") != NULL, TRUE);
        JSON_DECREF(r)
    }

    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itA_control",
            "from_rowid", 1,
            "limit", 10
        ), yuno);
    check_int("get-page A control after delete of D", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-iterator",
        json_pack("{s:s}", "iterator_id", "itA_control"), yuno);
    JSON_DECREF(r)

    r = gobj_command(yuno, "list-keys",
        json_pack("{s:s}", "topic_name", TOPIC_NAME), yuno);
    check_int("delete-key list-keys result", kw_get_int(0, r, "result", -999, 0), 0);
    data = kw_get_list(0, r, "data", 0, 0);
    check_int("delete-key D is gone", find_key_records(data, KEY_D), -1);
    /*  A took appends from the realtime sections above, so assert it is still
     *  THERE rather than a count: what matters is that one key's delete took
     *  only that key.  */
    check_bool("delete-key A untouched", find_key_records(data, KEY_A) > 0, TRUE);
    JSON_DECREF(r)

    /*  Deleting it twice is an error, not a second success  */
    r = gobj_command(yuno, "delete-key",
        json_pack("{s:s, s:s, s:b}",
            "topic_name", TOPIC_NAME,
            "key", KEY_D,
            "force", 1
        ), yuno);
    check_int("delete-key twice result", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)

    /*-------------------------------------------------*
     *      delete-topic: `force=1` arrives as an INTEGER.
     *
     *  The command line turns `force=1` into the json integer 1, and
     *  kw_get_bool() without KW_WILD_NUMBER refused it ("path MUST BE a json
     *  boolean") and read FALSE: a topic with records could never be deleted
     *  by command. delete-key, above, always read it right.
     *-------------------------------------------------*/
    if(!tranger2_create_topic(
            tranger,
            TOPIC_NAME3,
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
        )) {
        printf("%s: FAIL (create topic3)\n", APP);
        return -1;
    }
    {
        md2_record_ex_t md = {0};
        json_t *jn_record = json_pack("{s:s, s:I, s:s}",
            "id", KEY_A,
            "tm", (json_int_t)BASE_T,
            "content", "payload"
        );
        if(tranger2_append_record(tranger, TOPIC_NAME3, BASE_T, 0, &md, jn_record) < 0) {
            printf("%s: FAIL (append topic3)\n", APP);
            return -1;
        }
    }

    r = gobj_command(yuno, "delete-topic",
        json_pack("{s:s}", "topic_name", TOPIC_NAME3), yuno);
    check_int("delete-topic no-force result", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)

    r = gobj_command(yuno, "delete-topic",
        json_pack("{s:s, s:i}",
            "topic_name", TOPIC_NAME3,
            "force", 1
        ), yuno);
    check_int("delete-topic force=1 (integer) result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    r = gobj_command(yuno, "topics", json_pack("{}"), yuno);
    data = kw_get_list(0, r, "data", 0, 0);
    check_bool("delete-topic topic3 is gone",
        json_str_in_list(0, data, TOPIC_NAME3, FALSE), FALSE);
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

    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itDeadAll",
            "topic_name", TOPIC_NAME2,
            "rkey", ".*"
        ), yuno);
    check_int("open-iterator rkey on topic2", kw_get_int(0, r, "result", -999, 0), 0);
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

    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itDeadAll",
            "from_rowid", 1,
            "limit", 10
        ), yuno);
    check_int("get-page rkey on a dead topic", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)

    r = gobj_command(yuno, "close-iterator",
        json_pack("{s:s}", "iterator_id", "itDeadAll"), yuno);
    check_int("close-iterator rkey on a dead topic", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    global_result += test_json(NULL);

    /*  `rtDead` is left REGISTERED on the closed topic on purpose: it is the
     *  crash as it happened in production — mt_destroy sweeping a registry
     *  entry whose handle the tranger already freed. It must drop the entry
     *  without touching it (gobj_end() runs it, and the leak check that
     *  follows would catch a handle left behind).  */

    /*-------------------------------------------------*
     *      ABA: the topic is closed AND OPENED AGAIN under its handles
     *      delete-topic + create-topic, a
     *      stop/start of the service, a backup: the name is back, the
     *      handles are not. Judged by the topic's NAME, the stale pointers
     *      looked alive again and were dereferenced after being freed (a
     *      SIGSEGV when the session closed); judged by their identity
     *      (id + creator), they are gone.
     *
     *      Opening topic2 again also brings rtDead's topic back: mt_destroy
     *      must drop it by identity too.
     *-------------------------------------------------*/
    if(!tranger2_open_topic(tranger, TOPIC_NAME2, FALSE)) {
        printf("%s: FAIL (reopen topic2)\n", APP);
        return -1;
    }
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itABA",
            "topic_name", TOPIC_NAME2,
            "key", KEY_A
        ), yuno);
    check_int("open-iterator ABA", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itABAAll",
            "topic_name", TOPIC_NAME2,
            "rkey", ".*"
        ), yuno);
    check_int("open-iterator rkey ABA", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-rt",
        json_pack("{s:s, s:s, s:s}",
            "rt_id", "rtABA",
            "topic_name", TOPIC_NAME2,
            "key", KEY_A
        ), yuno);
    check_int("open-rt ABA", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    if(tranger2_close_topic(tranger, TOPIC_NAME2) < 0 ||
            !tranger2_open_topic(tranger, TOPIC_NAME2, FALSE)) {
        printf("%s: FAIL (close + reopen topic2)\n", APP);
        global_result += -1;
    }

    set_expected_results(
        "handles of a topic opened again are gone, not resurrected",
        NULL,   // no errors expected
        NULL, NULL, 1
    );
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itABA",
            "from_rowid", 1,
            "limit", 10
        ), yuno);
    check_int("get-page on a topic opened again", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itABAAll",
            "from_rowid", 1,
            "limit", 10
        ), yuno);
    check_int("get-page rkey on a topic opened again", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)

    /*  An id whose handle went with its topic is FREE again. With the dead
     *  entry kept in the registries, the same id answers "already open"
     *  (result 0, no data) and every get-page after it -1, until a close by
     *  hand. A real open carries data.  */
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itABA",
            "topic_name", TOPIC_NAME2,
            "key", KEY_A
        ), yuno);
    check_int("open-iterator itABA again result", kw_get_int(0, r, "result", -999, 0), 0);
    check_int("open-iterator itABA again is a real open",
        kw_get_int(0, kw_get_dict(0, r, "data", 0, 0), "total_rows", -1, 0), KEY_A_ROWS);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itABAAll",
            "topic_name", TOPIC_NAME2,
            "rkey", ".*"
        ), yuno);
    check_int("open-iterator itABAAll again result", kw_get_int(0, r, "result", -999, 0), 0);
    check_int("open-iterator itABAAll again is a real open",
        kw_get_int(0, kw_get_dict(0, r, "data", 0, 0), "total_rows", -1, 0), KEY_A_ROWS);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-rt",
        json_pack("{s:s, s:s, s:s}",
            "rt_id", "rtABA",
            "topic_name", TOPIC_NAME2,
            "key", KEY_A
        ), yuno);
    check_int("open-rt rtABA again result", kw_get_int(0, r, "result", -999, 0), 0);
    check_bool("open-rt rtABA again is a real open",
        strstr(kw_get_str(0, r, "comment", "", 0), "already open") == NULL, TRUE);
    JSON_DECREF(r)
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itABA",
            "from_rowid", 1,
            "limit", 10
        ), yuno);
    check_int("get-page itABA opened again", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    r = gobj_command(yuno, "close-iterator",
        json_pack("{s:s}", "iterator_id", "itABA"), yuno);
    check_int("close-iterator on a topic opened again", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-iterator",
        json_pack("{s:s}", "iterator_id", "itABAAll"), yuno);
    check_int("close-iterator rkey on a topic opened again",
        kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    global_result += test_json(NULL);
    /*  rtABA stays registered, like rtDead: mt_destroy sweeps both.  */

    /*-------------------------------------------------*
     *      A backward page counts from the LIVE end of the key. A window
     *      cut with the row count frozen at the open skips the newest rows
     *      once the key grew ("newest first"), and a page past the old
     *      count comes back empty.
     *-------------------------------------------------*/
    set_expected_results("a backward page counts from the live end", NULL, NULL, NULL, 1);
    for(int j = 0; j < 10; j++) {
        if(append_one(tranger, KEY_E, BASE_T + j) < 0) {
            printf("%s: FAIL (append E)\n", APP);
            return -1;
        }
    }
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s, s:b}",
            "iterator_id", "itE",
            "topic_name", TOPIC_NAME,
            "key", KEY_E,
            "backward", 1
        ), yuno);
    check_int("open-iterator E backward result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    for(int j = 10; j < 13; j++) {
        if(append_one(tranger, KEY_E, BASE_T + j) < 0) {
            printf("%s: FAIL (append E again)\n", APP);
            return -1;
        }
    }
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itE",
            "from_rowid", 1,
            "limit", 2
        ), yuno);
    {
        json_t *page = kw_get_dict(0, r, "data", 0, 0);
        json_t *rows = kw_get_list(0, page, "data", 0, 0);
        check_int("backward page 1 after appends: total_rows",
            kw_get_int(0, page, "total_rows", -1, 0), 13);
        check_int("backward page 1 after appends: len", json_array_size(rows), 2);
        check_int("backward page 1 after appends: newest",
            record_rowid(json_array_get(rows, 0)), 13);
        check_int("backward page 1 after appends: next",
            record_rowid(json_array_get(rows, 1)), 12);
    }
    JSON_DECREF(r)
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itE",
            "from_rowid", 11,
            "limit", 5
        ), yuno);
    {
        json_t *rows = kw_get_list(0, kw_get_dict(0, r, "data", 0, 0), "data", 0, 0);
        check_int("backward last page after appends: len", json_array_size(rows), 3);
        check_int("backward last page after appends: first",
            record_rowid(json_array_get(rows, 0)), 3);
        check_int("backward last page after appends: last",
            record_rowid(json_array_get(rows, 2)), 1);
    }
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-iterator",
        json_pack("{s:s}", "iterator_id", "itE"), yuno);
    JSON_DECREF(r)
    global_result += test_json(NULL);

    /*-------------------------------------------------*
     *      A key deleted and BORN AGAIN under a multi-key iterator is a
     *      deleted key still: judged by its presence in the topic's cache,
     *      the reborn key would be paged with the row count of the dead one
     *      -- result 0, stale total_rows, no log.
     *-------------------------------------------------*/
    set_expected_results("a key born again under an rkey iterator was deleted", NULL, NULL, NULL, 1);
    for(int j = 0; j < 6; j++) {
        if(append_one(tranger, KEY_F, BASE_T + j) < 0) {
            printf("%s: FAIL (append F)\n", APP);
            return -1;
        }
    }
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itF",
            "topic_name", TOPIC_NAME,
            "rkey", "^F$"
        ), yuno);
    check_int("open-iterator F multi result", kw_get_int(0, r, "result", -999, 0), 0);
    check_int("open-iterator F multi total_rows",
        kw_get_int(0, kw_get_dict(0, r, "data", 0, 0), "total_rows", -1, 0), 6);
    JSON_DECREF(r)
    r = gobj_command(yuno, "delete-key",
        json_pack("{s:s, s:s, s:b}",
            "topic_name", TOPIC_NAME,
            "key", KEY_F,
            "force", 1
        ), yuno);
    check_int("delete-key F result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    for(int j = 10; j < 12; j++) {
        if(append_one(tranger, KEY_F, BASE_T + j) < 0) {
            printf("%s: FAIL (append F again)\n", APP);
            return -1;
        }
    }
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itF",
            "from_rowid", 1,
            "limit", 10
        ), yuno);
    check_int("get-page F born again result", kw_get_int(0, r, "result", -999, 0), -1);
    check_bool("get-page F born again says why",
        strstr(kw_get_str(0, r, "comment", "", 0), "was deleted") != NULL, TRUE);
    JSON_DECREF(r)
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s}", "iterator_id", "itF"), yuno);
    check_bool("get-page F born again: closed",
        strstr(kw_get_str(0, r, "comment", "", 0), "Iterator not found") != NULL, TRUE);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itF",
            "topic_name", TOPIC_NAME,
            "rkey", "^F$"
        ), yuno);
    check_int("open-iterator F born again total_rows",
        kw_get_int(0, kw_get_dict(0, r, "data", 0, 0), "total_rows", -1, 0), 2);
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-iterator",
        json_pack("{s:s}", "iterator_id", "itF"), yuno);
    JSON_DECREF(r)
    global_result += test_json(NULL);

    /*-------------------------------------------------*
     *      A MULTI-key iterator paged backward counts from the LIVE end
     *      too: 7.25.3 did it for one key only, and the parts of an rkey
     *      iterator kept the row counts of the open -- so "newest first"
     *      on the whole-topic card never showed a row appended after it.
     *-------------------------------------------------*/
    set_expected_results("a multi-key backward page counts from the live end", NULL, NULL, NULL, 1);
    for(int j = 0; j < 4; j++) {
        if(append_one(tranger, KEY_G, BASE_T + j) < 0) {
            printf("%s: FAIL (append G)\n", APP);
            return -1;
        }
    }
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s, s:b}",
            "iterator_id", "itG",
            "topic_name", TOPIC_NAME,
            "rkey", "^" KEY_G "$",
            "backward", 1
        ), yuno);
    check_int("open-iterator G multi backward result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    for(int j = 4; j < 6; j++) {
        if(append_one(tranger, KEY_G, BASE_T + j) < 0) {
            printf("%s: FAIL (append G again)\n", APP);
            return -1;
        }
    }
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itG",
            "from_rowid", 1,
            "limit", 2
        ), yuno);
    {
        json_t *page = kw_get_dict(0, r, "data", 0, 0);
        json_t *rows = kw_get_list(0, page, "data", 0, 0);
        check_int("multi backward page 1 after appends: total_rows",
            kw_get_int(0, page, "total_rows", -1, 0), 6);
        check_int("multi backward page 1 after appends: len", json_array_size(rows), 2);
        check_int("multi backward page 1 after appends: newest",
            record_rowid(json_array_get(rows, 0)), 6);
        check_int("multi backward page 1 after appends: next",
            record_rowid(json_array_get(rows, 1)), 5);
    }
    JSON_DECREF(r)

    /*  A key the iterator does not page is deleted: its keys watch hears
     *  it, and must not keep it -- it recorded every delete of the topic
     *  for the life of the iterator.  */
    for(int j = 0; j < 2; j++) {
        if(append_one(tranger, KEY_H, BASE_T + j) < 0) {
            printf("%s: FAIL (append H)\n", APP);
            return -1;
        }
    }
    r = gobj_command(yuno, "delete-key",
        json_pack("{s:s, s:s, s:b}",
            "topic_name", TOPIC_NAME,
            "key", KEY_H,
            "force", 1
        ), yuno);
    check_int("delete-key H result", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    {
        char creator[NAME_MAX];
        snprintf(creator, sizeof(creator), "%s^keys", gobj_name(yuno));
        json_t *watch = tranger2_get_rt_mem_by_id(tranger, TOPIC_NAME, "itG^__keys__", creator);
        check_bool("the keys watch is under its own creator", watch != NULL, TRUE);
        check_bool("the keys watch keeps no key it does not page",
            json_object_get(json_object_get(watch, "deleted_keys"), KEY_H) != NULL, FALSE);
    }
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itG",
            "from_rowid", 1,
            "limit", 2
        ), yuno);
    check_int("get-page G after another key was deleted", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-iterator",
        json_pack("{s:s}", "iterator_id", "itG"), yuno);
    JSON_DECREF(r)
    global_result += test_json(NULL);

    /*-------------------------------------------------*
     *      What a multi-key iterator does NOT promise, pinned so the
     *      docs say it exactly:
     *      - a FILTERED key keeps the row count of the open: a row
     *        appended after it is not paged, although each page opens
     *        (and indexes) the key again;
     *      - an UNFILTERED key is counted live, so paging FORWARD after
     *        an append to an EARLIER key shifts every later position by
     *        one: the next page repeats the last row of the one before.
     *-------------------------------------------------*/
    set_expected_results("multi-key paging: frozen filtered counts, forward shift", NULL, NULL, NULL, 1);
    for(int j = 0; j < 3; j++) {
        append_one(tranger, KEY_I, BASE_T + j);
    }
    for(int j = 0; j < 2; j++) {
        append_one(tranger, KEY_J, BASE_T + j);
    }
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s, s:I}",
            "iterator_id", "itIJf",
            "topic_name", TOPIC_NAME,
            "rkey", "^[" KEY_I KEY_J "]$",
            "from_t", (json_int_t)BASE_T
        ), yuno);
    check_int("open-iterator I+J filtered total_rows",
        kw_get_int(0, kw_get_dict(0, r, "data", 0, 0), "total_rows", -1, 0), 5);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itIJ",
            "topic_name", TOPIC_NAME,
            "rkey", "^[" KEY_I KEY_J "]$"
        ), yuno);
    JSON_DECREF(r)
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}", "iterator_id", "itIJ", "from_rowid", 1, "limit", 4), yuno);
    {
        json_t *rows = kw_get_list(0, kw_get_dict(0, r, "data", 0, 0), "data", 0, 0);
        json_t *last = json_array_get(rows, 3);
        check_str("forward page 1 ends in J", kw_get_str(0, last, "__md_tranger__`key", "", 0), KEY_J);
        check_int("forward page 1 ends in J rowid 1", record_rowid(last), 1);
    }
    JSON_DECREF(r)

    append_one(tranger, KEY_I, BASE_T + 3);    /*  to the EARLIER key  */

    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}", "iterator_id", "itIJf", "from_rowid", 1, "limit", 10), yuno);
    {
        json_t *page = kw_get_dict(0, r, "data", 0, 0);
        check_int("a filtered key keeps the count of the open",
            kw_get_int(0, page, "total_rows", -1, 0), 5);
        check_int("a filtered key does not page the new row",
            json_array_size(kw_get_list(0, page, "data", 0, 0)), 5);
    }
    JSON_DECREF(r)
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}", "iterator_id", "itIJ", "from_rowid", 5, "limit", 4), yuno);
    {
        json_t *page = kw_get_dict(0, r, "data", 0, 0);
        json_t *first = json_array_get(kw_get_list(0, page, "data", 0, 0), 0);
        check_int("an unfiltered key is counted live", kw_get_int(0, page, "total_rows", -1, 0), 6);
        check_str("forward page 2 starts in J", kw_get_str(0, first, "__md_tranger__`key", "", 0), KEY_J);
        check_int("forward page 2 repeats J rowid 1", record_rowid(first), 1);
    }
    JSON_DECREF(r)
    /*  ...and backward, after an append to a LATER key: page 1 is J2 J1
     *  (I has 4 rows, J 2), J gets a third, and page 2 starts with J1 again  */
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s, s:b}",
            "iterator_id", "itIJb",
            "topic_name", TOPIC_NAME,
            "rkey", "^[" KEY_I KEY_J "]$",
            "backward", 1
        ), yuno);
    JSON_DECREF(r)
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}", "iterator_id", "itIJb", "from_rowid", 1, "limit", 2), yuno);
    {
        json_t *rows = kw_get_list(0, kw_get_dict(0, r, "data", 0, 0), "data", 0, 0);
        check_int("backward page 1 ends in J rowid 1", record_rowid(json_array_get(rows, 1)), 1);
    }
    JSON_DECREF(r)
    append_one(tranger, KEY_J, BASE_T + 2);    /*  to the LATER key  */
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}", "iterator_id", "itIJb", "from_rowid", 3, "limit", 2), yuno);
    {
        json_t *first = json_array_get(
            kw_get_list(0, kw_get_dict(0, r, "data", 0, 0), "data", 0, 0), 0);
        check_str("backward page 2 starts in J", kw_get_str(0, first, "__md_tranger__`key", "", 0), KEY_J);
        check_int("backward page 2 repeats J rowid 1", record_rowid(first), 1);
    }
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-iterator", json_pack("{s:s}", "iterator_id", "itIJb"), yuno);
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-iterator", json_pack("{s:s}", "iterator_id", "itIJf"), yuno);
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-iterator", json_pack("{s:s}", "iterator_id", "itIJ"), yuno);
    JSON_DECREF(r)
    global_result += test_json(NULL);

    /*-------------------------------------------------*
     *      The keys watch of a multi-key iterator is named
     *      "<iterator_id>^__keys__", a name a client can give its own
     *      feed. Under the same creator, the client's feed took the id
     *      and the iterator could not open; and a get-page of a DEAD
     *      iterator closed the client's feed as if it were the watch.
     *-------------------------------------------------*/
    set_expected_results("a keys watch is not a client's feed", NULL, NULL, NULL, 1);
    r = gobj_command(yuno, "open-rt",
        json_pack("{s:s, s:s, s:s}",
            "rt_id", "itW^__keys__",
            "topic_name", TOPIC_NAME,
            "key", KEY_G
        ), yuno);
    check_int("open-rt named like a keys watch", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itW",
            "topic_name", TOPIC_NAME,
            "rkey", "^" KEY_G "$"
        ), yuno);
    check_int("open-iterator beside a feed named like its watch",
        kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-iterator",
        json_pack("{s:s}", "iterator_id", "itW"), yuno);
    check_int("close-iterator itW", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    g_rt_watch_id = 0;
    append_one(tranger, KEY_G, BASE_T + 10);
    check_int("the client's feed survives the iterator's close", g_rt_watch_id, 1);
    r = gobj_command(yuno, "close-rt",
        json_pack("{s:s}", "rt_id", "itW^__keys__"), yuno);
    check_bool("close-rt of the client's feed closes it",
        strstr(kw_get_str(0, r, "comment", "", 0), "Realtime feed closed") != NULL, TRUE);
    JSON_DECREF(r)
    global_result += test_json(NULL);

    /*-------------------------------------------------*
     *      open-rt refuses an rt_id no directory entry can hold: on a
     *      replica the feed is a directory named after it, and it
     *      answered "opened" and never delivered.
     *-------------------------------------------------*/
    {
        char long_id[NAME_MAX + 10];
        memset(long_id, 'x', sizeof(long_id) - 1);
        long_id[sizeof(long_id) - 1] = 0;
        r = gobj_command(yuno, "open-rt",
            as_peer(json_pack("{s:s, s:s}",
                "rt_id", long_id,
                "topic_name", TOPIC_NAME
            )), yuno);
        check_int("open-rt with an rt_id longer than NAME_MAX", kw_get_int(0, r, "result", -999, 0), -1);
        JSON_DECREF(r)
    }

    /*-------------------------------------------------*
     *      A no_rt list (open-list with to_rowid: no realtime) is not
     *      its topic's, and it outlives it. Every path that drops a
     *      registry entry "closed with its topic" asked the topic first,
     *      and a closed topic answered for the list too: nobody freed it
     *      (10 KB per list). The leak check at the end is the assertion.
     *-------------------------------------------------*/
    set_expected_results("a no_rt list is freed when its topic goes", NULL, NULL, NULL, 1);
    if(!tranger2_create_topic(
            tranger,
            TOPIC_NAME4,
            "id",
            "tm",
            NULL,
            sf_string_key,
            json_pack("{s:s, s:I, s:s}", "id", "", "tm", (json_int_t)0, "content", ""),
            0
        )) {
        printf("%s: FAIL (create topic4)\n", APP);
        return -1;
    }
    for(int j = 0; j < 4; j++) {
        json_t *jn_record = json_pack("{s:s, s:I, s:s}",
            "id", KEY_A, "tm", (json_int_t)(BASE_T + j), "content", "payload"
        );
        md2_record_ex_t md = {0};
        if(tranger2_append_record(tranger, TOPIC_NAME4, BASE_T + j, 0, &md, jn_record) < 0) {
            printf("%s: FAIL (append topic4)\n", APP);
            return -1;
        }
    }
    r = gobj_command(yuno, "open-list",
        json_pack("{s:s, s:s, s:s, s:i}",
            "list_id", "lstNoRt1",
            "topic_name", TOPIC_NAME4,
            "key", KEY_A,
            "to_rowid", 3
        ), yuno);
    check_int("open-list no_rt 1", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-list",
        json_pack("{s:s, s:s, s:s, s:i}",
            "list_id", "lstNoRt2",
            "topic_name", TOPIC_NAME4,
            "key", KEY_A,
            "to_rowid", 3
        ), yuno);
    check_int("open-list no_rt 2", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    /*  closed by somebody else: close-list still frees it  */
    tranger2_close_topic(tranger, TOPIC_NAME4);
    r = gobj_command(yuno, "close-list", json_pack("{s:s}", "list_id", "lstNoRt1"), yuno);
    check_bool("close-list of a no_rt list whose topic is closed closes it",
        strstr(kw_get_str(0, r, "comment", "", 0), "List closed") != NULL, TRUE);
    JSON_DECREF(r)

    /*  delete-topic drops the other one, and frees it  */
    r = gobj_command(yuno, "delete-topic",
        json_pack("{s:s, s:i}",
            "topic_name", TOPIC_NAME4,
            "force", 1
        ), yuno);
    check_int("delete-topic topic4", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-list", json_pack("{s:s}", "list_id", "lstNoRt2"), yuno);
    check_bool("delete-topic dropped the no_rt list",
        strstr(kw_get_str(0, r, "comment", "", 0), "List not found") != NULL, TRUE);
    JSON_DECREF(r)
    global_result += test_json(NULL);

    /*-------------------------------------------------*
     *      A one-key iterator named like a PART of a multi-key one
     *      ("<id>^<key>") is another identity: the parts are opened
     *      under their own creator (under one creator, "Iterator already
     *      exists" and every page over that key answers -1).
     *-------------------------------------------------*/
    set_expected_results("a part id is not a client's iterator id", NULL, NULL, NULL, 1);
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itP^" KEY_A,
            "topic_name", TOPIC_NAME,
            "key", KEY_A
        ), yuno);
    check_int("open-iterator named like a part", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itP",
            "topic_name", TOPIC_NAME,
            "rkey", "^" KEY_A "$"
        ), yuno);
    check_int("open-iterator multi beside it", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itP",
            "from_rowid", 1,
            "limit", 2
        ), yuno);
    check_int("get-page of the multi over the shared key", kw_get_int(0, r, "result", -999, 0), 0);
    check_int("get-page of the multi over the shared key: len",
        json_array_size(kw_get_list(0, kw_get_dict(0, r, "data", 0, 0), "data", 0, 0)), 2);
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-iterator", json_pack("{s:s}", "iterator_id", "itP"), yuno);
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-iterator", json_pack("{s:s}", "iterator_id", "itP^" KEY_A), yuno);
    JSON_DECREF(r)
    global_result += test_json(NULL);

    /*-------------------------------------------------*
     *      A SESSION that only PAGES. Its handles are stamped with it as
     *      src, and C_TRANGER watches the session (EV_ON_CLOSE) from the
     *      first handle -- ONCE: the second handle must not re-subscribe,
     *      which gobj logs as "subscription(s) REPEATED" with a stack
     *      trace. When the session goes, every handle it opened goes.
     *
     *      The session is a real C_IEVENT_SRV, created and never started
     *      (it needs no loop for that), hosted by the probe so that the
     *      CHILD-model subscription it makes lands on an FSM that declares
     *      EV_ON_CLOSE.
     *-------------------------------------------------*/
    /*  The re-subscription is a WARNING and this test captures from ERROR
     *  up, so for these two sections a second capture takes warnings too.
     *  (INFO stays out: the "Closing iterator" lines are not asserted.)  */
    gobj_log_add_handler("test_capture_warn", "testing", LOG_OPT_UP_WARNING, 0);
    set_expected_results(
        "a paging session is watched once",
        /*  no "subscription(s) REPEATED", nothing but the three answers
         *  that name the yuno (two iterators and a list opened)  */
        expect_role_warnings(3),
        NULL, NULL, 1
    );
    hgobj session = gobj_create("session", C_IEVENT_SRV, 0, probe);
    if(!session) {
        printf("%s: FAIL (session create)\n", APP);
        return -1;
    }

    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itSession1",
            "topic_name", TOPIC_NAME,
            "key", KEY_A
        ), session);
    check_int("open-iterator session 1", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    {
        json_t *subs = gobj_find_subscriptions(session, EV_ON_CLOSE, 0, yuno);
        check_int("watched after the first handle", json_array_size(subs), 1);
        JSON_DECREF(subs)
    }

    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}",
            "iterator_id", "itSession2",
            "topic_name", TOPIC_NAME,
            "key", KEY_B
        ), session);
    check_int("open-iterator session 2", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    {
        json_t *subs = gobj_find_subscriptions(session, EV_ON_CLOSE, 0, yuno);
        check_int("still watched once after the second", json_array_size(subs), 1);
        JSON_DECREF(subs)
    }

    /*  A stateful list is a handle of the session too: stamped with no
     *  owner, it is reaped by nobody, and collects every append in memory
     *  after its client died.  */
    r = gobj_command(yuno, "open-list",
        json_pack("{s:s, s:s, s:s}",
            "list_id", "lstSession",
            "topic_name", TOPIC_NAME,
            "key", KEY_A
        ), session);
    check_int("open-list session", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    {
        json_t *subs = gobj_find_subscriptions(session, EV_ON_CLOSE, 0, yuno);
        check_int("still watched once after the list", json_array_size(subs), 1);
        JSON_DECREF(subs)
    }
    global_result += test_json(NULL);

    /*  The session closes its LAST Live card -- its last subscription to
     *  this service -- and stays alive: its Rows cards still page. Reaping
     *  on that unsubscribe would take the paging iterators too, and they
     *  would answer "Iterator not found" from then on.
     *  A session's iterators are reaped by its EV_ON_CLOSE, below.  */
    set_expected_results(
        "a session that closes its last Live card keeps its iterators",
        NULL,
        NULL, NULL, 1
    );
    gobj_subscribe_event(yuno, EV_TRANGER_RECORD_ADDED, 0, session);
    gobj_unsubscribe_event(yuno, EV_TRANGER_RECORD_ADDED, 0, session);
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itSession1",
            "from_rowid", 1,
            "limit", 1
        ), session);
    check_int("a live session still pages after its last unsubscribe",
        kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "get-list-data",
        json_pack("{s:s}", "list_id", "lstSession"), session);
    check_bool("a live session keeps its list after its last unsubscribe",
        strstr(kw_get_str(0, r, "comment", "", 0), "List not found") == NULL, TRUE);
    JSON_DECREF(r)
    global_result += test_json(NULL);

    /*  The session dies: both iterators are reaped, the watch goes with
     *  it, and the probe (its parent) heard the close exactly once.  */
    set_expected_results(
        "a gone paging session takes its iterators with it",
        expect_role_warnings(3),    // two iterators and a list not found, said with the yuno
        NULL, NULL, 1
    );
    g_session_closed = 0;
    gobj_publish_event(session, EV_ON_CLOSE, json_object());
    check_int("the parent heard the close once", g_session_closed, 1);
    {
        json_t *subs = gobj_find_subscriptions(session, EV_ON_CLOSE, 0, yuno);
        check_int("no watch left on a gone session", json_array_size(subs), 0);
        JSON_DECREF(subs)
    }
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itSession1",
            "from_rowid", 1,
            "limit", 10
        ), yuno);
    check_int("get-page on a reaped iterator 1", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}",
            "iterator_id", "itSession2",
            "from_rowid", 1,
            "limit", 10
        ), yuno);
    check_int("get-page on a reaped iterator 2", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)
    r = gobj_command(yuno, "get-list-data",
        json_pack("{s:s}", "list_id", "lstSession"), yuno);
    check_bool("a gone session takes its list with it",
        strstr(kw_get_str(0, r, "comment", "", 0), "List not found") != NULL, TRUE);
    JSON_DECREF(r)
    global_result += test_json(NULL);
    gobj_destroy(session);
    gobj_log_del_handler("test_capture_warn");

    /*-------------------------------------------------*
     *      A handle is its session's. Session B knows the ids of the
     *      handles of session A (print-tranger shows them to any `read`
     *      user), and closes or reads none of them: -403, and a warning.
     *      Session A, and a gobj of this yuno, can. (Up to 7.25.4 any
     *      session closed or read any handle by its id.)
     *-------------------------------------------------*/
    set_expected_results("a handle of another session is refused", NULL, NULL, NULL, 1);
    gobj_log_register_handler("not_owner", 0, count_not_owner, 0);
    gobj_log_add_handler("count_not_owner", "not_owner", LOG_OPT_UP_WARNING, 0);
    hgobj sessA = gobj_create("sessA", C_IEVENT_SRV, 0, probe);
    hgobj sessB = gobj_create("sessB", C_IEVENT_SRV, 0, probe);
    if(!sessA || !sessB) {
        printf("%s: FAIL (sessions create)\n", APP);
        return -1;
    }
    r = gobj_command(yuno, "open-rt",
        json_pack("{s:s, s:s, s:s}", "rt_id", "rtOwn", "topic_name", TOPIC_NAME, "key", KEY_A), sessA);
    check_int("session A opens a feed", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}", "iterator_id", "itOwn", "topic_name", TOPIC_NAME, "key", KEY_A), sessA);
    check_int("session A opens an iterator", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s}", "iterator_id", "itOwnM", "topic_name", TOPIC_NAME, "rkey", "^[AB]$"), sessA);
    check_int("session A opens a multi-key iterator", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-list",
        json_pack("{s:s, s:s, s:s}", "list_id", "lstOwn", "topic_name", TOPIC_NAME, "key", KEY_A), sessA);
    check_int("session A opens a list", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    g_not_owner_said = 0;
    struct {
        const char *command;
        json_t *kw;
    } foreign[] = {
        {"close-rt",        json_pack("{s:s}", "rt_id", "rtOwn")},
        {"close-iterator",  json_pack("{s:s}", "iterator_id", "itOwn")},
        {"get-page",        json_pack("{s:s, s:i, s:i}", "iterator_id", "itOwn", "from_rowid", 1, "limit", 1)},
        {"get-page",        json_pack("{s:s, s:i, s:i}", "iterator_id", "itOwnM", "from_rowid", 1, "limit", 1)},
        {"close-iterator",  json_pack("{s:s}", "iterator_id", "itOwnM")},
        {"get-list-data",   json_pack("{s:s}", "list_id", "lstOwn")},
        {"close-list",      json_pack("{s:s}", "list_id", "lstOwn")},
    };
    for(size_t i = 0; i < ARRAY_SIZE(foreign); i++) {
        r = gobj_command(yuno, foreign[i].command, foreign[i].kw, sessB);
        char name[80];
        snprintf(name, sizeof(name), "%s by another session", foreign[i].command);
        check_int(name, kw_get_int(0, r, "result", -999, 0), -403);
        JSON_DECREF(r)
    }
    check_int("each refusal is said", g_not_owner_said, (json_int_t)ARRAY_SIZE(foreign));
    check_bool("the feed of session A is still open",
        tranger2_get_rt_mem_by_id(tranger, TOPIC_NAME, "rtOwn", gobj_name(yuno)) != NULL, TRUE);

    /*  Its owner reads and closes them; a gobj of this yuno too  */
    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i}", "iterator_id", "itOwn", "from_rowid", 1, "limit", 1), sessA);
    check_int("get-page by its session", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "get-list-data", json_pack("{s:s}", "list_id", "lstOwn"), yuno);
    check_int("get-list-data by a gobj of the yuno", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-rt", json_pack("{s:s}", "rt_id", "rtOwn"), sessA);
    check_bool("close-rt by its session",
        strstr(kw_get_str(0, r, "comment", "", 0), "Realtime feed closed") != NULL, TRUE);
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-iterator", json_pack("{s:s}", "iterator_id", "itOwn"), sessA);
    check_int("close-iterator by its session", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-iterator", json_pack("{s:s}", "iterator_id", "itOwnM"), yuno);
    check_int("close-iterator by a gobj of the yuno", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-list", json_pack("{s:s}", "list_id", "lstOwn"), sessA);
    check_int("close-list by its session", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    check_int("nothing more is refused", g_not_owner_said, (json_int_t)ARRAY_SIZE(foreign));
    gobj_log_del_handler("count_not_owner");
    gobj_publish_event(sessA, EV_ON_CLOSE, json_object());
    gobj_destroy(sessA);
    gobj_destroy(sessB);
    global_result += test_json(NULL);

    /*-------------------------------------------------*
     *      The same through the agent. Every command-yuno reaches the
     *      yuno over its ONE link to the agent (a C_IEVENT_CLI), whoever
     *      sent it, with the `__username__` the agent stamped: the channel
     *      names nobody, the user does. Another user is refused, and so is
     *      a relayed command of no user; the same user is not (through the
     *      agent two sessions of one user are one owner). (In 7.25.4 bob
     *      closed alice's iterator with result 0.)
     *-------------------------------------------------*/
    set_expected_results("a handle of another user through the agent is refused", NULL, NULL, NULL, 1);
    gobj_log_add_handler("count_not_owner", "not_owner", LOG_OPT_UP_WARNING, 0);
    hgobj relay = gobj_create("agent_client", C_IEVENT_CLI, 0, probe);
    if(!relay) {
        printf("%s: FAIL (relay create)\n", APP);
        return -1;
    }
    r = gobj_command(yuno, "open-iterator",
        json_pack("{s:s, s:s, s:s, s:s}", "iterator_id", "itRelay", "topic_name", TOPIC_NAME,
            "key", KEY_A, "__username__", "alice"), relay);
    check_int("alice opens an iterator through the agent", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-rt",
        json_pack("{s:s, s:s, s:s, s:s}", "rt_id", "rtRelay", "topic_name", TOPIC_NAME,
            "key", KEY_A, "__username__", "alice"), relay);
    check_int("alice opens a feed through the agent", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    g_not_owner_said = 0;
    struct {
        const char *command;
        json_t *kw;
    } relayed[] = {
        {"close-iterator",  json_pack("{s:s, s:s}", "iterator_id", "itRelay", "__username__", "bob")},
        {"get-page",        json_pack("{s:s, s:i, s:i, s:s}", "iterator_id", "itRelay",
                                "from_rowid", 1, "limit", 1, "__username__", "bob")},
        {"close-rt",        json_pack("{s:s, s:s}", "rt_id", "rtRelay", "__username__", "bob")},
        {"close-iterator",  json_pack("{s:s}", "iterator_id", "itRelay")},
    };
    for(size_t i = 0; i < ARRAY_SIZE(relayed); i++) {
        r = gobj_command(yuno, relayed[i].command, relayed[i].kw, relay);
        char name[80];
        snprintf(name, sizeof(name), "%s by another user through the agent", relayed[i].command);
        check_int(name, kw_get_int(0, r, "result", -999, 0), -403);
        JSON_DECREF(r)
    }
    check_int("each relayed refusal is said", g_not_owner_said, (json_int_t)ARRAY_SIZE(relayed));

    r = gobj_command(yuno, "get-page",
        json_pack("{s:s, s:i, s:i, s:s}", "iterator_id", "itRelay", "from_rowid", 1, "limit", 1,
            "__username__", "alice"), relay);
    check_int("get-page by its user through the agent", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-iterator",
        json_pack("{s:s, s:s}", "iterator_id", "itRelay", "__username__", "alice"), relay);
    check_int("close-iterator by its user through the agent", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-rt",
        json_pack("{s:s, s:s}", "rt_id", "rtRelay", "__username__", "alice"), relay);
    check_bool("close-rt by its user through the agent",
        strstr(kw_get_str(0, r, "comment", "", 0), "Realtime feed closed") != NULL, TRUE);
    JSON_DECREF(r)
    check_int("nothing more is refused through the agent", g_not_owner_said,
        (json_int_t)ARRAY_SIZE(relayed));
    gobj_log_del_handler("count_not_owner");
    gobj_destroy(relay);
    global_result += test_json(NULL);

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

    /*-------------------------------------------------*
     *      Permission `write`: add-record asks it, and a refusal
     *      answers -403 before anything is done.
     *-------------------------------------------------*/
    set_expected_results("write is asked", NULL, NULL, NULL, 1);
    g_deny_authz = "write";
    g_last_authz[0] = 0;
    r = gobj_command(yuno, "add-record",
        json_pack("{s:s, s:{s:s, s:I}}", "topic_name", TOPIC_NAME,
            "record", "id", "K", "tm", (json_int_t)BASE_T), yuno);
    check_int("add-record refused", kw_get_int(0, r, "result", -999, 0), -403);
    check_str("add-record asks", g_last_authz, "write");
    /*  A -403 names the yuno first, as every answer of a command does
     *  (REV-CLOUD V20.12): here the yuno has no role, the prefix is empty  */
    check_bool("a -403 names the yuno",
        strstr(kw_get_str(0, r, "comment", "", 0), ": No permission to 'write'") != NULL, TRUE);
    JSON_DECREF(r)
    check_int("a refused add-record adds nothing",
        (json_int_t)tranger2_topic_key_size(tranger, TOPIC_NAME, "K"), 0);

    /*  check-json asks `read`, as print-tranger: it had no authz, and its
     *  walk logs an ERROR per node held twice (REV-CLOUD V20.11)  */
    g_deny_authz = "read";
    g_last_authz[0] = 0;
    r = gobj_command(yuno, "check-json", json_object(), yuno);
    check_int("check-json refused", kw_get_int(0, r, "result", -999, 0), -403);
    check_str("check-json asks", g_last_authz, "read");
    JSON_DECREF(r)
    g_deny_authz = NULL;
    global_result += test_json(NULL);

    /*  And what it finds is its ANSWER, logging nothing (REV-CLOUD V21.2.3):
     *  json_check_refcounts() logged an ERROR for every json held more than
     *  max_refcount times. By default it looks for corrupted jsons only.  */
    set_expected_results("check-json answers", json_array(), NULL, NULL, 1);
    r = gobj_command(yuno, "check-json", json_object(), yuno);
    check_int("check-json: nothing corrupted", kw_get_int(0, r, "result", -999, 0), 0);
    json_t *jn_report = kw_get_dict(0, r, "data", 0, 0);
    check_bool("check-json: the jsons walked", kw_get_int(0, jn_report, "jsons", 0, 0) > 0, TRUE);
    check_int("check-json: none corrupted", kw_get_int(0, jn_report, "corrupted", -1, 0), 0);
    JSON_DECREF(r)
    /*  What is held more than max_refcount times is the ANSWER, a 0: -1 is a
     *  fault, a corrupted json or a walk cut short. A sound tranger asked
     *  with max_refcount=1 answered -1 (REV-CLOUD V22.5.4).  */
    r = gobj_command(yuno, "check-json", json_pack("{s:i}", "max_refcount", 1), yuno);
    jn_report = kw_get_dict(0, r, "data", 0, 0);
    check_int("check-json max_refcount=1: answers what it found, a 0",
        kw_get_int(0, r, "result", -999, 0), 0);
    check_bool("check-json max_refcount=1: the walk was whole",
        kw_get_bool(0, jn_report, "truncated", 1, 0), FALSE);
    check_bool("check-json max_refcount=1: shows at most 20",
        json_array_size(kw_get_list(0, jn_report, "shown", 0, 0)) <= 20, TRUE);
    JSON_DECREF(r)
    global_result += test_json(NULL);

    /*-------------------------------------------------*
     *      add-record: a record appended to a topic. Up to 7.25.4 it
     *      was a stub that answered -1 "Pending to review" and logged an
     *      ERROR with a stack.
     *-------------------------------------------------*/
    set_expected_results("add-record", NULL, NULL, NULL, 1);
    r = gobj_command(yuno, "add-record", json_pack("{s:{s:s}}", "record", "id", "K"), yuno);
    check_int("add-record without topic_name", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)
    r = gobj_command(yuno, "add-record",
        as_peer(json_pack("{s:s, s:{s:s}}", "topic_name", "nope", "record", "id", "K")), yuno);
    check_int("add-record to a topic that is not there", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)
    r = gobj_command(yuno, "add-record", as_peer(json_pack("{s:s}", "topic_name", TOPIC_NAME)), yuno);
    check_int("add-record without record", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)
    r = gobj_command(yuno, "add-record",
        as_peer(json_pack("{s:s, s:s}", "topic_name", TOPIC_NAME, "record", "[1,2]")), yuno);
    check_int("add-record of a record that is not a dict", kw_get_int(0, r, "result", -999, 0), -1);
    JSON_DECREF(r)

    r = gobj_command(yuno, "add-record",
        json_pack("{s:s, s:I, s:{s:s, s:I, s:s}}", "topic_name", TOPIC_NAME,
            "__t__", (json_int_t)(BASE_T + 5000),
            "record", "id", "K", "tm", (json_int_t)(BASE_T + 5000), "content", "added"), yuno);
    check_int("add-record", kw_get_int(0, r, "result", -999, 0), 0);
    check_int("add-record answers the rowid", kw_get_int(0, r, "data`rowid", -1, 0), 1);
    check_int("add-record answers the t", kw_get_int(0, r, "data`t", -1, 0), BASE_T + 5000);
    JSON_DECREF(r)
    /*  As command-yuno forwards it: the record a string, the numbers too  */
    r = gobj_command(yuno, "add-record",
        json_pack("{s:s, s:s, s:s, s:s}", "topic_name", TOPIC_NAME,
            "__t__", "946689801",
            "user_flag", "3",
            "record", "{\"id\":\"K\",\"tm\":946689801,\"content\":\"forwarded\"}"), yuno);
    check_int("add-record forwarded", kw_get_int(0, r, "result", -999, 0), 0);
    check_int("add-record forwarded answers the rowid", kw_get_int(0, r, "data`rowid", -1, 0), 2);
    JSON_DECREF(r)
    check_int("add-record: the key holds both",
        (json_int_t)tranger2_topic_key_size(tranger, TOPIC_NAME, "K"), 2);
    r = gobj_command(yuno, "open-list",
        json_pack("{s:s, s:s, s:s, s:b}", "list_id", "lstK", "topic_name", TOPIC_NAME,
            "key", "K", "return_data", 1), yuno);
    {
        json_t *rows = kw_get_list(0, r, "data", 0, 0);
        json_t *second = json_array_get(rows, 1);
        check_str("add-record: what was added is read",
            kw_get_str(0, second, "content", "", 0), "forwarded");
        check_int("add-record: with its user_flag",
            kw_get_int(0, second, "__md_tranger__`user_flag", -1, 0), 3);
    }
    JSON_DECREF(r)
    global_result += test_json(NULL);

    /*-------------------------------------------------*
     *      The data of a peer is refused: a WARNING (no stack) and -1
     *      that says why, asked BEFORE the tranger, which logged an ERROR
     *      for each one -- an unknown topic or a name with '/' or '..'
     *      ("Cannot open topic", "Invalid topic name"), a system topic to
     *      delete, an unknown system_flag, a record without its pkey or
     *      with a pkey that is a path, a __t__ that is no date (REV-CLOUD
     *      V20.4.2). The phase captures the warnings too.
     *-------------------------------------------------*/
    if(!tranger2_create_topic(tranger, "topic_sys", "id", "tm", NULL, sf_string_key,
            json_pack("{s:s}", "id", ""), json_pack("{s:b}", "system_topic", 1)) ||
       !tranger2_create_topic(tranger, "topic_int", "id", "", NULL, sf_int_key,
            json_pack("{s:I}", "id", (json_int_t)0), 0)) {
        printf("%s: FAIL (create the topics of the refusals)\n", APP);
        return -1;
    }
    gobj_log_del_handler("test_capture");
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_WARNING, 0);
    {
        json_t *expected_refusals = json_array();
        /*  desc x2, open-topic, delete-topic, delete-key, open-list,
         *  list-keys, open-iterator, open-rt, add-record  */
        for(int i = 0; i < 10; i++) {
            expect_refusal(expected_refusals, "topic not found, refused");
        }
        expect_refusal(expected_refusals, "system topic, delete refused");
        for(int i = 0; i < 6; i++) {
            expect_refusal(expected_refusals, "topic of a wrong shape, create refused");
        }
        for(int i = 0; i < 5; i++) {
            expect_refusal(expected_refusals, "record of a wrong shape, refused");
        }
        json_array_append_new(expected_refusals, json_pack("{s:s, s:s}",    // V20.4.1
            "msg", "Command parameter of the wrong type", "__priority__", "WARNING"));
        /*  the same refusal for a C caller: an ERROR (REV-CLOUD V20 #17)  */
        json_array_append_new(expected_refusals, json_pack("{s:s, s:s}",
            "msg", "topic not found, refused", "__priority__", "ERROR"));
        json_array_append_new(expected_refusals, json_pack("{s:s, s:s}",
            "msg", "GClass Attribute NOT FOUND", "__priority__", "WARNING"));
        set_expected_results("a peer's data is refused", expected_refusals, NULL, NULL, 1);
    }

    r = gobj_command(yuno, "desc", as_peer(json_pack("{s:s}", "topic_name", "nope")), yuno);
    check_refused("desc of an unknown topic", r);
    r = gobj_command(yuno, "desc", as_peer(json_pack("{s:s}", "topic_name", "../" TOPIC_NAME)), yuno);
    check_refused("desc of a topic name with '..'", r);
    r = gobj_command(yuno, "open-topic", as_peer(json_pack("{s:s}", "topic_name", "nope")), yuno);
    check_refused("open-topic of an unknown topic", r);
    r = gobj_command(yuno, "delete-topic",
        as_peer(json_pack("{s:s, s:b}", "topic_name", "a/b", "force", 1)), yuno);
    check_refused("delete-topic of a topic name with '/'", r);
    r = gobj_command(yuno, "delete-key",
        as_peer(json_pack("{s:s, s:s}", "topic_name", "nope", "key", KEY_A)), yuno);
    check_refused("delete-key of an unknown topic", r);
    r = gobj_command(yuno, "open-list",
        as_peer(json_pack("{s:s, s:s}", "list_id", "lstNope", "topic_name", "nope")), yuno);
    check_refused("open-list of an unknown topic", r);
    r = gobj_command(yuno, "list-keys", as_peer(json_pack("{s:s}", "topic_name", "nope")), yuno);
    check_refused("list-keys of an unknown topic", r);
    r = gobj_command(yuno, "open-iterator",
        as_peer(json_pack("{s:s, s:s, s:s}", "iterator_id", "itNope", "topic_name", "nope", "key", KEY_A)), yuno);
    check_refused("open-iterator of an unknown topic", r);
    r = gobj_command(yuno, "open-rt",
        as_peer(json_pack("{s:s, s:s}", "rt_id", "rtNope", "topic_name", "nope")), yuno);
    check_refused("open-rt of an unknown topic", r);
    r = gobj_command(yuno, "add-record",
        as_peer(json_pack("{s:s, s:{s:s}}", "topic_name", "a/b", "record", "id", "K")), yuno);
    check_refused("add-record to a topic name with '/'", r);

    r = gobj_command(yuno, "delete-topic",
        as_peer(json_pack("{s:s, s:b}", "topic_name", "topic_sys", "force", 1)), yuno);
    check_refused("delete-topic of a system topic", r);
    check_bool("the system topic is still there",
        tranger2_topic_is_open(tranger, "topic_sys"), TRUE);

    r = gobj_command(yuno, "create-topic", as_peer(json_pack("{s:s}", "topic_name", "a/b")), yuno);
    check_refused("create-topic of a name with '/'", r);
    r = gobj_command(yuno, "create-topic",
        as_peer(json_pack("{s:s, s:s}", "topic_name", "topic_new", "system_flag", "sf_bogus")), yuno);
    check_refused("create-topic with an unknown system_flag", r);
    r = gobj_command(yuno, "create-topic",
        as_peer(json_pack("{s:s, s:s}", "topic_name", "topic_new", "system_flag", "sf_string_key|sf_int_key")), yuno);
    check_refused("create-topic with two key types", r);
    r = gobj_command(yuno, "create-topic",
        as_peer(json_pack("{s:s, s:s}", "topic_name", "topic_new", "pkey", "")), yuno);
    check_refused("create-topic of a new topic without pkey", r);
    r = gobj_command(yuno, "create-topic",
        as_peer(json_pack("{s:s, s:i}", "topic_name", "topic_new", "jn_cols", 5)), yuno);
    check_refused("create-topic with jn_cols not a dict", r);
    {
        /*  Its directory cannot be made: a CRITICAL of the tranger, which
         *  exits the yuno with the default on_critical_error  */
        char long_name[NAME_MAX + 2];
        memset(long_name, 'x', sizeof(long_name) - 1);
        long_name[sizeof(long_name) - 1] = 0;
        r = gobj_command(yuno, "create-topic", as_peer(json_pack("{s:s}", "topic_name", long_name)), yuno);
        check_refused("create-topic of a name longer than NAME_MAX", r);
    }
    check_bool("no refused create made the topic",
        tranger2_topic_is_open(tranger, "topic_new"), FALSE);

    r = gobj_command(yuno, "add-record",
        as_peer(json_pack("{s:s, s:{s:s}}", "topic_name", TOPIC_NAME, "record", "content", "no pkey")), yuno);
    check_refused("add-record without its pkey", r);
    r = gobj_command(yuno, "add-record",
        as_peer(json_pack("{s:s, s:{s:s}}", "topic_name", TOPIC_NAME, "record", "id", "../escape")), yuno);
    check_refused("add-record with a pkey that is a path", r);
    r = gobj_command(yuno, "add-record",
        as_peer(json_pack("{s:s, s:{s:s}}", "topic_name", TOPIC_NAME, "record", "id", ".hidden")), yuno);
    check_refused("add-record with a pkey with a leading dot", r);
    r = gobj_command(yuno, "add-record",
        as_peer(json_pack("{s:s, s:I, s:{s:s}}", "topic_name", TOPIC_NAME,
            "__t__", (json_int_t)9223372036854775807LL, "record", "id", "K")), yuno);
    check_refused("add-record with a __t__ that is no date", r);
    r = gobj_command(yuno, "add-record",
        as_peer(json_pack("{s:s, s:{s:s}}", "topic_name", "topic_int", "record", "id", "abc")), yuno);
    check_refused("add-record of a string to an int key", r);
    check_int("no refused add-record added a record",
        (json_int_t)tranger2_topic_key_size(tranger, TOPIC_NAME, "K"), 2);
    check_int("nor to the int key topic",
        (json_int_t)tranger2_topic_size(tranger, "topic_int"), 0);

    /*  A peer's, with the `__username__` C_IEVENT_SRV stamps: the parser
     *  refuses a C caller's with an ERROR and a stack  */
    r = gobj_command(yuno, "print-tranger",
        json_pack("{s:i, s:s}", "path", 5, "__username__", "peer@test"), yuno);
    check_refused("print-tranger with a path that is not a string", r);

    /*  The C caller's: an ERROR with a stack, a broken contract  */
    r = gobj_command(yuno, "desc", json_pack("{s:s}", "topic_name", "nope"), yuno);
    check_refused("desc of an unknown topic, from C", r);

    global_result += test_json(NULL);

    /*-------------------------------------------------*
     *      More of a peer's data, refused with a WARNING (REV-CLOUD V20):
     *      a topic named as a FILE of the database (its mkrdir() was a
     *      CRITICAL, an exit of the yuno), a new topic_version over a
     *      SYSTEM topic (it took its mark), a record that is not a dict
     *      (-1 with no log), a key that is a path (it read another key),
     *      a time bound of a wrong type (and an integer one, taken now);
     *      and a print-tranger path that is not there, answered with no
     *      log (it was refused, where kw_find_path() logged an ERROR).
     *      A one-shot open-list named as this service read the key of an
     *      open iterator: "Iterator already exists", and the key left out.
     *-------------------------------------------------*/
    {
        json_t *expected = json_array();
        expect_refusal(expected, "topic of a wrong shape, create refused");  // a file
        expect_refusal(expected, "topic of a wrong shape, create refused");  // system
        expect_refusal(expected, "record of a wrong shape, refused");
        for(int i = 0; i < 3; i++) {    // open-iterator, open-list, open-rt
            expect_refusal(expected, "key that is not one directory component, refused");
        }
        json_t *loaded = expect_role_warnings(1);   // List loaded, from_t an integer
        json_array_extend(expected, loaded);
        JSON_DECREF(loaded)
        expect_refusal(expected, "time bound of a wrong type, refused");
        json_t *opened = expect_role_warnings(3);   // Iterator opened, List loaded, Iterator closed
        json_array_extend(expected, opened);
        JSON_DECREF(opened)
        json_t *not_there = expect_role_warnings(2);    // two print-tranger paths not there, no log
        json_array_extend(expected, not_there);
        JSON_DECREF(not_there)
        expect_refusal(expected, "topic of a wrong shape, create refused");  // a version, no jn_cols
        json_t *created = expect_role_warnings(1);  // the version WITH its jn_cols, taken
        json_array_extend(expected, created);
        JSON_DECREF(created)
        expect_refusal(expected, "topic of a wrong shape, create refused");  // a treedb's topic
        expect_refusal(expected, "record of a wrong shape, refused");       // __t__ of 45 bits
        expect_refusal(expected, "record of a wrong shape, refused");       // a tkey of 45 bits
        /*  c4 B: a tkey date past 44 bits, plain and sf_tm_ms; one it keeps, taken  */
        expect_refusal(expected, "record of a wrong shape, refused");
        expect_refusal(expected, "record of a wrong shape, refused");
        json_t *dated = expect_role_warnings(1);
        json_array_extend(expected, dated);
        JSON_DECREF(dated)
        /*  V46.3: a text in the tkey that is no date, twice  */
        expect_refusal(expected, "record of a wrong shape, refused");
        expect_refusal(expected, "record of a wrong shape, refused");
        /*  REV-CLOUD V20 #3: a peer's writes into a treedb's topic, a new topic in its store  */
        expect_refusal(expected, "write of a topic of a treedb or a msg2db, refused");  // add-record
        expect_refusal(expected, "write of a topic of a treedb or a msg2db, refused");  // delete-key
        expect_refusal(expected, "write of a topic of a treedb or a msg2db, refused");  // delete-topic
        expect_refusal(expected, "topic of a wrong shape, create refused");             // create-topic
        json_t *added = expect_role_warnings(1);    // the C caller's add-record, taken
        json_array_extend(expected, added);
        JSON_DECREF(added)
        /*  #4: a new topic named as what a store keeps  */
        for(int i = 0; i < 4; i++) {
            expect_refusal(expected, "topic of a wrong shape, create refused");
        }
        /*  #13: a jn_var of what the tranger keeps, or of a wrong type  */
        for(int i = 0; i < 4; i++) {
            expect_refusal(expected, "topic of a wrong shape, create refused");
        }
        /*  #16: a version of a topic with no columns, with an empty jn_cols; and
         *  without jn_cols, refused  */
        json_t *changed = expect_role_warnings(1);
        json_array_extend(expected, changed);
        JSON_DECREF(changed)
        expect_refusal(expected, "topic of a wrong shape, create refused");
        /*  V23.4.2: a NUL in the pkey, down the record, in its json text  */
        for(int i = 0; i < 3; i++) {
            expect_refusal(expected, "record of a wrong shape, refused");
        }
        set_expected_results("more of a peer's data is refused", expected, NULL, NULL, 1);
    }

    r = gobj_command(yuno, "create-topic",
        as_peer(json_pack("{s:s}", "topic_name", "__timeranger2__.json")), yuno);
    check_refused("create-topic named as a file of the database", r);
    check_bool("the file is still a file",
        file_exists(path_database, "__timeranger2__.json"), TRUE);

    r = gobj_command(yuno, "create-topic",
        as_peer(json_pack("{s:s, s:{s:I}}", "topic_name", "topic_sys",
            "jn_var", "topic_version", (json_int_t)99)), yuno);
    check_refused("create-topic of a new topic_version over a system topic", r);
    {
        char topic_sys_dir[PATH_MAX];
        build_path(topic_sys_dir, sizeof(topic_sys_dir), path_database, "topic_sys", NULL);
        json_t *topic_var = load_json_from_file(0, topic_sys_dir, "topic_var.json", 0);
        check_bool("the system topic keeps its mark on disk",
            json_boolean_value(json_object_get(topic_var, "system_topic")), TRUE);
        check_int("and its topic_version",
            kw_get_int(0, topic_var, "topic_version", 0, KW_WILD_NUMBER), 0);
        JSON_DECREF(topic_var)
    }

    r = gobj_command(yuno, "add-record",
        as_peer(json_pack("{s:s, s:i}", "topic_name", TOPIC_NAME, "record", 5)), yuno);
    check_refused("add-record of a record that is not a dict", r);

    r = gobj_command(yuno, "open-iterator",
        as_peer(json_pack("{s:s, s:s, s:s}", "iterator_id", "itPath", "topic_name", TOPIC_NAME,
            "key", "../keys/" KEY_A)), yuno);
    check_refused("open-iterator of a key that is a path", r);
    r = gobj_command(yuno, "open-list",
        as_peer(json_pack("{s:s, s:s, s:s, s:b}", "list_id", "lstPath", "topic_name", TOPIC_NAME,
            "key", "../keys/" KEY_A, "return_data", 1)), yuno);
    check_refused("open-list of a key that is a path", r);
    r = gobj_command(yuno, "open-rt",
        as_peer(json_pack("{s:s, s:s, s:s}", "rt_id", "rtPath", "topic_name", TOPIC_NAME,
            "key", "../x")), yuno);
    check_refused("open-rt of a key that is a path", r);

    /*  key C holds KEY_C_ROWS records at t BASE_T+0..3  */
    r = gobj_command(yuno, "open-list",
        as_peer(json_pack("{s:s, s:s, s:s, s:b, s:I}", "list_id", "lstFromT", "topic_name", TOPIC_NAME,
            "key", KEY_C, "return_data", 1, "from_t", (json_int_t)(BASE_T + 2))), yuno);
    check_int("open-list with an integer from_t", kw_get_int(0, r, "result", -999, 0), 0);
    check_int("open-list with an integer from_t: the records from it",
        json_array_size(kw_get_list(0, r, "data", 0, 0)), KEY_C_ROWS - 2);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-list",
        as_peer(json_pack("{s:s, s:s, s:s, s:b, s:{}}", "list_id", "lstBadT", "topic_name", TOPIC_NAME,
            "key", KEY_C, "return_data", 1, "from_t")), yuno);
    check_refused("open-list with a from_t that is a dict", r);

    r = gobj_command(yuno, "open-iterator",
        as_peer(json_pack("{s:s, s:s}", "topic_name", TOPIC_NAME, "key", KEY_C)), yuno);
    check_int("open-iterator of key C, named as its key",
        kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    r = gobj_command(yuno, "open-list",
        as_peer(json_pack("{s:s, s:s, s:s, s:b}", "list_id", gobj_name(yuno), "topic_name", TOPIC_NAME,
            "key", KEY_C, "return_data", 1)), yuno);
    check_int("a one-shot open-list named as the service",
        kw_get_int(0, r, "result", -999, 0), 0);
    check_int("reads the key an iterator of the service has open",
        json_array_size(kw_get_list(0, r, "data", 0, 0)), KEY_C_ROWS);
    JSON_DECREF(r)
    r = gobj_command(yuno, "close-iterator", as_peer(json_pack("{s:s}", "iterator_id", KEY_C)), yuno);
    check_int("close-iterator of key C", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    /*  A path that is not there, of any length or under a string, is said
     *  by the answer only: kw_find_path() walks it in a loop and, with
     *  verbose FALSE, logs nothing (REV-CLOUD V21.2.4). They were refused
     *  with a WARNING here, where kw_find_path() logged an ERROR.  */
    {
        char long_path[300 + sizeof("`y")];
        memset(long_path, 'x', 300);
        snprintf(long_path + 300, sizeof(long_path) - 300, "`y");
        r = gobj_command(yuno, "print-tranger", as_peer(json_pack("{s:s}", "path", long_path)), yuno);
        check_refused("print-tranger of a segment longer than 255 bytes: not found", r);
    }
    r = gobj_command(yuno, "print-tranger", as_peer(json_pack("{s:s}", "path", "directory`x")), yuno);
    check_refused("print-tranger of a path under a string: not found", r);
    r = gobj_command(yuno, "print-tranger", as_peer(json_pack("{s:s}", "path", "directory")), yuno);
    check_int("print-tranger of a string", kw_get_int(0, r, "result", -999, 0), 0);
    check_str("is the string", json_string_value(json_object_get(r, "data")), path_database);
    JSON_DECREF(r)

    /*
     *  A new topic_version of a topic that exists (REV-CLOUD V20 D4): it
     *  re-writes topic_cols.json from jn_cols, so without them the columns
     *  were written EMPTY; and over a topic of a treedb, whose schema is the
     *  treedb's, it took its cols and pkey2s for good. A plain topic with
     *  its jn_cols keeps the feature.
     */
    if(!tranger2_create_topic(tranger, "topic_plain", "id", "", NULL, sf_string_key,
            json_pack("{s:s, s:s}", "id", "", "name", ""), json_pack("{s:i}", "topic_version", 1))) {
        printf("%s: FAIL (create topic_plain)\n", APP);
        global_result += -1;
    }
    {
        char plain_dir[PATH_MAX];
        build_path(plain_dir, sizeof(plain_dir), path_database, "topic_plain", NULL);
        r = gobj_command(yuno, "create-topic",
            as_peer(json_pack("{s:s, s:{s:i}}", "topic_name", "topic_plain",
                "jn_var", "topic_version", 2)), yuno);
        check_refused("create-topic of a new topic_version without jn_cols", r);
        json_t *jn_cols_disk = load_json_from_file(0, plain_dir, "topic_cols.json", 0);
        check_int("the topic keeps its columns on disk", (json_int_t)json_object_size(jn_cols_disk), 2);
        JSON_DECREF(jn_cols_disk)
        r = gobj_command(yuno, "create-topic",
            as_peer(json_pack("{s:s, s:{s:i}, s:{s:s, s:s, s:s}}", "topic_name", "topic_plain",
                "jn_var", "topic_version", 2, "jn_cols", "id", "", "name", "", "more", "")), yuno);
        check_int("create-topic of a new topic_version with its jn_cols",
            kw_get_int(0, r, "result", -999, 0), 0);
        JSON_DECREF(r)
        jn_cols_disk = load_json_from_file(0, plain_dir, "topic_cols.json", 0);
        check_int("the topic takes its new columns", (json_int_t)json_object_size(jn_cols_disk), 3);
        JSON_DECREF(jn_cols_disk)
    }
    {
        /*
         *  A topic of a treedb: what treedb_open_db() leaves in the tranger
         *  for each topic of the treedb, `treedbs`.<treedb>.<topic> (its
         *  indexes). Put there by hand: a real treedb on this tranger reads
         *  the plain topics of the test as its own, whose columns are no
         *  json objects.
         */
        json_t *treedbs = json_object();
        json_object_set_new(treedbs, "tdb", json_pack("{s:{}}", "topic_plain"));
        json_object_set_new(tranger, "treedbs", treedbs);
        char plain_dir[PATH_MAX];
        build_path(plain_dir, sizeof(plain_dir), path_database, "topic_plain", NULL);
        r = gobj_command(yuno, "create-topic",
            as_peer(json_pack("{s:s, s:{s:i}, s:{s:s}}", "topic_name", "topic_plain",
                "jn_var", "topic_version", 99, "jn_cols", "id", "")), yuno);
        check_refused("create-topic of a new topic_version over a topic of a treedb", r);
        json_t *topic_var = load_json_from_file(0, plain_dir, "topic_var.json", 0);
        check_int("the treedb's topic keeps its topic_version",
            kw_get_int(0, topic_var, "topic_version", 0, KW_WILD_NUMBER), 2);
        JSON_DECREF(topic_var)
        json_object_del(tranger, "treedbs");
    }

    /*
     *  A time a record cannot keep (REV-CLOUD V20 D8): the md2 row holds 44
     *  bits of __t__ and of __tm__, the flags were written over the rest
     */
    r = gobj_command(yuno, "add-record",
        as_peer(json_pack("{s:s, s:I, s:{s:s}}", "topic_name", TOPIC_NAME,
            "__t__", (json_int_t)TRANGER2_MAX_TIME + 1, "record", "id", "K")), yuno);
    check_refused("add-record with a __t__ of more than 44 bits", r);
    r = gobj_command(yuno, "add-record",
        as_peer(json_pack("{s:s, s:{s:s, s:I}}", "topic_name", TOPIC_NAME,
            "record", "id", "K", "tm", (json_int_t)TRANGER2_MAX_TIME + 1)), yuno);
    check_refused("add-record with a tkey time of more than 44 bits", r);
    check_int("no refused add-record added a record (44 bits)",
        (json_int_t)tranger2_topic_key_size(tranger, TOPIC_NAME, "K"), 2);

    /*
     *  And a tkey that is a DATE (REV-CLOUD V20 c4 B): approxidate() reads
     *  "@<seconds> +0000" with no year limit, and the row kept 44 bits of
     *  it -- of it times 1000 on an sf_tm_ms topic, where 2e10 seconds is
     *  already past them. A date it can keep is taken.
     */
    r = gobj_command(yuno, "add-record",
        as_peer(json_pack("{s:s, s:{s:s, s:s}}", "topic_name", TOPIC_NAME,
            "record", "id", "K", "tm", "@99999999999999 +0000")), yuno);
    check_refused("add-record with a tkey date past 44 bits", r);
    check_int("no refused add-record added a record (a date past 44 bits)",
        (json_int_t)tranger2_topic_key_size(tranger, TOPIC_NAME, "K"), 2);
    if(!tranger2_create_topic(tranger, "topic_tm_ms", "id", "tm", NULL, sf_string_key|sf_tm_ms,
            json_pack("{s:s, s:I}", "id", "", "tm", (json_int_t)0), 0)) {
        printf("%s: FAIL (create topic_tm_ms)\n", APP);
        global_result += -1;
    }
    r = gobj_command(yuno, "add-record",
        as_peer(json_pack("{s:s, s:{s:s, s:s}}", "topic_name", "topic_tm_ms",
            "record", "id", "M", "tm", "@20000000000 +0000")), yuno);
    check_refused("add-record with a tkey date past 44 bits in ms (sf_tm_ms)", r);
    r = gobj_command(yuno, "add-record",
        as_peer(json_pack("{s:s, s:{s:s, s:s}}", "topic_name", "topic_tm_ms",
            "record", "id", "M", "tm", "@1700000000 +0000")), yuno);
    check_int("add-record with a tkey date it can keep (sf_tm_ms)",
        kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    check_int("only the date it can keep was added (sf_tm_ms)",
        (json_int_t)tranger2_topic_key_size(tranger, "topic_tm_ms", "M"), 1);

    /*
     *  A text in the tkey that is no date (REV-CLOUD V46.3): approxidate()
     *  read it as NOW, and "1761000000000" (milliseconds, past the year
     *  9999) was appended with NOW*1000 in silence
     */
    static const char *not_dates[] = {"1761000000000", "garbage"};
    for(int i = 0; i < 2; i++) {
        r = gobj_command(yuno, "add-record",
            as_peer(json_pack("{s:s, s:{s:s, s:s}}", "topic_name", "topic_tm_ms",
                "record", "id", "M", "tm", not_dates[i])), yuno);
        const char *comment = kw_get_str(0, r, "comment", "", 0);
        check_bool("add-record with a tkey that is no date says it",
            strstr(comment, "a text in its tkey that is no date")? TRUE : FALSE, TRUE);
        check_refused("add-record with a tkey that is no date (sf_tm_ms)", r);
    }
    check_int("no tkey that is no date was added (sf_tm_ms)",
        (json_int_t)tranger2_topic_key_size(tranger, "topic_tm_ms", "M"), 1);

    /*
     *  REV-CLOUD V20 #3 (G): the topics of a treedb or a msg2db open on the
     *  tranger are written through it, never by a peer's add-record,
     *  delete-key or delete-topic: a raw record appended to `users` was that
     *  node at the next load. Nor does a peer make a new topic in its store.
     *  A C caller is its own owner. The treedb is put by hand, as above.
     */
    {
        json_t *treedbs = json_object();
        json_object_set_new(treedbs, "tdb", json_pack("{s:{}}", "topic_plain"));
        json_object_set_new(tranger, "treedbs", treedbs);
        r = gobj_command(yuno, "add-record",
            as_peer(json_pack("{s:s, s:{s:s}}", "topic_name", "topic_plain", "record", "id", "raw")),
            yuno);
        check_refused("add-record of a peer into a topic of a treedb", r);
        check_int("nothing was appended to the treedb's topic",
            (json_int_t)tranger2_topic_key_size(tranger, "topic_plain", "raw"), 0);
        r = gobj_command(yuno, "delete-key",
            as_peer(json_pack("{s:s, s:s, s:b}", "topic_name", "topic_plain", "key", "raw",
                "force", 1)), yuno);
        check_refused("delete-key of a peer in a topic of a treedb", r);
        r = gobj_command(yuno, "delete-topic",
            as_peer(json_pack("{s:s, s:b}", "topic_name", "topic_plain", "force", 1)), yuno);
        check_refused("delete-topic of a peer of a topic of a treedb", r);
        check_bool("the treedb's topic is still there",
            tranger2_topic_is_open(tranger, "topic_plain"), TRUE);
        r = gobj_command(yuno, "create-topic",
            as_peer(json_pack("{s:s, s:{s:s}}", "topic_name", "fresh", "jn_cols", "id", "")), yuno);
        check_refused("create-topic of a peer of a new topic in the store of a treedb", r);
        check_bool("no topic was made in the treedb's store",
            tranger2_topic_is_open(tranger, "fresh"), FALSE);
        r = gobj_command(yuno, "add-record",
            json_pack("{s:s, s:{s:s}}", "topic_name", "topic_plain", "record", "id", "by_c"), yuno);
        check_int("the C caller's add-record into the treedb's topic",
            kw_get_int(0, r, "result", -999, 0), 0);
        JSON_DECREF(r)
        json_object_del(tranger, "treedbs");
    }

    /*
     *  #4 (A, J): a NEW topic named as what a store keeps beside its topics:
     *  hidden files, the system's namespace, its .json files, backups
     */
    {
        const char *reserved[] = {".hidden", "__x", "x.json", "x.bak", NULL};
        for(int i = 0; reserved[i]; i++) {
            r = gobj_command(yuno, "create-topic",
                as_peer(json_pack("{s:s, s:{s:s}}", "topic_name", reserved[i], "jn_cols", "id", "")),
                yuno);
            check_refused(reserved[i], r);
            check_bool("no topic was made of a name of the store",
                tranger2_topic_is_open(tranger, reserved[i]), FALSE);
        }
    }

    /*
     *  #13 (L): the jn_var of a peer: no field the tranger keeps (it made
     *  the topic undeletable: system_topic), and the types the tranger
     *  reads (an ERROR with a stack at the next change of version)
     */
    {
        json_t *bad_vars[] = {
            json_pack("{s:b}", "system_topic", 1),
            json_pack("{s:i, s:s}", "topic_version", 1, "last_rowid_id", "x"),
            json_pack("{s:s}", "directory", "/tmp"),
            json_pack("{s:s}", "topic_version", "2"),
            NULL
        };
        for(int i = 0; bad_vars[i]; i++) {
            r = gobj_command(yuno, "create-topic",
                as_peer(json_pack("{s:s, s:o, s:{s:s}}", "topic_name", "topic_var_x",
                    "jn_var", bad_vars[i], "jn_cols", "id", "")), yuno);
            check_refused("create-topic with a jn_var of the tranger's", r);
        }
        check_bool("no topic was made with a jn_var of the tranger's",
            tranger2_topic_is_open(tranger, "topic_var_x"), FALSE);
    }

    /*
     *  #16 (c3#1d): a new topic_version of a topic with no columns BY DESIGN
     *  (timeranger2 columns are optional) takes an empty jn_cols: there is
     *  nothing to wipe. Without jn_cols it is refused, before the tranger's
     *  ERROR
     */
    if(!tranger2_create_topic(tranger, "topic_nocols", "id", "", NULL, sf_string_key,
            json_object(), json_pack("{s:i}", "topic_version", 1))) {
        printf("%s: FAIL (create topic_nocols)\n", APP);
        global_result += -1;
    }
    {
        r = gobj_command(yuno, "create-topic",
            as_peer(json_pack("{s:s, s:{s:i}, s:{}}", "topic_name", "topic_nocols",
                "jn_var", "topic_version", 2, "jn_cols")), yuno);
        check_int("a new version of a topic with no columns, with an empty jn_cols",
            kw_get_int(0, r, "result", -999, 0), 0);
        JSON_DECREF(r)
        char nocols_dir[PATH_MAX];
        build_path(nocols_dir, sizeof(nocols_dir), path_database, "topic_nocols", NULL);
        json_t *topic_var = load_json_from_file(0, nocols_dir, "topic_var.json", 0);
        check_int("the topic with no columns takes its new version",
            kw_get_int(0, topic_var, "topic_version", 0, KW_WILD_NUMBER), 2);
        JSON_DECREF(topic_var)
        r = gobj_command(yuno, "create-topic",
            as_peer(json_pack("{s:s, s:{s:i}}", "topic_name", "topic_nocols",
                "jn_var", "topic_version", 3)), yuno);
        check_refused("a new version without jn_cols", r);
    }

    /*
     *  REV-CLOUD V23.4.2: a NUL in a string of a peer's record (a frame is
     *  decoded with JSON_ALLOW_NUL). C reads a string up to it: the pkey
     *  "K\u0000evil" was appended under the key "K". Refused, at any depth,
     *  and in the json text of the record too, which was parsed up to it.
     */
    {
        json_int_t k_rows = (json_int_t)tranger2_topic_key_size(tranger, TOPIC_NAME, "K");
        r = gobj_command(yuno, "add-record",
            as_peer(json_pack("{s:s, s:{s:s%, s:I}}", "topic_name", TOPIC_NAME,
                "record", "id", "K\0evil", (size_t)6, "tm", (json_int_t)(BASE_T + 6000))), yuno);
        check_bool("add-record of a pkey with a NUL: refused, saying why",
            strstr(kw_get_str(0, r, "comment", "", 0),
                "record refused by topic 'topic_cmd': a string with a NUL in it")? TRUE : FALSE, TRUE);
        check_refused("add-record of a pkey with a NUL", r);
        r = gobj_command(yuno, "add-record",
            as_peer(json_pack("{s:s, s:{s:s, s:I, s:{s:[s%]}}}", "topic_name", TOPIC_NAME,
                "record", "id", "K", "tm", (json_int_t)(BASE_T + 6000),
                "content", "list", "a\0b", (size_t)3)), yuno);
        check_refused("add-record of a record with a NUL down it", r);
        r = gobj_command(yuno, "add-record",
            as_peer(json_pack("{s:s, s:s%}", "topic_name", TOPIC_NAME,
                "record", "{\"id\":\"K\",\"tm\":946690000}\0junk", (size_t)30)), yuno);
        check_refused("add-record of the json text of a record with a NUL", r);
        check_int("the refused records added nothing",
            (json_int_t)tranger2_topic_key_size(tranger, TOPIC_NAME, "K"), k_rows);
    }

    global_result += test_json(NULL);
    gobj_log_del_handler("test_capture");
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_ERROR, 0);

    /*-------------------------------------------------*
     *      delete-key-files: retention by hand. The old files of a key go,
     *      the g_rowid of the rest stay. Its refusals, from a peer, are
     *      WARNINGs (not captured here), but a key not there: it is no
     *      malformed data, and is answered with no log (as delete-key)
     *-------------------------------------------------*/
    set_expected_results(
        "delete-key-files",
        json_pack("[{s:s}, {s:s}, {s:s}]",
            "msg", "record of a purged period of its key, refused",    // the C caller's add-record
            "msg", "purge of a topic of a treedb or a msg2db, refused", // from C too
            "msg", "topic whose files do not sort as time, refused"     // from C
        ),
        NULL, NULL, 1
    );
    {
        json_t *topic5 = tranger2_create_topic(
            tranger, TOPIC_NAME5, "id", "tm",
            json_pack("{s:s}", "filename_mask", "%Y-%m-%d"),
            sf_string_key,
            json_pack("{s:s, s:I, s:s}", "id", "", "tm", (json_int_t)0, "content", ""),
            0
        );
        check_bool("topic of one file a day", topic5 != NULL, TRUE);
        for(int d = 0; d < 3; d++) {
            for(int j = 0; j < 2; j++) {
                uint64_t t = BASE_T + (uint64_t)d*86400 + (uint64_t)j;
                json_t *jn_record = json_pack("{s:s, s:I, s:s}",
                    "id", "P", "tm", (json_int_t)t, "content", "day");
                md2_record_ex_t md = {0};
                tranger2_append_record(tranger, TOPIC_NAME5, t, 0, &md, jn_record);
            }
        }

        r = gobj_command(yuno, "delete-key-files",
            json_pack("{s:s, s:s, s:s}",
                "topic_name", TOPIC_NAME5, "key", "P", "before", "2000-01-02T00:00:00+0000"
            ), yuno);
        check_int("delete-key-files by date", kw_get_int(0, r, "result", -999, 0), 0);
        check_int("delete-key-files by date: rows", kw_get_int(0, r, "data`rows", -1, 0), 2);
        JSON_DECREF(r)
        check_int("the key keeps the rest",
            (json_int_t)tranger2_topic_key_size(tranger, TOPIC_NAME5, "P"), 4);

        r = gobj_command(yuno, "delete-key-files",
            json_pack("{s:s, s:I}",
                "topic_name", TOPIC_NAME5, "before", (json_int_t)(BASE_T + 2*86400)
            ), yuno);
        check_int("delete-key-files of every key", kw_get_int(0, r, "result", -999, 0), 0);
        check_int("delete-key-files of every key: rows", kw_get_int(0, r, "data`rows", -1, 0), 2);
        check_int("delete-key-files of every key: keys", kw_get_int(0, r, "data`keys_purged", -1, 0), 1);
        JSON_DECREF(r)

        r = gobj_command(yuno, "open-list",
            json_pack("{s:s, s:s, s:s, s:b}",
                "list_id", "lstPurged", "topic_name", TOPIC_NAME5, "key", "P", "return_data", 1
            ), yuno);
        json_t *rows_left = kw_get_list(0, r, "data", 0, 0);
        check_int("what is left", (json_int_t)json_array_size(rows_left), 2);
        check_int("its first g_rowid", record_rowid(json_array_get(rows_left, 0)), 5);
        JSON_DECREF(r)

        r = gobj_command(yuno, "list-keys",
            json_pack("{s:s}", "topic_name", TOPIC_NAME5), yuno);
        check_int("list-keys says the rows purged",
            find_key_field(kw_get_list(0, r, "data", 0, 0), "P", "purged"), 4);
        JSON_DECREF(r)

        r = gobj_command(yuno, "delete-key-files",
            as_peer(json_pack("{s:s, s:s, s:I}",
                "topic_name", TOPIC_NAME5, "key", "P",
                "before", (json_int_t)time_in_seconds() + 86400
            )), yuno);
        check_refused("delete-key-files in the future", r);
        r = gobj_command(yuno, "delete-key-files",
            as_peer(json_pack("{s:s, s:s, s:s}",
                "topic_name", TOPIC_NAME5, "key", "P", "before", "no date at all"
            )), yuno);
        check_refused("delete-key-files of a text that is no date", r);
        r = gobj_command(yuno, "delete-key-files",
            as_peer(json_pack("{s:s, s:s}", "topic_name", TOPIC_NAME5, "key", "P")), yuno);
        check_refused("delete-key-files with no before", r);
        r = gobj_command(yuno, "delete-key-files",
            as_peer(json_pack("{s:s, s:s, s:I}",
                "topic_name", TOPIC_NAME5, "key", "never_existed", "before", (json_int_t)BASE_T
            )), yuno);
        check_bool("delete-key-files of a key not there: Key not found",
            strstr(kw_get_str(0, r, "comment", "", 0), "Key not found in topic") != NULL, TRUE);
        check_refused("delete-key-files of a key not there", r);
        r = gobj_command(yuno, "delete-key-files",
            as_peer(json_pack("{s:s, s:s, s:I}",
                "topic_name", TOPIC_NAME5, "key", "../x", "before", (json_int_t)BASE_T
            )), yuno);
        check_bool("delete-key-files of a key that is a path: the answer of the other commands",
            strstr(kw_get_str(0, r, "comment", "", 0), "is not one directory component") != NULL, TRUE);
        check_refused("delete-key-files of a key that is a path", r);
        check_int("the refusals deleted nothing",
            (json_int_t)tranger2_topic_key_size(tranger, TOPIC_NAME5, "P"), 2);

        /*
         *  archive-key-files: the same purge, the files kept in a batch of
         *  the archive_directory attribute. With none configured, refused
         *  with no log (no malformed data: a configuration missing)
         */
        for(int d = 0; d < 3; d++) {
            uint64_t t = BASE_T + (uint64_t)d*86400;
            json_t *jn_record = json_pack("{s:s, s:I, s:s}",
                "id", "ARC", "tm", (json_int_t)t, "content", "day");
            md2_record_ex_t md = {0};
            tranger2_append_record(tranger, TOPIC_NAME5, t, 0, &md, jn_record);
        }
        r = gobj_command(yuno, "archive-key-files",
            as_peer(json_pack("{s:s, s:s, s:I}",
                "topic_name", TOPIC_NAME5, "key", "ARC", "before", (json_int_t)(BASE_T + 86400)
            )), yuno);
        check_bool("archive-key-files with no archive_directory: it says so",
            strstr(kw_get_str(0, r, "comment", "", 0), "no archive_directory configured") != NULL, TRUE);
        check_refused("archive-key-files with no archive_directory", r);

        char path_archive[PATH_MAX];
        build_path(path_archive, sizeof(path_archive), path_root, "tr_c_tranger_cmd_archive", NULL);
        rmrdir(path_archive);
        mkrdir(path_archive, 02770);
        gobj_write_str_attr(yuno, "archive_directory", path_archive);
        r = gobj_command(yuno, "archive-key-files",
            json_pack("{s:s, s:s, s:I}",
                "topic_name", TOPIC_NAME5, "key", "ARC", "before", (json_int_t)(BASE_T + 86400)
            ), yuno);
        check_int("archive-key-files", kw_get_int(0, r, "result", -999, 0), 0);
        check_int("archive-key-files: rows", kw_get_int(0, r, "data`rows", -1, 0), 1);
        check_bool("archive-key-files: it says archived",
            strstr(kw_get_str(0, r, "comment", "", 0), "rows archived") != NULL, TRUE);
        JSON_DECREF(r)
        check_int("the key keeps the rest",
            (json_int_t)tranger2_topic_key_size(tranger, TOPIC_NAME5, "ARC"), 2);
        {
            char key_archive[PATH_MAX];
            build_path(key_archive, sizeof(key_archive), path_archive, DATABASE, TOPIC_NAME5, "ARC", NULL);
            int batches = 0;
            DIR *dir = opendir(key_archive);
            struct dirent *entry;
            while(dir && (entry = readdir(dir)) != NULL) {
                if(entry->d_name[0] != '.') {
                    batches++;
                }
            }
            if(dir) {
                closedir(dir);
            }
            check_int("archive-key-files: one batch", batches, 1);
        }
        gobj_write_str_attr(yuno, "archive_directory", "");
        rmrdir(path_archive);

        /*
         *  A record of a purged period is refused BEFORE the tranger: the
         *  cause in the answer, a WARNING for a peer (not captured here), an
         *  ERROR for a C caller -- not the tranger's ERROR and "see the log"
         */
        r = gobj_command(yuno, "add-record",
            as_peer(json_pack("{s:s, s:I, s:{s:s, s:I, s:s}}",
                "topic_name", TOPIC_NAME5, "__t__", (json_int_t)(BASE_T + 10),
                "record", "id", "P", "tm", (json_int_t)(BASE_T + 10), "content", "late"
            )), yuno);
        check_int("add-record of a purged period", kw_get_int(0, r, "result", -999, 0), -1);
        check_bool("it says why",
            strstr(kw_get_str(0, r, "comment", "", 0), "purged period") != NULL, TRUE);
        JSON_DECREF(r)
        r = gobj_command(yuno, "add-record",
            json_pack("{s:s, s:I, s:{s:s, s:I, s:s}}",
                "topic_name", TOPIC_NAME5, "__t__", (json_int_t)(BASE_T + 86400 + 10),
                "record", "id", "P", "tm", (json_int_t)(BASE_T + 86400 + 10), "content", "late"
            ), yuno);
        check_int("add-record of a purged period, from C", kw_get_int(0, r, "result", -999, 0), -1);
        JSON_DECREF(r)
        {
            json_t *record_p = json_pack("{s:s}", "id", "P");
            check_bool("tranger2_record_in_purged_period(): day 2, purged",
                tranger2_record_in_purged_period(tranger, TOPIC_NAME5, BASE_T + 86400, record_p), TRUE);
            check_bool("tranger2_record_in_purged_period(): day 3, kept",
                tranger2_record_in_purged_period(tranger, TOPIC_NAME5, BASE_T + 2*86400, record_p), FALSE);
            check_bool("tranger2_record_in_purged_period(): now",
                tranger2_record_in_purged_period(tranger, TOPIC_NAME5, 0, record_p), FALSE);
            JSON_DECREF(record_p)
        }
        r = gobj_command(yuno, "add-record",
            as_peer(json_pack("{s:s, s:I, s:{s:s, s:I, s:s}}",
                "topic_name", TOPIC_NAME5, "__t__", (json_int_t)(BASE_T + 2*86400 + 10),
                "record", "id", "P", "tm", (json_int_t)(BASE_T + 2*86400 + 10), "content", "day3"
            )), yuno);
        check_int("add-record of the day after the purge", kw_get_int(0, r, "result", -999, 0), 0);
        check_int("its g_rowid follows the base", kw_get_int(0, r, "data`rowid", -1, 0), 7);
        JSON_DECREF(r)

        /*
         *  Refused before the tranger from a C caller too: a topic of a
         *  treedb (its keys are its nodes), and a file name mask whose ids do
         *  not sort as time
         */
        {
            json_t *treedbs = json_object_get(tranger, "treedbs");
            BOOL made = FALSE;
            if(!treedbs) {
                treedbs = json_object();
                json_object_set_new(tranger, "treedbs", treedbs);
                made = TRUE;
            }
            json_object_set_new(treedbs, "treedb_fake", json_pack("{s:{}}", TOPIC_NAME5));
            r = gobj_command(yuno, "delete-key-files",
                json_pack("{s:s, s:s, s:I}",
                    "topic_name", TOPIC_NAME5, "key", "P", "before", (json_int_t)(BASE_T + 3*86400)
                ), yuno);
            check_bool("delete-key-files of a treedb's topic, from C: it says why",
                strstr(kw_get_str(0, r, "comment", "", 0), "is a topic of a treedb") != NULL, TRUE);
            check_refused("delete-key-files of a treedb's topic, from C", r);
            json_object_del(treedbs, "treedb_fake");
            if(made) {
                json_object_del(tranger, "treedbs");
            }
        }
        json_t *topic6 = tranger2_create_topic(
            tranger, TOPIC_NAME6, "id", "tm",
            json_pack("{s:s}", "filename_mask", "%d-%m-%Y"),
            sf_string_key,
            json_pack("{s:s, s:I, s:s}", "id", "", "tm", (json_int_t)0, "content", ""),
            0
        );
        check_bool("topic of a mask that does not sort", topic6 != NULL, TRUE);
        {
            md2_record_ex_t md = {0};
            tranger2_append_record(tranger, TOPIC_NAME6, BASE_T, 0, &md,
                json_pack("{s:s, s:I, s:s}", "id", "M", "tm", (json_int_t)BASE_T, "content", "m"));
        }
        r = gobj_command(yuno, "delete-key-files",
            json_pack("{s:s, s:s, s:I}",
                "topic_name", TOPIC_NAME6, "key", "M", "before", (json_int_t)(BASE_T + 3*86400)
            ), yuno);
        check_bool("delete-key-files of a mask that does not sort: it says why",
            strstr(kw_get_str(0, r, "comment", "", 0), "do not sort as time") != NULL, TRUE);
        check_refused("delete-key-files of a mask that does not sort", r);
        check_int("the refused purges deleted nothing",
            (json_int_t)tranger2_topic_key_size(tranger, TOPIC_NAME6, "M"), 1);

        /*
         *  The pages of a multi-key iterator across a purge: an unfiltered
         *  part keeps its positions (no live row skipped); a filtered one
         *  cannot, and the iterator is closed, saying so
         */
        const char *keys_qr[] = {"Q", "R"};
        for(int k = 0; k < 2; k++) {
            for(int d = 0; d < 3; d++) {
                for(int j = 0; j < 2; j++) {
                    uint64_t t = BASE_T + (uint64_t)d*86400 + (uint64_t)j;
                    char content[8];
                    snprintf(content, sizeof(content), "%s%c%d", keys_qr[k], 'a' + d, j + 1);
                    md2_record_ex_t md = {0};
                    tranger2_append_record(tranger, TOPIC_NAME5, t, 0, &md,
                        json_pack("{s:s, s:I, s:s}", "id", keys_qr[k], "tm", (json_int_t)t, "content", content));
                }
            }
        }
        r = gobj_command(yuno, "open-iterator",
            json_pack("{s:s, s:s, s:s}",
                "topic_name", TOPIC_NAME5, "rkey", "^(Q|R)$", "iterator_id", "itQR"), yuno);
        check_int("open-iterator Q|R", kw_get_int(0, r, "result", -999, 0), 0);
        JSON_DECREF(r)
        r = gobj_command(yuno, "open-iterator",
            json_pack("{s:s, s:s, s:s, s:I}",
                "topic_name", TOPIC_NAME5, "rkey", "^(Q|R)$", "iterator_id", "itQRf",
                "from_t", (json_int_t)BASE_T), yuno);
        check_int("open-iterator Q|R filtered", kw_get_int(0, r, "result", -999, 0), 0);
        JSON_DECREF(r)

        r = gobj_command(yuno, "delete-key-files",
            json_pack("{s:s, s:I}", "topic_name", TOPIC_NAME5, "before", (json_int_t)(BASE_T + 86400)), yuno);
        check_int("purge the first day of Q and R", kw_get_int(0, r, "data`rows", -1, 0), 4);
        JSON_DECREF(r)

        r = gobj_command(yuno, "get-page",
            json_pack("{s:s, s:i, s:i}", "iterator_id", "itQR", "from_rowid", 5, "limit", 4), yuno);
        check_int("multi-key page after the purge", kw_get_int(0, r, "result", -999, 0), 0);
        json_t *qr = kw_get_list(0, r, "data`data", 0, 0);
        char got_qr[128] = "";
        int iq; json_t *rec;
        json_array_foreach(qr, iq, rec) {
            if(got_qr[0]) {
                strncat(got_qr, " ", sizeof(got_qr) - strlen(got_qr) - 1);
            }
            strncat(got_qr, kw_get_str(0, rec, "content", "?", 0), sizeof(got_qr) - strlen(got_qr) - 1);
        }
        check_str("its positions stay: Q's last day, R's purged ones empty", got_qr, "Qc1 Qc2");
        check_int("its total: the positions of the open", kw_get_int(0, r, "data`total_rows", -1, 0), 12);
        JSON_DECREF(r)
        r = gobj_command(yuno, "close-iterator", json_pack("{s:s}", "iterator_id", "itQR"), yuno);
        JSON_DECREF(r)

        r = gobj_command(yuno, "get-page",
            json_pack("{s:s, s:i, s:i}", "iterator_id", "itQRf", "from_rowid", 1, "limit", 4), yuno);
        check_int("a filtered multi-key page after the purge", kw_get_int(0, r, "result", -999, 0), -1);
        check_bool("it says why",
            strstr(kw_get_str(0, r, "comment", "", 0), "were purged since it opened") != NULL, TRUE);
        JSON_DECREF(r)
    }
    global_result += test_json(NULL);

    /*-------------------------------------------------*
     *      A record of a purged period of a key flagged unlisted (its
     *      base could not be read when the topic opened, and can be now)
     *      is refused before the tranger too. Its cache has no base, and
     *      tranger2_record_in_purged_period() said FALSE: the append
     *      listed the key again, found the base, and the record reached
     *      the tranger's ERROR ("see the log").
     *-------------------------------------------------*/
    if(geteuid() != 0) {
        set_expected_results(
            "a purged period of a key flagged unlisted",
            json_pack("[{s:s}, {s:s}, {s:s}]",
                "msg", "Cannot read the base of a purged key",   // the open
                "msg", "The base of a purged key cannot be read: the key is flagged unlisted, every load of it says load_failed",
                "msg", "record of a purged period of its key, refused"  // the C caller's add-record
            ),
            NULL, NULL, 1
        );
        json_t *topic7 = tranger2_create_topic(
            tranger, TOPIC_NAME7, "id", "tm",
            json_pack("{s:s}", "filename_mask", "%Y-%m-%d"),
            sf_string_key,
            json_pack("{s:s, s:I, s:s}", "id", "", "tm", (json_int_t)0, "content", ""),
            0
        );
        check_bool("topic of the unlisted key", topic7 != NULL, TRUE);
        for(int d = 0; d < 3; d++) {
            for(int j = 0; j < 2; j++) {
                uint64_t t = BASE_T + (uint64_t)d*86400 + (uint64_t)j;
                md2_record_ex_t md = {0};
                tranger2_append_record(tranger, TOPIC_NAME7, t, 0, &md,
                    json_pack("{s:s, s:I, s:s}", "id", "U", "tm", (json_int_t)t, "content", "day"));
            }
        }
        r = gobj_command(yuno, "delete-key-files",
            json_pack("{s:s, s:s, s:I}",
                "topic_name", TOPIC_NAME7, "key", "U", "before", (json_int_t)(BASE_T + 86400)
            ), yuno);
        check_int("purge the first day of U", kw_get_int(0, r, "data`rows", -1, 0), 2);
        JSON_DECREF(r)

        char purged_u[PATH_MAX];
        build_path(purged_u, sizeof(purged_u), path_database, TOPIC_NAME7, "keys", "U", ".purged", NULL);
        tranger2_close_topic(tranger, TOPIC_NAME7);
        chmod(purged_u, 0);
        topic7 = tranger2_open_topic(tranger, TOPIC_NAME7, TRUE);
        chmod(purged_u, 0600);
        check_bool("the key is flagged unlisted",
            json_is_true(json_object_get(
                json_object_get(json_object_get(topic7, "cache"), "U"), "unlisted"
            )), TRUE);

        {
            json_t *record_u = json_pack("{s:s}", "id", "U");
            check_bool("tranger2_record_in_purged_period(): day 1 of a key unlisted, purged",
                tranger2_record_in_purged_period(tranger, TOPIC_NAME7, BASE_T + 10, record_u), TRUE);
            check_bool("tranger2_record_in_purged_period(): day 2 of a key unlisted, kept",
                tranger2_record_in_purged_period(tranger, TOPIC_NAME7, BASE_T + 86400, record_u), FALSE);
            JSON_DECREF(record_u)
        }
        r = gobj_command(yuno, "add-record",
            as_peer(json_pack("{s:s, s:I, s:{s:s, s:I, s:s}}",
                "topic_name", TOPIC_NAME7, "__t__", (json_int_t)(BASE_T + 10),
                "record", "id", "U", "tm", (json_int_t)(BASE_T + 10), "content", "late"
            )), yuno);
        check_int("add-record of a purged period, key unlisted", kw_get_int(0, r, "result", -999, 0), -1);
        check_bool("it says why, key unlisted",
            strstr(kw_get_str(0, r, "comment", "", 0), "purged period") != NULL, TRUE);
        JSON_DECREF(r)
        r = gobj_command(yuno, "add-record",
            json_pack("{s:s, s:I, s:{s:s, s:I, s:s}}",
                "topic_name", TOPIC_NAME7, "__t__", (json_int_t)(BASE_T + 20),
                "record", "id", "U", "tm", (json_int_t)(BASE_T + 20), "content", "late"
            ), yuno);
        check_int("add-record of a purged period, key unlisted, from C", kw_get_int(0, r, "result", -999, 0), -1);
        JSON_DECREF(r)
        check_bool("the refusals left the key as it was: unlisted",
            json_is_true(json_object_get(
                json_object_get(json_object_get(topic7, "cache"), "U"), "unlisted"
            )), TRUE);
        r = gobj_command(yuno, "add-record",
            as_peer(json_pack("{s:s, s:I, s:{s:s, s:I, s:s}}",
                "topic_name", TOPIC_NAME7, "__t__", (json_int_t)(BASE_T + 2*86400 + 10),
                "record", "id", "U", "tm", (json_int_t)(BASE_T + 2*86400 + 10), "content", "day3"
            )), yuno);
        check_int("add-record after the purge, key unlisted", kw_get_int(0, r, "result", -999, 0), 0);
        check_int("its g_rowid follows the base", kw_get_int(0, r, "data`rowid", -1, 0), 7);
        JSON_DECREF(r)
        global_result += test_json(NULL);
    }

    /*-------------------------------------------------*
     *      A master that LOST its lock (another process took the store
     *      while it was stopped) goes on as a replica: its tranger says
     *      `master` false. (7.25.4: the `master` attribute of C_TRANGER
     *      went on saying TRUE, and the service went on opening its feeds
     *      as a master's -- rt_mem, which only a local append fires.)
     *      LAST: the service stays a replica.
     *-------------------------------------------------*/
    set_expected_results(
        "a master that lost its lock reads as a replica",
        json_pack("[{s:s}, {s:s}]",
            "msg", "Master lock NOT retaken after a stop: another process holds it, go on as not master",
            "msg", "Cannot append record, NO master"
        ),
        NULL, NULL, 1
    );
    tranger2_stop(tranger);
    json_t *other_master = tranger2_startup(
        0,
        json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", LOG_OPT_TRACE_STACK
        ),
        0
    );
    append_one(tranger, KEY_A, BASE_T + 2000);  /*  the write that finds the lock taken  */
    check_bool("the tranger says it is not the master",
        kw_get_bool(0, tranger, "master", 0, 0), FALSE);
    check_bool("C_TRANGER says it is not the master",
        gobj_read_bool_attr(yuno, "master"), FALSE);
    r = gobj_command(yuno, "open-rt",
        json_pack("{s:s, s:s, s:s}",
            "rt_id", "rtLOST",
            "topic_name", TOPIC_NAME,
            "key", KEY_A
        ), yuno);
    check_int("open-rt after the lock was lost", kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)
    check_bool("its feed follows the disk, as a replica's",
        tranger2_get_rt_disk_by_id(tranger, TOPIC_NAME, "rtLOST", gobj_name(yuno)) != NULL, TRUE);
    r = gobj_command(yuno, "close-rt",
        json_pack("{s:s}", "rt_id", "rtLOST"), yuno);
    JSON_DECREF(r)
    /*  A replica does not append: master only, refused before the library  */
    r = gobj_command(yuno, "add-record",
        json_pack("{s:s, s:{s:s, s:I}}", "topic_name", TOPIC_NAME,
            "record", "id", "K", "tm", (json_int_t)BASE_T), yuno);
    check_int("add-record on a replica", kw_get_int(0, r, "result", -999, 0), -1);
    check_bool("add-record on a replica says READ-ONLY",
        strstr(kw_get_str(0, r, "comment", "", 0), "READ-ONLY") != NULL, TRUE);
    JSON_DECREF(r)
    /*  Nor does it delete, or create a topic: refused before the library,
     *  which logged an ERROR ("Only master can delete", "Cannot open
     *  TimeRanger topic. Not found and no master") (REV-CLOUD V20.4.2)  */
    r = gobj_command(yuno, "delete-key",
        json_pack("{s:s, s:s, s:b}", "topic_name", TOPIC_NAME, "key", KEY_A, "force", 1), yuno);
    check_bool("delete-key on a replica says READ-ONLY",
        strstr(kw_get_str(0, r, "comment", "", 0), "READ-ONLY") != NULL, TRUE);
    check_refused("delete-key on a replica", r);
    r = gobj_command(yuno, "delete-topic",
        json_pack("{s:s, s:b}", "topic_name", TOPIC_NAME, "force", 1), yuno);
    check_bool("delete-topic on a replica says READ-ONLY",
        strstr(kw_get_str(0, r, "comment", "", 0), "READ-ONLY") != NULL, TRUE);
    check_refused("delete-topic on a replica", r);
    r = gobj_command(yuno, "create-topic",
        json_pack("{s:s}", "topic_name", "topic_on_a_replica"), yuno);
    check_bool("create-topic on a replica says READ-ONLY",
        strstr(kw_get_str(0, r, "comment", "", 0), "READ-ONLY") != NULL, TRUE);
    check_refused("create-topic on a replica", r);
    tranger2_shutdown(other_master);
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
        test_authz_checker, // global_authz_checker: every permission, but g_deny_authz
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
