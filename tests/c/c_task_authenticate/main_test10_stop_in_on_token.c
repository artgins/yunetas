/****************************************************************************
 *          MAIN_TEST10_STOP_IN_ON_TOKEN.C
 *
 *          Self-contained c_task_authenticate test: as test9, but the
 *          driver stops the task INSIDE its EV_ON_TOKEN, as ycli does: the
 *          run ends there (its C_TASK runs no logout with no client) and
 *          leaves no HTTP client.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test10_stop_in_on_token"
#define APP_DOC     "task_authenticate run 3 times, stopped inside EV_ON_TOKEN: no HTTP client left"

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
    set_task_runs(3, TRUE);
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
