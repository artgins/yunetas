/****************************************************************************
 *          entry_point.c
 *
 *          Entry point for yunos (yuneta daemons).
 *
 *          Copyright (c) 2014-2018 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <argp.h>

#include <command_parser.h>
#include <stats_parser.h>
#include <json_config.h>
#include <helpers.h>
#include <kwid.h>
#include <yev_loop.h>
#include <log_udp_handler.h>
#include <gobj_environment.h>
#include <stacktrace_with_backtrace.h>
// #include <stacktrace_with_backtrace.h>
#include <yuneta_version.h>
#include <rotatory.h>

#include "yunetas_register.h"
#include "yunetas_environment.h"
#include "dbsimple.h"
#include "ydaemon.h"
#include "c_authz.h"         // the grandmother
#include "c_yuno.h"         // the grandmother
#include "entry_point.h"

#include "../../gobj-c/src/stacktrace_with_backtrace.h"

/***************************************************************************
 *      Constants
 ***************************************************************************/
#define PREFIX_TEST_APP "test_" /* BUG The apps with this prefix don't get the limit of 15 bytes in APP_NAME */

#define KW_GET(__name__, __default__, __func__) \
    __name__ = __func__(0, kw, #__name__, __default__, 0);

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE hgobj __yuno_gobj__ = 0;
PRIVATE char __realm_id__[NAME_MAX] = {0};
PRIVATE char __node_owner__[NAME_MAX] = {0};
PRIVATE char __realm_owner__[NAME_MAX] = {0};
PRIVATE char __realm_role__[NAME_MAX] = {0};
PRIVATE char __realm_name__[NAME_MAX] = {0};
PRIVATE char __realm_env__[NAME_MAX] = {0};
PRIVATE char __yuno_role__[NAME_MAX] = {0};
PRIVATE char __yuno_id__[NAME_MAX] = {0};
PRIVATE char __yuno_name__[NAME_MAX] = {0};
PRIVATE char __yuno_tag__[NAME_MAX] = {0};

PRIVATE char __app_name__[NAME_MAX] = {0};
PRIVATE char __yuno_version__[NAME_MAX] = {0};
PRIVATE char __argp_program_version__[NAME_MAX] = {0};
PRIVATE char __app_doc__[NAME_MAX] = {0};
PRIVATE char __app_datetime__[NAME_MAX] = {0};
PRIVATE char __process_name__[64] = {0};


PRIVATE json_t *__jn_config__ = 0;

PRIVATE int __auto_kill_time__ = 0;
PRIVATE int __as_daemon__ = 0;
PRIVATE int __assure_kill_time__ = 5;
PRIVATE BOOL __quick_death__ = 0;
PRIVATE BOOL __ordered_death__ = 1;  // WARNING Vamos a probar otra vez las muertes ordenadas 22/03/2017

PRIVATE int __print__ = 0;

PRIVATE int (*__startup_persistent_attrs_fn__)(void) = 0;
PRIVATE void (*__end_persistent_attrs_fn__)(void) = 0;
PRIVATE int (*__load_persistent_attrs_fn__)(hgobj gobj, json_t *jn_attrs) = db_load_persistent_attrs;
PRIVATE int (*__save_persistent_attrs_fn__)(hgobj gobj, json_t *jn_attrs) = db_save_persistent_attrs;
PRIVATE int (*__remove_persistent_attrs_fn__)(hgobj gobj, json_t *jn_attrs) = db_remove_persistent_attrs;
PRIVATE json_t * (*__list_persistent_attrs_fn__)(hgobj gobj, json_t *jn_attrs) = db_list_persistent_attrs;

PRIVATE json_function_t __command_parser_fn__ = command_parser;
PRIVATE json_function_t __stats_parser_fn__ = stats_parser;

PRIVATE authz_checker_fn __authz_checker_fn__ = authz_checker;
PRIVATE authenticate_parser_fn __authenticate_parser_fn__ = authenticate_parser;

uint64_t MEM_MIN_BLOCK = 512;                     /* smaller memory block */
uint64_t MEM_MAX_BLOCK = 16*1024LL*1024LL;         /* largest memory block */
uint64_t MEM_SUPERBLOCK = 16*1024LL*1024LL;        /* super-block size */
uint64_t MEM_MAX_SYSTEM_MEMORY = 64*1024LL*1024LL; /* maximum core memory */

BOOL USE_OWN_SYSTEM_MEMORY = FALSE;
BOOL DEBUG_MEMORY = FALSE;

/***************************************************************************
 *      Structures
 ***************************************************************************/
/* Used by main to communicate with parse_opt. */
struct arguments {
    int start;
    int stop;
    int use_config_file;
    int print_verbose_config;
    int print_final_config;
    int print_role;
    const char *config_json_file;
    const char *parameter_config;
    int verbose_log;
};

/***************************************************************************
 *      Prototypes
 ***************************************************************************/
PRIVATE void daemon_catch_signals(void);
PRIVATE error_t parse_opt(int key, char *arg, struct argp_state *state);
PRIVATE void process(
    const char *process_name,
    const char *work_dir,
    const char *domain_dir,
    void (*cleaning_fn)(void)
);


/***************************************************************************
 *                      argp setup
 ***************************************************************************/
const char *argp_program_bug_address;                               // Public for argp
const char *argp_program_version = __argp_program_version__;        // Public for argp

/* A description of the arguments we accept. */
PRIVATE char args_doc[] = "[{json config}]";

/* The options we understand. */
PRIVATE struct argp_option options[] = {
{"start",                   'S',    0,      0,  "Start the yuno (as daemon)",   0},
{"stop",                    'K',    0,      0,  "Stop the yuno (as daemon)",    0},
{"config-file",             'f',    "FILE", 0,  "Load settings from json config file or [files]", 0},
{"print-config",            'p',    0,      0,  "Print the final json config", 0},
{"print-verbose-config",    'P',    0,      0,  "Print verbose json config", 0},
{"print-role",              'r',    0,      0,  "Print the basic yuno's information", 0},
{"version",                 'v',    0,      0,  "Print yuno version", 0},
{"yuneta-version",          'V',    0,      0,  "Print yuneta version", 0},
{"verbose-log",             'l',    "LEVEL",0,  "Verbose log level", 0},
{0}
};

/* Our argp parser. */
PRIVATE struct argp argp = {
    options,
    parse_opt,
    args_doc,
    __app_doc__,
    0, 0, 0
};

/***************************************************************************
 *      argp parser
 ***************************************************************************/
PRIVATE error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    /*
     *  Get the input argument from argp_parse,
     *  which we know is a pointer to our arguments structure.
     */
    struct arguments *arguments = state->input;

    switch (key) {
    case 'S':
        arguments->start = 1;
        break;
    case 'K':
        arguments->stop = 1;
        break;
    case 'v':
        printf("%s\n", argp_program_version);
        exit(0);
    case 'V':
        printf("%s\n", YUNETA_VERSION);
        exit(0);
    case 'f':
        arguments->config_json_file = arg;
        arguments->use_config_file = 1;
        break;
    case 'p':
        arguments->print_final_config = 1;
        break;
    case 'r':
        arguments->print_role = 1;
        break;
    case 'P':
        arguments->print_verbose_config = 1;
        break;
    case 'l':
        if(arg) {
            arguments->verbose_log = atoi(arg);
        }
        break;

    case ARGP_KEY_ARG:
        if (state->arg_num >= 1) {
            /* Too many arguments. */
            argp_usage (state);
        }
        arguments->parameter_config = arg;
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int tries = 0;

    /*
     *  __yuno_gobj__ is 0 for watcher fork if we are running --stop
     */
    hgobj gobj = __yuno_gobj__;

    if(gobj) {
        if(__quick_death__ && !__ordered_death__) {
            _exit(0);
        }
        tries++;
        gobj_set_yuno_must_die();

        if(!__auto_kill_time__) {
            if(__assure_kill_time__ > 0)
                alarm(__assure_kill_time__); // que se muera a la fuerza en 5 o 10 seg.
        }
        if(tries > 1) {
            _exit(-1);
        }
        return;
    }

    print_error(PEF_SYSLOG, "Signal handler without __yuno_gobj__, signal %d, pid %d", sig, getpid());
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void raise_sighandler(int sig)
{
    /*
     *  __yuno_gobj__ is 0 for watcher fork, if we are running --stop
     */
    hgobj gobj = __yuno_gobj__;

    if(gobj) {
        gobj_set_deep_tracing(-1);
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_RUNTIME_ERROR,
            "msg",          "%s", "Kill -10",
            NULL
        );
    }
}

PRIVATE void daemon_catch_signals(void)
{
    struct sigaction sigIntHandler;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    memset(&sigIntHandler, 0, sizeof(sigIntHandler));
    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigaction(SIGALRM, &sigIntHandler, NULL);   // to debug in kdevelop
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);    // ctrl+c
//    sigaction(SIGTERM, &sigIntHandler, NULL);

    sigIntHandler.sa_handler = raise_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigaction(SIGUSR1, &sigIntHandler, NULL);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_process_name2(const char *role, const char *name)
{
    if(role && name) {
        snprintf(__process_name__, sizeof(__process_name__), "%s^%s", role, name);
    }
    return 0;
}
PRIVATE const char *get_process_name(void)
{
    return __process_name__;
}

/***************************************************************************
 *  New yuneta setup function.
 ***************************************************************************/
PUBLIC int yuneta_setup(
    int (*startup_persistent_attrs)(void),
    void (*end_persistent_attrs)(void),
    int (*load_persistent_attrs)(hgobj gobj, json_t *jn_attrs),
    int (*save_persistent_attrs)(hgobj gobj, json_t *jn_attrs),
    int (*remove_persistent_attrs)(hgobj gobj, json_t *jn_attrs),
    json_t * (*list_persistent_attrs)(hgobj gobj, json_t *jn_attrs),
    json_function_t command_parser,
    json_function_t stats_parser,
    authz_checker_fn authz_checker,
    authenticate_parser_fn authenticate_parser,
    BOOL use_own_system_memory,
    uint64_t mem_min_block,
    uint64_t mem_max_block,
    uint64_t mem_superblock,
    uint64_t mem_max_system_memory,
    BOOL debug_memory
)
{
    if(startup_persistent_attrs) {
        __startup_persistent_attrs_fn__ = startup_persistent_attrs;
    }
    if(end_persistent_attrs) {
        __end_persistent_attrs_fn__ = end_persistent_attrs;
    }
    if(load_persistent_attrs) {
        __load_persistent_attrs_fn__ = load_persistent_attrs;
    }
    if(save_persistent_attrs) {
        __save_persistent_attrs_fn__ = save_persistent_attrs;
    }
    if(remove_persistent_attrs) {
        __remove_persistent_attrs_fn__ = remove_persistent_attrs;
    }
    if(list_persistent_attrs) {
        __list_persistent_attrs_fn__ = list_persistent_attrs;
    }
    if(command_parser) {
        __command_parser_fn__ = command_parser;
    }
    if(stats_parser) {
        __stats_parser_fn__ = stats_parser;
    }
    if(authz_checker) {
        __authz_checker_fn__ = authz_checker;
    }
    if(authenticate_parser) {
        __authenticate_parser_fn__ = authenticate_parser;
    }

    USE_OWN_SYSTEM_MEMORY = use_own_system_memory;

    if(mem_min_block) {
        MEM_MIN_BLOCK = mem_min_block;
    }
    if(mem_max_block) {
        MEM_MAX_BLOCK = mem_max_block;
    }
    if(mem_superblock) {
        MEM_SUPERBLOCK = mem_superblock;
    }
    if(mem_max_system_memory) {
        MEM_MAX_SYSTEM_MEMORY = mem_max_system_memory;
    }

    DEBUG_MEMORY = debug_memory;

    return 0;
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
PUBLIC int yuneta_entry_point(int argc, char *argv[],
    const char *APP_NAME,
    const char *APP_VERSION,
    const char *APP_SUPPORT,
    const char *APP_DOC,
    const char *APP_DATETIME,
    const char *fixed_config,
    const char *variable_config,
    void (*register_yuno_and_more)(void), // HACK This function is executed on yunetas environment (mem, log, paths) BEFORE creating the yuno
    void (*cleaning_fn)(void) // HACK This function is executed after free all yuneta resources
) {
    snprintf(__argp_program_version__, sizeof(__argp_program_version__),
        "%s %s %s",
        APP_NAME,
        APP_VERSION,
        APP_DATETIME
    );
    argp_program_bug_address = APP_SUPPORT;
    strncpy(__yuno_version__, APP_VERSION, sizeof(__yuno_version__)-1);
    strncpy(__app_name__, APP_NAME, sizeof(__app_name__)-1);
    strncpy(__app_datetime__, APP_DATETIME, sizeof(__app_datetime__)-1);
    strncpy(__app_doc__, APP_DOC, sizeof(__app_doc__)-1);

    /*------------------------------------------------*
     *  Process name = yuno role
     *  This name is limited to 15 characters.
     *      - 15+null by Linux (longer names are not showed in top)
     *------------------------------------------------*/
    if(strlen(APP_NAME) > 15) {
        if(strncmp(APP_NAME, PREFIX_TEST_APP, strlen(PREFIX_TEST_APP))!=0) {
            print_error(
                PEF_EXIT,
                "role name '%s' TOO LONG!, maximum is 15 characters",
                APP_NAME
            );
        }
    }

    /*------------------------------------------------*
     *  Process name = yuno role
     *------------------------------------------------*/
    const char *process_name = APP_NAME;

#ifdef DEBUG
    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);
#endif

    /*------------------------------------------------*
     *          Parse input arguments
     *------------------------------------------------*/
    struct arguments arguments = {0};
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    if(arguments.stop) {
        daemon_shutdown(process_name);
        return 0;
    }
    if(arguments.start) {
        __as_daemon__ = 1;
        __quick_death__ = 1;
    }
    if(arguments.print_verbose_config ||
            arguments.print_final_config ||
            arguments.print_role) {
        __print__ = TRUE;
    }

    /*------------------------------------------------*
     *          Load json config
     *------------------------------------------------*/
    char *sconfig = json_config( // HACK I know that sconfig is malloc'ed
        arguments.print_verbose_config,     // WARNING if true will exit(0)
        arguments.print_final_config,       // WARNING if true will exit(0)
        fixed_config,
        variable_config,
        arguments.use_config_file? arguments.config_json_file: 0,
        arguments.parameter_config,
        PEF_EXIT
    );
    if(!sconfig) {
        print_error(
            PEF_EXIT,
            "json_config() of '%s' failed",
            APP_NAME
        );
    }

    /*------------------------------------------------*
     *          Setup memory
     *------------------------------------------------*/
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gobj_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

    /*---------------------------------------------------*
     *      WARNING now all json is gbmem allocated
     *---------------------------------------------------*/
    glog_init();
    rotatory_start_up();

    /*-------------------------------*
     *      Re-alloc with gbmem
     *-------------------------------*/
    __jn_config__ = legalstring2json(sconfig, TRUE); //
    if(!__jn_config__) {
        print_error(
            PEF_EXIT,
            "legalstring2json() of '%s' failed",
            APP_NAME
        );
    }
    free(sconfig);  // HACK I know that sconfig is malloc'ed

    /*------------------------------------------------*
     *          Load environment config
     *------------------------------------------------*/
    int xpermission = 02775;
    int rpermission = 0664;
    const char *work_dir = 0;           /* by default, total silence */
    const char *domain_dir = 0;         /* by default, total silence */

    json_t *jn_environment = kw_get_dict(0, __jn_config__, "environment", 0, 0);
    if(jn_environment) {
        xpermission = (int)kw_get_int(0, jn_environment, "xpermission", xpermission, 0);
        rpermission = (int)kw_get_int(0, jn_environment, "rpermission", rpermission, 0);
        work_dir = kw_get_str(0, jn_environment, "work_dir", 0, 0);
        domain_dir = kw_get_str(0, jn_environment, "domain_dir", 0, 0);
        register_yuneta_environment(
            work_dir,
            domain_dir,
            xpermission,
            rpermission
        );
    }

    /*------------------------------------------------*
     *  Init ginsfsm and yuneta
     *------------------------------------------------*/
    json_t *jn_global = kw_get_dict(0, __jn_config__, "global", 0, 0);

    gobj_start_up(
        argc,
        argv,
        jn_global,  // Not owned, it's duplicated
        __startup_persistent_attrs_fn__,
        __end_persistent_attrs_fn__,
        __load_persistent_attrs_fn__,
        __save_persistent_attrs_fn__,
        __remove_persistent_attrs_fn__,
        __list_persistent_attrs_fn__,
        __command_parser_fn__,
        __stats_parser_fn__,
        __authz_checker_fn__,
        __authenticate_parser_fn__,
        MEM_MAX_BLOCK,          // max_block, largest memory block
        MEM_MAX_SYSTEM_MEMORY   // max_system_memory, maximum system memory
    );

    /*------------------------------------------------*
     *  Init log handlers
     *------------------------------------------------*/
    if(jn_environment && !__print__) {
        json_t *jn_log_handlers = kw_get_dict(0,
            jn_environment,
            __as_daemon__? "daemon_log_handlers":"console_log_handlers",
            0,
            0
        );
        const char *key;
        json_t *kw;
        json_object_foreach(jn_log_handlers, key, kw) {
            const char *handler_type = kw_get_str(0, kw, "handler_type", "", 0);
            log_handler_opt_t handler_options = LOG_OPT_LOGGER;
            KW_GET(handler_options, handler_options, kw_get_int)
            if(strcmp(handler_type, "stdout")==0) {
                if(arguments.verbose_log) {
                    handler_options = arguments.verbose_log;
                }
                gobj_log_add_handler(key, handler_type, handler_options, 0); // HACK until now: no output

            } else if(strcmp(handler_type, "file")==0) {
                const char *filename_mask = kw_get_str(0, kw, "filename_mask", 0, 0);
                if(!empty_string(filename_mask)) {
                    char temp[NAME_MAX];
                    char *path = yuneta_log_file(temp, sizeof(temp), filename_mask, TRUE);
                    if(path) {
                        size_t bf_size = 0;                     // 0 = default 64K
                        int max_megas_rotatoryfile_size = 0;    // 0 = default 8 Megas
                        int min_free_disk_percentage = 0;       // 0 = default 10

                        KW_GET(bf_size, bf_size, kw_get_int)
                        KW_GET(max_megas_rotatoryfile_size, max_megas_rotatoryfile_size, kw_get_int)
                        KW_GET(min_free_disk_percentage, min_free_disk_percentage, kw_get_int)

                        hrotatory_t hr = rotatory_open(
                            path,
                            bf_size,
                            max_megas_rotatoryfile_size,
                            min_free_disk_percentage,
                            xpermission,
                            rpermission,
                            TRUE    // exit on failure
                        );
                        gobj_log_add_handler(key, handler_type, handler_options, hr);
                    }
                }

            } else if(strcmp(handler_type, "udp")==0 ||
                    strcmp(handler_type, "upd")==0) { // "upd" is a sintax bug in all versions <= 2.2.5
                const char *url = kw_get_str(0, kw, "url", 0, 0);
                if(!empty_string(url)) {
                    size_t bf_size = 0;                 // 0 = default 64K
                    size_t udp_frame_size = 0;          // 0 = default 1500
                    output_format_t output_format = 0;   // 0 = default OUTPUT_FORMAT_YUNETA
                    const char *bindip = 0;

                    KW_GET(bindip, bindip, kw_get_str)
                    KW_GET(bf_size, bf_size, kw_get_int)
                    KW_GET(udp_frame_size, udp_frame_size, kw_get_int)
                    KW_GET(output_format, output_format, kw_get_int)

                    udpc_t udpc = udpc_open(
                        url,
                        bindip,
                        "",
                        bf_size,
                        udp_frame_size,
                        output_format,
                        TRUE    // exit on failure
                    );
                    if(udpc) {
                        gobj_log_add_handler(key, "udp", handler_options, udpc);
                    }
                }
            }
        }
        jn_environment = 0; // protect, no more use
    }

    /*------------------------------------------------*
     *      Re-read
     *------------------------------------------------*/
    const char *realm_id  = kw_get_str(0, __jn_config__, "environment`realm_id", "", 0);
    const char *realm_owner  = kw_get_str(0, __jn_config__, "environment`realm_owner", "", 0);
    const char *realm_role  = kw_get_str(0, __jn_config__, "environment`realm_role", "", 0);
    const char *realm_name  = kw_get_str(0, __jn_config__, "environment`realm_name", "", 0);
    const char *realm_env  = kw_get_str(0, __jn_config__, "environment`realm_env", "", 0);
    const char *node_owner  = kw_get_str(0, __jn_config__, "environment`node_owner", "", 0);

    /*------------------------------------------------*
     *      Get yuno attributes.
     *------------------------------------------------*/
    json_t *jn_yuno = kw_get_dict(0, __jn_config__, "yuno", 0, 0);
    if(!jn_yuno) {
        print_error(
            PEF_EXIT,
            "'yuno' dict NOT FOUND in json config"
        );
    }

    const char *yuno_role  = kw_get_str(0, jn_yuno, "yuno_role", "", 0);
    const char *yuno_id  = kw_get_str(0, jn_yuno, "yuno_id", "", 0);
    const char *yuno_name  = kw_get_str(0, jn_yuno, "yuno_name", "", 0);
    const char *yuno_tag  = kw_get_str(0, jn_yuno, "yuno_tag", "", 0);
    if(empty_string(yuno_role)) {
        print_error(
            PEF_EXIT,
            "'yuno_role' is EMPTY"
        );
    }
    if(strcmp(yuno_role, APP_NAME)!=0) {
        print_error(
            PEF_EXIT,
            "yuno_role '%s' and process name '%s' MUST MATCH!",
            yuno_role,
            APP_NAME
        );
    }
    if(empty_string(yuno_name)) {
        yuno_name = APP_NAME;
    }

    snprintf(__node_owner__, sizeof(__node_owner__), "%s", node_owner);
    snprintf(__realm_id__, sizeof(__realm_id__), "%s", realm_id);
    snprintf(__realm_owner__, sizeof(__realm_owner__), "%s", realm_owner);
    snprintf(__realm_role__, sizeof(__realm_role__), "%s", realm_role);
    snprintf(__realm_name__, sizeof(__realm_name__), "%s", realm_name);
    snprintf(__realm_env__, sizeof(__realm_env__), "%s", realm_env);
    snprintf(__yuno_role__, sizeof(__yuno_role__), "%s", yuno_role);
    snprintf(__yuno_id__, sizeof(__yuno_id__), "%s", yuno_id);
    snprintf(__yuno_name__, sizeof(__yuno_name__), "%s", yuno_name);
    snprintf(__yuno_tag__, sizeof(__yuno_tag__), "%s", yuno_tag);

    set_process_name2(__yuno_role__, __yuno_name__);

    /*----------------------------------------------*
     *  Check executable name versus yuno_role
     *  Paranoid!
     *----------------------------------------------*/
    if(1) {
        char *xx = strdup(argv[0]);
        if(xx) {
            char *bname = basename(xx);
            char *p = strrchr(bname, '.');
            if(p && (strcmp(p, ".pm")==0 || strcasecmp(p, ".exe")==0)) {
                *p = 0; // delete extensions (VOS and Windows)
            }
            p = strchr(bname, '^');
            if(p)
                *p = 0; // delete name segment
            if(strcmp(bname, yuno_role)!=0) {
                print_error(
                    PEF_EXIT,
                    "yuno role '%s' and executable name '%s' MUST MATCH!",
                    yuno_role,
                    bname
                );
            }
            free(xx);
        }
    }

    /*------------------------------------------------*
     *  Print basic inform
     *------------------------------------------------*/
    if(arguments.print_role) {
        json_t *jn_tags = kw_get_dict_value(0, jn_yuno, "tags", 0, 0);
        json_t *jn_required_services = kw_get_dict_value(0, jn_yuno, "required_services", 0, 0);
        json_t *jn_public_services = kw_get_dict_value(0, jn_yuno, "public_services", 0, 0);
        json_t *jn_service_descriptor = kw_get_dict_value(0, jn_yuno, "service_descriptor", 0, 0);

        json_t *jn_basic_info = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s}",
            "role", __yuno_role__,
            "name", __yuno_name__,
            "alias", __yuno_tag__,
            "version", __yuno_version__,
            "date", __app_datetime__,
            "description", __app_doc__,
            "yuneta_version", YUNETA_VERSION
        );
        if(jn_basic_info) {
            size_t flags = JSON_INDENT(4);
            if(jn_tags) {
                json_object_set(jn_basic_info, "tags", jn_tags);
            } else {
                json_object_set_new(jn_basic_info, "tags", json_array());
            }
            if(jn_required_services) {
                json_object_set(jn_basic_info, "required_services", jn_required_services);
            } else {
                json_object_set_new(jn_basic_info, "required_services", json_array());
            }
            if(jn_public_services) {
                json_object_set(jn_basic_info, "public_services", jn_public_services);
            } else {
                json_object_set_new(jn_basic_info, "public_services", json_array());
            }
            if(jn_service_descriptor) {
                json_object_set(jn_basic_info, "service_descriptor", jn_service_descriptor);
            } else {
                json_object_set_new(jn_basic_info, "service_descriptor", json_array());
            }

            json_dumpf(jn_basic_info, stdout, flags);
            printf("\n");
            JSON_DECREF(jn_basic_info)
        }
        exit(0);
    }

    /*------------------------------------------------*
     *  Register gclasses
     *------------------------------------------------*/
    yunetas_register_c_core();

    if(register_yuno_and_more) {
        register_yuno_and_more();
    }

    /*------------------------------------------------*
     *          Run
     *------------------------------------------------*/
    if(__as_daemon__) {
        daemon_run(
            process,
            get_process_name(),
            work_dir,
            domain_dir,
            daemon_catch_signals,
            cleaning_fn
        );
    } else {
        daemon_catch_signals();
        if(__auto_kill_time__) {
            /* kill in x seconds, to debug exit in kdevelop */
            alarm(__auto_kill_time__);
        }
        process(get_process_name(), work_dir, domain_dir, cleaning_fn);
    }

    return gobj_get_exit_code();
}

/***************************************************************************
 *                      Process
 ***************************************************************************/
PRIVATE void process(
    const char *process_name,
    const char *work_dir,
    const char *domain_dir,
    void (*cleaning_fn)(void)
)
{
    gobj_log_info(0,0,
        "msgset",       "%s", MSGSET_START_STOP,
        "msg",          "%s", "Starting yuno",
        "work_dir",     "%s", work_dir,
        "domain_dir",   "%s", domain_dir,
        "node_owner",   "%s", __node_owner__,
        "realm_id",     "%s", __realm_id__,
        "realm_owner",  "%s", __realm_owner__,
        "realm_role",   "%s", __realm_role__,
        "realm_name",   "%s", __realm_name__,
        "realm_env",    "%s", __realm_env__,
        "yuno_role",    "%s", __yuno_role__,
        "id",           "%s", __yuno_id__,
        "yuno_name",    "%s", __yuno_name__,
        "yuno_tag",     "%s", __yuno_tag__,
        "version",      "%s", __yuno_version__,
        "date",         "%s", __app_datetime__,
        "pid",          "%d", getpid(),
        "watcher_pid",  "%d", get_watcher_pid(),
        "description",  "%s", __app_doc__,
        NULL
    );

    /*------------------------------------------------*
     *          Create main process yuno
     *------------------------------------------------*/
    json_t *kw_yuno = json_pack(
        "{s:i, s:i, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
        "pid",          getpid(),
        "watcher_pid",  get_watcher_pid(),
        "process",      get_process_name(),
        "hostname",     get_hostname(),
        "node_uuid",    node_uuid(),
        "node_owner",   __node_owner__,
        "realm_id",     __realm_id__,
        "realm_owner",  __realm_owner__,
        "realm_role",   __realm_role__,
        "realm_name",   __realm_name__,
        "realm_env",    __realm_env__,
        "yuno_id",      __yuno_id__,
        "yuno_tag",     __yuno_tag__,
        "yuno_role",    __yuno_role__,
        "yuno_name",    __yuno_name__,
        "yuno_version", __yuno_version__
    );

    json_object_update_new(kw_yuno,
        json_pack("{s:s, s:s, s:s, s:s, s:s}",
            "work_dir",     work_dir,
            "domain_dir",   domain_dir,
            "appName",      __app_name__,
            "appDesc",      __app_doc__,
            "appDate",      __app_datetime__
        )
    );
    json_t *jn_yuno = kw_get_dict(0,
        __jn_config__,
        "yuno",
        0,
        0
    );
    json_object_update_missing(kw_yuno, jn_yuno);

    hgobj yuno = __yuno_gobj__ = gobj_create_yuno(__yuno_name__, C_YUNO, kw_yuno);
    if(!yuno) {
        gobj_log_error(0,0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_RUNTIME_ERROR,
            "msg",          "%s", "gobj_create_yuno() FAILED",
            "role",         "%s", __yuno_role__,
            NULL
        );
        exit(0); // Exit with 0 to avoid that watcher restart yuno.
    }

    /*------------------------------------------------*
     *          Create services
     *------------------------------------------------*/
    json_t *jn_services = kw_get_list(0,
        __jn_config__,
        "services",
        0,
        0
    );
    if(jn_services) {
        size_t index;
        json_t *jn_service_tree;
        json_array_foreach(jn_services, index, jn_service_tree) {
            if(!json_is_object(jn_service_tree)) {
                gobj_log_error(yuno, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "service config MUST BE an json object",
                    "index",        "%d", index,
                    NULL
                );
                continue;
            }
            const char *service_name = kw_get_str(0, jn_service_tree, "name", 0, 0);
            if(empty_string(service_name)) {
                gobj_log_error(yuno, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "service without name",
                    "index",        "%d", index,
                    NULL
                );
                continue;
            }
            json_incref(jn_service_tree);
            if(!gobj_service_factory(service_name, jn_service_tree)) {
                gobj_log_error(yuno, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "gobj_service_factory() FAILED",
                    "service",      "%s", service_name,
                    NULL
                );
            }
        }
    }

    /*------------------------*
     *      Start main
     *------------------------*/
    gobj_start(yuno);

    /*---------------------------------*
     *      Auto services
     *---------------------------------*/
    gobj_autostart_services();
    if(gobj_read_bool_attr(yuno, "autoplay")) {
        gobj_play(yuno);    /* will play default_service */
    }
    gobj_autoplay_services();

    /*-----------------------------------*
     *      Run main event loop
     *-----------------------------------*/
    /*
     *  Forever loop. Returning is because someone order to stop with gobj_set_yuno_must_die()
     */
    yev_loop_run(yuno_event_loop(), -1);

    /*
     *  Shutdown
     */
    gobj_shutdown();

    yev_loop_run_once(yuno_event_loop());  // Give an opportunity to close
    yev_loop_stop(yuno_event_loop());
    yev_loop_run_once(yuno_event_loop());  // Give an opportunity to close

    /*---------------------------*
     *      End
     *---------------------------*/
    gobj_end(); // This destroy yuno
    rotatory_end();
    json_decref(__jn_config__);
    if(cleaning_fn) {
        cleaning_fn();
    }
    print_track_mem();
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void set_assure_kill_time(int seconds)
{
    __assure_kill_time__ = seconds;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void set_auto_kill_time(int seconds)
{
    __auto_kill_time__ = seconds;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void set_ordered_death(BOOL ordered_death)
{
    __ordered_death__ = ordered_death;
}
