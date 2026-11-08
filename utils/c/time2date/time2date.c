/****************************************************************************
 *          TIME2DATE.C
 *
 *          Conver time_t to date
 *
 *          Copyright (c) 2018 Niyamaka.
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include <argp-standalone.h>
#include <gobj.h>
#include <testing.h>
#include <helpers.h>
#include <kwid.h>
#include <yev_loop.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define APP         "time2date"
#define DOC         "Convert time_t to ascii date and viceversa"

#define VERSION     YUNETA_VERSION
#define SUPPORT     "<support at artgins.com>"
#define DATETIME    __DATE__ " " __TIME__


/***************************************************************************
 *              Structures
 ***************************************************************************/
/*
 *  Used by main to communicate with parse_opt.
 */
#define MIN_ARGS 0
#define MAX_ARGS 1
struct arguments
{
    char *args[MAX_ARGS+1];     /* positional args */
    char *TZ;
    BOOL time_in_miliseconds;
};

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/***************************************************************************
 *      Data
 ***************************************************************************/
int counter = 0;
const char *argp_program_version = APP " " VERSION;
const char *argp_program_bug_address = SUPPORT;

/* Program documentation. */
static char doc[] = DOC;

/* A description of the arguments we accept. */
static char args_doc[] = "TIME_T";

/*
 *  The options we understand.
 *  See https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
 */
static struct argp_option options[] = {
/*-name-----------------key-----arg-----------------flags---doc-----------------group */
{"TZ",                  'z',    "TIME_ZONE",        0,      "Time zone.",       1},
{"miliseconds",         'm',    0,                  0,      "Time in miliseconds",1},
{0}
};

/* Our argp parser. */
static struct argp argp = {
    options,
    parse_opt,
    args_doc,
    doc,
    0,
    0,
    0
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

    case 'z':
        arguments->TZ = arg;
        break;

    case 'm':
        arguments->time_in_miliseconds = 1;
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

static void ull_to_hex_string(unsigned long long num, char *hexString, char separate) {
    char temp[5];  // Temporary buffer to store each 4-digit hex group
    int i;

    // Initialize the string
    hexString[0] = '\0';

    // Process each group of 4 hex digits (16 bits) from the most significant to least
    for (i = (sizeof(num) * 2) - 4; i >= 0; i -= 4) {
        unsigned long long part = (num >> (i * 4)) & 0xFFFF;  // Extract 16 bits (4 hex digits)
        sprintf(temp, "%04llx", part);  // Convert the 16 bits to a 4-digit hex string
        if (strlen(hexString) > 0) {
            if(separate) {
                strcat(hexString, "-");  // Add space between groups
            }
        }
        strcat(hexString, temp);  // Append the current group to the final string
    }}

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

    /*
     *  With no log handler the ERROR of a zone libc does not read went
     *  nowhere, and the time was printed in UTC with the name of that zone
     */
    sys_malloc_fn_t malloc_func;
    sys_free_fn_t free_func;
    gbmem_get_json_allocators(&malloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);
    gobj_start_up(
        argc,
        argv,
        NULL, // jn_global_settings
        NULL, // persistent_attrs
        NULL, // global_command_parser
        NULL, // global_stats_parser
        NULL, // global_authz_checker
        NULL  // global_authentication_parser
    );
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_UP_WARNING, 0);

    timestamp_t timestamp;
    int offset;

    char now[32];
    if(arguments.args[0]==0) {
        time_t t;
        time(&t);
        snprintf(now, sizeof(now), "%ld", t);
        arguments.args[0] = now;
    }
    if(strstr(arguments.args[0], "T")) {
        /*
         *  Standard date format: <date>T<time>. Up to 7.30.14 the answer
         *  was not checked: "2026-10-01Tgarbage" printed 1970, exit code 0
         */
        if(parse_date_basic(arguments.args[0], &timestamp, &offset) < 0) {
            fprintf(stderr, "TIME_T refused, a text that is no date: '%s'\n\n", arguments.args[0]);
            exit(-1);
        }
    } else {
        timestamp = strtoull(arguments.args[0], NULL, 0);
        if(arguments.time_in_miliseconds) {
            timestamp /= 1000;
        }
    }

    setlocale(LC_ALL, "en_US.UTF-8"); //Deja el numero (PRItime) tal cual

    struct tm *tm;
    char utc_stamp[164];

    tm = gmtime((time_t *)&timestamp);
    if(!tm) {
        printf("timestamp too big\n");
        exit(-1);
    }
    strftime(utc_stamp, sizeof(utc_stamp), "%Y-%m-%dT%H:%M:%S%z", tm);

    if(empty_string(arguments.TZ)) {
        char local_stamp[164];
        tm = localtime((time_t *)&timestamp);
        if(!tm) {
            printf("timestamp too big\n");
            exit(-1);
        }
        strftime(local_stamp, sizeof(local_stamp), "%Y-%m-%dT%H:%M:%S%z", tm);

        char hexString[1264];  // Buffer to store the resulting string
        ull_to_hex_string(timestamp, hexString, 0);
        char hexString2[1264];  // Buffer to store the resulting string
        ull_to_hex_string(timestamp, hexString2, 1);

        printf("gmtime of %"PRItime" (0x%s %s): UTC %s, LOCAL %s, yday %d\n",
            timestamp, hexString, hexString2, utc_stamp, local_stamp, tm->tm_yday
        );
    } else {
        char local_stamp[164];
        tm = localtime((time_t *)&timestamp);
        if(!tm) {
            printf("timestamp too big\n");
            exit(-1);
        }
        strftime(local_stamp, sizeof(local_stamp), "%Y-%m-%dT%H:%M:%S%z", tm);

        char tz_stamp[164];
        time_t offset;
        struct tm tm2;
        gmtime2timezone(timestamp, arguments.TZ, &tm2, &offset);

        /*
         *  A zone libc does not read: its ERROR is logged. Up to 7.30.14 the
         *  time was printed in UTC with that zone's name, exit code 0
         */
        json_t *jn_log = gobj_get_log_data();
        json_int_t errors = kw_get_int(0, jn_log, "error", 0, 0);
        json_decref(jn_log);
        if(errors > 0) {
            exit(-1);   // Error already logged
        }
        strftime(tz_stamp, sizeof(tz_stamp), "%FT%T%z", &tm2);

        char hexString[1264];  // Buffer to store the resulting string
        ull_to_hex_string(timestamp, hexString, 0);
        char hexString2[1264];  // Buffer to store the resulting string
        ull_to_hex_string(timestamp, hexString2, 1);

        printf("gmtime of %"PRItime" (0x%s %s): UTC %s, %s %s, LOCAL %s, yday %d\n",
            timestamp, hexString, hexString2, utc_stamp, arguments.TZ, tz_stamp, local_stamp, tm2.tm_yday
        );
    }
    gobj_end();
    return 0;
}
