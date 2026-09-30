/****************************************************************************
 *          MAIN_WRONG_ACK.C
 *
 *          Self-contained MQTT broker test: an ack of the wrong QoS from a
 *          client (a PUBREL of a QoS 1 message, a PUBREC of a QoS 1 message)
 *          is said as a WARNING of the peer, never an ERROR; the CONNECT
 *          decode trace never prints the password, nor reads past the
 *          username; the dump of a malformed CONNECT never carries the
 *          password; and auth_data is shown masked. See c_wrong_ack.c.
 *
 *          Embedded C_AUTHZ + C_MQTT_BROKER, an input gate on port 18117,
 *          and a raw MQTT client (a C_TCP of the driver) that writes the
 *          packets by hand.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include <c_mqtt_broker.h>
#include <c_prot_mqtt2.h>
#include "c_wrong_ack.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_mqtt_" "wrong_ack"
#define APP_DOC         "Acks of the wrong QoS, CONNECT trace and auth_data of the broker"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

/*
 *  Dedicated work_dir, wiped at start so the broker's sessions and
 *  queues are fresh every run
 */
#define WORK_DIR        "/tmp/test_mqtt_wrong_ack"
#define MQTT_TEST_PORT  "18117"

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
                'enable_new_clients': true                          \n\
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
                '__range__': [1, 2],                                \n\
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
                                'iamServer': true,                  \n\
                                'max_inflight_messages': 20         \n\
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
            'name': 'c_wrong_ack',                                   \n\
            'gclass': 'C_WRONG_ACK',                                 \n\
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

/***************************************************************************
 *  Log scanner: every line of the log, traces included
 ***************************************************************************/
extern const char *wrong_ack_username;
extern const char *wrong_ack_password_needle;
extern const char wrong_ack_password_fill;

PRIVATE int log_errors = 0;         // ERROR or worse, expected or not
PRIVATE int password_shown = 0;     // lines with the password
PRIVATE int connect_traces = 0;     // CONNECT decode traces
PRIVATE int username_exact = 0;     // lines with "username '<username>'"

PRIVATE int scan_log_write(void *v, int priority, const char *bf, size_t len)
{
    char username_quoted[NAME_MAX];
    snprintf(username_quoted, sizeof(username_quoted), "username '%s'", wrong_ack_username);

    if(priority <= LOG_ERR) {
        log_errors++;
    }
    /*
     *  A dump splits the password in rows of 16 bytes: look for a row of it
     */
    char password_row[17];
    memset(password_row, wrong_ack_password_fill, 16);
    password_row[16] = 0;
    if(memmem(bf, len, wrong_ack_password_needle, strlen(wrong_ack_password_needle)) ||
        memmem(bf, len, password_row, 16)
    ) {
        password_shown++;
    }
    if(memmem(bf, len, "CONNECT\n", strlen("CONNECT\n"))) {
        connect_traces++;
        if(memmem(bf, len, username_quoted, strlen(username_quoted))) {
            username_exact++;
        }
    }
    return 0;
}

/***************************************************************************
 *  What the scanner saw: no ERROR, no password, the username as it is
 ***************************************************************************/
PRIVATE int check_scanned_log(void)
{
    int ret = 0;
    if(log_errors != 0) {
        printf("%sERROR --> %s: %d%s\n", On_Red BWhite,
            "the log has ERRORs, a peer's fault must be a WARNING", log_errors, Color_Off);
        ret = -1;
    }
    if(connect_traces == 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite,
            "no CONNECT decode trace was seen", Color_Off);
        ret = -1;
    }
    if(password_shown != 0) {
        printf("%sERROR --> %s: %d%s\n", On_Red BWhite,
            "the password is in the log", password_shown, Color_Off);
        ret = -1;
    }
    if(username_exact != connect_traces) {
        printf("%sERROR --> %s%s\n", On_Red BWhite,
            "the CONNECT trace does not print the username as it is", Color_Off);
        ret = -1;
    }
    return ret;
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
    res += register_c_wrong_ack();

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
        json_pack("[{s:s},{s:s},{s:s},{s:s}]",
            "msg", "No authz db, authz only to local access",   // C_AUTHZ of the test, without a db
            "msg", "Message not found",     // PUBREL of the QoS 1 message, a WARNING
            "msg", "QoS mismatch",          // PUBREC of the QoS 1 message, a WARNING
            "msg", "Mqtt: too much data"    // the CONNECT with one byte too many
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
    result += check_scanned_log();
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

    /*
     *  Scan every line, traces included: the password, and the errors
     */
    gobj_log_register_handler(
        "scan_log",         // handler_name
        0,                  // close_fn
        scan_log_write,     // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("scan_log", "scan_log", LOG_OPT_ALL, 0);

    /*------------------------------------------------*
     *  Fresh treedb store every run
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
