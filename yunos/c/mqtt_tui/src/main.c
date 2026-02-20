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
    char *mqtt_session_expiry_interval;
    int mqtt_session_expiry_interval_set;
    char *mqtt_keepalive;
    char *mqtt_will_topic;
    char *mqtt_will_payload;
    char *mqtt_will_qos;
    char *mqtt_will_retain;
    char *mqtt_will_properties;

    char *user_id;
    char *user_passw;
    char *jwt;

    int verbose;                /* verbose */
    int print;
    int print_version;
    int print_yuneta_version;
    int print_with_metadata;
    int list_mqtt_properties;
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
{"mqtt_connect_properties", 'e',    "PROPERTIES",0,     "MQTT CONNECT properties as JSON string (MQTT v5 only). E.g. '{\"receive-maximum\":100}'. session-expiry-interval is always set via -x.", 10},
{"mqtt_session_expiry_interval", 'x', "SECONDS", 0, "Set the session-expiry-interval property on the CONNECT packet. Applies to MQTT v5 clients only. Set to 0-4294967294 to specify the session will expire in that many seconds after the client disconnects, or use -1, 4294967295, or ∞ for a session that does not expire. Defaults to -1 if -c is also given, or 0 if -c not given."
"If the session is set to never expire, either with -x or -c, then a client id must be provided", 10},
{"mqtt_keepalive",  'a',    "SECONDS",  0,      "MQTT keepalive in seconds for this client, default 60", 10},
{"will-topic",      2,    "TOPIC",      0,      "MQTT Will Topic", 10},
{"will-payload",    3,    "PAYLOAD",    0,      "MQTT Will Payload", 10},
{"will-qos",        4,    "QOS",        0,      "MQTT Will QoS", 10},
{"will-retain",     5,    "RETAIN",     0,      "MQTT Will Retain", 10},
{"will-properties", 6,    "JSON",       0,      "MQTT Will properties as JSON string (MQTT v5 only). E.g. '{\"will-delay-interval\":30,\"message-expiry-interval\":60}'.", 10},

{0,                 0,      0,          0,      "OAuth2 keys", 20},
{"auth_system",     'K',    "AUTH_SYSTEM",0,    "OpenID System(default: keycloak, to get now a jwt)", 20},
{"auth_url",        'k',    "AUTH_URL", 0,      "OpenID Endpoint (to get now a jwt). If empty it will be used MQTT username/password", 20},
{"azp",             'Z',    "AZP",      0,      "azp (Authorized Party, client_id in keycloak)", 20},
{"user_id",         'u',    "USER_ID",  0,      "MQTT Username or OAuth2 User Id (to get now a jwt)", 20},
{"user_passw",      'U',    "USER_PASSW",0,     "OAuth2 User Password (to get now a jwt)", 20},
{"mqtt_passw",      'P',    "MQTT_PASSW",0,     "MQTT Password", 20},
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
{"list-mqtt-properties", 7, 0,          0,      "List all MQTT v5 properties available for --mqtt_connect_properties and --will-properties, then exit.", 50},
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
static error_t parse_opt(int key, char *arg, struct argp_state *state)
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
        arguments->mqtt_session_expiry_interval_set = 1;
        break;
    case 'a':
        arguments->mqtt_keepalive = arg;
        break;

    case 2:
        arguments->mqtt_will_topic = arg;
        break;
    case 3:
        arguments->mqtt_will_payload = arg;
        break;
    case 4:
        arguments->mqtt_will_qos = arg;
        break;
    case 5:
        arguments->mqtt_will_retain = arg;
        break;
    case 6:
        arguments->mqtt_will_properties = arg;
        break;
    case 7:
        arguments->list_mqtt_properties = 1;
        break;

    case 'u':
        arguments->user_id = arg;
        break;
    case 'U':
    case 'P':
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
        gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
    }
    if(arguments.verbose > 3) {
        gobj_set_gclass_trace(gclass_find_by_name(C_TCP), "traffic", TRUE);
    }
    if(arguments.verbose > 4) {
        gobj_set_deep_tracing(1);
    }

    return 0;
}

/***************************************************************************
 *                      Print MQTT v5 property tables
 ***************************************************************************/
static void print_mqtt_properties(void)
{
    printf(
        "MQTT v5 CONNECT properties  (use with --mqtt_connect_properties)\n"
        "  Note: session-expiry-interval is always controlled by the -x flag.\n"
        "\n"
        "  %-40s %-12s %s\n"
        "  %-40s %-12s %s\n",
        "Property name", "Type", "Description",
        "----------------------------------------", "------------", "-------------------------------"
    );
    printf("  %-40s %-12s %s\n", "receive-maximum",               "integer",  "0..65535. Max incoming QoS1/2 in-flight");
    printf("  %-40s %-12s %s\n", "maximum-packet-size",           "integer",  "0..4294967295. Max packet size the client accepts");
    printf("  %-40s %-12s %s\n", "topic-alias-maximum",           "integer",  "0..65535. Max topic aliases the client accepts");
    printf("  %-40s %-12s %s\n", "request-response-information",  "byte",     "0 or 1. Request response info in CONNACK");
    printf("  %-40s %-12s %s\n", "request-problem-information",   "byte",     "0 or 1. Request reason string/user-property on errors");
    printf("  %-40s %-12s %s\n", "authentication-method",         "string",   "UTF-8 name of the authentication method");
    printf("  %-40s %-12s %s\n", "authentication-data",           "binary",   "Base64-encoded authentication data");
    printf("  %-40s %-12s %s\n", "user-property",                 "obj",      "{\"name\":\"key\",\"value\":\"val\"} — arbitrary key/value");

    printf(
        "\n"
        "MQTT v5 WILL properties  (use with --will-properties)\n"
        "\n"
        "  %-40s %-12s %s\n"
        "  %-40s %-12s %s\n",
        "Property name", "Type", "Description",
        "----------------------------------------", "------------", "-------------------------------"
    );
    printf("  %-40s %-12s %s\n", "will-delay-interval",           "integer",  "Seconds to delay the will after disconnect");
    printf("  %-40s %-12s %s\n", "payload-format-indicator",      "byte",     "0=binary, 1=UTF-8 string payload");
    printf("  %-40s %-12s %s\n", "message-expiry-interval",       "integer",  "Lifetime of the will message in seconds");
    printf("  %-40s %-12s %s\n", "content-type",                  "string",   "MIME type of the payload (e.g. application/json)");
    printf("  %-40s %-12s %s\n", "response-topic",                "string",   "UTF-8 topic for request/response pattern");
    printf("  %-40s %-12s %s\n", "correlation-data",              "binary",   "Base64-encoded correlation data");
    printf("  %-40s %-12s %s\n", "user-property",                 "obj",      "{\"name\":\"key\",\"value\":\"val\"} — arbitrary key/value");

    printf(
        "\n"
        "JSON format examples:\n"
        "  --mqtt_connect_properties='{\"receive-maximum\":100,\"topic-alias-maximum\":10}'\n"
        "  --will-properties='{\"will-delay-interval\":30,\"message-expiry-interval\":3600,\"content-type\":\"text/plain\"}'\n"
        "\n"
        "Types:\n"
        "  integer   JSON number (e.g. 30)\n"
        "  byte      JSON number 0 or 1\n"
        "  string    JSON string (e.g. \"text/plain\")\n"
        "  binary    JSON string containing base64-encoded bytes\n"
        "  obj       JSON object with \"name\" and \"value\" string fields\n"
        "\n"
    );
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
    arguments.mqtt_session_expiry_interval = "0";
    arguments.mqtt_keepalive = "60";
    arguments.user_id = "yuneta";
    arguments.user_passw = "";
    arguments.jwt = "";
    arguments.mqtt_connect_properties = "";
    arguments.mqtt_will_topic = "";
    arguments.mqtt_will_payload = "";
    arguments.mqtt_will_qos = "";
    arguments.mqtt_will_retain = "";
    arguments.mqtt_will_properties = "";

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
     *  Apply session expiry default: -1 (never) if -c given, 0 (non-persistent) otherwise
     */
    if(arguments.mqtt_persistent_session && !arguments.mqtt_session_expiry_interval_set) {
        arguments.mqtt_session_expiry_interval = "-1";
    }

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
    if(arguments.list_mqtt_properties) {
        print_mqtt_properties();
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
        json_t *kw_utility = json_pack(
            "{s:{s:b, s:s, s:s, s:s, s:s, s:s, s:b, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:b}}",
            "global",
            "C_MQTT_TUI.verbose", arguments.verbose,
            "C_MQTT_TUI.auth_system", arguments.auth_system,
            "C_MQTT_TUI.auth_url", arguments.auth_url,
            "C_MQTT_TUI.mqtt_client_id", arguments.mqtt_client_id,
            "C_MQTT_TUI.mqtt_protocol", arguments.mqtt_protocol,
            "C_MQTT_TUI.mqtt_clean_session", arguments.mqtt_persistent_session?"0":"1",
            "C_MQTT_TUI.mqtt_persistent_db", arguments.mqtt_persistent_client_db?1:0,
            "C_MQTT_TUI.mqtt_session_expiry_interval", arguments.mqtt_session_expiry_interval,
            "C_MQTT_TUI.mqtt_keepalive", arguments.mqtt_keepalive,
            "C_MQTT_TUI.mqtt_connect_properties", arguments.mqtt_connect_properties,

            "C_MQTT_TUI.mqtt_will_topic", arguments.mqtt_will_topic,
            "C_MQTT_TUI.mqtt_will_payload", arguments.mqtt_will_payload,
            "C_MQTT_TUI.mqtt_will_qos", arguments.mqtt_will_qos,
            "C_MQTT_TUI.mqtt_will_retain", arguments.mqtt_will_retain,
            "C_MQTT_TUI.mqtt_will_properties", arguments.mqtt_will_properties,

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
