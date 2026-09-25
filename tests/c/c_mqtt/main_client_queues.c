/****************************************************************************
 *          MAIN_CLIENT_QUEUES.C
 *
 *          Self-contained MQTT CLIENT test: the persistent queues of a
 *          C_PROT_MQTT2 client against a RAW broker. An incoming QoS 2
 *          message sent again with DUP=1 is delivered once, and an outgoing
 *          message that expires while queued gives its slot to the next
 *          one. See c_client_queues.c.
 *
 *          A raw broker (C_TCP_S + C_CHANNEL + C_PROT_RAW, port 18113) of
 *          the driver writes the packets by hand, and the mqtt client
 *          (C_CHANNEL + C_PROT_MQTT2 + C_TCP) connects to it.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include <c_prot_mqtt2.h>
#include "c_client_queues.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_mqtt_" "client_queues"
#define APP_DOC         "The persistent queues of an MQTT client"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

/*
 *  Dedicated work_dir, wiped at start
 */
#define WORK_DIR        "/tmp/test_mqtt_client_queues"
#define MQTT_TEST_PORT  "18113"

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
        'work_dir': '"WORK_DIR"',                                   \n\
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
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': '__raw_broker__',                               \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'raw_port',                             \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://127.0.0.1:"MQTT_TEST_PORT"',  \n\
                        'backlog': 4,                               \n\
                        'use_dups': 0                               \n\
                    }                                               \n\
                }                                                   \n\
            ],                                                      \n\
            '[^^children^^]': {                                     \n\
                '__range__': [1, 1],                                \n\
                '__vars__': {                                       \n\
                },                                                  \n\
                '__content__': {                                    \n\
                    'name': 'raw-(^^__range__^^)',                  \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'raw-(^^__range__^^)',          \n\
                            'gclass': 'C_PROT_RAW',                 \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'gclass': 'C_TCP'               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__output_side__',                              \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'mqtt_client',                          \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'mqtt_client',                  \n\
                            'gclass': 'C_PROT_MQTT2',               \n\
                            'kw': {                                 \n\
                                'iamServer': false,                 \n\
                                'mqtt_client_id': 'queues_client',  \n\
                                'mqtt_protocol': 'mqttv311'         \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'mqtt_client',          \n\
                                    'gclass': 'C_TCP',              \n\
                                    'kw': {                         \n\
                                        'url': 'tcp://127.0.0.1:"MQTT_TEST_PORT"' \n\
                                    }                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'c_client_queues',                              \n\
            'gclass': 'C_CLIENT_QUEUES',                            \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            }                                                       \n\
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
     *  Register gclasses
     *--------------------*/
    res += register_c_prot_mqtt2();
    res += register_c_client_queues();

    /*------------------------------------------------*
     *  Suppress noisy traces
     *------------------------------------------------*/
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER),  "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    /*------------------------------------------------*
     *  Safety: kill the yuno after 30 s if stuck
     *------------------------------------------------*/
    set_auto_kill_time(30);

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
     *  Fresh work_dir every run
     *------------------------------------------------*/
    rmrdir(WORK_DIR);

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
        test_authz_checker, // authz_checker: allow all in self-contained test
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

    if(result < 0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP_NAME);
    }
    return result < 0 ? -1 : 0;
}
