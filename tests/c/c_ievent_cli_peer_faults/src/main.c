/****************************************************************************
 *          MAIN.C
 *
 *          Main of test_c_ievent_cli_peer_faults
 *          Tests what C_IEVENT_CLI does with the faults of its server: each
 *          is logged as the peer's, a warning with its names masked
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include "c_test_cli_peer.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_c_ievent_cli_peer_faults"
#define APP_DOC         "Test what C_IEVENT_CLI does with the faults of its server"

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
            'gclass': 'C_TEST_CLI_PEER',                            \n\
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
                        'url': 'ws://127.0.0.1:7887',               \n\
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
        },                                                          \n\
        {                                                           \n\
            'name': 'cli',                                          \n\
            'gclass': 'C_IEVENT_CLI',                               \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
                'jwt': 'tester',                                    \n\
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
                                    'url': 'ws://127.0.0.1:7887',   \n\
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
    ]                                                               \n\
}                                                                   \n\
";

time_measure_t time_measure;

/***************************************************************************
 *  Each fault of the server: ONE line (a stack is written as more lines of
 *  the same log), a WARNING, and without the credential of its names
 ***************************************************************************/
typedef struct {
    const char *mark;
    int expected;
    int lines;
    int priority;
} peer_log_count_t;
peer_log_count_t peer_log_counts[] = {
    {"event UNKNOWN in not-session state",          1, 0, -1},
    {"It's not my role",                            1, 0, -1},
    {"It's not my name",                            1, 0, -1},
    {"\"subscription event ignored, I'm client",    1, 0, -1},
    {"unsubscription event ignored, I'm client",    1, 0, -1},
    {"or out of its place, refused",                3, 0, -1},
    {"\"Service not found\"",                       2, 0, -1},
    {"service of the event not found",              1, 0, -1},
    {0, 0, 0, 0}
};

int secret_logged = 0;  // lines that carry a credential of the server: none

static int peer_log_write(void *v, int priority, const char *bf, size_t len)
{
    const char *secret = "tester-secret";
    if(memmem(bf, len, secret, strlen(secret))) {
        secret_logged++;
    }
    for(int i=0; peer_log_counts[i].mark && priority <= LOG_WARNING; i++) {
        peer_log_count_t *c = &peer_log_counts[i];
        if(memmem(bf, len, c->mark, strlen(c->mark))) {
            c->lines++;
            c->priority = priority;
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
    result += register_c_test_cli_peer();

    /*------------------------------*
     *  Start test
     *------------------------------*/
    set_expected_results(
        APP_NAME,
        /*  Strict FIFO of the warnings and errors: one warning per fault  */
        json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
            "msg", "event UNKNOWN in not-session state",
            "msg", "EV_ON_OPEN_ERROR: cannot open connection",  // the client's own: no session yet
            "msg", "It's not my role",
            "msg", "It's not my name",
            "msg", "subscription event ignored, I'm client",
            "msg", "unsubscription event ignored, I'm client",
            "msg", "event of the transport itself, or out of its place, refused",   // EV_ON_OPEN
            "msg", "event of the transport itself, or out of its place, refused",   // EV_ON_CLOSE
            "msg", "event of the transport itself, or out of its place, refused",   // the ACK in session
            "msg", "Service not found",     // EV_MT_STATS
            "msg", "Frame of an event that is not public, refused", // its answer, read by the raw server
            "msg", "Service not found",     // EV_MT_COMMAND
            "msg", "Frame of an event that is not public, refused", // its answer, read by the raw server
            "msg", "service of the event not found, published to the subscribers"
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
        if(c->lines != c->expected || c->priority != LOG_WARNING) {
            printf("%sERROR --> %s: '%s' lines %d (expected %d), priority %d (expected %d)%s\n",
                On_Red BWhite,
                "a fault of the server is not ONE warning",
                c->mark,
                c->lines,
                c->expected,
                c->priority,
                LOG_WARNING,
                Color_Off
            );
            result += -1;
        }
    }
    if(secret_logged) {
        printf("%sERROR --> %s: %d lines%s\n",
            On_Red BWhite,
            "a credential in a name the server wrote went to the log",
            secret_logged,
            Color_Off
        );
        result += -1;
    }
    if(test_cli_peer_failed) {
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
