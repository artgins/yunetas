/****************************************************************************
 *          MAIN_TEST26_LOGOUT_400.C
 *
 *          Self-contained c_task_authenticate test: the IdP answers the
 *          end-session 400 invalid_grant (a session already gone there): the
 *          token was published, result 0, and ONE WARNING, without a dump of
 *          the answer. Up to 7.30.14 it was an ERROR with the answer dumped.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test26_logout_400"
#define APP_DOC     "task_authenticate: an end-session the IdP refuses"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'token_endpoint':       'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/token', "
    "  'end_session_endpoint': 'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/logout',"
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{'return_status_logout': 400, 'expected_logouts': 1}";

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    set_task_end_delay(1000);
    return run_task_authenticate_test(
        argc, argv,
        APP_NAME, APP_DOC,
        task_kw_snippet,
        mock_idp_kw_snippet,
        0,             // expected_result
        "task_authenticate: the IdP did not end the session",
        TRUE            // capture warnings too
    );
}
