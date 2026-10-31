/****************************************************************************
 *          MAIN_TEST20_IDP_MUTE.C
 *
 *          Self-contained c_task_authenticate test: the mock IdP takes the
 *          /token request and never answers it. The C_TASK times out, and
 *          its caller must hear it: EV_ON_TOKEN with result=-1 and ONE
 *          warning per run. Twice: what the first run published does not
 *          count for the second. In 7.29.3 nothing came out, and nothing
 *          was logged.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test20_idp_mute"
#define APP_DOC     "task_authenticate: an IdP that never answers /token"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'token_endpoint':       'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/token', "
    "  'end_session_endpoint': 'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/logout',"
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{'mute_token_requests': true}";

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
        mock_idp_kw_snippet,
        -1,             // expected_result
        "[\"task_authenticate: the IdP did not answer\", \"task_authenticate: the IdP did not answer\"]",
        TRUE            // capture warnings too
    );
}
