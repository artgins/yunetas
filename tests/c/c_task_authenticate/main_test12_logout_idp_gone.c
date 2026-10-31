/****************************************************************************
 *          MAIN_TEST12_LOGOUT_IDP_GONE.C
 *
 *          Self-contained c_task_authenticate test: the mock IdP drops the connection of the
 *          first two /logout: two arrive, not a third, and the token was
 *          already published: result 0, and a WARNING for each.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test12_logout_idp_gone"
#define APP_DOC     "task_authenticate: an IdP that drops the connection of every /logout"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'token_endpoint':       'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/token', "
    "  'end_session_endpoint': 'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/logout',"
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{'drop_logout_requests': 9, 'expected_logouts': 2}";

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    set_task_end_delay(6000);
    return run_task_authenticate_test(
        argc, argv,
        APP_NAME, APP_DOC,
        task_kw_snippet,
        mock_idp_kw_snippet,
        0,             // expected_result
        "[\"task_authenticate: the IdP closed the connection before answering, the request is sent again\", \"task_authenticate: the IdP closed the connection before answering, the request is not sent again\"]",
        TRUE            // capture warnings too
    );
}
