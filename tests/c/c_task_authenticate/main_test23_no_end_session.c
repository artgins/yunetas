/****************************************************************************
 *          MAIN_TEST23_NO_END_SESSION.C
 *
 *          Self-contained c_task_authenticate test: a `token_endpoint` with no `end_session_endpoint`,
 *          and no `issuer`: a run that cannot start. It does not start (the
 *          ERROR of the configuration), and it reaches its caller:
 *          EV_ON_TOKEN with result=-1. Twice: each run says it. In 7.29.3 it
 *          published nothing, and a caller that waits for that event waited
 *          for ever.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test23_no_end_session"
#define APP_DOC     "task_authenticate: a token_endpoint with no end_session_endpoint"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'token_endpoint': 'http://127.0.0.1:1/realms/test/protocol/openid-connect/token',               "
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    set_task_runs(2, FALSE);

    return run_task_authenticate_test(
        argc, argv,
        APP_NAME, APP_DOC,
        task_kw_snippet,
        "{}",
        -1,             // expected_result
        "[\"task_authenticate: no IdP configured: set 'issuer' or "
        "'token_endpoint'+'end_session_endpoint'\", "
        "\"task_authenticate: no IdP configured: set 'issuer' or "
        "'token_endpoint'+'end_session_endpoint'\"]",
        FALSE           // errors only
    );
}
