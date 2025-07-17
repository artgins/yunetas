/****************************************************************************
 *          YSCAPEC.C
 *
 *          Escape File to C String
 *
 *          Copyright (c) 2020 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <argp.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define NAME        "yscapec"
#define DOC         "Escape File to C String"

#define VERSION     "1.0.0"
#define SUPPORT     "<niyamaka at yuneta.io>"
#define DATETIME    __DATE__ " " __TIME__

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
    int no_conversion;          /* no_conversion */
    int line_size;              /* line size */
};

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/***************************************************************************
 *      Data
 ***************************************************************************/
const char *argp_program_version = NAME " " VERSION;
const char *argp_program_bug_address = SUPPORT;

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
{"no-conversion",   'n',    0,          0,      "Don't convert double quote to single quote.",    0},
{"line-size",       's',    "SIZE",     0,      "Line size",    0},
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
        arguments->line_size = atoi(arg);
        break;
    case 'n':
        arguments->no_conversion = 1;
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
int your_utility(const char *filename, int no_conversion, int line_size)
{
    FILE *file = fopen(filename, "r");
    if(!file) {
        printf("Cannot open '%s': %s\n", filename, strerror(errno));
        exit(-1);
    }

    fputs("\"\\\n", stdout);

    int ch;
    int count = 0;
    while ((ch = getc(file)) != EOF) {
        count++;
        switch (ch) {
        case '\"':
            if(no_conversion) {
                fputs("\\\"", stdout);
            } else {
                fputs("'", stdout);
            }
            break;
        case '\\':
            fputs("\\\\", stdout);
            break;
        case '\r':
        case '\n':
            count = line_size - count;
            while(count>0) {
                fputs(" ", stdout);
                count--;
            }
            fputs("\\n\\\n", stdout);
            count = 0;
            break;
        case '\t':
            fputs("    ", stdout);
            break;
        // and so on
        default:
            if (iscntrl(ch)) {
                fprintf(stdout, "\\%03o", ch);
            } else {
                fputc(ch, stdout);
            }
        }
    }

    count = line_size - count - 1;
    while(count>0) {
        fputs(" ", stdout);
        count--;
    }
    fputs("\\n\\\n", stdout);
    count = 0;

    fputs("\";\n", stdout);

    fclose(file);
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
    arguments.no_conversion = 0;
    arguments.line_size = 70;
    /*
     *  Parse arguments
     */
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    /*
     *  Do your work
     */
    return your_utility(arguments.args[0], arguments.no_conversion, arguments.line_size);
}
