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

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    test_overwrite();
    test_create();
    test_refusals();
    test_ownership();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free\n");
        failed++;
    }

    printf("\n%s\n\n", failed? "FAILED": "all passed");
    return failed? -1: 0;
}
