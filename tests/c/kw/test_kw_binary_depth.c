/****************************************************************************
 *          TEST_KW_BINARY_DEPTH.C
 *
 *          A binary field (the `gbuffer` that gobj_start_up() registers
 *          with kw_add_binary_type()) counts only at the TOP level of a kw.
 *          kw_incref(), kw_decref() and kw_twin() look for it there and
 *          nowhere else; kw_duplicate(), kw_filter_metadata() and
 *          kw_filter_private() must do the same.
 *
 *          Up to 7.30.16 those three increfed a key named `gbuffer` at ANY
 *          depth. A json written by a peer can carry one inside a dict
 *          (`{"x": {"gbuffer": 4096}}`: kw_deserialize() cleans only the
 *          top level of a frame), and the integer was taken for a pointer:
 *          gbuffer_incref() on an address the peer chose, a crash (SIGSEGV)
 *          of the yuno. A real gbuffer nested that way was increfed and
 *          never released (kw_decref() looks only at the top level): a
 *          leak.
 *
 *          And the frontier: gbuf2json_from_peer() and
 *          anystring2json_from_peer() drop every "gbuffer" key of a json a
 *          peer wrote, at any depth, with one warning, however the name is
 *          spelled with \u escapes (each of the 127 ways).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>

/***************************************************************************
 *              Helpers
 ***************************************************************************/
static int failed = 0;

static void check(const char *title, BOOL ok, const char *detail)
{
    if(ok) {
        printf("  ok  %s\n", title);
    } else {
        printf("FAIL %-56s %s\n", title, detail?detail:"");
        failed++;
    }
}

/*
 *  The errors and warnings logged since the last call: the gbuffer helpers
 *  say "BAD gbuf_incref()" / "BAD gbuf_decref()" as errors
 */
static int logged = 0;

static int count_log_write(void *h, int priority, const char *bf, size_t len)
{
    if(priority <= LOG_WARNING) {
        logged++;
        printf("     logged: %.*s\n", (int)len, bf);
    }
    return 0;
}

static BOOL error_logged(void)
{
    BOOL some = logged > 0;
    logged = 0;
    return some;
}

static gbuffer_t *new_gbuf(void)
{
    gbuffer_t *gbuf = gbuffer_create(16, 16);
    gbuffer_append_string(gbuf, "peer bytes");
    return gbuf;
}

/***************************************************************************
 *  A real gbuffer nested in a dict is not the kw's binary field: a copy
 *  takes no reference of it (kw_decref() would never release one)
 ***************************************************************************/
static void test_nested_real_gbuffer(void)
{
    printf("\n--- a nested gbuffer is data, not the kw's binary field ---\n");
    char detail[128];

    gbuffer_t *gbuf = new_gbuf();
    json_t *kw = json_pack("{s:{s:I}, s:[{s:I}]}",
        "x", "gbuffer", (json_int_t)(uintptr_t)gbuf,
        "l", "gbuffer", (json_int_t)(uintptr_t)gbuf
    );

    json_t *dup = kw_duplicate(0, kw);
    snprintf(detail, sizeof(detail), "refcount %d, wanted 1", (int)gbuf->refcount);
    check("kw_duplicate: no reference taken", gbuf->refcount == 1, detail);
    KW_DECREF(dup)

    json_t *meta = kw_filter_metadata(0, json_incref(kw));
    snprintf(detail, sizeof(detail), "refcount %d, wanted 1", (int)gbuf->refcount);
    check("kw_filter_metadata: no reference taken", gbuf->refcount == 1, detail);
    KW_DECREF(meta)

    json_t *priv = kw_filter_private(0, json_incref(kw));
    snprintf(detail, sizeof(detail), "refcount %d, wanted 1", (int)gbuf->refcount);
    check("kw_filter_private: no reference taken", gbuf->refcount == 1, detail);
    KW_DECREF(priv)

    /*  A list's dicts are not the top level either: kw_incref() of a list counts none  */
    json_t *list = json_pack("[{s:I}]", "gbuffer", (json_int_t)(uintptr_t)gbuf);
    json_t *dup_list = kw_duplicate(0, list);
    snprintf(detail, sizeof(detail), "refcount %d, wanted 1", (int)gbuf->refcount);
    check("kw_duplicate of a list: no reference taken", gbuf->refcount == 1, detail);
    JSON_DECREF(dup_list)
    JSON_DECREF(list)

    JSON_DECREF(kw)
    check("no error logged", !error_logged(), NULL);
    GBUFFER_DECREF(gbuf)
}

/***************************************************************************
 *  The top level still counts: the copy holds a reference of its own
 ***************************************************************************/
static void test_top_level_real_gbuffer(void)
{
    printf("\n--- the top-level gbuffer is still the kw's binary field ---\n");
    char detail[128];

    gbuffer_t *gbuf = new_gbuf();
    json_t *kw = json_pack("{s:I, s:s, s:i}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf,
        "a", "b",
        "__md", 1
    );

    json_t *dup = kw_duplicate(0, kw);
    snprintf(detail, sizeof(detail), "refcount %d, wanted 2", (int)gbuf->refcount);
    check("kw_duplicate: the copy holds a reference", gbuf->refcount == 2, detail);
    KW_DECREF(dup)

    /*  kw owned by the filter: its reference goes, the copy's comes  */
    json_t *meta = kw_filter_metadata(0, kw);
    snprintf(detail, sizeof(detail), "refcount %d, wanted 1", (int)gbuf->refcount);
    check("kw_filter_metadata: the copy holds the reference", gbuf->refcount == 1 &&
        json_object_get(meta, "gbuffer") && !json_object_get(meta, "__md"), detail);
    gbuffer_incref(gbuf);   // to look at it after the copy goes
    KW_DECREF(meta)
    snprintf(detail, sizeof(detail), "refcount %d, wanted 1", (int)gbuf->refcount);
    check("KW_DECREF of the copy releases its reference", gbuf->refcount == 1, detail);

    /*  A top-level gbuffer of 0 (a peer's that could not be read) is nothing  */
    json_t *kw0 = json_pack("{s:I}", "gbuffer", (json_int_t)0);
    json_t *dup0 = kw_duplicate(0, kw0);
    check("kw_duplicate of a gbuffer of 0: no error", !error_logged(), NULL);
    KW_DECREF(dup0)
    KW_DECREF(kw0)

    check("no error logged", !error_logged(), NULL);
    GBUFFER_DECREF(gbuf)
}

/***************************************************************************
 *  What a peer writes: an integer it chose, nested. Up to 7.30.16 a crash.
 ***************************************************************************/
static void test_nested_peer_integer(void)
{
    printf("\n--- a peer's nested \"gbuffer\" integer is not dereferenced ---\n");

    json_t *kw = json_pack("{s:{s:I}, s:{s:{s:I}}, s:[{s:I}], s:{s:I}}",
        "x", "gbuffer", (json_int_t)4096,
        "_geometry", "y", "gbuffer", (json_int_t)4096,
        "l", "gbuffer", (json_int_t)4096,
        "__md_treedb__", "gbuffer", (json_int_t)4096
    );

    json_t *dup = kw_duplicate(0, kw);
    check("kw_duplicate", dup && json_equal(dup, kw), NULL);
    KW_DECREF(dup)

    json_t *meta = kw_filter_metadata(0, json_incref(kw));
    check("kw_filter_metadata", meta && !json_object_get(meta, "__md_treedb__") &&
        json_integer_value(json_object_get(json_object_get(meta, "x"), "gbuffer")) == 4096, NULL);
    KW_DECREF(meta)

    json_t *priv = kw_filter_private(0, json_incref(kw));
    check("kw_filter_private", priv && !json_object_get(priv, "_geometry"), NULL);
    KW_DECREF(priv)

    check("no error logged", !error_logged(), NULL);
    KW_DECREF(kw)
}

/***************************************************************************
 *  The frontier: a json of a peer leaves the parser with no "gbuffer" key,
 *  at any depth, said with ONE warning. A dict of it may become the top
 *  level of a kw (C_NODE makes a kw of a peer's "record"): up to 7.30.16
 *  a create-node of a peer with {"record": {"gbuffer": 4096}} crashed.
 ***************************************************************************/
static void test_peer_json_frontier(void)
{
    printf("\n--- a json of a peer has no binary field name, at any depth ---\n");

    const char *text =
        "{\"gbuffer\": 4096, \"a\": {\"gbuffer\": 1, \"l\": [{\"gbuffer\": 2}, 3]}, "
        "\"b\": \"gbuffer\", \"__gbuffer___\": {\"data\": \"\"}}";
    json_t *want = json_pack("{s:{s:[{},i]}, s:s, s:{s:s}}",
        "a", "l", 3,
        "b", "gbuffer",
        "__gbuffer___", "data", ""
    );

    error_logged();
    json_t *jn = anystring2json_from_peer(0, text, strlen(text), 0);
    int warnings = logged;
    check("anystring2json_from_peer drops them all", json_equal(jn, want), NULL);
    check("anystring2json_from_peer: one warning", warnings == 1 && error_logged(), NULL);
    JSON_DECREF(jn)

    gbuffer_t *gbuf = gbuffer_create(strlen(text)+1, strlen(text)+1);
    gbuffer_append_string(gbuf, text);
    jn = gbuf2json_from_peer(0, gbuf, 0);   // gbuf owned
    warnings = logged;
    check("gbuf2json_from_peer drops them all", json_equal(jn, want), NULL);
    check("gbuf2json_from_peer: one warning", warnings == 1 && error_logged(), NULL);
    JSON_DECREF(jn)
    JSON_DECREF(want)

    /*  The name spelled with an escape is the same key  */
    const char *escaped = "{\"a\": {\"gbuff\\u0065r\": 5}}";
    jn = anystring2json_from_peer(0, escaped, strlen(escaped), 0);
    warnings = logged;
    check("the name spelled with a \\u escape: dropped",
        jn && json_object_size(json_object_get(jn, "a")) == 0 && warnings == 1 && error_logged(), NULL);
    JSON_DECREF(jn)

    /*
     *  Every spelling of the name with escapes: each subset of its letters
     *  written as \u00XX, beside an escape that is no letter of it. The
     *  text is walked only when it holds "gbuffer", "\u006" or "\u007"
     *  (up to 7.30.17, any "\u"): each of these must still be dropped.
     */
    {
        const char *name = "gbuffer";
        int kept = 0;
        for(int mask = 1; mask < (1 << 7); mask++) {
            char key[7*6 + 1];
            size_t n = 0;
            for(int i = 0; i < 7; i++) {
                if(mask & (1 << i)) {
                    n += (size_t)snprintf(key + n, sizeof(key) - n, "\\u%04x", (unsigned)name[i]);
                } else {
                    key[n++] = name[i];
                    key[n] = 0;
                }
            }
            char spelled[256];
            snprintf(spelled, sizeof(spelled),
                "{\"t\": \"caf\\u00e9\", \"a\": {\"%s\": 5}, \"%s\": 6}", key, key);
            jn = anystring2json_from_peer(0, spelled, strlen(spelled), 0);
            warnings = logged;
            if(!jn || json_object_size(json_object_get(jn, "a")) != 0 ||
                    json_object_get(jn, "gbuffer") || warnings != 1 || !error_logged()) {
                printf("     kept: %s\n", spelled);
                kept++;
            }
            JSON_DECREF(jn)
        }
        check("every spelling of the name with \\u escapes: dropped", kept == 0, NULL);
    }

    const char *clean = "{\"a\": {\"b\": [1, {\"c\": \"gbuffer\"}]}}";
    jn = anystring2json_from_peer(0, clean, strlen(clean), 0);
    check("a json without them: untouched, no warning", jn && !error_logged(), NULL);
    JSON_DECREF(jn)

    /*
     *  An inter-event of the wire: the kw a gate hands on has no "gbuffer"
     *  of the peer, and a dict of it can be a kw (kw_twin() of the record)
     */
    const char *frame =
        "{\"event\": \"EV_X\", \"kw\": {\"gbuffer\": 4096, "
        "\"record\": {\"id\": \"a\", \"gbuffer\": 4096}}}";
    gbuf = gbuffer_create(strlen(frame)+1, strlen(frame)+1);
    gbuffer_append_string(gbuf, frame);
    json_t *kw = iev_create_from_gbuffer(0, NULL, gbuf, 1);    // gbuf owned
    warnings = logged;
    check("iev_create_from_gbuffer: no gbuffer in the kw", kw &&
        !json_object_get(kw, "gbuffer") &&
        !json_object_get(json_object_get(kw, "record"), "gbuffer"), NULL);
    check("iev_create_from_gbuffer: one warning", warnings == 1 && error_logged(), NULL);
    json_t *own = kw_twin(0, json_object_get(kw, "record"));
    check("kw_twin of its record", own != NULL, NULL);
    KW_DECREF(own)
    KW_DECREF(kw)

    /*  The same with verbose 0: the frontier does not depend on it  */
    gbuf = gbuffer_create(strlen(frame)+1, strlen(frame)+1);
    gbuffer_append_string(gbuf, frame);
    kw = iev_create_from_gbuffer(0, NULL, gbuf, 0);    // gbuf owned
    warnings = logged;
    check("iev_create_from_gbuffer, verbose 0: no gbuffer in the kw", kw &&
        !json_object_get(kw, "gbuffer") &&
        !json_object_get(json_object_get(kw, "record"), "gbuffer"), NULL);
    check("iev_create_from_gbuffer, verbose 0: one warning", warnings == 1 && error_logged(), NULL);
    KW_DECREF(kw)

    check("no other log", !error_logged(), NULL);
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IOLBF, 0);   // a red run crashes: keep what it said

    sys_malloc_fn_t malloc_func;
    sys_free_fn_t free_func;
    gbmem_get_json_allocators(&malloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    glog_init();
    gobj_log_register_handler("counting", 0, count_log_write, 0);
    gobj_log_add_handler("count", "counting", LOG_OPT_UP_WARNING, 0);

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    test_nested_real_gbuffer();
    test_top_level_real_gbuffer();
    test_nested_peer_integer();
    test_peer_json_frontier();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free\n");
        failed++;
    }

    printf("\n%s\n\n", failed? "FAILED": "all passed");
    return failed? -1: 0;
}
