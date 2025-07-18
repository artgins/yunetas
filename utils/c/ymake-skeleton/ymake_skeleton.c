/****************************************************************************
 *          YMAKE-SKELETON.C
 *          Make a new skeleton of a yuno.
 *
 *          Copyright (c) 2021 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <argp.h>
#include <yuneta.h>
#include "make_skeleton.h"

/***************************************************************************
 *      Constants
 ***************************************************************************/
#define NAME            "ymake-skeleton"
#define APP_VERSION     __yuneta_version__
#define APP_DATETIME    __DATE__ " " __TIME__
#define APP_SUPPORT     "<support at artgins.com>"

/***************************************************************************
 *      Structures
 ***************************************************************************/
#define MIN_ARGS 2
#define MAX_ARGS 2
/* Used by main to communicate with parse_opt. */
struct arguments
{
    char *args[MAX_ARGS+1];     /* positional args */
    char *yunorole;             /* yunorole keyword */
    char *rootname;             /* rootname keyword */
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
  "ymake-skeleton -- a Yuneta utility to make a new skeleton from a yuno project or gclass source code.\n"
"Ex: ymake-skeleton ./gate_frigo yuno_gate --yunorole=gate_frigo --rootname=gate_frigo,frigo";

/* A description of the arguments we accept. */
static char args_doc[] = "SOURCE-PROJECT DESTINATION-PROJECT";

/* The options we understand. */
static struct argp_option options[] = {
{"yunorole",        'r',    "KEYWORD",  0,   "YunoRole keyword", 1},
{"rootname",        'n',    "KEYWORD",  0,   "RootName keyword(s) (separated by commas if more than one)", 2},
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
    case 'r':
        arguments->yunorole = arg;
        break;

    case 'n':
        arguments->rootname = arg;
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

    /*
     *  Parse arguments
     */
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    if(empty_string(arguments.args[0])) {
        printf("Enter a source project path\n");
        exit(-1);
    }
    if(!is_directory(arguments.args[0])) {
        printf("Source project path not found: %s\n", arguments.args[0]);
        exit(-1);
    }

    if(access(arguments.args[1], 0)==0) {
        printf("Destination project path already exists: %s\n", arguments.args[1]);
        exit(-1);
    }

    if(empty_string(arguments.args[1])) {
        printf("Enter a destination project path\n");
        exit(-1);
    }

    if(empty_string(arguments.yunorole)) {
        printf("What yunorole keyword?\n");
        exit(-1);
    }
    if(empty_string(arguments.rootname)) {
        printf("What rootname keyword(s)?\n");
        exit(-1);
    }

    make_skeleton(arguments.args[0], arguments.args[1], arguments.yunorole, arguments.rootname);

    exit(0);
}
