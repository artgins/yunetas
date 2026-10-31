/****************************************************************************
 *          MAIN_TEST25_UNTRUSTED_INPUT.C
 *
 *          Self-contained auth_bff test of what the BFF takes from others:
 *          the credentials and the refresh_token cookie it puts in the form
 *          to the IdP (each key and value form-urlencoded, and whole), the
 *          tokens it puts in a Set-Cookie (refused with any byte a cookie
 *          cannot carry), the Origin of a logout, a refresh, a login, a
 *          callback and a token (exactly allowed_origin), the size of a
 *          token cookie (the rule of the cookies the BFF writes, in
 *          refresh, logout and token), the body of an IdP answer (1 MB at
 *          most), the Retry-After of a 429 of the IdP (passed on when it is
 *          delay-seconds or a real HTTP-date, and exposed to a SPA of
 *          another origin), an IdP that closes the connection before
 *          answering (the request not sent again), and requests pipelined
 *          on one connection (answered in their order; more waiting than
 *          the queue drop the connection). The driver
 *          (c_test25_untrusted_input.c) says what each step must get. The
 *          logs, in order: the WARNING of each 4xx; for a token that is not
 *          a cookie value, a WARNING naming it and its byte, and the
 *          WARNING of the 502; for the JSON string "", the WARNING of its
 *          502; for the body over the limit, the WARNING of the HTTP client
 *          and the WARNING of the 502; for a 429 of the IdP, the WARNING of
 *          the 429, after the WARNING of a Retry-After not passed on; for
 *          the IdP that closes the connection, the ERROR of the 502; for
 *          17, 40 and 600 requests pipelined behind a login, the WARNING of
 *          each of the first 17 404s and the WARNING of the connection
 *          dropped.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>

#include "c_mock_keycloak.h"
#include "c_test25_untrusted_input.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_auth_bff_test25_untrusted_input"
#define APP_DOC         "auth_bff: the form to the IdP, its tokens, bodies and Retry-After, the Origin of requests, the size of cookies, pipelined requests"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#define BFF_PORT    "18888"
#define KC_PORT     "18889"

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
            'name': 'c_test25_untrusted_input',                                \n\
            'gclass': 'C_TEST25_UNTRUSTED_INPUT',                              \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
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
                                'issuer': 'http://127.0.0.1:"KC_PORT"/realms/test/', \n\
                                'client_id':    'test-client',      \n\
                                'client_secret': '',                \n\
                                'cookie_domain': '',                \n\
                                'allowed_origin': 'http://localhost', \n\
                                'expose_access_token': true,        \n\
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
            'name': '__idp_side__',                                  \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'persistent_channels': false                        \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'idp_server',                            \n\
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
                    'name': 'idp-1',                                 \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'idp-1',                         \n\
                            'gclass': 'C_MOCK_KEYCLOAK',            \n\
                            'kw': {                                 \n\
                                'access_token_pad': 0               \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'idp-1',                 \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'idp-1',         \n\
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
    res += register_c_test25_untrusted_input();

    /*------------------------------------------------*
     *  Suppress noisy traces
     *------------------------------------------------*/
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER),  "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    // Samples of traces: DON'T REMOVE
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
    set_auto_kill_time(20);

    /*------------------------------*
     *  Captured: errors and warnings (see main()), in order.
     *------------------------------*/
    json_t *jn_expected = json_array();
    #define EXPECT_REJECTED(code) \
        json_array_append_new(jn_expected, json_pack("{s:s, s:s}", \
            "msg", "👤BFF request rejected", "error_code", (code)))
    #define EXPECT_REFUSED(code) \
        json_array_append_new(jn_expected, json_pack("{s:s, s:s}", \
            "msg", "👤BFF IdP answer refused", "error_code", (code)))
    #define EXPECT_NOT_COOKIE(tok, byte) \
        json_array_append_new(jn_expected, json_pack("{s:s, s:s, s:s}", \
            "msg", "👤BFF IdP answered a token that is not a cookie value", \
            "token", (tok), "byte", (byte)))
    #define EXPECT_RETRY_AFTER_NOT_PASSED(value) \
        json_array_append_new(jn_expected, json_pack("{s:s, s:s}", \
            "msg", "👤BFF IdP answered a Retry-After that is not delay-seconds " \
                   "(1 to 10 digits) nor an HTTP-date, not passed on", \
            "value", (value)))

    /*  4-10: the Origin of a logout  */
    for(int i = 0; i < 7; i++) {
        EXPECT_REJECTED("origin_not_allowed");
    }
    /*  11-12: a token that is not a cookie value  */
    EXPECT_NOT_COOKIE("access_token", "0x3B");
    EXPECT_REFUSED("auth_unexpected_error");
    EXPECT_NOT_COOKIE("refresh_token", "0x0D");
    EXPECT_REFUSED("auth_unexpected_error");
    /*  14, 15, 18: a cookie longer than the BFF writes  */
    for(int i = 0; i < 3; i++) {
        EXPECT_REJECTED("token_too_large");
    }
    /*  19: the JSON string ""  */
    EXPECT_REFUSED("auth_empty_response");
    /*  20: an IdP body over the limit  */
    json_array_append_new(jn_expected, json_pack("{s:s, s:s}",
        "msg",              "HTTP body larger than its limit, message refused",
        "http_parser_type", "response"
    ));
    EXPECT_REFUSED("auth_unexpected_error");
    /*  22-25: a 429 and its Retry-After  */
    EXPECT_REJECTED("auth_rate_limited");
    EXPECT_REJECTED("auth_rate_limited");
    EXPECT_RETRY_AFTER_NOT_PASSED("soon");
    EXPECT_REJECTED("auth_rate_limited");
    EXPECT_RETRY_AFTER_NOT_PASSED("-5");
    EXPECT_REJECTED("auth_rate_limited");
    /*  26-31: every byte outside cookie-octet  */
    {
        const char *bytes[] = {"0xC3", "0x7F", "0x20", "0x22", "0x2C", "0x5C"};
        for(int i = 0; i < 6; i++) {
            EXPECT_NOT_COOKIE("access_token", bytes[i]);
            EXPECT_REFUSED("auth_unexpected_error");
        }
    }
    /*  32-45: Retry-After not passed on, or passed on (NULL)  */
    {
        const char *values[] = {
            "Wed, 99 Oct 2026 07:28:00 GMT",
            "Wed, 21 Oct 2026 99:99:99 GMT",
            "Thu, 21 Oct 2026 07:28:00 GMT",
            "Wed, 21 Foo 2026 07:28:00 GMT",
            "Xyz, 21 Oct 2026 07:28:00 GMT",
            "Sun, 29 Feb 2026 07:28:00 GMT",
            "12345678901",
            NULL, NULL, NULL, NULL, NULL,
            "120, 60",
            NULL
        };
        for(size_t i = 0; i < sizeof(values)/sizeof(values[0]); i++) {
            if(values[i]) {
                EXPECT_RETRY_AFTER_NOT_PASSED(values[i]);
            }
            EXPECT_REJECTED("auth_rate_limited");
        }
    }
    /*  46-51: the Origin of a refresh, a login, a callback, a token, a logout  */
    for(int i = 0; i < 6; i++) {
        EXPECT_REJECTED("origin_not_allowed");
    }
    /*  53: the IdP closes the connection before answering  */
    json_array_append_new(jn_expected, json_pack("{s:s, s:s}",
        "msg",          "👤BFF server error",
        "error_code",   "auth_service_unavailable"
    ));
    /*  55: three requests in one write  */
    EXPECT_REJECTED("unknown_endpoint");
    EXPECT_REJECTED("method_not_allowed");
    /*  56-58: more pipelined requests than the queue, each 404 until the drop  */
    for(int f = 0; f < 3; f++) {
        for(int i = 0; i < 17; i++) {
            EXPECT_REJECTED("unknown_endpoint");
        }
        json_array_append_new(jn_expected, json_pack("{s:s}",
            "msg",          "👤BFF too many pipelined requests waiting for their "
                            "answer, connection dropped"
        ));
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
