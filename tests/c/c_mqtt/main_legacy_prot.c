/****************************************************************************
 *          MAIN_LEGACY_PROT.C
 *
 *          C_PROT_MQTT (deprecated) as a server, driven by hand on a fake
 *          transport: no broker, no socket. See c_legacy_prot.c. The log
 *          is scanned too: the password of a malformed CONNECT must not be
 *          in it.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "test_work_dir.h"
#include <c_prot_mqtt.h>
#include "c_fake_transport.h"
#include "c_legacy_prot.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_mqtt_" "legacy_prot"
#define APP_DOC         "C_PROT_MQTT as a server, on a fake transport"

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
            'name': 'c_legacy_prot',                                \n\
            'gclass': 'C_LEGACY_PROT',                              \n\
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
extern const char *legacy_password_needle;
extern const char legacy_password_fill;

PRIVATE int password_shown = 0;     // lines with the password

PRIVATE int scan_log_write(void *v, int priority, const char *bf, size_t len)
{
    /*
     *  A dump splits the password in rows of 16 bytes: look for a row of it
     */
    char password_row[17];
    memset(password_row, legacy_password_fill, 16);
    password_row[16] = 0;
    if(memmem(bf, len, legacy_password_needle, strlen(legacy_password_needle)) ||
        memmem(bf, len, password_row, 16)
    ) {
        password_shown++;
    }
    return 0;
}

/***************************************************************************
 *  What the scanner saw: no password
 ***************************************************************************/
PRIVATE int check_scanned_log(void)
{
    if(password_shown != 0) {
        printf("%sERROR --> %s: %d%s\n", On_Red BWhite,
            "the password is in the log", password_shown, Color_Off);
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
    res += register_c_prot_mqtt();
    res += register_c_fake_transport();
    res += register_c_legacy_prot();

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
        json_pack("[{s:s}]",
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
     *  Scan every line, traces included: the password
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
    if(!test_work_dir_create("test_mqtt_legacy_prot")) {
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
