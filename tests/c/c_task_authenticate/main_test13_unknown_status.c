/****************************************************************************
 *          MAIN_TEST13_UNKNOWN_STATUS.C
 *
 *          Self-contained c_task_authenticate test: the mock IdP answers /token with a status
 *          llhttp has no name for: result=-1. llhttp_status_name() aborts on
 *          such a status, and a peer chooses it.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test13_unknown_status"
#define APP_DOC     "task_authenticate: a status with no name"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'token_endpoint':       'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/token', "
    "  'end_session_endpoint': 'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/logout',"
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{'raw_token_answer': 'HTTP/1.1 298 Odd\\r\\nContent-Length: 0\\r\\n\\r\\n'}";

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
        NULL,
        TRUE            // capture warnings too
    );
}
