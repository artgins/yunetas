/****************************************************************************
 *          test_topic_pkey_integer_iterator.c
 *
 *  - do_test: Open as master, open iterator (realtime by disk) without callback
 *  - do_test2: Open as master, open iterator (realtime by disk) with callback
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
#define MAX_RECS    10 // Records to find

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
int global_result = 0;
json_int_t tm = 0;
uint64_t t = 0;
uint64_t total_rows = 0;
int leidos = 0;

int iterator_callback1(
    json_t *tranger,
    json_t *topic,
    json_t *match_cond,     // not yours, don't own
    md2_record_t *md2_record,
    json_t *record,      // must be owned
    const char *key,
    json_int_t relative_rowid
)
{
    t++;
    tm++;
    total_rows++;

    json_int_t tm_ = kw_get_int(0, record, "tm", 0, KW_REQUIRED);
    if(tm_ != tm) {
        global_result += -1;
        printf("%sERROR --> %s%s\n", On_Red BWhite, "BAD count tm 1 of message", Color_Off);
        JSON_DECREF(record)
        return -1;
    }
    if(md2_record->__tm__ != tm) {
        global_result += -1;
        printf("%sERROR --> %s%s\n", On_Red BWhite, "BAD count tm 2 of message", Color_Off);
        JSON_DECREF(record)
        return -1;
    }

    uint64_t t_ = md2_record->__t__;
    if(t_ != t) {
        global_result += -1;
        printf("%sERROR --> %s%s\n", On_Red BWhite, "BAD count t of message", Color_Off);
        JSON_DECREF(record)
        return -1;
    }

    JSON_DECREF(record)
    return 0;
}

int iterator_callback2(
    json_t *tranger,
    json_t *topic,
    json_t *match_cond,     // not yours, don't own
    md2_record_t *md2_record,
    json_t *record,      // must be owned
    const char *key,
    json_int_t relative_rowid
)
{
    t++;
    tm++;
    total_rows++;

    json_int_t tm_ = kw_get_int(0, record, "tm", 0, KW_REQUIRED);
    if(tm_ != tm) {
        global_result += -1;
        printf("%sERROR --> %s%s\n", On_Red BWhite, "BAD count tm 1 of message", Color_Off);
        JSON_DECREF(record)
        return -1;
    }
    if(md2_record->__tm__ != tm) {
        global_result += -1;
        printf("%sERROR --> %s%s\n", On_Red BWhite, "BAD count tm 2 of message", Color_Off);
        JSON_DECREF(record)
        return -1;
    }

    uint64_t t_ = md2_record->__t__;
    if(t_ != t) {
        global_result += -1;
        printf("%sERROR --> %s%s\n", On_Red BWhite, "BAD count t of message", Color_Off);
        JSON_DECREF(record)
        return -1;
    }

    JSON_DECREF(record)
    return 0;
}

int load_rango_callback(
    json_t *tranger,
    json_t *topic,
    json_t *match_cond,     // not yours, don't own
    md2_record_t *md2_record,
    json_t *record,      // must be owned
    const char *key,
    json_int_t relative_rowid
)
{
    leidos++;
    if(1) { //pinta_rows) {
        printf("rowid = %"PRIu64"\n", (uint64_t)relative_rowid);
    }
    JSON_DECREF(record);
    return 1; // add to returned list
}

/***************************************************************************
 *              Test
 *  Open as master, open iterator (realtime by disk) without callback
 *  HACK: return -1 to fail, 0 to ok
 ***************************************************************************/
int do_test(void)
{
    int result = 0;
    global_result = 0;

    /*
     *  Write the tests in ~/tests_yuneta/
     */
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    char path_topic[PATH_MAX];

    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    build_path(path_topic, sizeof(path_topic), path_database, TOPIC_NAME, NULL);

    /*-------------------------------------------------*
     *      Startup the timeranger db
     *-------------------------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger2_startup", // test name
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
    result += test_json(NULL);  // NULL: we want to check only the logs

    /*-------------------------------------------------*
     *      Open the topic
     *-------------------------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger2_open_topic", // test name
        NULL,   // error's list
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    json_t *topic = tranger2_open_topic(
        tranger,
        TOPIC_NAME,     // topic name
        TRUE
    );
    result += test_json(NULL);  // NULL: we want to check only the logs
    if(!topic) {
        tranger2_shutdown(tranger);
        return -1;
    }

    /*--------------------------------------------------------------------*
     *  Create an iterator, no callback, match_cond NULL (use defaults)
     *--------------------------------------------------------------------*/
    set_expected_results( // Check that no logs happen
        "create iterator", // test name
        NULL,   // error's list
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );
    json_t *iterator = tranger2_open_iterator(
        tranger,
        topic,
        "0000000000000000001",     // key,
        NULL,   // match_cond, owned
        NULL,   // load_record_callback
        NULL,   // iterator id
        NULL    // data
    );
    result += test_json(NULL);  // NULL: we want to check only the logs
    if(!iterator) {
        tranger2_shutdown(tranger);
        return -1;
    }

    /*---------------------------------------------*
     *  Check tranger memory with iterator opened
     *---------------------------------------------*/
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
                        '0000000000000000001': { \
                            'files': [ \
                                { \
                                    'id': '2000-01-01', \
                                    'fr_t': 946684800, \
                                    'to_t': 946771199, \
                                    'fr_tm': 946684800, \
                                    'to_tm': 946771199, \
                                    'rows': 86400, \
                                    'wr_time': 9999 \
                                }, \
                                { \
                                    'id': '2000-01-02', \
                                    'fr_t': 946771200, \
                                    'to_t': 946774799, \
                                    'fr_tm': 946771200, \
                                    'to_tm': 946774799, \
                                    'rows': 3600, \
                                    'wr_time': 9999 \
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
                                    'rows': 86400, \
                                    'wr_time': 9999 \
                                }, \
                                { \
                                    'id': '2000-01-02', \
                                    'fr_t': 946771200, \
                                    'to_t': 946774799, \
                                    'fr_tm': 946771200, \
                                    'to_tm': 946774799, \
                                    'rows': 3600, \
                                    'wr_time': 9999 \
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
                    'iterators': [\
                        { \
                            'id': '0000000000000000001', \
                            'key': '0000000000000000001', \
                            'topic_name': '%s', \
                            'match_cond': {}, \
                            'segments': [ \
                                { \
                                    'id': '2000-01-01', \
                                    'fr_t': 946684800, \
                                    'to_t': 946771199, \
                                    'fr_tm': 946684800, \
                                    'to_tm': 946771199, \
                                    'rows': 86400, \
                                    'wr_time': 99999, \
                                    'first_row': 1, \
                                    'last_row': 86400, \
                                    'key': '0000000000000000001' \
                                }, \
                                { \
                                    'id': '2000-01-02', \
                                    'fr_t': 946771200, \
                                    'to_t': 946774799, \
                                    'fr_tm': 946771200, \
                                    'to_tm': 946774799, \
                                    'rows': 3600, \
                                    'wr_time': 99999, \
                                    'first_row': 86401, \
                                    'last_row': 90000, \
                                    'key': '0000000000000000001' \
                                } \
                            ], \
                            'cur_segment': 0, \
                            'cur_rowid': 0, \
                            'load_record_callback': 0 \
                        } \
                    ] \
                } \
            } \
        } \
        ", path_root, DATABASE, path_database, TOPIC_NAME, TOPIC_NAME, path_topic, TOPIC_NAME);

        const char *ignore_keys[]= {
            "__timeranger2__.json",
            "wr_time",
            NULL
        };
        set_expected_results(
            "check tranger mem, with iterator open",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger));
    }

    /*-------------------------------*
     *      Close iterator
     *-------------------------------*/
    set_expected_results( // Check that no logs happen
        "close iterator", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );
    result += tranger2_close_iterator(tranger, iterator);
    result += test_json(NULL);  // NULL: we want to check only the logs

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
    result += test_json(NULL);  // NULL: we want to check only the logs

    result += global_result;

    return result;
}

/***************************************************************************
 *              Test
 *  Open as master, open iterator (realtime by disk) with callback
 *  HACK: return -1 to fail, 0 to ok
 ***************************************************************************/
int do_test2(void)
{
    int result = 0;
    global_result = 0;

    /*
     *  Write the tests in ~/tests_yuneta/
     */
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    char path_topic[PATH_MAX];

    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    build_path(path_topic, sizeof(path_topic), path_database, TOPIC_NAME, NULL);

    /*-------------------------------------------------*
     *      Startup the timeranger db
     *-------------------------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger2_startup", // test name
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
    result += test_json(NULL);  // NULL: we want to check only the logs

    /*--------------------------------------------------------------------*
     *  Create an iterator, no callback, match_cond NULL (use defaults)
     *--------------------------------------------------------------------*/
    set_expected_results( // Check that no logs happen
        "open iterator", // test name
        NULL,   // error's list
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    total_rows = 0;
    time_measure_t time_measure;
    MT_START_TIME(time_measure)

    /*
     *  Open iterator to key1
     */
    t = tm = 946684800-1;
    tranger2_open_iterator(
        tranger,
        tranger2_topic(tranger, TOPIC_NAME),
        "0000000000000000001",     // key,
        NULL,   // match_cond, owned
        iterator_callback1,    // load_record_callback
        "it1",
        NULL
    );

    json_t *iterator1 = tranger2_get_iterator_by_id(tranger, "it1");

    if(tm != 946774799) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "BAD count tm of message", Color_Off);
        print_json2("BAD count tm of message", tranger);
        result += -1;
    }
    if(t != 946774799) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "BAD count t of message", Color_Off);
        print_json2("BAD count t of message", tranger);
        result += -1;
    }

    /*
     *  Open iterator to key2
     */
    t = tm = 946684800-1;
    json_t *iterator2 = tranger2_open_iterator(
        tranger,
        tranger2_topic(tranger, TOPIC_NAME),
        "0000000000000000002",     // key,
        NULL,   // match_cond, owned
        iterator_callback2,    // load_record_callback
        NULL,   // id
        NULL    // data
    );

    if(tm != 946774799) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "BAD count tm of message", Color_Off);
        print_json2("BAD count tm of message", tranger);
        result += -1;
    }
    if(t != 946774799) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "BAD count t of message", Color_Off);
        print_json2("BAD count t of message", tranger);
        result += -1;
    }

    /*
     *  Check totals
     */
    if(total_rows != MAX_KEYS*MAX_RECORDS) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "BAD count total_rows of message", Color_Off);
        print_json2("BAD count total_rows of message", tranger);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, MAX_KEYS*MAX_RECORDS)
    MT_PRINT_TIME(time_measure, "tranger2_open_iterator")

    result += test_json(NULL);  // NULL: we want to check only the logs

    /*---------------------------------------------*
     *  Repeat Open iterator to key2
     *---------------------------------------------*/
    set_expected_results( // Check that no logs happen
        "repeat open iterator", // test name
        json_pack("[{s:s}]", // error's list
                  "msg", "tranger2_open_iterator(): Iterator already exists"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );
    json_t *iterator22 = tranger2_open_iterator(
        tranger,
        tranger2_topic(tranger, TOPIC_NAME),
        "0000000000000000002",     // key,
        NULL,   // match_cond, owned
        NULL,   // load_record_callback
        NULL,   // id
        NULL    // data
    );
    if(iterator22) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "Repeat iterator must be null", Color_Off);
        result += -1;
    }
    result += test_json(NULL);  // NULL: we want to check only the logs

    /*---------------------------------------------*
     *  Check iterator mem
     *---------------------------------------------*/
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
                    'rd_fd_files': { \
                        '0000000000000000001': { \
                            '2000-01-01.md2': 9999, \
                            '2000-01-01.json': 9999, \
                            '2000-01-02.md2': 9999, \
                            '2000-01-02.json': 9999 \
                        }, \
                        '0000000000000000002': { \
                            '2000-01-01.md2': 9999, \
                            '2000-01-01.json': 9999, \
                            '2000-01-02.md2': 9999, \
                            '2000-01-02.json': 9999 \
                        } \
                    }, \
                    'cache': { \
                        '0000000000000000001': { \
                            'files': [ \
                                { \
                                    'id': '2000-01-01', \
                                    'fr_t': 946684800, \
                                    'to_t': 946771199, \
                                    'fr_tm': 946684800, \
                                    'to_tm': 946771199, \
                                    'rows': 86400, \
                                    'wr_time': 9999 \
                                }, \
                                { \
                                    'id': '2000-01-02', \
                                    'fr_t': 946771200, \
                                    'to_t': 946774799, \
                                    'fr_tm': 946771200, \
                                    'to_tm': 946774799, \
                                    'rows': 3600, \
                                    'wr_time': 9999 \
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
                                    'rows': 86400, \
                                    'wr_time': 9999 \
                                }, \
                                { \
                                    'id': '2000-01-02', \
                                    'fr_t': 946771200, \
                                    'to_t': 946774799, \
                                    'fr_tm': 946771200, \
                                    'to_tm': 946774799, \
                                    'rows': 3600, \
                                    'wr_time': 9999 \
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
                    'disks': [ \
                        { \
                            'id': 'it1', \
                            'topic_name': 'topic_pkey_integer', \
                            'key': '0000000000000000001', \
                            'match_cond': {}, \
                            'load_record_callback': 99999 \
                        }, \
                        { \
                            'id': '0000000000000000002', \
                            'topic_name': 'topic_pkey_integer', \
                            'key': '0000000000000000002', \
                            'match_cond': {}, \
                            'load_record_callback': 99999 \
                        } \
                    ], \
                    'iterators': [\
                        { \
                            'id': 'it1', \
                            'key': '0000000000000000001', \
                            'topic_name': '%s', \
                            'match_cond': {}, \
                            'segments': [ \
                                { \
                                    'id': '2000-01-01', \
                                    'fr_t': 946684800, \
                                    'to_t': 946771199, \
                                    'fr_tm': 946684800, \
                                    'to_tm': 946771199, \
                                    'rows': 86400, \
                                    'wr_time': 99999, \
                                    'first_row': 1, \
                                    'last_row': 86400, \
                                    'key': '0000000000000000001' \
                                }, \
                                { \
                                    'id': '2000-01-02', \
                                    'fr_t': 946771200, \
                                    'to_t': 946774799, \
                                    'fr_tm': 946771200, \
                                    'to_tm': 946774799, \
                                    'rows': 3600, \
                                    'wr_time': 99999, \
                                    'first_row': 86401, \
                                    'last_row': 90000, \
                                    'key': '0000000000000000001' \
                                } \
                            ], \
                            'cur_segment': 1, \
                            'cur_rowid': 90000, \
                            'load_record_callback': 9999 \
                        }, \
                        { \
                            'id': '0000000000000000002', \
                            'key': '0000000000000000002', \
                            'topic_name': '%s', \
                            'match_cond': {}, \
                            'segments': [ \
                                { \
                                    'id': '2000-01-01', \
                                    'fr_t': 946684800, \
                                    'to_t': 946771199, \
                                    'fr_tm': 946684800, \
                                    'to_tm': 946771199, \
                                    'rows': 86400, \
                                    'wr_time': 99999, \
                                    'first_row': 1, \
                                    'last_row': 86400, \
                                    'key': '0000000000000000002' \
                                }, \
                                { \
                                    'id': '2000-01-02', \
                                    'fr_t': 946771200, \
                                    'to_t': 946774799, \
                                    'fr_tm': 946771200, \
                                    'to_tm': 946774799, \
                                    'rows': 3600, \
                                    'wr_time': 99999, \
                                    'first_row': 86401, \
                                    'last_row': 90000, \
                                    'key': '0000000000000000002' \
                                } \
                            ], \
                            'cur_segment': 1, \
                            'cur_rowid': 90000, \
                            'load_record_callback': 9999 \
                        } \
                    ] \
                } \
            } \
        } \
        ", path_root, DATABASE, path_database, TOPIC_NAME, TOPIC_NAME, path_topic, TOPIC_NAME, TOPIC_NAME);

        const char *ignore_keys[]= {
            "__timeranger2__.json",
            "wr_time",
            "load_record_callback",
            "2000-01-01.md2",
            "2000-01-01.json",
            "2000-01-02.md2",
            "2000-01-02.json",
            NULL
        };
        set_expected_results(
            "check iterator mem",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger));
    }

    /*-------------------------------*
     *      Close iterators
     *-------------------------------*/
    set_expected_results( // Check that no logs happen
        "close iterators", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );
    result += tranger2_close_iterator(tranger, iterator1);
    result += tranger2_close_iterator(tranger, iterator2);
    result += test_json(NULL);  // NULL: we want to check only the logs

    /*-------------------------------------*
     *  Search absolute range, forward
     *-------------------------------------*/
    if(1) {
        const char *test_name = "Search absolute range, forward";
        set_expected_results( // Check that no logs happen
            test_name, // test name
            NULL,   // error's list, It must not be any log error
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );

        json_int_t from_rowid = MAX_RECORDS/2 + 1;
        json_int_t to_rowid = MAX_RECORDS/2 + MAX_RECS;

        MT_START_TIME(time_measure)

        leidos = 0;
        json_t *topic = tranger2_topic(tranger, TOPIC_NAME);
        json_t *match_cond = json_pack("{s:I, s:I}",
            "from_rowid", (json_int_t)from_rowid,
            "to_rowid", (json_int_t)to_rowid
        );
        json_t *iterator = tranger2_open_iterator(
            tranger,
            topic,
            "0000000000000000001",  // key
            match_cond,             // match_cond, owned
            load_rango_callback,    // load_record_callback
            NULL,                   // id
            NULL                    // data
        );

        MT_INCREMENT_COUNT(time_measure, MAX_KEYS*MAX_RECORDS)
        MT_PRINT_TIME(time_measure, test_name)

        result += tranger2_close_iterator(tranger, iterator);

//        uint64_t result[MAX_RECS];
//        int max = sizeof(result)/sizeof(result[0]);
//        for(int ii=0; ii<max; ii++) result[ii]=ii+from_rowid;
//
//        test_result(kw_get_list(tr_list, "data", 0, KW_REQUIRED), result, max);
//
//        printf("from %lu to %lu:\n", (unsigned long)from_rowid, (unsigned long)to_rowid);
//        for(int ii=0; ii<max; ii++) printf("    %lu\n", (unsigned long)result[ii]);
//        printf("\n");

        result += test_json(NULL);  // NULL: we want to check only the logs
    }

    /*-------------------------------------*
     *  Search Absolute range, backward
     *-------------------------------------*/
//    json_int_t from_rowid = appends/2 + 1;
//    json_int_t to_rowid = appends/2 + MAX_RECS;

    /*-------------------------------------*
     *  Search Relative range, forward
     *-------------------------------------*/
//    json_int_t from_rowid = -10;

    /*-------------------------------------*
     *  Search Relative range, backward
     *-------------------------------------*/
//    json_int_t from_rowid = -10;

    /*-------------------------------------*
     *  Search Relative range, forward
     *-------------------------------------*/
//    json_int_t from_rowid = -20;
//    json_int_t to_rowid = -10;

    /*-------------------------------------*
     *  Search Relative range, backward
     *-------------------------------------*/
//    json_int_t from_rowid = -20;
//    json_int_t to_rowid = -10;

    /*-------------------------------------*
     *  Search BAD Relative range, forward
     *-------------------------------------*/
//    json_int_t from_rowid = -10;
//    json_int_t to_rowid = -20;

    /*-------------------------------------*
     *      Search by rowid
     *-------------------------------------*/
//    json_int_t key = appends/2 + 1;

    /*-------------------------------------*
     *      Open rt list
     *-------------------------------------*/
//    set_expected_results( // Check that no logs happen
//        "open rt list 2", // test name
//        NULL,   // error's list, It must not be any log error
//        NULL,   // expected, NULL: we want to check only the logs
//        NULL,   // ignore_keys
//        TRUE    // verbose
//    );
//
//    all_leidos = 0;
//    one_leidos = 0;
//
//    json_t *tr_list = tranger2_open_rt_list(
//        tranger,
//        TOPIC_NAME,
//        "",             // key
//        all_load_record_callback,
//        ""              // list id
//    );
//
//    tranger2_open_rt_list(
//        tranger,
//        TOPIC_NAME,
//        "0000000000000000001",       // key
//        one_load_record_callback,
//        "list2"              // list id
//    );
//
//    result += test_json(NULL);  // NULL: we want to check only the logs

    /*-------------------------------------*
     *      Add records
     *-------------------------------------*/
//    set_expected_results( // Check that no logs happen
//        "append records 2", // test name
//        NULL,   // error's list, It must not be any log error
//        NULL,   // expected, NULL: we want to check only the logs
//        NULL,   // ignore_keys
//        TRUE    // verbose
//    );
//
//    uint64_t t1 = 946684800; // 2000-01-01T00:00:00+0000
//    for(json_int_t i=0; i<MAX_KEYS; i++) {
//        uint64_t tm = t1;
//        for(json_int_t j=0; j<MAX_RECORDS; j++) {
//            json_t *jn_record1 = json_pack("{s:I, s:I, s:s}",
//               "id", i + 1,
//               "tm", tm+j,
//               "content",
//               "Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el."
//               "Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el."
//               "Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el.x"
//            );
//            md2_record_t md_record;
//            tranger2_append_record(tranger, TOPIC_NAME, tm+j, 0, &md_record, jn_record1);
//        }
//    }
//    result += test_json(NULL);  // NULL: we want to check only the logs

    /*-------------------------------------*
     *      Close rt lists
     *-------------------------------------*/
//    set_expected_results( // Check that no logs happen
//        "close rt lists 2", // test name
//        NULL,   // error's list, It must not be any log error
//        NULL,   // expected, NULL: we want to check only the logs
//        NULL,   // ignore_keys
//        TRUE    // verbose
//    );
//
//    tranger2_close_rt_list(
//        tranger,
//        tr_list
//    );
//
//    json_t *list2 =tranger2_get_rt_list_by_id(
//        tranger,
//        "list2"
//    );
//    tranger2_close_rt_list(
//        tranger,
//        list2
//    );
//
//    result += test_json(NULL);  // NULL: we want to check only the logs
//
//    if(all_leidos != MAX_KEYS*MAX_RECORDS) {
//        printf("%sRecords read not match%s, leidos %d, records %d\n", On_Red BWhite,Color_Off,
//           (int)all_leidos, MAX_KEYS*MAX_RECORDS
//        );
//        result += -1;
//    }
//
//    if(one_leidos != MAX_RECORDS) {
//        printf("%sRecords read not match%s, leidos %d, records %d\n", On_Red BWhite,Color_Off,
//            (int)one_leidos, MAX_RECORDS
//        );
//        result += -1;
//    }

    /*-------------------------------*
     *  tranger_backup_topic
     *-------------------------------*/
    // TODO old test in test_timeranger2.c

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
    result += test_json(NULL);  // NULL: we want to check only the logs

    result += global_result;

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
