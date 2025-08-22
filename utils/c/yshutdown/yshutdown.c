/****************************************************************************
 *          YSHUTDOWN.C
 *
 *          Shutdown all Yuneta processes, including the agent
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <argp-standalone.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <yunetas.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define NAME        "yshutdown"
#define DOC         "Shutdown all Yuneta processes, including the agent"

#define APP_VERSION     YUNETA_VERSION
#define APP_DATETIME    __DATE__ " " __TIME__
#define APP_SUPPORT     "<support at artgins.com>"

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
    int no_kill_agent;
    int no_kill_system;
    int verbose;
};

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/***************************************************************************
 *      Data
 ***************************************************************************/
const char *argp_program_version = NAME " " APP_VERSION;
const char *argp_program_bug_address = APP_SUPPORT;

/* Program documentation. */
static char doc[] = DOC;

/* A description of the arguments we accept. */
static char args_doc[] = "";

/*
 *  The options we understand.
 *  See https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
 */
static struct argp_option options[] = {
/*-name-------------key-----arg---------flags---doc-----------------group */
{"verbose",         'l',    0,          0,      "Verbose mode.", 0},
{"no-kill-agent",   'n',    0,          0,      "Don't kill Yuneta agent.", 0},
{"no-kill-system",  's',    0,          0,      "Don't kill system's yunos (logcenter).", 0},
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

int no_kill_system;

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
    case 'n':
        arguments->no_kill_agent = 1;
        break;

    case 's':
        arguments->no_kill_system = 1;
        no_kill_system = 1;
        break;

    case 'l':
        arguments->verbose = 1;
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
 *
 ***************************************************************************/
static int kill_yuno(const char *directory, const char *pidfile, int verbose)
{
    int pid = 0;
    FILE *file = fopen(pidfile, "r");
    if(!file) {
        return -1;
    }
    int x = fscanf(file, "%d", &pid);
    if(x) {} // avoid warning
    fclose(file);

    int ret = kill(pid, SIGKILL);
    if(ret == 0) {
        unlink(pidfile);
        if(verbose) {
            printf("Pid %d, killed ('%s')\n", pid, pidfile);
        }
    } else if(errno == ESRCH) {
        unlink(pidfile);
    } else {
        printf("Pid %d, cannot kill ('%s'). Error '%s'\n", pid, pidfile, strerror(errno));
        return -1;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
static BOOL find_yuno_pid_cb(
    hgobj gojb,
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[255]
    int level,              // level of tree where file found
    wd_option opt               // index of file inside of directory, relative to 0
)
{
    int verbose = (int)(size_t)user_data;
    if(no_kill_system) {
        if (strstr(fullpath, "logcenter")!=0
        ) {
            return TRUE;
        }
    }

    kill_yuno(directory, fullpath, verbose);
    return TRUE; // to continue
}

static int shutdown_yuneta(int no_kill_agent, int verbose)
{
    walk_dir_tree(
        0,
        "/yuneta/realms",
        "yuno.pid",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        find_yuno_pid_cb,
        (void *)(uintptr_t)verbose
    );
    if(!no_kill_agent) {
        kill_yuno("/yuneta/agent", "/yuneta/realms/agent/yuneta_agent.pid", verbose);
        usleep(100);
        int x = system("killall -9 yuneta_agent > /dev/null 2>&1"); // Sometimes the agent is not killed, be sure!
        if(x) {} // avoid warning
    }
    return 0;
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
    arguments.no_kill_agent = 0;
    arguments.verbose = 0;

    /*
     *  Parse arguments
     */
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    /*
     *  Do your work
     */
    return shutdown_yuneta(arguments.no_kill_agent, arguments.verbose);
}
