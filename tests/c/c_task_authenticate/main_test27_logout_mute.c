/****************************************************************************
 *          MAIN_TEST27_LOGOUT_MUTE.C
 *
 *          Self-contained c_task_authenticate test: the IdP takes the
 *          end-session and never answers it. The token was published (result
 *          0); when the C_TASK times out (10 s), ONE WARNING. Up to 7.30.14
 *          the run ended with no log at all.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test27_logout_mute"
#define APP_DOC     "task_authenticate: an IdP silent at the end-session"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'token_endpoint':       'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/token', "
    "  'end_session_endpoint': 'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/logout',"
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{'mute_logout_requests': true, 'expected_logouts': 1}";

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    set_task_end_delay(12000);
    return run_task_authenticate_test(
        argc, argv,
        APP_NAME, APP_DOC,
        task_kw_snippet,
        mock_idp_kw_snippet,
        0,             // expected_result
        "task_authenticate: the IdP did not answer the end-session",
        TRUE            // capture warnings too
    );
}
