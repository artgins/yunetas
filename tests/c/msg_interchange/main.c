/****************************************************************************
 *          MAIN.C
 *
 *          Generic test runner for JSON-driven message interchange tests.
 *
 *          Usage: test_msg_interchange <test_file.json>
 *
 *          The JSON test file defines:
 *            - test_name: identifier
 *            - timeout: auto-kill timeout in seconds (default 10)
 *            - variable_config: yuno service configuration
 *            - script: array of actions to execute
 *            - expected_errors: array of expected error log messages
 *            - expected_event_trace: array of events to verify in order
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "c_test_runner.h"

/*
 *  Conditionally include module headers
 */
#ifdef CONFIG_MODULE_MQTT
#include <c_mqtt_broker.h>
#include <c_prot_mqtt2.h>
#endif

#ifdef CONFIG_MODULE_TEST
#include <c_pepon.h>
#include <c_teston.h>
#endif

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

/***************************************************************************
 *              Data
 ***************************************************************************/
PRIVATE json_t *test_definition = NULL;
PRIVATE const char *test_name = "test_msg_interchange";
PRIVATE char fixed_config_buf[1024];
PRIVATE char *variable_config_str = NULL;

time_measure_t time_measure;
int result = 0;

/***************************************************************************
 *  Authz checker: allow everything in self-contained tests
 ***************************************************************************/
PRIVATE BOOL test_authz_checker(hgobj gobj, const char *authz, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return TRUE;
}

/***************************************************************************
 *  Inject C_TEST_RUNNER as default_service into the variable_config
 ***************************************************************************/
PRIVATE int inject_test_runner_service(json_t *variable_config)
{
    json_t *services = json_object_get(variable_config, "services");
    if(!services) {
        services = json_array();
        json_object_set_new(variable_config, "services", services);
    }

    // Get script and expected_event_trace from test definition
    json_t *script = json_object_get(test_definition, "script");
    json_t *expected_trace = json_object_get(test_definition, "expected_event_trace");

    // Build the test_runner service with script and expected_trace as kw
    json_t *runner_kw = json_object();
    if(script) {
        json_object_set(runner_kw, "script", script); // borrowed ref, set adds ref
    }
    if(expected_trace) {
        json_object_set(runner_kw, "expected_event_trace", expected_trace);
    }

    json_t *runner_service = json_pack("{s:s, s:s, s:b, s:b, s:b, s:o}",
        "name",             "test_runner",
        "gclass",           "C_TEST_RUNNER",
        "default_service",  1,
        "autostart",        1,
        "autoplay",         0,
        "kw",               runner_kw
    );

    // Insert at beginning so it becomes the default service
    json_array_insert(services, 0, runner_service);
    JSON_DECREF(runner_service)

    return 0;
}

/***************************************************************************
 *  Register gclasses
 ***************************************************************************/
static int register_yuno_and_more(void)
{
    int res = 0;

    /*--------------------*
     *  Register gclasses
     *--------------------*/
    res += register_c_test_runner();

#ifdef CONFIG_MODULE_MQTT
    res += register_c_mqtt_broker();
    res += register_c_prot_mqtt2();
#endif

#ifdef CONFIG_MODULE_TEST
    res += register_c_pepon();
    res += register_c_teston();
#endif

    /*------------------------------------------------*
     *  Suppress noisy traces
     *------------------------------------------------*/
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER), "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    /*------------------------------------------------*
     *  Safety: auto-kill timeout from JSON
     *------------------------------------------------*/
    int timeout = (int)json_integer_value(json_object_get(test_definition, "timeout"));
    if(timeout <= 0) {
        timeout = 10;
    }
    set_auto_kill_time(timeout);

    /*------------------------------*
     *  Expected errors from JSON
     *------------------------------*/
    json_t *expected_errors = json_deep_copy(json_object_get(test_definition, "expected_errors"));
    if(!expected_errors) {
        expected_errors = json_array();
    }

    set_expected_results(
        test_name,
        expected_errors,
        NULL,           // no JSON comparison
        NULL,           // no ignore_keys
        TRUE            // verbose
    );

    MT_START_TIME(time_measure)

    return res;
}

/***************************************************************************
 *  Cleanup
 ***************************************************************************/
static void cleaning(void)
{
    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test_name)

    result += test_json(NULL);  // check captured error log
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*------------------------------*
     *  Check arguments
     *------------------------------*/
    if(argc < 2) {
        fprintf(stderr, "Usage: %s <test_file.json>\n", argv[0]);
        return -1;
    }

    const char *test_file = argv[1];

    /*------------------------------*
     *  Load JSON test definition
     *------------------------------*/
    json_error_t error;
    test_definition = json_load_file(test_file, 0, &error);
    if(!test_definition) {
        fprintf(stderr, "ERROR: Cannot load test file '%s': %s (line %d)\n",
            test_file, error.text, error.line);
        return -1;
    }

    /*------------------------------*
     *  Extract test metadata
     *------------------------------*/
    const char *name = json_string_value(json_object_get(test_definition, "test_name"));
    if(name && strlen(name) > 0) {
        test_name = name;
    }

    /*------------------------------*
     *  Build fixed_config from test_name
     *------------------------------*/
    snprintf(fixed_config_buf, sizeof(fixed_config_buf),
        "{"
        "    \"yuno\": {"
        "        \"yuno_role\": \"%s\","
        "        \"tags\": [\"test\", \"yunetas\", \"msg_interchange\"]"
        "    }"
        "}",
        test_name
    );

    /*------------------------------*
     *  Build variable_config
     *------------------------------*/
    json_t *var_config = json_deep_copy(json_object_get(test_definition, "variable_config"));
    if(!var_config) {
        fprintf(stderr, "ERROR: Test file '%s' missing 'variable_config'\n", test_file);
        JSON_DECREF(test_definition)
        return -1;
    }

    // Inject C_TEST_RUNNER service
    inject_test_runner_service(var_config);

    // Serialize to string for yuneta_entry_point
    variable_config_str = json_dumps(var_config, JSON_COMPACT);
    JSON_DECREF(var_config)

    if(!variable_config_str) {
        fprintf(stderr, "ERROR: Cannot serialize variable_config\n");
        JSON_DECREF(test_definition)
        return -1;
    }

    /*------------------------------*
     *  Init logger
     *------------------------------*/
    glog_init();
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    gobj_log_register_handler(
        "testing",
        0,
        capture_log_write,
        0
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_ERROR, 0);

    /*------------------------------------------------*
     *      Memory leak check
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    yuneta_setup(
        NULL,                   // persistent_attrs
        NULL,                   // command_parser
        NULL,                   // stats_parser
        test_authz_checker,     // authz_checker
        NULL,                   // authentication_parser
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY,
        USE_OWN_SYSTEM_MEMORY,
        MEM_MIN_BLOCK,
        MEM_SUPERBLOCK
    );

    /*
     *  Shift argv past the test file argument so yuneta_entry_point's
     *  argp parser doesn't interpret the file path as parameter_config.
     *  Set argv[1] to test_name so that after the shift it becomes argv[0],
     *  making basename(argv[0]) match the yuno_role.
     */
    argv[1] = (char *)test_name;
    result += yuneta_entry_point(
        argc - 1, argv + 1,
        test_name, APP_VERSION, APP_SUPPORT,
        "JSON-driven message interchange test",
        APP_DATETIME,
        fixed_config_buf,
        variable_config_str,
        register_yuno_and_more,
        cleaning
    );

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    /*------------------------------*
     *  Cleanup
     *  NOTE: test_definition was allocated with default malloc before
     *  yuneta_entry_point switched jansson to gbmem allocators.
     *  We cannot use JSON_DECREF here (it would call gbmem free on
     *  malloc'd memory). Instead, reload with the file path to free
     *  properly, or simply skip — the process is about to exit.
     *------------------------------*/
    free(variable_config_str);
    // test_definition intentionally not freed: allocated with malloc,
    // but jansson now uses gbmem; process is exiting anyway.

    if(result < 0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, test_name);
    }
    return result < 0 ? -1 : 0;
}
