/****************************************************************************
 *          test_msg2db_column_default.c
 *
 *  The default of a column belongs to the schema, and a message must not
 *  spend it.
 *
 *  _set_volatil_values() read every field of a message with
 *  kw_get_dict_value(gobj, kw, field, default, 0), handing it the col's
 *  `default` -- a pointer borrowed from the topic's cols, which
 *  tranger2_dict_topic_desc_cols() answers increfed, not copied. That reader
 *  OWNS its default and decrefs it when it finds the field. A boolean default
 *  is a jansson singleton and never noticed; a string or an integer was freed
 *  after the first message that carried its field, and the next one logged
 *  "BAD json_decref()" (the alarms msg2db of yunovatios, schema 2: the yuno
 *  corrupted its heap and was relaunched).
 *
 *  What this test pins, through an append run and a load run:
 *
 *    - many messages that carry every field log nothing, and the schema's
 *      defaults are still there and still what they were;
 *    - a message that lacks a VOLATILE field gets its default; one that
 *      carries it keeps its own value;
 *    - the load (which sets the volatile values of every record it reads)
 *      does the same, with nothing in the log.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <signal.h>
#include <yunetas.h>

#define APP             "test_msg2db_column_default"
#define DATABASE        "tr_msg2db_default_test"
#define MSG2DB_NAME     "msg2db_test"
#define TOPIC_NAME      "alarms"

#define APPENDS         20  /* the first one spent the default, the second
                               one decreffed freed memory */

PRIVATE yev_loop_h yev_loop;
PRIVATE int global_result = 0;

/*
 *  A string, an integer and a boolean default, on persistent and on volatile
 *  columns (no flag: not written, set from the message or its default).
 */
PRIVATE char msg2db_schema[]= "\
{                                                                   \n\
    'id': '"MSG2DB_NAME"',                                          \n\
    'schema_version': '1',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': '"TOPIC_NAME"',                                   \n\
            'pkey': 'id',                                           \n\
            'tkey': 'tm',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'pkey2': 'alarm',                                       \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Device Id',                          \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'tm': {                                             \n\
                    'header': 'Time',                               \n\
                    'type': 'integer',                              \n\
                    'flag': ['persistent','required','time']        \n\
                },                                                  \n\
                'alarm': {                                          \n\
                    'header': 'Alarm',                              \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'severity': {                                       \n\
                    'header': 'Severity',                           \n\
                    'type': 'string',                               \n\
                    'default': 'medium',                            \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'count': {                                          \n\
                    'header': 'Count',                              \n\
                    'type': 'integer',                              \n\
                    'default': 0,                                   \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'active': {                                         \n\
                    'header': 'Active',                             \n\
                    'type': 'boolean',                              \n\
                    'default': false,                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                '_note': {                                          \n\
                    'header': 'Note',                               \n\
                    'type': 'string',                               \n\
                    'default': 'none',                              \n\
                    'flag': []                                      \n\
                },                                                  \n\
                '_hits': {                                          \n\
                    'header': 'Hits',                               \n\
                    'type': 'integer',                              \n\
                    'default': 7,                                   \n\
                    'flag': []                                      \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************************
 *  A master tranger watches its /disks directory with inotify, and
 *  tranger2_shutdown() cancels that watcher asynchronously: the io_uring
 *  CQEs that free each fs_event arrive on later turns of the loop. Without
 *  those turns their memory is still allocated at the leak check (see
 *  README.md).
 ***************************************************************************/
PRIVATE void drain_loop(void)
{
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
}

/***************************************************************************
 *  `what` failed: say it and count it.
 ***************************************************************************/
PRIVATE int fail(const char *what)
{
    printf("%sERROR --> %s%s\n", On_Red BWhite, Color_Off, what);
    return -1;
}

/***************************************************************************
 *  The schema's defaults are still there, and still what they were.
 ***************************************************************************/
PRIVATE int check_defaults(json_t *tranger)
{
    int result = 0;
    json_t *cols = tranger2_dict_topic_desc_cols(tranger, TOPIC_NAME);

    json_t *severity = json_object_get(json_object_get(cols, "severity"), "default");
    if(!json_is_string(severity) || strcmp(json_string_value(severity), "medium")!=0) {
        result += fail("the default of 'severity' is not 'medium' any more");
    }
    json_t *count = json_object_get(json_object_get(cols, "count"), "default");
    if(!json_is_integer(count) || json_integer_value(count) != 0) {
        result += fail("the default of 'count' is not 0 any more");
    }
    json_t *note = json_object_get(json_object_get(cols, "_note"), "default");
    if(!json_is_string(note) || strcmp(json_string_value(note), "none")!=0) {
        result += fail("the default of '_note' is not 'none' any more");
    }
    json_t *hits = json_object_get(json_object_get(cols, "_hits"), "default");
    if(!json_is_integer(hits) || json_integer_value(hits) != 7) {
        result += fail("the default of '_hits' is not 7 any more");
    }

    JSON_DECREF(cols)
    return result;
}

/***************************************************************************
 *  The volatile fields of the message of (id, alarm): `note` and `hits`.
 ***************************************************************************/
PRIVATE int check_volatile(
    json_t *tranger,
    const char *id,
    const char *alarm,
    const char *note,
    json_int_t hits
)
{
    int result = 0;
    json_t *msg = msg2db_get_message( // NOT yours
        tranger, MSG2DB_NAME, TOPIC_NAME, id, alarm
    );
    if(!msg) {
        return fail("a message is not there");
    }
    const char *note_ = json_string_value(json_object_get(msg, "_note"));
    if(!note_ || strcmp(note_, note)!=0) {
        printf("    %s/%s: _note '%s', expected '%s'\n", id, alarm, note_? note_: "(none)", note);
        result += fail("a volatile string field is wrong");
    }
    json_t *hits_ = json_object_get(msg, "_hits");
    if(!json_is_integer(hits_) || json_integer_value(hits_) != hits) {
        printf("    %s/%s: _hits %lld, expected %lld\n", id, alarm,
            (long long)json_integer_value(hits_), (long long)hits);
        result += fail("a volatile integer field is wrong");
    }
    return result;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];

    if(!(getenv("YUNETA_STORE") && strlen(getenv("YUNETA_STORE")) > 0)) {
        snprintf(path_root, sizeof(path_root), "/tmp");
    } else {
        snprintf(path_root, sizeof(path_root), "%s", getenv("YUNETA_STORE"));
    }
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    #define TRANGER_CONFIG() json_pack("{s:s, s:s, s:b, s:i}", \
        "path", path_root, \
        "database", DATABASE, \
        "master", 1, \
        "on_critical_error", 0 \
    )

    /*-------------------------------------------------------*
     *  First run: many messages carrying every field, and
     *  one carrying none of the volatile ones.
     *-------------------------------------------------------*/
    set_expected_results("append",
        json_pack("[{s:s}, {s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, TRUE
    );

    json_t *tranger = tranger2_startup(0, TRANGER_CONFIG(), yev_loop);
    helper_quote2doublequote(msg2db_schema);
    json_t *jn_schema = legalstring2json(msg2db_schema, TRUE);
    msg2db_open_db(tranger, MSG2DB_NAME, jn_schema, "");

    for(int i = 0; i < APPENDS; i++) {
        json_t *msg = json_pack("{s:s, s:I, s:s, s:s, s:I, s:b, s:s, s:I}",
            "id", "device-1",
            "tm", (json_int_t)(1700000000 + i),
            "alarm", "HIGH",
            "severity", "high",
            "count", (json_int_t)i,
            "active", 1,
            "_note", "seen",
            "_hits", (json_int_t)i
        );
        msg2db_append_message(tranger, MSG2DB_NAME, TOPIC_NAME, msg, "");
    }
    json_t *bare = json_pack("{s:s, s:I, s:s}",
        "id", "device-2",
        "tm", (json_int_t)1700001000,
        "alarm", "LOW"
    );
    msg2db_append_message(tranger, MSG2DB_NAME, TOPIC_NAME, bare, "");

    result += check_defaults(tranger);
    result += check_volatile(tranger, "device-1", "HIGH", "seen", APPENDS - 1);
    result += check_volatile(tranger, "device-2", "LOW", "none", 7);

    msg2db_close_db(tranger, MSG2DB_NAME);
    tranger2_shutdown(tranger);
    drain_loop();

    result += test_json(NULL);

    /*-------------------------------------------------------*
     *  Second run: the load sets the volatile values of each
     *  record it reads, from the defaults (none was written).
     *-------------------------------------------------------*/
    set_expected_results("load", json_array(), NULL, NULL, TRUE);

    tranger = tranger2_startup(0, TRANGER_CONFIG(), yev_loop);
    helper_quote2doublequote(msg2db_schema);
    json_t *jn_schema2 = legalstring2json(msg2db_schema, TRUE);
    msg2db_open_db(tranger, MSG2DB_NAME, jn_schema2, "");

    result += check_defaults(tranger);
    result += check_volatile(tranger, "device-1", "HIGH", "none", 7);
    result += check_volatile(tranger, "device-2", "LOW", "none", 7);

    msg2db_close_db(tranger, MSG2DB_NAME);
    tranger2_shutdown(tranger);
    drain_loop();

    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    yev_loop_reset_running(yev_loop);
}

PRIVATE void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;
    signal(SIGPIPE, SIG_IGN);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigaction(SIGALRM, &sigIntHandler, NULL);
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);
}

/***************************************************************************
 *
 ***************************************************************************/
int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    sys_malloc_fn_t malloc_func;
    sys_free_fn_t free_func;

    gbmem_get_json_allocators(&malloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    unsigned long memory_check_list[] = {0}; // WARNING: list ended with 0
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gbmem_setup(
        256*1024L,
        1024*1024*1024L,
        FALSE,
        0,
        0
    );
    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    yuno_catch_signals();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler(
        "testing",
        0,
        capture_log_write,
        0
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    yev_loop_create(0, 2024, 10, NULL, &yev_loop);

    int result = do_test();
    result += global_result;

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }
    if(result < 0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    }
    return result < 0 ? -1 : 0;
}
