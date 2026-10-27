/***********************************************************************
 *          C_TEST_AGENT_FILE_CONTENT.C
 *
 *          GClass to test how the agent reads a file whole for its
 *          read-file and read-json commands (read_file_content(), the
 *          agent's own file_content.c)
 *
 *          Up to 7.28.9 the agent read at most the largest block a yuno
 *          may allocate and cut the rest without saying so, and a read()
 *          that failed was taken for an empty file. Verified here, against
 *          a maximum block of 1 MiB (main.c):
 *
 *              - a small file, and one of exactly the most that fits, come
 *                back whole;
 *              - one a byte longer is refused, with the reason;
 *              - an empty file is an empty string;
 *              - a file whose size the kernel does not tell (/proc, size 0)
 *                is read whole all the same;
 *              - a directory (read() fails) and a file that does not exist
 *                are refused, with the reason.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>

#include "file_content.h"
#include "c_test_agent_file_content.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "level of help"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-------json_fn---------description--*/
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,    cmd_help,       "Command's help"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES  = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name--------------------flag----alias---items---description--*/
SDATA_END()
};

/*---------------------------------------------*
 *      Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    char path_dir[PATH_MAX];
} PRIVATE_DATA;

/***************************************************************************
 *              Framework Method Create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  A directory of its own, wiped on every run
     */
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    build_path(priv->path_dir, sizeof(priv->path_dir), path_root, "c_agent_file_content", NULL);
    rmrdir(priv->path_dir);
    mkrdir(priv->path_dir, 02770);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

/***************************************************************************
 *              Framework Method Start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);

    return 0;
}

/***************************************************************************
 *              Framework Method Stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);

    return 0;
}

/***************************************************************************
 *              Framework Method Play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Fire a one-shot timer to run tests inside the event loop
     */
    set_timeout(priv->timer, 100);

    return 0;
}

/***************************************************************************
 *  A file of `size` bytes ('x'), in the test directory. Returns 0 or -1
 *  (logged).
 ***************************************************************************/
PRIVATE int write_test_file(hgobj gobj, const char *path, size_t size)
{
    FILE *f = fopen(path, "w");
    if(!f) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST FAIL: cannot write a test file",
            "path",         "%s", path,
            NULL
        );
        return -1;
    }
    for(size_t i=0; i<size; i++) {
        fputc('x', f);
    }
    fclose(f);
    return 0;
}

/***************************************************************************
 *  Read `path`: it must come back whole with `expected_len` bytes (and NUL
 *  after them), or be refused with a reason when `expected_len` is -1.
 *  Returns 0 or -1 (logged).
 ***************************************************************************/
PRIVATE int check_read(hgobj gobj, const char *what, const char *path, long expected_len)
{
    char why[256];
    size_t len = 12345;
    char *s = read_file_content(gobj, path, &len, why, sizeof(why));

    int ret = 0;
    if(expected_len < 0) {
        if(s || empty_string(why)) {
            ret = -1;
        }
    } else {
        if(!s || len != (size_t)expected_len || s[len] != 0 || strlen(s) != len) {
            ret = -1;
        }
    }
    if(ret < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a file read",
            "what",         "%s", what,
            "path",         "%s", path,
            "expected_len", "%ld", expected_len,
            "content",      "%s", s? "returned" : "NULL",
            "len",          "%lu", (unsigned long)len,
            "why",          "%s", why,
            NULL
        );
    }
    GBMEM_FREE(s);
    return ret;
}

/***************************************************************************
 *  The reads
 ***************************************************************************/
PRIVATE int run_tests(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    size_t max = gbmem_get_maximum_block();    // MEM_MAX_BLOCK less the tracking header
    if(max == 0 || max > 1024*1024) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: the maximum block is not the one of main.c",
            "max",          "%lu", (unsigned long)max,
            NULL
        );
        return -1;
    }

    char path[PATH_MAX];

    build_path(path, sizeof(path), priv->path_dir, "small", NULL);
    result += write_test_file(gobj, path, 10);
    result += check_read(gobj, "a small file", path, 10);

    build_path(path, sizeof(path), priv->path_dir, "fits", NULL);
    result += write_test_file(gobj, path, max - 1);
    result += check_read(gobj, "the most that fits (its NUL is the last byte)", path, (long)(max - 1));

    build_path(path, sizeof(path), priv->path_dir, "too_big", NULL);
    result += write_test_file(gobj, path, max);
    result += check_read(gobj, "a byte more than fits: refused, not cut", path, -1);

    build_path(path, sizeof(path), priv->path_dir, "empty", NULL);
    result += write_test_file(gobj, path, 0);
    result += check_read(gobj, "an empty file", path, 0);

    /*
     *  A file whose size the kernel says is 0: read whole by growing
     */
    char why[256];
    size_t len = 0;
    char *s = read_file_content(gobj, "/proc/self/status", &len, why, sizeof(why));
    if(!s || len == 0 || !strstr(s, "VmRSS") || strlen(s) != len) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a file of unknown size",
            "len",          "%lu", (unsigned long)len,
            "why",          "%s", why,
            NULL
        );
        result += -1;
    }
    GBMEM_FREE(s);

    result += check_read(gobj, "a directory: read() fails", priv->path_dir, -1);

    build_path(path, sizeof(path), priv->path_dir, "does_not_exist", NULL);
    result += check_read(gobj, "a file that does not exist", path, -1);

    if(result == 0) {
        gobj_log_info(gobj, 0,
            "msgset", "%s", MSGSET_INFO,
            "msg", "%s", "All file_content tests PASSED",
            NULL
        );
    }
    return result;
}

/***************************************************************************
 *  EV_TIMEOUT -- runs the tests inside the event loop, then exits
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    run_tests(gobj);

    gobj_log_info(gobj, 0,
        "msgset", "%s", MSGSET_INFO,
        "msg", "%s", "Exit to die",
        NULL
    );
    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_STOPPED
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *              Command: help
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw)
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *              GClass
 ***************************************************************************/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
};

GOBJ_DEFINE_GCLASS(C_TEST_AGENT_FILE_CONTENT);

PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    ev_action_t st_idle[] = {
        {EV_TIMEOUT,                ac_timeout,         0},
        {EV_STOPPED,                ac_stopped,         0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_STOPPED,                0},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  //lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
        command_table,
        s_user_trace_level,
        0   // gcflag_t
    );
    return __gclass__ ? 0 : -1;
}

/***************************************************************************
 *              Registration
 ***************************************************************************/
PUBLIC int register_c_test_agent_file_content(void)
{
    return create_gclass(C_TEST_AGENT_FILE_CONTENT);
}
