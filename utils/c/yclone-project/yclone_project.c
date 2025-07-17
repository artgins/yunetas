/****************************************************************************
 *          YCLONE-PROJECT.C
 *          Clone a project with new name
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <argp.h>
#include <yuneta.h>
#include "make_clone.h"

/***************************************************************************
 *      Constants
 ***************************************************************************/
#define NAME            "yclone-project"
#define APP_VERSION     __yuneta_version__
#define APP_DATETIME    __DATE__ " " __TIME__
#define APP_SUPPORT     "<niyamaka at yuneta.io>"

/***************************************************************************
 *      Structures
 ***************************************************************************/
#define MIN_ARGS 2
#define MAX_ARGS 2
/* Used by main to communicate with parse_opt. */
struct arguments
{
    char *args[MAX_ARGS+1];     /* positional args */
    char *source_keyword;           /* source project keyword */
    char *destination_keyword;      /* destination project keyword */
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
  "yclone-project -- a Yuneta utility to clone a project. All source keywords (lower, capitalize, upper) will be rename to destination keyword (in filenames and in content of files)";

/* A description of the arguments we accept. */
static char args_doc[] = "SOURCE-PROJECT DESTINATION-PROJECT";

/* The options we understand. */
static struct argp_option options[] = {
{"src-keyword",     's',    "KEYWORD",  0,   "Source keyword", 1},
{"dst-keyword",     'd',    "KEYWORD",  0,   "Destination keyword", 2},
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
    case 's':
        arguments->source_keyword = arg;
        break;

    case 'd':
        arguments->destination_keyword = arg;
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

    if(empty_string(arguments.source_keyword)) {
        printf("What source project keyword?\n");
        exit(-1);
    }
    if(empty_string(arguments.destination_keyword)) {
        printf("What destination project keyword?\n");
        exit(-1);
    }

    make_clone(arguments.args[0], arguments.args[1], arguments.source_keyword, arguments.destination_keyword);

    exit(0);
}
