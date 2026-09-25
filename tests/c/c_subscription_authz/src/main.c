/****************************************************************************
 *          MAIN.C
 *
 *          Main of test_c_subscription_authz
 *          Tests the authorization of an external subscription
 *          (EVF_AUTHZ_SUBSCRIBE, gated by the yuno's enable_subscription_authz)
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <malloc.h>
#include <yunetas.h>
#include "c_test_subs_authz.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_c_subscription_authz"
#define APP_DOC         "Test the authorization of an external subscription"

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
            'name': 'subscriber',                                   \n\
            'gclass': 'C_TEST_SUBS_AUTHZ',                          \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'publisher',                                    \n\
            'gclass': 'C_TEST_PUB_AUTHZ',                           \n\
            'autostart': true,                                      \n\
            'autoplay': false                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'treedb_host',                                  \n\
            'gclass': 'C_TEST_TREEDB_HOST',                         \n\
            'autostart': true,                                      \n\
            'autoplay': false                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__input_side__',                               \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': true,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'server_port',                          \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'ws://127.0.0.1:7794',              \n\
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
                '__range__': [1,3],                                 \n\
                '__vars__': {                                       \n\
                },                                                  \n\
                '__content__': {                                    \n\
                    'name': 'input-(^^__range__^^)',                \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'input-(^^__range__^^)',        \n\
                            'gclass': 'C_IEVENT_SRV',               \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'input-(^^__range__^^)', \n\
                                    'gclass': 'C_WEBSOCKET',        \n\
                                    'kw': {                         \n\
                                        'iamServer': true           \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                    'name': 'input-(^^__range__^^)', \n\
                                            'gclass': 'C_TCP'       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'cli_off',                                        \n\
            'gclass': 'C_IEVENT_CLI',                               \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
                'jwt': 'nobody',                                     \n\
                'remote_yuno_name': '',                             \n\
                'remote_yuno_role': '"APP_NAME"',                 \n\
                'remote_yuno_service': 'publisher'                  \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'cli_off_gate',                           \n\
                    'gclass': 'C_IOGATE',                           \n\
                    'as_service': true,                             \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'cli_off',                        \n\
                            'gclass': 'C_CHANNEL',                  \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'cli_off',                \n\
                                    'gclass': 'C_WEBSOCKET',        \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'cli_off',        \n\
                                            'gclass': 'C_TCP',      \n\
                                            'kw': {                 \n\
                                    'url': 'ws://127.0.0.1:7794'   \n\
                                            }                       \n\
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
            'name': 'cli_nobody',                                        \n\
            'gclass': 'C_IEVENT_CLI',                               \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
                'jwt': 'nobody',                                     \n\
                'remote_yuno_name': '',                             \n\
                'remote_yuno_role': '"APP_NAME"',                 \n\
                'remote_yuno_service': 'publisher'                  \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'cli_nobody_gate',                           \n\
                    'gclass': 'C_IOGATE',                           \n\
                    'as_service': true,                             \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'cli_nobody',                        \n\
                            'gclass': 'C_CHANNEL',                  \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'cli_nobody',                \n\
                                    'gclass': 'C_WEBSOCKET',        \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'cli_nobody',        \n\
                                            'gclass': 'C_TCP',      \n\
                                            'kw': {                 \n\
                                    'url': 'ws://127.0.0.1:7794'   \n\
                                            }                       \n\
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
            'name': 'cli_reader',                                        \n\
            'gclass': 'C_IEVENT_CLI',                               \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
                'jwt': 'reader',                                     \n\
                'remote_yuno_name': '',                             \n\
                'remote_yuno_role': '"APP_NAME"',                 \n\
                'remote_yuno_service': 'publisher'                  \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'cli_reader_gate',                           \n\
                    'gclass': 'C_IOGATE',                           \n\
                    'as_service': true,                             \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'cli_reader',                        \n\
                            'gclass': 'C_CHANNEL',                  \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'cli_reader',                \n\
                                    'gclass': 'C_WEBSOCKET',        \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'cli_reader',        \n\
                                            'gclass': 'C_TCP',      \n\
                                            'kw': {                 \n\
                                    'url': 'ws://127.0.0.1:7794'   \n\
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
    ]                                                               \n\
}                                                                   \n\
";

time_measure_t time_measure;

/***************************************************************************
 *  Authentication without a C_AUTHZ: the user is the `jwt` of the identity
 *  card, and it may reach the `publisher` and `treedb_subs_authz` services. What C_AUTHZ leaves on
 *  the gate is repeated: `__username__` on the C_IEVENT_SRV (src).
 ***************************************************************************/
static json_t *test_authentication_parser(hgobj gobj_service, json_t *kw, hgobj src)
{
    const char *username = kw_get_str(gobj_service, kw, "jwt", "", 0);
    gobj_write_str_attr(src, "__username__", username);

    json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:{s:[], s:[]}}",
        "result", 0,
        "comment", "test authentication",
        "username", username,
        "services_roles",
            "publisher",
            "treedb_subs_authz"
    );
    KW_DECREF(kw)
    return jn_resp;
}

/***************************************************************************
 *  `reader` holds `read`; nobody else holds anything. Counts what it is
 *  asked, so the test sees the permission name the gate resolved.
 ***************************************************************************/
static BOOL test_authz_checker(hgobj gobj, const char *authz, json_t *kw, hgobj src)
{
    const char *username = kw_get_str(gobj, kw, "__username__", "", 0);
    if(strcmp(authz, "read")==0) {
        authz_asked_read++;
    } else {
        authz_asked_other++;
    }
    BOOL allow = (strcmp(username, "reader")==0 && strcmp(authz, "read")==0)? TRUE: FALSE;

    KW_DECREF(kw)
    return allow;
}

/***************************************************************************
 *  Reads the `authzs` trace: each line of a check must carry the kw it
 *  checked (its `__username__`). Up to 7.25.4 the trace printed the kw after
 *  the checker had freed it, and the line came out without it.
 ***************************************************************************/
static int authzs_traces_with_kw = 0;
static int authzs_traces_without_kw = 0;

static int authzs_trace_write(void *v, int priority, const char *bf, size_t len)
{
    const char *mark = "authzs \xF0\x9F\x94\x91";   // "authzs 🔑"
    if(memmem(bf, len, mark, strlen(mark))) {
        const char *key = "\"__username__\"";
        if(memmem(bf, len, key, strlen(key))) {
            authzs_traces_with_kw++;
        } else {
            authzs_traces_without_kw++;
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
    result += register_c_test_subs_authz();

    /*------------------------------*
     *  Start test
     *------------------------------*/
    set_expected_results(
        APP_NAME,
        /*  Strict FIFO of the warnings and errors (the capture handler of
         *  main() takes nothing below a warning): the two refusals of
         *  `nobody` with the gate on, EV_TEST_FEED of `publisher` and
         *  EV_TREEDB_NODE_UPDATED of the C_NODE. A wrong result is an
         *  error, which is not in this list.  */
        json_pack("[{s:s}, {s:s}]",
            "msg", "No permission to subscribe event",
            "msg", "No permission to subscribe event"
        ),
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

    /*
     *  Four checks with the trace on: two subscriptions of `nobody`, two of
     *  `reader` (and those of the tranger feed)
     */
    if(authzs_traces_without_kw > 0 || authzs_traces_with_kw < 4) {
        printf("%sERROR --> %s: with kw %d, without kw %d%s\n",
            On_Red BWhite,
            "the authzs trace lost the kw it checked",
            authzs_traces_with_kw,
            authzs_traces_without_kw,
            Color_Off
        );
        result += -1;
    }
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*
     *  glibc fills every freed block: a use-after-free reads garbage and
     *  crashes instead of passing unseen (the `authzs` trace up to 7.25.4)
     */
    mallopt(M_PERTURB, 0xa5);

    /*------------------------------*
     *  Captura salida logger
     *------------------------------*/
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
        "authzs_trace",
        0,
        authzs_trace_write,
        0
    );
    gobj_log_add_handler("authzs_trace", "authzs_trace", LOG_OPT_ALL, 0);

    /*------------------------------------------------*
     *      To check memory loss
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
        test_authentication_parser,
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
