/****************************************************************************
 *          MAIN_EMU_DEVICE.C
 *          emu_device main
 *
 *          Emulator of device gates
 *
 *          Copyright (c) 2018 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <argp.h>
#include <unistd.h>
#include <yuneta.h>
#include "c_emu_device.h"
#include "yuno_emu_device.h"

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

    char *url;
    char *path;
    char *database;
    char *topic;
    char *leading;

    char *from_t;
    char *to_t;

    char *from_rowid;
    char *to_rowid;

    char *user_flag_mask_set;
    char *user_flag_mask_notset;

    char *system_flag_mask_set;
    char *system_flag_mask_notset;

    char *key;
    char *notkey;

    char *window;
    char *interval;

    int print_role;
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
#define APP_NAME        "emu_device"
#define APP_DOC         "Emulator of device gates"

#define APP_VERSION     __yuneta_version__
#define APP_SUPPORT     "<niyamaka@yuneta.io>"
#define APP_DATETIME    __DATE__ " " __TIME__

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': '"ROLE_EMU_DEVICE"',                           \n\
        'tags': ['yuneta', 'utils']                                 \n\
    }                                                               \n\
}                                                                   \n\
";
PRIVATE char variable_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'use_system_memory': true,                                  \n\
        'log_gbmem_info': true,                                     \n\
        'MEM_MIN_BLOCK': 512,                                       \n\
        'MEM_MAX_BLOCK': 209715200,              #^^  200*M         \n\
        'MEM_SUPERBLOCK': 209715200,             #^^  200*M         \n\
        'MEM_MAX_SYSTEM_MEMORY': 2147483648,     #^^ 2*G            \n\
        'console_log_handlers': {                                   \n\
            'to_stdout': {                                          \n\
                'handler_type': 'stdout',                           \n\
                'handler_options': 255                              \n\
            },                                                      \n\
            'to_udp': {                                             \n\
                'handler_type': 'udp',                              \n\
                'url': 'udp://127.0.0.1:1992',                      \n\
                'handler_options': 255                              \n\
            }                                                       \n\
        },                                                          \n\
        'daemon_log_handlers': {                                    \n\
            'to_file': {                                            \n\
                'handler_type': 'file',                             \n\
                'filename_mask': 'emu_device-W.log',            \n\
                'handler_options': 255                              \n\
            },                                                      \n\
            'to_udp': {                                             \n\
                'handler_type': 'udp',                              \n\
                'url': 'udp://127.0.0.1:1992',                      \n\
                'handler_options': 255                              \n\
            }                                                       \n\
        }                                                           \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'required_services': [],                                    \n\
        'public_services': [],                                      \n\
        'service_descriptor': {                                     \n\
        },                                                          \n\
        'trace_levels': {                                           \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
        'Tcp0.output_priority': 1,                                  \n\
        'Connex.timeout_between_connections':10                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'emu_device',                                   \n\
            'gclass': 'Emu_device',                                 \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'zchilds': [                                            \n\
            ]                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__output_side__',                              \n\
            'gclass': 'IOGate',                                     \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'zchilds': [                                            \n\
                {                                                   \n\
                    'name': 'output',                               \n\
                    'gclass': 'Channel',                            \n\
                    'zchilds': [                                    \n\
                        {                                           \n\
                            'name': 'output',                       \n\
                            'gclass': 'Prot_raw',                   \n\
                            'zchilds': [                            \n\
                                {                                   \n\
                                    'name': 'output',               \n\
                                    'gclass': 'Connex',             \n\
                                    'kw': {                         \n\
                                        'urls':[                    \n\
                                        ]                           \n\
                                    }                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
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
static char args_doc[] = "FILE";

/*
 *  The options we understand.
 *  See https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
 */
static struct argp_option options[] = {
/*-name-------------key-----arg---------flags---doc-----------------group */
{0,                     0,      0,                  0,      "Database",         2},
{"url",                 'u',    "URL",              0,      "Url of yuno to connect.",2},
{"path",                'a',    "PATH",             0,      "Path.",            2},
{"database",            'b',    "DATABASE",         0,      "Database.",        2},
{"topic",               'c',    "TOPIC",            0,      "Topic.",           2},
{"leading",             'd',    "LEADING",          0,      "Leading data, coded in base64, sent once on connection.",          2},

{0,                     0,      0,                  0,      "Search conditions", 4},
{"from-t",              1,      "TIME",             0,      "From time.",       4},
{"to-t",                2,      "TIME",             0,      "To time.",         4},
{"from-rowid",          4,      "TIME",             0,      "From rowid.",      5},
{"to-rowid",            5,      "TIME",             0,      "To rowid.",        5},

{"user-flag-set",       9,      "MASK",             0,      "Mask of User Flag set.",   6},
{"user-flag-not-set",   10,     "MASK",             0,      "Mask of User Flag not set.",6},

{"system-flag-set",     13,     "MASK",             0,      "Mask of System Flag set.",   7},
{"system-flag-not-set", 14,     "MASK",             0,      "Mask of System Flag not set.",7},

{"key",                 21,     "KEY",              0,      "Key.",             9},
{"not-key",             22,     "KEY",              0,      "Not key.",         9},

{0,                     0,      0,                  0,      "Injection speed", 10},
{"window",              'w',    "NUMBER",           0,      "Number of messages to send in each interval (default 1).", 10},
{"interval",            'i',    "MILISECONDS",      0,      "Interval in miliseconds to send 'window' frames (default 1000).", 10},

{0,                     0,      0,                  0,      "Local keys", 11},
{"print",               'p',    0,                  0,      "Print configuration.", 11},
{"config-file",         'f',    "FILE",             0,      "load settings from json config file or [files]", 11},
{"print-role",          'r',    0,                  0,      "print the basic yuno's information", 11},
{"verbose",             'l',    "LEVEL",            0,      "Verbose level.", 11},
{"version",             'v',    0,                  0,      "Print program version.", 11},
{"yuneta-version",      'V',    0,                  0,      "Print yuneta version", 11},
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
    case 'u':
        arguments->url= arg;
        break;
    case 'a':
        arguments->path= arg;
        break;
    case 'b':
        arguments->database= arg;
        break;
    case 'c':
        arguments->topic= arg;
        break;
    case 'd':
        arguments->leading= arg;
        break;

    case 1: // from_t
        arguments->from_t = arg;
        break;
    case 2: // to_t
        arguments->to_t = arg;
        break;

    case 4: // from_rowid
        arguments->from_rowid = arg;
        break;
    case 5: // to_rowid
        arguments->to_rowid = arg;
        break;

    case 9:
        arguments->user_flag_mask_set = arg;
        break;
    case 10:
        arguments->user_flag_mask_notset = arg;
        break;

    case 13:
        arguments->system_flag_mask_set = arg;
        break;
    case 14:
        arguments->system_flag_mask_notset = arg;
        break;

    case 21:
        arguments->key = arg;
        break;
    case 22:
        arguments->notkey = arg;
        break;

    case 'w':
        arguments->window = arg;
        break;
    case 'i':
        arguments->interval = arg;
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
    /*-------------------*
     *  Register yuno
     *-------------------*/
    register_yuno_emu_device();

    /*--------------------*
     *  Register service
     *--------------------*/
    gobj_register_gclass(GCLASS_EMU_DEVICE);
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
        json_t *kw_utility = json_pack("{s:{}}",
            "global"
        );
        json_t *kw_emu_device = kw_get_dict_value(kw_utility, "global", 0, 0);

        if(arguments.url) {
            json_object_set_new(kw_emu_device, "Emu_device.url", json_string(arguments.url));
        }
        if(arguments.path) {
            json_object_set_new(kw_emu_device, "Emu_device.path", json_string(arguments.path));
        }
        if(arguments.database) {
            json_object_set_new(kw_emu_device, "Emu_device.database", json_string(arguments.database));
        }
        if(arguments.topic) {
            json_object_set_new(kw_emu_device, "Emu_device.topic", json_string(arguments.topic));
        }
        if(arguments.leading) {
            json_object_set_new(kw_emu_device, "Emu_device.leading", json_string(arguments.leading));
        }

        if(arguments.from_t) {
            json_object_set_new(kw_emu_device, "Emu_device.from_t", json_string(arguments.from_t));
        }
        if(arguments.to_t) {
            json_object_set_new(kw_emu_device, "Emu_device.to_t", json_string(arguments.to_t));
        }

        if(arguments.from_rowid) {
            json_object_set_new(
                kw_emu_device, "Emu_device.from_rowid", json_string(arguments.from_rowid));
        }
        if(arguments.to_rowid) {
            json_object_set_new(
                kw_emu_device, "Emu_device.to_rowid", json_string(arguments.to_rowid));
        }

        if(arguments.user_flag_mask_set) {
            json_object_set_new(
                kw_emu_device,
                "Emu_device.user_flag_mask_set",
                json_string(arguments.user_flag_mask_set)
            );
        }
        if(arguments.user_flag_mask_notset) {
            json_object_set_new(
                kw_emu_device,
                "Emu_device.user_flag_mask_notset",
                json_string(arguments.user_flag_mask_notset)
            );
        }

        if(arguments.system_flag_mask_set) {
            json_object_set_new(
                kw_emu_device,
                "Emu_device.system_flag_mask_set",
                json_string(arguments.system_flag_mask_set)
            );
        }
        if(arguments.system_flag_mask_notset) {
            json_object_set_new(
                kw_emu_device,
                "Emu_device.system_flag_mask_notset",
                json_string(arguments.system_flag_mask_notset)
            );
        }

        if(arguments.key) {
            json_object_set_new(
                kw_emu_device,
                "Emu_device.key",
                json_string(arguments.key)
            );
        }
        if(arguments.notkey) {
            json_object_set_new(
                kw_emu_device,
                "Emu_device.notkey",
                json_string(arguments.notkey)
            );
        }
        if(arguments.window) {
            json_object_set_new(
                kw_emu_device,
                "Emu_device.window",
                json_string(arguments.window)
            );
        }
        if(arguments.interval) {
            json_object_set_new(
                kw_emu_device,
                "Emu_device.interval",
                json_string(arguments.interval)
            );
        }

        char *param1_ = json_dumps(kw_utility, JSON_COMPACT);
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
    static uint32_t mem_list[] = {0};
    gbmem_trace_alloc_free(0, mem_list);
#endif

    if(arguments.verbose > 0) {
        gobj_set_gclass_trace(GCLASS_EMU_DEVICE, "info", TRUE);
    }
    if(arguments.verbose > 1) {
        gobj_set_gclass_trace(GCLASS_IEVENT_CLI, "ievents", TRUE);
        gobj_set_gclass_trace(GCLASS_IEVENT_CLI, "kw", TRUE);
    }
    if(arguments.verbose > 2) {
        gobj_set_gclass_trace(GCLASS_TCP0, "traffic", TRUE);
    }
    if(arguments.verbose > 3) {
        gobj_set_gobj_trace(0, "machine", TRUE, 0);
        gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
        gobj_set_gobj_trace(0, "subscriptions", TRUE, 0);
        gobj_set_gobj_trace(0, "create_delete", TRUE, 0);
        gobj_set_gobj_trace(0, "start_stop", TRUE, 0);
    }
    gobj_set_gclass_no_trace(GCLASS_TIMER, "machine", TRUE);

//     set_auto_kill_time(10);

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
