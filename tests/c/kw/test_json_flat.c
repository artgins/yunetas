/****************************************************************************
 *          TEST_JSON_FLAT.C
 *
 *          The flat form of a json: one row per leaf, the id being the
 *          path of the item.
 *
 *          MOST OF THIS FILE IS THE LIST OF THINGS THE FIRST
 *          IMPLEMENTATION GOT WRONG, measured before rewriting it:
 *
 *              {"1630": {...}}   came back as an ARRAY OF 1631 ELEMENTS
 *              {"a": {}}         the key VANISHED
 *              {"a": []}         vanished
 *              {"a`b": 1}        split into {"a": {"b": 1}}
 *              {"a": {"": 1}}    came back as {"a": 1}, a level short
 *              "a`1000"          materialised 1001 elements for one datum
 *
 *          A round trip that holds for the easy cases and loses data on
 *          the others is worse than no round trip: it is trusted.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <locale.h>
#include <signal.h>
#include <yunetas.h>

/***************************************************************************
 *              Helpers
 ***************************************************************************/
static int failed = 0;

static void fail(const char *title, const char *detail)
{
    printf("FAIL %-44s %s\n", title, detail?detail:"");
    failed++;
}

/*
 *  A json goes to flat and comes back the same. It is THE property: the
 *  flat form is a way of writing a json, not a lossy summary of it.
 */
static void round_trip(const char *title, const char *json_txt, const char *expected_flat)
{
    json_error_t error;
    json_t *orig = json_loads(json_txt, JSON_DECODE_ANY, &error);
    if(!orig) {
        fail(title, "the source does not parse");
        return;
    }

    json_t *flat = json2flat(orig);
    if(!flat) {
        fail(title, "json2flat answered NULL");
        JSON_DECREF(orig)
        return;
    }

    if(expected_flat) {
        json_t *exp = json_loads(expected_flat, 0, &error);
        if(!json_equal(flat, exp)) {
            char *got = json_dumps(flat, JSON_COMPACT|JSON_SORT_KEYS);
            fail(title, got);
            printf("     wanted %s\n", expected_flat);
            free(got);
        }
        JSON_DECREF(exp)
    }

    char err[256];
    json_t *back = flat2json(flat, err, sizeof(err));
    if(!back) {
        fail(title, err);
        JSON_DECREF(orig) JSON_DECREF(flat)
        return;
    }

    if(!json_equal(orig, back)) {
        char *s_back = json_dumps(back, JSON_COMPACT|JSON_SORT_KEYS);
        fail(title, "does not come back the same");
        printf("     was    %s\n", json_txt);
        printf("     came   %s\n", s_back);
        free(s_back);
    }

    else {
        char *s_flat = json_dumps(flat, JSON_COMPACT|JSON_SORT_KEYS);
        printf("  ok  %-44s %s\n", title, s_flat);
        free(s_flat);
    }

    JSON_DECREF(orig) JSON_DECREF(flat) JSON_DECREF(back)
}

/*
 *  A flat dict that cannot be rebuilt must SAY SO, not answer half a tree.
 */
static void must_refuse(const char *title, json_t *flat)
{
    char err[256];
    json_t *back = flat2json(flat, err, sizeof(err));
    if(back) {
        char *s = json_dumps(back, JSON_COMPACT|JSON_SORT_KEYS);
        fail(title, "was accepted");
        printf("     answered %s\n", s);
        free(s);
        JSON_DECREF(back)
    } else if(!err[0]) {
        fail(title, "refused without saying why");
    } else {
        printf("  ok  %-44s refused: %s\n", title, err);
    }
    JSON_DECREF(flat)
}

/***************************************************************************
 *              The tests
 ***************************************************************************/
static int test_round_trips(void)
{
    printf("\n--- ida y vuelta ---\n");

    round_trip("nested object", "{\"a\":{\"b\":1},\"c\":\"x\"}",
        "{\"a`b\":1,\"c\":\"x\"}");
    round_trip("array of scalars", "{\"a\":[1,2,3]}",
        "{\"a`[0]\":1,\"a`[1]\":2,\"a`[2]\":3}");
    round_trip("array of objects", "{\"a\":[{\"b\":1},{\"b\":2}]}",
        "{\"a`[0]`b\":1,\"a`[1]`b\":2}");
    round_trip("array root", "[{\"a\":1},{\"a\":2}]",
        "{\"[0]`a\":1,\"[1]`a\":2}");
    round_trip("array of arrays", "{\"m\":[[1,2],[3]]}",
        "{\"m`[0]`[0]\":1,\"m`[0]`[1]\":2,\"m`[1]`[0]\":3}");
    round_trip("every scalar kind",
        "{\"s\":\"x\",\"i\":-3,\"r\":1.5,\"t\":true,\"f\":false,\"n\":null}", NULL);

    /*  The one that broke: a dict keyed by a yuno id. */
    round_trip("numeric key (a yuno id)",
        "{\"1630\":{\"role\":\"db_history_co\"}}",
        "{\"1630`role\":\"db_history_co\"}");
    round_trip("numeric keys, several",
        "{\"1620\":1,\"1630\":2,\"0\":3}", "{\"1620\":1,\"1630\":2,\"0\":3}");

    /*  Empty containers: no leaves of their own, so they ARE leaves. */
    round_trip("empty object", "{\"a\":{},\"b\":1}", "{\"a\":{},\"b\":1}");
    round_trip("empty array", "{\"a\":[],\"b\":1}", "{\"a\":[],\"b\":1}");
    round_trip("empty deep", "{\"a\":{\"b\":{}}}", "{\"a`b\":{}}");
    round_trip("empty inside an array", "{\"a\":[{},[]]}",
        "{\"a`[0]\":{},\"a`[1]\":[]}");

    /*  Escapes. */
    round_trip("key with a backtick", "{\"a`b\":1}", "{\"a``b\":1}");
    round_trip("key of only backticks", "{\"`\":1}", "{\"``\":1}");
    round_trip("key that looks like an index", "{\"[0]\":1}", "{\"[[0]\":1}");
    round_trip("key that starts with a bracket", "{\"[x\":1}", "{\"[[x\":1}");
    round_trip("empty key", "{\"a\":{\"\":1}}", "{\"a`\":1}");
    round_trip("empty key at the root", "{\"\":1}", "{\"\":1}");
    round_trip("key with a dot", "{\"a.b\":{\"c\":1}}", "{\"a.b`c\":1}");

    /*  A real one: a piece of a treedb schema, empties and all. */
    round_trip("a treedb-ish schema",
        "{\"topics\":[{\"id\":\"device_groups\",\"cols\":{\"id\":{\"flag\":[\"persistent\",\"required\"]},"
        "\"properties\":{}}}],\"schema_version\":8}", NULL);

    return 0;
}

static int test_refusals(void)
{
    printf("\n--- lo que tiene que rechazar ---\n");

    /*  The same id as a leaf and as a container: the nested result would
     *  depend on which key jansson hands over first. */
    must_refuse("leaf then container",
        json_pack("{s:i, s:i}", "a", 1, "a`b", 2));
    must_refuse("container then leaf",
        json_pack("{s:i, s:i}", "a`b", 2, "a", 1));

    /*  An index where the tree already holds an object, and the reverse. */
    must_refuse("object and array at once",
        json_pack("{s:i, s:i}", "a`b", 1, "a`[0]", 2));
    must_refuse("array and object at once",
        json_pack("{s:i, s:i}", "a`[0]", 1, "a`b", 2));

    /*  A root cannot be both. */
    must_refuse("both kinds of root",
        json_pack("{s:i, s:i}", "[0]", 1, "a", 2));

    /*  One id must not materialise a million nulls. */
    must_refuse("index over the limit",
        json_pack("{s:i}", "a`[999999]", 1));

    /*  Not an object at all. */
    {
        char err[256];
        json_t *arr = json_array();
        json_t *back = flat2json(arr, err, sizeof(err));
        if(back) {
            fail("a flat json must be an object", "was accepted");
            JSON_DECREF(back)
        } else {
            printf("  ok  %-44s refused: %s\n", "a flat json must be an object", err);
        }
        JSON_DECREF(arr)
    }

    return 0;
}

static int test_holes(void)
{
    printf("\n--- huecos y limites ---\n");

    /*  A hole inside an array is a null, and it stays a null. */
    char err[256];
    json_t *flat = json_pack("{s:i, s:i}", "a`[0]", 1, "a`[2]", 3);
    json_t *back = flat2json(flat, err, sizeof(err));
    json_t *exp = json_pack("{s:[i,n,i]}", "a", 1, 3);
    if(!back) {
        fail("a hole in an array", err);
    } else if(!json_equal(back, exp)) {
        char *s = json_dumps(back, JSON_COMPACT);
        fail("a hole in an array", s);
        free(s);
    } else {
        printf("  ok  %-44s %s\n", "a hole in an array", "[1,null,3]");
    }
    JSON_DECREF(flat) JSON_DECREF(back) JSON_DECREF(exp)

    /*  '[00]' is not a second spelling of '[0]': the form is canonical, so
     *  it is malformed and must not quietly land in slot zero. */
    must_refuse("index with a leading zero",
        json_pack("{s:i, s:i}", "a`[0]", 1, "a`[00]", 2));

    return 0;
}

static int test_key_join_split(void)
{
    printf("\n--- componer y partir un id ---\n");

    /*  A string is a key and an integer is an index: "[0]" as a KEY and
     *  index 0 are different things, and that is why they are typed. */
    json_t *segs = json_pack("[s,i,s,s]", "a", 0, "b`c", "[x");
    char *key = flat_key_join(segs);
    if(!key || strcmp(key, "a`[0]`b``c`[[x") != 0) {
        fail("flat_key_join", key?key:"NULL");
    } else {
        printf("  ok  %-44s %s\n", "flat_key_join", key);
    }

    json_t *back = flat_key_split(key?key:"");
    json_t *exp = json_pack("[s,i,s,s]", "a", 0, "b`c", "[x");
    if(!json_equal(back, exp)) {
        char *s = json_dumps(back, JSON_COMPACT);
        fail("flat_key_split", s);
        free(s);
    } else {
        printf("  ok  %-44s %s\n", "flat_key_split", "and back again");
    }
    /*  And the key that LOOKS like an index survives as a key. */
    json_t *segs2 = json_pack("[s]", "[0]");
    char *key2 = flat_key_join(segs2);
    json_t *back2 = flat_key_split(key2?key2:"");
    if(!key2 || strcmp(key2, "[[0]") != 0 || !json_equal(back2, segs2)) {
        fail("a key that looks like an index", key2?key2:"NULL");
    } else {
        printf("  ok  %-44s %s\n", "a key that looks like an index", key2);
    }
    GBMEM_FREE(key2)
    JSON_DECREF(segs2) JSON_DECREF(back2)

    GBMEM_FREE(key)
    JSON_DECREF(segs) JSON_DECREF(back) JSON_DECREF(exp)

    return 0;
}

static int test_diff_apply(void)
{
    printf("\n--- diff y aplicar ---\n");

    json_t *a = json2flat(json_pack("{s:{s:i, s:s}, s:[i,i]}",
        "cfg", "port", 2020, "host", "old",
        "list", 1, 2
    ));
    json_t *b = json2flat(json_pack("{s:{s:i, s:s}, s:[i,i,i]}",
        "cfg", "port", 2020, "host", "new",
        "list", 1, 2, 3
    ));

    json_t *diff = flat_diff(a, b);
    json_t *changed = json_object_get(diff, "changed");
    json_t *added   = json_object_get(diff, "added");
    json_t *removed = json_object_get(diff, "removed");

    if(json_object_size(changed) != 1 || !json_object_get(changed, "cfg`host")) {
        char *s = json_dumps(changed, JSON_COMPACT|JSON_SORT_KEYS);
        fail("the diff names what changed", s);
        free(s);
    } else {
        printf("  ok  %-44s %s\n", "the diff names what changed", "cfg`host");
    }
    if(json_object_size(added) != 1 || !json_object_get(added, "list`[2]")) {
        char *s = json_dumps(added, JSON_COMPACT|JSON_SORT_KEYS);
        fail("the diff names what was added", s);
        free(s);
    } else {
        printf("  ok  %-44s %s\n", "the diff names what was added", "list`[2]");
    }
    if(json_object_size(removed) != 0) {
        fail("nothing was removed", "but the diff says otherwise");
    }

    /*  And applying it turns one into the other, exactly. */
    char err[256];
    if(flat_apply(a, diff, err, sizeof(err)) < 0) {
        fail("flat_apply", err);
    } else if(!json_equal(a, b)) {
        char *s = json_dumps(a, JSON_COMPACT|JSON_SORT_KEYS);
        fail("flat_apply lands on the target", s);
        free(s);
    } else {
        printf("  ok  %-44s %s\n", "flat_apply lands on the target", "a == b");
    }

    JSON_DECREF(a) JSON_DECREF(b) JSON_DECREF(diff)

    /*  A removal has to remove. */
    json_t *c = json2flat(json_pack("{s:i, s:i}", "x", 1, "y", 2));
    json_t *d = json2flat(json_pack("{s:i}", "x", 1));
    json_t *diff2 = flat_diff(c, d);
    if(flat_apply(c, diff2, err, sizeof(err)) < 0 || !json_equal(c, d)) {
        char *s = json_dumps(c, JSON_COMPACT|JSON_SORT_KEYS);
        fail("flat_apply removes", s);
        free(s);
    } else {
        printf("  ok  %-44s %s\n", "flat_apply removes", "y is gone");
    }
    JSON_DECREF(c) JSON_DECREF(d) JSON_DECREF(diff2)

    return 0;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    test_round_trips();
    test_refusals();
    test_holes();
    test_key_join_split();
    test_diff_apply();

    gobj_end();

    printf("\n%s\n\n", failed? "FAILED": "all passed");
    return failed? -1: 0;
}
