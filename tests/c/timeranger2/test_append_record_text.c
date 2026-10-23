/****************************************************************************
 *          test_append_record_text.c
 *
 *  tranger2_append_record_text(): a record whose content is already json
 *  TEXT is written as it is, and is the same record a read gets back.
 *
 *      1. A text is appended: its key and its __tm__ are read from the
 *         text, a read gets the same json back, and a realtime list gets
 *         it parsed, with its __md_tranger__. Spaces and escapes stay.
 *      2. The json append and the text append write the same way: the
 *         rows of one key follow each other whatever the append.
 *      3. A topic keyed by an integer, with a tkey that is a date: the key
 *         and the __tm__ are those of the json append.
 *      4. What is refused, with an ERROR and nothing written: a text that
 *         is not one json object, a text not followed by a NUL, a text with
 *         __md_tranger__, a pkey with a NUL, a pkey that is a path, a text
 *         without its pkey, a topic keyed by rowid.
 *      5. After a reopen, the cache build takes every record the text
 *         append wrote: they are read back.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP             "test_append_record_text"
#define DATABASE        "tr_append_record_text"
#define TOPIC_NAME      "topic_text"
#define TOPIC_INT       "topic_text_int"
#define TOPIC_ROWID     "topic_text_rowid"
#define BASE_T          946684800   // 2000-01-01T00:00:00+0000

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE int listed = 0;
PRIVATE json_t *last_listed = NULL;

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE json_t *startup_master(const char *path_root)
{
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i, s:s, s:i, s:i, s:I}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y",
        "xpermission" , 02770,
        "rpermission", 0600,
        "yev_loop", (json_int_t)0
    );
    return tranger2_startup(0, jn_tranger, 0);
}

PRIVATE int create_topic(json_t *tranger, const char *topic_name, const char *pkey, const char *tkey, system_flag2_t sf)
{
    json_t *topic = tranger2_create_topic(
        tranger,
        topic_name,
        pkey,
        tkey,
        json_pack("{s:i, s:s, s:i, s:i}",
            "on_critical_error", 4,
            "filename_mask", "%Y-%m-%d",
            "xpermission" , 02700,
            "rpermission", 0600
        ),
        sf,
        json_object(),
        0
    );
    return topic? 0 : -1;
}

PRIVATE int on_record(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *record
)
{
    listed++;
    JSON_DECREF(last_listed)
    last_listed = record;
    return 0;
}

/*
 *  Append `text` (a C string: its NUL follows it) as text
 */
PRIVATE int append_text(json_t *tranger, const char *topic_name, const char *text, md2_record_ex_t *md)
{
    memset(md, 0, sizeof(*md));
    return tranger2_append_record_text(tranger, topic_name, BASE_T, 0, md, text, strlen(text));
}

/*
 *  Read back the record `md` names and compare it with `text` parsed
 */
PRIVATE int check_read_back(json_t *tranger, const char *topic_name, const char *key,
    md2_record_ex_t *md, const char *text, const char *what)
{
    json_t *topic = tranger2_topic(tranger, topic_name);
    json_t *jn_read = tranger2_read_record_content(tranger, topic, key, md);
    json_t *jn_expected = json_loads(text, JSON_ALLOW_NUL, 0);
    json_object_del(jn_read, "__md_tranger__");     // a read adds the metadata
    int ret = 0;
    if(!jn_read || !jn_expected || !json_equal(jn_read, jn_expected)) {
        printf("%sERROR%s --> %s: the record read back is not the text appended\n",
            On_Red BWhite, Color_Off, what);
        print_json("read", jn_read);
        ret = -1;
    }
    JSON_DECREF(jn_read)
    JSON_DECREF(jn_expected)
    return ret;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX];
    const char *home = getenv("HOME");
    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    /*-------------------------------------*
     *  Setup
     *-------------------------------------*/
    set_expected_results(
        "setup",
        json_pack("[{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = startup_master(path_root);
    if(!tranger ||
            create_topic(tranger, TOPIC_NAME, "id", "tm", sf_string_key) < 0 ||
            create_topic(tranger, TOPIC_INT, "n", "date", sf_int_key) < 0 ||
            create_topic(tranger, TOPIC_ROWID, "", "tm", sf_rowid_key) < 0) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    json_t *list = tranger2_open_list(
        tranger,
        TOPIC_NAME,
        json_pack("{s:I}", "load_record_callback", (json_int_t)(uintptr_t)on_record),
        json_object(),
        "rt-text",
        FALSE,
        ""
    );
    result += test_json(NULL);

    /*-------------------------------------*
     *  1. A text appended
     *-------------------------------------*/
    set_expected_results("1. a text appended, read back, listed", NULL, NULL, NULL, 1);
    const char *t1 = "{\"id\":\"A\",\"tm\":1000,\"v\":1.5,\"s\":\"caf\\u00e9 \\\"q\\\"\",\"o\":{\"x\":[1,2]}}";
    const char *t2 = "  { \"tm\" : 2000 ,\n \"id\" : \"A\" , \"nul\":\"a\\u0000b\" }  ";
    md2_record_ex_t md1, md2;
    if(append_text(tranger, TOPIC_NAME, t1, &md1) < 0 || append_text(tranger, TOPIC_NAME, t2, &md2) < 0) {
        printf("%sERROR%s --> a valid text was refused\n", On_Red BWhite, Color_Off);
        result += -1;
    } else {
        if(md1.__tm__ != 1000 || md2.__tm__ != 2000) {
            printf("%sERROR%s --> __tm__ not read from the text: %lu %lu\n",
                On_Red BWhite, Color_Off, (unsigned long)md1.__tm__, (unsigned long)md2.__tm__);
            result += -1;
        }
        if(md1.g_rowid != 1 || md2.g_rowid != 2) {
            printf("%sERROR%s --> g_rowid %lu %lu, expected 1 2\n",
                On_Red BWhite, Color_Off, (unsigned long)md1.g_rowid, (unsigned long)md2.g_rowid);
            result += -1;
        }
        if(md2.__size__ != strlen(t2) + 1) {
            printf("%sERROR%s --> the text was not written as it is: size %lu, expected %lu\n",
                On_Red BWhite, Color_Off, (unsigned long)md2.__size__, (unsigned long)strlen(t2) + 1);
            result += -1;
        }
        result += check_read_back(tranger, TOPIC_NAME, "A", &md1, t1, "t1");
        result += check_read_back(tranger, TOPIC_NAME, "A", &md2, t2, "t2");

        json_t *jn_expected = json_loads(t2, JSON_ALLOW_NUL, 0);
        json_t *jn_md = json_object_get(last_listed, "__md_tranger__");
        json_t *jn_listed = json_deep_copy(last_listed);
        json_object_del(jn_listed, "__md_tranger__");
        if(listed != 2 || !jn_md ||
                json_integer_value(json_object_get(jn_md, "g_rowid")) != 2 ||
                !json_equal(jn_listed, jn_expected)) {
            printf("%sERROR%s --> the realtime list did not get the record: %d listed\n",
                On_Red BWhite, Color_Off, listed);
            print_json("last listed", last_listed);
            result += -1;
        }
        JSON_DECREF(jn_listed)
        JSON_DECREF(jn_expected)
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  2. The json append and the text
     *     append on the same key
     *-------------------------------------*/
    set_expected_results("2. json and text appends follow each other", NULL, NULL, NULL, 1);
    {
        md2_record_ex_t md3 = {0}, md4;
        tranger2_append_record(tranger, TOPIC_NAME, BASE_T, 0, &md3,
            json_pack("{s:s, s:I, s:s}", "id", "A", "tm", (json_int_t)3000, "via", "json")
        );
        const char *t4 = "{\"id\":\"A\",\"tm\":4000,\"via\":\"text\"}";
        append_text(tranger, TOPIC_NAME, t4, &md4);
        if(md3.g_rowid != 3 || md4.g_rowid != 4 || md4.__offset__ != md3.__offset__ + md3.__size__) {
            printf("%sERROR%s --> rows of the key do not follow: g_rowid %lu %lu, offsets %lu+%lu %lu\n",
                On_Red BWhite, Color_Off,
                (unsigned long)md3.g_rowid, (unsigned long)md4.g_rowid,
                (unsigned long)md3.__offset__, (unsigned long)md3.__size__, (unsigned long)md4.__offset__);
            result += -1;
        }
        result += check_read_back(tranger, TOPIC_NAME, "A", &md4, t4, "t4");
        if(listed != 4) {
            printf("%sERROR%s --> the list got %d records, expected 4\n", On_Red BWhite, Color_Off, listed);
            result += -1;
        }
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  3. An integer key, a date tkey
     *-------------------------------------*/
    set_expected_results("3. integer key and date tkey, as the json append", NULL, NULL, NULL, 1);
    {
        const char *t5 = "{\"n\":42,\"date\":\"2026-10-23T10:00:00+0000\",\"v\":true}";
        md2_record_ex_t md5;
        if(append_text(tranger, TOPIC_INT, t5, &md5) < 0) {
            printf("%sERROR%s --> an integer-keyed text was refused\n", On_Red BWhite, Color_Off);
            result += -1;
        }
        json_t *jn5 = json_loads(t5, 0, 0);
        BOOL fits;
        uint64_t tm = tranger2_record_tkey_time(tranger2_topic(tranger, TOPIC_INT), jn5, &fits);
        if(md5.__tm__ != tm || tm == 0) {
            printf("%sERROR%s --> __tm__ %lu, the json append's %lu\n",
                On_Red BWhite, Color_Off, (unsigned long)md5.__tm__, (unsigned long)tm);
            result += -1;
        }
        JSON_DECREF(jn5)
        result += check_read_back(tranger, TOPIC_INT, "0000000000000000042", &md5, t5, "t5");
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  4. Refused, nothing written
     *-------------------------------------*/
    set_expected_results(
        "4. refused texts",
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Cannot append record, its content is not one json object",
            "msg", "Cannot append record, its content is not one json object",
            "msg", "Cannot append record, content NULL or not followed by a NUL",
            "msg", "Cannot append record as text, it carries a __md_tranger__ (the metadata of another append)",
            "msg", "Cannot append record, its pkey has a NUL in it",
            "msg", "Cannot append record, invalid pkey (path traversal)",
            "msg", "Cannot append record, no pkey",
            "msg", "Cannot append record as text: only a topic keyed by a string or an integer takes one"
        ),
        NULL, NULL, 1
    );
    {
        md2_record_ex_t md;
        const char *refused[] = {
            "{\"id\":\"A\",",
            "[{\"id\":\"A\"}]",
            NULL,   // not followed by a NUL
            "{\"id\":\"A\",\"__md_tranger__\":{}}",
            "{\"id\":\"a\\u0000b\"}",
            "{\"id\":\"../x\"}",
            "{\"tm\":1}",
        };
        int n = (int)(sizeof(refused)/sizeof(refused[0]));
        for(int i = 0; i < n; i++) {
            int ret;
            if(refused[i]) {
                ret = append_text(tranger, TOPIC_NAME, refused[i], &md);
            } else {
                const char bf[] = "{\"id\":\"A\"}X";
                memset(&md, 0, sizeof(md));
                ret = tranger2_append_record_text(tranger, TOPIC_NAME, BASE_T, 0, &md, bf, sizeof(bf) - 2);
            }
            if(ret >= 0) {
                printf("%sERROR%s --> refused text %d was appended\n", On_Red BWhite, Color_Off, i);
                result += -1;
            }
        }
        if(append_text(tranger, TOPIC_ROWID, "{\"tm\":1}", &md) >= 0) {
            printf("%sERROR%s --> a text was appended to a rowid topic\n", On_Red BWhite, Color_Off);
            result += -1;
        }
        if(listed != 4) {
            printf("%sERROR%s --> a refused text reached the list: %d\n", On_Red BWhite, Color_Off, listed);
            result += -1;
        }
    }
    result += test_json(NULL);

    tranger2_close_list(tranger, list);
    JSON_DECREF(last_listed)

    /*-------------------------------------*
     *  5. Reopen: the cache build takes
     *     the records of the text append
     *-------------------------------------*/
    set_expected_results("5. reopen, every record read back", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    tranger = startup_master(path_root);
    tranger2_open_topic(tranger, TOPIC_NAME, TRUE);
    listed = 0;
    list = tranger2_open_list(
        tranger,
        TOPIC_NAME,
        json_pack("{s:s, s:I, s:I}",
            "key", "A",
            "to_rowid", (json_int_t)1000000,   // no realtime
            "load_record_callback", (json_int_t)(uintptr_t)on_record
        ),
        json_object(),
        "",
        FALSE,
        ""
    );
    if(listed != 4) {
        printf("%sERROR%s --> after a reopen %d records of key A, expected 4\n",
            On_Red BWhite, Color_Off, listed);
        result += -1;
    }
    tranger2_close_list(tranger, list);
    JSON_DECREF(last_listed)
    result += test_json(NULL);

    /*-------------------------------------*
     *  Shutdown
     *-------------------------------------*/
    set_expected_results("shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *              Main
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

PRIVATE void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    memset(&sigIntHandler, 0, sizeof(sigIntHandler));
    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigaction(SIGALRM, &sigIntHandler, NULL);
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);
}

int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;
    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);
    yuno_catch_signals();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("testing", 0, capture_log_write, 0);
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    yev_loop_create(0, 2024, 10, NULL, &yev_loop);

    int result = do_test();

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
    } else {
        printf("<-- %sTEST OK%s: %s\n", On_Green BWhite, Color_Off, APP);
    }
    return result < 0? -1 : 0;
}
