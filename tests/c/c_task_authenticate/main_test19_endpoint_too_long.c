/****************************************************************************
 *          MAIN_TEST19_ENDPOINT_TOO_LONG.C
 *
 *          Self-contained c_task_authenticate test: the discovery answers a token_endpoint of 4096
 *          bytes, one more than it keeps: result=-1 and ONE WARNING. Cut, it
 *          would go on as another URL.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test19_endpoint_too_long"
#define APP_DOC     "task_authenticate: a discovery with an endpoint too long"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'issuer':     'http://127.0.0.1:" IDP_PORT "/realms/test/',                                 "
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{'discovery_endpoint_len': 4096}";

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
