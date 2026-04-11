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
 *  Locate the first child of `gclass_name` under the service named
 *  `service_name`.
 ***************************************************************************/
PUBLIC hgobj test_helpers_find_service_child(
    hgobj caller_gobj,
    const char *service_name,
    const char *gclass_name
)
{
    hgobj service = gobj_find_service(service_name, FALSE);
    if(!service) {
        gobj_log_error(caller_gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP_ERROR,
            "msg",          "%s", "test_helpers: service not found",
            "service",      "%s", service_name,
            NULL
        );
        return NULL;
    }

    json_t *jn_filter = json_pack("{s:s}",
        "__gclass_name__", gclass_name
    );
    json_t *dl = gobj_match_children_tree(service, jn_filter);

    hgobj found = NULL;
    if(dl && json_array_size(dl) > 0) {
        found = (hgobj)(size_t)json_integer_value(json_array_get(dl, 0));
    } else {
        gobj_log_error(caller_gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP_ERROR,
            "msg",          "%s", "test_helpers: no matching child under service",
            "service",      "%s", service_name,
            "gclass",       "%s", gclass_name,
            NULL
        );
    }
    gobj_free_iter(dl);
    return found;
}

/***************************************************************************
 *  Thin wrapper around test_helpers_find_service_child for the
 *  common case of locating the C_AUTH_BFF instance in __bff_side__.
 ***************************************************************************/
PUBLIC hgobj test_helpers_find_bff(hgobj caller_gobj)
{
    return test_helpers_find_service_child(
        caller_gobj,
        "__bff_side__",
        "C_AUTH_BFF"
    );
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
