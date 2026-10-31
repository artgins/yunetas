/****************************************************************************
 *          MAIN_TEST9_RUNS.C
 *
 *          Self-contained c_task_authenticate test: the task is run
 *          three times in one yuno, stopped and started again after each
 *          EV_ON_TOKEN (outside it). Each run creates its own HTTP client
 *          and retires it at its end: after the third, the driver finds no
 *          C_PROT_HTTP_CL and no C_TCP under the task (each run left its
 *          client stopped, a child for good: three of each).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test9_runs"
#define APP_DOC     "task_authenticate run 3 times in one yuno: no HTTP client left"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'token_endpoint':       'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/token', "
    "  'end_session_endpoint': 'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/logout',"
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

int main(int argc, char *argv[])
{
    set_task_runs(3, FALSE);
    return run_task_authenticate_test(
        argc, argv,
        APP_NAME, APP_DOC,
        task_kw_snippet,
        "{}",           // default mock IdP behaviour
        0,              // expected_result: success
        NULL,           // no expected log
        TRUE            // capture warnings too
    );
}
