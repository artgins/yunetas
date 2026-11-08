/****************************************************************************
 *          MAIN_TEST32_TOKEN_NOT_OBJECT.C
 *
 *          Self-contained c_task_authenticate test: /token answers 200 with a
 *          JSON body that is not an object ([1,2]): result=-1 and ONE
 *          WARNING, the one of an answer with no body.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test32_token_not_object"
#define APP_DOC     "task_authenticate: a token answer whose body is not an object"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'issuer':     'http://127.0.0.1:" IDP_PORT "/realms/test/',                                 "
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{'raw_token_answer': 'HTTP/1.1 200 OK\\r\\nContent-Type: application/json\\r\\nContent-Length: 5\\r\\n\\r\\n[1,2]'}";

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
        "Oauth2 response without body",
        TRUE            // capture warnings too
    );
}
