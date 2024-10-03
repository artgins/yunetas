/****************************************************************************
 *          test.c

rowid   tm
1       972902859     'Abuelo'      forward [2,3,4,5,6,7, cnt+8, cnt+9]
2       972899259     'Abuelo'      backward [9,8,7,6,5,4,3,1]
3       972809259     'Abuela'
4       1603961259    'Padre'
5       1603874859    'Madre'
6       1917074859    'Hijo'
7       1948610859    'Hija'
8       2264230059    'Nieto'
9       2232694059    'Nieta'
10      2390460459    'Nieto'
11      2421996459    'Nieta'
12      2390460459    'Nieto'
13      2421996459    'Nieta'
14      2390460459    'Nieto'
15      2421996459    'Nieta'
16      2390460459    'Nieto'
17      2421996459    'Nieta'
18      2390460459    'Nieto'
19      2421996459    'Nieta'

 *          Copyright (c) 2018 Niyamaka, 2024- ArtGins
 *          All Rights Reserved.
 ****************************************************************************/
#include <argp.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <locale.h>
#include <time.h>
#include <yunetas.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define DATABASE    "tr_msg"
#define TOPIC_NAME  "topic_test"

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PUBLIC void yuno_catch_signals(void);

/***************************************************************************
 *      Data
 ***************************************************************************/
yev_loop_t *yev_loop;
yev_event_t *yev_event_once;
yev_event_t *yev_event_periodic;
int wait_time = 1;
int times_once = 0;
int times_periodic = 0;

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
static int print_topic_iter(json_t * list, uint64_t *result, int max)
{
    int count = 0;

//print_json(list);
    json_t *messages = kw_get_dict(list, "messages", 0, KW_REQUIRED);
    const char *key;
    json_t *message;
    json_object_foreach(messages, key, message) {
        //print_json(message);

        if(count >= max) {
            printf("ERROR count >= max, count %d, max %d\n", count, max);
        }
        json_t *active = kw_get_dict(message, "active", 0, KW_REQUIRED);
        if(active) {
            json_int_t rowid = kw_get_int(active, "__md_tranger__`__rowid__", 0, KW_REQUIRED);
            if(result[count] != rowid) {
                printf("ERROR count rowid not match, count %d, wait rowid %d, found rowid %d\n",
                    count,
                    (int)(result[count]),
                    (int)(rowid)
                );
            }
        }
        count++;
    }

    if(count != max) {
        printf("ERROR count != max, count %d, max %d\n", count, max);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
static int print_key_iter(json_t * list, const char *key, uint64_t *result, int max)
{
    int count = 0;

//print_json(list);
    json_t *message = kw_get_subdict_value(list, "messages", key, 0, KW_REQUIRED);
    json_t *instances = kw_get_list(message, "instances", 0, KW_REQUIRED);

    json_t *instance;
    int idx;
    json_array_foreach(instances, idx, instance) {
        //print_json(message);

        if(count >= max) {
            printf("ERROR count >= max, count %d, max %d\n", count, max);
        }
        json_int_t rowid = kw_get_int(instance, "__md_tranger__`__rowid__", 0, KW_REQUIRED);

        if(0) {
            json_int_t tm = kw_get_int(instance, "__md_tranger__`__tm__", 0, KW_REQUIRED);
            printf("===> tm %lu, rowid %lu\n", (unsigned long)tm, (unsigned long)rowid);
        }

        if(result[count] != rowid) {
            printf("ERROR count rowid not match, count %d, wait rowid %d, found rowid %d\n",
                count,
                (int)(result[count]),
                (int)(rowid)
            );
        }
        count++;
    }

    if(count != max) {
        printf("ERROR count != max, count %d, max %d\n", count, max);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
static void test(json_t *rc2, int caso, uint64_t cnt)
{
    struct timespec st, et;
    double dt;

    /*-------------------------------------*
     *  Loop
     *-------------------------------------*/
    switch(caso) {
    case 1:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            trmsg_add_instance(
                rc2,
                "FAMILY",   // topic
                json_pack("{s:s, s:s, s:s, s:i}",
                    "name", "Abuelo",
                    "birthday", "2000-10-30T11:47:39.0+0100",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                    "level", 0
                ),
                fc_only_desc_cols,
                0
            );
            trmsg_add_instance(
                rc2,
                "FAMILY",   // topic
                json_pack("{s:s, s:s, s:s, s:i}",
                    "name", "Abuelo",
                    "birthday", "2000-10-30T10:47:39.0+0100",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                    "level", 0
                ),
                fc_only_desc_cols,
                0
            );
            trmsg_add_instance(
                rc2,
                "FAMILY",   // topic
                json_pack("{s:s, s:s, s:s, s:i}",
                    "name", "Abuela",
                    "birthday", "2000-10-29T09:47:39.0+0100",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",

                    "level", 0
                ),
                fc_only_desc_cols,
                0
            );
            trmsg_add_instance(
                rc2,
                "FAMILY",   // topic
                json_pack("{s:s, s:s, s:s, s:i}",
                    "name", "Padre",
                    "birthday", "2020-10-29T09:47:39.0+0100",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",

                    "level", 0
                ),
                fc_only_desc_cols,
                0
            );
            trmsg_add_instance(
                rc2,
                "FAMILY",   // topic
                json_pack("{s:s, s:s, s:s, s:i}",
                    "name", "Madre",
                    "birthday", "2020-10-28T08:47:39.0+0000",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",

                    "level", 0
                ),
                fc_only_desc_cols,
                0
            );
            trmsg_add_instance(
                rc2,
                "FAMILY",   // topic
                json_pack("{s:s, s:s, s:s, s:i}",
                    "name", "Hijo",
                    "birthday", "2030-10-01T09:47:39.0+0100",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",

                    "level", 0
                ),
                fc_only_desc_cols,
                0
            );
            trmsg_add_instance(
                rc2,
                "FAMILY",   // topic
                json_pack("{s:s, s:s, s:s, s:i}",
                    "name", "Hija",
                    "birthday", "2031-10-01T09:47:39.0+0100",                                        "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",

                    "level", 0
                ),
                fc_only_desc_cols,
                0
            );
            trmsg_add_instance(
                rc2,
                "FAMILY",   // topic
                json_pack("{s:s, s:s, s:s, s:i}",
                    "name", "Nieto",
                    "birthday", "2041-10-01T09:47:39.0+0100",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",

                    "level", 0
                ),
                fc_only_desc_cols,
                0
            );
            trmsg_add_instance(
                rc2,
                "FAMILY",   // topic
                json_pack("{s:s, s:s, s:s, s:i}",
                    "name", "Nieta",
                    "birthday", "2040-10-01T09:47:39.0+0100",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",

                    "level", 0
                ),
                fc_only_desc_cols,
                0
            );

            for (i = 0; i < cnt/2; i++) {
                trmsg_add_instance(
                    rc2,
                    "FAMILY",   // topic
                    json_pack("{s:s, s:s, s:s, s:i}",
                        "name", "Nieto",
                        "birthday", "2045-10-01T09:47:39.0+0100",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",

                        "level", i+1
                    ),
                    fc_only_desc_cols,
                    0
                );
                trmsg_add_instance(
                    rc2,
                    "FAMILY",   // topic
                    json_pack("{s:s, s:s, s:s, s:i}",
                        "name", "Nieta",
                        "birthday", "2046-10-01T09:47:39.0+0100",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",

                        "level", i+1
                    ),
                    fc_only_desc_cols,
                    0
                );
            }

            clock_gettime (CLOCK_MONOTONIC, &et);
        }
        break;

    case 2:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            json_t *list  = trmsg_open_list(
                rc2,
                "FAMILY",   // topic
                0           // filter
            );

            uint64_t result[] = {2,3,4,5,6,7, 8, 9}; // empieza 2 porque hay dos Abuelo
            result[7-1] += cnt;
            result[8-1] += cnt;
            int max = sizeof(result)/sizeof(result[0]);

            print_topic_iter(list, result, max);

            tranger_close_list(rc2, list);

            clock_gettime (CLOCK_MONOTONIC, &et);
        }
        break;

    case 3:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            json_t *list  = trmsg_open_list(
                rc2,
                "FAMILY",           // topic
                json_pack("{s:b}",  // filter
                    "backward", 1
                )
            );

            uint64_t result[] = {9,8, 7,6,5,4,3,1};
            int max = sizeof(result)/sizeof(result[0]);

            print_topic_iter(list, result, max);

            tranger_close_list(rc2, list);

            clock_gettime (CLOCK_MONOTONIC, &et);
        }

        break;

    case 10:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            uint64_t edades[] = {9,1,3,2,5,4,6,6,8,8,0};
            for(int i=0; edades[i]!=0; i++) {
                trmsg_add_instance(
                    rc2,
                    "FAMILY2",   // topic
                    json_pack("{s:s, s:i, s:s, s:i}",
                        "name", "Bisnieto",
                        "birthday", (int)edades[i],
                        "address", "a",
                        "level", i
                    ),
                    fc_only_desc_cols,
                    0
                );
            }

            json_t *list  = trmsg_open_list(
                rc2,
                "FAMILY2",           // topic
                json_pack("{s:s, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "backward", 0,
                    "order_by_tm", 1
                )
            );

            uint64_t result[] = {2,4,3,6,5,7,8,9,10,1};
            int max = sizeof(result)/sizeof(result[0]);

            print_key_iter(list, "Bisnieto", result, max);

            tranger_close_list(rc2, list);

            clock_gettime (CLOCK_MONOTONIC, &et);

        }
        break;
    case 11:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            uint64_t edades[] = {9,1,3,2,5,4,6,6,8,8,0};
            for(int i=0; edades[i]!=0; i++) {
                trmsg_add_instance(
                    rc2,
                    "FAMILY3",   // topic
                    json_pack("{s:s, s:i, s:s, s:i}",
                        "name", "Bisnieto",
                        "birthday", (int)edades[i],
                        "address", "a",
                        "level", i
                    ),
                    fc_only_desc_cols,
                    0
                );
            }

            json_t *list  = trmsg_open_list(
                rc2,
                "FAMILY3",           // topic
                json_pack("{s:s, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "backward", 1,
                    "order_by_tm", 1
                )
            );

            uint64_t result[] = {2,4,3,6,5,8,7,10,9,1};
            int max = sizeof(result)/sizeof(result[0]);

            print_key_iter(list, "Bisnieto", result, max);

            tranger_close_list(rc2, list);

            clock_gettime (CLOCK_MONOTONIC, &et);
        }
        break;

    case 12:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            uint64_t edades[] = {9,1,3,2,5,4,6,6,8,8,0};
            for(int i=0; edades[i]!=0; i++) {
                trmsg_add_instance(
                    rc2,
                    "FAMILY4",   // topic
                    json_pack("{s:s, s:i, s:s, s:i}",
                        "name", "Bisnieto",
                        "birthday", (int)edades[i],
                        "address", "a",
                        "level", i
                    ),
                    fc_only_desc_cols,
                    0
                );
            }

            json_t *list  = trmsg_open_list(
                rc2,
                "FAMILY4",           // topic
                json_pack("{s:s, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "backward", 0,
                    "order_by_tm", 0
                )
            );

            uint64_t result[] = {1,2,3,4,5,6,7,8,9,10};
            int max = sizeof(result)/sizeof(result[0]);

            print_key_iter(list, "Bisnieto", result, max);

            tranger_close_list(rc2, list);

            clock_gettime (CLOCK_MONOTONIC, &et);

        }
        break;
    case 13:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            uint64_t edades[] = {9,1,3,2,5,4,6,6,8,8,0};
            for(int i=0; edades[i]!=0; i++) {
                trmsg_add_instance(
                    rc2,
                    "FAMILY5",   // topic
                    json_pack("{s:s, s:i, s:s, s:i}",
                        "name", "Bisnieto",
                        "birthday", (int)edades[i],
                        "address", "a",
                        "level", i
                    ),
                    fc_only_desc_cols,
                    0
                );
            }

            json_t *list  = trmsg_open_list(
                rc2,
                "FAMILY5",           // topic
                json_pack("{s:s, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "backward", 1,
                    "order_by_tm", 0
                )
            );

            uint64_t result[] = {10,9,8,7,6,5,4,3,2,1};
            int max = sizeof(result)/sizeof(result[0]);

            print_key_iter(list, "Bisnieto", result, max);

            tranger_close_list(rc2, list);

            clock_gettime (CLOCK_MONOTONIC, &et);
        }
        break;

    case 20:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            json_t *list  = trmsg_open_list(
                rc2,
                "FAMILY2",           // topic
                json_pack("{s:s, s:i, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "max_key_instances", 2,
                    "backward", 0,
                    "order_by_tm", 1
                )
            );

            uint64_t result[] = {10,1};
            int max = sizeof(result)/sizeof(result[0]);

            print_key_iter(list, "Bisnieto", result, max);

            tranger_close_list(rc2, list);

            clock_gettime (CLOCK_MONOTONIC, &et);

        }
        break;
    case 21:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            json_t *list  = trmsg_open_list(
                rc2,
                "FAMILY3",           // topic
                json_pack("{s:s, s:i, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "max_key_instances", 2,
                    "backward", 1,
                    "order_by_tm", 1
                )
            );

            uint64_t result[] = {9,1};
            int max = sizeof(result)/sizeof(result[0]);

            print_key_iter(list, "Bisnieto", result, max);

            tranger_close_list(rc2, list);

            clock_gettime (CLOCK_MONOTONIC, &et);
        }
        break;

    case 22:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            json_t *list  = trmsg_open_list(
                rc2,
                "FAMILY4",           // topic
                json_pack("{s:s, s:i, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "max_key_instances", 2,
                    "backward", 0,
                    "order_by_tm", 0
                )
            );

            uint64_t result[] = {9,10};
            int max = sizeof(result)/sizeof(result[0]);

            print_key_iter(list, "Bisnieto", result, max);

            tranger_close_list(rc2, list);

            clock_gettime (CLOCK_MONOTONIC, &et);

        }
        break;
    case 23:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            json_t *list  = trmsg_open_list(
                rc2,
                "FAMILY5",           // topic
                json_pack("{s:s, s:i, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "max_key_instances", 2,
                    "backward", 1,
                    "order_by_tm", 0
                )
            );

            uint64_t result[] = {2,1};
            int max = sizeof(result)/sizeof(result[0]);

            print_key_iter(list, "Bisnieto", result, max);

            tranger_close_list(rc2, list);

            clock_gettime (CLOCK_MONOTONIC, &et);
        }
        break;

    case 30:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            json_t *list  = trmsg_open_list(
                rc2,
                "FAMILY2",           // topic
                json_pack("{s:s, s:i, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "max_key_instances", 1,
                    "backward", 0,
                    "order_by_tm", 1
                )
            );

            uint64_t result[] = {10}; // WARNING debería ser el 1. No uses tm con inst 1!!
            int max = sizeof(result)/sizeof(result[0]);

            print_key_iter(list, "Bisnieto", result, max);

            tranger_close_list(rc2, list);

            clock_gettime (CLOCK_MONOTONIC, &et);

        }
        break;
    case 31:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            json_t *list  = trmsg_open_list(
                rc2,
                "FAMILY3",           // topic
                json_pack("{s:s, s:i, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "max_key_instances", 1,
                    "backward", 1,
                    "order_by_tm", 1
                )
            );

            uint64_t result[] = {1};
            int max = sizeof(result)/sizeof(result[0]);

            print_key_iter(list, "Bisnieto", result, max);

            tranger_close_list(rc2, list);

            clock_gettime (CLOCK_MONOTONIC, &et);
        }
        break;

    case 32:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            json_t *list  = trmsg_open_list(
                rc2,
                "FAMILY4",           // topic
                json_pack("{s:s, s:i, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "max_key_instances", 1,
                    "backward", 0,
                    "order_by_tm", 0
                )
            );

            uint64_t result[] = {10};
            int max = sizeof(result)/sizeof(result[0]);

            print_key_iter(list, "Bisnieto", result, max);

            tranger_close_list(rc2, list);

            clock_gettime (CLOCK_MONOTONIC, &et);

        }
        break;
    case 33:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);

            json_t *list  = trmsg_open_list(
                rc2,
                "FAMILY5",           // topic
                json_pack("{s:s, s:i, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "max_key_instances", 1,
                    "backward", 1,
                    "order_by_tm", 0
                )
            );

            uint64_t result[] = {1};
            int max = sizeof(result)/sizeof(result[0]);

            print_key_iter(list, "Bisnieto", result, max);

            tranger_close_list(rc2, list);

            clock_gettime (CLOCK_MONOTONIC, &et);
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

    printf("# test %d (%12" PRIu64 "): %f, %'lu op/sec\n\n",
        caso,
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
    arguments.repeat = 10000; // Mínimo 10
    /*
     *  Parse arguments
     */
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    setlocale(LC_ALL, "");

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
    char *path = "/test/trmsg/db_test";

    /*------------------------------*
     *  Destruye la bbdd previa
     *------------------------------*/
    rmrdir(path);

    /*------------------------------*
     *  Crea la bbdd
     *------------------------------*/
    static const json_desc_t family_json_desc[] = {
        // Name             Type        Default
        {"name",            "str",      ""},
        {"birthday",        "int",      "1"},
        {"address",         "str",      "Calle pepe"},
        {"level",           "int",      "2"},
        {"sample1",         "dict",     "{}"},
        {"sample2",         "list",     "[]"},
        {0}
    };

    static topic_desc_t db_test_desc[] = {
        // Topic Name,  Pkey        Key Type        Tkey            Topic Json Desc
        {"FAMILY",      "name",     sf_string_key,  "birthday",     family_json_desc},
        {"FAMILY2",     "name",     sf_string_key,  "birthday",     family_json_desc},
        {"FAMILY3",     "name",     sf_string_key,  "birthday",     family_json_desc},
        {"FAMILY4",     "name",     sf_string_key,  "birthday",     family_json_desc},
        {"FAMILY5",     "name",     sf_string_key,  "birthday",     family_json_desc},
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
    if(arguments.caso == 0) {
        // Ejecuta todos los casos
        test(rc2, 1, arguments.repeat);
        test(rc2, 2, arguments.repeat);
        test(rc2, 3, arguments.repeat);

        test(rc2, 10, 10);
        test(rc2, 11, 10);
        test(rc2, 12, 10);
        test(rc2, 13, 10);

        test(rc2, 20, 10);
        test(rc2, 21, 10);
        test(rc2, 22, 10);
        test(rc2, 23, 10);

        test(rc2, 30, 1);
        test(rc2, 31, 1);
        test(rc2, 32, 1);
        test(rc2, 33, 1);

    } else {
        test(rc2, arguments.caso, arguments.repeat);
    }

    /*------------------------------*
     *  Cierra la bbdd
     *------------------------------*/
    tranger_shutdown(rc2);

    /*---------------------------*
     *      Destroy all
     *---------------------------*/
    gbmem_shutdown();
    end_ghelpers_library();

    return 0;
}
