/****************************************************************************
 *          MAIN_TEST4_DISCOVERY_FAILURE.C
 *
 *          Self-contained c_task_authenticate test: OIDC discovery
 *          response is missing the required token_endpoint field.
 *
 *          The mock IdP is configured with override_discovery_body that
 *          omits token_endpoint.  c_task_authenticate's
 *          result_save_discovery detects the missing endpoints, logs a
 *          gobj_log_error and publishes EV_ON_TOKEN with result=-1 (so
 *          the caller does not hang waiting for it).
 *
 *          This test asserts:
 *            (a) the EV_ON_TOKEN published has result=-1, and
 *            (b) the matching error log is captured (and consumed) by
 *                the FIFO matcher.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test4_discovery_failure"
#define APP_DOC     "task_authenticate discovery missing endpoints failure path"

PRIVATE char task_kw_snippet[] =
    "{                                                                  "
    "  'issuer':     'http://127.0.0.1:18901/realms/test/',             "
    "  'client_id':  'test-client',                                     "
    "  'user_id':    'mockuser',                                        "
    "  'user_passw': 'mockpass'                                         "
    "}                                                                  ";

/*
 *  Force the mock to return a discovery body with `issuer` only — no
 *  token_endpoint and no end_session_endpoint.  result_save_discovery
 *  must reject this and emit the failure event.
 */
PRIVATE char mock_idp_kw_snippet[] =
    "{                                                                  "
    "  'override_discovery_body': {                                     "
    "    'issuer': 'http://127.0.0.1:18901/realms/test'                 "
    "  }                                                                "
    "}                                                                  ";

int main(int argc, char *argv[])
{
    return run_task_authenticate_test(
        argc, argv,
        APP_NAME, APP_DOC,
        task_kw_snippet,
        mock_idp_kw_snippet,
        -1,             // expected_result: failure
        "task_authenticate: OIDC discovery missing endpoints",
        FALSE           // capture errors only — the log is at ERROR level
    );
}
