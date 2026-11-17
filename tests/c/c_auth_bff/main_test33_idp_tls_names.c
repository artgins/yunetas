/****************************************************************************
 *          MAIN_TEST33_IDP_TLS_NAMES.C
 *
 *          Self-contained auth_bff test: each IdP of `issuers` is
 *          verified against its OWN name. A BFF with two https issuers,
 *          `localhost` and `127.0.0.1`, on ports where nobody listens: both
 *          outbounds set up TLS (C_TCP writes the ssl_server_name of its url
 *          into its crypto dict) and are refused. Up to the first build of
 *          7.31.6 the two shared the BFF's crypto dict, so the second was
 *          verified against the name of the first ("hostname mismatch" in
 *          production, auth.artgins.es checked as auth.artgins.com).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>

#include "c_test33_idp_tls_names.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_auth_bff_test33_idp_tls_names"
#define APP_DOC         "auth_bff: each IdP of issuers verified against its own name"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#define BFF_PORT    "18948"
#define IDP1_PORT   "18949"
#define IDP2_PORT   "18950"

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
            'name': 'c_test33_idp_tls_names',               \n\
            'gclass': 'C_TEST33_IDP_TLS_NAMES',               \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'check_delay_ms': 3000,                             \n\
                'bff_port': '"BFF_PORT"'                            \n\
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
                                'issuers': ['https://localhost:"IDP1_PORT"/realms/test/', 'https://127.0.0.1:"IDP2_PORT"/realms/test/'], \n\
                                'client_id':    'test-client',      \n\
                                'client_secret': '',                \n\
                                'cookie_domain': '',                \n\
                                'allowed_origin': 'http://localhost', \n\
                                'allowed_redirect_uri': '',         \n\
                                'crypto': {'ssl_use_system_ca': true, 'ssl_verify_mode': 'required'}, \n\
                                'pending_queue_size': 16,           \n\
                                'idp_timeout_ms': 500,              \n\
                                'idp_connect_timeout_ms': 500,      \n\
                                'idp_failover_connect_ms': 500      \n\
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

    res += register_c_test33_idp_tls_names();

    /* Suppress noisy traces */
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER),  "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    set_auto_kill_time(10);

    /*
     *  The first IdP fails its discovery at the start (it is down); the
     *  login goes to the second, which fails too, and is answered 502.
     *  The second waits idp_connect_timeout_ms: it is the last IdP left.
     */
    json_t *expected_errors = json_pack(
        "[{s:s, s:s, s:s}, {s:s, s:s}, {s:s, s:s, s:s}, {s:s, s:s}, {s:s, s:s}]",
        "msg",          "👤BFF OIDC discovery: IdP did not answer in time",
        "__priority__", "ERROR",
        "issuer",       "https://localhost:"IDP1_PORT"/realms/test/",
        "msg",          "👤BFF IdP down, the logins go to the next IdP",
        "__priority__", "WARNING",
        "msg",          "👤BFF OIDC discovery: IdP did not answer in time",
        "__priority__", "ERROR",
        "issuer",       "https://127.0.0.1:"IDP2_PORT"/realms/test/",
        "msg",          "👤BFF IdP down, the logins go to the next IdP",
        "__priority__", "WARNING",
        "msg",          "👤BFF server error",
        "error_code",   "auth_service_unavailable"
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
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_WARNING, 0);

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
