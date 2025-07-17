/****************************************************************************
 *          MAIN_YCOMMAND.C
 *          ycommand main
 *
 *          Yuneta Statistics
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <argp.h>
#include <unistd.h>
#include <yuneta_tls.h>
#include "c_ycommand.h"
#include "yuno_ycommand.h"

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
    char *url;
    char *azp;
    char *yuno_role;
    char *yuno_name;
    char *yuno_service;
    char *command;
    int wait;

    char *auth_system;
    char *auth_url;
    char *user_id;
    char *user_passw;
    char *jwt;

    int verbose;                /* verbose */
    int print;
    int print_version;
    int print_yuneta_version;
    int interactive;
    int print_with_metadata;
};

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "ycommand"
#define APP_DOC         "Yuneta Command"

#define APP_VERSION     __yuneta_version__
#define APP_DATETIME    __DATE__ " " __TIME__
#define APP_SUPPORT     "<niyamaka at yuneta.io>"

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'realm_owner': 'agent',                                     \n\
        'work_dir': '/yuneta',                                      \n\
        'domain_dir': 'realms/agent/ycommand'                       \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'yuno_role': 'ycommand',                                    \n\
        'tags': ['yuneta', 'core']                                  \n\
    }                                                               \n\
}                                                                   \n\
";
PRIVATE char variable_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'use_system_memory': true,                                  \n\
        'log_gbmem_info': true,                                     \n\
        'MEM_MIN_BLOCK': 512,                                       \n\
        'MEM_MAX_BLOCK': 209715200,             #^^  200*M          \n\
        'MEM_SUPERBLOCK': 209715200,            #^^  200*M          \n\
        'MEM_MAX_SYSTEM_MEMORY': 2147483648,    #^^ 2*G             \n\
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
                'filename_mask': 'ycommand-W.log',                  \n\
                'handler_options': 255                              \n\
            }                                                       \n\
        }                                                           \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'trace_levels': {                                           \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'ycommand',                                     \n\
            'gclass': 'YCommand',                                   \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'zchilds': [                                            \n\
            ]                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************************
 *      Data
 ***************************************************************************/
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
{0,                 0,      0,          0,      "Remote Service keys", 10},
{"command",         'c',    "COMMAND",  0,      "Command.", 10},
{"interactive",     'i',    0,          0,      "Interactive.", 10},
{"wait",            'w',    "SECONDS",  0,      "Wait until exit, default 2.", 10},

{0,                 0,      0,          0,      "OAuth2 keys", 20},
{"auth_system",     'K',    "AUTH_SYSTEM",0,    "OpenID System(default: keycloak, to get now a jwt)", 20},
{"auth_url",        'k',    "AUTH_URL", 0,      "OpenID Endpoint (to get now a jwt)", 20},
{"azp",             'Z',    "AZP",      0,      "azp (Authorized Party, client_id in keycloak)", 20},
{"user_id",         'x',    "USER_ID",  0,      "OAuth2 User Id (to get now a jwt)", 20},
{"user_passw",      'X',    "USER_PASSW",0,     "OAuth2 User Password (to get now a jwt)", 20},
{"jwt",             'j',    "JWT",      0,      "Jwt (previously got it)", 21},

{0,                 0,      0,          0,      "Connection keys", 30},
{"url",             'u',    "URL",      0,      "Url to connect (default 'ws://127.0.0.1:1991').", 30},
{"yuno_role",       'O',    "ROLE",     0,      "Remote yuno role. Default: 'yuneta_agent'", 30},
{"yuno_name",       'o',    "NAME",     0,      "Remote yuno name. Default: ''", 30},
{"yuno_service",    'S',    "SERVICE",  0,      "Remote yuno service. Default: '__default_service__'", 30}, // TODO chequea todos, estaba solo como 'service'

{0,                 0,      0,          0,      "Local keys.", 50},
{"print",           'p',    0,          0,      "Print configuration.", 50},
{"print-role",      'r',    0,          0,      "print the basic yuno's information"},
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
    doc
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

    case 'x':
        arguments->user_id = arg;
        break;
    case 'X':
        arguments->user_passw = arg;
        break;
    case 'j':
        arguments->jwt = arg;
        break;

    case 'u':
        arguments->url = arg;
        break;

    case 'c':
        arguments->command = arg;
        break;
    case 'i':
        arguments->interactive = 1;
        break;
    case 'w':
        if(arg) {
            arguments->wait = atoi(arg);
        }
        break;

    case 'Z':
        arguments->azp = arg;
        break;
    case 'O':
        arguments->yuno_role = arg;
        break;
    case 'o':
        arguments->yuno_name = arg;
        break;

    case 'S':
        arguments->yuno_service = arg;
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
static void register_yuno_and_more(void)
{
    /*------------------------*
     *  Register yuneta-tls
     *------------------------*/
    yuneta_register_c_tls();

    /*-------------------*
     *  Register yuno
     *-------------------*/
    register_yuno_ycommand();

    /*--------------------*
     *  Register service
     *--------------------*/
    gobj_register_gclass(GCLASS_EDITLINE);
    gobj_register_gclass(GCLASS_YCOMMAND);
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    struct arguments arguments;
    /*
     *  Default values
     */
    memset(&arguments, 0, sizeof(arguments));
    arguments.url = "ws://127.0.0.1:1991";
    arguments.command = "";
    arguments.azp = "";
    arguments.yuno_role = "yuneta_agent";
    arguments.yuno_name = "";
    arguments.yuno_service = "__default_service__";
    arguments.auth_system = "keycloak";
    arguments.auth_url = "";
    arguments.user_id = "";
    arguments.user_passw = "";
    arguments.jwt = "";
    arguments.wait = 2;

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
        printf("%s\n", __yuneta_long_version__);
        exit(0);
    }

    /*
     *  Put configuration
     */
    {
        json_t *kw_utility = json_pack(
            "{s:{s:b, s:s, s:i, s:i, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:b}}",
            "global",
            "YCommand.verbose", arguments.verbose,
            "YCommand.command", arguments.command,
            "YCommand.interactive", arguments.interactive,
            "YCommand.wait", arguments.wait,
            "YCommand.auth_system", arguments.auth_system,
            "YCommand.auth_url", arguments.auth_url,
            "YCommand.user_id", arguments.user_id,
            "YCommand.user_passw", arguments.user_passw,
            "YCommand.jwt", arguments.jwt,
            "YCommand.url", arguments.url,
            "YCommand.azp", arguments.azp,
            "YCommand.yuno_role", arguments.yuno_role,
            "YCommand.yuno_name", arguments.yuno_name,
            "YCommand.yuno_service", arguments.yuno_service,
            "YCommand.print_with_metadata", arguments.print_with_metadata
        );

        char *param1_ = json_dumps(kw_utility, JSON_COMPACT);
        if(!param1_) {
            printf("Some parameter is wrong\n");
            exit(-1);
        }
        int len = strlen(param1_) + 3;
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
     *  To trace memory
     *------------------------------------------------*/
#ifdef DEBUG
    BOOL debug_memory = 0;
    static uint32_t mem_list[] = {14740, 0};
    gbmem_trace_alloc_free(debug_memory, mem_list);
#endif

    int log_handler_options = -1;
    if(arguments.verbose == 0) {
        log_handler_options = 7;
    }
    if(arguments.verbose > 0) {
        gobj_set_gclass_trace(GCLASS_IEVENT_CLI, "ievents2", TRUE);
        gobj_set_gclass_trace(GCLASS_IEVENT_CLI, "kw", TRUE);

        gobj_set_gclass_trace(GCLASS_TCP0, "connections", TRUE);
        gobj_set_gclass_trace(GCLASS_TCP1, "connections", TRUE);
        gobj_set_gclass_trace(GCLASS_TCP_S0, "listen", TRUE);
        gobj_set_gclass_trace(GCLASS_TCP_S1, "listen", TRUE);
        gobj_set_gclass_trace(GCLASS_TCP_S0, "not-accepted", TRUE);
        gobj_set_gclass_trace(GCLASS_TCP_S1, "not-accepted", TRUE);
        gobj_set_gclass_trace(GCLASS_TCP_S0, "accepted", TRUE);
        gobj_set_gclass_trace(GCLASS_TCP_S1, "accepted", TRUE);
        gobj_set_gclass_trace(GCLASS_PROT_HTTP_CLI, "traffic", TRUE);
    }

    if(arguments.verbose > 1) {
        gobj_set_gobj_trace(0, "machine", TRUE, 0);
        gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
    }
    if(arguments.verbose > 2) {
        gobj_set_gobj_trace(0, "subscriptions", TRUE, 0);
        gobj_set_gobj_trace(0, "create_delete", TRUE, 0);
    }
    if(arguments.verbose > 3) {
        gobj_set_gobj_trace(0, "create_delete2", TRUE, 0);
        gobj_set_gobj_trace(0, "ev_kw2", TRUE, 0);
    }
    if(arguments.verbose > 4) {
        gobj_set_gclass_trace(GCLASS_TASK, "messages", TRUE);
        gobj_set_gclass_trace(GCLASS_TCP0, "traffic", TRUE);
        gobj_set_gclass_trace(GCLASS_TCP1, "traffic", TRUE);
    }
    if(arguments.verbose > 5) {
        gobj_set_gobj_trace(0, "start_stop", TRUE, 0);
        gobj_set_gobj_trace(0, "libuv", TRUE, 0);
        gobj_set_gclass_trace(GCLASS_YCOMMAND, "trace-kb", TRUE);
    }
    if(arguments.verbose < 0) {
        gobj_set_deep_tracing(arguments.verbose);
    }

    gobj_set_gclass_no_trace(GCLASS_TIMER, "machine", TRUE);

#ifdef DEBUG
    if(debug_memory) {
        log_handler_options &= ~LOG_HND_OPT_BEATIFUL_JSON;
    }
#endif
    static char my_variable_config[16*1024];
    snprintf(my_variable_config, sizeof(my_variable_config), variable_config, log_handler_options);

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(my_variable_config);
    yuneta_setup(
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0
    );
    return yuneta_entry_point(
        idx, argvs,
        APP_NAME, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        my_variable_config,
        register_yuno_and_more
    );
}
