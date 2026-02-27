/****************************************************************************
 *          TIME2RANGE.C
 *
 *          Get range of a time
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <argp.h>
#include <time.h>
#include <errno.h>
#include <regex.h>
#include <locale.h>
#include <libintl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ghelpers.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define NAME        "time2range"
#define DOC         "Get range of a time"

#define VERSION     "1.0.0"
#define SUPPORT     "<niyamaka at yuneta.io>"
#define DATETIME    __DATE__ " " __TIME__


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

    time_t t;
    char *type;
    int range;
    char *TZ;
};

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/***************************************************************************
 *      Data
 ***************************************************************************/
int counter = 0;
const char *argp_program_version = NAME " " VERSION;
const char *argp_program_bug_address = SUPPORT;

/* Program documentation. */
static char doc[] = DOC;

/* A description of the arguments we accept. */
static char args_doc[] = "";

/*
 *  The options we understand.
 *  See https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
 */
static struct argp_option options[] = {
/*-name-----------------key-----arg-----------------flags---doc-----------------group */
{"time_t",              't',    "TIME_T",           0,      "Time Epoch",       1},
{"type",                'p',    "TYPE",             0,      "hours,days,weeks,months,years",2},
{"range",               'r',    "RANGE",            0,      "Range.",           3},
{"TZ",                  'z',    "TIME_ZONE",        0,      "Time zone.",       4},
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
    case 't':
        if(arg) {
            if(all_numbers(arg)) {
                arguments->t = atoll(arg);
            } else {
                timestamp_t timestamp;
                int offset;
                parse_date_basic(arg, &timestamp, &offset);
                arguments->t = timestamp;
            }
        }
        break;

    case 'p':
        arguments->type = arg;
        break;

    case 'r':
        if(arg) {
            arguments->range = atoi(arg);
        }
        break;

    case 'z':
        arguments->TZ = arg;
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
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    if(!arguments.t) {
        printf("What time_t?\n");
        exit(-1);
    }
    if(!arguments.type) {
        printf("What type (hours,days,weeks,months,years)?\n");
        exit(-1);
    }

    time_range_t time_range;

    SWITCHS(arguments.type) {
        CASES("hours")
            time_range = get_hours_range(arguments.t, arguments.range, arguments.TZ);
            break;

        CASES("days")
            time_range = get_days_range(arguments.t, arguments.range, arguments.TZ);
            break;

        CASES("weeks")
            time_range = get_weeks_range(arguments.t, arguments.range, arguments.TZ);
            break;

        CASES("months")
            time_range = get_months_range(arguments.t, arguments.range, arguments.TZ);
            break;

        CASES("years")
            time_range = get_years_range(arguments.t, arguments.range, arguments.TZ);
            break;

        DEFAULTS
            printf("What type (hours,days,weeks,months,years)?\n");
            exit(-1);
            break;
    } SWITCHS_END;


    char start[80];
    char end[80];

    time_t offset;
    if(empty_string(arguments.TZ)) {
        struct tm *tm = gmtime(&time_range.start);
        strftime(start, sizeof(start), "%FT%T%z", tm);

        tm = gmtime(&time_range.end);
        strftime(end, sizeof(end), "%FT%T%z", tm);
    } else {
        struct tm tm;
        gmtime2timezone(time_range.start, arguments.TZ, &tm, &offset);
        strftime(start, sizeof(start), "%FT%T%z", &tm);

        gmtime2timezone(time_range.end, arguments.TZ, &tm, &offset);
        strftime(end, sizeof(end), "%FT%T%z", &tm);
    }

    printf("Start: %s, End: %s\n", start, end);
    return 0;
}
