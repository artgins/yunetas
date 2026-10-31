/****************************************************************************
 *          MAIN_TEST17_DISCOVERY_CUT.C
 *
 *          Self-contained c_task_authenticate test: the mock IdP drops the connection of every
 *          discovery: it goes twice, and the caller gets result=-1 and a
 *          WARNING: an IdP that always cuts does not hold the task for ever.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test17_discovery_cut"
#define APP_DOC     "task_authenticate: an IdP that drops the connection of every discovery"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'issuer':     'http://127.0.0.1:" IDP_PORT "/realms/test/',                                 "
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'mockuser',                                                                   "
    "  'user_passw': 'mockpass'                                                                    "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{'drop_discovery_requests': 9}";

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
        "[\"task_authenticate: the IdP closed the connection before answering, the request is sent again\", \"task_authenticate: the IdP closed the connection before answering, the request is not sent again\"]",
        TRUE            // capture warnings too
    );
}
