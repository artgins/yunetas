/****************************************************************************
 *          MAIN_MQTT_CLIENT.C
 *          mqtt_tui main
 *
 *          Mqtt TUI (Terminal User Interface)
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <unistd.h>

#include <argp-standalone.h>
#include <yunetas.h>
#include <c_editline.h>
#include "c_prot_mqtt2.h"  // TODO remove when mqtt be migrated
#include "c_mqtt_tui.h"

/***************************************************************************
 *              Structures
 ***************************************************************************/
/*
 *  Used by main to communicate with parse_opt.
 */
#define MIN_ARGS 0
#define MAX_ARGS 0
struct arguments
{
    char *args[MAX_ARGS+1];     /* positional args */

    int print_role;
    char *url_broker;
    char *url_mqtt;
    char *azp;

    char *auth_system;
    char *auth_url;
    char *mqtt_client_id;
    int mqtt_persistent_session;
    int mqtt_persistent_client_db;
    char *mqtt_protocol;

    char *mqtt_connect_properties;
    char *mqtt_will;
    char *mqtt_session_expiry_interval;
    char *mqtt_keepalive;
    int mqtt_persistent;

    char *user_id;
    char *user_passw;
    char *jwt;

    int verbose;                /* verbose */
    int print;
    int print_version;
    int print_yuneta_version;
    int print_with_metadata;
};

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "mqtt_tui"
#define APP_DOC         "Mqtt client"

#define APP_VERSION     YUNETA_VERSION
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           512
#define MEM_MAX_BLOCK           (1*1024*1024*1024L)     // 1*G
#define MEM_SUPERBLOCK          (1*1024*1024*1024L)     // 1*G
#define MEM_MAX_SYSTEM_MEMORY   (16*1024*1024*1024L)    // 16*G

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'realm_owner': '',                                          \n\
        'work_dir': '/yuneta',                                      \n\
        'domain_dir': 'realms/agent/mqtt_tui'                       \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'yuno_role': 'mqtt_tui',                                    \n\
        'tags': ['yuneta', 'core']                                  \n\
    }                                                               \n\
}                                                                   \n\
";

PRIVATE char variable_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'console_log_handlers': {                                   \n\
            'to_stdout': {                                          \n\
                'handler_type': 'stdout',                           \n\
                'handler_options': %d       #^^ 7                   \n\
            },                                                      \n\
            'to_udp': {                                             \n\
                'handler_type': 'udp',                              \n\
                'url': 'udp://127.0.0.1:1992',                      \n\
                'handler_options': 255                              \n\
            },                                                      \n\
            'to_file': {                                            \n\
                'handler_type': 'file',                             \n\
                'filename_mask': 'mqtt_tui-W.log',                  \n\
                'handler_options': 255                              \n\
            }                                                       \n\
        }                                                           \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'trace_levels': {                                           \n\
            'C_TCP': ['connections'],                               \n\
            'C_TCP_S': ['listen', 'not-accepted', 'accepted']       \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'mqtt_tui',                                     \n\
            'gclass': 'C_MQTT_TUI',                                 \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************************
 *      Data
 ***************************************************************************/
struct arguments arguments;

// Set by yuneta_entry_point()
// const char *argp_program_version = APP_NAME " " APP_VERSION;
// const char *argp_program_bug_address = APP_SUPPORT;

/* Program documentation. */
static char doc[] = APP_DOC;

/* A description of the arguments we accept. */
static char args_doc[] = "";

/*
 *  The options we understand.
 *  See https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
 */
static struct argp_option options[] = {
/*-name-------------key-----arg---------flags---doc-----------------group */
{0,                 0,      0,          0,      "MQTT keys", 10},
{"mqtt-persistent-session",'c', 0,      0,      "Enable persistent client mode (Set 'clean_session' to 0). When this argument is used, the broker will be instructed not to clean existing sessions for the same client id when the client connects, and sessions will never expire when the client disconnects. MQTT v5 clients can change their session expiry interval with the -x argument.\n"
"When a session is persisted on the broker, the subscriptions for the client will be maintained after it disconnects, along with subsequent QoS 1 and QoS 2 messages that arrive. When the client reconnects and does not clean the session, it will receive all of the queued messages.", 10},
{"mqtt-persistent-db", 'd', 0,          0,      "Use persistent database for Inflight and Queued Messages in mqtt client side", 10},
{"id",              'i',    "CLIENT_ID",0,      "MQTT Client ID", 10},
{"mqtt_protocol",   'q',    "PROTOCOL", 0,      "MQTT Protocol. Can be mqttv5 (v5), mqttv311 (v311) or mqttv31 (v31). Defaults to v5.", 10},
{"mqtt_connect_properties", 'r',    "PROPERTIES",0,     "(TODO) MQTT CONNECT properties", 10},
{"mqtt_session_expiry_interval", 'x', "SECONDS", 0, "Set the session-expiry-interval property on the CONNECT packet. Applies to MQTT v5 clients only. Set to 0-4294967294 to specify the session will expire in that many seconds after the client disconnects, or use -1, 4294967295, or ∞ for a session that does not expire. Defaults to -1 if -c is also given, or 0 if -c not given."
"If the session is set to never expire, either with -x or -c, then a client id must be provided", 10},
{"mqtt_will",       'w',    "WILL",     0,      "(TODO) MQTT Will (topic, retain, qos, payload)", 10},
{"mqtt_keepalive",  'a',    "SECONDS",  0,      "MQTT keepalive in seconds for this client, default 60", 10},

{0,                 0,      0,          0,      "OAuth2 keys", 20},
{"auth_system",     'K',    "AUTH_SYSTEM",0,    "OpenID System(default: keycloak, to get now a jwt)", 20},
{"auth_url",        'k',    "AUTH_URL", 0,      "OpenID Endpoint (to get now a jwt). If empty it will be used MQTT username/password", 20},
{"azp",             'Z',    "AZP",      0,      "azp (Authorized Party, client_id in keycloak)", 20},
{"user_id",         'u',    "USER_ID",  0,      "MQTT Username or OAuth2 User Id (to get now a jwt)", 20},
{"user_passw",      'U',    "USER_PASSW",0,     "MQTT Password or OAuth2 User Password (to get now a jwt)", 20},
{"jwt",             'j',    "JWT",      0,      "Jwt (previously got it)", 21},

{0,                 0,      0,          0,      "Connection keys", 30},
{"url-mqtt",        'h',    "URL-MQTT", 0,      "Url of mqtt port (default 'mqtt://127.0.0.1:1810').", 30},
{"url-broker",      'b',    "URL-BROKER",0,     "Url of broker port (default 'ws://127.0.0.1:1800').", 30},

{0,                 0,      0,          0,      "Local keys.", 50},
{"print",           'p',    0,          0,      "Print configuration.", 50},
{"print-role",      'r',    0,          0,      "print the basic yuno's information", 0},
{"verbose",         'l',    "LEVEL",    0,      "Verbose level.", 50},
{"version",         'v',    0,          0,      "Print version.", 50},
{"yuneta-version",  'V',    0,          0,      "Print yuneta version", 50},
{"with-metadata",   'm',    0,          0,      "Print with metadata", 50},
{0}
};

/* Our argp parser. */
static struct argp argp = {
    options,
    parse_opt,
    args_doc,
    doc,
    0,0,0
};

/***************************************************************************
 *  Parse a single option
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
    /*
     *  Get the input argument from argp_parse,
     *  which we know is a pointer to our arguments structure.
     */
    struct arguments *arguments = state->input;

    switch (key) {
    case 'K':
        arguments->auth_system = arg;
        break;
    case 'k':
        arguments->auth_url = arg;
        break;

    case 'd':
        arguments->mqtt_persistent_client_db = 1;
        break;
    case 'c':
        arguments->mqtt_persistent_session = 1;
        break;
    case 'i':
        arguments->mqtt_client_id = arg;
        break;
    case 'q':
        arguments->mqtt_protocol = arg;
        break;
    case 'e':
        arguments->mqtt_connect_properties = arg;
        break;
    case 'x':
        arguments->mqtt_session_expiry_interval = arg;
        break;
    case 'w':
        arguments->mqtt_will = arg;
        break;
    case 'a':
        arguments->mqtt_keepalive = arg;
        break;

    case 'u':
        arguments->user_id = arg;
        break;
    case 'U':
        arguments->user_passw = arg;
        break;
    case 'j':
        arguments->jwt = arg;
        break;

    case 'b':
        arguments->url_broker = arg;
        break;
    case 'h':
        arguments->url_mqtt = arg;
        break;

    case 'Z':
        arguments->azp = arg;
        break;

    case 'v':
        arguments->print_version = 1;
        break;

    case 'V':
        arguments->print_yuneta_version = 1;
        break;

    case 'l':
        if(arg) {
            arguments->verbose = atoi(arg);
        }
        break;

    case 'm':
        arguments->print_with_metadata = 1;
        break;
    case 'r':
        arguments->print_role = 1;
        break;
    case 'p':
        arguments->print = 1;
        break;

    case ARGP_KEY_ARG:
        if (state->arg_num >= MAX_ARGS) {
            /* Too many arguments. */
            argp_usage (state);
        }
        arguments->args[state->arg_num] = arg;
        break;

    case ARGP_KEY_END:
        if (state->arg_num < MIN_ARGS) {
            /* Not enough arguments. */
            argp_usage (state);
        }
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

/***************************************************************************
 *                      Register
 ***************************************************************************/
static int register_yuno_and_more(void)
{
    /*--------------------*
     *  Register gclass
     *--------------------*/
    register_c_mqtt_client();
    register_c_editline();
    register_c_prot_mqtt2(); // TODO remove when mqtt be migrated

    /*------------------------------------------------*
     *          Traces
     *------------------------------------------------*/
    // Avoid timer trace, too much information
    gobj_set_gclass_no_trace(gclass_find_by_name(C_YUNO), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER), "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    if(arguments.verbose > 0) {
        gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_SRV), "ievents2", TRUE);
        gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "ievents2", TRUE);
        gobj_set_gclass_trace(gclass_find_by_name(C_PROT_MQTT2), "show-decode", TRUE);
        gobj_set_gclass_trace(gclass_find_by_name(C_PROT_MQTT2), "traffic", TRUE);
    }
    if(arguments.verbose > 1) {
        gobj_set_gobj_trace(0, "machine", TRUE, 0);
        gobj_set_gobj_trace(0, "start_stop", TRUE, 0);
    }
    if(arguments.verbose > 2) {
        gobj_set_gobj_trace(0, "subscriptions", TRUE, 0);
        gobj_set_gobj_trace(0, "create_delete", TRUE, 0);
    }
    if(arguments.verbose > 3) {
        gobj_set_gclass_trace(gclass_find_by_name(C_TCP), "traffic", TRUE);
        gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
    }
    if(arguments.verbose > 4) {
        gobj_set_deep_tracing(1);
    }

    return 0;
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*
     *  Default values
     */
    memset(&arguments, 0, sizeof(arguments));
    arguments.url_mqtt = "mqtt://127.0.0.1:1810";
    arguments.url_broker = "ws://127.0.0.1:1800";
    arguments.azp = "";
    arguments.auth_system = "keycloak";
    arguments.auth_url = "";
    arguments.mqtt_protocol = "mqttv5";
    arguments.mqtt_client_id = "";
    arguments.mqtt_persistent_client_db = 0;
    arguments.mqtt_persistent_session = 0;
    arguments.mqtt_session_expiry_interval = "-1";
    arguments.mqtt_keepalive = "60";
    arguments.user_id = "yuneta";
    arguments.user_passw = "";
    arguments.jwt = "";

    /*
     *  Save args
     */
    char argv0[512];
    char *argvs[]= {argv0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    memset(argv0, 0, sizeof(argv0));
    strncpy(argv0, argv[0], sizeof(argv0)-1);
    int idx = 1;

    /*
     *  Parse arguments
     */
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    /*
     *  Check arguments
     */
    if(arguments.print_version) {
        printf("%s %s\n", APP_NAME, APP_VERSION);
        exit(0);
    }
    if(arguments.print_yuneta_version) {
        printf("%s\n", YUNETA_VERSION);
        exit(0);
    }

    /*
     *  Check configuration
     */

    const char *protocols[] = {"mqttv5", "mqttv311", "mqttv31", "v5", "v311", "v31", 0};
    if(!str_in_list(protocols, arguments.mqtt_protocol, FALSE)) {
        fprintf(stderr, "Error: Invalid protocol version argument given.\n\n");
        exit(0);
    }

    if(strstr(arguments.mqtt_protocol, "v5")) {
        if(atoi(arguments.mqtt_session_expiry_interval) < 0) {
            if(empty_string(arguments.mqtt_client_id)) {
                fprintf(stderr, "Error: You must provide a client id if you are using an infinite session expiry interval.\n");
                exit(0);
            }
        }
    }
    if(arguments.mqtt_persistent_session && empty_string(arguments.mqtt_client_id)) {
        fprintf(stderr, "\nError: You must provide a client id if you are using the -c option.\n\n");
        exit(0);
    }

    /*
     *  Put configuration
     */
    {
        // TODO missing connect properties, will
        json_t *kw_utility = json_pack(
            "{s:{s:b, s:s, s:s, s:s, s:s, s:s, s:i, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:b}}",
            "global",
            "C_MQTT_TUI.verbose", arguments.verbose,
            "C_MQTT_TUI.auth_system", arguments.auth_system,
            "C_MQTT_TUI.auth_url", arguments.auth_url,
            "C_MQTT_TUI.mqtt_client_id", arguments.mqtt_client_id,
            "C_MQTT_TUI.mqtt_protocol", arguments.mqtt_protocol,
            "C_MQTT_TUI.mqtt_clean_session", arguments.mqtt_persistent_session?"0":"1",
            "C_MQTT_TUI.mqtt_persistent_client_db", arguments.mqtt_persistent_client_db?1:0,
            "C_MQTT_TUI.mqtt_session_expiry_interval", arguments.mqtt_session_expiry_interval,
            "C_MQTT_TUI.mqtt_keepalive", arguments.mqtt_keepalive,
            "C_MQTT_TUI.user_id", arguments.user_id,
            "C_MQTT_TUI.user_passw", arguments.user_passw,
            "C_MQTT_TUI.jwt", arguments.jwt,
            "C_MQTT_TUI.url_mqtt", arguments.url_mqtt,
            "C_MQTT_TUI.url_broker", arguments.url_broker,
            "C_MQTT_TUI.azp", arguments.azp,
            "C_MQTT_TUI.print_with_metadata", arguments.print_with_metadata
        );
        char *param1_ = json_dumps(kw_utility, JSON_COMPACT);
        if(!param1_) {
            printf("Some parameter is wrong\n");
            exit(-1);
        }
        int len = (int)strlen(param1_) + 3;
        char *param1 = malloc(len);
        if(param1) {
            memset(param1, 0, len);
            snprintf(param1, len, "%s", param1_);
        }
        free(param1_);
        argvs[idx++] = param1;
    }

    if(arguments.print) {
        argvs[idx++] = "-P";
    }
    if(arguments.print_role) {
        argvs[idx++] = "--print-role";
    }

    /*------------------------------------------------*
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0}; // WARNING: the list ended with 0
    set_memory_check_list(memory_check_list);

    /*------------------------------------------------*
     *      To check
     *------------------------------------------------*/
    // gobj_set_deep_tracing(1);
    // set_auto_kill_time(6);
    // set_measure_times(-1 & ~YEV_TIMER_TYPE);

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    int log_handler_options = -1;
    if(arguments.verbose == 0) {
        log_handler_options = 7;
    }

    static char my_variable_config[16*1024];
    snprintf(my_variable_config, sizeof(my_variable_config), variable_config, log_handler_options);

    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(my_variable_config);
    yuneta_setup(
        NULL,       // persistent_attrs, default internal dbsimple
        NULL,       // command_parser, default internal command_parser
        NULL,       // stats_parser, default internal stats_parser
        NULL,       // authz_checker, default Monoclass C_AUTHZ
        NULL,       // authentication_parser, default Monoclass C_AUTHZ
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY,
        USE_OWN_SYSTEM_MEMORY,
        MEM_MIN_BLOCK,
        MEM_SUPERBLOCK
    );
    return yuneta_entry_point(
        idx, argvs,
        APP_NAME, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        my_variable_config,
        register_yuno_and_more,
        NULL
    );
}
