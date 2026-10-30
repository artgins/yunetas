/****************************************************************************
 *          test_msg2db_topics_left_out.c
 *
 *  What msg2db_open_db() leaves out of a msg2db gets nothing of it.
 *
 *  The first loop over the schema topics leaves out a topic whose name
 *  breaks the name rule, one without pkey=id and one without pkey2, saying
 *  why once. The second loop, the one that opens the lists and builds the
 *  index, asked whether the topic was in the tranger -- not whether the
 *  first loop had taken it -- so a topic left out but opened in the tranger
 *  before (by somebody else) got a list and an index all the same, without
 *  a word.
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

#define APP             "test_msg2db_topics_left_out"
#define DATABASE        "tr_msg2db_left_out"
#define MSG2DB_NAME     "msg2db_left_out"

PRIVATE yev_loop_h yev_loop;
PRIVATE int global_result = 0;

/*
 *  `good` is taken; `a/b` breaks the name rule; `pre` has no pkey2, and it
 *  is opened in the tranger before the msg2db.
 */
PRIVATE char msg2db_schema[]= "\
{                                                                   \n\
    'id': '"MSG2DB_NAME"',                                          \n\
    'schema_version': '1',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'good',                                           \n\
            'pkey': 'id',                                           \n\
            'tkey': 'tm',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'pkey2': 'alarm',                                       \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id':    {'header': 'Id', 'type': 'string', 'flag': ['persistent','required']}, \n\
                'tm':    {'header': 'Time', 'type': 'integer', 'flag': ['persistent','required','time']}, \n\
                'alarm': {'header': 'Alarm', 'type': 'string', 'flag': ['persistent','required']} \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'id': 'a/b',                                            \n\
            'pkey': 'id',                                           \n\
            'pkey2': 'alarm',                                       \n\
            'cols': {}                                              \n\
        },                                                          \n\
        {                                                           \n\
            'id': 'pre',                                            \n\
            'pkey': 'id',                                           \n\
            'tkey': 'tm',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id':    {'header': 'Id', 'type': 'string', 'flag': ['persistent','required']}, \n\
                'tm':    {'header': 'Time', 'type': 'integer', 'flag': ['persistent','required','time']} \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************************
 *  The master's inotify watcher is cancelled asynchronously: give the loop
 *  the turns that free it (same as test_pkey2_empty)
 ***************************************************************************/
PRIVATE void drain_loop(void)
{
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
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

    /*
     *  Each topic left out is said once, by the first loop, and nothing
     *  more: the second loop said each one three times more up to 7.26.8,
     *  and opened `pre` without a word up to 7.26.9.
     */
    set_expected_results("left_out", json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic",            // pre, opened before the msg2db
            "msg", "Creating topic",            // good
            "msg", "Topic of the msg2db left out: its name breaks the name rule (empty, '.', '..', '/' or '`')",
            "msg", "Schema topic without pkey2"
        ),
        NULL, NULL, TRUE
    );

    json_t *tranger = tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    ), yev_loop);

    json_t *pre = tranger2_create_topic(
        tranger,
        "pre",
        "id",
        "tm",
        NULL,   // jn_topic_ext
        sf_string_key,
        json_pack("{s:{s:s, s:s, s:[s,s]}}",
            "id", "header", "Id", "type", "string", "flag", "persistent", "required"
        ),
        0       // jn_topic_var
    );
    if(!pre) {
        printf("%sERROR --> %s the topic 'pre' cannot be created\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    helper_quote2doublequote(msg2db_schema);
    json_t *jn_schema = legalstring2json(msg2db_schema, TRUE);
    json_t *msg2db = msg2db_open_db(tranger, MSG2DB_NAME, jn_schema, "");

    if(!json_object_get(msg2db, "good")) {
        printf("%sERROR --> %s the topic taken has no index\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    const char *left_out[] = {"pre", "a/b", NULL};
    for(int i = 0; left_out[i]; i++) {
        if(json_object_get(msg2db, left_out[i])) {
            printf("%sERROR --> %s the topic left out '%s' has an index\n",
                On_Red BWhite, Color_Off, left_out[i]
            );
            result += -1;
        }
    }

    /*
     *  And no list: msg2db_close_db() closes what the second loop opened,
     *  a list left behind would be a leak at the end
     */
    msg2db_close_db(tranger, MSG2DB_NAME);
    tranger2_shutdown(tranger);
    drain_loop();

    result += test_json(NULL);

    rmrdir(path_database);
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
