/****************************************************************************
 *          MAIN_TEST5_FORM_ENCODING.C
 *
 *          Self-contained c_task_authenticate test: credentials that hold
 *          the bytes of a form ("+", "%", "&", "=", "@").
 *
 *          The user is "user+1@x.com" and the password
 *          "p+a%41&client_id=evil&grant_type=x". The task builds its
 *          /token form through C_PROT_HTTP_CL, which form-urlencodes each
 *          key and value: the mock IdP must find ONE grant_type, ONE
 *          client_id, and the username and the password as they are
 *          (`expected_form`); else it logs an ERROR and answers 400. The
 *          driver expects EV_ON_TOKEN result=0 and no captured error.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "test_main.h"

#define APP_NAME    "test_task_authenticate_test5_form_encoding"
#define APP_DOC     "task_authenticate credentials with the bytes of a form"

PRIVATE char task_kw_snippet[] =
    "{                                                                                              "
    "  'token_endpoint':       'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/token', "
    "  'end_session_endpoint': 'http://127.0.0.1:" IDP_PORT "/realms/test/protocol/openid-connect/logout',"
    "  'client_id':  'test-client',                                                                "
    "  'user_id':    'user+1@x.com',                                                               "
    "  'user_passw': 'p+a%41&client_id=evil&grant_type=x'                                          "
    "}                                                                                             ";

PRIVATE char mock_idp_kw_snippet[] =
    "{                                                                  "
    "  'expected_form': {                                               "
    "    'grant_type': 'password',                                      "
    "    'client_id':  'test-client',                                   "
    "    'username':   'user+1@x.com',                                  "
    "    'password':   'p+a%41&client_id=evil&grant_type=x'             "
    "  }                                                                "
    "}                                                                  ";

int main(int argc, char *argv[])
{
    return run_task_authenticate_test(
        argc, argv,
        APP_NAME, APP_DOC,
        task_kw_snippet,
        mock_idp_kw_snippet,
        0,              // expected_result: success
        NULL,           // no expected log
        FALSE           // capture errors only
    );
}
