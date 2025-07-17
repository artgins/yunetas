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
#include <yuneta_version.h>
#include "rename_gclass.h"

/***************************************************************************
 *      Constants
 ***************************************************************************/
#define NAME            "yclone-gclass"
#define APP_VERSION     __yuneta_version__
#define APP_DATETIME    __DATE__ " " __TIME__
#define APP_SUPPORT     "<niyamaka at yuneta.io>"

/***************************************************************************
 *      Structures
 ***************************************************************************/
#define MIN_ARGS 0
#define MAX_ARGS 2
/* Used by main to communicate with parse_opt. */
struct arguments
{
    char *args[MAX_ARGS+1];     /* positional args */
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
  "yclone-gclass -- a Yuneta utility to rename a gclass\n"
  "Ex: yclone-gclass postgres authenticate # without prefix and extension";

/* A description of the arguments we accept. */
static char args_doc[] = "yclone-gclass OLD_GCLASS NEW_GCLASS";

/* The options we understand. */
static struct argp_option options[] = {
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

    if(!arguments.args[0] || !arguments.args[1]) {
        printf("\nEnter a old gclass-name and new gclass-name please.\n");
        exit(-1);
    }

    return rename_gclass(arguments.args[0], arguments.args[1]);
}
