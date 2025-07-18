/****************************************************************************
 *          YUNO-SKELETON.C
 *          Make skeleton of yunos ang gclasses.
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <argp.h>
#include <yuneta.h>
#include "make_skeleton.h"

/***************************************************************************
 *      Constants
 ***************************************************************************/
#define NAME            "yuno-skeleton"
#define APP_VERSION     __yuneta_version__
#define APP_DATETIME    __DATE__ " " __TIME__
#define APP_SUPPORT     "<support at artgins.com>"
#define DEFAULT_SKELETON_PATH "/yuneta/development/bin/skeletons"
#define DEFAULT_JSON_CONFIG "__skeletons__.json"

/***************************************************************************
 *      Structures
 ***************************************************************************/
#define MIN_ARGS 0
#define MAX_ARGS 1
/* Used by main to communicate with parse_opt. */
struct arguments
{
    char *args[MAX_ARGS+1];     /* positional args */
    int list_skeletons;         /* list skeletons */
    char *path;                 /* skeleton path*/
};

/***************************************************************************
 *      Prototypes
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/***************************************************************************
 *      Data
 ***************************************************************************/
const char *argp_program_version = NAME " " APP_VERSION;
const char *argp_program_bug_address = APP_SUPPORT;

/* Program documentation. */
static char doc[] =
  "yuno-skeleton -- a Yuneta utility to make yuno's skeletons and gclasses";

/* A description of the arguments we accept. */
static char args_doc[] = "SKELETON";

/* The options we understand. */
static struct argp_option options[] = {
{"list",            'l',    0,          0,   "List available skeletons", 1},
{"skeletons-path",  'p',    "PATH", 0,   "Path of skeletons, default is " DEFAULT_SKELETON_PATH, 1},
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
    case 'l':
        arguments->list_skeletons = 1;
        break;

    case 'p':
        arguments->path = arg;
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
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    struct arguments arguments;

    /*
     *  Default values
     */
    memset(&arguments, 0, sizeof(arguments));
    arguments.list_skeletons = 0;
    arguments.path = DEFAULT_SKELETON_PATH;

    /*
     *  Parse arguments
     */
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    if(arguments.list_skeletons) {
        list_skeletons(arguments.path, DEFAULT_JSON_CONFIG);
        exit(0);
    }
    if(!arguments.args[0]) {
        printf("\nEnter a skeleton please. Available list:\n");
        list_skeletons(arguments.path, DEFAULT_JSON_CONFIG);
        exit(-1);
    }

    make_skeleton(arguments.path, DEFAULT_JSON_CONFIG, arguments.args[0]);

    exit(0);
}
