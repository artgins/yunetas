/****************************************************************************
 *          MAIN_TEST1_LOGIN.C
 *
 *          Self-contained auth_bff test: happy path POST /auth/login.
 *
 *          Topology (all on 127.0.0.1):
 *            c_test1_login   (driver, default service, autoplay)
 *            __bff_side__    (C_IOGATE)
 *                └── server (C_TCP_S) :18801
 *                └── bff-1  (C_CHANNEL)
 *                    └── bff-1 (C_AUTH_BFF) → crypto.url = http://127.0.0.1:18802/
 *                        └── bff-1 (C_PROT_HTTP_SR)
 *                            └── bff-1 (C_TCP)
 *            __kc_side__     (C_IOGATE)
 *                └── server (C_TCP_S) :18802
 *                └── kc-1   (C_CHANNEL)
 *                    └── kc-1 (C_MOCK_KEYCLOAK) → defaults: 200 + mockuser
 *                        └── kc-1 (C_PROT_HTTP_SR)
 *                            └── kc-1 (C_TCP)
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>

#include "c_mock_keycloak.h"
#include "c_test1_login.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_auth_bff_test1_login"
#define APP_DOC         "Self-contained auth_bff /auth/login happy path test"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#define BFF_PORT    "18801"
#define KC_PORT     "18802"

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
        'console_log_handlers': {                                   \n\
        },                                                          \n\
        'daemon_log_handlers': {                                    \n\
        }                                                           \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'autoplay': true,                                           \n\
        'required_services': [],                                    \n\
        'public_services': [],                                      \n\
        'service_descriptor': {                                     \n\
        },                                                          \n\
        'realm_owner': 'test',                                      \n\
        'realm_id':    'test',                                      \n\
        'trace_levels': {                                           \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
        'Authz.allow_anonymous_in_localhost': true                  \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'c_test1_login',                                \n\
            'gclass': 'C_TEST1_LOGIN',                              \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'bff_url': 'http://127.0.0.1:"BFF_PORT"/'           \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__bff_side__',                                 \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'persistent_channels': false                        \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'bff_server',                           \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://0.0.0.0:"BFF_PORT"',          \n\
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
                    'name': 'bff-1',                                \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'bff-1',                        \n\
                            'gclass': 'C_AUTH_BFF',                 \n\
                            'kw': {                                 \n\
                                'keycloak_url': 'http://127.0.0.1:"KC_PORT"/', \n\
                                'realm':        'test',             \n\
                                'client_id':    'test-client',      \n\
                                'client_secret': '',                \n\
                                'cookie_domain': '',                \n\
                                'allowed_origin': '',               \n\
                                'allowed_redirect_uri': '',         \n\
                                'crypto': {},                       \n\
                                'pending_queue_size': 16            \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'bff-1',                \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'bff-1',        \n\
                                            'gclass': 'C_TCP'       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__kc_side__',                                  \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'persistent_channels': false                        \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'kc_server',                            \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://0.0.0.0:"KC_PORT"',           \n\
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
                    'name': 'kc-1',                                 \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'kc-1',                         \n\
                            'gclass': 'C_MOCK_KEYCLOAK',            \n\
                            'kw': {                                 \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'kc-1',                 \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'kc-1',         \n\
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
    res += register_c_test1_login();

    /*------------------------------------------------*
     *  Suppress noisy traces
     *------------------------------------------------*/
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER),  "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    // Samples of traces
    // gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_SRV), "identity-card", TRUE);
    // gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "identity-card", TRUE);

    // gobj_set_gclass_trace(gclass_find_by_name(C_TCP), "traffic", TRUE);

    // Global traces
    // gobj_set_global_trace("create_delete", TRUE);
    // gobj_set_global_trace("machine", TRUE);
    // gobj_set_global_trace("create_delete2", TRUE);
    // gobj_set_global_trace("subscriptions", TRUE);
    // gobj_set_global_trace("start_stop", TRUE);
    // gobj_set_global_trace("monitor", TRUE);
    // gobj_set_global_trace("event_monitor", TRUE);
    // gobj_set_global_trace("liburing", TRUE);
    // gobj_set_global_trace("ev_kw", TRUE);
    // gobj_set_global_trace("authzs", TRUE);
    // gobj_set_global_trace("states", TRUE);
    // gobj_set_global_trace("gbuffers", TRUE);
    // gobj_set_global_trace("timer_periodic", TRUE);
    // gobj_set_global_trace("timer", TRUE);
    // gobj_set_global_trace("liburing_timer", TRUE);

    /*------------------------------------------------*
     *  Safety: kill the yuno after 10 s if stuck
     *------------------------------------------------*/
    set_auto_kill_time(10);

    /*------------------------------*
     *  Capture only errors
     *------------------------------*/
    set_expected_results(
        APP_NAME,
        json_array(),   // empty — we expect no errors
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
     *  Capture only ERROR+ logs for test verification.
     *  Any unexpected error log will fail the test.
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
