/****************************************************************************
 *          MAIN_TEST11_LOGOUT_IDP_DROPS.C
 *
 *          Self-contained c_task_authenticate test: the mock IdP drops the connection of the
 *          first /logout. End-session is idempotent, so it is sent again on
 *          the next connection, once, with a WARNING: two /logout arrive.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test11_logout_idp_drops"
#define APP_DOC     "task_authenticate: an IdP that drops the connection of a /logout"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'token_endpoint':       'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/token', "
    "  'end_session_endpoint': 'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/logout',"
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{'drop_logout_requests': 1, 'expected_logouts': 2}";

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    set_task_end_delay(2500);
    return run_task_authenticate_test(
        argc, argv,
        APP_NAME, APP_DOC,
        task_kw_snippet,
        mock_idp_kw_snippet,
        0,             // expected_result
        "[\"task_authenticate: the IdP closed the connection before answering, the request is sent again\"]",
        TRUE            // capture warnings too
    );
}
