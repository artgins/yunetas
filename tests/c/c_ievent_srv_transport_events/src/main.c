/****************************************************************************
 *          MAIN.C
 *
 *          Main of test_c_ievent_srv_transport_events
 *          Tests that a peer of a C_IEVENT_SRV cannot send the events of the
 *          transport itself (EV_ON_OPEN, EV_ON_CLOSE, EV_ON_MESSAGE,
 *          EV_ON_IEV_MESSAGE, EV_ON_ID, EV_ON_ID_NAK) nor the events a treedb
 *          publishes (EV_TREEDB_NODE_*), and that an event of a peer its
 *          subscriber does not take is refused at the gate: the gate
 *          publishes none of them, one warning for each, and a legit event
 *          still goes
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include "c_test_transport_events.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_c_ievent_srv_transport_events"
#define APP_DOC         "Test that a peer of C_IEVENT_SRV cannot send the events of the transport"
#define TEST_PORT       "7898"

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
            'gclass': 'C_TEST_TRANSPORT_EVENTS',                       \n\
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
                                        'url': 'ws://127.0.0.1:" TEST_PORT "', \n\
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
 *  and it may reach the `tester`
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
 *  The refusals: ONE line, a WARNING, for the six events of the transport,
 *  and ONE for the five of a treedb
 ***************************************************************************/
#define REFUSAL_MARK "event of the transport itself sent by a peer, refused"
#define TREEDB_REFUSAL_MARK "event of a treedb sent by a peer, refused"
#define NOT_TAKEN_MARK "event of a peer the yuno does not take now, refused"
#define NOT_SAID_MARK "Events of a peer refused, not said"
#define NOT_SAID_EXPECTED "\"not_said\": 49"
int refusal_lines = 0;
int refusal_priority = -1;
int treedb_refusal_lines = 0;
int treedb_refusal_priority = -1;
int not_taken_lines = 0;
int not_taken_priority = -1;
int not_said_lines = 0;
int not_said_right = 0;

static int refusal_log_write(void *v, int priority, const char *bf, size_t len)
{
    if(memmem(bf, len, REFUSAL_MARK, strlen(REFUSAL_MARK))) {
        refusal_lines++;
        refusal_priority = priority;
    }
    if(memmem(bf, len, TREEDB_REFUSAL_MARK, strlen(TREEDB_REFUSAL_MARK))) {
        treedb_refusal_lines++;
        treedb_refusal_priority = priority;
    }
    if(memmem(bf, len, NOT_TAKEN_MARK, strlen(NOT_TAKEN_MARK))) {
        not_taken_lines++;
        not_taken_priority = priority;
    }
    if(memmem(bf, len, NOT_SAID_MARK, strlen(NOT_SAID_MARK))) {
        not_said_lines++;
        if(memmem(bf, len, NOT_SAID_EXPECTED, strlen(NOT_SAID_EXPECTED))) {
            not_said_right++;
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
    result += register_c_test_transport_events();

    /*------------------------------*
     *  Start test
     *------------------------------*/
    set_expected_results(
        APP_NAME,
        /*  Strict FIFO of the warnings and errors: one refusal for the six,
         *  one for the five, one for the frames the tester does not take
         *  (no "Event NOT DEFINED in state")  */
        json_pack("[{s:s}, {s:s}, {s:s}]",
            "msg", REFUSAL_MARK,
            "msg", TREEDB_REFUSAL_MARK,
            "msg", NOT_TAKEN_MARK
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

    if(refusal_lines != 1 || refusal_priority != LOG_WARNING) {
        printf("%sERROR --> %s: lines %d (expected 1), priority %d (expected %d)%s\n",
            On_Red BWhite,
            "the refusal is not ONE warning",
            refusal_lines,
            refusal_priority,
            LOG_WARNING,
            Color_Off
        );
        result += -1;
    }
    if(treedb_refusal_lines != 1 || treedb_refusal_priority != LOG_WARNING) {
        printf("%sERROR --> %s: lines %d (expected 1), priority %d (expected %d)%s\n",
            On_Red BWhite,
            "the refusal of the treedb events is not ONE warning",
            treedb_refusal_lines,
            treedb_refusal_priority,
            LOG_WARNING,
            Color_Off
        );
        result += -1;
    }
    if(not_taken_lines != 1 || not_taken_priority != LOG_WARNING) {
        printf("%sERROR --> %s: lines %d (expected 1), priority %d (expected %d)%s\n",
            On_Red BWhite,
            "the refusal of the events not taken is not ONE warning",
            not_taken_lines,
            not_taken_priority,
            LOG_WARNING,
            Color_Off
        );
        result += -1;
    }
    if(not_said_lines != 1 || not_said_right != 1) {
        printf("%sERROR --> %s: lines %d (expected 1), with not_said 49: %d%s\n",
            On_Red BWhite,
            "the end of the refusals is not ONE line with the ones not said",
            not_said_lines,
            not_said_right,
            Color_Off
        );
        result += -1;
    }
    if(test_transport_events_failed) {
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
        "refusal_log",
        0,
        refusal_log_write,
        0
    );
    gobj_log_add_handler("refusal_log", "refusal_log", LOG_OPT_ALL, 0);

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
