/****************************************************************************
 *          MAIN_TEST6_IDP_DROPS.C
 *
 *          Self-contained c_task_authenticate test: the mock IdP drops
 *          the connection of the first /token without answering it.
 *          C_TASK runs its job again when the connection opens again, and
 *          the password grant must not be sent again: EV_ON_TOKEN with
 *          result=-1 and ONE warning (the mock would answer a second
 *          /token 200), and nothing more: the close of the HTTP client
 *          stopped while connected goes to its parent.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test6_idp_drops"
#define APP_DOC     "task_authenticate: an IdP that drops the connection before answering /token"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'token_endpoint':       'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/token', "
    "  'end_session_endpoint': 'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/logout',"
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{'drop_token_requests': 1}";

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    return run_task_authenticate_test(
        argc, argv,
        APP_NAME, APP_DOC,
        task_kw_snippet,
        mock_idp_kw_snippet,
        -1,             // expected_result
        "[\"task_authenticate: the IdP closed the connection before answering, the request is not sent again\"]",
        TRUE            // capture warnings too
    );
}
