/****************************************************************************
 *          MAIN_TEST1_DISCOVERY.C
 *
 *          Self-contained c_task_authenticate test: issuer-driven OIDC
 *          discovery happy path.
 *
 *          The C_TASK_AUTHENTICATE service is configured with `issuer`,
 *          which prepends action_discover/result_save_discovery to the
 *          chain.  The mock IdP serves a synthesised discovery document,
 *          then a synthesised /token reply, then 204 on /logout.  The
 *          driver expects EV_ON_TOKEN result=0 and no captured warnings.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test1_discovery"
#define APP_DOC     "task_authenticate issuer-driven discovery happy path"

PRIVATE char task_kw_snippet[] =
    "{                                                                  "
    "  'issuer':     'http://127.0.0.1:18901/realms/test/',             "
    "  'azp':        'test-client',                                     "
    "  'user_id':    'mockuser',                                        "
    "  'user_passw': 'mockpass'                                         "
    "}                                                                  ";

int main(int argc, char *argv[])
{
    return run_task_authenticate_test(
        argc, argv,
        APP_NAME, APP_DOC,
        task_kw_snippet,
        "{}",           // default mock IdP behaviour
        0,              // expected_result: success
        NULL,           // no expected logs
        FALSE           // capture errors only
    );
}
