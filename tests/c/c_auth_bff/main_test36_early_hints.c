/****************************************************************************
 *          MAIN_TEST36_EARLY_HINTS.C
 *
 *          Self-contained auth_bff test of an IdP that answers a 1xx (103
 *          Early Hints) before its final answer. One BFF with one IdP:
 *
 *            bff1 (BFF1_PORT)  issuer A
 *            A    (IDP_A_PORT) a mock Keycloak; the driver sets its
 *                              raw_answer: a 103, then the 200
 *
 *          The driver (c_test36_early_hints.c) says what each step must
 *          get: each login its own answer.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>

#include "c_mock_keycloak.h"
#include "c_test36_early_hints.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_auth_bff_test36_early_hints"
#define APP_DOC         "auth_bff: a 1xx of the IdP is not the answer of a request"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#define BFF1_PORT   "18958"
#define IDP_A_PORT  "18959"

#define IDP_A       "http://127.0.0.1:"IDP_A_PORT"/realms/test"

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
            'name': 'c_test36_early_hints',                      \n\
            'gclass': 'C_TEST36_EARLY_HINTS',                    \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'bff1_port': '"BFF1_PORT"'                          \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__idp_a_side__',                               \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'persistent_channels': false                        \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'idpa_server',                          \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://0.0.0.0:"IDP_A_PORT"',        \n\
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
                    'name': 'idpa-1',                               \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'idpa-1',                       \n\
                            'gclass': 'C_MOCK_KEYCLOAK',            \n\
                            'kw': {                                 \n\
                                'issuer_claim': '"IDP_A"'           \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'idpa-1',               \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'idpa-1',       \n\
                                            'gclass': 'C_TCP'       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                },                                                  \n\
                {                                                   \n\
                    'name': 'idpa-2',                               \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'idpa-2',                       \n\
                            'gclass': 'C_MOCK_KEYCLOAK',            \n\
                            'kw': {                                 \n\
                                'issuer_claim': '"IDP_A"'           \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'idpa-2',               \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'idpa-2',       \n\
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
            'name': '__bff1_side__',                                \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'persistent_channels': false                        \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'bff1_server',                          \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://0.0.0.0:"BFF1_PORT"',         \n\
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
                    'name': 'bff1',                                 \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'bff1',                         \n\
                            'gclass': 'C_AUTH_BFF',                 \n\
                            'kw': {                                 \n\
                                'issuer': '"IDP_A"/',               \n\
                                'client_id':    'test-client',      \n\
                                'client_secret': '',                \n\
                                'cookie_domain': '',                \n\
                                'allowed_origin': 'http://localhost',\n\
                                'allowed_redirect_uri': '',         \n\
                                'crypto': {},                       \n\
                                'idp_timeout_ms': 3000              \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'bff1',                 \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'bff1',         \n\
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
    res += register_c_test36_early_hints();


    /*------------------------------------------------*
     *  Suppress noisy traces
     *------------------------------------------------*/
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER),  "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    // Samples of traces: DON'T REMOVE
    // gobj_set_gclass_trace(gclass_find_by_name(C_AUTH_BFF), "messages", TRUE);
    // gobj_set_global_trace("machine", TRUE);

    set_auto_kill_time(60);

    /*
     *  The logs of the run, in order, with the priority of each
     */
    json_t *jn_expected = json_array();
    #define EXPECT_CAUSE(prio, text, cause) \
        json_array_append_new(jn_expected, json_pack("{s:s, s:s, s:s}", \
            "msg", (text), "__priority__", (prio), "cause", (cause)))
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
     *  Capture WARNING+ logs for test verification.
     *  Any unexpected log will fail the test.
     */
    gobj_log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_WARNING, 0);

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
