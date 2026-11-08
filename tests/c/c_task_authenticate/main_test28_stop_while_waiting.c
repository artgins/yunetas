/****************************************************************************
 *          MAIN_TEST28_STOP_WHILE_WAITING.C
 *
 *          Self-contained c_task_authenticate test: nobody listens at the
 *          port of the IdP, and the owner stops the task 1 s after its start,
 *          while it waits for the connection. Nothing more comes: no
 *          EV_ON_TOKEN and no WARNING when the wait (10 s) would have ended.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test28_stop_while_waiting"
#define APP_DOC     "task_authenticate: the owner stops the task while it waits for the IdP"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'token_endpoint':       'http://127.0.0.1:1/realms/test/protocol/openid-connect/token',     "
    "  'end_session_endpoint': 'http://127.0.0.1:1/realms/test/protocol/openid-connect/logout',    "
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{}";

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    set_task_stop_after(1000, 11000);
    return run_task_authenticate_test(
        argc, argv,
        APP_NAME, APP_DOC,
        task_kw_snippet,
        mock_idp_kw_snippet,
        0,             // expected_result
        NULL,
        TRUE            // capture warnings too
    );
}
