/****************************************************************************
 *          YLIST.C
 *
 *          List yunos
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <argp-standalone.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <yunetas.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define NAME        "ylist"
#define DOC         "List yunos"

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
    int pids;                   /* only pids */
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
{"pids",        'p',    0,      0,   "Display only pids", 0},
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
    case 'p':
        arguments->pids = 1;
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
PRIVATE int list_yuno(const char *directory, const char *pidfile, struct arguments *arguments)
{
    int pid = 0;

    FILE *file = fopen(pidfile, "r");
    if(!file) {
        return -1;
    }
    (void)fscanf(file, "%d", &pid);
    fclose(file);

    int ret = kill(pid, 0);
    if(ret == 0) {
        if(arguments->pids) {
            printf("%d ", pid);
        } else {
            printf("Pid %d, path '%s'\n", pid, pidfile);
        }
    } else if(errno == ESRCH) {
        unlink(pidfile);
    } else {
        printf("Pid %d: cannot check ('%s'). Error '%s'\n", pid, pidfile, strerror(errno));
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL list_yuno_pid_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[255]
    int level,              // level of tree where file found
    wd_option opt           // option parameter
)
{
    struct arguments *arguments = user_data;
    list_yuno(directory, fullpath, arguments);
    return TRUE; // to continue
}

PRIVATE int ylist(struct arguments *arguments)
{
    walk_dir_tree(
        0,
        "/yuneta/realms",
        "yuno.pid",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        list_yuno_pid_cb,
        arguments
    );
    if(arguments->pids) {
        printf("\n");
    } else {
        list_yuno("/yuneta/agent", "/yuneta/realms/agent/yuneta_agent.pid", arguments);
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
    arguments.pids = 0;

    /*
     *  Parse arguments
     */
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    /*
     *  Do your work
     */
    return ylist(&arguments);
}
