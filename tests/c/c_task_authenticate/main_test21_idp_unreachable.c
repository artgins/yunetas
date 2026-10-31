/****************************************************************************
 *          MAIN_TEST21_IDP_UNREACHABLE.C
 *
 *          Self-contained c_task_authenticate test: nobody listens at the
 *          port of the IdP (the mock is at another one), so the connection
 *          never opens and C_TASK, which starts its first job at the open,
 *          has nothing to time out. The wait for the connection is bounded:
 *          EV_ON_TOKEN with result=-1 and ONE warning per run. Twice: each
 *          run arms the wait again, and leaves no C_TASK and no client
 *          behind. In 7.29.3 the task waited for ever.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test21_idp_unreachable"
#define APP_DOC     "task_authenticate: an IdP that cannot be reached"

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
    set_task_runs(2, FALSE);

    return run_task_authenticate_test(
        argc, argv,
        APP_NAME, APP_DOC,
        task_kw_snippet,
        "{}",
        -1,             // expected_result
        "[\"task_authenticate: the IdP cannot be reached\", \"task_authenticate: the IdP cannot be reached\"]",
        TRUE            // capture warnings too
    );
}
