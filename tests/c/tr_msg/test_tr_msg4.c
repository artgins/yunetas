/****************************************************************************
 *          test.c
 *
 *          Copyright (c) 2018 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <argp.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <locale.h>
#include <time.h>
#include <ghelpers.h>

/***************************************************************************
 *      Constants
 ***************************************************************************/

/***************************************************************************
 *      Structures
 ***************************************************************************/

/*
 *  Used by main to communicate with parse_opt.
 */
#define MIN_ARGS 0
#define MAX_ARGS 0
struct arguments
{
    char *args[MAX_ARGS+1];     /* positional args */

    int devices;
    int trazas;
};

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/***************************************************************************
 *      Data
 ***************************************************************************/
static json_t *jn_mem_topic = 0;

// Set by yuneta_entry_point()
// const char *argp_program_version = APP_NAME " " APP_VERSION;
// const char *argp_program_bug_address = APP_SUPPORT;

/* Program documentation. */
static char doc[] = "";

/* A description of the arguments we accept. */
static char args_doc[] = "";

/*
 *  The options we understand.
 *  See https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
 */
static struct argp_option options[] = {
{"devices",         'd',    "DEVICES",  0,      "Nº of devices.  Default: 1000", 1},
{"trazas",          't',    "TRAZAS",   0,      "Nº of traces (1/sec). Default:100.", 1},
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
    case 'd':
        if(arg) {
            arguments->devices = atoi(arg);
        }
        break;

    case 't':
        if(arg) {
            arguments->trazas = atoi(arg);
        }
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
 *  Prints to the provided buffer a nice number of bytes (KB, MB, GB, etc)
 *  https://www.mbeckler.org/blog/?p=114
 ***************************************************************************/
void pretty_bytes(char* bf, int bfsize, uint64_t bytes)
{
    const char* suffixes[7];
    suffixes[0] = "B";
    suffixes[1] = "Miles";
    suffixes[2] = "Millones";
    suffixes[3] = "GB";
    suffixes[4] = "TB";
    suffixes[5] = "PB";
    suffixes[6] = "EB";
    uint s = 0; // which suffix to use
    double count = bytes;
    while (count >= 1000 && s < 7)
    {
        s++;
        count /= 1000;
    }
    if (count - floor(count) == 0.0)
        snprintf(bf, bfsize, "%d %s", (int)count, suffixes[s]);
    else
        snprintf(bf, bfsize, "%.1f %s", count, suffixes[s]);
}

/***************************************************************************
 *
 ***************************************************************************/
static inline double ts_diff2 (struct timespec start, struct timespec end)
{
    uint64_t s, e;
    s = ((uint64_t)start.tv_sec)*1000000 + ((uint64_t)start.tv_nsec)/1000;
    e = ((uint64_t)end.tv_sec)*1000000 + ((uint64_t)end.tv_nsec)/1000;
    return ((double)(e-s))/1000000;
}

/***************************************************************************
 *
 ***************************************************************************/
static inline struct timespec ts_diff (struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
}

/***************************************************************************
 *
 ***************************************************************************/
static void test(json_t *rc2, int caso, const char *desc, int devices, int trazas)
{
    uint64_t cnt;
    struct timespec st, et;
    double dt;
    static json_t *hrc2_topic_iter1 = 0;
    static json_t *hrc2_topic_iter2 = 0;

    cnt = devices * trazas;

    /*-------------------------------------*
     *  Loop
     *-------------------------------------*/
    switch(caso) {
    case 1:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            for(long trace=1; trace<=trazas; trace++) { // traces de 1 hora, a 1/segundo
                for(unsigned long imei=1; imei<=devices; imei++) {
                    const char *event = "CycleOff";
                    trmsg_add_instance(
                        rc2,
                        "gpss4",     // topic
                        json_pack("{s:I, s:s, s:I, s:f, s:f, s:i, s:i, s:b, s:b}",
                            "imei", (json_int_t)imei,
                            "event", event,
                            "gps_date", (json_int_t)trace,
                            "latitude", 0.0,
                            "longitude", 0.0,
                            "altitude", 0,
                            "heading", 0,
                            "on", 0,
                            "idle", 0
                        ),
                        fc_only_desc_cols,
                        0
                    );
                }
            }

            clock_gettime (CLOCK_MONOTONIC, &et);
        }
        break;

    case 2:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            hrc2_topic_iter1 = trmsg_open_list(
                rc2,
                "gpss4",    // topic
                0          // filter
            );

            clock_gettime (CLOCK_MONOTONIC, &et);
        }
        break;

    case 3:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            hrc2_topic_iter2 = trmsg_open_list(
                rc2,
                "gpss4",    // topic
                json_pack("{s:b, s:b}",  // filter
                    "backward", 1,
                    "order_by_tm", 1
                )
            );

            clock_gettime (CLOCK_MONOTONIC, &et);
        }
        break;

    case 4:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            cnt = 1;
            const char *key = "1";

            json_t *msg = trmsg_get_active_md(
                hrc2_topic_iter1,
                key
            );
            if(!msg) printf("Merde\n");

            clock_gettime (CLOCK_MONOTONIC, &et);
        }
        break;

    case 5:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            cnt = 1;
            const char *key = "500";
            json_t *msg = trmsg_get_active_md(
                hrc2_topic_iter1,
                key
            );
            if(!msg) printf("Merde\n");

            clock_gettime (CLOCK_MONOTONIC, &et);
        }
        break;

    case 6:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            cnt = 1;
            const char *key = "999";
            json_t *msg = trmsg_get_active_md(
                hrc2_topic_iter1,
                key
            );
            if(!msg) printf("Merde\n");

            clock_gettime (CLOCK_MONOTONIC, &et);
        }
        break;

    case 7:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            cnt = 1;
            const char *key = "1";
            json_t *msg = trmsg_get_active_md(
                hrc2_topic_iter2,
                key
            );
            if(!msg) printf("Merde\n");

            clock_gettime (CLOCK_MONOTONIC, &et);
        }
        break;

    case 8:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            cnt = 1;
            const char *key = "500";
            json_t *msg = trmsg_get_active_md(
                hrc2_topic_iter2,
                key
            );
            if(!msg) printf("Merde\n");

            clock_gettime (CLOCK_MONOTONIC, &et);
        }
        break;

    case 9:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            cnt = 1;
            const char *key = "999";
            json_t *msg = trmsg_get_active_md(
                hrc2_topic_iter2,
                key
            );
            if(!msg) printf("Merde\n");

            clock_gettime (CLOCK_MONOTONIC, &et);

            tranger_close_list(rc2, hrc2_topic_iter1);
            tranger_close_list(rc2, hrc2_topic_iter2);

        }
        break;

    default:
        printf("MIERDA\n");
    }


    /*-------------------------------------*
     *  Your end code
     *-------------------------------------*/

    /*-------------------------------------*
     *  Print times
     *-------------------------------------*/
    char bf[128];
    pretty_bytes(bf, sizeof(bf), cnt);

    dt = ts_diff2(st, et);

    setlocale(LC_ALL, "");
    printf("# test %d %-16s ==> devices %'d; trazas %'d; %'"PRIu64" items; %f seconds; %'lu op/sec\n\n",
        caso,
        desc,
        devices,
        trazas,
        cnt,
        dt,
        (unsigned long)(((double)cnt)/dt)
    );
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
    arguments.devices = 1000;
    arguments.trazas = 100;

    /*
     *  Parse arguments
     */
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    /*-------------------------------------*
     *  Your start code
     *-------------------------------------*/
    log_startup(
        "test",             // application name
        "1.0.0",            // applicacion version
        "test_glogger"     // executable program, to can trace stack
    );
    log_add_handler("test_stdout", "stdout", LOG_OPT_UP_WARNING|LOG_HND_OPT_BEATIFUL_JSON, 0);

    static uint32_t mem_list[] = {0, 0};
    gbmem_trace_alloc_free(0, mem_list);

    /*------------------------------------------------*
     *          Setup memory
     *------------------------------------------------*/
    #define MEM_MIN_BLOCK   512
    uint64_t MEM_MAX_SYSTEM_MEMORY = free_ram_in_kb() * 1024LL;
    MEM_MAX_SYSTEM_MEMORY /= 100LL;
    MEM_MAX_SYSTEM_MEMORY *= 90LL;  // Coge el 90% de la memoria

    uint64_t MEM_MAX_BLOCK = (MEM_MAX_SYSTEM_MEMORY / sizeof(md_record_t)) * sizeof(md_record_t);
    MEM_MAX_BLOCK = MIN(1*1024*1024*1024LL, MEM_MAX_BLOCK);  // 1*G max

    uint64_t MEM_SUPERBLOCK = MEM_MAX_BLOCK;

    if(0) {
        gbmem_startup(
            MEM_MIN_BLOCK,
            MEM_MAX_BLOCK,
            MEM_SUPERBLOCK,
            MEM_MAX_SYSTEM_MEMORY,
            NULL,
            0
        );
    } else {
        gbmem_startup_system(
            MEM_MAX_BLOCK,
            MEM_MAX_SYSTEM_MEMORY
        );
    }

    /*
     *  WARNING now all json is gbmem allocated
     */
    json_set_alloc_funcs(
        gbmem_malloc,
        gbmem_free
    );
    uv_replace_allocator(
        gbmem_malloc,
        gbmem_realloc,
        gbmem_calloc,
        gbmem_free
    );

    /*------------------------------*
     *  La bbddd de pruebas
     *------------------------------*/
    char *path = "/test/trmsg/db_test4";

    /*------------------------------*
     *  Destruye la bbdd previa
     *------------------------------*/
    rmrdir(path);

    /*------------------------------*
     *  Crea la bbdd
     *------------------------------*/
    static const json_desc_t traces_json_desc[] = {
        // Name             Type        Default
        {"imei",            "int",      ""},
        {"event",           "str",      ""},
        {"gps_date",        "int",      ""},
        {"latitude",        "int",      ""},
        {"longitude",       "int",      ""},
        {"altitude",        "int",      ""},
        {"heading",         "int",      ""},
        {"on",              "bool",     "false"},
        {"idle",            "bool",     "true"},
        {0}
    };

    static topic_desc_t db_test_desc[] = {
    // Topic Name,  Pkey    Key Type                        Tkey            Topic Json Desc
    {"gpss4",      "imei",  sf_int_key|sf_no_record_disk,   "gps_date",     traces_json_desc},
    {0}
    };

    json_t *jn_tranger = json_pack("{s:s, s:b}",
        "path", path,
        "master", 1
    );
    json_t *rc2 = tranger_startup(
        jn_tranger // owned
    );
    trmsg_open_topics(rc2, db_test_desc);

    /*------------------------------*
     *  Ejecuta los tests
     *------------------------------*/
    printf("\n\n");
    test(rc2, 1, "ADD RECORDS", arguments.devices, arguments.trazas);
    test(rc2, 2, "LOAD FORWARD", arguments.devices, arguments.trazas);
    test(rc2, 3, "LOAD BACKWARD/TM", arguments.devices, arguments.trazas);
    test(rc2, 4, "FIND fore first", arguments.devices, arguments.trazas);
    test(rc2, 5, "FIND fore medium", arguments.devices, arguments.trazas);
    test(rc2, 6, "FIND fore last", arguments.devices, arguments.trazas);
    test(rc2, 7, "FIND back first", arguments.devices, arguments.trazas);
    test(rc2, 8, "FIND back medium", arguments.devices, arguments.trazas);
    test(rc2, 9, "FIND back last", arguments.devices, arguments.trazas);

    /*------------------------------*
     *  Cierra la bbdd
     *------------------------------*/
    tranger_shutdown(rc2);
    json_decref(jn_mem_topic);

    /*---------------------------*
     *      Destroy all
     *---------------------------*/
    gbmem_shutdown();
    end_ghelpers_library();


    return 0;
}

