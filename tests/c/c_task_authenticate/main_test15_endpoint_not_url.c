/****************************************************************************
 *          MAIN_TEST15_ENDPOINT_NOT_URL.C
 *
 *          Self-contained c_task_authenticate test: the discovery answers a token_endpoint that is
 *          not a URL: result=-1 and ONE WARNING. The caller is told: it used
 *          to wait for ever (the job ended with no EV_ON_TOKEN).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test15_endpoint_not_url"
#define APP_DOC     "task_authenticate: a discovery with an endpoint that is not a URL"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'issuer':     'http://127.0.0.1:" IDP_PORT "/realms/test/',                                 "
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{'override_discovery_body': {'token_endpoint': 'no-scheme', 'end_session_endpoint': 'no-scheme'}}";

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
