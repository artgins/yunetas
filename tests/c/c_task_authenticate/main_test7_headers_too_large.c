/****************************************************************************
 *          MAIN_TEST7_HEADERS_TOO_LARGE.C
 *
 *          Self-contained c_task_authenticate test: the mock IdP answers
 *          /token 200 with a header of 70 KB, over the 64 KB the HTTP
 *          client takes: it is published with headers_too_large and no
 *          body. EV_ON_TOKEN with result=-1, the WARNING of the HTTP
 *          client and ONE warning of the task naming it.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test7_headers_too_large"
#define APP_DOC     "task_authenticate: an IdP answer with a head over the limit of the HTTP client"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'token_endpoint':       'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/token', "
    "  'end_session_endpoint': 'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/logout',"
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{'raw_token_answer': '@bighead'}";

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
        "[\"HTTP headers larger than their limit, message refused\", \"task_authenticate: IdP answer refused\"]",
        TRUE            // capture warnings too
    );
}
