/****************************************************************************
 *          MAIN_TEST30_END_SESSION_NOT_URL.C
 *
 *          Self-contained c_task_authenticate test: the discovery answers an
 *          end_session_endpoint that is not a URL and a token_endpoint that
 *          is: result=-1 and ONE WARNING (each endpoint is checked).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test30_end_session_not_url"
#define APP_DOC     "task_authenticate: a discovery whose end_session_endpoint is not a URL"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'issuer':     'http://127.0.0.1:" IDP_PORT "/realms/test/',                                 "
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{'override_discovery_body': {'token_endpoint': 'http://127.0.0.1/', 'end_session_endpoint': 'no-scheme'}}";

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
