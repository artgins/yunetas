/****************************************************************************
 *          MAIN_TEST18_DISCOVERY_FAILURE.C
 *
 *          Self-contained auth_bff test: BFF configured with `issuer`,
 *          mock IdP returns a discovery body that omits the required
 *          `token_endpoint` (and `end_session_endpoint`) fields.
 *
 *          Asserts that
 *
 *            (a) c_auth_bff::save_oidc_discovery rejects the malformed
 *                body and emits the corresponding gobj_log_error, and
 *            (b) the BFF's `discovery_done` stat counter stays 0 — so
 *                any subsequent /auth/login request would queue on
 *                dl_pending rather than being routed to a half-resolved
 *                IdP.
 *
 *          The mock IdP's new `override_discovery_body` knob lets us
 *          force-feed the broken document.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>

#include "c_mock_keycloak.h"
#include "c_test18_discovery_failure.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_auth_bff_test18_discovery_failure"
#define APP_DOC         "auth_bff OIDC discovery missing-endpoints failure"

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
            'name': 'c_test18_discovery_failure',                   \n\
            'gclass': 'C_TEST18_DISCOVERY_FAILURE',                 \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'check_delay_ms': 1500                              \n\
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
                                'issuer': 'http://127.0.0.1:"KC_PORT"/realms/test/', \n\
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
                    'name': 'idp-1',                                \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'idp-1',                        \n\
                            'gclass': 'C_MOCK_KEYCLOAK',            \n\
                            'kw': {                                 \n\
                                'override_discovery_body': {        \n\
                                    'issuer': 'http://127.0.0.1:"KC_PORT"/realms/test' \n\
                                }                                   \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'idp-1',                \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
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

static int register_yuno_and_more(void)
{
    int res = 0;

    if(mock_keycloak_init_jwk() < 0) {
        return -1;
    }

    res += register_c_mock_keycloak();
    res += register_c_test18_discovery_failure();

    /* Suppress noisy traces */
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER),  "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    set_auto_kill_time(10);

    /*
     *  Expect exactly one ERROR captured: the BFF's
     *  save_oidc_discovery rejecting the malformed body.
     *  kw_match_simple is a subset match — only `msg` is required.
     */
    json_t *expected_errors = json_pack("[{s:s}]",
        "msg", "👤BFF OIDC discovery: missing required endpoints in response"
    );
    set_expected_results(
        APP_NAME,
        expected_errors,
        NULL,           // no JSON comparison
        NULL,           // no ignore_keys
        TRUE            // verbose
    );

    MT_START_TIME(time_measure)

    return res;
}

static void cleaning(void)
{
    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, APP_NAME)

    result += test_json(NULL);
    mock_keycloak_end_jwk();
}

int main(int argc, char *argv[])
{
    glog_init();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    gobj_log_register_handler(
        "testing",
        0,
        capture_log_write,
        0
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_ERROR, 0);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

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
