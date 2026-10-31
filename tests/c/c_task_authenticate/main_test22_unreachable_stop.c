/****************************************************************************
 *          MAIN_TEST22_UNREACHABLE_STOP.C
 *
 *          Self-contained c_task_authenticate test: nobody listens at the
 *          port of the IdP, and the owner stops the task inside the
 *          EV_ON_TOKEN of the end of the wait, as ycli does: it runs in the
 *          publication of the task's own timer.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test22_unreachable_stop"
#define APP_DOC     "task_authenticate: the owner stops the task when the IdP cannot be reached"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'token_endpoint':       'http://127.0.0.1:1/realms/test/protocol/openid-connect/token',     "
    "  'end_session_endpoint': 'http://127.0.0.1:1/realms/test/protocol/openid-connect/logout',    "
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    set_task_runs(1, TRUE);

    return run_task_authenticate_test(
        argc, argv,
        APP_NAME, APP_DOC,
        task_kw_snippet,
        "{}",
        -1,             // expected_result
        "task_authenticate: the IdP cannot be reached",
        TRUE            // capture warnings too
    );
}
