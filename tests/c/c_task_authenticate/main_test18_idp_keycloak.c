/****************************************************************************
 *          MAIN_TEST18_IDP_KEYCLOAK.C
 *
 *          Self-contained test of C_IDP_KEYCLOAK against the mock IdP: what
 *          goes again when a connection is cut, what does not, and what it
 *          takes from the answers. The driver (c_idp_driver.c) says what
 *          each step must get. The logs, in order: a WARNING for the token
 *          too long, and for each request given up. No ERROR.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include <command_parser.h>

#include "c_mock_idp.h"
#include "c_idp_driver.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_task_authenticate_test18_idp_keycloak"
#define APP_DOC         "C_IDP_KEYCLOAK: what goes again when a connection is cut"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#ifndef IDP_PORT
#error "build with -DIDP_PORT=\"<port>\": one port per test binary (see CMakeLists.txt)"
#endif

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

PRIVATE char variable_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'work_dir': '/tmp',                                         \n\
        'console_log_handlers': {},                                 \n\
        'daemon_log_handlers': {}                                   \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'autoplay': true,                                           \n\
        'required_services': [],                                    \n\
        'public_services': [],                                      \n\
        'service_descriptor': {},                                   \n\
        'realm_owner': 'test',                                      \n\
        'realm_id':    'test',                                      \n\
        'trace_levels': {}                                          \n\
    },                                                              \n\
    'global': {                                                     \n\
        'Authz.allow_anonymous_in_localhost': true                  \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'idp-driver',                                   \n\
            'gclass': 'C_IDP_DRIVER',                               \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true                                        \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'idp',                                          \n\
            'gclass': 'C_IDP_KEYCLOAK',                             \n\
            'autostart': true,                                      \n\
            'kw': {                                                 \n\
                'kc_base_url': 'http://127.0.0.1:" IDP_PORT "',     \n\
                'kc_realm': 'test',                                 \n\
                'kc_admin_client_id': 'admin-cli',                  \n\
                'kc_admin_client_secret': 'secret',                 \n\
                'kc_crypto': {},                                    \n\
                'kc_timeout_ms': 5000                               \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__idp_side__',                                 \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'persistent_channels': false                        \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'idp_server',                           \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://0.0.0.0:" IDP_PORT "',        \n\
                        'child_tree_filter': {                      \n\
                            'kw': {                                 \n\
                                '__gclass_name__': 'C_CHANNEL',     \n\
                                '__disabled__': false,              \n\
                                'connected': false                  \n\
                            }                                       \n\
                        }                                           \n\
                    }                                               \n\
                },                                                  \n\
                {                                                   \n\
                    'name': 'idp-1',                                \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {},                                       \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'idp-1',                        \n\
                            'gclass': 'C_MOCK_IDP',                 \n\
                            'kw': {},                               \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'idp-1',                \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {},                       \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'idp-1',        \n\
                                            'gclass': 'C_TCP'       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************************
 *  Authz checker: allow everything in the self-contained test
 ***************************************************************************/
PRIVATE BOOL test_authz_checker(hgobj gobj, const char *authz, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return TRUE;
}

time_measure_t time_measure;
int result = 0;

/***************************************************************************
 *  HACK: runs on yunetas environment BEFORE creating the yuno
 ***************************************************************************/
static int register_yuno_and_more(void)
{
    int res = 0;

    res += register_c_mock_idp();
    res += register_c_idp_driver();

    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER),  "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    /*------------------------------------------------*
     *  Safety: kill the yuno after 30 s if stuck
     *------------------------------------------------*/
    set_auto_kill_time(30);

    json_t *jn_expected = json_array();
    #define EXPECT_MSG(text) \
        json_array_append_new(jn_expected, json_pack("{s:s}", "msg", (text)))

    /*  1: the token too long  */
    EXPECT_MSG("Keycloak admin token longer than this gclass keeps");
    /*  3, 5, 6: requests given up  */
    for(int i = 0; i < 3; i++) {
        EXPECT_MSG("Keycloak closed the connection before answering, the request is not sent again");
    }
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
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    glog_init();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_WARNING, 0);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    yuneta_setup(
        NULL,               // persistent_attrs
        command_parser,
        NULL,               // stats_parser
        test_authz_checker,
        NULL,               // authentication_parser
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
