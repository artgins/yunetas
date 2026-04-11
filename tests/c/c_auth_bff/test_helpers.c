/****************************************************************************
 *          TEST_HELPERS.C
 *
 *          See the companion .h for the public surface and the
 *          TEST_HELPERS_BEGIN_DYING macro contract.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#include <yunetas.h>

#include "test_helpers.h"

/***************************************************************************
 *  Locate the first C_AUTH_BFF instance under __bff_side__.
 ***************************************************************************/
PUBLIC hgobj test_helpers_find_bff(hgobj caller_gobj)
{
    hgobj bff_side = gobj_find_service("__bff_side__", FALSE);
    if(!bff_side) {
        gobj_log_error(caller_gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test_helpers: __bff_side__ service not found",
            NULL
        );
        return NULL;
    }

    json_t *jn_filter = json_pack("{s:s}",
        "__gclass_name__", "C_AUTH_BFF"
    );
    json_t *dl = gobj_match_children_tree(bff_side, jn_filter);

    hgobj bff = NULL;
    if(dl && json_array_size(dl) > 0) {
        bff = (hgobj)(size_t)json_integer_value(json_array_get(dl, 0));
    } else {
        gobj_log_error(caller_gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test_helpers: no C_AUTH_BFF found under __bff_side__",
            NULL
        );
    }
    gobj_free_iter(dl);
    return bff;
}

/***************************************************************************
 *  Cross-check a subset of a C_AUTH_BFF's stat counters.
 ***************************************************************************/
PUBLIC void test_helpers_check_stats(
    hgobj caller_gobj,
    hgobj bff,
    const char *test_name,
    const test_stat_expect_t *expected
)
{
    if(!bff) {
        /* Error already logged by the caller via test_helpers_find_bff */
        return;
    }
    json_t *resp = gobj_stats(bff, NULL, 0, caller_gobj);
    if(!resp) {
        gobj_log_error(caller_gobj, 0,
            "function",  "%s", __FUNCTION__,
            "msgset",    "%s", MSGSET_APP_ERROR,
            "msg",       "%s", "test_helpers: gobj_stats(bff) returned NULL",
            "test_name", "%s", test_name ? test_name : "",
            NULL
        );
        return;
    }
    json_t *jn_data = kw_get_dict(caller_gobj, resp, "data", NULL, 0);
    if(!jn_data) {
        gobj_log_error(caller_gobj, 0,
            "function",  "%s", __FUNCTION__,
            "msgset",    "%s", MSGSET_APP_ERROR,
            "msg",       "%s", "test_helpers: stats response has no 'data' dict",
            "test_name", "%s", test_name ? test_name : "",
            NULL
        );
        JSON_DECREF(resp)
        return;
    }

    for(int i = 0; expected[i].name; i++) {
        json_int_t got = json_integer_value(
            json_object_get(jn_data, expected[i].name));
        if(got != expected[i].expected) {
            gobj_log_error(caller_gobj, 0,
                "function",  "%s", __FUNCTION__,
                "msgset",    "%s", MSGSET_APP_ERROR,
                "msg",       "%s", "test_helpers: BFF stat counter mismatch",
                "test_name", "%s", test_name ? test_name : "",
                "stat",      "%s", expected[i].name,
                "expected",  "%lld", (long long)expected[i].expected,
                "got",       "%lld", (long long)got,
                NULL
            );
        }
    }

    JSON_DECREF(resp)
}
