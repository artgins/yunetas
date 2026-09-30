/****************************************************************************
 *          MAIN.C
 *
 *          Main of test_c_ievent_srv_identity_card
 *          Tests the identity cards a peer sends to C_IEVENT_SRV: what it
 *          refuses is logged as the peer's, a capped warning
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include "c_test_identity_card.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_c_ievent_srv_identity_card"
#define APP_DOC         "Test the identity cards a peer sends to C_IEVENT_SRV"

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
            'name': 'tester',                                       \n\
            'gclass': 'C_TEST_IDENTITY_CARD',                       \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false                                       \n\
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
                        'url': 'ws://127.0.0.1:7813',               \n\
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
            'name': 'cli_raw',                                      \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'cli_raw',                              \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'cli_raw',                      \n\
                            'gclass': 'C_WEBSOCKET',                \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'cli_raw',              \n\
                                    'gclass': 'C_TCP',              \n\
                                    'kw': {                         \n\
                                        'url': 'ws://127.0.0.1:7813', \n\
                                        'timeout_between_connections': 100 \n\
                                    }                               \n\
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
 *  Authentication without a C_AUTHZ: the user is the `jwt` of the card,
 *  and it may reach the `tester`.
 ***************************************************************************/
static json_t *test_authentication_parser(hgobj gobj_service, json_t *kw, hgobj src)
{
    const char *username = kw_get_str(gobj_service, kw, "jwt", "", 0);
    gobj_write_str_attr(src, "__username__", username);

    json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:{s:[]}}",
        "result", 0,
        "comment", "test authentication",
        "username", username,
        "services_roles",
            "tester"
    );
    KW_DECREF(kw)
    return jn_resp;
}

/***************************************************************************
 *  Each refusal: ONE line (a stack is written as more lines of the same
 *  log), a WARNING, capped, and without the jwt of the card
 ***************************************************************************/
typedef struct {
    const char *mark;
    int lines;
    int priority;
    size_t longest;
} peer_log_count_t;
peer_log_count_t peer_log_counts[] = {
    {"Identity card without its routing",               0, -1, 0},
    {"Identity card refused, dst_role NOT MATCH",       0, -1, 0},
    {"Identity card refused, dst_yuno NOT MATCH",       0, -1, 0},
    {"Identity card refused, without yuno role",        0, -1, 0},
    {"Identity card refused, without yuno service",     0, -1, 0},
    {"Identity card refused, dst_service NOT FOUND",    0, -1, 0},
    {"Identity card refused, its jwt is not a string",  0, -1, 0},
    {"Event before the identity card, channel closed",  0, -1, 0},
    {0, 0, 0, 0}
};

int jwt_logged = 0;     // lines that carry the jwt of a card: none

static int peer_log_write(void *v, int priority, const char *bf, size_t len)
{
    const char *jwt = "tester-jwt-credential";
    if(memmem(bf, len, jwt, strlen(jwt))) {
        jwt_logged++;
    }
    for(int i=0; peer_log_counts[i].mark; i++) {
        peer_log_count_t *c = &peer_log_counts[i];
        if(memmem(bf, len, c->mark, strlen(c->mark))) {
            c->lines++;
            c->priority = priority;
            if(len > c->longest) {
                c->longest = len;
            }
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
    result += register_c_test_identity_card();

    /*------------------------------*
     *  Start test
     *------------------------------*/
    set_expected_results(
        APP_NAME,
        /*  Strict FIFO of the warnings and errors: one warning per refused
         *  card, and no error  */
        json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
            "msg", "Identity card without its routing (__md_iev__ ievent stack), refused",
            "msg", "Identity card refused, dst_role NOT MATCH",
            "msg", "Identity card refused, dst_yuno NOT MATCH",
            "msg", "Identity card refused, without yuno role",
            "msg", "Identity card refused, without yuno service",
            "msg", "Identity card refused, dst_service NOT FOUND in this yuno",
            "msg", "Identity card refused, its jwt is not a string",
            "msg", "Event before the identity card, channel closed"
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

    for(int i=0; peer_log_counts[i].mark; i++) {
        peer_log_count_t *c = &peer_log_counts[i];
        if(c->lines != 1 || c->priority != LOG_WARNING || c->longest > 1500) {
            printf("%sERROR --> %s: '%s' lines %d (expected 1), priority %d (expected %d), longest %lu%s\n",
                On_Red BWhite,
                "a refused identity card is not ONE capped warning",
                c->mark,
                c->lines,
                c->priority,
                LOG_WARNING,
                (unsigned long)c->longest,
                Color_Off
            );
            result += -1;
        }
    }
    if(jwt_logged) {
        printf("%sERROR --> %s: %d lines%s\n",
            On_Red BWhite,
            "the jwt of a card was written to the log",
            jwt_logged,
            Color_Off
        );
        result += -1;
    }
    if(test_identity_card_failed) {
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
        "peer_log",
        0,
        peer_log_write,
        0
    );
    gobj_log_add_handler("peer_log", "peer_log", LOG_OPT_ALL, 0);

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
