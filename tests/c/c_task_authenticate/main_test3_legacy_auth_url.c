/****************************************************************************
 *          MAIN_TEST3_LEGACY_AUTH_URL.C
 *
 *          Self-contained c_task_authenticate test: legacy auth_url
 *          configuration.
 *
 *          c_task_authenticate's mt_start emits a deprecation warning
 *          when auth_url is the only IdP source configured, then
 *          synthesises the Keycloak path scheme.  This test asserts:
 *
 *            (a) the deprecation warning is emitted exactly once
 *                (consumed via the FIFO matcher), and
 *            (b) the login flow still works end to end against the same
 *                mock IdP — EV_ON_TOKEN result=0.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test3_legacy_auth_url"
#define APP_DOC     "task_authenticate legacy auth_url deprecation warning path"

PRIVATE char task_kw_snippet[] =
    "{                                                                  "
    "  'auth_url':   'http://127.0.0.1:18901/realms/test',              "
    "  'azp':        'test-client',                                     "
    "  'user_id':    'mockuser',                                        "
    "  'user_passw': 'mockpass'                                         "
    "}                                                                  ";

int main(int argc, char *argv[])
{
    /* kw_match_simple is a subset match — only the `msg` field is
     * required to match.  Other fields ("function", "auth_url", …) do
     * not need to be enumerated here. */
    json_t *expected_logs = json_pack("[{s:s}]",
        "msg", "task_authenticate: 'auth_url' is deprecated; use 'issuer' or explicit endpoints"
    );

    return run_task_authenticate_test(
        argc, argv,
        APP_NAME, APP_DOC,
        task_kw_snippet,
        "{}",           // default mock IdP behaviour
        0,              // expected_result: success
        expected_logs,
        TRUE            // capture warnings: the deprecation must be visible
    );
}
