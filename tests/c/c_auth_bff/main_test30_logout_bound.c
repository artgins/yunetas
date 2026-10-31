/****************************************************************************
 *          MAIN_TEST30_LOGOUT_BOUND.C
 *
 *          Self-contained auth_bff test of the time of a logout: a logout
 *          answers within idp_logout_timeout_ms (10 s by default), cookies
 *          cleared, with an IdP that is mute or cannot be reached, and with
 *          the end-session sent again or the discovery still to do. The
 *          driver (c_test30_logout_bound.c) says what each BFF must answer.
 *          Every BFF has the default times but the last.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>

#include "c_mock_keycloak.h"
#include "c_test30_logout_bound.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_auth_bff_test30_logout_bound"
#define APP_DOC         "auth_bff: a logout answers within idp_logout_timeout_ms, cookies cleared"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#define BFF1_PORT   "18898"
#define BFF2_PORT   "18900"
#define BFF3_PORT   "18901"
#define BFF4_PORT   "18902"
#define BFF5_PORT   "18903"
#define BFF6_PORT   "18904"
#define KC1_PORT    "18905"
#define KC3_PORT    "18906"
#define KC4_PORT    "18907"
#define KC6_PORT    "18908"
#define NO_PORT     "1"     /* nobody listens */

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': '"APP_NAME"',                                  \n\
        'tags': ['test', 'yunetas']                                 \n\
    }                                                               \n\
}                                                                   \n\
";

/*
 *  One BFF on its own port, and one mock IdP on its own port
 */
#define ENDPOINTS(port)                                                                 \
    "'token_endpoint': 'http://127.0.0.1:" port "/realms/test/protocol/openid-connect/token'," \
    "'end_session_endpoint': 'http://127.0.0.1:" port "/realms/test/protocol/openid-connect/logout',"

#define ISSUER(port)                                                    \
    "'issuer': 'http://127.0.0.1:" port "/realms/test/',"

#define BFF_SERVICE(name, port, idp_kw)                                 \
    "{"                                                                 \
    "  'name': '" name "', 'gclass': 'C_IOGATE',"                       \
    "  'autostart': true, 'autoplay': true,"                            \
    "  'kw': {'persistent_channels': false},"                           \
    "  'children': ["                                                   \
    "    {'name': 'server', 'gclass': 'C_TCP_S',"                       \
    "     'kw': {"                                                      \
    "       'url': 'tcp://0.0.0.0:" port "',"                           \
    "       'child_tree_filter': {'kw': {"                              \
    "         '__gclass_name__': 'C_CHANNEL',"                          \
    "         '__disabled__': false,"                                   \
    "         'connected': false"                                       \
    "       }}"                                                         \
    "     }"                                                            \
    "    },"                                                            \
    "    {'name': 'channel', 'gclass': 'C_CHANNEL', 'kw': {},"          \
    "     'children': ["                                                \
    "       {'name': '" name "-bff', 'gclass': 'C_AUTH_BFF',"           \
    "        'kw': {"                                                   \
    idp_kw                                                              \
    "          'client_id': 'test-client',"                             \
    "          'allowed_origin': 'http://localhost',"                   \
    "          'crypto': {}"                                            \
    "        },"                                                        \
    "        'children': ["                                             \
    "          {'name': '" name "-http', 'gclass': 'C_PROT_HTTP_SR', 'kw': {}," \
    "           'children': ["                                          \
    "             {'name': '" name "-tcp', 'gclass': 'C_TCP'}"          \
    "           ]"                                                      \
    "          }"                                                       \
    "        ]"                                                         \
    "       }"                                                          \
    "     ]"                                                            \
    "    }"                                                             \
    "  ]"                                                               \
    "},"

#define IDP_SERVICE(name, port, mock_kw)                                \
    "{"                                                                 \
    "  'name': '" name "', 'gclass': 'C_IOGATE',"                       \
    "  'autostart': true, 'autoplay': true,"                            \
    "  'kw': {'persistent_channels': false},"                           \
    "  'children': ["                                                   \
    "    {'name': 'server', 'gclass': 'C_TCP_S',"                       \
    "     'kw': {"                                                      \
    "       'url': 'tcp://0.0.0.0:" port "',"                           \
    "       'child_tree_filter': {'kw': {"                              \
    "         '__gclass_name__': 'C_CHANNEL',"                          \
    "         '__disabled__': false,"                                   \
    "         'connected': false"                                       \
    "       }}"                                                         \
    "     }"                                                            \
    "    },"                                                            \
    "    {'name': 'channel', 'gclass': 'C_CHANNEL', 'kw': {},"          \
    "     'children': ["                                                \
    "       {'name': '" name "-mock', 'gclass': 'C_MOCK_KEYCLOAK',"     \
    "        'kw': {" mock_kw "},"                                      \
    "        'children': ["                                             \
    "          {'name': '" name "-http', 'gclass': 'C_PROT_HTTP_SR', 'kw': {}," \
    "           'children': ["                                          \
    "             {'name': '" name "-tcp', 'gclass': 'C_TCP'}"          \
    "           ]"                                                      \
    "          }"                                                       \
    "        ]"                                                         \
    "       }"                                                          \
    "     ]"                                                            \
    "    }"                                                             \
    "  ]"                                                               \
    "},"

PRIVATE char variable_config[]=
"{"
"    'environment': {"
"        'work_dir': '/tmp',"
"        'console_log_handlers': {},"
"        'daemon_log_handlers': {}"
"    },"
"    'yuno': {"
"        'autoplay': true,"
"        'required_services': [],"
"        'public_services': [],"
"        'service_descriptor': {},"
"        'realm_owner': 'test',"
"        'realm_id':    'test',"
"        'trace_levels': {}"
"    },"
"    'global': {"
"        'Authz.allow_anonymous_in_localhost': true"
"    },"
"    'services': ["
    BFF_SERVICE("__bff_1__", BFF1_PORT, ENDPOINTS(KC1_PORT))
    BFF_SERVICE("__bff_2__", BFF2_PORT, ENDPOINTS(NO_PORT))
    BFF_SERVICE("__bff_3__", BFF3_PORT, ENDPOINTS(KC3_PORT))
    BFF_SERVICE("__bff_4__", BFF4_PORT, ISSUER(KC4_PORT))
    BFF_SERVICE("__bff_5__", BFF5_PORT, ISSUER(NO_PORT))
    BFF_SERVICE("__bff_6__", BFF6_PORT, ENDPOINTS(KC6_PORT) "'idp_logout_timeout_ms': 1000,")
    IDP_SERVICE("__idp_mute__", KC1_PORT, "'silent_logout': true")
    IDP_SERVICE("__idp_cut__", KC3_PORT, "'silent_logout': true, 'drop_logout_requests': 1")
    IDP_SERVICE("__idp_mute_discovery__", KC4_PORT, "'silent_discovery': true")
    IDP_SERVICE("__idp_slow__", KC6_PORT, "'latency_ms': 800")
"        {"
"            'name': 'c_test30_logout_bound',"
"            'gclass': 'C_TEST30_LOGOUT_BOUND',"
"            'default_service': true,"
"            'autostart': true,"
"            'autoplay': true"
"        }"
"    ]"
"}";

/***************************************************************************
 *  Authz checker: allow everything in the self-contained test
 ***************************************************************************/
PRIVATE BOOL test_authz_checker(hgobj gobj, const char *authz, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return TRUE;
}

time_measure_t time_measure;

/***************************************************************************
 *  HACK: runs on yunetas environment BEFORE creating the yuno
 ***************************************************************************/
int result = 0;

static int register_yuno_and_more(void)
{
    int res = 0;

    /*--------------------*
     *  Init JWT key first — any GClass that signs tokens depends on it
     *--------------------*/
    if(mock_keycloak_init_jwk() < 0) {
        return -1;  // Error already logged
    }

    /*--------------------*
     *  Register gclasses
     *--------------------*/
    res += register_c_mock_keycloak();
    res += register_c_test30_logout_bound();

    /*------------------------------------------------*
     *  Suppress noisy traces
     *------------------------------------------------*/
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER),  "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    // Samples of traces: DON'T REMOVE
    // gobj_set_gclass_trace(gclass_find_by_name(C_TCP), "traffic", TRUE);

    // Global traces
    // gobj_set_global_trace("machine", TRUE);
    // gobj_set_global_trace("create_delete", TRUE);

    /*------------------------------------------------*
     *  Safety: kill the yuno if stuck, after the SPA gave up on every BFF
     *------------------------------------------------*/
    set_auto_kill_time(25);

    /*------------------------------*
     *  Captured: errors, in order. They come in the order of the requests,
     *  which the driver sends one per tick.
     *------------------------------*/
    json_t *jn_expected = json_array();
    #define EXPECT_MSG(text, cause) \
        json_array_append_new(jn_expected, json_pack("{s:s, s:s}", "msg", (text), "cause", (cause)))
    #define EXPECT_SERVER_ERROR(code) \
        json_array_append_new(jn_expected, json_pack("{s:s, s:s}", \
            "msg", "👤BFF server error", "error_code", (code)))

    /*  1, 2 and 3: the task of the logout  */
    for(int i = 0; i < 3; i++) {
        EXPECT_MSG("👤BFF IdP outbound watchdog fired",
            "idp_logout_timeout_ms: the logout did not end in its time");
        EXPECT_SERVER_ERROR("auth_timeout");
    }
    /*  4 and 5: the discovery the logouts waited for: two in 4, one in 5  */
    EXPECT_MSG("👤BFF OIDC discovery: IdP did not answer in time",
        "idp_logout_timeout_ms: the logout did not end in its time");
    EXPECT_SERVER_ERROR("auth_service_unavailable");
    EXPECT_SERVER_ERROR("auth_service_unavailable");
    EXPECT_MSG("👤BFF OIDC discovery: IdP did not answer in time",
        "idp_logout_timeout_ms: the logout did not end in its time");
    EXPECT_SERVER_ERROR("auth_service_unavailable");
    set_expected_results(
        APP_NAME,
        jn_expected,
        NULL,           // no JSON comparison
        NULL,           // no ignore_keys
        TRUE            // verbose
    );

    MT_START_TIME(time_measure)

    return res;
}

/***************************************************************************
 *  HACK: runs on yunetas environment BEFORE destroying the yuno
 ***************************************************************************/
static void cleaning(void)
{
    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, APP_NAME)

    result += test_json(NULL);  // check captured error log
    mock_keycloak_end_jwk();
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*------------------------------*
     *  Init logger
     *------------------------------*/
    glog_init();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    /*
     *  Capture only ERROR+ logs: the IdP that cannot be reached, and the one
     *  that cuts, warn all the time they last.
     */
    gobj_log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_ERROR, 0);

    /*------------------------------------------------*
     *      Memory leak check
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    yuneta_setup(
        NULL,       // persistent_attrs
        NULL,       // command_parser
        NULL,       // stats_parser
        test_authz_checker,
        NULL,       // authentication_parser
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY,
        USE_OWN_SYSTEM_MEMORY,
        MEM_MIN_BLOCK,
        MEM_SUPERBLOCK
    );

    result += yuneta_entry_point(
        argc, argv,
        APP_NAME, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        variable_config,
        register_yuno_and_more,
        cleaning
    );

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    return result;
}
