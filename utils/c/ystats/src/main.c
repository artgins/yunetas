/****************************************************************************
 *          MAIN_YSTATS.C
 *          ystats main
 *
 *          Yuneta Statistics
 *
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <argp.h>
#include <unistd.h>
#include "c_ystats.h"

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
    int refresh_time;           /* refresh_time time, in seconds (0 remove subscription) */
    char *url;
    char *stats;
    char *attribute;
    char *command;
    char *azp;
    char *yuno_role;
    char *yuno_name;
    char *yuno_service;
    char *gobj_name;

    char *auth_system;
    char *auth_url;
    char *user_id;
    char *user_passw;
    char *jwt;

    int verbose;                /* verbose */
    int print;
    int print_version;
    int print_yuneta_version;
    int use_config_file;
    const char *config_json_file;
    int print_with_metadata;
};

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "ystats"
#define APP_DOC         "Yuneta Statistics"

#define APP_VERSION     YUNETA_VERSION
#define APP_DATETIME    __DATE__ " " __TIME__
#define APP_SUPPORT     "<support at artgins.com>"

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
        'realm_owner': 'agent',                                     \n\
        'work_dir': '/yuneta',                                      \n\
        'domain_dir': 'realms/agent/ystats'                         \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'yuno_role': 'ystats',                                      \n\
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
            'name': 'ystats',                                       \n\
            'gclass': 'C_YSTATS',                                   \n\
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
{0,                 0,      0,          0,      "Remote Service keys", 10},
{"refresh_time",    't',    "NUMBER",   0,      "Refresh time, in seconds (default 1).", 10},
{"stats",           's',    "STATS",    0,      "Statistic to ask.", 10},
{"attribute",       'a',    "ATTRIBUTE",0,      "Attribute to ask .", 10},
{"command",         'c',    "COMMAND",  0,      "Command to execute .", 10},
{"gobj_name",       'g',    "GOBJNAME", 0,      "Attribute's GObj (named-gobj or full-path).", 10},

{0,                 0,      0,          0,      "OAuth2 keys", 20},
{"auth_system",     'K',    "AUTH_SYSTEM",0,    "OpenID System(default: keycloak, to get now a jwt)", 20},
{"auth_url",        'k',    "AUTH_URL", 0,      "OpenID Endpoint (to get now a jwt)", 20},
{"user_id",         'x',    "USER_ID",  0,      "OAuth2 User Id (to get now a jwt)", 20},
{"azp",             'Z',    "AZP",      0,      "azp (Authorized Party, client_id in keycloak)", 20},
{"user_passw",      'X',    "USER_PASSW",0,     "OAuth2 User Password (to get now a jwt)", 20},
{"jwt",             'j',    "JWT",      0,      "Jwt (previously got it)", 21},

{0,                 0,      0,          0,      "Connection keys", 30},
{"url",             'u',    "URL",      0,      "Url to connect. Default: 'ws://127.0.0.1:1991'.", 30},
{"yuno_role",       'O',    "ROLE",     0,      "Remote yuno role. Default: ''", 30},
{"yuno_name",       'o',    "NAME",     0,      "Remote yuno name. Default: ''", 30},
{"yuno_service",    'S',    "SERVICE",  0,      "Remote yuno service. Default: '__default_service__'", 30},

{0,                 0,      0,          0,      "Local keys.", 50},
{"print",           'p',    0,          0,      "Print configuration.", 50},
{"config-file",     'f',    "FILE",     0,      "load settings from json config file or [files]", 50},
{"print-role",      'r',    0,          0,      "print the basic yuno's information", 50},
{"verbose",         'l',    "LEVEL",    0,      "Verbose level.", 50},
{"version",         'v',    0,          0,      "Print program version.", 50},
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
    0,
    0,
    0
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

    case 't':
        if(arg) {
            arguments->refresh_time = atoi(arg);
        }
        break;

    case 'u':
        arguments->url = arg;
        break;

    case 'O':
        arguments->yuno_role = arg;
        break;
    case 'o':
        arguments->yuno_name = arg;
        break;

    case 'Z':
        arguments->azp = arg;
        break;
    case 'S':
        arguments->yuno_service = arg;
        break;
    case 's':
        arguments->stats = arg;
        break;

    case 'a':
        arguments->attribute = arg;
        break;
    case 'c':
        arguments->command = arg;
        break;
    case 'g':
        arguments->gobj_name = arg;
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
    case 'f':
        arguments->config_json_file = arg;
        arguments->use_config_file = 1;
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
    register_c_ystats();

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
    }
    if(arguments.verbose > 1) {
        gobj_set_gclass_trace(gclass_find_by_name(C_TCP), "traffic", TRUE);
    }
    if(arguments.verbose > 2) {
        gobj_set_gobj_trace(0, "machine", TRUE, 0);
        gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
        gobj_set_gobj_trace(0, "subscriptions", TRUE, 0);
        gobj_set_gobj_trace(0, "create_delete", TRUE, 0);
        gobj_set_gobj_trace(0, "start_stop", TRUE, 0);
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
    arguments.refresh_time = 1;
    arguments.url = "ws://127.0.0.1:1991";
    arguments.stats = "";
    arguments.gobj_name = "";
    arguments.attribute = "";
    arguments.command = "";
    arguments.azp = "";
    arguments.yuno_role = 0;
    arguments.yuno_name = 0;
    arguments.yuno_service = "__default_service__";
    arguments.auth_system = "keycloak";
    arguments.auth_url = "";
    arguments.user_id = "";
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
     *  Put configuration
     */
    if(arguments.use_config_file) {
        int l = strlen("--config-file=") + strlen(arguments.config_json_file) + 4;
        char *param2 = malloc(l);
        snprintf(param2, l, "--config-file=%s", arguments.config_json_file);
        argvs[idx++] = param2;
    } else {
        json_t *kw_utility = json_pack(
            "{s:{s:i, s:b, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:b}}",
            "global",
            "C_YSTATS.refresh_time", arguments.refresh_time,
            "C_YSTATS.verbose", arguments.verbose,
            "C_YSTATS.stats", arguments.stats,
            "C_YSTATS.gobj_name", arguments.gobj_name,
            "C_YSTATS.attribute", arguments.attribute,
            "C_YSTATS.command", arguments.command,
            "C_YSTATS.azp", arguments.azp,
            "C_YSTATS.auth_system", arguments.auth_system,
            "C_YSTATS.auth_url", arguments.auth_url,
            "C_YSTATS.user_id", arguments.user_id,
            "C_YSTATS.user_passw", arguments.user_passw,
            "C_YSTATS.jwt", arguments.jwt,
            "C_YSTATS.url", arguments.url,
            "C_YSTATS.yuno_service", arguments.yuno_service,
            "C_YSTATS.print_with_metadata", arguments.print_with_metadata
        );
        json_t *jn_ystats = kw_get_dict_value(0, kw_utility, "global", 0, 0);
        if(arguments.yuno_role) {
            json_object_set_new(jn_ystats, "C_YSTATS.yuno_role", json_string(arguments.yuno_role));
        }
        if(arguments.yuno_name) {
            json_object_set_new(jn_ystats, "C_YSTATS.yuno_name", json_string(arguments.yuno_name));
        }

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
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0}; // WARNING: the list ended with 0
    set_memory_check_list(memory_check_list);

    int log_handler_options = -1;
    if(arguments.verbose == 0) {
        log_handler_options = 7;
    }
    static char my_variable_config[16*1024];
    snprintf(my_variable_config, sizeof(my_variable_config), variable_config, log_handler_options);

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    yuneta_setup(
        NULL,       // persistent_attrs, default internal dbsimple
        NULL,       // command_parser, default internal command_parser
        NULL,       // stats_parser, default internal stats_parser
        NULL,       // authz_checker, default Monoclass C_AUTHZ
        NULL,       // authenticate_parser, default Monoclass C_AUTHZ
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY,
        FALSE, //USE_OWN_SYSTEM_MEMORY,
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
