/****************************************************************************
 *          TEST_HELPERS.H
 *
 *          Shared utility helpers for the tests/c/c_auth_bff/ suite.
 *
 *          Each test binary gets its own yuno with its own driver GClass
 *          and its own mt_create / mt_start / FSM.  These helpers cover
 *          the three recurring blocks of code that every driver would
 *          otherwise duplicate:
 *
 *            - locating the first C_AUTH_BFF instance under __bff_side__
 *            - cross-checking an arbitrary subset of BFF stat counters
 *            - entering the graceful shutdown sequence (stop the
 *              transient http_cl tree + arm a 100 ms death timer)
 *
 *          The graceful shutdown is provided as a macro because it has
 *          to touch PRIVATE_DATA fields owned by the caller gclass.
 *          Every test driver that uses TEST_HELPERS_BEGIN_DYING must
 *          declare these three fields in its PRIVATE_DATA:
 *              hgobj timer;
 *              hgobj gobj_http_cl;
 *              BOOL  dying;
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************
 *      Types
 ***************************************************************/
typedef struct {
    const char *name;
    json_int_t  expected;
} test_stat_expect_t;

/***************************************************************
 *      Prototypes
 ***************************************************************/

/*
 *  Locate the first child of gclass C_AUTH_BFF under the
 *  __bff_side__ service.  Returns the hgobj or NULL on failure
 *  (error logged from `caller_gobj`).
 */
PUBLIC hgobj test_helpers_find_bff(hgobj caller_gobj);

/*
 *  Generalised version: locate the first child of `gclass_name`
 *  under the service named `service_name`.  Used by test7 to reach
 *  C_MOCK_KEYCLOAK inside __kc_side__ and cross-check the mock's
 *  own stat counters.  Returns NULL on failure (error logged).
 */
PUBLIC hgobj test_helpers_find_service_child(
    hgobj caller_gobj,
    const char *service_name,
    const char *gclass_name
);

/*
 *  Cross-check a subset of a C_AUTH_BFF instance's stat counters.
 *  `expected` is a NULL-terminated array (sentinel with name==NULL).
 *  `test_name` is used in the error messages so a ctest failure
 *  points at the right test binary.
 *
 *  Each mismatch fires a gobj_log_error which the test harness
 *  turns into a test failure via test_json(NULL) in cleaning().
 */
PUBLIC void test_helpers_check_stats(
    hgobj caller_gobj,
    hgobj bff,
    const char *test_name,
    const test_stat_expect_t *expected
);

/***************************************************************
 *      Macros
 ***************************************************************/

/*
 *  Enter the graceful shutdown sequence.  Stops the transient
 *  http_cl + c_tcp stack so io_uring gets a chance to cancel
 *  in-flight reads/writes, then arms the death timer so the
 *  yuno doesn't try to destroy gobjs that are still RUNNING.
 *
 *  See c_test1_login.c::ac_on_message for the full rationale.
 *
 *  Caller's PRIVATE_DATA must contain:
 *      hgobj timer;
 *      hgobj gobj_http_cl;
 *      BOOL  dying;
 */
#define TEST_HELPERS_BEGIN_DYING(priv) do {             \
    if((priv)->gobj_http_cl) {                          \
        gobj_stop_tree((priv)->gobj_http_cl);           \
    }                                                   \
    (priv)->dying = TRUE;                               \
    set_timeout((priv)->timer, 100);                    \
} while(0)

#ifdef __cplusplus
}
#endif
