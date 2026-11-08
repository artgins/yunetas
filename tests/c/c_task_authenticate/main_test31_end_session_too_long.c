/****************************************************************************
 *          MAIN_TEST31_END_SESSION_TOO_LONG.C
 *
 *          Self-contained c_task_authenticate test: the discovery answers an
 *          end_session_endpoint of 4096 bytes, longer than a path: result=-1
 *          and ONE WARNING.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test31_end_session_too_long"
#define APP_DOC     "task_authenticate: a discovery whose end_session_endpoint is too long"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'issuer':     'http://127.0.0.1:" IDP_PORT "/realms/test/',                                 "
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{'discovery_endpoint_len': 4096, 'discovery_long_key': 'end_session_endpoint'}";

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
        "task_authenticate: OIDC discovery with an endpoint that is not a URL",
        TRUE            // capture warnings too
    );
}
