/****************************************************************************
 *          MAIN_YTESTS.C
 *          ytests main
 *
 *          Yuneta Tests
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <argp.h>
#include <unistd.h>
#include <yuneta_tls.h>
#include "c_ytests.h"
#include "yuno_ytests.h"

/***************************************************************************
 *              Structures
 ***************************************************************************/
/*
 *  Used by main to communicate with parse_opt.
 */
#define MIN_ARGS 0
#define MAX_ARGS 1
struct arguments
{
    char *args[MAX_ARGS+1];     /* positional args */

    int print_role;
    int repeat;                 /* repeat time, in seconds (0 remove subscription) */
    char *url;
    char *azp;
    char *yuno_role;
    char *yuno_name;
    char *yuno_service;

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
};

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "ytests"
#define APP_DOC         "Yuneta Tests"

#define APP_VERSION     __yuneta_version__
#define APP_DATETIME    __DATE__ " " __TIME__
#define APP_SUPPORT     "<support at artgins.com>"

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'realm_owner': 'agent',                                     \n\
        'work_dir': '/yuneta',                                      \n\
        'domain_dir': 'realms/agent/ytests'                         \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'yuno_role': 'ytests',                                      \n\
        'tags': ['yuneta', 'core']                                  \n\
    }                                                               \n\
}                                                                   \n\
";
PRIVATE char variable_config[]= "\
{                                                                   \n\
    '__json_config_variables__': {                                  \n\
    },                                                              \n\
    'environment': {                                                \n\
        'use_system_memory': true,                                  \n\
        'log_gbmem_info': true,                                     \n\
        'MEM_MIN_BLOCK': 512,                                       \n\
        'MEM_MAX_BLOCK': 52428800,              #^^  50*M           \n\
        'MEM_SUPERBLOCK': 52428800,             #^^  50*M           \n\
        'MEM_MAX_SYSTEM_MEMORY': 2147483648,     #^^ 2*G            \n\
        'console_log_handlers': {                                   \n\
            'to_stdout': {                                          \n\
                'handler_type': 'stdout',                           \n\
                'handler_options': 15        #^^ 15 en prod         \n\
            },                                                      \n\
            'to_udp': {                                             \n\
                'handler_type': 'udp',                              \n\
                'url': 'udp://127.0.0.1:1992',                      \n\
                'handler_options': 255                              \n\
            },                                                      \n\
            'to_file': {                                            \n\
                'handler_type': 'file',                             \n\
                'filename_mask': 'ytests-W.log',                \n\
                'handler_options': 255                              \n\
            }                                                       \n\
        }                                                           \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'timeout': 200,                                             \n\
        'trace_levels': {                                           \n\
            'Tcp0': ['connections']                                 \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'ytests',                                       \n\
            'gclass': 'YTests',                                     \n\
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

// Set by yuneta_entry_point()
// const char *argp_program_version = APP_NAME " " APP_VERSION;
// const char *argp_program_bug_address = APP_SUPPORT;

/* Program documentation. */
static char doc[] = APP_DOC;

/* A description of the arguments we accept. */
static char args_doc[] = "FILE";

/*
 *  The options we understand.
 *  See https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
 */
static struct argp_option options[] = {
/*-name-------------key-----arg---------flags---doc-----------------group */
{0,                 0,      0,          0,      "OAuth2 keys", 20},
{"auth_system",     'K',    "AUTH_SYSTEM",0,    "OpenID System(default: keycloak, to get now a jwt)", 20},
{"auth_url",        'k',    "AUTH_URL", 0,      "OpenID Endpoint (to get now a jwt)", 20},
{"azp",             'Z',    "AZP",      0,      "azp (Authorized Party, client_id in keycloak)", 20},
{"user_id",         'x',    "USER_ID",  0,      "OAuth2 User Id (to get now a jwt)", 20},
{"user_passw",      'X',    "USER_PASSW",0,     "OAuth2 User Password (to get now a jwt)", 20},
{"jwt",             'j',    "JWT",      0,      "Jwt (previously got it)", 21},

{0,                 0,      0,          0,      "Connection keys", 30},
{"url",             'u',    "URL",      0,      "Url to connect. Default: 'ws://127.0.0.1:1991'.", 30},
{"yuno_role",       'O',    "ROLE",     0,      "Remote yuno role. Default: ''", 30},
{"yuno_name",       'o',    "NAME",     0,      "Remote yuno name. Default: ''", 30},
{"yuno_service",    'S',    "SERVICE",  0,      "Remote yuno service. Default: '__default_service__'", 30},

{0,                 0,      0,          0,      "Local keys", 40},
{"repeat",          't',    "TIMES",    0,      "Repeat execution 'repeat' times. Set -1 to infinite loop. Default: 1", 1},
{"print",           'p',    0,          0,      "Print configuration.", 40},
{"config-file",     'f',    "FILE",     0,      "load settings from json config file or [files]", 40},
{"print-role",      'r',    0,          0,      "print the basic yuno's information"},
{"verbose",         'l',    "LEVEL",    0,      "Verbose level.", 40},
{"version",         'v',    0,          0,      "Print version.", 40},
{"yuneta-version",  'V',    0,          0,      "Print yuneta version", 40},
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

    case 't':
        if(arg) {
            arguments->repeat = atoi(arg);
        }
        break;

    case 'u':
        arguments->url = arg;
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
static void register_yuno_and_more(void)
{
    /*------------------------*
     *  Register yuneta-tls
     *------------------------*/
    yuneta_register_c_tls();

    /*-------------------*
     *  Register yuno
     *-------------------*/
    register_yuno_ytests();

    /*--------------------*
     *  Register service
     *--------------------*/
    gobj_register_gclass(GCLASS_YTESTS);
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
    arguments.repeat = 1;
    arguments.url = "ws://127.0.0.1:1991";
    arguments.azp = "";
    arguments.yuno_role = "yuneta_agent";
    arguments.yuno_name = "";
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
    char *argvs[]= {argv0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    memset(argv0, 0, sizeof(argv0));
    strncpy(argv0, argv[0], sizeof(argv0)-1);
    int idx = 1;

    /*
     *  Parse arguments
     */
    argp_parse (&argp, argc, argv, 0, 0, &arguments);
    char *path = arguments.args[0];
    if(!path) {
        path = "";
    }

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
    if(arguments.use_config_file) {
        int l = strlen("--config-file=") + strlen(arguments.config_json_file) + 4;
        char *param2 = malloc(l);
        snprintf(param2, l, "--config-file=%s", arguments.config_json_file);
        argvs[idx++] = param2;
    } else {
        json_t *kw_utility = json_pack(
            "{s:{s:i, s:i, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}}",
            "global",
            "YTests.repeat", arguments.repeat,
            "YTests.verbose", arguments.verbose,
            "YTests.path", path,
            "YTests.auth_system", arguments.auth_system,
            "YTests.auth_url", arguments.auth_url,
            "YTests.user_id", arguments.user_id,
            "YTests.user_passw", arguments.user_passw,
            "YTests.jwt", arguments.jwt,
            "YTests.url", arguments.url,
            "YTests.azp", arguments.azp,
            "YTests.yuno_role", arguments.yuno_role,
            "YTests.yuno_name", arguments.yuno_name,
            "YTests.yuno_service", arguments.yuno_service
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
#ifndef CONFIG_BUILD_TYPE_RELEASE
    static uint32_t mem_list[] = {0};
    gbmem_trace_alloc_free(0, mem_list);
#endif

    if(arguments.verbose > 0) {
        gobj_set_gclass_trace(GCLASS_IEVENT_CLI, "ievents", TRUE);
        gobj_set_gclass_trace(GCLASS_IEVENT_CLI, "kw", TRUE);
    }
    if(arguments.verbose > 1) {
        gobj_set_gclass_trace(GCLASS_TCP0, "traffic", TRUE);
    }
    if(arguments.verbose > 2) {
        gobj_set_gobj_trace(0, "machine", TRUE, 0);
        gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
        gobj_set_gobj_trace(0, "subscriptions", TRUE, 0);
        gobj_set_gobj_trace(0, "create_delete", TRUE, 0);
        gobj_set_gobj_trace(0, "start_stop", TRUE, 0);
    }
    gobj_set_gclass_no_trace(GCLASS_TIMER, "machine", TRUE);

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
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
        variable_config,
        register_yuno_and_more
    );
}
