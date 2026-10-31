/****************************************************************************
 *          MAIN_TEST16_MINIMAL_ANSWER.C
 *
 *          Self-contained c_task_authenticate test: an answer of /token with only an access_token
 *          and a refresh_token: result 0, and no log. The rest of the
 *          envelope is the IdP's to send or not: each key missing used to
 *          be an ERROR with a stack.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test16_minimal_answer"
#define APP_DOC     "task_authenticate: a token answer with only its tokens"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'token_endpoint':       'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/token', "
    "  'end_session_endpoint': 'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/logout',"
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{'override_token_body': {'access_token': 'AT', 'refresh_token': 'RT'}}";

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
        0,             // expected_result
        NULL,
        TRUE            // capture warnings too
    );
}
