/****************************************************************************
 *          MAIN.C
 *
 *          Main of test_c_ievent_cli_operator
 *          Tests how C_IEVENT_CLI takes the operator of a request of its
 *          server: verified from its token (verify_operator), or as the
 *          server says (OPERATOR_UNVERIFIED, test_c_ievent_cli_operator_unverified)
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include "c_test_cli_operator.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#ifdef OPERATOR_UNVERIFIED
    #define APP_NAME    "test_c_ievent_cli_operator_unverified"
    #define TEST_PORT   "7892"
    #define VERIFY      "false"
    #define STORE       "/tmp/test_c_ievent_cli_operator_unverified"
#else
    #define APP_NAME    "test_c_ievent_cli_operator"
    #define TEST_PORT   "7891"
    #define VERIFY      "true"
    #define STORE       "/tmp/test_c_ievent_cli_operator"
#endif
#define APP_DOC         "Test how C_IEVENT_CLI takes the operator of a request of its server"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

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
 *  The client
 */
#define CLI_TREE "\
        {                                                           \n\
            'name': 'cli',                                          \n\
            'gclass': 'C_IEVENT_CLI',                               \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
                'jwt': 'tester',                                    \n\
                'verify_operator': " VERIFY ",                      \n\
                'service_principal': 'the-controlcenter',           \n\
                'remote_yuno_name': '',                             \n\
                'remote_yuno_role': 'raw_server',                   \n\
                'remote_yuno_service': 'raw'                        \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'cli_gate',                             \n\
                    'gclass': 'C_IOGATE',                           \n\
                    'as_service': true,                             \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'cli',                          \n\
                            'gclass': 'C_CHANNEL',                  \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'cli',                  \n\
                                    'gclass': 'C_WEBSOCKET',        \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'cli',          \n\
                                            'gclass': 'C_TCP',      \n\
                                            'kw': {                 \n\
                                    'url': 'ws://127.0.0.1:" TEST_PORT "', \n\
                                    'timeout_between_connections': 100 \n\
                                            }                       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
        }                                                           \n\
"


PRIVATE char variable_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
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
        'trace_levels': {                                           \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'tester',                                       \n\
            'gclass': 'C_TEST_CLI_OPERATOR',                        \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'authz',                                        \n\
            'gclass': 'C_AUTHZ',                                    \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'tranger_path': '" STORE "',                        \n\
                'master': true,                                     \n\
                'initial_load': {                                   \n\
                    'users': [                                      \n\
                        {                                           \n\
                            'id': 'bob@example.com',                \n\
                            'disabled': true                        \n\
                        }                                           \n\
                    ]                                               \n\
                },                                                  \n\
                'jwks': [                                           \n\
                    {                                               \n\
                        'kid': 'https://idp.test.example/realms/test', \n\
                        'kty': 'oct',                               \n\
                        'alg': 'HS256',                             \n\
                        'k': 'eXVuZXRhLW9wZXJhdG9yLXRlc3Qta2V5LTAxMjM0NTY' \n\
                    }                                               \n\
                ],                                                  \n\
                'assertion_time_leeway': 3600,                      \n\
                'assertion_jwks': [                                 \n\
                    {                                               \n\
                        'kid': 'test-controlcenter',                \n\
                        'kty': 'EC',                                \n\
                        'crv': 'P-256',                             \n\
                        'alg': 'ES256',                             \n\
                        'use': 'sig',                               \n\
                        'x': 'nXhUxejW5sVCIMMt5VPATGJS_nsEIvIHbw_xYLdpa1Q', \n\
                        'y': 'EmyPj6BtdnzvQTYMrLgqtRxTZig5KqIS1hh2YEB2kW8'  \n\
                    }                                               \n\
                ]                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__input_side__',                               \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'server_port',                          \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'ws://127.0.0.1:" TEST_PORT "',      \n\
                        'child_tree_filter': {                      \n\
                            'kw': {                                 \n\
                                '__gclass_name__': 'C_CHANNEL',     \n\
                                '__disabled__': false,              \n\
                                'connected': false                  \n\
                            }                                       \n\
                        }                                           \n\
                    }                                               \n\
                }                                                   \n\
            ],                                                      \n\
            '[^^children^^]': {                                     \n\
                '__range__': [1,2],                                 \n\
                '__vars__': {                                       \n\
                },                                                  \n\
                '__content__': {                                    \n\
                    'name': 'raw-(^^__range__^^)',                  \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'raw-(^^__range__^^)',          \n\
                            'gclass': 'C_WEBSOCKET',                \n\
                            'kw': {                                 \n\
                                'iamServer': true                   \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'raw-(^^__range__^^)',  \n\
                                    'gclass': 'C_TCP'               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        }," CLI_TREE "                                              \n\
    ]                                                               \n\
}                                                                   \n\
";

time_measure_t time_measure;

/***************************************************************************
 *  No log line carries a token of the test (they are made in the tester)
 ***************************************************************************/
int token_logged = 0;

static int token_log_write(void *v, int priority, const char *bf, size_t len)
{
    for(int i=0; i<13; i++) {
        if(test_tokens[i] && memmem(bf, len, test_tokens[i], strlen(test_tokens[i]))) {
            token_logged++;
        }
    }
    return 0;
}

/***************************************************************************
 *  HACK This function is executed on yunetas environment (mem, log, paths)
 *  BEFORE creating the yuno
 ***************************************************************************/
int result = 0;

static int register_yuno_and_more(void)
{
    int result = 0;

    /*--------------------*
     *  Register gclass
     *--------------------*/
    result += register_c_test_cli_operator();

    /*------------------------------*
     *  Start test
     *------------------------------*/
#ifdef OPERATOR_UNVERIFIED
    json_t *errors_list = json_array();
#else
    /*  Strict FIFO: one warning per request whose operator is refused  */
    json_t *errors_list = json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
        "msg", "Request of the server refused: its operator is not verified",   // another key
        "msg", "Request of the server refused: its operator is not verified",   // expired
        "msg", "Request of the server refused: its operator is not verified",   // another node
        "msg", "Request of the server refused: its operator is not verified",   // not a string
        "msg", "Request of the server refused: its operator is not verified",   // a disabled user
        "msg", "Request of the server refused: its operator is not verified",   // a login token
        "msg", "Request of the server refused: its operator is not verified",   // lives too long
        "msg", "Request of the server refused: its operator is not verified"    // no sub
    );
#endif
    set_expected_results(
        APP_NAME,
        errors_list,
        NULL,   // expected
        NULL,   // ignore_keys
        1       // verbose
    );

    MT_START_TIME(time_measure)

    return result;
}

/***************************************************************************
 *  HACK This function is executed on yunetas environment
 *  AFTER the yuno has stopped
 ***************************************************************************/
static void cleaning(void)
{
    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, APP_NAME)

    result += test_json(NULL);

    if(token_logged) {
        printf("%sERROR --> %s: %d lines%s\n",
            On_Red BWhite,
            "a token of an operator went to the log",
            token_logged,
            Color_Off
        );
        result += -1;
    }
    if(test_cli_operator_failed) {
        result += -1;
    }
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*------------------------------*
     *  Captura salida logger
     *------------------------------*/
    rmrdir(STORE);      // the authz store of a run before
    mkrdir(STORE, 02770);   // C_AUTHZ opens its treedb only in a dir that exists
    glog_init();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    gobj_log_register_handler(
        "testing",
        0,
        capture_log_write,
        0
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_WARNING, 0);

    gobj_log_register_handler(
        "token_log",
        0,
        token_log_write,
        0
    );
    gobj_log_add_handler("token_log", "token_log", LOG_OPT_ALL, 0);

    /*------------------------------------------------*
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    set_auto_kill_time(20);    // a crash-free hang must not hang the suite

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    yuneta_setup(
        NULL,       // persistent_attrs
        NULL,       // command_parser
        NULL,       // stats_parser
        NULL,       // authz_checker
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

    rmrdir(STORE);

    if(get_cur_system_memory()!=0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result<0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP_NAME);
    }
    return result<0?-1:0;
}
