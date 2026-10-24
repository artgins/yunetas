/****************************************************************************
 *          TEST_KW_SET_DICT_VALUE.C
 *
 *          kw_set_dict_value() writes a value at a path, and the value
 *          REPLACES what the path holds -- "like json_object_set", as its
 *          header says and as its gobj-js twin does.
 *
 *          Up to 7.25.4 it wrote only a key that did not exist: on a key
 *          that was there it dropped the value and answered 0. Every caller
 *          meant to overwrite, and two of them paid for it: the stale ref a
 *          treedb clears from a STRING fkey column stayed, so the node could
 *          never be cleaned or force-deleted; and the `__username__` that
 *          C_IEVENT_SRV sets in what a peer sends did not replace one the
 *          peer had put there itself.
 *
 *          It also answered 0 when it wrote nothing (a scalar in the
 *          middle of the path, an index out of its list).
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

/*
 *  Set `path` of the json `before` to `value` and compare the answer and
 *  the json with what is expected.
 */
static void check_set(
    const char *title,
    const char *before,
    const char *path,
    json_t *value,          // owned
    int expected_ret,
    const char *expected    // the json after
)
{
    json_t *kw = json_loads(before, 0, 0);
    json_t *want = json_loads(expected, 0, 0);
    if(!kw || !want) {
        printf("FAIL %-48s the test json does not parse\n", title);
        failed++;
        JSON_DECREF(kw)
        JSON_DECREF(want)
        JSON_DECREF(value)
        return;
    }

    int ret = kw_set_dict_value(0, kw, path, value);

    if(ret != expected_ret || !json_equal(kw, want)) {
        char *got = json2uglystr(kw);
        printf("FAIL %-48s ret %d (wanted %d), got %s, wanted %s\n",
            title, ret, expected_ret, got, expected);
        GBMEM_FREE(got)
        failed++;
    } else {
        printf("  ok  %s\n", title);
    }
    JSON_DECREF(kw)
    JSON_DECREF(want)
}

/***************************************************************************
 *              The tests
 ***************************************************************************/
static void test_overwrite(void)
{
    printf("\n--- the value replaces what the path holds ---\n");

    /*
     *  What a treedb does to clear a stale ref from a string fkey column
     */
    check_set("a leaf string is replaced",
        "{\"parent\": \"topic^x^hook\"}", "parent", json_string(""),
        0, "{\"parent\": \"\"}");

    check_set("a leaf at the end of a path is replaced",
        "{\"a\": {\"b\": 1, \"c\": 2}}", "a`b", json_integer(5),
        0, "{\"a\": {\"b\": 5, \"c\": 2}}");

    check_set("a dict is replaced whole",
        "{\"a\": {\"b\": 1}}", "a", json_integer(7),
        0, "{\"a\": 7}");

    /*
     *  What C_IEVENT_SRV does with the `__username__` of what a peer sends
     */
    check_set("a key the kw already has is replaced",
        "{\"cmd\": \"x\", \"__username__\": \"somebody\"}", "__username__", json_string("me"),
        0, "{\"cmd\": \"x\", \"__username__\": \"me\"}");

    check_set("an item of a list is replaced",
        "{\"l\": [{\"k\": 1}, 2]}", "l`1", json_string("z"),
        0, "{\"l\": [{\"k\": 1}, \"z\"]}");

    check_set("a key inside an item of a list is replaced",
        "{\"l\": [{\"k\": 1}]}", "l`0`k", json_integer(2),
        0, "{\"l\": [{\"k\": 2}]}");
}

static void test_create(void)
{
    printf("\n--- what the path needs is created ---\n");

    check_set("a new key",
        "{}", "a", json_true(),
        0, "{\"a\": true}");

    check_set("the dicts of the path",
        "{\"x\": 1}", "a`b`c", json_integer(1),
        0, "{\"x\": 1, \"a\": {\"b\": {\"c\": 1}}}");

    check_set("a dict of the path that exists keeps its keys",
        "{\"a\": {\"x\": 1}}", "a`b", json_integer(2),
        0, "{\"a\": {\"x\": 1, \"b\": 2}}");
}

static void test_refusals(void)
{
    printf("\n--- nothing written: -1, and the kw as it was ---\n");

    check_set("a scalar in the middle of the path",
        "{\"a\": 5}", "a`b", json_integer(1),
        -1, "{\"a\": 5}");

    check_set("a null in the middle of the path",
        "{\"a\": null}", "a`b", json_integer(1),
        -1, "{\"a\": null}");

    check_set("an index out of its list",
        "{\"l\": [1]}", "l`3`k", json_integer(1),
        -1, "{\"l\": [1]}");

    check_set("an empty path",
        "{\"a\": 1}", "", json_integer(1),
        -1, "{\"a\": 1}");

    json_t *list = json_array();
    if(kw_set_dict_value(0, list, "a", json_integer(1)) != -1 || json_array_size(list) != 0) {
        printf("FAIL %-48s\n", "a kw that is not a dict");
        failed++;
    } else {
        printf("  ok  %s\n", "a kw that is not a dict");
    }
    JSON_DECREF(list)
}

static void test_ownership(void)
{
    printf("\n--- the value is owned ---\n");

    json_t *kw = json_pack("{s:s}", "a", "old");
    json_t *value = json_string("new");
    json_incref(value);     // one reference for us, one for the call
    kw_set_dict_value(0, kw, "a", value);
    if(value->refcount != 2 || json_object_get(kw, "a") != value) {
        printf("FAIL %-48s refcount %d\n", "the kw holds the value, the call's ref is spent",
            (int)value->refcount);
        failed++;
    } else {
        printf("  ok  %s\n", "the kw holds the value, the call's ref is spent");
    }
    JSON_DECREF(kw)
    if(value->refcount != 1) {
        printf("FAIL %-48s refcount %d\n", "the replaced kw lets it go", (int)value->refcount);
        failed++;
    }
    JSON_DECREF(value)
}

/*
 *  A reader with KW_CREATE whose write fails: kw_set_dict_value() spends
 *  the default, so the reader must not hand it back. It did, freed --
 *  with CONFIG_DEBUG_TRACK_MEMORY the default's memory is checked below.
 */
static void check_create_refused(const char *title, json_t *got)
{
    if(got != NULL) {
        printf("FAIL %-48s returned %p, wanted NULL\n", title, (void *)got);
        failed++;
    } else {
        printf("  ok  %s\n", title);
    }
}

static void test_readers_create_refused(void)
{
    printf("\n--- a reader whose KW_CREATE cannot write returns NULL ---\n");

    json_t *kw = json_pack("{s:i}", "a", 5);

    check_create_refused("kw_get_dict, a scalar on the path",
        kw_get_dict(0, kw, "a`b", json_object(), KW_CREATE));
    check_create_refused("kw_get_dict, an empty path",
        kw_get_dict(0, kw, "", json_object(), KW_CREATE));
    check_create_refused("kw_get_list, a scalar on the path",
        kw_get_list(0, kw, "a`b", json_array(), KW_CREATE));
    check_create_refused("kw_get_dict_value, a scalar on the path",
        kw_get_dict_value(0, kw, "a`b", json_string("x"), KW_CREATE));
    check_create_refused("kw_get_subdict_value, a scalar on the path",
        kw_get_subdict_value(0, kw, "a`b", "k", json_string("x"), KW_CREATE));

    /*  The kw is as it was.  */
    json_t *want = json_pack("{s:i}", "a", 5);
    if(!json_equal(kw, want)) {
        printf("FAIL %-48s\n", "the kw is left as it was");
        failed++;
    } else {
        printf("  ok  %s\n", "the kw is left as it was");
    }
    JSON_DECREF(want)

    /*  And a write that can be done still answers the default, held by the kw.  */
    json_t *d = kw_get_dict(0, kw, "c`d", json_object(), KW_CREATE);
    if(!d || kw_find_path(0, kw, "c`d", FALSE) != d) {
        printf("FAIL %-48s\n", "a create that can write returns what it wrote");
        failed++;
    } else {
        printf("  ok  %s\n", "a create that can write returns what it wrote");
    }
    JSON_DECREF(kw)
}

/*
 *  The other half (REV-CLOUD V10.1): with KW_CREATE, a path that holds a
 *  value of another type, or no kw at all, handed the default back -- an
 *  orphan the caller wrote into and nobody freed (the leak check at the
 *  end catches it). And an empty segment, which the writer dropped, so
 *  "a`" wrote over `a`.
 */
static void test_create_wrong_type(void)
{
    printf("\n--- KW_CREATE over another type, or no kw: NULL, the default spent ---\n");

    json_t *kw = json_pack("{s:s, s:{}}", "s", "text", "d");

    check_create_refused("kw_get_dict, the path holds a string",
        kw_get_dict(0, kw, "s", json_object(), KW_CREATE));
    check_create_refused("kw_get_list, the path holds a dict",
        kw_get_list(0, kw, "d", json_array(), KW_CREATE));
    check_create_refused("kw_get_dict, no kw",
        kw_get_dict(0, NULL, "x", json_object(), KW_CREATE));
    check_create_refused("kw_get_list, no kw",
        kw_get_list(0, NULL, "x", json_array(), KW_CREATE));
    check_create_refused("kw_get_dict_value, no kw",
        kw_get_dict_value(0, NULL, "x", json_string("v"), KW_CREATE));

    json_t *want = json_pack("{s:s, s:{}}", "s", "text", "d");
    if(!json_equal(kw, want)) {
        printf("FAIL %-48s\n", "the kw is left as it was");
        failed++;
    } else {
        printf("  ok  %s\n", "the kw is left as it was");
    }
    JSON_DECREF(want)
    JSON_DECREF(kw)
}

static void test_empty_segment(void)
{
    printf("\n--- an empty segment is refused ---\n");

    check_set("'a`' does not write over a",
        "{\"a\": 5}", "a`", json_integer(1), -1, "{\"a\": 5}");
    check_set("'`a' is refused",
        "{\"a\": 5}", "`a", json_integer(1), -1, "{\"a\": 5}");
    check_set("'b``c' is refused",
        "{\"a\": 5}", "b``c", json_integer(1), -1, "{\"a\": 5}");

    json_t *kw = json_pack("{s:i}", "a", 5);
    check_create_refused("kw_get_dict 'a`' does not write over a",
        kw_get_dict(0, kw, "a`", json_object(), KW_CREATE));
    if(json_integer_value(json_object_get(kw, "a")) != 5) {
        printf("FAIL %-48s\n", "a is still 5");
        failed++;
    } else {
        printf("  ok  %s\n", "a is still 5");
    }
    JSON_DECREF(kw)
}

/*
 *  A key that is not valid UTF-8 in the middle of the path: the
 *  intermediate dict was freed by json_object_set_new() and then walked.
 */
static void test_invalid_utf8_segment(void)
{
    printf("\n--- an intermediate key that is not UTF-8 ---\n");

    json_t *kw = json_object();
    int ret = kw_set_dict_value(0, kw, "\xff\xfe`b", json_integer(1));
    if(ret != -1 || json_object_size(kw) != 0) {
        printf("FAIL %-48s ret %d, size %d\n", "refused, nothing written",
            ret, (int)json_object_size(kw));
        failed++;
    } else {
        printf("  ok  %s\n", "refused, nothing written");
    }
    JSON_DECREF(kw)
}

/*
 *  REV-CLOUD V11.1. A null at the leaf is "no value": a create replaces it,
 *  as a missing key (it answered NULL since the V10.1 contract). And the
 *  caller that did not check that NULL: msg_iev_push_stack() went on with
 *  json_object_set_new(NULL), which frees the stack, and then used it.
 */
static void test_null_leaf_is_absent(void)
{
    printf("\n--- a null at the leaf is replaced by a create ---\n");

    json_t *kw = json_pack("{s:n, s:n}", "d", "l");
    json_t *d = kw_get_dict(0, kw, "d", json_object(), KW_CREATE);
    json_t *l = kw_get_list(0, kw, "l", json_array(), KW_CREATE);
    if(!d || !json_is_object(json_object_get(kw, "d")) || d != json_object_get(kw, "d")) {
        printf("FAIL %-48s\n", "kw_get_dict replaces a null");
        failed++;
    } else {
        printf("  ok  %s\n", "kw_get_dict replaces a null");
    }
    if(!l || !json_is_array(json_object_get(kw, "l")) || l != json_object_get(kw, "l")) {
        printf("FAIL %-48s\n", "kw_get_list replaces a null");
        failed++;
    } else {
        printf("  ok  %s\n", "kw_get_list replaces a null");
    }
    JSON_DECREF(kw)

    /*
     *  A peer's __md_iev__ (or a stack in it) of the wrong type: replaced
     *  with a warning, nothing used freed, and the entry lands (REV-CLOUD
     *  V13.4: refusing it warned again on every push of the same message).
     */
    json_t *kw2 = json_pack("{s:i}", "__md_iev__", 5);
    int ret = msg_iev_push_stack(0, kw2, "ievent_gate_stack", json_pack("{s:s}", "x", "y"));
    int ret2 = msg_iev_push_stack(0, kw2, "ievent_gate_stack", json_pack("{s:s}", "x", "z"));
    json_t *stack2 = kw_find_path(0, kw2, "__md_iev__`ievent_gate_stack", FALSE);
    if(ret != 0 || ret2 != 0 || json_array_size(stack2) != 2) {
        printf("FAIL %-48s ret %d %d\n", "msg_iev_push_stack over a scalar __md_iev__", ret, ret2);
        failed++;
    } else {
        printf("  ok  %s\n", "msg_iev_push_stack over a scalar __md_iev__");
    }
    JSON_DECREF(kw2)

    json_t *kw4 = json_pack("{s:{s:s}}", "__md_iev__", "ievent_gate_stack", "bad");
    ret = msg_iev_push_stack(0, kw4, "ievent_gate_stack", json_pack("{s:s}", "x", "y"));
    json_t *stack4 = kw_find_path(0, kw4, "__md_iev__`ievent_gate_stack", FALSE);
    if(ret != 0 || json_array_size(stack4) != 1) {
        printf("FAIL %-48s ret %d\n", "msg_iev_push_stack over a scalar stack", ret);
        failed++;
    } else {
        printf("  ok  %s\n", "msg_iev_push_stack over a scalar stack");
    }
    JSON_DECREF(kw4)

    /*  And a null one is replaced: the push lands.  */
    json_t *kw3 = json_pack("{s:n}", "__md_iev__");
    ret = msg_iev_push_stack(0, kw3, "ievent_gate_stack", json_pack("{s:s}", "x", "y"));
    json_t *stack = kw_find_path(0, kw3, "__md_iev__`ievent_gate_stack", FALSE);
    if(ret != 0 || json_array_size(stack) != 1) {
        printf("FAIL %-48s ret %d\n", "msg_iev_push_stack over a null __md_iev__", ret);
        failed++;
    } else {
        printf("  ok  %s\n", "msg_iev_push_stack over a null __md_iev__");
    }
    JSON_DECREF(kw3)
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

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    test_overwrite();
    test_create();
    test_refusals();
    test_ownership();
    test_readers_create_refused();
    test_create_wrong_type();
    test_empty_segment();
    test_invalid_utf8_segment();
    test_null_leaf_is_absent();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free\n");
        failed++;
    }

    printf("\n%s\n\n", failed? "FAILED": "all passed");
    return failed? -1: 0;
}
