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
#define MAX_KEYS    2
#define MAX_RECORDS 90000 // 1 day and 1 hour

int repeat = 10000; // Mínimo 10

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PUBLIC void yuno_catch_signals(void);

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE yev_loop_t *yev_loop;
PRIVATE int global_result = 0;

/***************************************************************************
 *
 ***************************************************************************/
static int print_key_iter(json_t * list, const char *key, uint64_t *expected, int max)
{
    int count = 0;
    int result = 0;

//print_json(list);
    json_t *message = kw_get_subdict_value(0, list, "messages", key, 0, KW_REQUIRED);
    json_t *instances = kw_get_list(0, message, "instances", 0, KW_REQUIRED);

    json_t *instance;
    int idx;
    json_array_foreach(instances, idx, instance) {
        if(count >= max) {
            printf("%sERROR%s --> count >= max, count %d, max %d\n", On_Red BWhite, Color_Off, count, max);
            result += -1;
        }
        json_int_t rowid = kw_get_int(0, instance, "__md_tranger__`__rowid__", 0, KW_REQUIRED);

        if(0) {
            json_int_t tm = kw_get_int(0, instance, "__md_tranger__`__tm__", 0, KW_REQUIRED);
            printf("===> tm %lu, rowid %lu\n", (unsigned long)tm, (unsigned long)rowid);
        }

        if(expected[count] != rowid) {
            printf("%sERROR%s --> count rowid not match, count %d, wait rowid %d, found rowid %d\n", On_Red BWhite, Color_Off,
                count,
                (int)(expected[count]),
                (int)(rowid)
            );
            result += -1;
        }
        count++;
    }

    if(count != max) {
        printf("%sERROR%s --> count != max, count %d, max %d\n", On_Red BWhite, Color_Off, count, max);
        result += -1;
    }
    return result;
}

/***************************************************************************
 *
 ***************************************************************************/
static int test(json_t *tranger, int caso, int cnt, int result)
{
    /*-------------------------------------*
     *  Loop
     *-------------------------------------*/
    switch(caso) {
    case 1:
        {
            const char *test_name = "case 1";
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            trmsg_add_instance(
                tranger,
                "FAMILY",   // topic
                json_pack("{s:s, s:s, s:s, s:i}",
                    "name", "Abuelo",
                    "birthday", "2000-10-30T11:47:39.0+0100",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                    "level", 0
                ),
                0
            );
            trmsg_add_instance(
                tranger,
                "FAMILY",   // topic
                json_pack("{s:s, s:s, s:s, s:i}",
                    "name", "Abuelo",
                    "birthday", "2000-10-30T10:47:39.0+0100",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                    "level", 0
                ),
                0
            );
            trmsg_add_instance(
                tranger,
                "FAMILY",   // topic
                json_pack("{s:s, s:s, s:s, s:i}",
                    "name", "Abuela",
                    "birthday", "2000-10-29T09:47:39.0+0100",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",

                    "level", 0
                ),
                0
            );
            trmsg_add_instance(
                tranger,
                "FAMILY",   // topic
                json_pack("{s:s, s:s, s:s, s:i}",
                    "name", "Padre",
                    "birthday", "2020-10-29T09:47:39.0+0100",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",

                    "level", 0
                ),
                0
            );
            trmsg_add_instance(
                tranger,
                "FAMILY",   // topic
                json_pack("{s:s, s:s, s:s, s:i}",
                    "name", "Madre",
                    "birthday", "2020-10-28T08:47:39.0+0000",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",

                    "level", 0
                ),
                0
            );
            trmsg_add_instance(
                tranger,
                "FAMILY",   // topic
                json_pack("{s:s, s:s, s:s, s:i}",
                    "name", "Hijo",
                    "birthday", "2030-10-01T09:47:39.0+0100",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",

                    "level", 0
                ),
                0
            );
            trmsg_add_instance(
                tranger,
                "FAMILY",   // topic
                json_pack("{s:s, s:s, s:s, s:i}",
                    "name", "Hija",
                    "birthday", "2031-10-01T09:47:39.0+0100",                                        "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",

                    "level", 0
                ),
                0
            );
            trmsg_add_instance(
                tranger,
                "FAMILY",   // topic
                json_pack("{s:s, s:s, s:s, s:i}",
                    "name", "Nieto",
                    "birthday", "2041-10-01T09:47:39.0+0100",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",

                    "level", 0
                ),
                0
            );
            trmsg_add_instance(
                tranger,
                "FAMILY",   // topic
                json_pack("{s:s, s:s, s:s, s:i}",
                    "name", "Nieta",
                    "birthday", "2040-10-01T09:47:39.0+0100",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",

                    "level", 0
                ),
                0
            );

            for (int i = 0; i < cnt/2; i++) {
                trmsg_add_instance(
                    tranger,
                    "FAMILY",   // topic
                    json_pack("{s:s, s:s, s:s, s:i}",
                        "name", "Nieto",
                        "birthday", "2045-10-01T09:47:39.0+0100",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",

                        "level", i+1
                    ),
                    0
                );
                trmsg_add_instance(
                    tranger,
                    "FAMILY",   // topic
                    json_pack("{s:s, s:s, s:s, s:i}",
                        "name", "Nieta",
                        "birthday", "2046-10-01T09:47:39.0+0100",
                    "address", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",

                        "level", i+1
                    ),
                    0
                );
            }

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL, result);  // NULL: we want to check only the logs
        }
        break;

    case 2:
        {
            const char *test_name = "case 2";
            set_expected_results( // Check that no logs happen
                test_name, // test name
                json_pack("[{s:s},{s:s}]", // error's list
                    "msg", "key is required to trmsg_open_list",
                    "msg", "tranger2_close_iterator(): iterator NULL"
                ),
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            json_t *list  = trmsg_open_list(
                tranger,
                "FAMILY",   // topic
                0           // filter
            );

            trmsg_close_list(tranger, list);

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL, result);  // NULL: we want to check only the logs
        }
        break;

    case 10:
        {
            const char *test_name = "case 10";
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)


            uint64_t edades[] = {9,1,3,2,5,4,6,6,8,8,0};
            for(int i=0; edades[i]!=0; i++) {
                trmsg_add_instance(
                    tranger,
                    "FAMILY2",   // topic
                    json_pack("{s:s, s:i, s:s, s:i}",
                        "name", "Bisnieto",
                        "birthday", (int)edades[i],
                        "address", "a",
                        "level", i
                    ),
                    0
                );
            }

            json_t *list  = trmsg_open_list(
                tranger,
                "FAMILY2",           // topic
                json_pack("{s:s, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "backward", 0,
                    "order_by_tm", 1
                )
            );

            uint64_t expected[] = {2,4,3,6,5,7,8,9,10,1};
            int max = sizeof(expected)/sizeof(expected[0]);

            print_key_iter(list, "Bisnieto", expected, max);

            trmsg_close_list(tranger, list);

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL, result);  // NULL: we want to check only the logs
        }
        break;
    case 11:
        {
            const char *test_name = "case 11";
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)


            uint64_t edades[] = {9,1,3,2,5,4,6,6,8,8,0};
            for(int i=0; edades[i]!=0; i++) {
                trmsg_add_instance(
                    tranger,
                    "FAMILY3",   // topic
                    json_pack("{s:s, s:i, s:s, s:i}",
                        "name", "Bisnieto",
                        "birthday", (int)edades[i],
                        "address", "a",
                        "level", i
                    ),
                    0
                );
            }

            json_t *list  = trmsg_open_list(
                tranger,
                "FAMILY3",           // topic
                json_pack("{s:s, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "backward", 1,
                    "order_by_tm", 1
                )
            );

            uint64_t expected[] = {2,4,3,6,5,8,7,10,9,1};
            int max = sizeof(expected)/sizeof(expected[0]);

            print_key_iter(list, "Bisnieto", expected, max);

            trmsg_close_list(tranger, list);

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL, result);  // NULL: we want to check only the logs
        }
        break;

    case 12:
        {
            const char *test_name = "case 12";
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            uint64_t edades[] = {9,1,3,2,5,4,6,6,8,8,0};
            for(int i=0; edades[i]!=0; i++) {
                trmsg_add_instance(
                    tranger,
                    "FAMILY4",   // topic
                    json_pack("{s:s, s:i, s:s, s:i}",
                        "name", "Bisnieto",
                        "birthday", (int)edades[i],
                        "address", "a",
                        "level", i
                    ),
                    0
                );
            }

            json_t *list  = trmsg_open_list(
                tranger,
                "FAMILY4",           // topic
                json_pack("{s:s, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "backward", 0,
                    "order_by_tm", 0
                )
            );

            uint64_t expected[] = {1,2,3,4,5,6,7,8,9,10};
            int max = sizeof(expected)/sizeof(expected[0]);

            print_key_iter(list, "Bisnieto", expected, max);

            trmsg_close_list(tranger, list);

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL, result);  // NULL: we want to check only the logs
        }
        break;
    case 13:
        {
            const char *test_name = "case 13";
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            uint64_t edades[] = {9,1,3,2,5,4,6,6,8,8,0};
            for(int i=0; edades[i]!=0; i++) {
                trmsg_add_instance(
                    tranger,
                    "FAMILY5",   // topic
                    json_pack("{s:s, s:i, s:s, s:i}",
                        "name", "Bisnieto",
                        "birthday", (int)edades[i],
                        "address", "a",
                        "level", i
                    ),
                    0
                );
            }

            json_t *list  = trmsg_open_list(
                tranger,
                "FAMILY5",           // topic
                json_pack("{s:s, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "backward", 1,
                    "order_by_tm", 0
                )
            );

            uint64_t expected[] = {10,9,8,7,6,5,4,3,2,1};
            int max = sizeof(expected)/sizeof(expected[0]);

            print_key_iter(list, "Bisnieto", expected, max);

            trmsg_close_list(tranger, list);

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL, result);  // NULL: we want to check only the logs
        }
        break;

    case 20:
        {
            const char *test_name = "case 20";
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            json_t *list  = trmsg_open_list(
                tranger,
                "FAMILY2",           // topic
                json_pack("{s:s, s:i, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "max_key_instances", 2,
                    "backward", 0,
                    "order_by_tm", 1
                )
            );

            uint64_t expected[] = {10,1};
            int max = sizeof(expected)/sizeof(expected[0]);

            print_key_iter(list, "Bisnieto", expected, max);

            trmsg_close_list(tranger, list);

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL, result);  // NULL: we want to check only the logs
        }
        break;
    case 21:
        {
            const char *test_name = "case 21";
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            json_t *list  = trmsg_open_list(
                tranger,
                "FAMILY3",           // topic
                json_pack("{s:s, s:i, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "max_key_instances", 2,
                    "backward", 1,
                    "order_by_tm", 1
                )
            );

            uint64_t expected[] = {9,1};
            int max = sizeof(expected)/sizeof(expected[0]);

            print_key_iter(list, "Bisnieto", expected, max);

            trmsg_close_list(tranger, list);

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL, result);  // NULL: we want to check only the logs
        }
        break;

    case 22:
        {
            const char *test_name = "case 22";
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            json_t *list  = trmsg_open_list(
                tranger,
                "FAMILY4",           // topic
                json_pack("{s:s, s:i, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "max_key_instances", 2,
                    "backward", 0,
                    "order_by_tm", 0
                )
            );

            uint64_t expected[] = {9,10};
            int max = sizeof(expected)/sizeof(expected[0]);

            print_key_iter(list, "Bisnieto", expected, max);

            trmsg_close_list(tranger, list);

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL, result);  // NULL: we want to check only the logs
        }
        break;
    case 23:
        {
            const char *test_name = "case 23";
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            json_t *list  = trmsg_open_list(
                tranger,
                "FAMILY5",           // topic
                json_pack("{s:s, s:i, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "max_key_instances", 2,
                    "backward", 1,
                    "order_by_tm", 0
                )
            );

            uint64_t expected[] = {2,1};
            int max = sizeof(expected)/sizeof(expected[0]);

            print_key_iter(list, "Bisnieto", expected, max);

            trmsg_close_list(tranger, list);

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL, result);  // NULL: we want to check only the logs
        }
        break;

    case 30:
        {
            const char *test_name = "case 30";
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            json_t *list  = trmsg_open_list(
                tranger,
                "FAMILY2",           // topic
                json_pack("{s:s, s:i, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "max_key_instances", 1,
                    "backward", 0,
                    "order_by_tm", 1
                )
            );

            uint64_t expected[] = {10}; // WARNING debería ser el 1. No uses tm con inst 1!!
            int max = sizeof(expected)/sizeof(expected[0]);

            print_key_iter(list, "Bisnieto", expected, max);

            trmsg_close_list(tranger, list);

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL, result);  // NULL: we want to check only the logs
        }
        break;
    case 31:
        {
            const char *test_name = "case 31";
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            json_t *list  = trmsg_open_list(
                tranger,
                "FAMILY3",           // topic
                json_pack("{s:s, s:i, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "max_key_instances", 1,
                    "backward", 1,
                    "order_by_tm", 1
                )
            );

            uint64_t expected[] = {1};
            int max = sizeof(expected)/sizeof(expected[0]);

            print_key_iter(list, "Bisnieto", expected, max);

            trmsg_close_list(tranger, list);

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL, result);  // NULL: we want to check only the logs
        }
        break;

    case 32:
        {
            const char *test_name = "case 32";
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            json_t *list  = trmsg_open_list(
                tranger,
                "FAMILY4",           // topic
                json_pack("{s:s, s:i, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "max_key_instances", 1,
                    "backward", 0,
                    "order_by_tm", 0
                )
            );

            uint64_t expected[] = {10};
            int max = sizeof(expected)/sizeof(expected[0]);

            print_key_iter(list, "Bisnieto", expected, max);

            trmsg_close_list(tranger, list);

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL, result);  // NULL: we want to check only the logs
        }
        break;
    case 33:
        {
            const char *test_name = "case 33";
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            json_t *list  = trmsg_open_list(
                tranger,
                "FAMILY5",           // topic
                json_pack("{s:s, s:i, s:b, s:b}",  // filter
                    "key", "Bisnieto",
                    "max_key_instances", 1,
                    "backward", 1,
                    "order_by_tm", 0
                )
            );

            uint64_t expected[] = {1};
            int max = sizeof(expected)/sizeof(expected[0]);

            print_key_iter(list, "Bisnieto", expected, max);

            trmsg_close_list(tranger, list);

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL, result);  // NULL: we want to check only the logs
        }
        break;

    default:
        printf("MIERDA\n");
        result += -1;
    }

    return result;
}

/***************************************************************************
 *              Test
 *  Open as master, check main files, add records, open rt lists
 *  HACK: return -1 to fail, 0 to ok
 ***************************************************************************/
int do_test(void)
{
    int result = 0;

    /*
     *  Write the tests in ~/tests_yuneta/
     */
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    char path_topic[PATH_MAX];

    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    build_path(path_topic, sizeof(path_topic), path_database, TOPIC_NAME, NULL);

    /*-------------------------------------------------*
     *      Startup the timeranger db
     *-------------------------------------------------*/
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i, s:s, s:i, s:i, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", 0,
        "filename_mask", "%Y",
        "xpermission" , 02770,
        "rpermission", 0600,
        "trace_level", 1
    );
    json_t *tranger = tranger2_startup(0, jn_tranger, 0);

    /*------------------------------*
     *  Crea la bbdd
     *------------------------------*/
    static const json_desc_t family_json_desc[] = {
        // Name             Type        Default
        {"name",            "str",      "",             0},
        {"birthday",        "int",      "1",            0},
        {"address",         "str",      "Calle pepe",   0},
        {"level",           "int",      "2",            0},
        {"sample1",         "dict",     "{}",           0},
        {"sample2",         "list",     "[]",           0},
        {0}
    };

    static topic_desc_t db_test_desc[] = {
        // Topic Name,  Pkey        Key Type        Tkey            Topic Json Desc
        {"FAMILY",      "name",     sf2_string_key, "birthday",     family_json_desc},
        {"FAMILY2",     "name",     sf2_string_key, "birthday",     family_json_desc},
        {"FAMILY3",     "name",     sf2_string_key, "birthday",     family_json_desc},
        {"FAMILY4",     "name",     sf2_string_key, "birthday",     family_json_desc},
        {"FAMILY5",     "name",     sf2_string_key, "birthday",     family_json_desc},
        {0}
    };

    result += trmsg_open_topics(tranger, db_test_desc);

    /*------------------------------*
     *  Ejecuta los tests
     *------------------------------*/
    // Ejecuta todos los casos
    result += test(tranger, 1, repeat, result);
    //result += test(tranger, 2, repeat, result); TODO repon
    result += test(tranger, 10, 10, result);
    result += test(tranger, 11, 10, result);
    result += test(tranger, 12, 10, result);
    result += test(tranger, 13, 10, result);

    result += test(tranger, 20, 10, result);
    result += test(tranger, 21, 10, result);
    result += test(tranger, 22, 10, result);
    result += test(tranger, 23, 10, result);

    result += test(tranger, 30, 1, result);
    result += test(tranger, 31, 1, result);
    result += test(tranger, 32, 1, result);
    result += test(tranger, 33, 1, result);

    /*------------------------*
     *      Close topic
     *------------------------*/
    set_expected_results( // Check that no logs happen
        "check_close_topic", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );
    tranger2_close_topic(tranger, TOPIC_NAME);
    result += test_json(NULL, result);  // NULL: we want to check only the logs

    /*-------------------------------*
     *      Shutdown timeranger
     *-------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger_shutdown", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );
    tranger2_shutdown(tranger);
    result += test_json(NULL, result);  // NULL: we want to check only the logs

    return result;
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    /*----------------------------------*
     *      Startup gobj system
     *----------------------------------*/
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gobj_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

//    gobj_set_deep_tracing(2);           // TODO TEST
//    gobj_set_global_trace(0, TRUE);     // TODO TEST

    unsigned long memory_check_list[] = {0}; // WARNING: list ended with 0
    set_memory_check_list(memory_check_list);

    init_backtrace_with_bfd(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_bfd);

    gobj_start_up(
        argc,
        argv,
        NULL, // jn_global_settings
        NULL, // startup_persistent_attrs
        NULL, // end_persistent_attrs
        0,  // load_persistent_attrs
        0,  // save_persistent_attrs
        0,  // remove_persistent_attrs
        0,  // list_persistent_attrs
        NULL, // global_command_parser
        NULL, // global_stats_parser
        NULL, // global_authz_checker
        NULL, // global_authenticate_parser
        256*1024L,    // max_block, largest memory block
        1*1024*1024L   // max_system_memory, maximum system memory
    );

    yuno_catch_signals();

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    /*------------------------------*
     *  Captura salida logger
     *------------------------------*/
    gobj_log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);
    gobj_log_add_handler(
        "test_stdout",
        "stdout",
        LOG_OPT_UP_WARNING,
        0
    );

    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_create(
        0,
        2024,
        &yev_loop
    );

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    int result = do_test();
    result += global_result;

    /*--------------------------------*
     *  Stop the event loop
     *--------------------------------*/
    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory()!=0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        result += -1;
    }

    return result;
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int xtimes_once = 0;
    xtimes_once++;
    yev_loop->running = 0;
    if(xtimes_once > 1) {
        exit(-1);
    }
}

PUBLIC void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    memset(&sigIntHandler, 0, sizeof(sigIntHandler));
    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigaction(SIGALRM, &sigIntHandler, NULL);   // to debug in kdevelop
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);    // ctrl+c
}
