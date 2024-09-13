/****************************************************************************
 *          test_topic_pkey_integer.c
 *
 *  - do_test: Open as master, check main files, open rt lists and ADD RECORDS
 *  - do_test2: Open as master and change topic version and cols
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <gobj.h>
#include <timeranger2.h>
#include <stacktrace_with_bfd.h>
#include <yunetas_ev_loop.h>
#include <testing.h>

#define DATABASE    "tr_topic_pkey_integer"
#define TOPIC_NAME  "topic_pkey_integer"
#define MAX_KEYS    2
#define MAX_RECORDS 90000 // 1 day and 1 hour

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_t *yev_loop;
yev_event_t *yev_event_once;
yev_event_t *yev_event_periodic;
int wait_time = 1;
int times_once = 0;
int times_periodic = 0;

/***************************************************************************
 *
 ***************************************************************************/
size_t all_leidos = 0;
int all_load_record_callback(
    json_t *tranger,
    json_t *topic,
    json_t *match_cond,     // not yours, don't own
    md2_record_t *md2_record,
    json_t *jn_record,      // must be owned
    const char *key,
    json_int_t relative_rowid
)
{
    all_leidos++;
    JSON_DECREF(jn_record)
    return 0;
}

size_t one_leidos = 0;
int one_load_record_callback(
    json_t *tranger,
    json_t *topic,
    json_t *match_cond, // not yours, don't own
    md2_record_t *md2_record,
    json_t *jn_record,  // must be owned
    const char *key,
    json_int_t relative_rowid
)
{
    one_leidos++;
    JSON_DECREF(jn_record)
    return 0;
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
    json_t *tranger = tranger2_startup(0, jn_tranger);

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
        result += test_json_file(file, result);
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
        sf2_int_key,  // system_flag
        json_pack("{s:s, s:I, s:s}", // jn_cols, owned
            "id", "",
            "tm", (json_int_t)0,
            "content", ""
        ),
        0
    );
    result += test_json(NULL, result);  // NULL: we want to check only the logs
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
        result += test_json_file(file, result);
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
        result += test_json_file(file, result);
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
        result += test_json_file(file, result);
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
                    'cache': { \
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
        set_expected_results(
            "check_tranger_mem1",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger), result);
    }

    /*-------------------------------------*
     *      Add records
     *-------------------------------------*/
    set_expected_results( // Check that no logs happen
        "append records without open_rt_list", // test name
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
            md2_record_t md_record;
            tranger2_append_record(tranger, TOPIC_NAME, tm+j, 0, &md_record, jn_record1);
        }
    }

    MT_INCREMENT_COUNT(time_measure, MAX_KEYS*MAX_RECORDS)
    MT_PRINT_TIME(time_measure, "tranger2_append_record")

    result += test_json(NULL, result);  // NULL: we want to check only the logs

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
    result += test_json(NULL, result);  // NULL: we want to check only the logs

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
            'topics': { \
            } \
        } \
        ", path_root, DATABASE, path_database);

        const char *ignore_keys[]= {
            "__timeranger2__.json",
            NULL
        };
        set_expected_results(
            "check_tranger_mem2",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger), result);
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
        sf2_int_key,    // system_flag
        json_pack("{s:s, s:I, s:s}", // jn_cols, owned
            "id", "",
            "tm", (json_int_t)0,
            "content", ""
        ),
        0
    );
    result += test_json(NULL, result);  // NULL: we want to check only the logs
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

    json_t *tr_list = tranger2_open_rt_list(
        tranger,
        TOPIC_NAME,
        "",             // key
        NULL,           // match_cond,
        all_load_record_callback,
        "list1"
    );

    tranger2_open_rt_list(
        tranger,
        TOPIC_NAME,
        "0000000000000000001",       // key
        NULL,   // match_cond
        one_load_record_callback,
        "list2"
    );

    result += test_json(NULL, result);  // NULL: we want to check only the logs

    /*------------------------------------------*
     *  Check tranger memory with lists opened
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
                    'cache': { \
                    }, \
                    'lists': [ \
                        { \
                            'id': 'list1', \
                            'topic_name': '%s', \
                            'key': '', \
                            'match_cond': {}, \
                            'load_record_callback': 99999 \
                        }, \
                        { \
                            'id': 'list2', \
                            'topic_name': '%s', \
                            'key': '0000000000000000001', \
                            'match_cond': {}, \
                            'load_record_callback': 99999 \
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
        set_expected_results(
            "check_tranger_mem3",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger), result);
    }

    /*-------------------------------------*
     *      Add records
     *-------------------------------------*/
    set_expected_results( // Check that no logs happen
        "append records with open_rt_list", // test name
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
            md2_record_t md_record;
            tranger2_append_record(tranger, TOPIC_NAME, tm+j, 0, &md_record, jn_record1);
        }
    }

    MT_INCREMENT_COUNT(time_measure, MAX_KEYS*MAX_RECORDS)
    MT_PRINT_TIME(time_measure, "tranger2_append_record")

    result += test_json(NULL, result);  // NULL: we want to check only the logs

    /*------------------------------------------*
     *  Check tranger memory with lists opened
     *  and records added
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
                    }, \
                    'lists': [ \
                        { \
                            'id': 'list1', \
                            'topic_name': '%s', \
                            'key': '', \
                            'match_cond': {}, \
                            'load_record_callback': 99999 \
                        }, \
                        { \
                            'id': 'list2', \
                            'topic_name': '%s', \
                            'key': '0000000000000000001', \
                            'match_cond': {}, \
                            'load_record_callback': 99999 \
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
        set_expected_results(
            "check_tranger_mem4",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger), result);
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

    tranger2_close_rt_list(
        tranger,
        tr_list
    );

    json_t *list2 =tranger2_get_rt_list_by_id(
        tranger,
        "list2"
    );
    tranger2_close_rt_list(
        tranger,
        list2
    );

    result += test_json(NULL, result);  // NULL: we want to check only the logs

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
    json_t *tranger = tranger2_startup(0, jn_tranger);
    result += test_json(NULL, result);  // NULL: we want to check only the logs

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
    result += test_json(NULL, result);  // NULL: we want to check only the logs
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
        result += test_json_file(file, result);
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
        result += test_json_file(file, result);
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
        set_expected_results(
            "check_tranger_mem5",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger), result);
    }

    /*-------------------------------*
     *      Shutdown timeranger
     *-------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger_shutdown 2", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );
    tranger2_shutdown(tranger);
    result += test_json(NULL, result);  // NULL: we want to check only the logs

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

    unsigned long memory_check_list[] = {1627, 1628, 1769, 0}; // WARNING: list ended with 0
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
        64*1024L,    // max_block, largest memory block
        256*1024L   // max_system_memory, maximum system memory
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
    result += do_test2();

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
