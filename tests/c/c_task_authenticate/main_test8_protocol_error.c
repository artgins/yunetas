/****************************************************************************
 *          MAIN_TEST8_PROTOCOL_ERROR.C
 *
 *          Self-contained c_task_authenticate test: the mock IdP answers
 *          /token "HTTP/1.1 200 OK" and then a header name with a space:
 *          the HTTP client publishes it with protocol_error and no body.
 *          EV_ON_TOKEN with result=-1, the WARNING of the HTTP client and
 *          ONE warning of the task naming it.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test8_protocol_error"
#define APP_DOC     "task_authenticate: an IdP answer that breaks the protocol after its status line"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'token_endpoint':       'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/token', "
    "  'end_session_endpoint': 'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/logout',"
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{'raw_token_answer': 'HTTP/1.1 200 OK\\r\\nContent-Type: application/json\\r\\nBad Header: x\\r\\nContent-Length: 2\\r\\n\\r\\n{}'}";

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
        "[\"Protocol violation: non-HTTP data received\", \"task_authenticate: IdP answer refused\"]",
        TRUE            // capture warnings too
    );
}
