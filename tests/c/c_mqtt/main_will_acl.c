/****************************************************************************
 *          MAIN_WILL_ACL.C
 *
 *          Self-contained MQTT broker test: the will of a client that goes
 *          away with no DISCONNECT obeys the publish ACL of the client's
 *          group -- refused to a topic the group may not publish to (a
 *          WARNING, nothing delivered), delivered to one it may; and a
 *          retained message stored by the broker reaches a later
 *          subscriber, with no leak of its payload. See c_will_acl.c.
 *
 *          Embedded C_AUTHZ + C_MQTT_BROKER (enable_acl on), an input gate
 *          on port 18118, and raw MQTT clients (C_TCPs of the driver) that
 *          write the packets by hand.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "test_work_dir.h"
#include <c_mqtt_broker.h>
#include <c_prot_mqtt2.h>
#include "c_will_acl.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_mqtt_" "will_acl"
#define APP_DOC         "The will of a client under the broker's ACL"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

/*
 *  A work_dir of this run (test_work_dir.c), removed at the
 *  end: fresh every run, and two runs at once do not share it
 */
#define WORK_DIR        "@WORK_DIR@"   // placeholder: the dir of this run (test_work_dir.c)
#define MQTT_TEST_PORT  "18118"

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
        'Authz.allow_anonymous_in_localhost': true,                 \n\
        '__input_side__.__json_config_variables__': {               \n\
            '__input_url__':  'mqtt://127.0.0.1:"MQTT_TEST_PORT"',  \n\
            '__input_host__': '127.0.0.1',                          \n\
            '__input_port__': '"MQTT_TEST_PORT"'                    \n\
        }                                                           \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'authz',                                        \n\
            'gclass': 'C_AUTHZ',                                    \n\
            'priority': 0,                                          \n\
            'default_service': false,                               \n\
            'autostart': true,                                      \n\
            'autoplay': true                                        \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'mqtt_broker',                                  \n\
            'gclass': 'C_MQTT_BROKER',                              \n\
            'default_service': false,                               \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'enable_new_clients': false,                        \n\
                'enable_acl': true                                  \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__input_side__',                               \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': true,                                      \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'server_port',                          \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': '(^^__input_url__^^)',               \n\
                        'backlog': 4,                               \n\
                        'use_dups': 0                               \n\
                    }                                               \n\
                }                                                   \n\
            ],                                                      \n\
            '[^^children^^]': {                                     \n\
                '__range__': [1, 4],                                \n\
                '__vars__': {                                       \n\
                },                                                  \n\
                '__content__': {                                    \n\
                    'name': '(^^__input_port__^^)-(^^__range__^^)', \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': '(^^__input_port__^^)-(^^__range__^^)', \n\
                            'gclass': 'C_PROT_MQTT2',               \n\
                            'kw': {                                 \n\
                                'iamServer': true                   \n\
                            },                                      \n\
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
            'name': '__top_side__',                                 \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'c_will_acl',                                   \n\
            'gclass': 'C_WILL_ACL',                                 \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";
PRIVATE char variable_config_run[sizeof(variable_config) + PATH_MAX]; // WORK_DIR replaced by the dir of this run

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
    res += register_c_mqtt_broker();
    res += register_c_prot_mqtt2();
    res += register_c_will_acl();

    /*------------------------------------------------*
     *  Suppress noisy traces
     *------------------------------------------------*/
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER),  "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    /*------------------------------------------------*
     *  Safety: kill the yuno after 10 s if stuck
     *------------------------------------------------*/
    set_auto_kill_time(10);

    /*------------------------------*
     *  Capture only errors
     *------------------------------*/
    set_expected_results(
        APP_NAME,
        json_pack("[{s:s},{s:s}]",
            "msg", "No authz db, authz only to local access",   // C_AUTHZ of the test, without a db
            "msg", "Will message refused by the ACL: not published"   // the will to a refused topic
        ),
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
     *  Capture WARNING+ logs for test verification.
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
     *  Fresh treedb store every run
     *------------------------------------------------*/
    if(!test_work_dir_create("test_mqtt_will_acl")) {
        return -1;  // Error already said
    }
    if(test_work_dir_config(variable_config_run, sizeof(variable_config_run), variable_config, WORK_DIR) < 0) {
        test_work_dir_remove();
        return -1;  // Error already said
    }

    /*------------------------------------------------*
     *      Memory leak check
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config_run);
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
        variable_config_run,
        register_yuno_and_more,
        cleaning
    );

    test_work_dir_remove();

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
