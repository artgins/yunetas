/****************************************************************************
 *          YTESTCONFIG.C
 *
 *          Test a json configuration file
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <argp.h>
#include <errno.h>
#include <stdlib.h>
#include <ghelpers.h>
#include <yuneta.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define NAME        "ytestconfig"
#define DOC         "Test a json configuration file"

#define APP_VERSION     __yuneta_version__
#define APP_DATETIME    __DATE__ " " __TIME__
#define APP_SUPPORT     "<support at artgins.com>"

/***************************************************************************
 *              Structures
 ***************************************************************************/
/*
 *  Used by main to communicate with parse_opt.
 */
#define MIN_ARGS 1
#define MAX_ARGS 1
struct arguments
{
    char *args[MAX_ARGS+1];     /* positional args */
    int verbose;                /* verbose */
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
static char args_doc[] = "FILE";

/*
 *  The options we understand.
 *  See https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
 */
static struct argp_option options[] = {
/*-name-------------key-----arg---------flags---doc-----------------group */
{"verbose",         'l',    0,          0,      "Verbose mode."},
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
int your_utility(int verbose, const char *filename)
{
    char *final_sconfig = json_config(
        verbose,    // WARNING if true will exit(0)
        1,          // WARNING if true will exit(0)
        0,
        0,
        filename,
        0,
        0
    );
    if(final_sconfig) {
        free(final_sconfig);
    } else {
        printf("ERROR json_config()\n");
        exit(-1);
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
    arguments.verbose = 0;

    /*
     *  Parse arguments
     */
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    /*
     *  Do your work
     */
    return your_utility(arguments.verbose, arguments.args[0]);
}
