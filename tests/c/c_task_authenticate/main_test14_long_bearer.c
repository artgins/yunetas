/****************************************************************************
 *          MAIN_TEST14_LONG_BEARER.C
 *
 *          Self-contained c_task_authenticate test: an access_token of 2000 bytes: the Authorization of
 *          the /logout carries it whole (it was cut at 1024 bytes).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test14_long_bearer"
#define APP_DOC     "task_authenticate: the Authorization of the logout, whole"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'token_endpoint':       'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/token', "
    "  'end_session_endpoint': 'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/logout',"
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{'access_token_len': 2000, 'expected_bearer_len': 2007, 'expected_logouts': 1}";

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    set_task_end_delay(1000);
    return run_task_authenticate_test(
        argc, argv,
        APP_NAME, APP_DOC,
        task_kw_snippet,
        mock_idp_kw_snippet,
        0,             // expected_result
        NULL,
        TRUE            // capture warnings too
    );
}
