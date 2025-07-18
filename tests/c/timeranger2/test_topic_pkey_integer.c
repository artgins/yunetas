/****************************************************************************
 *          test_topic_pkey_integer.c
 *
 *  - do_test: Open as master, check main files, open rt lists and append RECORDS
 *  - do_test2: Open as master and change topic version and cols
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP "test_topic_pkey_integer"

/***************************************************************
 *              Constants
 ***************************************************************/
#define DATABASE    "tr_topic_pkey_integer"
#define TOPIC_NAME  "topic_pkey_integer"
#define MAX_KEYS    2
#define MAX_RECORDS 90000 // 1 day and 1 hour

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE int global_result = 0;

PRIVATE json_int_t key1_g_rowid_1[2]        = {0, 1};       // i_rowid must be 1
PRIVATE json_int_t key1_g_rowid_2[2]        = {0, 2};       // i_rowid must be 2
PRIVATE json_int_t key1_g_rowid_86399[2]    = {0, 86399};   // i_rowid must be 86399
PRIVATE json_int_t key1_g_rowid_86400[2]    = {0, 86400};   // i_rowid must be 86400
PRIVATE json_int_t key1_g_rowid_86401[2]    = {0, 1};       // i_rowid must be 1
PRIVATE json_int_t key1_g_rowid_86402[2]    = {0, 2};       // i_rowid must be 2
PRIVATE json_int_t key1_g_rowid_89999[2]    = {0, 3599};    // i_rowid must be 3599
PRIVATE json_int_t key1_g_rowid_90000[2]    = {0, 3600};    // i_rowid must be 3600

PRIVATE json_int_t key2_g_rowid_1[2]        = {0, 1};       // i_rowid must be 1
PRIVATE json_int_t key2_g_rowid_2[2]        = {0, 2};       // i_rowid must be 2
PRIVATE json_int_t key2_g_rowid_86399[2]    = {0, 86399};   // i_rowid must be 86399
PRIVATE json_int_t key2_g_rowid_86400[2]    = {0, 86400};   // i_rowid must be 86400
PRIVATE json_int_t key2_g_rowid_86401[2]    = {0, 1};       // i_rowid must be 1
PRIVATE json_int_t key2_g_rowid_86402[2]    = {0, 2};       // i_rowid must be 2
PRIVATE json_int_t key2_g_rowid_89999[2]    = {0, 3599};    // i_rowid must be 3599
PRIVATE json_int_t key2_g_rowid_90000[2]    = {0, 3600};    // i_rowid must be 3600

/***************************************************************************
 *
 ***************************************************************************/
int pinta_md = 0;
size_t all_leidos = 0;
int all_load_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list, // iterator or rt_list/rt_disk id, don't own
    json_int_t rowid,
    md2_record_ex_t *md2_record,
    json_t *record      // must be owned
)
{
    all_leidos++;

    if(pinta_md) {
        char temp[1024];
        tranger2_print_md1_record(
            temp,
            sizeof(temp),
            key,
            rowid,
            md2_record,
            FALSE
        );
        printf("%s\n", temp);
    }

    if(all_leidos > 180000) {
        global_result += -1;
        printf("%sERROR%s --> all_leidos(%d) > 180000\n", On_Red BWhite, Color_Off,
            (int)all_leidos
        );
    }

    json_int_t g_rowid = rowid;
    json_int_t i_rowid = (json_int_t)md2_record->rowid;

    SWITCHS(key) {
        CASES("0000000000000000002")
            switch(g_rowid) {
                case 1:
                    key2_g_rowid_1[0] = i_rowid;
                    break;
                case 2:
                    key2_g_rowid_2[0] = i_rowid;
                    break;
                case 86399:
                    key2_g_rowid_86399[0] = i_rowid;
                    break;
                case 86400:
                    key2_g_rowid_86400[0] = i_rowid;
                    break;
                case 86401:
                    key2_g_rowid_86401[0] = i_rowid;
                    break;
                case 86402:
                    key2_g_rowid_86402[0] = i_rowid;
                    break;
                case 89999:
                    key2_g_rowid_89999[0] = i_rowid;
                    break;
                case 90000:
                    key2_g_rowid_90000[0] = i_rowid;
                    break;

                default:
                    break;
            }
            break;

        DEFAULTS
            break;
    } SWITCHS_END

    JSON_DECREF(record)
    return 0;
}

size_t one_leidos = 0;
int one_load_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list, // iterator or rt_list/rt_disk id, don't own
    json_int_t rowid,   // g_rowid
    md2_record_ex_t *md2_record,
    json_t *record      // must be owned
)
{
    one_leidos++;

    if(one_leidos > 90000) {
        global_result += -1;
        printf("%sERROR%s --> one_leidos(%d) > 90000\n", On_Red BWhite, Color_Off,
            (int)all_leidos
        );
    }

    json_int_t g_rowid = rowid;
    json_int_t i_rowid = (json_int_t)md2_record->rowid;

    SWITCHS(key) {
        CASES("0000000000000000001")
            switch(g_rowid) {
                case 1:
                    key1_g_rowid_1[0] = i_rowid;
                    break;
                case 2:
                    key1_g_rowid_2[0] = i_rowid;
                    break;
                case 86399:
                    key1_g_rowid_86399[0] = i_rowid;
                    break;
                case 86400:
                    key1_g_rowid_86400[0] = i_rowid;
                    break;
                case 86401:
                    key1_g_rowid_86401[0] = i_rowid;
                    break;
                case 86402:
                    key1_g_rowid_86402[0] = i_rowid;
                    break;
                case 89999:
                    key1_g_rowid_89999[0] = i_rowid;
                    break;
                case 90000:
                    key1_g_rowid_90000[0] = i_rowid;
                    break;

                default:
                    break;
            }
            break;

        DEFAULTS
            break;
    } SWITCHS_END

    JSON_DECREF(record)
    return 0;
}

int check_rowids(void)
{
    int result = 0;

    if(key1_g_rowid_1[0]      != key1_g_rowid_1[1]) {
        result += -1;
        printf("%sERROR%s --> key1 g_rowid 1 not match(%d, %d)\n", On_Red BWhite, Color_Off,
            (int)key1_g_rowid_1[0], (int)key1_g_rowid_1[1]
        );
    }
    if(key1_g_rowid_2[0]      != key1_g_rowid_2[1]) {
        result += -1;
        printf("%sERROR%s --> key1 g_rowid 2 not match(%d, %d)\n", On_Red BWhite, Color_Off,
            (int)key1_g_rowid_2[0], (int)key1_g_rowid_2[1]
        );
    }
    if(key1_g_rowid_86399[0]  != key1_g_rowid_86399[1]) {
        result += -1;
        printf("%sERROR%s --> key1 g_rowid 86399 not match(%d, %d)\n", On_Red BWhite, Color_Off,
            (int)key1_g_rowid_86399[0], (int)key1_g_rowid_86399[1]
        );
    }
    if(key1_g_rowid_86400[0]  != key1_g_rowid_86400[1]) {
        result += -1;
        printf("%sERROR%s --> key1 g_rowid 86400 not match(%d, %d)\n", On_Red BWhite, Color_Off,
            (int)key1_g_rowid_86400[0], (int)key1_g_rowid_86400[1]
        );
    }
    if(key1_g_rowid_86401[0]  != key1_g_rowid_86401[1]) {
        result += -1;
        printf("%sERROR%s --> key1 g_rowid 86401 not match(%d, %d)\n", On_Red BWhite, Color_Off,
            (int)key1_g_rowid_86401[0], (int)key1_g_rowid_86401[1]
        );
    }
    if(key1_g_rowid_86402[0]  != key1_g_rowid_86402[1]) {
        result += -1;
        printf("%sERROR%s --> key1 g_rowid 86402 not match(%d, %d)\n", On_Red BWhite, Color_Off,
            (int)key1_g_rowid_86402[0], (int)key1_g_rowid_86402[1]
        );
    }
    if(key1_g_rowid_89999[0]  != key1_g_rowid_89999[1]) {
        result += -1;
        printf("%sERROR%s --> key1 g_rowid 89999 not match(%d, %d)\n", On_Red BWhite, Color_Off,
            (int)key1_g_rowid_89999[0], (int)key1_g_rowid_89999[1]
        );
    }
    if(key1_g_rowid_90000[0]  != key1_g_rowid_90000[1]) {
        result += -1;
        printf("%sERROR%s --> key1 g_rowid 90000 not match(%d, %d)\n", On_Red BWhite, Color_Off,
            (int)key1_g_rowid_90000[0], (int)key1_g_rowid_90000[1]
        );
    }

    if(key2_g_rowid_1[0]      != key2_g_rowid_1[1]) {
        result += -1;
        printf("%sERROR%s --> key2 g_rowid 1 not match(%d, %d)\n", On_Red BWhite, Color_Off,
            (int)key2_g_rowid_1[0], (int)key2_g_rowid_1[1]
        );
    }
    if(key2_g_rowid_2[0]      != key2_g_rowid_2[1]) {
        result += -1;
        printf("%sERROR%s --> key2 g_rowid 2 not match(%d, %d)\n", On_Red BWhite, Color_Off,
            (int)key2_g_rowid_2[0], (int)key2_g_rowid_2[1]
        );
    }
    if(key2_g_rowid_86399[0]  != key2_g_rowid_86399[1]) {
        result += -1;
        printf("%sERROR%s --> key2 g_rowid 86399 not match(%d, %d)\n", On_Red BWhite, Color_Off,
            (int)key2_g_rowid_86399[0], (int)key2_g_rowid_86399[1]
        );
    }
    if(key2_g_rowid_86400[0]  != key2_g_rowid_86400[1]) {
        result += -1;
        printf("%sERROR%s --> key2 g_rowid 86400 not match(%d, %d)\n", On_Red BWhite, Color_Off,
            (int)key2_g_rowid_86400[0], (int)key2_g_rowid_86400[1]
        );
    }
    if(key2_g_rowid_86401[0]  != key2_g_rowid_86401[1]) {
        result += -1;
        printf("%sERROR%s --> key2 g_rowid 86401 not match(%d, %d)\n", On_Red BWhite, Color_Off,
            (int)key2_g_rowid_86401[0], (int)key2_g_rowid_86401[1]
        );
    }
    if(key2_g_rowid_86402[0]  != key2_g_rowid_86402[1]) {
        result += -1;
        printf("%sERROR%s --> key2 g_rowid 86402 not match(%d, %d)\n", On_Red BWhite, Color_Off,
            (int)key2_g_rowid_86402[0], (int)key2_g_rowid_86402[1]
        );
    }
    if(key2_g_rowid_89999[0]  != key2_g_rowid_89999[1]) {
        result += -1;
        printf("%sERROR%s --> key2 g_rowid 89999 not match(%d, %d)\n", On_Red BWhite, Color_Off,
            (int)key2_g_rowid_89999[0], (int)key2_g_rowid_89999[1]
        );
    }
    if(key2_g_rowid_90000[0]  != key2_g_rowid_90000[1]) {
        result += -1;
        printf("%sERROR%s --> key2 g_rowid 90000 not match(%d, %d)\n", On_Red BWhite, Color_Off,
            (int)key2_g_rowid_90000[0], (int)key2_g_rowid_90000[1]
        );
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
    uint64_t t1;
    int result = 0;
    json_t *topic;
    char file[PATH_MAX];

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

    /*------------------------------------*
     *  Check __timeranger2__.json file
     *------------------------------------*/
    build_path(file, sizeof(file), path_database, "__timeranger2__.json", NULL);
    if(1) {
        char expected[]= "\
        { \
          'filename_mask': '%Y', \
          'rpermission': 384, \
          'xpermission': 1528 \
        } \
        ";
        set_expected_results(
            "check__timeranger2__.json",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            NULL,
            TRUE
        );
        result += test_json_file(file);
    }

    /*-------------------------------------------------*
     *      Create a topic
     *-------------------------------------------------*/
    set_expected_results(
        "create topic", // test name
        json_pack("[{s:s},{s:s},{s:s},{s:s}]", // error's list
            "msg", "Creating topic",
            "msg", "Creating topic_desc.json",
            "msg", "Creating topic_cols.json",
            "msg", "Creating topic_var.json"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    topic = tranger2_create_topic(
        tranger,
        TOPIC_NAME,     // topic name
        "id",           // pkey
        "tm",           // tkey
        json_pack("{s:i, s:s, s:i, s:i}", // jn_topic_desc
            "on_critical_error", 4,
            "filename_mask", "%Y-%m-%d",
            "xpermission" , 02700,
            "rpermission", 0600
        ),
        sf_int_key,  // system_flag
        json_pack("{s:s, s:I, s:s}", // jn_cols, owned
            "id", "",
            "tm", (json_int_t)0,
            "content", ""
        ),
        0
    );
    result += test_json(NULL);  // NULL: we want to check only the logs
    if(!topic) {
        tranger2_shutdown(tranger);
        return -1;
    }

    /*------------------------------------*
     *  Check "topic_desc.json" file
     *------------------------------------*/
    build_path(file, sizeof(file), path_topic, "topic_desc.json", NULL);
    if(1) {
        char expected[16*1024];
        snprintf(expected, sizeof(expected), "\
        { \
            'topic_name': '%s', \
            'pkey': 'id', \
            'tkey': 'tm', \
            'system_flag': 4, \
            'filename_mask': '%%Y-%%m-%%d', \
            'xpermission': 1472, \
            'rpermission': 384 \
        } \
        ", TOPIC_NAME);

        set_expected_results(
            "check_topic_desc.json",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            NULL,
            TRUE
        );
        result += test_json_file(file);
    }

    /*------------------------------------*
     *  Check "topic_cols.json" file
     *------------------------------------*/
    build_path(file, sizeof(file), path_topic, "topic_cols.json", NULL);
    if(1) {
        char expected[]= "\
        { \
          'id': '', \
          'tm': 0, \
          'content': '' \
        } \
        ";

        set_expected_results(
            "check_topic_cols.json",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            NULL,
            TRUE
        );
        result += test_json_file(file);
    }

    /*------------------------------------*
     *  Check "topic_var.json" file
     *------------------------------------*/
    build_path(file, sizeof(file), path_topic, "topic_var.json", NULL);
    if(1) {
        char expected[]= "\
        { \
        } \
        ";

        set_expected_results(
            "check_topic_var.json",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            NULL,
            TRUE
        );
        result += test_json_file(file);
    }

    /*------------------------------------------*
     *  Check tranger memory with topic opened
     *------------------------------------------*/
    if(1) {
        char expected[16*1024];
        snprintf(expected, sizeof(expected), "\
        { \
            'path': '%s', \
            'database': '%s', \
            'filename_mask': '%%Y', \
            'xpermission': 1528, \
            'rpermission': 384, \
            'on_critical_error': 0, \
            'master': true, \
            'gobj': 0, \
            'trace_level': 1, \
            'directory': '%s', \
            'fd_opened_files': { \
                '__timeranger2__.json': 9999 \
            }, \
            'yev_loop': 0, \
            'topics': { \
                '%s': { \
                    'topic_name': '%s', \
                    'pkey': 'id', \
                    'tkey': 'tm', \
                    'system_flag': 4, \
                    'filename_mask': '%%Y-%%m-%%d', \
                    'xpermission': 1472, \
                    'rpermission': 384, \
                    'cols': { \
                        'id': '', \
                        'tm': 0, \
                        'content': '' \
                    }, \
                    'directory': '%s', \
                    'wr_fd_files': {}, \
                    'rd_fd_files': {}, \
                    'cache': {}, \
                    'lists': [], \
                    'disks': [], \
                    'iterators': [] \
                } \
            } \
        } \
        ", path_root, DATABASE, path_database, TOPIC_NAME, TOPIC_NAME, path_topic);

        const char *ignore_keys[]= {
            "__timeranger2__.json",
            NULL
        };
        json_t *expected_ = string2json(helper_quote2doublequote(expected), TRUE);
        if(!expected_) {
            result += -1;
        }
        set_expected_results(
            "check_tranger_mem1",      // test name
            NULL,
            expected_,
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger));
    }

    /*-------------------------------------*
     *      Add records
     *-------------------------------------*/
    set_expected_results( // Check that no logs happen
        "append records without open_rt_mem()", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    time_measure_t time_measure;
    MT_START_TIME(time_measure)

    t1 = 946684800; // 2000-01-01T00:00:00+0000
    for(json_int_t i=0; i<MAX_KEYS; i++) {
        uint64_t tm = t1;
        for(json_int_t j=0; j<MAX_RECORDS; j++) {
            json_t *jn_record1 = json_pack("{s:I, s:I, s:s}",
               "id", i + 1,
               "tm", tm+j,
               "content",
               "Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el."
               "Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el."
               "Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el.x"
            );
            md2_record_ex_t md_record;
            tranger2_append_record(tranger, TOPIC_NAME, tm+j, 0, &md_record, jn_record1);
        }
    }

    MT_INCREMENT_COUNT(time_measure, MAX_KEYS*MAX_RECORDS)
    MT_PRINT_TIME(time_measure, "tranger2_append_record")

    result += test_json(NULL);  // NULL: we want to check only the logs

    /*----------------------------------------------*
     *  Check tranger memory after append records
     *----------------------------------------------*/
    if(1) {
        char expected[32*1024];
        snprintf(expected, sizeof(expected), "\
        { \
            'path': '%s', \
            'database': '%s', \
            'filename_mask': '%%Y', \
            'xpermission': 1528, \
            'rpermission': 384, \
            'on_critical_error': 0, \
            'master': true, \
            'gobj': 0, \
            'trace_level': 1, \
            'directory': '%s', \
            'fd_opened_files': { \
                '__timeranger2__.json': 9999 \
            }, \
            'yev_loop': 0, \
            'topics': { \
                '%s': { \
                    'topic_name': '%s', \
                    'pkey': 'id', \
                    'tkey': 'tm', \
                    'system_flag': 4, \
                    'filename_mask': '%%Y-%%m-%%d', \
                    'xpermission': 1472, \
                    'rpermission': 384, \
                    'cols': { \
                        'id': '', \
                        'tm': 0, \
                        'content': '' \
                    }, \
                    'directory': '%s', \
                    'wr_fd_files': {\
                        '0000000000000000001': { \
                            '2000-01-02.json': 99999, \
                            '2000-01-02.md2': 99999 \
                        }, \
                        '0000000000000000002': { \
                            '2000-01-02.json': 99999, \
                            '2000-01-02.md2': 99999 \
                        } \
                    }, \
                    'rd_fd_files': {}, \
                    'cache': { \
                        '0000000000000000001': { \
                            'files': [ \
                                { \
                                    'id': '2000-01-01', \
                                    'fr_t': 946684800, \
                                    'to_t': 946771199, \
                                    'fr_tm': 70369690862464, \
                                    'to_tm': 70369690948863, \
                                    'rows': 86400 \
                                }, \
                                { \
                                    'id': '2000-01-02', \
                                    'fr_t': 946771200, \
                                    'to_t': 946774799, \
                                    'fr_tm': 70369690948864, \
                                    'to_tm': 70369690952463, \
                                    'rows': 3600 \
                                } \
                            ], \
                            'total': { \
                                'fr_t': 946684800, \
                                'to_t': 946774799, \
                                'fr_tm': 70369690862464, \
                                'to_tm': 70369690952463, \
                                'rows': 90000 \
                            } \
                        }, \
                        '0000000000000000002': { \
                            'files': [ \
                                { \
                                    'id': '2000-01-01', \
                                    'fr_t': 946684800, \
                                    'to_t': 946771199, \
                                    'fr_tm': 70369690862464, \
                                    'to_tm': 70369690948863, \
                                    'rows': 86400 \
                                }, \
                                { \
                                    'id': '2000-01-02', \
                                    'fr_t': 946771200, \
                                    'to_t': 946774799, \
                                    'fr_tm': 70369690948864, \
                                    'to_tm': 70369690952463, \
                                    'rows': 3600 \
                                } \
                            ], \
                            'total': { \
                                'fr_t': 946684800, \
                                'to_t': 946774799, \
                                'fr_tm': 70369690862464, \
                                'to_tm': 70369690952463, \
                                'rows': 90000 \
                            } \
                        } \
                    }, \
                    'lists': [], \
                    'disks': [], \
                    'iterators': [] \
                } \
            } \
        } \
        ", path_root, DATABASE, path_database, TOPIC_NAME, TOPIC_NAME, path_topic);

        const char *ignore_keys[]= {
            "__timeranger2__.json",
            "load_record_callback",
            "2000-01-02.json",
            "2000-01-02.md2",
            NULL
        };
        json_t *expected_ = string2json(helper_quote2doublequote(expected), TRUE);
        if(!expected_) {
            result += -1;
        }
        set_expected_results(
            "check_tranger_mem4",      // test name
            NULL,
            expected_,
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger));
    }

    /*-------------------------------------*
     *      Delete topic
     *-------------------------------------*/
    set_expected_results( // Check that no logs happen
        "delete topic", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    result += tranger2_delete_topic(
        tranger,
        TOPIC_NAME
    );
    result += test_json(NULL);  // NULL: we want to check only the logs

    if(is_directory(path_topic)) {
        printf("%sTopic continue existing %s\n", On_Red BWhite,Color_Off);
        result += -1;
    }

    /*------------------------------------------*
     *  Check tranger memory with topic deleted
     *------------------------------------------*/
    if(1) {
        char expected[16*1024];
        snprintf(expected, sizeof(expected), "\
        { \
            'path': '%s', \
            'database': '%s', \
            'filename_mask': '%%Y', \
            'xpermission': 1528, \
            'rpermission': 384, \
            'on_critical_error': 0, \
            'master': true, \
            'gobj': 0, \
            'trace_level': 1, \
            'directory': '%s', \
            'fd_opened_files': { \
                '__timeranger2__.json': 9999 \
            }, \
            'yev_loop': 0, \
            'topics': { \
            } \
        } \
        ", path_root, DATABASE, path_database);

        const char *ignore_keys[]= {
            "__timeranger2__.json",
            NULL
        };
        json_t *expected_ = string2json(helper_quote2doublequote(expected), TRUE);
        if(!expected_) {
            result += -1;
        }
        set_expected_results(
            "check_tranger_mem2",      // test name
            NULL,
            expected_,
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger));
    }

    /*-------------------------------------------------*
     *      Re-Create a topic
     *-------------------------------------------------*/
    set_expected_results(
        "re-create topic", // test name
        json_pack("[{s:s},{s:s},{s:s},{s:s}]", // error's list
            "msg", "Creating topic",
            "msg", "Creating topic_desc.json",
            "msg", "Creating topic_cols.json",
            "msg", "Creating topic_var.json"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    topic = tranger2_create_topic(
        tranger,
        TOPIC_NAME,     // topic name
        "id",           // pkey
        "tm",           // tkey
        json_pack("{s:i, s:s, s:i, s:i}", // jn_topic_desc
            "on_critical_error", 4,
            "filename_mask", "%Y-%m-%d",
            "xpermission" , 02700,
            "rpermission", 0600
        ),
        sf_int_key,    // system_flag
        json_pack("{s:s, s:I, s:s}", // jn_cols, owned
            "id", "",
            "tm", (json_int_t)0,
            "content", ""
        ),
        0
    );
    result += test_json(NULL);  // NULL: we want to check only the logs
    if(!topic) {
        tranger2_shutdown(tranger);
        return -1;
    }

    /*-------------------------------------*
     *      Open rt list
     *-------------------------------------*/
    set_expected_results( // Check that no logs happen
        "open rt list", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    all_leidos = 0;
    one_leidos = 0;

    json_t *tr_list = tranger2_open_rt_mem(
        tranger,
        TOPIC_NAME,
        "",             // key
        NULL,           // match_cond,
        all_load_record_callback,
        "list1",    // rt id
        "",         // creator
        NULL
    );

    tranger2_open_rt_mem(
        tranger,
        TOPIC_NAME,
        "0000000000000000001",       // key
        NULL,   // match_cond
        one_load_record_callback,
        "list2",    // rt id
        "",         // creator
        NULL
    );

    result += test_json(NULL);  // NULL: we want to check only the logs

    /*------------------------------------------*
     *  Check tranger memory with lists opened
     *------------------------------------------*/
    if(1) {
        char expected[32*1024];
        snprintf(expected, sizeof(expected), "\
        { \
            'path': '%s', \
            'database': '%s', \
            'filename_mask': '%%Y', \
            'xpermission': 1528, \
            'rpermission': 384, \
            'on_critical_error': 0, \
            'master': true, \
            'gobj': 0, \
            'trace_level': 1, \
            'directory': '%s', \
            'fd_opened_files': { \
                '__timeranger2__.json': 9999 \
            }, \
            'yev_loop': 0, \
            'topics': { \
                '%s': { \
                    'topic_name': '%s', \
                    'pkey': 'id', \
                    'tkey': 'tm', \
                    'system_flag': 4, \
                    'filename_mask': '%%Y-%%m-%%d', \
                    'xpermission': 1472, \
                    'rpermission': 384, \
                    'cols': { \
                        'id': '', \
                        'tm': 0, \
                        'content': '' \
                    }, \
                    'directory': '%s', \
                    'wr_fd_files': {}, \
                    'rd_fd_files': {}, \
                    'cache': {}, \
                    'lists': [ \
                        { \
                            'id': 'list1', \
                            'creator': '', \
                            'topic_name': '%s', \
                            'key': '', \
                            'match_cond': {}, \
                            'load_record_callback': 99999, \
                            'list_type': 'rt_mem'\
                        }, \
                        { \
                            'id': 'list2', \
                            'creator': '', \
                            'topic_name': '%s', \
                            'key': '0000000000000000001', \
                            'match_cond': {}, \
                            'load_record_callback': 99999, \
                            'list_type': 'rt_mem'\
                        } \
                    ], \
                    'disks': [], \
                    'iterators': [] \
                } \
            } \
        } \
        ", path_root, DATABASE, path_database, TOPIC_NAME, TOPIC_NAME, path_topic,TOPIC_NAME,TOPIC_NAME);

        const char *ignore_keys[]= {
            "__timeranger2__.json",
            "load_record_callback",
            NULL
        };
        json_t *expected_ = string2json(helper_quote2doublequote(expected), TRUE);
        if(!expected_) {
            result += -1;
        }
        set_expected_results(
            "check_tranger_mem3",      // test name
            NULL,
            expected_,
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger));
    }

    /*-------------------------------------*
     *      Add records
     *-------------------------------------*/
    set_expected_results( // Check that no logs happen
        "append records with open_rt_mem()", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    MT_START_TIME(time_measure)

    t1 = 946684800; // 2000-01-01T00:00:00+0000
    for(json_int_t i=0; i<MAX_KEYS; i++) {
        uint64_t tm = t1;
        for(json_int_t j=0; j<MAX_RECORDS; j++) {
            json_t *jn_record1 = json_pack("{s:I, s:I, s:s}",
               "id", i + 1,
               "tm", tm+j,
               "content",
               "Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el."
               "Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el."
               "Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el.x"
            );
            md2_record_ex_t md_record;
            tranger2_append_record(tranger, TOPIC_NAME, tm+j, 0, &md_record, jn_record1);
        }
    }

    MT_INCREMENT_COUNT(time_measure, MAX_KEYS*MAX_RECORDS)
    MT_PRINT_TIME(time_measure, "tranger2_append_record  (old 130.000 op/sec)")

    result += test_json(NULL);  // NULL: we want to check only the logs

    /*------------------------------------------*
     *  Check tranger memory with lists opened
     *  and records added
     *------------------------------------------*/
    if(1) {
        char expected[32*1024];
        snprintf(expected, sizeof(expected), "\
        { \
            'path': '%s', \
            'database': '%s', \
            'filename_mask': '%%Y', \
            'xpermission': 1528, \
            'rpermission': 384, \
            'on_critical_error': 0, \
            'master': true, \
            'gobj': 0, \
            'trace_level': 1, \
            'directory': '%s', \
            'fd_opened_files': { \
                '__timeranger2__.json': 9999 \
            }, \
            'yev_loop': 0, \
            'topics': { \
                '%s': { \
                    'topic_name': '%s', \
                    'pkey': 'id', \
                    'tkey': 'tm', \
                    'system_flag': 4, \
                    'filename_mask': '%%Y-%%m-%%d', \
                    'xpermission': 1472, \
                    'rpermission': 384, \
                    'cols': { \
                        'id': '', \
                        'tm': 0, \
                        'content': '' \
                    }, \
                    'directory': '%s', \
                    'wr_fd_files': {\
                        '0000000000000000001': { \
                            '2000-01-02.json': 99999, \
                            '2000-01-02.md2': 99999 \
                        }, \
                        '0000000000000000002': { \
                            '2000-01-02.json': 99999, \
                            '2000-01-02.md2': 99999 \
                        } \
                    }, \
                    'rd_fd_files': {}, \
                    'cache': { \
                        '0000000000000000001': { \
                            'files': [ \
                                { \
                                    'id': '2000-01-01', \
                                    'fr_t': 946684800, \
                                    'to_t': 946771199, \
                                    'fr_tm': 70369690862464, \
                                    'to_tm': 70369690948863, \
                                    'rows': 86400 \
                                }, \
                                { \
                                    'id': '2000-01-02', \
                                    'fr_t': 946771200, \
                                    'to_t': 946774799, \
                                    'fr_tm': 70369690948864, \
                                    'to_tm': 70369690952463, \
                                    'rows': 3600 \
                                } \
                            ], \
                            'total': { \
                                'fr_t': 946684800, \
                                'to_t': 946774799, \
                                'fr_tm': 70369690862464, \
                                'to_tm': 70369690952463, \
                                'rows': 90000 \
                            } \
                        }, \
                        '0000000000000000002': { \
                            'files': [ \
                                { \
                                    'id': '2000-01-01', \
                                    'fr_t': 946684800, \
                                    'to_t': 946771199, \
                                    'fr_tm': 70369690862464, \
                                    'to_tm': 70369690948863, \
                                    'rows': 86400 \
                                }, \
                                { \
                                    'id': '2000-01-02', \
                                    'fr_t': 946771200, \
                                    'to_t': 946774799, \
                                    'fr_tm': 70369690948864, \
                                    'to_tm': 70369690952463, \
                                    'rows': 3600 \
                                } \
                            ], \
                            'total': { \
                                'fr_t': 946684800, \
                                'to_t': 946774799, \
                                'fr_tm': 70369690862464, \
                                'to_tm': 70369690952463, \
                                'rows': 90000 \
                            } \
                        } \
                    }, \
                    'lists': [ \
                        { \
                            'id': 'list1', \
                            'creator': '', \
                            'topic_name': '%s', \
                            'key': '', \
                            'match_cond': {}, \
                            'load_record_callback': 99999, \
                            'list_type': 'rt_mem'\
                        }, \
                        { \
                            'id': 'list2', \
                            'creator': '', \
                            'topic_name': '%s', \
                            'key': '0000000000000000001', \
                            'match_cond': {}, \
                            'load_record_callback': 99999, \
                            'list_type': 'rt_mem' \
                        } \
                    ], \
                    'disks': [], \
                    'iterators': [] \
                } \
            } \
        } \
        ", path_root, DATABASE, path_database, TOPIC_NAME, TOPIC_NAME, path_topic, TOPIC_NAME, TOPIC_NAME);

        const char *ignore_keys[]= {
            "__timeranger2__.json",
            "load_record_callback",
            "2000-01-02.json",
            "2000-01-02.md2",
            NULL
        };
        json_t *expected_ = string2json(helper_quote2doublequote(expected), TRUE);
        if(!expected_) {
            result += -1;
        }
        set_expected_results(
            "check_tranger_mem4",      // test name
            NULL,
            expected_,
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger));
    }

    /*-------------------------------------*
     *      Close rt lists
     *-------------------------------------*/
    set_expected_results( // Check that no logs happen
        "close rt lists", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    tranger2_close_rt_mem(
        tranger,
        tr_list
    );

    json_t *list2 = tranger2_get_rt_mem_by_id(
        tranger,
        TOPIC_NAME,
        "list2",
        ""
    );
    tranger2_close_rt_mem(
        tranger,
        list2
    );

    result += test_json(NULL);  // NULL: we want to check only the logs

    if(all_leidos != MAX_KEYS*MAX_RECORDS) {
        printf("%sRecords read not match%s, leidos %d, records %d\n", On_Red BWhite,Color_Off,
           (int)all_leidos, MAX_KEYS*MAX_RECORDS
        );
        result += -1;
    }

    if(one_leidos != MAX_RECORDS) {
        printf("%sRecords read not match%s, leidos %d, records %d\n", On_Red BWhite,Color_Off,
            (int)one_leidos, MAX_RECORDS
        );
        result += -1;
    }

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
    result += test_json(NULL);  // NULL: we want to check only the logs

    /*-------------------------------*
     *      Shutdown timeranger
     *-------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger2_shutdown", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );
    tranger2_shutdown(tranger);
    result += test_json(NULL);  // NULL: we want to check only the logs

    return result;
}

/***************************************************************************
 *              Test2
 *  Open as master and change version and cols
 *  HACK: return -1 to fail, 0 to ok
 ***************************************************************************/
int do_test2(void)
{
    int result = 0;
    char file[PATH_MAX];

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
    build_path(path_topic, sizeof(path_topic), path_database, TOPIC_NAME, NULL);

    /*-------------------------------------------------*
     *      Startup the timeranger db
     *-------------------------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger2_startup 2", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", 0
    );
    json_t *tranger = tranger2_startup(0, jn_tranger, 0);
    result += test_json(NULL);  // NULL: we want to check only the logs

    /*-------------------------------------------------*
     *  Create a topic (already created, idempotent)
     *  But changing the cols and version
     *-------------------------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger2_create_topic 2", // test name
        json_pack("[{s:s},{s:s}]", // error's list
            "msg", "Re-Creating topic_var.json",
            "msg", "Re-Creating topic_cols.json"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    json_t *jn_cols = json_pack("{s:s, s:I, s:s, s:s}", // jn_cols, owned
        "id", "",
        "tm", (json_int_t)0,
        "content", "",
        "content2", ""
    );
    json_t *jn_var =json_pack("{s:i}",            // var
        "topic_version", 1
    );
    json_t *topic = tranger2_create_topic(
        tranger,
        TOPIC_NAME,     // topic name
        NULL,           // pkey
        NULL,           // tkey
        NULL,           // jn_topic_ext
        0,              // system_flag
        jn_cols,
        jn_var
    );
    result += test_json(NULL);  // NULL: we want to check only the logs
    if(!topic) {
        tranger2_shutdown(tranger);
        return -1;
    }

    /*------------------------------------*
     *  Check "topic_cols.json" file
     *------------------------------------*/
    build_path(file, sizeof(file), path_topic, "topic_cols.json", NULL);
    if(1) {
        char expected[]= "\
        { \
          'id': '', \
          'tm': 0, \
          'content': '', \
          'content2': '' \
        } \
        ";

        set_expected_results(
            "check_topic_cols.json 2",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            NULL,
            TRUE
        );
        result += test_json_file(file);
    }

    /*------------------------------------*
     *  Check "topic_var.json" file
     *------------------------------------*/
    build_path(file, sizeof(file), path_topic, "topic_var.json", NULL);
    if(1) {
        char expected[]= "\
        { \
            'topic_version': 1 \
        } \
        ";

        set_expected_results(
            "check_topic_var.json 2",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            NULL,
            TRUE
        );
        result += test_json_file(file);
    }

    /*------------------------------------------*
     *  Check tranger memory with topic opened
     *------------------------------------------*/
    if(1) {
        char expected[16*1024];
        snprintf(expected, sizeof(expected), "\
        { \
            'path': '%s', \
            'database': '%s', \
            'filename_mask': '%%Y', \
            'xpermission': 1528, \
            'rpermission': 384, \
            'on_critical_error': 0, \
            'master': true, \
            'gobj': 0, \
            'trace_level': 0, \
            'directory': '%s', \
            'fd_opened_files': { \
                '__timeranger2__.json': 9999 \
            }, \
            'yev_loop': 0, \
            'topics': { \
                '%s': { \
                    'topic_name': '%s', \
                    'pkey': 'id', \
                    'tkey': 'tm', \
                    'system_flag': 4, \
                    'filename_mask': '%%Y-%%m-%%d', \
                    'xpermission': 1472, \
                    'rpermission': 384, \
                    'topic_version': 1, \
                    'cols': { \
                        'id': '', \
                        'tm': 0, \
                        'content': '', \
                        'content2': '' \
                    }, \
                    'directory': '%s', \
                    'wr_fd_files': {}, \
                    'rd_fd_files': {}, \
                    'cache': { \
                        '0000000000000000001': { \
                            'files': [ \
                                { \
                                    'id': '2000-01-01', \
                                    'fr_t': 946684800, \
                                    'to_t': 946771199, \
                                    'fr_tm': 946684800, \
                                    'to_tm': 946771199, \
                                    'rows': 86400 \
                                }, \
                                { \
                                    'id': '2000-01-02', \
                                    'fr_t': 946771200, \
                                    'to_t': 946774799, \
                                    'fr_tm': 946771200, \
                                    'to_tm': 946774799, \
                                    'rows': 3600 \
                                } \
                            ], \
                            'total': { \
                                'fr_t': 946684800, \
                                'to_t': 946774799, \
                                'fr_tm': 946684800, \
                                'to_tm': 946774799, \
                                'rows': 90000 \
                            } \
                        }, \
                        '0000000000000000002': { \
                            'files': [ \
                                { \
                                    'id': '2000-01-01', \
                                    'fr_t': 946684800, \
                                    'to_t': 946771199, \
                                    'fr_tm': 946684800, \
                                    'to_tm': 946771199, \
                                    'rows': 86400 \
                                }, \
                                { \
                                    'id': '2000-01-02', \
                                    'fr_t': 946771200, \
                                    'to_t': 946774799, \
                                    'fr_tm': 946771200, \
                                    'to_tm': 946774799, \
                                    'rows': 3600 \
                                } \
                            ], \
                            'total': { \
                                'fr_t': 946684800, \
                                'to_t': 946774799, \
                                'fr_tm': 946684800, \
                                'to_tm': 946774799, \
                                'rows': 90000 \
                            } \
                        } \
                    }, \
                    'lists': [], \
                    'disks': [], \
                    'iterators': [] \
                } \
            } \
        } \
        ", path_root, DATABASE, path_database, TOPIC_NAME, TOPIC_NAME, path_topic);

        const char *ignore_keys[]= {
            "__timeranger2__.json",
            NULL
        };
        json_t *expected_ = string2json(helper_quote2doublequote(expected), TRUE);
        if(!expected_) {
            result += -1;
        }
        set_expected_results(
            "check_tranger_mem5",      // test name
            NULL,
            expected_,
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger));
    }

    /*-------------------------------*
     *      Shutdown timeranger
     *-------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger2_shutdown 2", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );
    tranger2_shutdown(tranger);
    result += test_json(NULL);  // NULL: we want to check only the logs

    /*-------------------------------*
     *      Check disk structure
     *-------------------------------*/
    if(1) {
        int ret;
        char path_key[PATH_MAX];

        ret = test_directory_permission(path_topic, 0770);
        result += ret;
        if(ret < 0) {
            printf("%s BAD directory_permission: %s %s\n", On_Red BWhite, path_topic, Color_Off);
        }

        build_path(path_key, sizeof(path_key),
            path_topic,
            "keys",
            "0000000000000000001",
            NULL
        );
        ret = test_directory_permission(path_key, 0700);
        if(ret < 0) {
            printf("%s BAD directory_permission: %s %s\n", On_Red BWhite, path_key, Color_Off);
        }
        result += ret;

        build_path(path_key, sizeof(path_key),
            path_topic,
            "keys",
            "0000000000000000002",
            NULL
        );
        ret = test_directory_permission(path_key, 0700);
        if(ret < 0) {
            printf("%s BAD directory_permission: %s %s\n", On_Red BWhite, path_key, Color_Off);
        }
        result += ret;

        char path_key_file[PATH_MAX];

        build_path(path_key_file, sizeof(path_key_file),
            path_topic,
            "keys",
            "0000000000000000001",
            "2000-01-01.json",
            NULL
        );
        ret = test_file_permission_and_size(path_key_file, 0600, 28944000);
        if(ret < 0) {
            printf("%s BAD file permission or size: %s %s\n", On_Red BWhite, path_key_file, Color_Off);
        }
        result += ret;

        build_path(path_key_file, sizeof(path_key_file),
            path_topic,
            "keys",
            "0000000000000000001",
            "2000-01-01.md2",
            NULL
        );
        ret = test_file_permission_and_size(path_key_file, 0600, 2764800);
        if(ret < 0) {
            printf("%s BAD file permission or size: %s %s\n", On_Red BWhite, path_key_file, Color_Off);
        }
        result += ret;

        build_path(path_key_file, sizeof(path_key_file),
            path_topic,
            "keys",
            "0000000000000000001",
            "2000-01-02.json",
            NULL
        );
        ret = test_file_permission_and_size(path_key_file, 0600, 1206000);
        if(ret < 0) {
            printf("%s BAD file permission or size: %s %s\n", On_Red BWhite, path_key_file, Color_Off);
        }
        result += ret;

        build_path(path_key_file, sizeof(path_key_file),
            path_topic,
            "keys",
            "0000000000000000001",
            "2000-01-02.md2",
            NULL
        );
        ret = test_file_permission_and_size(path_key_file, 0600, 115200);
        if(ret < 0) {
            printf("%s BAD file permission or size: %s %s\n", On_Red BWhite, path_key_file, Color_Off);
        }
        result += ret;
    }

    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*----------------------------------*
     *      Startup gobj system
     *----------------------------------*/
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gbmem_get_allocators(
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

    unsigned long memory_check_list[] = {0, 0}; // WARNING: list ended with 0
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(
        argc,
        argv,
        NULL,   // jn_global_settings
        NULL,   // persistent_attrs
        NULL,   // global_command_parser
        NULL,   // global_stats_parser
        NULL,   // global_authz_checker
        NULL    // global_authenticate_parser
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

    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_create(
        0,
        2024,
        10,
        NULL,
        &yev_loop
    );

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    int result = do_test();
    result += do_test2();
    result += check_rowids();
    result += global_result;

    /*--------------------------------*
     *  Stop the event loop
     *--------------------------------*/
    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory()!=0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result<0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    }
    return result<0?-1:0;
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int xtimes_once = 0;
    xtimes_once++;
    yev_loop_reset_running(yev_loop);
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
