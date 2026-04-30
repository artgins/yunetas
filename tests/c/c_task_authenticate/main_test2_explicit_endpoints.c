/****************************************************************************
 *          MAIN_TEST2_EXPLICIT_ENDPOINTS.C
 *
 *          Self-contained c_task_authenticate test: explicit
 *          token_endpoint + end_session_endpoint configuration.
 *
 *          With both endpoints set, the discovery step is skipped — the
 *          task chain runs action_get_token directly.  The driver
 *          expects EV_ON_TOKEN result=0 and no captured warnings.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test2_explicit_endpoints"
#define APP_DOC     "task_authenticate explicit endpoints (no discovery) path"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'token_endpoint':       'http://127.0.0.1:18901/realms/test/protocol/openid-connect/token', "
    "  'end_session_endpoint': 'http://127.0.0.1:18901/realms/test/protocol/openid-connect/logout',"
    "  'azp':        'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

int main(int argc, char *argv[])
{
    return run_task_authenticate_test(
        argc, argv,
        APP_NAME, APP_DOC,
        task_kw_snippet,
        "{}",           // default mock IdP behaviour
        0,              // expected_result: success
        NULL,           // no expected log
        FALSE           // capture errors only
    );
}
