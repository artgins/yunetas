/****************************************************************************
 *          MAIN_CLIENT_PUBREC.C
 *
 *          C_PROT_MQTT2 as a CLIENT, driven by hand on a fake transport: no
 *          broker, no socket. The PUBRECs it receives from the "broker". See
 *          c_client_pubrec.c. The log is scanned too: the "QoS mismatch"
 *          WARNING must name the client.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "test_work_dir.h"
#include <c_prot_mqtt2.h>
#include "c_fake_transport.h"
#include "c_client_pubrec.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_mqtt_" "client_pubrec"
#define APP_DOC         "PUBRECs received by C_PROT_MQTT2 as a client, on a fake transport"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#define WORK_DIR        "@WORK_DIR@"   // placeholder: the dir of this run (test_work_dir.c)

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
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'c_client_pubrec',                              \n\
            'gclass': 'C_CLIENT_PUBREC',                            \n\
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

/***************************************************************************
 *  Log scanner: every line of the log, traces included
 ***************************************************************************/
extern const char *client_pubrec_client_id;

PRIVATE int mismatch_lines = 0;             // lines of the "QoS mismatch" WARNING
PRIVATE int mismatch_with_client_id = 0;    // ... that name the client

PRIVATE int scan_log_write(void *v, int priority, const char *bf, size_t len)
{
    char client_id_field[NAME_MAX];
    snprintf(client_id_field, sizeof(client_id_field),
        "\"client_id\": \"%s\"", client_pubrec_client_id
    );

    if(memmem(bf, len, "\"QoS mismatch\"", strlen("\"QoS mismatch\""))) {
        mismatch_lines++;
        if(memmem(bf, len, client_id_field, strlen(client_id_field))) {
            mismatch_with_client_id++;
        }
    }
    return 0;
}

/***************************************************************************
 *  What the scanner saw: the "QoS mismatch" WARNING names the client
 ***************************************************************************/
PRIVATE int check_scanned_log(void)
{
    if(mismatch_lines == 0 || mismatch_with_client_id != mismatch_lines) {
        printf("%sERROR --> %s: %d of %d%s\n", On_Red BWhite,
            "the QoS mismatch WARNING does not name the client",
            mismatch_with_client_id, mismatch_lines, Color_Off);
        return -1;
    }
    return 0;
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
    res += register_c_fake_transport();
    res += register_c_client_pubrec();

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
            "msg", "Mqtt: Received PUBREC for an unknown packet ID",    // the peer's, a WARNING
            "msg", "QoS mismatch"           // PUBREC of the QoS 1 message, a WARNING
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
     *  Scan every line: the QoS mismatch WARNING
     */
    gobj_log_register_handler(
        "scan_log",         // handler_name
        0,                  // close_fn
        scan_log_write,     // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("scan_log", "scan_log", LOG_OPT_ALL, 0);

    /*------------------------------------------------*
     *  Fresh work_dir every run
     *------------------------------------------------*/
    if(!test_work_dir_create("test_mqtt_client_pubrec")) {
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
