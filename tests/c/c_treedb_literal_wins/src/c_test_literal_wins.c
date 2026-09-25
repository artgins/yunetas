/***********************************************************************
 *          C_TEST_LITERAL_WINS.C
 *
 *          GClass to test what an open of a dynamic-schema treedb does
 *          when its schema from C is newer than the schema file in use
 *
 *          The rule (user decision, 2026-09-23): with impose_c_schema
 *          off, a literal whose schema_version is HIGHER than the schema
 *          file in use wins WHOLE. It replaces the whole file, __system__
 *          is projected from it whole, and a topic it does not declare
 *          disappears from both. Whatever operator work that discards --
 *          an unsaved draft, a saved draft, an applied schema that never
 *          ran -- is said: one WARNING naming the treedb and the topics,
 *          and `withdrawn_at_open` in the API. A literal that is NOT
 *          higher is not installed: the file runs, and __system__ keeps
 *          what it holds.
 *
 *          Each scenario opens its own treedb and ends checking that the
 *          three homes of the schema agree: what RUNS (the topics the
 *          store opened, their topic_version and columns), the schema
 *          FILE in use, and __system__ (saved-schema answers no draft
 *          that differs from the file).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <liburing.h>

#include "c_test_literal_wins.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define SYSTEM_TREEDB   "treedb_system_schema"

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
    hgobj gobj_treedbs;
    hgobj timer;
    char path_database[PATH_MAX];

    /*
     *  The writes of __system__ this test watches (ac_system_write)
     */
    int system_writes;          // writes seen since watching
    int kill_at_write;          // SIGKILL the process at this write (0: never)
    char break_writes_of[NAME_MAX]; // the node of `topics` whose next writes fail, once created
    int broken_fds;             // descriptors of that node made read-only

    /*
     *  The scenarios run one per timeout (see ac_timeout)
     */
    int step;                   // 0: run_tests(), then late_scenarios[step - 1]
    BOOL repeat_step;           // the late scenario of this step runs again at the next
    int result;

    /*
     *  DC: where the sweep of the double crash is (scenario_double_crash)
     */
    int dc_phase;               // 0: with drafts, 1: without, 2: done
    int dc_k1;                  // the write the first process dies at, 0: not started
    int dc_writes;              // the writes of the whole projection
    int dc_combos;
    int dc_completed;           // combos where the second process completed the projection
} PRIVATE_DATA;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    build_path(
        priv->path_database,
        sizeof(priv->path_database),
        path_root,
        "c_treedb_literal_wins",
        NULL
    );
    rmrdir(priv->path_database);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  impose_c_schema off: the treedbs of this test change their schema
     *  dynamically, which is the case the rule is about
     */
    json_t *kw_treedbs = json_pack("{s:s, s:s, s:b, s:i, s:i, s:i, s:b}",
        "path", priv->path_database,
        "filename_mask", "%Y",
        "master", 1,
        "xpermission", 02770,
        "rpermission", 0660,
        "exit_on_error", LOG_OPT_TRACE_STACK,
        "impose_c_schema", 0
    );
    priv->gobj_treedbs = gobj_create_service(
        "treedbs",
        C_TREEDB,
        kw_treedbs,
        gobj
    );
    gobj_start_tree(priv->gobj_treedbs);

    set_timeout(priv->timer, 100);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop_tree(priv->gobj_treedbs);

    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Schema builders: the literal of each scenario, written in code
 ***************************************************************************/
PRIVATE json_t *col_id(void)
{
    return json_pack("{s:s, s:i, s:s, s:[s,s]}",
        "header", "Id", "fillspace", 10, "type", "string", "flag", "persistent", "required");
}

PRIVATE json_t *col_str(const char *header)
{
    return json_pack("{s:s, s:i, s:s, s:[s]}",
        "header", header, "fillspace", 10, "type", "string", "flag", "persistent");
}

PRIVATE json_t *col_fkey(const char *header)
{
    return json_pack("{s:s, s:i, s:s, s:[s]}",
        "header", header, "fillspace", 10, "type", "array", "flag", "fkey");
}

PRIVATE json_t *col_hook(const char *child_topic, const char *child_field)
{
    return json_pack("{s:s, s:i, s:s, s:[s], s:{s:s}}",
        "header", "Hook", "fillspace", 10, "type", "array", "flag", "hook",
        "hook", child_topic, child_field);
}

PRIVATE json_t *topic_of(const char *name, int topic_version, json_t *cols) // cols owned
{
    return json_pack("{s:s, s:s, s:s, s:i, s:o}",
        "id", name, "pkey", "id", "system_flag", "sf_string_key",
        "topic_version", topic_version, "cols", cols);
}

PRIVATE json_t *schema_of(const char *treedb_name, int schema_version, json_t *topics) // owned
{
    return json_pack("{s:s, s:i, s:o}",
        "id", treedb_name, "schema_version", schema_version, "topics", topics);
}

/***************************************************************************
 *  A TEST FAIL, with what was seen
 ***************************************************************************/
PRIVATE int test_fail(hgobj gobj, const char *treedb_name, const char *msg, json_t *seen) // seen owned
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", msg,
        "treedb_name",  "%s", treedb_name,
        "seen",         "%j", seen? seen : json_null(),
        NULL
    );
    JSON_DECREF(seen)
    return -1;
}

/***************************************************************************
 *  Open a treedb with a literal. `forced` imposes it from the code
 ***************************************************************************/
PRIVATE int open_db(hgobj gobj, const char *treedb_name, json_t *jn_schema, BOOL forced) // owned
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = json_pack("{s:s, s:i, s:s, s:o}",
        "filename_mask", "%Y",
        "exit_on_error", 0,
        "treedb_name", treedb_name,
        "treedb_schema", jn_schema
    );
    if(forced) {
        json_object_set_new(kw, "impose_c_schema", json_true());
    }
    json_t *jn_resp = gobj_command(priv->gobj_treedbs, "open-treedb", kw, gobj);
    int result = (int)kw_get_int(gobj, jn_resp, "result", -1, 0);
    if(result < 0) {
        test_fail(gobj, treedb_name, "TEST FAIL: open-treedb failed", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    return result < 0? -1 : 0;
}

/***************************************************************************
 *  close-treedb. HACK `force`: the yuno plays (the test runs from a timer)
 *  and this driver holds nothing of the treedb
 ***************************************************************************/
PRIVATE void close_db(hgobj gobj, const char *treedb_name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_resp = gobj_command(priv->gobj_treedbs, "close-treedb",
        json_pack("{s:s, s:b}", "treedb_name", treedb_name, "force", 1), gobj);
    JSON_DECREF(jn_resp)
}

/***************************************************************************
 *  A command of C_TREEDB about one treedb. Return is YOURS
 ***************************************************************************/
PRIVATE json_t *treedb_cmd(hgobj gobj, const char *treedb_name, const char *command, json_t *kw) // owned
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_object_set_new(kw, "treedb_name", json_string(treedb_name));
    return gobj_command(priv->gobj_treedbs, command, kw, gobj);
}

/***************************************************************************
 *  The operator edits the header of a column in __system__ (a draft)
 ***************************************************************************/
PRIVATE int edit_header(hgobj gobj, const char *treedb_name, const char *topic_name,
    const char *col_name, const char *header)
{
    char col_id[NAME_MAX];
    snprintf(col_id, sizeof(col_id), "%s.%s.%s", treedb_name, topic_name, col_name);
    json_t *edited = gobj_update_node(
        gobj_find_service(SYSTEM_TREEDB, FALSE),
        "cols",
        json_pack("{s:s, s:s}", "id", col_id, "header", header),
        json_pack("{s:b}", "refs", 1),
        gobj
    );
    int ret = edited? 0 : test_fail(gobj, treedb_name, "TEST FAIL: the operator edit was refused", NULL);
    JSON_DECREF(edited)
    return ret;
}

PRIVATE int save_schema(hgobj gobj, const char *treedb_name)
{
    json_t *jn_resp = treedb_cmd(gobj, treedb_name, "save-schema", json_object());
    int ret = 0;
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
        ret = test_fail(gobj, treedb_name, "TEST FAIL: save-schema failed", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    return ret;
}

PRIVATE int apply_schema(hgobj gobj, const char *treedb_name)
{
    json_t *jn_resp = treedb_cmd(gobj, treedb_name, "apply-schema", json_object());
    int ret = 0;
    if(!kw_get_bool(gobj, jn_resp, "data`applied", 0, 0)) {
        ret = test_fail(gobj, treedb_name, "TEST FAIL: apply-schema did not apply", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    return ret;
}

/***************************************************************************
 *  open-treedb, the answer as it is. Return is YOURS
 ***************************************************************************/
PRIVATE json_t *open_db_resp_as(hgobj gobj, const char *treedb_name, json_t *jn_schema, // owned
    BOOL forced)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = json_pack("{s:s, s:i, s:s, s:o}",
        "filename_mask", "%Y",
        "exit_on_error", 0,
        "treedb_name", treedb_name,
        "treedb_schema", jn_schema
    );
    if(forced) {
        json_object_set_new(kw, "impose_c_schema", json_true());
    }
    return gobj_command(priv->gobj_treedbs, "open-treedb", kw, gobj);
}

PRIVATE json_t *open_db_resp(hgobj gobj, const char *treedb_name, json_t *jn_schema) // owned
{
    return open_db_resp_as(gobj, treedb_name, jn_schema, FALSE);
}

/***************************************************************************
 *  The `c_schema_version` of a treedb in __system__, -1 when it has no
 *  projection
 ***************************************************************************/
PRIVATE json_int_t system_c_schema_version(hgobj gobj, const char *treedb_name)
{
    json_t *nodes = gobj_list_nodes(gobj_find_service(SYSTEM_TREEDB, FALSE), "treedbs",
        json_pack("{s:s}", "id", treedb_name), 0, gobj);
    json_t *node = json_array_get(nodes, 0);
    json_int_t v = node? kw_get_int(gobj, node, "c_schema_version", -1, KW_WILD_NUMBER) : -1;
    JSON_DECREF(nodes)
    return v;
}

/***************************************************************************
 *  saved-schema answers a pending save of the treedb
 ***************************************************************************/
PRIVATE BOOL has_saved_schema(hgobj gobj, const char *treedb_name)
{
    json_t *jn_resp = treedb_cmd(gobj, treedb_name, "saved-schema", json_object());
    BOOL saved = kw_get_bool(gobj, jn_resp, "data`saved", 0, 0);
    JSON_DECREF(jn_resp)
    return saved;
}

/***************************************************************************
 *  The schema file IN USE of a treedb. Return is YOURS
 ***************************************************************************/
PRIVATE json_t *load_schema_file(hgobj gobj, const char *treedb_name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char directory[PATH_MAX];
    build_path(directory, sizeof(directory), priv->path_database, treedb_name, NULL);
    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s.treedb_schema.json", treedb_name);
    return load_json_from_file(gobj, directory, filename, 0);
}

/***************************************************************************
 *  A topic of a schema whose topics are a list. Return NOT YOURS
 ***************************************************************************/
PRIVATE json_t *topic_in(json_t *jn_schema, const char *topic_name)
{
    int idx; json_t *topic;
    json_array_foreach(json_object_get(jn_schema, "topics"), idx, topic) {
        if(strcmp(json_string_value(json_object_get(topic, "id"))?
                json_string_value(json_object_get(topic, "id")) : "", topic_name)==0) {
            return topic;
        }
    }
    return NULL;
}

/***************************************************************************
 *  The columns of a topic as {name: header}, from a schema topic whose
 *  cols are a dict or a list, or from a topic desc. Return is YOURS
 ***************************************************************************/
PRIVATE json_t *headers_of(json_t *cols) // not owned
{
    json_t *headers = json_object();
    if(json_is_object(cols)) {
        const char *name; json_t *col;
        json_object_foreach(cols, name, col) {
            json_t *h = json_object_get(col, "header");
            json_object_set_new(headers, name, json_string(json_is_string(h)? json_string_value(h) : ""));
        }
    } else if(json_is_array(cols)) {
        int idx; json_t *col;
        json_array_foreach(cols, idx, col) {
            const char *name = json_string_value(json_object_get(col, "id"));
            json_t *h = json_object_get(col, "header");
            if(name) {
                json_object_set_new(headers, name,
                    json_string(json_is_string(h)? json_string_value(h) : ""));
            }
        }
    }
    return headers;
}

/***************************************************************************
 *  What the store RUNS for a topic: {name: header} of its columns, and its
 *  topic_version. NULL when the topic is not open. Return is YOURS
 ***************************************************************************/
PRIVATE json_t *running_headers(hgobj gobj, const char *treedb_name, const char *topic_name,
    json_int_t *p_topic_version)
{
    *p_topic_version = -1;
    char tranger_name[NAME_MAX];
    snprintf(tranger_name, sizeof(tranger_name), "tranger_%s", treedb_name);
    hgobj gobj_tranger = gobj_find_service(tranger_name, FALSE);
    json_t *tranger = gobj_tranger? gobj_read_pointer_attr(gobj_tranger, "tranger") : NULL;
    json_t *topic = tranger?
        json_object_get(json_object_get(tranger, "topics"), topic_name) : NULL;
    hgobj node = gobj_find_service(treedb_name, FALSE);
    if(!topic || !node) {
        return NULL;
    }
    *p_topic_version = kw_get_int(gobj, topic, "topic_version", -1, KW_WILD_NUMBER);
    json_t *desc = gobj_topic_desc(node, topic_name);
    json_t *headers = headers_of(json_object_get(desc, "cols"));
    JSON_DECREF(desc)
    return headers;
}

/***************************************************************************
 *  The header of a column in __system__, "" when the column is not there
 ***************************************************************************/
PRIVATE void system_header(hgobj gobj, const char *treedb_name, const char *topic_name,
    const char *col_name, char *bf, size_t bfsize)
{
    bf[0] = 0;
    char col_id[NAME_MAX];
    snprintf(col_id, sizeof(col_id), "%s.%s.%s", treedb_name, topic_name, col_name);
    json_t *nodes = gobj_list_nodes(gobj_find_service(SYSTEM_TREEDB, FALSE), "cols",
        json_pack("{s:s}", "id", col_id), 0, gobj);
    json_t *node = json_array_get(nodes, 0);
    if(node) {
        snprintf(bf, bfsize, "%s", kw_get_str(gobj, node, "header", "", 0));
    }
    JSON_DECREF(nodes)
}

/***************************************************************************
 *  Is a topic of the treedb projected in __system__?
 ***************************************************************************/
PRIVATE BOOL system_has_topic(hgobj gobj, const char *treedb_name, const char *topic_name)
{
    char topic_id[NAME_MAX];
    snprintf(topic_id, sizeof(topic_id), "%s.%s", treedb_name, topic_name);
    json_t *nodes = gobj_list_nodes(gobj_find_service(SYSTEM_TREEDB, FALSE), "topics",
        json_pack("{s:s}", "id", topic_id), 0, gobj);
    BOOL has = json_array_size(nodes) > 0? TRUE : FALSE;
    JSON_DECREF(nodes)
    return has;
}

/***************************************************************************
 *  The three homes of the schema AGREE:
 *
 *      - every topic of the FILE runs, at the file's topic_version, with
 *        the file's columns and headers;
 *      - every user topic that runs is in the file;
 *      - __system__ is the file: saved-schema answers no draft that
 *        differs from it (`draft_changed` {}), and every topic of the file
 *        is projected.
 ***************************************************************************/
PRIVATE int check_agree(hgobj gobj, const char *treedb_name, const char *label)
{
    int result = 0;
    json_t *file = load_schema_file(gobj, treedb_name);
    if(!file) {
        return test_fail(gobj, treedb_name, label, json_string("no schema file in use"));
    }

    int idx; json_t *topic;
    json_array_foreach(json_object_get(file, "topics"), idx, topic) {
        const char *topic_name = kw_get_str(gobj, topic, "id", "", 0);
        json_int_t file_tv = kw_get_int(gobj, topic, "topic_version", 0, KW_WILD_NUMBER);
        json_int_t running_tv;
        json_t *running = running_headers(gobj, treedb_name, topic_name, &running_tv);
        json_t *in_file = headers_of(json_object_get(topic, "cols"));
        if(!running || running_tv != file_tv || !json_equal(running, in_file) ||
                !system_has_topic(gobj, treedb_name, topic_name)) {
            result += test_fail(gobj, treedb_name, label, json_pack("{s:s, s:s, s:I, s:I, s:O, s:O, s:b}",
                "what", "a topic of the file does not run as the file says, or is not in __system__",
                "topic", topic_name,
                "file_version", file_tv,
                "running_version", running_tv,
                "in_file", in_file,
                "running", running? running : json_null(),
                "in_system", system_has_topic(gobj, treedb_name, topic_name)
            ));
        }
        JSON_DECREF(running)
        JSON_DECREF(in_file)
    }

    char tranger_name[NAME_MAX];
    snprintf(tranger_name, sizeof(tranger_name), "tranger_%s", treedb_name);
    hgobj gobj_tranger = gobj_find_service(tranger_name, FALSE);
    json_t *tranger = gobj_tranger? gobj_read_pointer_attr(gobj_tranger, "tranger") : NULL;
    const char *topic_name; json_t *running_topic;
    json_object_foreach(json_object_get(tranger, "topics"), topic_name, running_topic) {
        if(strncmp(topic_name, "__", 2)==0) {
            continue;
        }
        if(!topic_in(file, topic_name)) {
            result += test_fail(gobj, treedb_name, label, json_pack("{s:s, s:s}",
                "what", "a topic runs that the file does not declare",
                "topic", topic_name
            ));
        }
    }

    json_t *jn_resp = treedb_cmd(gobj, treedb_name, "saved-schema", json_object());
    json_t *draft_changed = kw_get_dict(gobj, jn_resp, "data`draft_changed", 0, 0);
    if(!draft_changed || json_object_size(draft_changed) > 0) {
        result += test_fail(gobj, treedb_name, label, json_pack("{s:s, s:O}",
            "what", "__system__ holds a draft that differs from the file in use",
            "saved_schema", jn_resp
        ));
    }
    JSON_DECREF(jn_resp)
    JSON_DECREF(file)
    return result;
}

/***************************************************************************
 *  What the last open withdrew, as saved-schema AND the `treedbs` row
 *  answer it: `topics` {name: kind} and `saved_schema_version`. Nothing
 *  expected (topics {} and 0) means the answer is {}.
 ***************************************************************************/
PRIVATE int check_withdrawn(hgobj gobj, const char *treedb_name, const char *label,
    json_int_t saved_version, json_t *topics) // topics owned
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    json_t *jn_resp = treedb_cmd(gobj, treedb_name, "saved-schema", json_object());
    json_t *w = kw_get_dict(gobj, jn_resp, "data`withdrawn_at_open", 0, 0);

    json_t *row_w = NULL;
    json_t *jn_rows = gobj_command(priv->gobj_treedbs, "treedbs", json_object(), gobj);
    int idx; json_t *row;
    json_array_foreach(kw_get_list(gobj, jn_rows, "data", 0, 0), idx, row) {
        if(strcmp(kw_get_str(gobj, row, "treedb_name", "", 0), treedb_name)==0) {
            row_w = json_object_get(row, "withdrawn_at_open");
        }
    }

    BOOL ok;
    if(json_object_size(topics) == 0 && saved_version == 0) {
        ok = (w && json_object_size(w) == 0)? TRUE : FALSE;
    } else {
        ok = (w &&
            json_equal(json_object_get(w, "topics"), topics) &&
            kw_get_int(gobj, w, "saved_schema_version", -1, 0) == saved_version)? TRUE : FALSE;
    }
    if(!ok || !row_w || !json_equal(w, row_w)) {
        result += test_fail(gobj, treedb_name, label, json_pack("{s:O, s:I, s:O, s:O}",
            "expected_topics", topics,
            "expected_saved_version", saved_version,
            "saved_schema", w? w : json_null(),
            "treedbs_row", row_w? row_w : json_null()
        ));
    }
    JSON_DECREF(jn_rows)
    JSON_DECREF(jn_resp)
    JSON_DECREF(topics)
    return result;
}

/***************************************************************************
 *  A column's header in the FILE in use, what the store RUNS, and in
 *  __system__, compared with what each is expected to say
 ***************************************************************************/
PRIVATE int check_header(hgobj gobj, const char *treedb_name, const char *label,
    const char *topic_name, const char *col_name,
    const char *in_file, const char *running, const char *in_system)
{
    json_t *file = load_schema_file(gobj, treedb_name);
    json_t *file_headers = headers_of(json_object_get(topic_in(file, topic_name), "cols"));
    json_int_t tv;
    json_t *run_headers = running_headers(gobj, treedb_name, topic_name, &tv);
    char sys[NAME_MAX];
    system_header(gobj, treedb_name, topic_name, col_name, sys, sizeof(sys));

    const char *f = kw_get_str(gobj, file_headers, col_name, "", 0);
    const char *r = kw_get_str(gobj, run_headers, col_name, "", 0);
    int result = 0;
    if(strcmp(f, in_file)!=0 || strcmp(r, running)!=0 || strcmp(sys, in_system)!=0) {
        result = test_fail(gobj, treedb_name, label, json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
            "topic", topic_name,
            "col", col_name,
            "in_file", f,
            "expected_in_file", in_file,
            "running", r,
            "expected_running", running,
            "in_system", sys,
            "expected_in_system", in_system
        ));
    }
    JSON_DECREF(run_headers)
    JSON_DECREF(file_headers)
    JSON_DECREF(file)
    return result;
}

/***************************************************************************
 *  RM: the developer removes a PARENT topic from the literal, and its
 *  fkey from the child. The treedb opens, and the parent is gone from
 *  the file, from what runs and from __system__. The same literal opens
 *  again, saying nothing.
 ***************************************************************************/
PRIVATE int scenario_removed_topic(hgobj gobj)
{
    const char *db = "tw_rm";
    int result = 0;

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o,o]",
            topic_of("users", 1, json_pack("{s:o, s:o, s:o}",
                "id", col_id(), "username", col_str("User"), "departments", col_fkey("Dept"))),
            topic_of("departments", 1, json_pack("{s:o, s:o, s:o}",
                "id", col_id(), "name", col_str("Name"), "users", col_hook("users", "departments")))
        )), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, db);

    for(int i=0; i<2; i++) {
        if(open_db(gobj, db, schema_of(db, 2, json_pack("[o]",
                topic_of("users", 2, json_pack("{s:o, s:o}",
                    "id", col_id(), "username", col_str("User")))
            )), FALSE) < 0) {
            return result - 1;
        }
        result += check_agree(gobj, db, "TEST FAIL: RM, a topic removed from the literal");
        if(system_has_topic(gobj, db, "departments")) {
            result += test_fail(gobj, db, "TEST FAIL: RM, the removed topic is still in __system__", NULL);
        }
        char sys[NAME_MAX];
        system_header(gobj, db, "users", "departments", sys, sizeof(sys));
        if(!empty_string(sys)) {
            result += test_fail(gobj, db, "TEST FAIL: RM, the removed fkey is still in __system__", NULL);
        }
        result += check_withdrawn(gobj, db, "TEST FAIL: RM, the developer's removal read as withdrawn work",
            0, json_object());
        close_db(gobj, db);
    }
    return result;
}

/***************************************************************************
 *  TIE+HOOK: the operator applies `users` (topic_version 2), never opened.
 *  The developer ships `users` 2 with a new fkey, and a new parent topic
 *  `groups` hooking it. The literal is newer than the file: it wins whole,
 *  the applied `users` is withdrawn and said, the treedb opens and runs
 *  the literal.
 ***************************************************************************/
PRIVATE int scenario_tie_and_hook(hgobj gobj)
{
    const char *db = "tw_tie";
    int result = 0;

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o,o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"))),
            topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
        )), FALSE) < 0) {
        return -1;
    }
    result += edit_header(gobj, db, "users", "username", "Operator user");
    result += save_schema(gobj, db);
    result += apply_schema(gobj, db);
    close_db(gobj, db);

    if(open_db(gobj, db, schema_of(db, 3, json_pack("[o,o,o]",
            topic_of("users", 2, json_pack("{s:o, s:o, s:o}",
                "id", col_id(), "username", col_str("User"), "group", col_fkey("Group"))),
            topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name"))),
            topic_of("groups", 1, json_pack("{s:o, s:o, s:o}",
                "id", col_id(), "name", col_str("Name"), "members", col_hook("users", "group")))
        )), FALSE) < 0) {
        return result - 1;
    }
    result += check_agree(gobj, db, "TEST FAIL: TIE+HOOK, the literal newer than an unopened apply");
    result += check_header(gobj, db, "TEST FAIL: TIE+HOOK, the literal does not run whole",
        "users", "username", "User", "User", "User");
    result += check_header(gobj, db, "TEST FAIL: TIE+HOOK, the literal's new fkey does not run",
        "users", "group", "Group", "Group", "Group");
    result += check_withdrawn(gobj, db, "TEST FAIL: TIE+HOOK, the withdrawn apply is not said",
        0, json_pack("{s:s}", "users", "applied"));
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  EQ: the operator applies `users` (topic_version 2), never opened; the
 *  developer ships `users` 2 with a new column. The literal runs, its new
 *  column is written, the apply is said. A second open with the same
 *  binary opens from the file, which IS the literal, and says nothing.
 ***************************************************************************/
PRIVATE int scenario_equal_topic_version(hgobj gobj)
{
    const char *db = "tw_eq";
    int result = 0;

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return -1;
    }
    result += edit_header(gobj, db, "users", "username", "Operator user");
    result += save_schema(gobj, db);
    result += apply_schema(gobj, db);
    close_db(gobj, db);

    for(int i=0; i<2; i++) {
        if(open_db(gobj, db, schema_of(db, 3, json_pack("[o]",
                topic_of("users", 2, json_pack("{s:o, s:o, s:o}",
                    "id", col_id(), "username", col_str("User"), "email", col_str("Email")))
            )), FALSE) < 0) {
            return result - 1;
        }
        result += check_agree(gobj, db, "TEST FAIL: EQ, a literal of the apply's topic_version");
        result += check_header(gobj, db, "TEST FAIL: EQ, the literal's column does not run",
            "users", "email", "Email", "Email", "Email");
        if(i == 0) {
            json_t *node = gobj_create_node(gobj_find_service(db, FALSE), "users",
                json_pack("{s:s, s:s, s:s}", "id", "u1", "username", "x", "email", "e@x"), 0, gobj);
            if(!node || strcmp(kw_get_str(gobj, node, "email", "", 0), "e@x")!=0) {
                result += test_fail(gobj, db, "TEST FAIL: EQ, a record of the literal's column", json_incref(node));
            }
            JSON_DECREF(node)
            result += check_withdrawn(gobj, db, "TEST FAIL: EQ, the withdrawn apply is not said",
                0, json_pack("{s:s}", "users", "applied"));
        } else {
            result += check_withdrawn(gobj, db, "TEST FAIL: EQ, a second open says something",
                0, json_object());
        }
        close_db(gobj, db);
    }
    return result;
}

/***************************************************************************
 *  A SAVED draft of `users` and an UNSAVED one of `departments`, and a
 *  literal newer than the file: both withdrawn, and said together with the
 *  saved schema it withdrew. The literal changes `users` only; the draft of
 *  `departments` goes too, because the literal wins whole.
 ***************************************************************************/
PRIVATE int scenario_drafts_withdrawn(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *db = "tw_saved";
    int result = 0;

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o,o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"))),
            topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
        )), FALSE) < 0) {
        return -1;
    }
    result += edit_header(gobj, db, "users", "username", "Saved login");
    result += save_schema(gobj, db);
    result += edit_header(gobj, db, "departments", "name", "Unsaved name");
    close_db(gobj, db);

    if(open_db(gobj, db, schema_of(db, 2, json_pack("[o,o]",
            topic_of("users", 2, json_pack("{s:o, s:o, s:o}",
                "id", col_id(), "username", col_str("User"), "email", col_str("Email"))),
            topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
        )), FALSE) < 0) {
        return result - 1;
    }
    result += check_agree(gobj, db, "TEST FAIL: SAVED, the literal over a saved and an unsaved draft");
    result += check_header(gobj, db, "TEST FAIL: SAVED, the unsaved draft survived",
        "departments", "name", "Name", "Name", "Name");
    result += check_withdrawn(gobj, db, "TEST FAIL: SAVED, the withdrawn drafts are not said",
        2, json_pack("{s:s, s:s}", "users", "saved", "departments", "unsaved"));

    char saved_dir[PATH_MAX];
    build_path(saved_dir, sizeof(saved_dir), priv->path_database, "__system__", "saved_schemas", NULL);
    if(file_exists(saved_dir, "tw_saved.treedb_schema.json")) {
        result += test_fail(gobj, db, "TEST FAIL: SAVED, the saved schema was not withdrawn", NULL);
    }
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  A literal NOT newer than the file is not installed: the file runs --
 *  an apply not opened yet runs at that open -- and __system__ keeps what
 *  it holds, the applied schema. EQUAL with another content: said, not
 *  applied. OLDER: "behind the schema in use".
 ***************************************************************************/
PRIVATE int scenario_literal_not_newer(hgobj gobj)
{
    const char *db = "tw_old";
    int result = 0;

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return -1;
    }
    result += edit_header(gobj, db, "users", "username", "Operator user");
    result += save_schema(gobj, db);
    result += apply_schema(gobj, db);
    close_db(gobj, db);

    if(open_db(gobj, db, schema_of(db, 2, json_pack("[o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return result - 1;
    }
    result += check_agree(gobj, db, "TEST FAIL: OLD, a literal of the file's schema_version");
    result += check_header(gobj, db, "TEST FAIL: OLD, the apply does not run at its open",
        "users", "username", "Operator user", "Operator user", "Operator user");
    result += check_withdrawn(gobj, db, "TEST FAIL: OLD, a literal not installed withdrew something",
        0, json_object());
    close_db(gobj, db);

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return result - 1;
    }
    result += check_agree(gobj, db, "TEST FAIL: OLD, a literal behind the file");
    result += check_header(gobj, db, "TEST FAIL: OLD, a literal behind the file ran",
        "users", "username", "Operator user", "Operator user", "Operator user");
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  A topic RENAMED in the literal (`departments` -> `sections`), and the
 *  hook and fkey that link it to `users` renamed with it: the old topic
 *  and the old fkey disappear, the new ones run and are projected.
 ***************************************************************************/
PRIVATE int scenario_renamed_topic(hgobj gobj)
{
    const char *db = "tw_ren";
    int result = 0;

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o,o]",
            topic_of("users", 1, json_pack("{s:o, s:o, s:o}",
                "id", col_id(), "username", col_str("User"), "departments", col_fkey("Dept"))),
            topic_of("departments", 1, json_pack("{s:o, s:o, s:o}",
                "id", col_id(), "name", col_str("Name"), "users", col_hook("users", "departments")))
        )), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, db);

    if(open_db(gobj, db, schema_of(db, 2, json_pack("[o,o]",
            topic_of("users", 2, json_pack("{s:o, s:o, s:o}",
                "id", col_id(), "username", col_str("User"), "sections", col_fkey("Section"))),
            topic_of("sections", 1, json_pack("{s:o, s:o, s:o}",
                "id", col_id(), "name", col_str("Name"), "users", col_hook("users", "sections")))
        )), FALSE) < 0) {
        return result - 1;
    }
    result += check_agree(gobj, db, "TEST FAIL: REN, a topic renamed in the literal");
    if(system_has_topic(gobj, db, "departments")) {
        result += test_fail(gobj, db, "TEST FAIL: REN, the old topic is still in __system__", NULL);
    }
    result += check_header(gobj, db, "TEST FAIL: REN, the renamed fkey",
        "users", "sections", "Section", "Section", "Section");
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  A literal newer than the file that CHANGES a topic without raising its
 *  topic_version: the file and __system__ take the literal whole, and the
 *  store keeps running its own columns (tranger2 installs a topic only
 *  over a lower topic_version). It is said, as a warning.
 ***************************************************************************/
PRIVATE int scenario_topic_not_raised(hgobj gobj)
{
    const char *db = "tw_nr";
    int result = 0;

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, db);

    if(open_db(gobj, db, schema_of(db, 2, json_pack("[o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("Login")))
        )), FALSE) < 0) {
        return result - 1;
    }
    result += check_header(gobj, db, "TEST FAIL: NR, a topic changed without its topic_version",
        "users", "username", "Login", "User", "Login");
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  impose_c_schema on (forced by the code): a literal newer than
 *  __system__ re-makes the projection whole, the removed topic included
 ***************************************************************************/
PRIVATE int scenario_imposed_removed_topic(hgobj gobj)
{
    const char *db = "tw_imp";
    int result = 0;

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o,o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"))),
            topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
        )), TRUE) < 0) {
        return -1;
    }
    close_db(gobj, db);

    if(open_db(gobj, db, schema_of(db, 2, json_pack("[o]",
            topic_of("users", 2, json_pack("{s:o, s:o, s:o}",
                "id", col_id(), "username", col_str("User"), "email", col_str("Email")))
        )), TRUE) < 0) {
        return result - 1;
    }
    result += check_agree(gobj, db, "TEST FAIL: IMP, an imposed literal that removes a topic");
    if(system_has_topic(gobj, db, "departments")) {
        result += test_fail(gobj, db, "TEST FAIL: IMP, the removed topic is still in __system__", NULL);
    }
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  GONE: a topic whose store directory is gone (its topic_var.json with it)
 *  is no APPLIED topic: nobody applied anything. Inferred from the file's
 *  topic_version being above the store's -- 0 for a topic with no
 *  topic_var.json -- it would read as "applied". The literal changes it:
 *  nothing of the operator's is withdrawn, and nothing is said.
 ***************************************************************************/
PRIVATE int scenario_missing_topic_dir_is_no_apply(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *db = "tw_l2";
    int result = 0;

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o,o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"))),
            topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
        )), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, db);

    char topic_dir[PATH_MAX];
    build_path(topic_dir, sizeof(topic_dir), priv->path_database, db, "departments", NULL);
    rmrdir(topic_dir);

    if(open_db(gobj, db, schema_of(db, 2, json_pack("[o,o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"))),
            topic_of("departments", 2, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Section name")))
        )), FALSE) < 0) {
        return result - 1;
    }
    result += check_agree(gobj, db, "TEST FAIL: GONE, a topic whose directory is gone");
    result += check_withdrawn(gobj, db, "TEST FAIL: GONE, a topic with no topic_var.json read as applied",
        0, json_object());
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  SNAP: a snapshot of __system__ holds the topic the literal removes (and
 *  the fkey column to it). The delete is refused, so the projection is
 *  NOT complete, and that is not hidden:
 *
 *      - c_schema_version does not claim the literal, so every open
 *        retries it (it was written first, the next open was silent);
 *      - save-schema refuses: it would publish the removed topic again
 *        (it did, and apply-schema resurrected it);
 *      - what the projection left behind is no operator's work: nothing
 *        is withdrawn.
 *
 *  Once the snapshot is deleted, the next open completes the projection.
 ***************************************************************************/
PRIVATE int scenario_snapshot_holds_removed_topic(hgobj gobj)
{
    const char *db = "tw_snap";
    int result = 0;
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o,o]",
            topic_of("users", 1, json_pack("{s:o, s:o, s:o}",
                "id", col_id(), "username", col_str("User"), "departments", col_fkey("Dept"))),
            topic_of("departments", 1, json_pack("{s:o, s:o, s:o}",
                "id", col_id(), "name", col_str("Name"), "users", col_hook("users", "departments")))
        )), FALSE) < 0) {
        return -1;
    }
    json_t *jn_resp = gobj_command(sys, "shoot-snap", json_pack("{s:s}", "name", "s1"), gobj);
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: SNAP, shoot-snap on __system__", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    close_db(gobj, db);

    for(int i=0; i<2; i++) {
        if(open_db(gobj, db, schema_of(db, 2, json_pack("[o]",
                topic_of("users", 2, json_pack("{s:o, s:o}",
                    "id", col_id(), "username", col_str("User")))
            )), FALSE) < 0) {
            return result - 1;
        }
        if(!system_has_topic(gobj, db, "departments")) {
            result += test_fail(gobj, db, "TEST FAIL: SNAP, a topic a snapshot holds was deleted", NULL);
        }
        if(system_c_schema_version(gobj, db) == 2) {
            result += test_fail(gobj, db,
                "TEST FAIL: SNAP, an unfinished projection claims the literal (c_schema_version)",
                json_integer(system_c_schema_version(gobj, db)));
        }
        jn_resp = treedb_cmd(gobj, db, "save-schema", json_object());
        if(kw_get_int(gobj, jn_resp, "result", -1, 0) >= 0) {
            result += test_fail(gobj, db,
                "TEST FAIL: SNAP, save-schema published an unfinished projection", json_incref(jn_resp));
        }
        JSON_DECREF(jn_resp)
        result += check_withdrawn(gobj, db, "TEST FAIL: SNAP, the projection's leftovers read as withdrawn work",
            0, json_object());
        jn_resp = treedb_cmd(gobj, db, "saved-schema", json_object());
        if(json_array_size(kw_get_list(gobj, jn_resp, "data`unfinished_projection", 0, 0)) != 2) {
            result += test_fail(gobj, db,
                "TEST FAIL: SNAP, saved-schema does not say what the projection could not remove",
                json_incref(jn_resp));
        }
        JSON_DECREF(jn_resp)
        close_db(gobj, db);
    }

    /*
     *  The snapshot goes (there is no delete-snap: its row of __snaps__)
     */
    json_t *snaps = gobj_list_nodes(sys, "__snaps__", json_pack("{s:s}", "name", "s1"), 0, gobj);
    json_t *snap = json_array_get(snaps, 0);
    if(!snap || gobj_delete_node(sys, "__snaps__",
            json_pack("{s:s}", "id", kw_get_str(gobj, snap, "id", "", 0)),
            json_object(), gobj) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: SNAP, the snapshot could not be deleted", json_incref(snaps));
    }
    JSON_DECREF(snaps)

    if(open_db(gobj, db, schema_of(db, 2, json_pack("[o]",
            topic_of("users", 2, json_pack("{s:o, s:o}",
                "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return result - 1;
    }
    result += check_agree(gobj, db, "TEST FAIL: SNAP, the projection was not completed once the snap went");
    if(system_has_topic(gobj, db, "departments")) {
        result += test_fail(gobj, db, "TEST FAIL: SNAP, the removed topic is still in __system__", NULL);
    }
    char sys_header[NAME_MAX];
    system_header(gobj, db, "users", "departments", sys_header, sizeof(sys_header));
    if(!empty_string(sys_header)) {
        result += test_fail(gobj, db, "TEST FAIL: SNAP, the removed fkey is still in __system__", NULL);
    }
    if(system_c_schema_version(gobj, db) != 2) {
        result += test_fail(gobj, db, "TEST FAIL: SNAP, a complete projection does not claim the literal",
            json_integer(system_c_schema_version(gobj, db)));
    }
    result += check_withdrawn(gobj, db, "TEST FAIL: SNAP, completing the projection withdrew work",
        0, json_object());
    jn_resp = treedb_cmd(gobj, db, "saved-schema", json_object());
    json_t *unfinished = kw_get_list(gobj, jn_resp, "data`unfinished_projection", 0, 0);
    if(!unfinished || json_array_size(unfinished) != 0) {
        result += test_fail(gobj, db, "TEST FAIL: SNAP, a complete projection still reads as unfinished",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  TWICE: open-treedb of a treedb already open here, with a newer literal
 *  and a save pending. It is refused UP FRONT: __system__ keeps its topics,
 *  the saved schema stays, nothing is withdrawn, the file is not touched.
 *  The open used to reconcile all of that first and then fail on the name
 *  of its tranger ("Internal error, tranger client NULL").
 ***************************************************************************/
PRIVATE int scenario_second_open_refused(hgobj gobj)
{
    const char *db = "tw_twice";
    int result = 0;

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o,o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"))),
            topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
        )), FALSE) < 0) {
        return -1;
    }
    result += edit_header(gobj, db, "users", "username", "Op");
    result += save_schema(gobj, db);

    json_t *jn_resp = open_db_resp(gobj, db, schema_of(db, 3, json_pack("[o]",
        topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
    )));
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0 ||
            !strstr(kw_get_str(gobj, jn_resp, "comment", "", 0), "already open")) {
        result += test_fail(gobj, db, "TEST FAIL: TWICE, a second open is not refused as already open",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)

    if(!system_has_topic(gobj, db, "departments")) {
        result += test_fail(gobj, db, "TEST FAIL: TWICE, a refused open removed a topic from __system__", NULL);
    }
    if(!has_saved_schema(gobj, db)) {
        result += test_fail(gobj, db, "TEST FAIL: TWICE, a refused open withdrew the saved schema", NULL);
    }
    result += check_withdrawn(gobj, db, "TEST FAIL: TWICE, a refused open withdrew work",
        0, json_object());
    json_t *file = load_schema_file(gobj, db);
    if(!topic_in(file, "departments") || kw_get_int(gobj, file, "schema_version", 0, KW_WILD_NUMBER) != 1) {
        result += test_fail(gobj, db, "TEST FAIL: TWICE, a refused open touched the file", json_incref(file));
    }
    JSON_DECREF(file)
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  RAN: an apply that RAN (an open read it), then a newer literal. The
 *  literal replaces a dynamic schema that was RUNNING: said, as "in_use",
 *  for the topics the apply changed. `departments`, which only the
 *  developer changed, is nobody's work: not said. The next open says
 *  nothing.
 ***************************************************************************/
PRIVATE int scenario_apply_that_ran(hgobj gobj)
{
    const char *db = "tw_ran";
    int result = 0;

    for(int i=0; i<2; i++) {
        if(open_db(gobj, db, schema_of(db, 1, json_pack("[o,o]",
                topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"))),
                topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
            )), FALSE) < 0) {
            return result - 1;
        }
        if(i == 0) {
            result += edit_header(gobj, db, "users", "username", "Operator user");
            result += save_schema(gobj, db);
            result += apply_schema(gobj, db);
        } else {
            result += check_header(gobj, db, "TEST FAIL: RAN, the apply does not run",
                "users", "username", "Operator user", "Operator user", "Operator user");
            result += check_withdrawn(gobj, db, "TEST FAIL: RAN, the open that runs the apply withdrew",
                0, json_object());
        }
        close_db(gobj, db);
    }

    for(int i=0; i<2; i++) {
        if(open_db(gobj, db, schema_of(db, 3, json_pack("[o,o]",
                topic_of("users", 3, json_pack("{s:o, s:o, s:o}",
                    "id", col_id(), "username", col_str("User"), "email", col_str("Email"))),
                topic_of("departments", 2, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Section")))
            )), FALSE) < 0) {
            return result - 1;
        }
        result += check_agree(gobj, db, "TEST FAIL: RAN, a literal over an apply that ran");
        result += check_withdrawn(gobj, db, i == 0?
                "TEST FAIL: RAN, the running dynamic schema replaced is not said" :
                "TEST FAIL: RAN, a second open says something",
            0, i == 0? json_pack("{s:s}", "users", "in_use") : json_object());
        close_db(gobj, db);
    }
    return result;
}

/***************************************************************************
 *  SEED: delete-treedb of a treedb whose file in use is an operator's
 *  apply, then an open with a literal of the SAME schema_version and
 *  another content. The file runs and the projection is seeded from it:
 *  c_schema_version must not claim the literal, and the tie is said at
 *  every open, the first one included.
 ***************************************************************************/
PRIVATE int scenario_seed_from_dynamic_file(hgobj gobj)
{
    const char *db = "tw_seed";
    int result = 0;

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return -1;
    }
    result += edit_header(gobj, db, "users", "username", "Operator user");
    result += save_schema(gobj, db);
    result += apply_schema(gobj, db);
    close_db(gobj, db);

    json_t *jn_resp = treedb_cmd(gobj, db, "delete-treedb", json_pack("{s:b}", "force", 1));
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: SEED, delete-treedb", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)

    for(int i=0; i<2; i++) {
        if(open_db(gobj, db, schema_of(db, 2, json_pack("[o]",
                topic_of("users", 2, json_pack("{s:o, s:o, s:o}",
                    "id", col_id(), "username", col_str("User"), "email", col_str("Email")))
            )), FALSE) < 0) {
            return result - 1;
        }
        result += check_header(gobj, db, "TEST FAIL: SEED, the file does not run",
            "users", "username", "Operator user", "Operator user", "Operator user");
        if(system_c_schema_version(gobj, db) == 2) {
            result += test_fail(gobj, db,
                "TEST FAIL: SEED, a projection seeded from a dynamic file claims the literal", NULL);
        }
        close_db(gobj, db);
    }
    return result;
}

/***************************************************************************
 *  LOCK: the store of the treedb is held by another process (here another
 *  open file description of this one: flock() does not tell them apart),
 *  and a newer literal arrives. The treedb opens as a REPLICA and runs its
 *  file: __system__ is not projected from a literal that does not run.
 *  Once the lock is free, the next open installs the literal, and
 *  __system__ follows.
 ***************************************************************************/
PRIVATE int scenario_client_store_locked(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *db = "tw_lock";
    int result = 0;

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o,o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"))),
            topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
        )), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, db);

    char lock_path[PATH_MAX];
    build_path(lock_path, sizeof(lock_path), priv->path_database, db, "__timeranger2__.json", NULL);
    int fd = open(lock_path, O_RDONLY|O_CLOEXEC);
    if(fd < 0 || flock(fd, LOCK_EX|LOCK_NB) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: LOCK, cannot take the lock of the store", NULL);
    }

    if(open_db(gobj, db, schema_of(db, 2, json_pack("[o]",
            topic_of("users", 2, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        result--;
    }
    if(!system_has_topic(gobj, db, "departments") || system_c_schema_version(gobj, db) != 1) {
        result += test_fail(gobj, db,
            "TEST FAIL: LOCK, __system__ projected a literal that a replica does not run",
            json_integer(system_c_schema_version(gobj, db)));
    }
    json_t *file = load_schema_file(gobj, db);
    if(kw_get_int(gobj, file, "schema_version", 0, KW_WILD_NUMBER) != 1) {
        result += test_fail(gobj, db, "TEST FAIL: LOCK, a replica installed the literal", json_incref(file));
    }
    JSON_DECREF(file)
    close_db(gobj, db);

    if(fd >= 0) {
        flock(fd, LOCK_UN);
        close(fd);
    }

    if(open_db(gobj, db, schema_of(db, 2, json_pack("[o]",
            topic_of("users", 2, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return result - 1;
    }
    result += check_agree(gobj, db, "TEST FAIL: LOCK, the literal once the store is free");
    if(system_has_topic(gobj, db, "departments") || system_c_schema_version(gobj, db) != 2) {
        result += test_fail(gobj, db, "TEST FAIL: LOCK, __system__ did not follow the literal",
            json_integer(system_c_schema_version(gobj, db)));
    }
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  REC: the record of an apply cannot be written. The apply is REFUSED and
 *  the file in use is not replaced: an apply nobody can report as withdrawn
 *  is not made.
 ***************************************************************************/
PRIVATE int scenario_apply_record_unwritable(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *db = "tw_rec";
    int result = 0;

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return -1;
    }
    result += edit_header(gobj, db, "users", "username", "Operator user");
    result += save_schema(gobj, db);

    char record_dir[PATH_MAX];
    build_path(record_dir, sizeof(record_dir),
        priv->path_database, "__system__", "saved_schemas", "tw_rec.applied.json", NULL);
    mkrdir(record_dir, 02770);

    json_t *jn_resp = treedb_cmd(gobj, db, "apply-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0 || kw_get_bool(gobj, jn_resp, "data`applied", 0, 0)) {
        result += test_fail(gobj, db, "TEST FAIL: REC, an apply whose record cannot be written was made",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    json_t *file = load_schema_file(gobj, db);
    if(kw_get_int(gobj, file, "schema_version", 0, KW_WILD_NUMBER) != 1) {
        result += test_fail(gobj, db, "TEST FAIL: REC, the file in use was replaced", json_incref(file));
    }
    JSON_DECREF(file)
    if(!has_saved_schema(gobj, db)) {
        result += test_fail(gobj, db, "TEST FAIL: REC, a refused apply lost the saved schema", NULL);
    }

    rmrdir(record_dir);
    result += apply_schema(gobj, db);
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  The operator adds a column to a topic in __system__ (a draft)
 ***************************************************************************/
PRIVATE int add_draft_col(hgobj gobj, const char *treedb_name, const char *topic_name,
    const char *col_name)
{
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);
    char topic_id[NAME_MAX];
    snprintf(topic_id, sizeof(topic_id), "%s.%s", treedb_name, topic_name);
    char col_id_[NAME_MAX];
    snprintf(col_id_, sizeof(col_id_), "%s.%s.%s", treedb_name, topic_name, col_name);

    json_t *col = gobj_create_node(sys, "cols",
        json_pack("{s:s, s:s, s:s, s:s, s:i, s:[s]}",
            "id", col_id_, "value", col_name, "header", "Draft", "type", "string",
            "fillspace", 10, "flag", "persistent"),
        json_pack("{s:b}", "refs", 1), gobj);
    if(!col) {
        return test_fail(gobj, treedb_name, "TEST FAIL: the operator's new column was refused", NULL);
    }
    JSON_DECREF(col)
    if(gobj_link_nodes(sys, "cols", "topics", json_pack("{s:s}", "id", topic_id),
            "cols", json_pack("{s:s}", "id", col_id_), gobj) < 0) {
        return test_fail(gobj, treedb_name, "TEST FAIL: the operator's new column was not linked", NULL);
    }
    return 0;
}

/***************************************************************************
 *  Is a column of a topic projected in __system__?
 ***************************************************************************/
PRIVATE BOOL system_has_col(hgobj gobj, const char *treedb_name, const char *topic_name,
    const char *col_name)
{
    char col_id_[NAME_MAX];
    snprintf(col_id_, sizeof(col_id_), "%s.%s.%s", treedb_name, topic_name, col_name);
    json_t *nodes = gobj_list_nodes(gobj_find_service(SYSTEM_TREEDB, FALSE), "cols",
        json_pack("{s:s}", "id", col_id_), 0, gobj);
    BOOL has = json_array_size(nodes) > 0? TRUE : FALSE;
    JSON_DECREF(nodes)
    return has;
}

/***************************************************************************
 *  Delete the snapshot of __system__ called `name` (there is no
 *  delete-snap: its row of __snaps__)
 ***************************************************************************/
PRIVATE int delete_system_snap(hgobj gobj, const char *treedb_name, const char *name)
{
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);
    int result = 0;
    json_t *snaps = gobj_list_nodes(sys, "__snaps__", json_pack("{s:s}", "name", name), 0, gobj);
    json_t *snap = json_array_get(snaps, 0);
    if(!snap || gobj_delete_node(sys, "__snaps__",
            json_pack("{s:s}", "id", kw_get_str(gobj, snap, "id", "", 0)),
            json_object(), gobj) < 0) {
        result = test_fail(gobj, treedb_name, "TEST FAIL: the snapshot could not be deleted",
            json_incref(snaps));
    }
    JSON_DECREF(snaps)
    return result;
}

/***************************************************************************
 *  Shoot a snapshot of __system__
 ***************************************************************************/
PRIVATE int shoot_system_snap(hgobj gobj, const char *treedb_name, const char *name)
{
    json_t *jn_resp = gobj_command(gobj_find_service(SYSTEM_TREEDB, FALSE), "shoot-snap",
        json_pack("{s:s}", "name", name), gobj);
    int result = 0;
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
        result = test_fail(gobj, treedb_name, "TEST FAIL: shoot-snap on __system__", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    return result;
}

/***************************************************************************
 *  What saved-schema answers in `draft_changed`, compared with `expected`
 ***************************************************************************/
PRIVATE int check_draft_changed(hgobj gobj, const char *treedb_name, const char *label,
    json_t *expected) // owned
{
    int result = 0;
    json_t *jn_resp = treedb_cmd(gobj, treedb_name, "saved-schema", json_object());
    json_t *draft_changed = kw_get_dict(gobj, jn_resp, "data`draft_changed", 0, 0);
    if(!draft_changed || !json_equal(draft_changed, expected)) {
        result = test_fail(gobj, treedb_name, label, json_pack("{s:O, s:O}",
            "expected", expected,
            "saved_schema", jn_resp
        ));
    }
    JSON_DECREF(jn_resp)
    JSON_DECREF(expected)
    return result;
}

/***************************************************************************
 *  Write the schema file IN USE of a treedb, as an operator or a crash
 *  would leave it
 ***************************************************************************/
PRIVATE int write_schema_file(hgobj gobj, const char *treedb_name, json_t *jn_schema) // owned
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char directory[PATH_MAX];
    build_path(directory, sizeof(directory), priv->path_database, treedb_name, NULL);
    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s.treedb_schema.json", treedb_name);
    if(save_json_to_file(gobj, directory, filename, 02770, 0660, 0, TRUE, FALSE, jn_schema) < 0) {
        return test_fail(gobj, treedb_name, "TEST FAIL: cannot write the schema file", NULL);
    }
    return 0;
}

/***************************************************************************
 *  UNF: an operator's work in __system__ while a projection is
 *  unfinished. The leftover (departments, held by a snapshot) is nobody's
 *  draft: saved-schema does not name it in `draft_changed`, and nothing
 *  withdraws it. The column the operator ADDS to users meanwhile is a
 *  draft: the retry of the projection replaces it, and SAYS so
 *  ("unsaved"). A column the operator
 *  adds to the leftover topic itself makes that topic a draft too.
 ***************************************************************************/
PRIVATE int scenario_draft_while_unfinished(hgobj gobj)
{
    const char *db = "tw_m1";
    int result = 0;

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o,o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"))),
            topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
        )), FALSE) < 0) {
        return -1;
    }
    result += shoot_system_snap(gobj, db, "m1");
    close_db(gobj, db);

    if(open_db(gobj, db, schema_of(db, 2, json_pack("[o]",
            topic_of("users", 2, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return result - 1;
    }
    result += check_draft_changed(gobj, db,
        "TEST FAIL: UNF, the leftover of an unfinished projection reads as a draft", json_object());

    result += add_draft_col(gobj, db, "users", "email");
    result += check_draft_changed(gobj, db,
        "TEST FAIL: UNF, the operator's column is not the only draft",
        json_pack("{s:b}", "users", 1));
    close_db(gobj, db);

    if(open_db(gobj, db, schema_of(db, 2, json_pack("[o]",
            topic_of("users", 2, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return result - 1;
    }
    if(system_has_col(gobj, db, "users", "email")) {
        result += test_fail(gobj, db,
            "TEST FAIL: UNF, the retry of the projection did not replace the operator's column", NULL);
    }
    result += check_withdrawn(gobj, db,
        "TEST FAIL: UNF, the operator's column went and nothing said it",
        0, json_pack("{s:s}", "users", "unsaved"));
    close_db(gobj, db);

    /*
     *  A column added to the LEFTOVER topic makes it somebody's draft
     */
    if(open_db(gobj, db, schema_of(db, 2, json_pack("[o]",
            topic_of("users", 2, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return result - 1;
    }
    result += add_draft_col(gobj, db, "departments", "budget");
    result += check_draft_changed(gobj, db,
        "TEST FAIL: UNF, a column added to a leftover topic is not a draft",
        json_pack("{s:b}", "departments", 1));
    close_db(gobj, db);

    result += delete_system_snap(gobj, db, "m1");
    if(open_db(gobj, db, schema_of(db, 2, json_pack("[o]",
            topic_of("users", 2, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return result - 1;
    }
    result += check_agree(gobj, db, "TEST FAIL: UNF, the projection was not completed");
    result += check_withdrawn(gobj, db,
        "TEST FAIL: UNF, completing the projection did not say the column added to the leftover",
        0, json_pack("{s:s}", "departments", "unsaved"));
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  LEFT: the leftovers of an unfinished projection are not withdrawn work on
 *  ANY path: the imposed retry, and a NEWER literal that arrives while the
 *  projection is unfinished. Both reported the leftover `groups` as
 *  "unsaved" once the snapshot was gone.
 ***************************************************************************/
PRIVATE int scenario_leftovers_on_every_path(hgobj gobj)
{
    int result = 0;

    for(int imposed=1; imposed>=0; imposed--) {
        const char *db = imposed? "tw_m2i" : "tw_m2d";
        if(open_db(gobj, db, schema_of(db, 1, json_pack("[o,o]",
                topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"))),
                topic_of("groups", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
            )), imposed) < 0) {
            return result - 1;
        }
        result += shoot_system_snap(gobj, db, db);
        close_db(gobj, db);

        if(open_db(gobj, db, schema_of(db, 2, json_pack("[o]",
                topic_of("users", 2, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
            )), imposed) < 0) {
            return result - 1;
        }
        result += check_withdrawn(gobj, db, "TEST FAIL: LEFT, an unfinished projection withdrew work",
            0, json_object());
        close_db(gobj, db);

        result += delete_system_snap(gobj, db, db);

        int next_version = imposed? 2 : 3;
        if(open_db(gobj, db, schema_of(db, next_version, json_pack("[o]",
                topic_of("users", next_version, json_pack("{s:o, s:o}",
                    "id", col_id(), "username", col_str("User")))
            )), imposed) < 0) {
            return result - 1;
        }
        if(system_has_topic(gobj, db, "groups")) {
            result += test_fail(gobj, db, "TEST FAIL: LEFT, the leftover is still in __system__", NULL);
        }
        result += check_withdrawn(gobj, db,
            imposed? "TEST FAIL: LEFT, the imposed retry reported the leftover as withdrawn work" :
                "TEST FAIL: LEFT, a newer literal reported the leftover as withdrawn work",
            0, json_object());
        close_db(gobj, db);
    }
    return result;
}

/***************************************************************************
 *  SEED0: a projection whose c_schema_version is 0 because it was SEEDED from
 *  a dynamic file (not because anything was left unfinished), then the
 *  developer takes that file into C, same schema_version. Nothing is
 *  installed, nothing is projected: the operator's drafts stay, nothing is
 *  said.
 ***************************************************************************/
PRIVATE int scenario_seed_is_not_unfinished(hgobj gobj)
{
    const char *db = "tw_l1";
    int result = 0;

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return -1;
    }
    result += edit_header(gobj, db, "users", "username", "Operator user");
    result += save_schema(gobj, db);
    result += apply_schema(gobj, db);
    close_db(gobj, db);

    json_t *jn_resp = treedb_cmd(gobj, db, "delete-treedb", json_pack("{s:b}", "force", 1));
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: SEED0, delete-treedb", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return result - 1;
    }
    result += add_draft_col(gobj, db, "users", "phone");
    result += edit_header(gobj, db, "users", "username", "Draft header");
    close_db(gobj, db);

    json_t *file = load_schema_file(gobj, db);
    if(open_db(gobj, db, file, FALSE) < 0) {
        return result - 1;
    }
    if(!system_has_col(gobj, db, "users", "phone")) {
        result += test_fail(gobj, db, "TEST FAIL: SEED0, the operator's column went", NULL);
    }
    result += check_header(gobj, db, "TEST FAIL: SEED0, the operator's header draft was overwritten",
        "users", "username", "Operator user", "Operator user", "Draft header");
    result += check_withdrawn(gobj, db, "TEST FAIL: SEED0, a literal equal to the file withdrew work",
        0, json_object());
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  APCR: the process died between the record of an apply and its rename.
 *  The record on disk is then of the apply that never happened, and its
 *  `previous` is the record of the file still in use (an apply that RAN).
 *  The next apply must keep the topics of that one: a newer literal then
 *  reports `users` as "in_use".
 ***************************************************************************/
PRIVATE int scenario_apply_after_a_crash(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *db = "tw_l3";
    int result = 0;

    for(int i=0; i<2; i++) {
        if(open_db(gobj, db, schema_of(db, 1, json_pack("[o,o]",
                topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"))),
                topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
            )), FALSE) < 0) {
            return result - 1;
        }
        if(i == 0) {
            result += edit_header(gobj, db, "users", "username", "Operator user");
            result += save_schema(gobj, db);
            result += apply_schema(gobj, db);
        }
        close_db(gobj, db);
    }

    /*
     *  The crash: a record written for an apply of 3 whose rename never
     *  happened, over the record of the file in use (2, "in_use")
     */
    char record_dir[PATH_MAX];
    build_path(record_dir, sizeof(record_dir), priv->path_database, "__system__", "saved_schemas", NULL);
    json_t *in_use_record = load_json_from_file(gobj, record_dir, "tw_l3.applied.json", 0);
    json_t *expected_topics = json_pack("{s:s}", "users", "in_use");
    if(!in_use_record || !json_equal(json_object_get(in_use_record, "topics"), expected_topics)) {
        result += test_fail(gobj, db, "TEST FAIL: APCR, the apply that ran is not recorded in_use",
            json_incref(in_use_record));
    }
    JSON_DECREF(expected_topics)
    json_t *crashed = json_pack("{s:i, s:{s:s}, s:o}",
        "schema_version", 3,
        "topics", "users", "applied",
        "previous", in_use_record? in_use_record : json_object()
    );
    save_json_to_file(gobj, record_dir, "tw_l3.applied.json", 02770, 0660, 0, TRUE, FALSE, crashed);

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o,o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"))),
            topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
        )), FALSE) < 0) {
        return result - 1;
    }
    result += edit_header(gobj, db, "departments", "name", "Operator name");
    result += save_schema(gobj, db);
    result += apply_schema(gobj, db);
    close_db(gobj, db);

    if(open_db(gobj, db, schema_of(db, 4, json_pack("[o,o]",
            topic_of("users", 4, json_pack("{s:o, s:o, s:o}",
                "id", col_id(), "username", col_str("User"), "email", col_str("Email"))),
            topic_of("departments", 4, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Section")))
        )), FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db,
        "TEST FAIL: APCR, the apply after a crash lost the topics of the apply that ran",
        0, json_pack("{s:s, s:s}", "users", "in_use", "departments", "applied"));
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  SEEDF: a projection SEEDED from the file that fails (here a column with a
 *  flag the meta-schema does not know). It is recorded as unfinished, so it is
 *  retried at every open and what it wrote is no draft; save-schema
 *  refuses meanwhile. Once the file is right, the next open completes it.
 *  It reset the numbers to 0 and was never retried when the file was not
 *  the literal: the partial projection read as drafts for ever.
 ***************************************************************************/
PRIVATE json_t *l4_file(const char *db, const char *flag)
{
    return schema_of(db, 2, json_pack("[o]",
        topic_of("users", 2, json_pack("{s:o, s:o, s:{s:s, s:i, s:s, s:[s,s]}}",
            "id", col_id(), "username", col_str("User"),
            "note", "header", "Note", "fillspace", 10, "type", "string", "flag", "persistent", flag))
    ));
}
PRIVATE int scenario_failed_seed_is_retried(hgobj gobj)
{
    const char *db = "tw_l4";
    int result = 0;

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, db);
    json_t *jn_resp = treedb_cmd(gobj, db, "delete-treedb", json_pack("{s:b}", "force", 1));
    JSON_DECREF(jn_resp)
    result += write_schema_file(gobj, db, l4_file(db, "strange_flag"));

    for(int i=0; i<2; i++) {
        if(open_db(gobj, db, schema_of(db, 1, json_pack("[o]",
                topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
            )), FALSE) < 0) {
            return result - 1;
        }
        result += check_draft_changed(gobj, db,
            "TEST FAIL: SEEDF, the partial projection of a failed seed reads as a draft", json_object());
        jn_resp = treedb_cmd(gobj, db, "save-schema", json_object());
        if(kw_get_int(gobj, jn_resp, "result", -1, 0) >= 0) {
            result += test_fail(gobj, db,
                "TEST FAIL: SEEDF, save-schema published a failed seed", json_incref(jn_resp));
        }
        JSON_DECREF(jn_resp)
        close_db(gobj, db);
    }

    result += write_schema_file(gobj, db, l4_file(db, "writable"));
    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return result - 1;
    }
    if(!system_has_col(gobj, db, "users", "note")) {
        result += test_fail(gobj, db, "TEST FAIL: SEEDF, the seed was not completed", NULL);
    }
    result += check_draft_changed(gobj, db,
        "TEST FAIL: SEEDF, a completed seed differs from the file", json_object());
    jn_resp = treedb_cmd(gobj, db, "saved-schema", json_object());
    if(json_array_size(kw_get_list(gobj, jn_resp, "data`unfinished_projection", 0, 0)) != 0) {
        result += test_fail(gobj, db, "TEST FAIL: SEEDF, a completed seed still reads as unfinished",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  FAIL: an open that fails is never "Treedb opened!". Refused by C_TREEDB
 *  (the literal has a column both hook and fkey), or by the library
 *  (treedb_open_db() finds no topics in the schema file in use): the
 *  answer is -1 and names the yuno; the second one says what to do.
 ***************************************************************************/
PRIVATE int scenario_open_that_fails(hgobj gobj)
{
    const char *db = "tw_l6";
    int result = 0;

    json_t *jn_resp = open_db_resp(gobj, db, schema_of(db, 1, json_pack("[o]",
        topic_of("users", 1, json_pack("{s:o, s:o, s:{s:s, s:i, s:s, s:[s,s], s:{s:s}}}",
            "id", col_id(), "username", col_str("User"),
            "boss", "header", "Boss", "fillspace", 10, "type", "array", "flag", "hook", "fkey",
                "hook", "users", "boss"))
    )));
    char prefix[NAME_MAX];
    snprintf(prefix, sizeof(prefix), "%s:", gobj_yuno_role_plus_name());
    const char *comment = kw_get_str(gobj, jn_resp, "comment", "", 0);
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0 || strncmp(comment, prefix, strlen(prefix))!=0) {
        result += test_fail(gobj, db, "TEST FAIL: FAIL, a refused schema does not name the yuno",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return result - 1;
    }
    close_db(gobj, db);
    result += write_schema_file(gobj, db, json_pack("{s:s, s:i}", "id", db, "schema_version", 1));

    jn_resp = open_db_resp(gobj, db, schema_of(db, 1, json_pack("[o]",
        topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
    )));
    comment = kw_get_str(gobj, jn_resp, "comment", "", 0);
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0 ||
            strncmp(comment, prefix, strlen(prefix))!=0 || !strstr(comment, "close-treedb")) {
        result += test_fail(gobj, db, "TEST FAIL: FAIL, an open that failed answered as opened",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  The two literals of the scenarios below: v1 users + departments, and
 *  v2 without departments
 ***************************************************************************/
PRIVATE json_t *users_departments_v1(const char *db)
{
    return schema_of(db, 1, json_pack("[o,o]",
        topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"))),
        topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
    ));
}
PRIVATE json_t *users_only_v2(const char *db)
{
    return schema_of(db, 2, json_pack("[o]",
        topic_of("users", 2, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
    ));
}

/***************************************************************************
 *  The record of an unfinished projection, as it is on disk: NULL when
 *  there is none, json null when it cannot be parsed. Return is YOURS
 ***************************************************************************/
PRIVATE json_t *unfinished_record(hgobj gobj, const char *treedb_name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), priv->path_database, "__system__", "saved_schemas", NULL);
    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s.unfinished.json", treedb_name);
    if(!file_exists(dir, filename)) {
        return NULL;
    }
    json_t *record = load_json_from_file(gobj, dir, filename, 0);
    return record? record : json_null();
}

/***************************************************************************
 *  UNF2: a column the operator adds to the LEFTOVER topic stays a draft
 *  through every retry that cannot finish, and is reported by the open
 *  that removes it. A retry while the snapshot still holds the topic does
 *  not take the column into the record's leftovers: saved-schema goes on
 *  showing it.
 ***************************************************************************/
PRIVATE int scenario_draft_on_leftover_across_retries(hgobj gobj)
{
    const char *db = "tw_m1b";
    int result = 0;

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    result += shoot_system_snap(gobj, db, "m1b");
    close_db(gobj, db);

    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    result += add_draft_col(gobj, db, "departments", "budget");
    result += check_draft_changed(gobj, db,
        "TEST FAIL: UNF2, a column added to a leftover topic is not a draft",
        json_pack("{s:b}", "departments", 1));
    close_db(gobj, db);

    /*
     *  A retry while the snapshot still holds departments
     */
    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    result += check_draft_changed(gobj, db,
        "TEST FAIL: UNF2, a retry that cannot finish took the operator's column for a leftover",
        json_pack("{s:b}", "departments", 1));
    result += check_withdrawn(gobj, db,
        "TEST FAIL: UNF2, a retry that removed nothing reported withdrawn work",
        0, json_object());
    json_t *record = unfinished_record(gobj, db);
    json_t *leftovers = json_object_get(record, "leftovers");
    int idx; json_t *jn_id;
    json_array_foreach(leftovers, idx, jn_id) {
        if(strcmp(json_string_value(jn_id)? json_string_value(jn_id) : "", "tw_m1b.departments.budget")==0) {
            result += test_fail(gobj, db,
                "TEST FAIL: UNF2, the record names the operator's column as a leftover",
                json_incref(record));
        }
    }
    JSON_DECREF(record)
    close_db(gobj, db);

    result += delete_system_snap(gobj, db, "m1b");
    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    if(system_has_col(gobj, db, "departments", "budget")) {
        result += test_fail(gobj, db, "TEST FAIL: UNF2, the projection was not completed", NULL);
    }
    result += check_agree(gobj, db, "TEST FAIL: UNF2, the projection was not completed");
    result += check_withdrawn(gobj, db,
        "TEST FAIL: UNF2, the operator's column went and nothing said it",
        0, json_pack("{s:s}", "departments", "unsaved"));
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  KEEP: the report of a draft does not depend on whether the write that
 *  replaces it succeeds. A draft stays a draft while a projection cannot
 *  replace it, and it is reported ONCE, by the open that replaces it.
 *
 *      tw_m2r  a draft on a topic the literal removes (a column added and
 *              a header edited), and the delete is refused;
 *      tw_m2c  a column the operator added to a topic the literal keeps,
 *              and its delete is refused: it is not reported while it is
 *              still there.
 ***************************************************************************/
PRIVATE int scenario_draft_where_the_projection_fails(hgobj gobj)
{
    int result = 0;

    for(int on_removed_topic=1; on_removed_topic>=0; on_removed_topic--) {
        const char *db = on_removed_topic? "tw_m2r" : "tw_m2c";
        const char *topic = on_removed_topic? "departments" : "users";

        if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
            return result - 1;
        }
        result += add_draft_col(gobj, db, topic, "budget");
        if(on_removed_topic) {
            result += edit_header(gobj, db, "departments", "name", "Operator name");
        }
        result += shoot_system_snap(gobj, db, db);
        close_db(gobj, db);

        for(int i=0; i<2; i++) {
            if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
                return result - 1;
            }
            result += check_withdrawn(gobj, db,
                "TEST FAIL: KEEP, a draft still in __system__ was reported as withdrawn",
                0, json_object());
            result += check_draft_changed(gobj, db,
                "TEST FAIL: KEEP, a draft the projection could not replace is no longer a draft",
                json_pack("{s:b}", topic, 1));
            close_db(gobj, db);
        }

        result += delete_system_snap(gobj, db, db);
        if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
            return result - 1;
        }
        result += check_agree(gobj, db, "TEST FAIL: KEEP, the projection was not completed");
        result += check_withdrawn(gobj, db,
            "TEST FAIL: KEEP, the open that replaced the draft did not say it",
            0, json_pack("{s:s}", topic, "unsaved"));
        close_db(gobj, db);
    }
    return result;
}

/***************************************************************************
 *  TORN: a record of an unfinished projection that cannot be read (torn)
 *  still says the projection is unfinished: saved-schema answers it,
 *  save-schema refuses, and the next open retries and writes it again.
 ***************************************************************************/
PRIVATE int scenario_unreadable_unfinished_record(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *db = "tw_l1b";
    int result = 0;

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    result += shoot_system_snap(gobj, db, "l1b");
    close_db(gobj, db);

    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    char path[PATH_MAX];
    build_path(path, sizeof(path), priv->path_database, "__system__", "saved_schemas",
        "tw_l1b.unfinished.json", NULL);
    int fd = open(path, O_WRONLY|O_TRUNC);
    if(fd < 0 || write(fd, "{\"schema_version\": 2, \"not_rem", 30) != 30) {
        result += test_fail(gobj, db, "TEST FAIL: TORN, cannot tear the record", NULL);
    }
    if(fd >= 0) {
        close(fd);
    }

    json_t *jn_resp = treedb_cmd(gobj, db, "saved-schema", json_object());
    if(json_array_size(kw_get_list(gobj, jn_resp, "data`unfinished_projection", 0, 0)) == 0) {
        result += test_fail(gobj, db,
            "TEST FAIL: TORN, an unreadable record reads as a finished projection", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    jn_resp = treedb_cmd(gobj, db, "save-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) >= 0) {
        result += test_fail(gobj, db,
            "TEST FAIL: TORN, save-schema published past an unreadable record", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    close_db(gobj, db);

    /*
     *  The next open retries, and writes the record again, whole
     */
    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    json_t *record = unfinished_record(gobj, db);
    if(!json_is_object(record) || json_array_size(json_object_get(record, "not_removed")) == 0) {
        result += test_fail(gobj, db,
            "TEST FAIL: TORN, the retry did not write the record again", json_incref(record));
    }
    JSON_DECREF(record)
    close_db(gobj, db);

    result += delete_system_snap(gobj, db, "l1b");
    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    if(system_has_topic(gobj, db, "departments")) {
        result += test_fail(gobj, db, "TEST FAIL: TORN, the projection was not completed", NULL);
    }
    result += check_agree(gobj, db, "TEST FAIL: TORN, the projection was not completed");

    /*
     *  What the torn record left is unknown: it is taken for a draft, and
     *  reported, never deleted in silence
     */
    result += check_withdrawn(gobj, db,
        "TEST FAIL: TORN, what a torn record left went and nothing said it",
        0, json_pack("{s:s}", "departments", "unsaved"));
    record = unfinished_record(gobj, db);
    if(record) {
        result += test_fail(gobj, db,
            "TEST FAIL: TORN, a completed projection left its record", json_incref(record));
    }
    JSON_DECREF(record)
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  STAMP: the node of a treedb in __system__ is created WITHOUT its numbers
 *  (schema_version and c_schema_version 0), and stamped last, when the
 *  whole projection is written. 7.25.4 created it with them: a crash
 *  before the topics were written left a projection that said it was of
 *  the literal, and the next open took it as done -- __system__ with no
 *  topics. The first record of the node, on disk, is what says it.
 ***************************************************************************/
PRIVATE int scenario_first_projection_stamped_last(hgobj gobj)
{
    const char *db = "tw_l2b";
    int result = 0;

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    hgobj gobj_tranger = gobj_find_service("tranger_system_schema", FALSE);
    json_t *tranger = gobj_tranger? gobj_read_pointer_attr(gobj_tranger, "tranger") : NULL;
    json_t *data = json_array();
    json_t *iterator = tranger? tranger2_open_iterator(
        tranger, "treedbs", db, json_object(), NULL, "", "test", data, NULL
    ) : NULL;
    json_t *first = json_array_get(data, 0);
    if(!first ||
            kw_get_int(gobj, first, "schema_version", -1, KW_WILD_NUMBER) != 0 ||
            kw_get_int(gobj, first, "c_schema_version", -1, KW_WILD_NUMBER) != 0) {
        result += test_fail(gobj, db,
            "TEST FAIL: STAMP, the node of the treedb was created with its numbers, before its topics",
            json_incref(data));
    }
    if(system_c_schema_version(gobj, db) != 1) {
        result += test_fail(gobj, db, "TEST FAIL: STAMP, the complete projection was not stamped",
            json_integer(system_c_schema_version(gobj, db)));
    }
    if(iterator) {
        tranger2_close_iterator(tranger, iterator);
    }
    JSON_DECREF(data)
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  FAIL2: after an open that failed (treedb_open_db() refused the schema
 *  file in use), a second open says THAT, not "already open", and the
 *  `treedbs` row says the treedb did not open.
 *
 *  YUNO: every answer of open-treedb and close-treedb names the yuno.
 ***************************************************************************/
PRIVATE int check_comment_prefix(hgobj gobj, const char *treedb_name, const char *label,
    json_t *jn_resp, const char *must_say) // jn_resp owned
{
    char prefix[NAME_MAX];
    snprintf(prefix, sizeof(prefix), "%s:", gobj_yuno_role_plus_name());
    const char *comment = kw_get_str(gobj, jn_resp, "comment", "", 0);
    int result = 0;
    if(strncmp(comment, prefix, strlen(prefix))!=0 || (must_say && !strstr(comment, must_say))) {
        result = test_fail(gobj, treedb_name, label, json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    return result;
}
PRIVATE int scenario_failed_open_says_so(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *db = "tw_l3b";
    int result = 0;

    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, db);
    result += write_schema_file(gobj, db, json_pack("{s:s, s:i}", "id", db, "schema_version", 2));

    json_t *jn_resp = open_db_resp(gobj, db, users_only_v2(db));
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0) {
        result += test_fail(gobj, db, "TEST FAIL: FAIL2, a failed open answered as opened",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)

    /*
     *  with_link_events written on the C_NODE of a treedb that did not
     *  open: nothing to hand the callback to, and nothing is logged (see
     *  the expected log list)
     */
    hgobj gobj_node = gobj_find_service(db, FALSE);
    if(!gobj_node) {
        result += test_fail(gobj, db, "TEST FAIL: FAIL2, no C_NODE after the failed open", NULL);
    } else {
        gobj_write_bool_attr(gobj_node, "with_link_events", TRUE);
    }

    jn_resp = open_db_resp(gobj, db, users_only_v2(db));
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0) {
        result += test_fail(gobj, db, "TEST FAIL: FAIL2, a second open was accepted",
            json_incref(jn_resp));
    }
    result += check_comment_prefix(gobj, db,
        "TEST FAIL: FAIL2, the second open does not say that the first one failed",
        jn_resp, "did not open");

    json_t *jn_rows = gobj_command(priv->gobj_treedbs, "treedbs", json_object(), gobj);
    json_t *row_found = NULL;
    int idx; json_t *row;
    json_array_foreach(kw_get_list(gobj, jn_rows, "data", 0, 0), idx, row) {
        if(strcmp(kw_get_str(gobj, row, "treedb_name", "", 0), db)==0) {
            row_found = row;
        }
    }
    if(!row_found || !json_is_false(json_object_get(row_found, "opened"))) {
        result += test_fail(gobj, db, "TEST FAIL: FAIL2, the treedbs row does not say it did not open",
            json_incref(jn_rows));
    }
    JSON_DECREF(jn_rows)

    /*
     *  delete-treedb says it did not open, and to close-treedb it first:
     *  it said "while it is OPEN"
     */
    jn_resp = treedb_cmd(gobj, db, "delete-treedb", json_pack("{s:b}", "force", 1));
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0) {
        result += test_fail(gobj, db, "TEST FAIL: FAIL2, delete-treedb of a treedb that did not open",
            json_incref(jn_resp));
    }
    result += check_comment_prefix(gobj, db,
        "TEST FAIL: FAIL2, delete-treedb does not say the treedb did not open",
        jn_resp, "did not open");

    /*
     *  The recovery is clean: close-treedb logs nothing (it logged "TreeDB
     *  not found" twice, with stacks), see the expected log list
     */
    jn_resp = gobj_command(priv->gobj_treedbs, "close-treedb",
        json_pack("{s:s, s:b}", "treedb_name", db, "force", 1), gobj);
    result += check_comment_prefix(gobj, db, "TEST FAIL: YUNO, close-treedb does not name the yuno",
        jn_resp, NULL);

    /*
     *  YUNO: the refusals of the parameters name the yuno too
     */
    jn_resp = gobj_command(priv->gobj_treedbs, "open-treedb",
        json_pack("{s:s}", "filename_mask", "%Y"), gobj);
    result += check_comment_prefix(gobj, db,
        "TEST FAIL: YUNO, open-treedb with no treedb_name does not name the yuno", jn_resp, NULL);
    jn_resp = gobj_command(priv->gobj_treedbs, "open-treedb",
        json_pack("{s:s, s:s}", "treedb_name", db, "filename_mask", ""), gobj);
    result += check_comment_prefix(gobj, db,
        "TEST FAIL: YUNO, open-treedb with no filename_mask does not name the yuno", jn_resp, NULL);
    jn_resp = gobj_command(priv->gobj_treedbs, "close-treedb",
        json_pack("{s:b}", "force", 1), gobj);
    result += check_comment_prefix(gobj, db,
        "TEST FAIL: YUNO, close-treedb with no treedb_name does not name the yuno", jn_resp, NULL);
    jn_resp = gobj_command(priv->gobj_treedbs, "close-treedb",
        json_pack("{s:s, s:b}", "treedb_name", "tw_nothing", "force", 1), gobj);
    result += check_comment_prefix(gobj, db,
        "TEST FAIL: YUNO, close-treedb of an unknown treedb does not name the yuno", jn_resp, NULL);
    return result;
}

/***************************************************************************
 *  The operator deletes a whole topic in __system__ (a draft), with its
 *  columns
 ***************************************************************************/
PRIVATE int delete_system_topic(hgobj gobj, const char *treedb_name, const char *topic_name)
{
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);
    char topic_id[NAME_MAX];
    snprintf(topic_id, sizeof(topic_id), "%s.%s", treedb_name, topic_name);
    char col_prefix[NAME_MAX + 2];
    snprintf(col_prefix, sizeof(col_prefix), "%s.", topic_id);

    int result = 0;
    if(gobj_delete_node(sys, "topics", json_pack("{s:s}", "id", topic_id),
            json_pack("{s:b}", "force", 1), gobj) < 0) {
        result += test_fail(gobj, treedb_name, "TEST FAIL: the operator's delete of a topic was refused",
            json_string(topic_id));
    }
    json_t *cols = gobj_list_nodes(sys, "cols", json_object(), 0, gobj);
    int idx; json_t *col;
    json_array_foreach(cols, idx, col) {
        const char *col_id = kw_get_str(gobj, col, "id", "", 0);
        if(strncmp(col_id, col_prefix, strlen(col_prefix))!=0) {
            continue;
        }
        if(gobj_delete_node(sys, "cols", json_pack("{s:s}", "id", col_id),
                json_pack("{s:b}", "force", 1), gobj) < 0) {
            result += test_fail(gobj, treedb_name,
                "TEST FAIL: the operator's delete of a column was refused", json_string(col_id));
        }
    }
    JSON_DECREF(cols)
    return result;
}

/***************************************************************************
 *  DEL: the operator's draft DELETES a whole topic in __system__, and a
 *  newer literal declares it: the projection re-creates it, and that
 *  replaces the draft, so it is reported -- "unsaved" (tw_n7u), or
 *  "saved" when a save published the deletion (tw_n7s, with the withdrawn
 *  saved schema).
 ***************************************************************************/
PRIVATE int scenario_deleted_topic_draft(hgobj gobj)
{
    int result = 0;

    for(int saved=0; saved<2; saved++) {
        const char *db = saved? "tw_n7s" : "tw_n7u";

        if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
            return result - 1;
        }
        result += delete_system_topic(gobj, db, "departments");
        result += check_draft_changed(gobj, db,
            "TEST FAIL: DEL, a topic the operator deleted is not a draft",
            json_pack("{s:b}", "departments", 1));
        if(saved) {
            result += save_schema(gobj, db);
        }
        close_db(gobj, db);

        if(open_db(gobj, db, schema_of(db, 3, json_pack("[o,o]",
                topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"))),
                topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
            )), FALSE) < 0) {
            return result - 1;
        }
        if(!system_has_topic(gobj, db, "departments")) {
            result += test_fail(gobj, db, "TEST FAIL: DEL, the literal did not re-create the topic", NULL);
        }
        result += check_agree(gobj, db, "TEST FAIL: DEL, the projection is not the literal");
        result += check_withdrawn(gobj, db,
            "TEST FAIL: DEL, the deletion the literal replaced was not reported",
            saved? 2 : 0, json_pack("{s:s}", "departments", saved? "saved" : "unsaved"));
        close_db(gobj, db);
    }
    return result;
}

/***************************************************************************
 *  EMPTY: a seed that died before its end (after delete-treedb, the node of
 *  the treedb is created with 0 / 0 and nothing else; emulated by hand)
 *  is completed by the next open, although the file runs and there is no
 *  record: nothing is reported, and __system__ is the file again. Not
 *  retried, every topic would read as a draft, and save-schema would
 *  publish a schema with no topics, which leaves a treedb that cannot
 *  open.
 *
 *  And a schema with NO topics is refused by save-schema and by
 *  apply-schema: a treedb without topics does not open.
 ***************************************************************************/
PRIVATE int scenario_seed_that_died(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *db = "tw_n1";
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);
    int result = 0;

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, db);
    json_t *nodes = gobj_list_nodes(sys, "treedbs", json_pack("{s:s}", "id", db), 0, gobj);
    json_int_t meta_version = kw_get_int(gobj, json_array_get(nodes, 0), "system_schema_version",
        0, KW_WILD_NUMBER);
    JSON_DECREF(nodes)
    json_t *jn_resp = treedb_cmd(gobj, db, "delete-treedb", json_pack("{s:b}", "force", 1));
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: EMPTY, delete-treedb", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    json_t *node = gobj_create_node(sys, "treedbs",
        json_pack("{s:s, s:i, s:i, s:I}", "id", db, "schema_version", 0, "c_schema_version", 0,
            "system_schema_version", meta_version),
        json_pack("{s:b}", "refs", 1), gobj);
    if(!node) {
        result += test_fail(gobj, db, "TEST FAIL: EMPTY, cannot emulate the seed that died", NULL);
    }
    JSON_DECREF(node)

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return result - 1;
    }
    if(!system_has_topic(gobj, db, "users") || !system_has_topic(gobj, db, "departments") ||
            system_c_schema_version(gobj, db) != 1) {
        result += test_fail(gobj, db, "TEST FAIL: EMPTY, the seed that died was not completed",
            json_integer(system_c_schema_version(gobj, db)));
    }
    result += check_agree(gobj, db, "TEST FAIL: EMPTY, the seed that died was not completed");
    result += check_withdrawn(gobj, db, "TEST FAIL: EMPTY, a seed that died reported withdrawn work",
        0, json_object());

    /*
     *  The operator deletes every topic: save-schema refuses
     */
    result += delete_system_topic(gobj, db, "users");
    result += delete_system_topic(gobj, db, "departments");
    jn_resp = treedb_cmd(gobj, db, "save-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0 ||
            !strstr(kw_get_str(gobj, jn_resp, "comment", "", 0), "no topics")) {
        result += test_fail(gobj, db, "TEST FAIL: EMPTY, save-schema saved a schema with no topics",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)

    /*
     *  A saved schema with no topics (written by hand): apply-schema refuses
     */
    char saved_dir[PATH_MAX];
    build_path(saved_dir, sizeof(saved_dir), priv->path_database, "__system__", "saved_schemas", NULL);
    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s.treedb_schema.json", db);
    if(save_json_to_file(gobj, saved_dir, filename, 02770, 0660, 0, TRUE, FALSE,
            json_pack("{s:s, s:i, s:[]}", "id", db, "schema_version", 5, "topics")) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: EMPTY, cannot write the saved schema", NULL);
    }
    jn_resp = treedb_cmd(gobj, db, "saved-schema", json_object());
    if(!kw_get_bool(gobj, jn_resp, "data`saved", 0, 0) ||
            kw_get_bool(gobj, jn_resp, "data`can_apply", 1, 0) ||
            !strstr(kw_get_str(gobj, jn_resp, "comment", "", 0), "no topics")) {
        result += test_fail(gobj, db,
            "TEST FAIL: EMPTY, saved-schema says a saved schema with no topics can be applied",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    jn_resp = treedb_cmd(gobj, db, "apply-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0 ||
            kw_get_bool(gobj, jn_resp, "data`applied", 0, 0) ||
            !strstr(kw_get_str(gobj, jn_resp, "comment", "", 0), "no topics")) {
        result += test_fail(gobj, db, "TEST FAIL: EMPTY, apply-schema applied a schema with no topics",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    json_t *file = load_schema_file(gobj, db);
    if(kw_get_int(gobj, file, "schema_version", 0, KW_WILD_NUMBER) != 1) {
        result += test_fail(gobj, db, "TEST FAIL: EMPTY, the file in use changed", json_incref(file));
    }
    JSON_DECREF(file)
    file_remove(saved_dir, filename);
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  SAVRM: a SAVED draft on the topic the literal removes, and a snapshot
 *  refuses the delete. The first open withdraws the saved schema and says
 *  that; the draft stays one. The open that finally replaces it says it
 *  as "saved" -- the kind is kept in the record.
 ***************************************************************************/
PRIVATE int scenario_saved_draft_across_retries(hgobj gobj)
{
    const char *db = "tw_n4";
    int result = 0;

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    result += add_draft_col(gobj, db, "departments", "budget");
    result += save_schema(gobj, db);
    result += shoot_system_snap(gobj, db, "n4");
    close_db(gobj, db);

    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db,
        "TEST FAIL: SAVRM, the first open did not say the saved schema it withdrew",
        2, json_object());
    result += check_draft_changed(gobj, db,
        "TEST FAIL: SAVRM, a draft the projection could not replace is no longer a draft",
        json_pack("{s:b}", "departments", 1));
    json_t *record = unfinished_record(gobj, db);
    json_t *expected_kinds = json_pack("{s:s}", "departments", "saved");
    if(!json_equal(json_object_get(record, "draft_kinds"), expected_kinds)) {
        result += test_fail(gobj, db, "TEST FAIL: SAVRM, the record does not keep the kind of the draft",
            json_incref(record));
    }
    JSON_DECREF(expected_kinds)
    JSON_DECREF(record)
    close_db(gobj, db);

    result += delete_system_snap(gobj, db, "n4");
    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    result += check_agree(gobj, db, "TEST FAIL: SAVRM, the projection was not completed");
    result += check_withdrawn(gobj, db,
        "TEST FAIL: SAVRM, a saved draft was reported as unsaved",
        0, json_pack("{s:s}", "departments", "saved"));
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  The operator adds a whole topic in __system__ (a draft), with its `id`
 *  column, at topic_version `topic_version` (the editor makes it 1)
 ***************************************************************************/
PRIVATE int add_system_topic(hgobj gobj, const char *treedb_name, const char *topic_name,
    int topic_version)
{
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);
    char topic_id[NAME_MAX];
    snprintf(topic_id, sizeof(topic_id), "%s.%s", treedb_name, topic_name);

    json_t *topic = gobj_create_node(sys, "topics",
        json_pack("{s:s, s:s, s:s, s:s, s:s, s:i, s:b, s:b}",
            "id", topic_id,
            "value", topic_name,
            "pkey", "id",
            "system_flag", "sf_string_key",
            "tkey", "",
            "topic_version", topic_version,
            "system_topic", 0,
            "main_topic", 0
        ),
        json_pack("{s:b}", "refs", 1),
        gobj
    );
    if(!topic) {
        return test_fail(gobj, treedb_name, "TEST FAIL: the operator's new topic was refused",
            json_string(topic_id));
    }
    JSON_DECREF(topic)
    if(gobj_link_nodes(sys, "topics", "treedbs", json_pack("{s:s}", "id", treedb_name),
            "topics", json_pack("{s:s}", "id", topic_id), gobj) < 0) {
        return test_fail(gobj, treedb_name, "TEST FAIL: the operator's new topic was not linked",
            json_string(topic_id));
    }
    return add_draft_col(gobj, treedb_name, topic_name, "id");
}

/***************************************************************************
 *  A newer literal v3: users + departments, and `groups` when asked
 ***************************************************************************/
PRIVATE json_t *users_departments_v3(const char *db, BOOL with_groups)
{
    json_t *topics = json_pack("[o,o]",
        topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"))),
        topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
    );
    if(with_groups) {
        json_array_append_new(topics,
            topic_of("groups", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name"))));
    }
    return schema_of(db, 3, topics);
}

/***************************************************************************
 *  AD: a topic the operator ADDED in __system__ and did not save, while a
 *  save of ANOTHER topic is pending. The added topic has topic_version 1
 *  and the file in use has none of it (0): that is not "a version raised
 *  by save-schema", the saved schema does not declare it. A newer literal
 *  reports it "unsaved":
 *
 *      tw_adu  the literal does not declare `groups` (it is removed);
 *      tw_adl  the literal declares `groups` (it is rewritten);
 *      tw_adr  a snapshot holds `departments` and `groups`: the first
 *              open is unfinished and the record keeps `groups` as
 *              "unsaved"; the open that completes says it so.
 *
 *  And the two cases next to it stay as they were: an unsaved DELETION of
 *  a topic with another save pending is "unsaved" (tw_dlu), and an added
 *  topic that was SAVED is "saved" (tw_ads).
 ***************************************************************************/
PRIVATE int scenario_added_topic_draft(hgobj gobj)
{
    int result = 0;

    for(int declared=0; declared<2; declared++) {
        const char *db = declared? "tw_adl" : "tw_adu";
        if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
            return result - 1;
        }
        result += edit_header(gobj, db, "users", "username", "Operator user");
        result += save_schema(gobj, db);
        result += add_system_topic(gobj, db, "groups", 1);
        result += check_draft_changed(gobj, db,
            "TEST FAIL: AD, the added topic is not a draft",
            json_pack("{s:b}", "groups", 1));
        close_db(gobj, db);

        if(open_db(gobj, db, users_departments_v3(db, declared? TRUE : FALSE), FALSE) < 0) {
            return result - 1;
        }
        result += check_agree(gobj, db, "TEST FAIL: AD, the projection is not the literal");
        result += check_withdrawn(gobj, db,
            "TEST FAIL: AD, an unsaved added topic was reported as saved",
            2, json_pack("{s:s, s:s}", "users", "saved", "groups", "unsaved"));
        close_db(gobj, db);
    }

    /*
     *  tw_adr: the retry path, the kind kept in the record
     */
    const char *db = "tw_adr";
    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return result - 1;
    }
    result += edit_header(gobj, db, "users", "username", "Operator user");
    result += save_schema(gobj, db);
    result += add_system_topic(gobj, db, "groups", 1);
    result += shoot_system_snap(gobj, db, "adr");
    close_db(gobj, db);

    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db,
        "TEST FAIL: AD, the unfinished open did not say only the saved users",
        2, json_pack("{s:s}", "users", "saved"));
    json_t *record = unfinished_record(gobj, db);
    json_t *expected_kinds = json_pack("{s:s}", "groups", "unsaved");
    if(!json_equal(json_object_get(record, "draft_kinds"), expected_kinds)) {
        result += test_fail(gobj, db, "TEST FAIL: AD, the record keeps a wrong kind for the added topic",
            json_incref(record));
    }
    JSON_DECREF(expected_kinds)
    JSON_DECREF(record)
    close_db(gobj, db);

    result += delete_system_snap(gobj, db, "adr");
    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    result += check_agree(gobj, db, "TEST FAIL: AD, the projection was not completed");
    result += check_withdrawn(gobj, db,
        "TEST FAIL: AD, the completing open reported the added topic as saved",
        0, json_pack("{s:s}", "groups", "unsaved"));
    close_db(gobj, db);

    /*
     *  tw_dlu: an unsaved deletion, another topic saved
     */
    db = "tw_dlu";
    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return result - 1;
    }
    result += edit_header(gobj, db, "users", "username", "Operator user");
    result += save_schema(gobj, db);
    result += delete_system_topic(gobj, db, "departments");
    close_db(gobj, db);
    if(open_db(gobj, db, users_departments_v3(db, FALSE), FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db,
        "TEST FAIL: AD, an unsaved deletion with another save pending",
        2, json_pack("{s:s, s:s}", "users", "saved", "departments", "unsaved"));
    close_db(gobj, db);

    /*
     *  tw_ads: an added topic that was saved
     */
    db = "tw_ads";
    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return result - 1;
    }
    result += add_system_topic(gobj, db, "groups", 1);
    result += save_schema(gobj, db);
    close_db(gobj, db);
    if(open_db(gobj, db, users_departments_v3(db, FALSE), FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db,
        "TEST FAIL: AD, a saved added topic",
        2, json_pack("{s:s}", "groups", "saved"));
    close_db(gobj, db);

    return result;
}

/***************************************************************************
 *  LE: an EDIT of a leftover is the operator's work. A snapshot holds
 *  `departments`, which the literal removes: the projection is unfinished
 *  and `departments` is a leftover. The operator then edits it:
 *
 *      tw_lec  the header of the column `departments.name`;
 *      tw_let  an attribute of the topic `departments` itself
 *              (`main_topic`).
 *
 *  The edit is a draft: saved-schema shows it while the projection is
 *  unfinished, a retry that cannot finish keeps it a draft (tw_lec: not a
 *  leftover in the new record, its kind kept), and the open that removes
 *  the topic reports it "unsaved". An unedited leftover is still nobody's
 *  draft (scenario SNAP).
 ***************************************************************************/
PRIVATE int scenario_edited_leftover(hgobj gobj)
{
    int result = 0;

    for(int on_topic=0; on_topic<2; on_topic++) {
        const char *db = on_topic? "tw_let" : "tw_lec";

        if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
            return result - 1;
        }
        result += shoot_system_snap(gobj, db, db);
        close_db(gobj, db);

        if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
            return result - 1;
        }
        if(on_topic) {
            char topic_id[NAME_MAX];
            snprintf(topic_id, sizeof(topic_id), "%s.departments", db);
            json_t *edited = gobj_update_node(
                gobj_find_service(SYSTEM_TREEDB, FALSE),
                "topics",
                json_pack("{s:s, s:b}", "id", topic_id, "main_topic", 1),
                json_pack("{s:b}", "refs", 1),
                gobj
            );
            if(!edited) {
                result += test_fail(gobj, db, "TEST FAIL: LE, the operator edit of the topic was refused",
                    NULL);
            }
            JSON_DECREF(edited)
        } else {
            result += edit_header(gobj, db, "departments", "name", "Operator name");
        }
        result += check_draft_changed(gobj, db,
            "TEST FAIL: LE, an edited leftover is not a draft",
            json_pack("{s:b}", "departments", 1));
        close_db(gobj, db);

        if(!on_topic) {
            /*
             *  A retry that cannot finish: the edit stays a draft
             */
            if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
                return result - 1;
            }
            result += check_withdrawn(gobj, db,
                "TEST FAIL: LE, a retry that did not remove the edit reported it",
                0, json_object());
            result += check_draft_changed(gobj, db,
                "TEST FAIL: LE, the edited leftover is no draft after a retry",
                json_pack("{s:b}", "departments", 1));
            json_t *record = unfinished_record(gobj, db);
            json_t *expected_kinds = json_pack("{s:s}", "departments", "unsaved");
            char col_id[NAME_MAX];
            snprintf(col_id, sizeof(col_id), "%s.departments.name", db);
            BOOL listed = FALSE;
            int idx; json_t *jn_id;
            json_array_foreach(json_object_get(record, "leftovers"), idx, jn_id) {
                if(json_is_string(jn_id) && strcmp(json_string_value(jn_id), col_id)==0) {
                    listed = TRUE;
                }
            }
            json_t *nodes = json_object_get(record, "leftover_nodes");
            if(json_object_size(nodes) != json_array_size(json_object_get(record, "leftovers")) ||
                    json_object_get(json_object_get(nodes, col_id), "header")) {
                result += test_fail(gobj, db,
                    "TEST FAIL: LE, the record does not keep what the projection left",
                    json_incref(record));
            }
            if(listed || !json_equal(json_object_get(record, "draft_kinds"), expected_kinds)) {
                result += test_fail(gobj, db,
                    "TEST FAIL: LE, the record takes the edited leftover for a leftover",
                    json_incref(record));
            }
            JSON_DECREF(expected_kinds)
            JSON_DECREF(record)
            close_db(gobj, db);
        }

        result += delete_system_snap(gobj, db, db);
        if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
            return result - 1;
        }
        result += check_agree(gobj, db, "TEST FAIL: LE, the projection was not completed");
        result += check_withdrawn(gobj, db,
            "TEST FAIL: LE, the edit of a leftover was deleted in silence",
            0, json_pack("{s:s}", "departments", "unsaved"));
        close_db(gobj, db);
    }
    return result;
}

/***************************************************************************
 *  A restart of __system__, as a new process sees it: C_TREEDB stops (its
 *  treedb closes, its tranger closes every file) and starts again, and
 *  __system__ is loaded from DISK. What a failed write left only in
 *  memory is gone.
 ***************************************************************************/
PRIVATE void restart_system(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->gobj_treedbs);
    gobj_start(priv->gobj_treedbs);
}

/***************************************************************************
 *  chmod every file of the key `key` of the topic `cols` of __system__:
 *  0440 makes the next write of that key fail
 ***************************************************************************/
PRIVATE int chmod_system_col_key(hgobj gobj, const char *treedb_name, const char *key, mode_t mode)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), priv->path_database, "__system__", "cols", "keys", key, NULL);
    DIR *d = opendir(dir);
    if(!d) {
        return test_fail(gobj, treedb_name, "TEST FAIL: FW, the key directory cannot be read",
            json_string(dir));
    }
    int result = 0;
    int changed = 0;
    struct dirent *de;
    while((de = readdir(d)) != NULL) {
        if(de->d_name[0] == '.') {
            continue;
        }
        char path[PATH_MAX];
        build_path(path, sizeof(path), dir, de->d_name, NULL);
        if(chmod(path, mode) < 0) {
            result += test_fail(gobj, treedb_name, "TEST FAIL: FW, chmod failed", json_string(path));
        } else {
            changed++;
        }
    }
    closedir(d);
    if(changed == 0) {
        result += test_fail(gobj, treedb_name, "TEST FAIL: FW, the key has no files", json_string(dir));
    }
    return result;
}

/***************************************************************************
 *  FW: a write of the projection FAILS on disk (the files of the column
 *  `users.username` are read-only): the update is taken back in memory,
 *  so memory and disk both say "User". The projection is unfinished, and
 *  the record lists `users.username` in `not_written`, with no node kept
 *  for it. After a RESTART of __system__ and with the files writable, the
 *  open that completes the projection reports nothing: a difference at an
 *  id the projection failed to write is nobody's work, not an "unsaved"
 *  draft of `users`.
 ***************************************************************************/
PRIVATE json_t *users_v1b(const char *db)
{
    return schema_of(db, 2, json_pack("[o,o]",
        topic_of("users", 2, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User v1b"))),
        topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
    ));
}

PRIVATE int scenario_failed_write_then_restart(hgobj gobj)
{
    const char *db = "tw_fw";
    const char *key = "tw_fw.users.username";
    int result = 0;

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, db);
    restart_system(gobj);

    result += chmod_system_col_key(gobj, db, key, 0440);
    if(open_db(gobj, db, users_v1b(db), FALSE) < 0) {
        return result - 1;
    }
    json_t *record = unfinished_record(gobj, db);
    json_t *not_written = json_object_get(record, "not_written");
    if(json_array_size(not_written) != 1 ||
            strcmp(json_string_value(json_array_get(not_written, 0))?
                json_string_value(json_array_get(not_written, 0)) : "", key)!=0) {
        result += test_fail(gobj, db, "TEST FAIL: FW, the failed write is not in the record",
            json_incref(record));
    }
    JSON_DECREF(record)
    close_db(gobj, db);
    result += chmod_system_col_key(gobj, db, key, 0660);

    restart_system(gobj);
    if(open_db(gobj, db, users_v1b(db), FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db,
        "TEST FAIL: FW, a failed write of the projection was reported as the operator's work",
        0, json_object());
    result += check_agree(gobj, db, "TEST FAIL: FW, the projection was not completed");
    result += check_header(gobj, db, "TEST FAIL: FW, the header of the completed projection",
        "users", "username", "User v1b", "User v1b", "User v1b");
    record = unfinished_record(gobj, db);
    if(record) {
        result += test_fail(gobj, db, "TEST FAIL: FW, a completed projection left its record",
            json_incref(record));
    }
    JSON_DECREF(record)
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  FWS: the same failed write, retried by the SAME process (no restart in
 *  between). The failed update was taken back in memory, so the retry
 *  finds the column to write and writes it: after a restart the disk says
 *  what the literal says, and nothing is a draft. An update that stayed in
 *  memory would leave the retry nothing to write: the projection taken as
 *  done, and after a restart the disk still saying "User" -- a draft of
 *  `users` that nobody made.
 ***************************************************************************/
PRIVATE int scenario_failed_write_retried_in_process(hgobj gobj)
{
    const char *db = "tw_fws";
    const char *key = "tw_fws.users.username";
    int result = 0;

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, db);
    restart_system(gobj);

    result += chmod_system_col_key(gobj, db, key, 0440);
    if(open_db(gobj, db, users_v1b(db), FALSE) < 0) {
        return result - 1;
    }
    close_db(gobj, db);
    result += chmod_system_col_key(gobj, db, key, 0660);

    if(open_db(gobj, db, users_v1b(db), FALSE) < 0) {
        return result - 1;
    }
    json_t *record = unfinished_record(gobj, db);
    if(record) {
        result += test_fail(gobj, db, "TEST FAIL: FWS, the retry did not complete the projection",
            json_incref(record));
    }
    JSON_DECREF(record)
    close_db(gobj, db);

    restart_system(gobj);
    if(open_db(gobj, db, users_v1b(db), FALSE) < 0) {
        return result - 1;
    }
    result += check_agree(gobj, db, "TEST FAIL: FWS, the retry did not write what it said it wrote");
    result += check_header(gobj, db, "TEST FAIL: FWS, the header on disk after the retry",
        "users", "username", "User v1b", "User v1b", "User v1b");
    result += check_withdrawn(gobj, db,
        "TEST FAIL: FWS, the open after the retry reported something",
        0, json_object());
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  UL: the operator UNLINKS a leftover column from its topic. A snapshot
 *  holds `departments`, which the literal removes, so it and its columns
 *  are leftovers; `departments.name` is unlinked. The column is still a
 *  node of __system__, in no topic. That is an edit of the leftover:
 *  saved-schema shows `departments` in `draft_changed`, and the open that
 *  completes the projection removes the column with its topic and reports
 *  `departments` as "unsaved". A later literal that declares the column
 *  again creates it: a column left in __system__ would make that literal
 *  fail on it at every open ("Node already exists").
 ***************************************************************************/
PRIVATE int scenario_unlinked_leftover_col(hgobj gobj)
{
    const char *db = "tw_ul";
    int result = 0;
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    result += shoot_system_snap(gobj, db, db);
    close_db(gobj, db);
    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    if(gobj_unlink_nodes(sys, "cols",
            "topics", json_pack("{s:s}", "id", "tw_ul.departments"),
            "cols", json_pack("{s:s}", "id", "tw_ul.departments.name"), gobj) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: UL, the operator's unlink was refused", NULL);
    }
    result += check_draft_changed(gobj, db,
        "TEST FAIL: UL, an unlinked leftover column is not a draft",
        json_pack("{s:b}", "departments", 1));
    close_db(gobj, db);

    result += delete_system_snap(gobj, db, db);
    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    result += check_agree(gobj, db, "TEST FAIL: UL, the projection was not completed");
    result += check_withdrawn(gobj, db,
        "TEST FAIL: UL, the unlink of a leftover column was withdrawn in silence",
        0, json_pack("{s:s}", "departments", "unsaved"));
    if(system_has_col(gobj, db, "departments", "name")) {
        result += test_fail(gobj, db, "TEST FAIL: UL, the unlinked column is still in __system__", NULL);
    }
    close_db(gobj, db);

    json_t *v3 = schema_of(db, 3, json_pack("[o,o]",
        topic_of("users", 2, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"))),
        topic_of("departments", 2, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name v3")))
    ));
    if(open_db(gobj, db, v3, FALSE) < 0) {
        return result - 1;
    }
    json_t *record = unfinished_record(gobj, db);
    if(record) {
        result += test_fail(gobj, db,
            "TEST FAIL: UL, a literal that declares the column again did not complete",
            json_incref(record));
    }
    JSON_DECREF(record)
    result += check_agree(gobj, db, "TEST FAIL: UL, the column declared again is not projected");
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  OA: a column node that no topic holds is in __system__ with the id a
 *  newer literal declares (the operator created it and never linked it;
 *  a link of a projection that failed leaves the same). The projection
 *  takes that node for the column -- written as the literal says and
 *  linked to its topic -- instead of failing on it at every open ("Node
 *  already exists"). The node is in no topic, so it changes no schema:
 *  saved-schema shows no draft. It is the operator's work all the same,
 *  and what the projection does to it is said: `users`, "unsaved" (no
 *  save carries a node that is in no topic).
 ***************************************************************************/
PRIVATE int scenario_orphan_col_adopted(hgobj gobj)
{
    const char *db = "tw_oa";
    int result = 0;
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return -1;
    }
    json_t *col = gobj_create_node(sys, "cols",
        json_pack("{s:s, s:s, s:s, s:s, s:i, s:[s]}",
            "id", "tw_oa.users.email", "value", "email", "header", "Operator email",
            "type", "string", "fillspace", 10, "flag", "persistent"),
        json_pack("{s:b}", "refs", 1), gobj);
    if(!col) {
        result += test_fail(gobj, db, "TEST FAIL: OA, the operator's column was refused", NULL);
    }
    JSON_DECREF(col)
    result += check_draft_changed(gobj, db,
        "TEST FAIL: OA, a column that no topic holds is taken for a change of the schema",
        json_object());
    close_db(gobj, db);

    json_t *v3 = schema_of(db, 3, json_pack("[o]",
        topic_of("users", 3, json_pack("{s:o, s:o, s:o}",
            "id", col_id(), "username", col_str("User"), "email", col_str("Email")))
    ));
    if(open_db(gobj, db, v3, FALSE) < 0) {
        return result - 1;
    }
    json_t *record = unfinished_record(gobj, db);
    if(record) {
        result += test_fail(gobj, db, "TEST FAIL: OA, the projection failed on the column node",
            json_incref(record));
    }
    JSON_DECREF(record)
    result += check_agree(gobj, db, "TEST FAIL: OA, the column is not projected");
    result += check_header(gobj, db, "TEST FAIL: OA, the column is not what the literal says",
        "users", "email", "Email", "Email", "Email");
    result += check_withdrawn(gobj, db,
        "TEST FAIL: OA, the operator's column was replaced in silence",
        0, json_pack("{s:s}", "users", "unsaved"));
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  OT: the operator unlinks a whole TOPIC from its treedb node: the topic
 *  and its columns stay in __system__, held by nothing the treedb reaches.
 *  A newer literal that declares the topic takes those nodes for it,
 *  instead of failing on them at every open, and reports the topic.
 ***************************************************************************/
PRIVATE int scenario_orphan_topic_adopted(hgobj gobj)
{
    const char *db = "tw_ot";
    int result = 0;
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    if(gobj_unlink_nodes(sys, "topics",
            "treedbs", json_pack("{s:s}", "id", db),
            "topics", json_pack("{s:s}", "id", "tw_ot.departments"), gobj) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: OT, the operator's unlink was refused", NULL);
    }
    result += check_draft_changed(gobj, db,
        "TEST FAIL: OT, an unlinked topic is not a draft",
        json_pack("{s:b}", "departments", 1));
    close_db(gobj, db);

    if(open_db(gobj, db, users_departments_v3(db, FALSE), FALSE) < 0) {
        return result - 1;
    }
    json_t *record = unfinished_record(gobj, db);
    if(record) {
        result += test_fail(gobj, db, "TEST FAIL: OT, the projection failed on the topic node",
            json_incref(record));
    }
    JSON_DECREF(record)
    result += check_agree(gobj, db, "TEST FAIL: OT, the topic is not projected");
    result += check_withdrawn(gobj, db,
        "TEST FAIL: OT, the operator's unlink of a topic was replaced in silence",
        0, json_pack("{s:s}", "departments", "unsaved"));
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  MS: the leftovers were kept under an OLDER meta-schema (the record says
 *  a lower `system_schema_version`), and the newer one added a field to
 *  `cols`: every node loaded from disk now carries it with its default,
 *  and the kept nodes do not (emulated: `description` is taken out of
 *  them). What the projection left cannot be told from an edit of it any
 *  more: every leftover is taken as left, and that is said, never
 *  reported as the operator's work.
 ***************************************************************************/
PRIVATE int scenario_leftovers_under_older_meta_schema(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *db = "tw_ms";
    int result = 0;

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    result += shoot_system_snap(gobj, db, db);
    close_db(gobj, db);
    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    close_db(gobj, db);

    json_t *meta = treedb_create_system_schema();
    json_int_t meta_version = kw_get_int(gobj, meta, "schema_version", 0, KW_WILD_NUMBER);
    JSON_DECREF(meta)

    json_t *record = unfinished_record(gobj, db);
    if(!json_is_object(record)) {
        return result + test_fail(gobj, db, "TEST FAIL: MS, the projection left no record", record);
    }
    json_t *nodes = json_object_get(record, "leftover_nodes");
    if(json_object_size(nodes) == 0) {
        result += test_fail(gobj, db, "TEST FAIL: MS, the record keeps no leftover node",
            json_incref(record));
    }
    int stripped = 0;
    const char *id; json_t *node;
    json_object_foreach(nodes, id, node) {
        if(json_object_get(node, "description")) {
            json_object_del(node, "description");
            stripped++;
        }
    }
    if(stripped == 0) {
        result += test_fail(gobj, db, "TEST FAIL: MS, no kept column carries `description`",
            json_incref(record));
    }
    json_object_set_new(record, "system_schema_version", json_integer(meta_version - 1));
    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), priv->path_database, "__system__", "saved_schemas", NULL);
    if(save_json_to_file(gobj, dir, "tw_ms.unfinished.json", 02770, 0660, 0, TRUE, FALSE,
            record) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: MS, cannot write the record", NULL);
    }

    result += delete_system_snap(gobj, db, db);
    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db,
        "TEST FAIL: MS, leftovers kept under an older meta-schema were reported as the operator's work",
        0, json_object());
    result += check_agree(gobj, db, "TEST FAIL: MS, the projection was not completed");
    if(system_has_topic(gobj, db, "departments")) {
        result += test_fail(gobj, db, "TEST FAIL: MS, the leftover topic is still in __system__", NULL);
    }
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  Watch the writes of __system__ (see ac_system_write), or stop
 ***************************************************************************/
PRIVATE void watch_system_writes(hgobj gobj, BOOL watch)
{
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);
    gobj_event_t events[] = {
        EV_TREEDB_NODE_CREATED,
        EV_TREEDB_NODE_UPDATED,
        EV_TREEDB_NODE_DELETED,
        EV_TREEDB_NODE_LINKED,
        EV_TREEDB_NODE_UNLINKED,
        0
    };
    for(int i = 0; events[i]; i++) {
        if(watch) {
            gobj_subscribe_event(sys, events[i], 0, gobj);
        } else {
            gobj_unsubscribe_event(sys, events[i], 0, gobj);
        }
    }
}

/***************************************************************************
 *  Make every later write of the key `key` of the topic `system_topic` of
 *  __system__ fail: each open descriptor of one of its files is replaced
 *  by a read-only one (tranger2 keeps them open, so a chmod would come
 *  too late). Return how many were replaced.
 ***************************************************************************/
PRIVATE int break_key_writes(hgobj gobj, const char *system_topic, const char *key)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), priv->path_database, "__system__", system_topic, "keys", key, NULL);
    size_t dlen = strlen(dir);

    int replaced = 0;
    DIR *d = opendir("/proc/self/fd");
    if(!d) {
        return 0;
    }
    struct dirent *de;
    while((de = readdir(d)) != NULL) {
        if(de->d_name[0] == '.') {
            continue;
        }
        char link[PATH_MAX];
        snprintf(link, sizeof(link), "/proc/self/fd/%s", de->d_name);
        char target[PATH_MAX];
        ssize_t n = readlink(link, target, sizeof(target) - 1);
        if(n <= 0) {
            continue;
        }
        target[n] = 0;
        if(strncmp(target, dir, dlen)!=0 || target[dlen] != '/') {
            continue;
        }
        int ro = open(target, O_RDONLY|O_CLOEXEC);
        if(ro < 0) {
            continue;
        }
        if(dup2(ro, atoi(de->d_name)) >= 0) {
            replaced++;
        }
        close(ro);
    }
    closedir(d);
    return replaced;
}

/***************************************************************************
 *  chmod every file of the key `key` of the topic `system_topic` of
 *  __system__, or of the directory of its keys when `key` is NULL
 ***************************************************************************/
PRIVATE int chmod_system_key(hgobj gobj, const char *treedb_name, const char *system_topic,
    const char *key, mode_t mode)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char dir[PATH_MAX];
    if(!key) {
        build_path(dir, sizeof(dir), priv->path_database, "__system__", system_topic, "keys", NULL);
        if(chmod(dir, mode) < 0) {
            return test_fail(gobj, treedb_name, "TEST FAIL: chmod of the keys directory failed",
                json_string(dir));
        }
        return 0;
    }
    build_path(dir, sizeof(dir), priv->path_database, "__system__", system_topic, "keys", key, NULL);
    DIR *d = opendir(dir);
    if(!d) {
        return test_fail(gobj, treedb_name, "TEST FAIL: the key directory cannot be read",
            json_string(dir));
    }
    int result = 0;
    int changed = 0;
    struct dirent *de;
    while((de = readdir(d)) != NULL) {
        if(de->d_name[0] == '.') {
            continue;
        }
        char path[PATH_MAX];
        build_path(path, sizeof(path), dir, de->d_name, NULL);
        if(chmod(path, mode) < 0) {
            result += test_fail(gobj, treedb_name, "TEST FAIL: chmod failed", json_string(path));
        } else {
            changed++;
        }
    }
    closedir(d);
    if(changed == 0) {
        result += test_fail(gobj, treedb_name, "TEST FAIL: the key has no files", json_string(dir));
    }
    return result;
}

/***************************************************************************
 *  A node of __system__, NULL when it is not there. Return is YOURS
 ***************************************************************************/
PRIVATE json_t *system_node(hgobj gobj, const char *system_topic, const char *id)
{
    json_t *nodes = gobj_list_nodes(gobj_find_service(SYSTEM_TREEDB, FALSE), system_topic,
        json_pack("{s:s}", "id", id), json_pack("{s:b}", "refs", 1), gobj);
    json_t *node = json_incref(json_array_get(nodes, 0));
    JSON_DECREF(nodes)
    return node;
}

/***************************************************************************
 *  Does the node `id` of __system__ link to `ref` ("<topic>^<id>^<hook>")?
 ***************************************************************************/
PRIVATE BOOL system_node_links(hgobj gobj, const char *system_topic, const char *id,
    const char *fkey, const char *ref)
{
    json_t *node = system_node(gobj, system_topic, id);
    json_t *refs = json_object_get(node, fkey);
    BOOL links = (json_is_array(refs) && json_list_str_index(refs, ref, FALSE) >= 0)? TRUE : FALSE;
    JSON_DECREF(node)
    return links;
}

/***************************************************************************
 *  A forked child has the io_uring rings of its parent mapped SHARED, and
 *  its own copy of their head and tail: a child that submits moves the
 *  kernel's head under the parent, whose next submissions all find the
 *  queue full (yev_loop keeps them, "Submission queue full and the kernel
 *  takes nothing"), and the
 *  completions of the child's operations reach the parent with pointers
 *  into memory that is not the parent's. So the child takes a ring of its
 *  own before anything else: the parent's operations in flight stay the
 *  parent's. HACK the ring is the first member of the loop (yev_loop.c).
 ***************************************************************************/
PRIVATE void child_takes_its_own_ring(void)
{
    struct io_uring *ring = (struct io_uring *)yuno_event_loop();
    unsigned entries = ring->sq.ring_entries;
    io_uring_queue_exit(ring);
    if(io_uring_queue_init(entries, ring, 0) < 0) {
        _exit(4);
    }
}

/***************************************************************************
 *  CR: the process DIES half way through a projection. v1 is projected;
 *  v2 changes a header and adds a column to `users`, adds the topic
 *  `roles`, keeps `groups` and removes `departments`: updates, creates,
 *  links, unlinks and deletes. A child process opens v2 and is killed
 *  (SIGKILL) at the k-th write of __system__, for every k of the
 *  projection. Each time, the next open with v2 completes the projection,
 *  says NOTHING that nobody did, and says, once, the operator's drafts
 *  it replaced (`drafts`: a header edited in `users`, a column added to
 *  `groups`) -- whether the process that died had replaced them already
 *  or not.
 *
 *  The log is not compared line by line here: which lines a retry says
 *  depends on where the process died. What is compared is the count of
 *  errors and warnings of each retry: no error, and the one warning of
 *  the drafts it withdraws (when there are drafts).
 ***************************************************************************/
PRIVATE json_t *cr_v1(const char *db)
{
    return schema_of(db, 1, json_pack("[o,o,o]",
        topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"))),
        topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name"))),
        topic_of("groups", 1, json_pack("{s:o, s:o}", "id", col_id(), "gname", col_str("Group")))
    ));
}

PRIVATE json_t *cr_v2(const char *db)
{
    return schema_of(db, 2, json_pack("[o,o,o]",
        topic_of("users", 2, json_pack("{s:o, s:o, s:o}",
            "id", col_id(), "username", col_str("User v2"), "email", col_str("Email"))),
        topic_of("roles", 1, json_pack("{s:o, s:o}", "id", col_id(), "rname", col_str("Role"))),
        topic_of("groups", 1, json_pack("{s:o, s:o}", "id", col_id(), "gname", col_str("Group")))
    ));
}

PRIVATE int cr_prepare(hgobj gobj, const char *db, BOOL drafts)
{
    int result = 0;
    if(open_db(gobj, db, cr_v1(db), FALSE) < 0) {
        return -1;
    }
    if(drafts) {
        result += edit_header(gobj, db, "users", "username", "Operator");
        result += add_draft_col(gobj, db, "groups", "extra");
    }
    close_db(gobj, db);
    return result;
}

/*
 *  Open `db` with v2 in a CHILD process killed at the `kill_at`-th write
 *  of __system__. 1 when it was killed there, 0 when the projection ended
 *  before, -1 on error.
 */
PRIVATE int cr_open_killed(hgobj gobj, const char *db, int kill_at)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if(pid < 0) {
        return test_fail(gobj, db, "TEST FAIL: CR, fork() failed", json_string(strerror(errno)));
    }
    if(pid == 0) {
        child_takes_its_own_ring();
        priv->system_writes = 0;
        priv->kill_at_write = kill_at;
        watch_system_writes(gobj, TRUE);
        open_db(gobj, db, cr_v2(db), FALSE);
        _exit(3);
    }

    int status = 0;
    if(waitpid(pid, &status, 0) < 0) {
        return test_fail(gobj, db, "TEST FAIL: CR, waitpid() failed", json_string(strerror(errno)));
    }
    if(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL) {
        return 1;
    }
    if(WIFEXITED(status) && WEXITSTATUS(status) == 3) {
        return 0;
    }
    return test_fail(gobj, db, "TEST FAIL: CR, the child process ended some other way",
        json_integer(status));
}

PRIVATE int scenario_crash_at_every_write(hgobj gobj, BOOL drafts)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *prefix = drafts? "tw_crd" : "tw_crn";
    int result = 0;

    /*
     *  How many writes the projection makes: an open that nobody kills
     */
    char db[NAME_MAX];
    snprintf(db, sizeof(db), "%s_all", prefix);
    result += cr_prepare(gobj, db, drafts);
    priv->system_writes = 0;
    priv->kill_at_write = 0;
    watch_system_writes(gobj, TRUE);
    if(open_db(gobj, db, cr_v2(db), FALSE) < 0) {
        watch_system_writes(gobj, FALSE);
        return result - 1;
    }
    watch_system_writes(gobj, FALSE);
    int writes = priv->system_writes;
    result += check_withdrawn(gobj, db, "TEST FAIL: CR, the projection nobody killed",
        0, drafts? json_pack("{s:s, s:s}", "users", "unsaved", "groups", "unsaved") : json_object());
    close_db(gobj, db);
    if(writes < 10) {
        return result + test_fail(gobj, db, "TEST FAIL: CR, too few writes seen",
            json_integer(writes));
    }

    for(int k = 1; k <= writes; k++) {
        snprintf(db, sizeof(db), "%s_%d", prefix, k);
        result += cr_prepare(gobj, db, drafts);

        int killed = cr_open_killed(gobj, db, k);
        if(killed != 1) {
            result += test_fail(gobj, db, "TEST FAIL: CR, the child was not killed at its write",
                json_integer(k));
            continue;
        }
        restart_system(gobj);

        json_t *record = unfinished_record(gobj, db);
        if(!json_is_true(json_object_get(record, "in_progress"))) {
            result += test_fail(gobj, db,
                "TEST FAIL: CR, a projection that died left no record of being in progress",
                record? json_incref(record) : json_integer(k));
        }
        JSON_DECREF(record)

        json_t *logs_before = gobj_get_log_data();
        if(open_db(gobj, db, cr_v2(db), FALSE) < 0) {
            JSON_DECREF(logs_before)
            result--;
            continue;
        }
        json_t *logs_after = gobj_get_log_data();
        json_int_t errors = kw_get_int(gobj, logs_after, "error", 0, 0) -
            kw_get_int(gobj, logs_before, "error", 0, 0);
        json_int_t warnings = kw_get_int(gobj, logs_after, "warning", 0, 0) -
            kw_get_int(gobj, logs_before, "warning", 0, 0);
        if(errors != 0 || warnings != (drafts? 1 : 0)) {
            result += test_fail(gobj, db, "TEST FAIL: CR, the open after the crash logged errors or warnings",
                json_pack("{s:i, s:I, s:I}", "k", k, "errors", errors, "warnings", warnings));
        }
        JSON_DECREF(logs_before)
        JSON_DECREF(logs_after)

        result += check_withdrawn(gobj, db,
            "TEST FAIL: CR, the open after the crash invented work, or lost the operator's",
            0, drafts? json_pack("{s:s, s:s}", "users", "unsaved", "groups", "unsaved") : json_object());
        result += check_agree(gobj, db, "TEST FAIL: CR, the open after the crash did not complete");
        record = unfinished_record(gobj, db);
        if(record) {
            result += test_fail(gobj, db, "TEST FAIL: CR, a completed projection left its record",
                json_incref(record));
        }
        JSON_DECREF(record)
        close_db(gobj, db);

        if(open_db(gobj, db, cr_v2(db), FALSE) < 0) {
            result--;
            continue;
        }
        result += check_withdrawn(gobj, db, "TEST FAIL: CR, the work was said twice", 0, json_object());
        close_db(gobj, db);
    }

    printf("CR (%s): a projection of %d writes killed at each one\n",
        drafts? "with drafts" : "no drafts", writes);
    return result;
}

/***************************************************************************
 *  LF: the LINK of a new topic to its treedb fails (every write of the
 *  topic node fails once it is created). The topic node is there, linked
 *  to nothing, and no column is written under it: the projection is
 *  unfinished, and the record takes the topic as the projection's
 *  (`not_written`). After a restart the open that completes it takes the
 *  topic, writes its columns and says nothing: nobody did anything.
 ***************************************************************************/
PRIVATE json_t *users_only_v1(const char *db)
{
    return schema_of(db, 1, json_pack("[o]",
        topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
    ));
}

PRIVATE int scenario_failed_link_of_new_topic(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *db = "tw_lf";
    int result = 0;

    if(open_db(gobj, db, users_only_v1(db), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, db);

    json_t *v3 = users_departments_v3(db, FALSE);
    snprintf(priv->break_writes_of, sizeof(priv->break_writes_of), "%s", "tw_lf.departments");
    priv->broken_fds = 0;
    watch_system_writes(gobj, TRUE);
    int opened = open_db(gobj, db, json_incref(v3), FALSE);
    watch_system_writes(gobj, FALSE);
    priv->break_writes_of[0] = 0;
    if(opened < 0) {
        JSON_DECREF(v3)
        return result - 1;
    }
    if(priv->broken_fds == 0) {
        result += test_fail(gobj, db, "TEST FAIL: LF, the writes of the topic were not broken", NULL);
    }
    json_t *record = unfinished_record(gobj, db);
    json_t *not_written = json_object_get(record, "not_written");
    if(json_array_size(not_written) != 1 ||
            json_list_str_index(not_written, "tw_lf.departments", FALSE) < 0 ||
            json_list_str_index(json_object_get(record, "leftovers"), "tw_lf.departments", FALSE) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: LF, the failed link is not in the record",
            json_incref(record));
    }
    JSON_DECREF(record)
    if(system_has_col(gobj, db, "departments", "name")) {
        result += test_fail(gobj, db,
            "TEST FAIL: LF, a column was written under a topic that no tree reaches", NULL);
    }
    result += check_withdrawn(gobj, db, "TEST FAIL: LF, the failed link reported something",
        0, json_object());
    close_db(gobj, db);

    restart_system(gobj);
    if(open_db(gobj, db, v3, FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db,
        "TEST FAIL: LF, a failed link of the projection was reported as the operator's work",
        0, json_object());
    result += check_agree(gobj, db, "TEST FAIL: LF, the projection was not completed");
    record = unfinished_record(gobj, db);
    if(record) {
        result += test_fail(gobj, db, "TEST FAIL: LF, a completed projection left its record",
            json_incref(record));
    }
    JSON_DECREF(record)
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  FT: the operator unlinks the topic `departments` from its treedb, and
 *  the write that TAKES it for a newer literal fails (its files are
 *  read-only). The topic and its columns stay as they are: nothing is
 *  deleted, nothing is said, and the record keeps the draft's kind. When
 *  the files are writable again, the open that takes the topic says
 *  `departments` once, as "unsaved".
 *
 *  FC: the same when the CREATE of a new topic fails (the directory of
 *  the keys of `topics` is read-only) while the operator's column for it
 *  exists, in no topic: the column stays, and the open that creates the
 *  topic takes it and says `departments` once.
 ***************************************************************************/
PRIVATE int scenario_failed_take_leaves_the_orphans(hgobj gobj)
{
    const char *db = "tw_ft";
    int result = 0;
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    if(gobj_unlink_nodes(sys, "topics",
            "treedbs", json_pack("{s:s}", "id", db),
            "topics", json_pack("{s:s}", "id", "tw_ft.departments"), gobj) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: FT, the operator's unlink was refused", NULL);
    }
    close_db(gobj, db);
    restart_system(gobj);

    result += chmod_system_key(gobj, db, "topics", "tw_ft.departments", 0440);
    if(open_db(gobj, db, users_departments_v3(db, FALSE), FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db, "TEST FAIL: FT, a take that failed reported the topic",
        0, json_object());
    const char *ids[] = {"tw_ft.departments", "tw_ft.departments.id", "tw_ft.departments.name", NULL};
    for(int i = 0; ids[i]; i++) {
        json_t *node = system_node(gobj, i == 0? "topics" : "cols", ids[i]);
        if(!node) {
            result += test_fail(gobj, db, "TEST FAIL: FT, a take that failed deleted a node",
                json_string(ids[i]));
        }
        JSON_DECREF(node)
    }
    json_t *record = unfinished_record(gobj, db);
    json_t *kinds = json_pack("{s:s}", "departments", "unsaved");
    if(!json_equal(json_object_get(record, "draft_kinds"), kinds)) {
        result += test_fail(gobj, db, "TEST FAIL: FT, the record lost the kind of the draft",
            json_incref(record));
    }
    JSON_DECREF(kinds)
    JSON_DECREF(record)
    close_db(gobj, db);
    result += chmod_system_key(gobj, db, "topics", "tw_ft.departments", 0660);
    restart_system(gobj);

    if(open_db(gobj, db, users_departments_v3(db, FALSE), FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db, "TEST FAIL: FT, the take said the draft not once",
        0, json_pack("{s:s}", "departments", "unsaved"));
    result += check_agree(gobj, db, "TEST FAIL: FT, the projection was not completed");
    close_db(gobj, db);

    /*
     *  FC
     */
    db = "tw_fc";
    if(open_db(gobj, db, users_only_v1(db), FALSE) < 0) {
        return result - 1;
    }
    json_t *col = gobj_create_node(sys, "cols",
        json_pack("{s:s, s:s, s:s, s:s, s:i, s:[s]}",
            "id", "tw_fc.departments.name", "value", "name", "header", "Operator name",
            "type", "string", "fillspace", 10, "flag", "persistent"),
        json_pack("{s:b}", "refs", 1), gobj);
    if(!col) {
        result += test_fail(gobj, db, "TEST FAIL: FC, the operator's column was refused", NULL);
    }
    JSON_DECREF(col)
    close_db(gobj, db);
    restart_system(gobj);

    result += chmod_system_key(gobj, db, "topics", NULL, 0550);
    int opened = open_db(gobj, db, users_departments_v3(db, FALSE), FALSE);
    result += chmod_system_key(gobj, db, "topics", NULL, 02770);
    if(opened < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db, "TEST FAIL: FC, a create that failed reported the topic",
        0, json_object());
    json_t *node = system_node(gobj, "cols", "tw_fc.departments.name");
    if(!node) {
        result += test_fail(gobj, db, "TEST FAIL: FC, a create that failed deleted the operator's column",
            NULL);
    }
    JSON_DECREF(node)
    close_db(gobj, db);
    restart_system(gobj);

    if(open_db(gobj, db, users_departments_v3(db, FALSE), FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db, "TEST FAIL: FC, the create said the operator's column not once",
        0, json_pack("{s:s}", "departments", "unsaved"));
    result += check_agree(gobj, db, "TEST FAIL: FC, the projection was not completed");
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  DN: two treedbs whose names start the same: `tw_dn` and `tw_dn.b`. The
 *  nodes of `tw_dn.b` have ids that start with "tw_dn." -- its topic `c`
 *  is "tw_dn.b.c", which reads as a topic "b.c" of `tw_dn`. Which treedb a
 *  node belongs to is read from the nodes, never from that prefix:
 *
 *      - a newer literal of `tw_dn` leaves the nodes of `tw_dn.b` alone,
 *        linked or not (a column of `tw_dn.b` unlinked from its topic is
 *        no orphan of `tw_dn`), and says nothing;
 *      - a column of `tw_dn.b` that the operator linked into `tw_dn` is
 *        only unlinked from it by the literal of `tw_dn`;
 *      - delete-treedb of `tw_dn` deletes no node of `tw_dn.b`.
 ***************************************************************************/
PRIVATE json_t *dn_other(const char *db, int version, const char *header)
{
    return schema_of(db, version, json_pack("[o]",
        topic_of("c", version, json_pack("{s:o, s:o, s:o}",
            "id", col_id(), "d", col_str(header), "e", col_str("E")))
    ));
}

PRIVATE int scenario_dotted_treedb_names(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *db = "tw_dn";
    const char *other = "tw_dn.b";
    int result = 0;
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, db);
    if(open_db(gobj, other, dn_other(other, 1, "D"), FALSE) < 0) {
        return result - 1;
    }
    close_db(gobj, other);

    if(gobj_unlink_nodes(sys, "cols",
            "topics", json_pack("{s:s}", "id", "tw_dn.b.c"),
            "cols", json_pack("{s:s}", "id", "tw_dn.b.c.e"), gobj) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: DN, the operator's unlink was refused", NULL);
    }

    json_t *v3 = schema_of(db, 3, json_pack("[o]",
        topic_of("users", 3, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User v3")))
    ));
    if(open_db(gobj, db, v3, FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db,
        "TEST FAIL: DN, the literal of a treedb reported the nodes of another",
        0, json_object());
    result += check_agree(gobj, db, "TEST FAIL: DN, the projection was not completed");
    if(!system_node_links(gobj, "cols", "tw_dn.b.c.d", "topics", "topics^tw_dn.b.c^cols")) {
        result += test_fail(gobj, db, "TEST FAIL: DN, a column of another treedb was touched", NULL);
    }
    json_t *node = system_node(gobj, "cols", "tw_dn.b.c.e");
    if(!node) {
        result += test_fail(gobj, db,
            "TEST FAIL: DN, an orphan of another treedb was deleted", NULL);
    }
    JSON_DECREF(node)
    close_db(gobj, db);

    /*
     *  A column of the other treedb linked into this one: only unlinked
     */
    if(gobj_link_nodes(sys, "cols",
            "topics", json_pack("{s:s}", "id", "tw_dn.users"),
            "cols", json_pack("{s:s}", "id", "tw_dn.b.c.d"), gobj) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: DN, the operator's link was refused", NULL);
    }
    json_t *v4 = schema_of(db, 4, json_pack("[o]",
        topic_of("users", 4, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User v4")))
    ));
    if(open_db(gobj, db, v4, FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db,
        "TEST FAIL: DN, the link of another treedb's column was not said",
        0, json_pack("{s:s}", "users", "unsaved"));
    result += check_agree(gobj, db, "TEST FAIL: DN, the foreign column was not unlinked");
    if(!system_node_links(gobj, "cols", "tw_dn.b.c.d", "topics", "topics^tw_dn.b.c^cols")) {
        result += test_fail(gobj, db, "TEST FAIL: DN, a foreign column was deleted, not unlinked", NULL);
    }
    close_db(gobj, db);

    /*
     *  delete-treedb of `tw_dn` with the column linked into it again
     */
    if(gobj_link_nodes(sys, "cols",
            "topics", json_pack("{s:s}", "id", "tw_dn.users"),
            "cols", json_pack("{s:s}", "id", "tw_dn.b.c.d"), gobj) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: DN, the operator's second link was refused", NULL);
    }
    json_t *jn_resp = gobj_command(priv->gobj_treedbs, "delete-treedb",
        json_pack("{s:s, s:b}", "treedb_name", db, "force", 1), gobj);
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: DN, delete-treedb failed", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    if(!system_node_links(gobj, "cols", "tw_dn.b.c.d", "topics", "topics^tw_dn.b.c^cols")) {
        result += test_fail(gobj, db,
            "TEST FAIL: DN, delete-treedb deleted a column of another treedb", NULL);
    }
    node = system_node(gobj, "topics", "tw_dn.users");
    if(node) {
        result += test_fail(gobj, db, "TEST FAIL: DN, delete-treedb left its own topic", NULL);
    }
    JSON_DECREF(node)

    /*
     *  The other treedb opens with a newer literal: its orphan is taken
     */
    if(open_db(gobj, other, dn_other(other, 2, "D2"), FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, other,
        "TEST FAIL: DN, the orphan of the other treedb was not taken, or not said",
        0, json_pack("{s:s}", "c", "unsaved"));
    result += check_agree(gobj, other, "TEST FAIL: DN, the other treedb was not completed");
    close_db(gobj, other);
    return result;
}

/***************************************************************************
 *  MV: the operator MOVES the column `departments.name` to `users`
 *  (unlinks it from one, links it to the other). A newer literal declares
 *  it where it was: the node is taken where the literal declares it, in
 *  ONE open (it is not created, "Node already exists"), and the move is
 *  said once, as a draft of both topics.
 *
 *  NP: the operator links `departments.name` to `users` TOO. A newer
 *  literal unlinks it from `users` -- the node is not deleted: the literal
 *  declares it in `departments` -- in one open, and says `users`.
 ***************************************************************************/
PRIVATE int scenario_column_moved_by_the_operator(hgobj gobj)
{
    int result = 0;
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);
    const char *dbs[] = {"tw_mv", "tw_np", NULL};

    for(int i = 0; dbs[i]; i++) {
        const char *db = dbs[i];
        char dep_id[NAME_MAX];
        snprintf(dep_id, sizeof(dep_id), "%s.departments", db);
        char users_id[NAME_MAX];
        snprintf(users_id, sizeof(users_id), "%s.users", db);
        char col_id_[NAME_MAX];
        snprintf(col_id_, sizeof(col_id_), "%s.departments.name", db);

        if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
            return result - 1;
        }
        if(i == 0 && gobj_unlink_nodes(sys, "cols",
                "topics", json_pack("{s:s}", "id", dep_id),
                "cols", json_pack("{s:s}", "id", col_id_), gobj) < 0) {
            result += test_fail(gobj, db, "TEST FAIL: MV, the operator's unlink was refused", NULL);
        }
        if(gobj_link_nodes(sys, "cols",
                "topics", json_pack("{s:s}", "id", users_id),
                "cols", json_pack("{s:s}", "id", col_id_), gobj) < 0) {
            result += test_fail(gobj, db, "TEST FAIL: MV, the operator's link was refused", NULL);
        }
        close_db(gobj, db);

        if(open_db(gobj, db, users_departments_v3(db, FALSE), FALSE) < 0) {
            return result - 1;
        }
        json_t *record = unfinished_record(gobj, db);
        if(record) {
            result += test_fail(gobj, db, "TEST FAIL: MV, the projection did not complete in one open",
                json_incref(record));
        }
        JSON_DECREF(record)
        result += check_withdrawn(gobj, db, "TEST FAIL: MV, the operator's move was not said once",
            0, i == 0?
                json_pack("{s:s, s:s}", "departments", "unsaved", "users", "unsaved") :
                json_pack("{s:s}", "users", "unsaved"));
        result += check_agree(gobj, db, "TEST FAIL: MV, the projection was not completed");
        char ref[NAME_MAX + 16];
        snprintf(ref, sizeof(ref), "topics^%s^cols", dep_id);
        if(!system_node_links(gobj, "cols", col_id_, "topics", ref)) {
            result += test_fail(gobj, db, "TEST FAIL: MV, the column is not where the literal declares it",
                NULL);
        }
        snprintf(ref, sizeof(ref), "topics^%s^cols", users_id);
        if(system_node_links(gobj, "cols", col_id_, "topics", ref)) {
            result += test_fail(gobj, db, "TEST FAIL: MV, the column is still where the operator put it",
                NULL);
        }
        close_db(gobj, db);

        if(open_db(gobj, db, users_departments_v3(db, FALSE), FALSE) < 0) {
            return result - 1;
        }
        result += check_withdrawn(gobj, db, "TEST FAIL: MV, the move was said twice", 0, json_object());
        close_db(gobj, db);
    }
    return result;
}

/***************************************************************************
 *  The errors and warnings logged since the counting handler was added
 *  (see run_tests), to count what one open says with the capture off.
 *  A watcher of the disk that cannot be created is not counted: a step of
 *  the test runs inside ONE callback of the event loop, so the watchers
 *  of the treedbs it closes are freed only when the loop runs again, and
 *  the inotify instances of the user run out (every test process of the
 *  machine shares them). That is the test, not the code under test.
 ***************************************************************************/
PRIVATE json_int_t counted_errors = 0;
PRIVATE json_int_t counted_warnings = 0;

PRIVATE int counting_log_write(void *h, int priority, const char *bf, size_t len)
{
    if(strstr(bf, "\"inotify_init1() FAILED\"") ||
            strstr(bf, "\"fs_create_watcher_event FAILED\"")) {
        return 0;
    }
    if(priority <= LOG_ERR) {
        counted_errors++;
    } else if(priority == LOG_WARNING) {
        counted_warnings++;
    }
    return 0;
}

PRIVATE json_int_t log_count(hgobj gobj, const char *level)
{
    return strcmp(level, "error")==0? counted_errors : counted_warnings;
}

/***************************************************************************
 *  A node of __system__ that must be gone
 ***************************************************************************/
PRIVATE int check_absent(hgobj gobj, const char *treedb_name, const char *label,
    const char *system_topic, const char *id)
{
    json_t *node = system_node(gobj, system_topic, id);
    int result = 0;
    if(node) {
        result = test_fail(gobj, treedb_name, label, json_pack("{s:s, s:O}", "id", id, "node", node));
    }
    JSON_DECREF(node)
    return result;
}

/***************************************************************************
 *  What one open of a treedb logs, as {"errors": n, "warnings": n}, with
 *  what it answers: compared with what is expected
 ***************************************************************************/
PRIVATE int open_counting_as(hgobj gobj, const char *treedb_name, json_t *jn_schema, // owned
    BOOL forced, const char *label, int errors, int warnings)
{
    json_int_t e0 = log_count(gobj, "error");
    json_int_t w0 = log_count(gobj, "warning");
    json_t *jn_resp = open_db_resp_as(gobj, treedb_name, jn_schema, forced);
    json_int_t e = log_count(gobj, "error") - e0;
    json_int_t w = log_count(gobj, "warning") - w0;
    int result = 0;
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 || e != errors || w != warnings) {
        result = test_fail(gobj, treedb_name, label, json_pack("{s:O, s:I, s:i, s:I, s:i}",
            "answer", jn_resp,
            "errors", e, "expected_errors", errors,
            "warnings", w, "expected_warnings", warnings
        ));
    }
    JSON_DECREF(jn_resp)
    return result;
}

PRIVATE int open_counting(hgobj gobj, const char *treedb_name, json_t *jn_schema, // owned
    const char *label, int errors, int warnings)
{
    return open_counting_as(gobj, treedb_name, jn_schema, FALSE, label, errors, warnings);
}

/***************************************************************************
 *  LM: a snapshot holds `departments`, which the literal removes, so it is
 *  a leftover. The operator MOVES its column `name` to `users` (unlinks it
 *  from `departments`, links it to `users`): the place of a leftover is
 *  part of "as left", so the move is a draft of both topics. The open that
 *  completes the projection deletes the column and says both.
 ***************************************************************************/
PRIVATE int scenario_leftover_moved(hgobj gobj)
{
    const char *db = "tw_lm";
    int result = 0;
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    result += shoot_system_snap(gobj, db, db);
    close_db(gobj, db);
    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    result += check_draft_changed(gobj, db, "TEST FAIL: LM, the leftover reads as a draft",
        json_object());
    if(gobj_unlink_nodes(sys, "cols", "topics", json_pack("{s:s}", "id", "tw_lm.departments"),
            "cols", json_pack("{s:s}", "id", "tw_lm.departments.name"), gobj) < 0 ||
        gobj_link_nodes(sys, "cols", "topics", json_pack("{s:s}", "id", "tw_lm.users"),
            "cols", json_pack("{s:s}", "id", "tw_lm.departments.name"), gobj) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: LM, the operator's move was refused", NULL);
    }
    result += check_draft_changed(gobj, db, "TEST FAIL: LM, the move of a leftover is not a draft",
        json_pack("{s:b, s:b}", "departments", 1, "users", 1));
    close_db(gobj, db);

    result += delete_system_snap(gobj, db, db);
    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db, "TEST FAIL: LM, the move of a leftover was withdrawn in silence",
        0, json_pack("{s:s, s:s}", "departments", "unsaved", "users", "unsaved"));
    result += check_agree(gobj, db, "TEST FAIL: LM, the projection was not completed");
    result += check_absent(gobj, db, "TEST FAIL: LM, the moved leftover stayed",
        "cols", "tw_lm.departments.name");
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  OE: a process dies between the delete of `departments` and the delete
 *  of its columns, so `departments.name` is left in NO topic, and the
 *  record of the projection in progress plans its delete. The operator
 *  edits that column before the next open: its header (`variant` 0), or
 *  a link to `users` (1). A node in no topic is still there, and it is
 *  compared: the edit is a draft, and the open that completes the
 *  projection deletes the column and says it.
 ***************************************************************************/
PRIVATE int scenario_orphaned_leftover_edited(hgobj gobj, int variant)
{
    int result = 0;
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    for(int k = 1; k <= 40; k++) {
        char db[64];
        snprintf(db, sizeof(db), "tw_oe%d_%d", variant, k);
        if(cr_prepare(gobj, db, FALSE) < 0) {
            return result - 1;
        }
        int killed = cr_open_killed(gobj, db, k);
        if(killed < 0) {
            return result - 1;
        }
        if(killed == 0) {
            break;
        }
        restart_system(gobj);

        char dep_id[NAME_MAX], col[NAME_MAX], users_id[NAME_MAX];
        snprintf(dep_id, sizeof(dep_id), "%s.departments", db);
        snprintf(col, sizeof(col), "%s.departments.name", db);
        snprintf(users_id, sizeof(users_id), "%s.users", db);
        json_t *t = system_node(gobj, "topics", dep_id);
        json_t *c = system_node(gobj, "cols", col);
        BOOL hit = (!t && c)? TRUE : FALSE;
        JSON_DECREF(t)
        JSON_DECREF(c)
        if(!hit) {
            continue;
        }

        if(variant == 0) {
            json_t *ed = gobj_update_node(sys, "cols",
                json_pack("{s:s, s:s}", "id", col, "header", "Operator header"),
                json_pack("{s:b}", "refs", 1), gobj);
            if(!ed) {
                result += test_fail(gobj, db, "TEST FAIL: OE, the operator's edit was refused", NULL);
            }
            JSON_DECREF(ed)
        } else if(gobj_link_nodes(sys, "cols", "topics", json_pack("{s:s}", "id", users_id),
                "cols", json_pack("{s:s}", "id", col), gobj) < 0) {
            result += test_fail(gobj, db, "TEST FAIL: OE, the operator's link was refused", NULL);
        }

        if(open_db(gobj, db, cr_v2(db), FALSE) < 0) {
            return result - 1;
        }
        result += check_withdrawn(gobj, db,
            "TEST FAIL: OE, the edit of a leftover in no topic was deleted in silence", 0,
            variant == 0?
                json_pack("{s:s}", "departments", "unsaved") :
                json_pack("{s:s, s:s}", "departments", "unsaved", "users", "unsaved"));
        result += check_agree(gobj, db, "TEST FAIL: OE, the projection was not completed");
        result += check_absent(gobj, db, "TEST FAIL: OE, the edited leftover stayed", "cols", col);
        close_db(gobj, db);
        return result;
    }
    return result + test_fail(gobj, "tw_oe",
        "TEST FAIL: OE, no write leaves departments deleted and its column in no topic", NULL);
}

/***************************************************************************
 *  AMB: treedbs `tw_am` and `tw_am.b`. The operator deletes the topic node
 *  `tw_am.b.departments` with force: its columns are left in no topic,
 *  and `tw_am.b.departments.name` could be the column `name` of the topic
 *  `b.departments` of `tw_am`, or of the topic `departments` of `tw_am.b`.
 *  The owner is read from what each says: the schema file of `tw_am.b`
 *  declares `departments.name`. An unrelated treedb and `tw_am` say
 *  nothing; the newer literal of `tw_am.b` takes the columns (ONE WARNING
 *  each, naming it) and deletes them, and says the operator's delete.
 ***************************************************************************/
PRIVATE json_t *users_only(const char *db, int v)
{
    return schema_of(db, v, json_pack("[o]",
        topic_of("users", v, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))));
}

PRIVATE int scenario_ambiguous_owner(hgobj gobj)
{
    int result = 0;
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    if(open_db(gobj, "tw_am", users_departments_v1("tw_am"), FALSE) < 0 ||
            open_db(gobj, "tw_am.b", users_departments_v1("tw_am.b"), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, "tw_am");
    close_db(gobj, "tw_am.b");
    if(gobj_delete_node(sys, "topics", json_pack("{s:s}", "id", "tw_am.b.departments"),
            json_pack("{s:b}", "force", 1), gobj) < 0) {
        result += test_fail(gobj, "tw_am.b", "TEST FAIL: AMB, the operator's delete was refused", NULL);
    }

    result += open_counting(gobj, "tw_zz", users_departments_v1("tw_zz"),
        "TEST FAIL: AMB, an unrelated treedb said the nodes of others", 0, 0);
    close_db(gobj, "tw_zz");
    result += open_counting(gobj, "tw_zz", users_only("tw_zz", 2),
        "TEST FAIL: AMB, an unrelated treedb said the nodes of others", 0, 0);
    close_db(gobj, "tw_zz");
    result += open_counting(gobj, "tw_am", users_only("tw_am", 2),
        "TEST FAIL: AMB, a treedb took the nodes that another one's schema declares", 0, 0);
    result += check_withdrawn(gobj, "tw_am", "TEST FAIL: AMB, tw_am said nodes of tw_am.b",
        0, json_object());
    close_db(gobj, "tw_am");
    json_t *orphan = system_node(gobj, "cols", "tw_am.b.departments.name");
    if(!orphan) {
        result += test_fail(gobj, "tw_am", "TEST FAIL: AMB, tw_am deleted a node of tw_am.b", NULL);
    }
    JSON_DECREF(orphan)

    result += open_counting(gobj, "tw_am.b", users_only("tw_am.b", 2),
        "TEST FAIL: AMB, the owner did not say once each node it took", 0, 3);
    result += check_withdrawn(gobj, "tw_am.b", "TEST FAIL: AMB, the operator's delete was not said",
        0, json_pack("{s:s}", "departments", "unsaved"));
    result += check_agree(gobj, "tw_am.b", "TEST FAIL: AMB, the projection was not completed");
    close_db(gobj, "tw_am.b");
    result += check_absent(gobj, "tw_am.b", "TEST FAIL: AMB, a node was left to none",
        "cols", "tw_am.b.departments.name");
    result += check_absent(gobj, "tw_am.b", "TEST FAIL: AMB, a node was left to none",
        "cols", "tw_am.b.departments.id");
    result += open_counting(gobj, "tw_am.b", users_only("tw_am.b", 3),
        "TEST FAIL: AMB, a later open said something", 0, 0);
    close_db(gobj, "tw_am.b");
    return result;
}

/***************************************************************************
 *  AF: AMB, when the treedb whose name could own the columns has the
 *  record of a projection that FAILED: what a failed move of ids, a
 *  failed seed, or a lost or unreadable record leaves (`not_written`
 *  names the treedb, nothing is `planned`, see new_unfinished). Such a
 *  record names no id, so the owner is read from the schemas: `tw_af`
 *  leaves the columns of `tw_af.b`, whether the record is the one its own
 *  open read or the file of another treedb, and `tw_af.b` takes them.
 *
 *  Red before: a record with no `planned` "named" every id, with an ERROR
 *  and a stack each time it was asked: `tw_af` took and deleted a column
 *  that the schema file of `tw_af.b` declares, and `tw_af.b` left its own
 *  columns to `tw_af`.
 ***************************************************************************/
PRIVATE int write_failure_record(hgobj gobj, const char *treedb_name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char path[PATH_MAX];
    char fn[NAME_MAX];
    snprintf(fn, sizeof(fn), "%s.unfinished.json", treedb_name);
    build_path(path, sizeof(path), priv->path_database, "__system__", "saved_schemas", fn, NULL);
    json_t *record = json_pack("{s:i, s:[], s:[s], s:[], s:{}, s:{}}",
        "schema_version", 0,
        "not_removed",
        "not_written", treedb_name,
        "leftovers",
        "draft_kinds",
        "replaced_kinds"
    );
    int ret = json_dump_file(record, path, JSON_INDENT(4));
    JSON_DECREF(record)
    if(ret < 0) {
        return test_fail(gobj, treedb_name, "TEST FAIL: AF, the record cannot be written",
            json_string(path));
    }
    return 0;
}

PRIVATE int scenario_ambiguous_with_failure_record(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    if(open_db(gobj, "tw_af", users_departments_v1("tw_af"), FALSE) < 0 ||
            open_db(gobj, "tw_af.b", users_departments_v1("tw_af.b"), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, "tw_af");
    close_db(gobj, "tw_af.b");
    if(gobj_delete_node(sys, "topics", json_pack("{s:s}", "id", "tw_af.b.departments"),
            json_pack("{s:b}", "force", 1), gobj) < 0) {
        result += test_fail(gobj, "tw_af.b", "TEST FAIL: AF, the operator's delete was refused", NULL);
    }

    /*
     *  The record the open of `tw_af` reads
     */
    result += write_failure_record(gobj, "tw_af");
    result += open_counting(gobj, "tw_af", users_only("tw_af", 2),
        "TEST FAIL: AF, a failure record made its treedb take the nodes of another one", 0, 0);
    close_db(gobj, "tw_af");
    json_t *orphan = system_node(gobj, "cols", "tw_af.b.departments.name");
    if(!orphan) {
        result += test_fail(gobj, "tw_af", "TEST FAIL: AF, tw_af deleted a node of tw_af.b", NULL);
    }
    JSON_DECREF(orphan)

    /*
     *  The record of ANOTHER treedb, read from its file
     */
    result += write_failure_record(gobj, "tw_af");
    result += open_counting(gobj, "tw_af.b", users_only("tw_af.b", 2),
        "TEST FAIL: AF, the owner did not take its nodes, or said more", 0, 3);
    result += check_withdrawn(gobj, "tw_af.b", "TEST FAIL: AF, the operator's delete was not said",
        0, json_pack("{s:s}", "departments", "unsaved"));
    close_db(gobj, "tw_af.b");
    result += check_absent(gobj, "tw_af.b", "TEST FAIL: AF, a node was left to none",
        "cols", "tw_af.b.departments.name");
    result += check_absent(gobj, "tw_af.b", "TEST FAIL: AF, a node was left to none",
        "cols", "tw_af.b.departments.id");

    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), priv->path_database, "__system__", "saved_schemas", NULL);
    file_remove(dir, "tw_af.unfinished.json");
    return result;
}

/***************************************************************************
 *  AC: the same ambiguity left by a CRASH: treedbs `tw_cx` and `tw_cx.bK`,
 *  and the projection of `tw_cx.bK` dies between the delete of
 *  `departments` and the delete of its columns. Its record plans them:
 *  that settles the owner. The next open completes the projection, deletes
 *  the columns, says nothing of them as the operator's work, and warns
 *  once per column that it took it; the one after says nothing.
 ***************************************************************************/
PRIVATE int scenario_ambiguous_after_crash(hgobj gobj)
{
    int result = 0;
    if(open_db(gobj, "tw_cx", users_departments_v1("tw_cx"), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, "tw_cx");
    for(int k = 1; k <= 40; k++) {
        char db[64];
        snprintf(db, sizeof(db), "tw_cx.b%d", k);
        if(cr_prepare(gobj, db, FALSE) < 0) {
            return result - 1;
        }
        int killed = cr_open_killed(gobj, db, k);
        if(killed < 0) {
            return result - 1;
        }
        if(killed == 0) {
            break;
        }
        restart_system(gobj);
        char dep_id[NAME_MAX], col[NAME_MAX];
        snprintf(dep_id, sizeof(dep_id), "%s.departments", db);
        snprintf(col, sizeof(col), "%s.departments.name", db);
        json_t *t = system_node(gobj, "topics", dep_id);
        json_t *c = system_node(gobj, "cols", col);
        BOOL hit = (!t && c)? TRUE : FALSE;
        JSON_DECREF(t)
        JSON_DECREF(c)
        if(!hit) {
            continue;
        }
        result += open_counting(gobj, db, cr_v2(db),
            "TEST FAIL: AC, the completing open did not take the columns, or said more", 0, 2);
        result += check_withdrawn(gobj, db, "TEST FAIL: AC, the projection's own nodes were said",
            0, json_object());
        result += check_agree(gobj, db, "TEST FAIL: AC, the projection was not completed");
        close_db(gobj, db);
        result += check_absent(gobj, db, "TEST FAIL: AC, a column was left for ever", "cols", col);
        result += open_counting(gobj, db, cr_v2(db),
            "TEST FAIL: AC, a later open said something", 0, 0);
        close_db(gobj, db);
        return result;
    }
    return result + test_fail(gobj, "tw_cx",
        "TEST FAIL: AC, no write leaves departments deleted and its columns in no topic", NULL);
}

/***************************************************************************
 *  DTM: delete-treedb deletes EVERY node of its treedb: a column the
 *  operator moved to another of its topics, and a column the operator
 *  left in no topic. Before, a node in no topic was not seen, and it
 *  stayed.
 ***************************************************************************/
PRIVATE int scenario_delete_treedb_every_node(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *db = "tw_dtm";
    int result = 0;
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o,o]",
            topic_of("users", 1, json_pack("{s:o, s:o, s:o}",
                "id", col_id(), "username", col_str("User"), "email", col_str("Email"))),
            topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
        )), FALSE) < 0) {
        return -1;
    }
    if(gobj_unlink_nodes(sys, "cols", "topics", json_pack("{s:s}", "id", "tw_dtm.departments"),
            "cols", json_pack("{s:s}", "id", "tw_dtm.departments.name"), gobj) < 0 ||
        gobj_link_nodes(sys, "cols", "topics", json_pack("{s:s}", "id", "tw_dtm.users"),
            "cols", json_pack("{s:s}", "id", "tw_dtm.departments.name"), gobj) < 0 ||
        gobj_unlink_nodes(sys, "cols", "topics", json_pack("{s:s}", "id", "tw_dtm.users"),
            "cols", json_pack("{s:s}", "id", "tw_dtm.users.email"), gobj) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: DTM, the operator's move or unlink was refused", NULL);
    }
    close_db(gobj, db);

    json_t *jn_resp = gobj_command(priv->gobj_treedbs, "delete-treedb",
        json_pack("{s:s, s:b}", "treedb_name", db, "force", 1), gobj);
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: DTM, delete-treedb failed", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)

    static const char *nodes[][2] = {
        {"treedbs", "tw_dtm"},
        {"topics", "tw_dtm.users"},
        {"topics", "tw_dtm.departments"},
        {"cols", "tw_dtm.users.id"},
        {"cols", "tw_dtm.users.username"},
        {"cols", "tw_dtm.users.email"},
        {"cols", "tw_dtm.departments.id"},
        {"cols", "tw_dtm.departments.name"},
        {NULL, NULL}
    };
    for(int i = 0; nodes[i][0]; i++) {
        result += check_absent(gobj, db, "TEST FAIL: DTM, delete-treedb left a node of its treedb",
            nodes[i][0], nodes[i][1]);
    }
    return result;
}

/***************************************************************************
 *  DTK: delete-treedb DIES at each of its writes of __system__, and is
 *  run again. The second run finishes the delete: it answers 0, and no
 *  node of the treedb is left -- also when the node of the treedb itself
 *  is gone and what is left is in no tree (a delete of 7.25.4 cut after
 *  its first write). Before, the node of the treedb went FIRST, and a
 *  delete cut after it answered -1 ("not projected in __system__") at
 *  every run, with the topics and columns still there.
 ***************************************************************************/
PRIVATE json_t *dtk_literal(const char *db)
{
    return schema_of(db, 1, json_pack("[o,o]",
        topic_of("users", 1, json_pack("{s:o, s:o, s:o}",
            "id", col_id(), "username", col_str("User"), "email", col_str("Email"))),
        topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
    ));
}

/*
 *  delete-treedb in a CHILD killed at the `kill_at`-th write of
 *  __system__. 1 when it was killed, 0 when it completed, -1 on error
 */
PRIVATE int delete_treedb_killed(hgobj gobj, const char *db, int kill_at)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if(pid < 0) {
        return test_fail(gobj, db, "TEST FAIL: DTK, fork() failed", json_string(strerror(errno)));
    }
    if(pid == 0) {
        child_takes_its_own_ring();
        priv->system_writes = 0;
        priv->kill_at_write = kill_at;
        watch_system_writes(gobj, TRUE);
        json_t *jn_resp = treedb_cmd(gobj, db, "delete-treedb", json_pack("{s:b}", "force", 1));
        JSON_DECREF(jn_resp)
        _exit(3);
    }
    int status = 0;
    if(waitpid(pid, &status, 0) < 0) {
        return test_fail(gobj, db, "TEST FAIL: DTK, waitpid() failed", json_string(strerror(errno)));
    }
    if(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL) {
        return 1;
    }
    if(WIFEXITED(status) && WEXITSTATUS(status) == 3) {
        return 0;
    }
    return test_fail(gobj, db, "TEST FAIL: DTK, the child process ended some other way",
        json_integer(status));
}

PRIVATE int scenario_delete_treedb_killed(hgobj gobj)
{
    int result = 0;
    int kills = 0;
    for(int k = 0; k <= 40; k++) {
        char db[64];
        snprintf(db, sizeof(db), "tw_dtk%d", k);
        if(open_db(gobj, db, dtk_literal(db), FALSE) < 0) {
            return result - 1;
        }
        close_db(gobj, db);

        /*
         *  k 0: a delete of 7.25.4 cut after its first write, which was
         *  the node of the treedb (emulated: that node deleted by hand)
         */
        int killed = 1;
        if(k == 0) {
            if(gobj_delete_node(gobj_find_service(SYSTEM_TREEDB, FALSE), "treedbs",
                    json_pack("{s:s}", "id", db), json_pack("{s:b}", "force", 1), gobj) < 0) {
                result += test_fail(gobj, db, "TEST FAIL: DTK, the node of the treedb could not be deleted", NULL);
            }
        } else {
            killed = delete_treedb_killed(gobj, db, k);
        }
        if(killed < 0) {
            return result - 1;
        }
        restart_system(gobj);

        json_int_t e0 = log_count(gobj, "error");
        json_t *jn_resp = treedb_cmd(gobj, db, "delete-treedb", json_pack("{s:b}", "force", 1));
        if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 || log_count(gobj, "error") - e0 != 0) {
            result += test_fail(gobj, db,
                "TEST FAIL: DTK, the delete-treedb run after a cut one did not finish it, or logged an error",
                json_pack("{s:i, s:O}", "k", k, "answer", jn_resp));
        }
        JSON_DECREF(jn_resp)

        static const char *nodes[][2] = {
            {"treedbs", ""},
            {"topics", ".users"},
            {"topics", ".departments"},
            {"cols", ".users.id"},
            {"cols", ".users.username"},
            {"cols", ".users.email"},
            {"cols", ".departments.id"},
            {"cols", ".departments.name"},
            {NULL, NULL}
        };
        for(int i = 0; nodes[i][0]; i++) {
            char id[NAME_MAX];
            snprintf(id, sizeof(id), "%s%s", db, nodes[i][1]);
            result += check_absent(gobj, db, "TEST FAIL: DTK, a node of the treedb is left",
                nodes[i][0], id);
        }
        json_t *record = unfinished_record(gobj, db);
        if(record) {
            result += test_fail(gobj, db, "TEST FAIL: DTK, a record of the treedb is left",
                json_incref(record));
        }
        JSON_DECREF(record)

        if(killed == 0) {
            break;
        }
        kills++;
    }
    if(kills < 5) {
        result += test_fail(gobj, "tw_dtk", "TEST FAIL: DTK, the delete was killed at too few writes",
            json_integer(kills));
    }
    return result;
}

/***************************************************************************
 *  RU: the record of an unfinished projection CANNOT BE WRITTEN
 *  (saved_schemas/ read-only) while a snapshot refuses a delete. The
 *  projection does not read as complete for that: the node of the treedb
 *  says it (`c_schema_version` -1), the record is kept in memory, the
 *  leftover is not a draft, `save-schema` refuses, and every open writes
 *  the record again. The open that completes the projection says nothing.
 *
 *  RL: the same, and the process that kept the record in memory is gone
 *  (a child that dies). The node still says unfinished: the record is
 *  LOST, a WARNING says it, `save-schema` refuses, and what __system__
 *  holds over the file is taken for drafts, reported as withdrawn (never
 *  deleted in silence) by the open that completes the projection.
 ***************************************************************************/
PRIVATE int check_unfinished(hgobj gobj, const char *treedb_name, const char *label,
    json_t *expected) // owned
{
    json_t *jn_resp = treedb_cmd(gobj, treedb_name, "saved-schema", json_object());
    json_t *ids = kw_get_list(gobj, jn_resp, "data`unfinished_projection", 0, 0);
    int result = 0;
    if(!ids || !json_equal(ids, expected)) {
        result = test_fail(gobj, treedb_name, label, json_pack("{s:O, s:O}",
            "expected", expected, "saved_schema", jn_resp));
    }
    JSON_DECREF(jn_resp)
    JSON_DECREF(expected)
    return result;
}

PRIVATE int check_save_refused(hgobj gobj, const char *treedb_name, const char *label)
{
    json_t *jn_resp = treedb_cmd(gobj, treedb_name, "save-schema", json_object());
    int result = 0;
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0) {
        result = test_fail(gobj, treedb_name, label, json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    return result;
}

PRIVATE int scenario_record_unwritable(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *db = "tw_ru";
    int result = 0;
    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), priv->path_database, "__system__", "saved_schemas", NULL);

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    result += shoot_system_snap(gobj, db, db);
    close_db(gobj, db);

    mkrdir(dir, 02770);
    chmod(dir, 0550);
    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        chmod(dir, 02770);
        return result - 1;
    }
    json_t *record = unfinished_record(gobj, db);
    if(record) {
        result += test_fail(gobj, db, "TEST FAIL: RU, a record was written in a read-only directory",
            json_incref(record));
    }
    JSON_DECREF(record)
    if(system_c_schema_version(gobj, db) != -1) {
        result += test_fail(gobj, db, "TEST FAIL: RU, the node does not say the projection is unfinished",
            json_integer(system_c_schema_version(gobj, db)));
    }
    result += check_unfinished(gobj, db, "TEST FAIL: RU, the projection reads as complete",
        json_pack("[s]", "tw_ru.departments"));
    result += check_draft_changed(gobj, db, "TEST FAIL: RU, the leftover reads as a draft",
        json_object());
    result += check_save_refused(gobj, db, "TEST FAIL: RU, save-schema published a leftover");
    close_db(gobj, db);

    chmod(dir, 02770);
    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    record = unfinished_record(gobj, db);
    if(!json_is_object(record)) {
        result += test_fail(gobj, db, "TEST FAIL: RU, the record was not written again", NULL);
    }
    JSON_DECREF(record)
    result += check_draft_changed(gobj, db, "TEST FAIL: RU, the leftover reads as a draft (2)",
        json_object());
    result += check_save_refused(gobj, db, "TEST FAIL: RU, save-schema published a leftover (2)");
    close_db(gobj, db);

    result += delete_system_snap(gobj, db, db);
    if(open_db(gobj, db, users_only(db, 3), FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db, "TEST FAIL: RU, the leftover was said as the operator's work",
        0, json_object());
    result += check_agree(gobj, db, "TEST FAIL: RU, the projection was not completed");
    if(system_c_schema_version(gobj, db) != 3) {
        result += test_fail(gobj, db, "TEST FAIL: RU, the completed projection is not stamped",
            json_integer(system_c_schema_version(gobj, db)));
    }
    close_db(gobj, db);
    return result;
}

PRIVATE int scenario_record_lost(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *db = "tw_rl";
    int result = 0;
    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), priv->path_database, "__system__", "saved_schemas", NULL);

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    result += shoot_system_snap(gobj, db, db);
    close_db(gobj, db);

    mkrdir(dir, 02770);
    chmod(dir, 0550);
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if(pid < 0) {
        chmod(dir, 02770);
        return test_fail(gobj, db, "TEST FAIL: RL, fork() failed", json_string(strerror(errno)));
    }
    if(pid == 0) {
        child_takes_its_own_ring();
        open_db(gobj, db, users_only_v2(db), FALSE);
        _exit(3);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    chmod(dir, 02770);
    restart_system(gobj);

    /*
     *  The node says unfinished and there is no record: the WARNING of the
     *  lost record, the retry that the snapshot still refuses (its ERROR
     *  and its WARNING), and the record written this time
     */
    result += open_counting(gobj, db, users_only_v2(db),
        "TEST FAIL: RL, a lost record read as a complete projection", 1, 2);
    result += check_unfinished(gobj, db, "TEST FAIL: RL, a lost record reads as complete",
        json_pack("[s]", "tw_rl.departments"));
    result += check_save_refused(gobj, db, "TEST FAIL: RL, save-schema published over a lost record");
    close_db(gobj, db);

    result += delete_system_snap(gobj, db, db);
    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db,
        "TEST FAIL: RL, what a lost record left was deleted in silence",
        0, json_pack("{s:s}", "departments", "unsaved"));
    result += check_agree(gobj, db, "TEST FAIL: RL, the projection was not completed");
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  DD: names with a dot give two elements ONE qualified id in __system__.
 *  (a) one treedb: the column `x.y` of the topic `u` and the column `y` of
 *      the topic `u.x` are both `tw_dd.u.x.y`;
 *  (b) two treedbs: the topic `b.c` of `tw_dx` and the topic `c` of
 *      `tw_dx.b` are both `tw_dx.b.c`.
 *  Such a schema is refused, loudly, naming both: the treedb does not
 *  open, and nothing is written in __system__. A draft that collides is
 *  not saved, and a saved schema that collides is not applied. Before, the
 *  two were one node with no error ((a)), or the second treedb opened and
 *  logged "Node already exists" for its columns ((b)).
 ***************************************************************************/
PRIVATE int open_refused(hgobj gobj, const char *treedb_name, json_t *jn_schema, // owned
    const char *label)
{
    json_int_t e0 = log_count(gobj, "error");
    json_t *jn_resp = open_db_resp(gobj, treedb_name, jn_schema);
    int result = 0;
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0 || log_count(gobj, "error") - e0 != 1) {
        result = test_fail(gobj, treedb_name, label, json_pack("{s:O, s:I}",
            "answer", jn_resp, "errors", log_count(gobj, "error") - e0));
    }
    JSON_DECREF(jn_resp)
    close_db(gobj, treedb_name);
    return result;
}

PRIVATE int scenario_colliding_ids(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    result += open_refused(gobj, "tw_dd", schema_of("tw_dd", 1, json_pack("[o,o]",
            topic_of("u", 1, json_pack("{s:o, s:o}", "id", col_id(), "x.y", col_str("XY"))),
            topic_of("u.x", 1, json_pack("{s:o, s:o}", "id", col_id(), "y", col_str("Y")))
        )), "TEST FAIL: DD, a schema whose columns collide was not refused");
    result += check_absent(gobj, "tw_dd", "TEST FAIL: DD, a refused schema was projected",
        "treedbs", "tw_dd");
    result += check_absent(gobj, "tw_dd", "TEST FAIL: DD, a refused schema was projected",
        "cols", "tw_dd.u.x.y");

    if(open_db(gobj, "tw_dx", schema_of("tw_dx", 1, json_pack("[o]",
            topic_of("b.c", 1, json_pack("{s:o, s:o}", "id", col_id(), "n", col_str("N"))))), FALSE) < 0) {
        return result - 1;
    }
    close_db(gobj, "tw_dx");
    for(int i = 0; i < 2; i++) {
        result += open_refused(gobj, "tw_dx.b", schema_of("tw_dx.b", 1, json_pack("[o]",
                topic_of("c", 1, json_pack("{s:o, s:o}", "id", col_id(), "n", col_str("N b"))))),
            "TEST FAIL: DD, a topic colliding with another treedb's was not refused");
    }
    if(!system_node_links(gobj, "topics", "tw_dx.b.c", "treedbs", "treedbs^tw_dx^topics")) {
        result += test_fail(gobj, "tw_dx", "TEST FAIL: DD, the node of tw_dx was touched", NULL);
    }
    result += check_absent(gobj, "tw_dx.b", "TEST FAIL: DD, a refused schema was projected",
        "treedbs", "tw_dx.b");

    /*
     *  A draft that collides: the operator adds the topic `u.x` next to
     *  the column `x` of `u` (both `tw_ds.u.x`)
     */
    const char *db = "tw_ds";
    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o]",
            topic_of("u", 1, json_pack("{s:o, s:o}", "id", col_id(), "x", col_str("X"))))), FALSE) < 0) {
        return result - 1;
    }
    json_t *topic = gobj_create_node(sys, "topics",
        json_pack("{s:s, s:s, s:s, s:s, s:i}",
            "id", "tw_ds.u.x", "value", "u.x", "pkey", "id", "system_flag", "sf_string_key",
            "topic_version", 1),
        json_pack("{s:b}", "refs", 1), gobj);
    if(!topic || gobj_link_nodes(sys, "topics", "treedbs", json_pack("{s:s}", "id", db),
            "topics", json_incref(topic), gobj) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: DD, the operator's topic was refused", NULL);
    }
    JSON_DECREF(topic)
    if(add_draft_col(gobj, db, "u.x", "id") < 0) {
        result += -1;
    }
    json_int_t e0 = log_count(gobj, "error");
    json_t *jn_resp = treedb_cmd(gobj, db, "save-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0 || log_count(gobj, "error") - e0 != 1 ||
            has_saved_schema(gobj, db)) {
        result += test_fail(gobj, db, "TEST FAIL: DD, a draft whose ids collide was saved",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)

    /*
     *  A saved schema that collides, written by hand: not applied
     */
    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), priv->path_database, "__system__", "saved_schemas", NULL);
    mkrdir(dir, 02770);
    save_json_to_file(gobj, dir, "tw_ds.treedb_schema.json", 02770, 0660, 0, TRUE, FALSE,
        schema_of(db, 2, json_pack("[o,o]",
            topic_of("u", 2, json_pack("{s:o, s:o}", "id", col_id(), "x", col_str("X")))  ,
            topic_of("u.x", 1, json_pack("{s:o}", "id", col_id()))
        )));
    e0 = log_count(gobj, "error");
    jn_resp = treedb_cmd(gobj, db, "apply-schema", json_object());
    json_t *file = load_schema_file(gobj, db);
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0 || log_count(gobj, "error") - e0 != 1 ||
            kw_get_int(gobj, file, "schema_version", 0, KW_WILD_NUMBER) != 1) {
        result += test_fail(gobj, db, "TEST FAIL: DD, a saved schema whose ids collide was applied",
            json_incref(jn_resp));
    }
    JSON_DECREF(file)
    JSON_DECREF(jn_resp)
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  COLF: the literal removes `departments`; the operator had added the
 *  column `budget` to it (a draft). The topic is deleted, and the deletes
 *  of its columns FAIL (the directory of the keys of `cols` is read-only).
 *  The draft is still in __system__, so it is not said at that open: the
 *  record keeps its kind. The open that removes the columns says it.
 ***************************************************************************/
PRIVATE int scenario_gone_topic_col_undeletable(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *db = "tw_l8";
    int result = 0;
    char keys[PATH_MAX];
    build_path(keys, sizeof(keys), priv->path_database, "__system__", "cols", "keys", NULL);

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    result += add_draft_col(gobj, db, "departments", "budget");
    close_db(gobj, db);

    chmod(keys, 0550);
    json_t *jn_resp = open_db_resp(gobj, db, users_only_v2(db));
    chmod(keys, 02770);
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: COLF, open-treedb failed", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    result += check_withdrawn(gobj, db, "TEST FAIL: COLF, a draft still in __system__ was said",
        0, json_object());
    json_t *record = unfinished_record(gobj, db);
    json_t *expected_kinds = json_pack("{s:s}", "departments", "unsaved");
    if(!json_equal(json_object_get(record, "draft_kinds"), expected_kinds)) {
        result += test_fail(gobj, db, "TEST FAIL: COLF, the record does not keep the kind of the draft",
            json_incref(record));
    }
    JSON_DECREF(expected_kinds)
    JSON_DECREF(record)
    close_db(gobj, db);

    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db, "TEST FAIL: COLF, the open that removed the draft did not say it",
        0, json_pack("{s:s}", "departments", "unsaved"));
    result += check_agree(gobj, db, "TEST FAIL: COLF, the projection was not completed");
    result += check_absent(gobj, db, "TEST FAIL: COLF, a column stayed", "cols", "tw_l8.departments.budget");
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  SE: a projection STAMPED BEFORE ITS TOPICS were written, as 7.25.4 and
 *  earlier wrote it and a crash of its first open left it: the node of the
 *  treedb says the literal 2 (schema_version and c_schema_version 2), it
 *  holds no topic, and there is no schema file. The open with the literal
 *  2 does not take it as done: it completes the projection, and says
 *  nothing (what is missing is the projection's, not the operator's).
 *  Before, it was taken as done, at every open.
 ***************************************************************************/
PRIVATE int scenario_stamped_before_its_topics(hgobj gobj)
{
    const char *db = "tw_se";
    int result = 0;
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    if(open_db(gobj, "tw_se0", users_departments_v1("tw_se0"), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, "tw_se0");
    json_t *meta = gobj_list_nodes(sys, "treedbs", json_pack("{s:s}", "id", "tw_se0"), 0, gobj);
    json_int_t meta_version = kw_get_int(gobj, json_array_get(meta, 0), "system_schema_version",
        0, KW_WILD_NUMBER);
    JSON_DECREF(meta)
    json_t *node = gobj_create_node(sys, "treedbs",
        json_pack("{s:s, s:i, s:i, s:I}", "id", db, "schema_version", 2, "c_schema_version", 2,
            "system_schema_version", meta_version),
        json_pack("{s:b}", "refs", 1), gobj);
    if(!node) {
        result += test_fail(gobj, db, "TEST FAIL: SE, the stamped node could not be made", NULL);
    }
    JSON_DECREF(node)

    json_t *v2 = schema_of(db, 2, json_pack("[o,o]",
        topic_of("users", 2, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User v2"))),
        topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
    ));
    result += open_counting(gobj, db, v2,
        "TEST FAIL: SE, the open said something of a projection that died", 0, 0);
    result += check_withdrawn(gobj, db, "TEST FAIL: SE, the projection's own gaps were said",
        0, json_object());
    result += check_agree(gobj, db, "TEST FAIL: SE, a projection stamped before its topics was taken as done");
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  SH, SI: 7.25.4 wrote the numbers of the treedb node first, then each
 *  topic, then its columns. Its process died after the write of the topic
 *  `users` (raised to topic_version 2) and before the write of its column
 *  `username`, whose header the literal 2 changes. The open with the
 *  literal 2 completes the projection: __system__ says the literal, no
 *  draft is left, and the next literal withdraws nothing, because nobody
 *  did anything. SH opens with the file, SI with impose_c_schema.
 ***************************************************************************/
PRIVATE json_t *sh_literal(const char *db, int version, const char *header)
{
    return schema_of(db, version, json_pack("[o,o]",
        topic_of("users", version, json_pack("{s:o, s:o}",
            "id", col_id(), "username", col_str(header))),
        topic_of("departments", 1, json_pack("{s:o, s:o}",
            "id", col_id(), "name", col_str("Name")))
    ));
}

PRIVATE int stamped_before_its_columns(hgobj gobj, const char *db, BOOL imposed)
{
    int result = 0;
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    if(open_db(gobj, db, sh_literal(db, 1, "User"), imposed) < 0) {
        return -1;
    }
    close_db(gobj, db);

    json_t *node = gobj_update_node(sys, "treedbs",
        json_pack("{s:s, s:i, s:i}", "id", db, "schema_version", 2, "c_schema_version", 2),
        json_pack("{s:b}", "refs", 1), gobj);
    char topic_id[NAME_MAX];
    snprintf(topic_id, sizeof(topic_id), "%s.users", db);
    json_t *topic = gobj_update_node(sys, "topics",
        json_pack("{s:s, s:i}", "id", topic_id, "topic_version", 2),
        json_pack("{s:b}", "refs", 1), gobj);
    if(!node || !topic) {
        result += test_fail(gobj, db, "TEST FAIL: SH, the state of the crash could not be made", NULL);
    }
    JSON_DECREF(node)
    JSON_DECREF(topic)

    if(open_db(gobj, db, sh_literal(db, 2, "User v2"), imposed) < 0) {
        return -1;
    }
    result += check_header(gobj, db,
        "TEST FAIL: SH, a column the projection that died did not write was taken as written",
        "users", "username", "User v2", "User v2", "User v2");
    result += check_agree(gobj, db, "TEST FAIL: SH, a projection stamped before its columns was taken as done");
    result += check_withdrawn(gobj, db, "TEST FAIL: SH, the projection's own gaps were said",
        0, json_object());
    close_db(gobj, db);

    if(open_db(gobj, db, sh_literal(db, 3, "User v2"), imposed) < 0) {
        return -1;
    }
    result += check_withdrawn(gobj, db, "TEST FAIL: SH, the next literal withdrew work nobody did",
        0, json_object());
    result += check_agree(gobj, db, "TEST FAIL: SH, the next literal does not agree");
    close_db(gobj, db);
    return result;
}

PRIVATE int scenario_stamped_before_its_columns(hgobj gobj)
{
    return stamped_before_its_columns(gobj, "tw_sh", FALSE);
}

PRIVATE int scenario_imposed_stamped_before_its_columns(hgobj gobj)
{
    return stamped_before_its_columns(gobj, "tw_si", TRUE);
}

/***************************************************************************
 *  SJ: SE with impose_c_schema. The node of the treedb says the literal 2,
 *  it holds no topic, and there is no schema file. The imposed open with
 *  the literal 2 completes the projection, and there is no draft.
 ***************************************************************************/
PRIVATE int scenario_imposed_stamped_before_its_topics(hgobj gobj)
{
    const char *db = "tw_sj";
    int result = 0;
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    if(open_db(gobj, "tw_sj0", sh_literal("tw_sj0", 1, "User"), TRUE) < 0) {
        return -1;
    }
    close_db(gobj, "tw_sj0");
    json_t *meta = gobj_list_nodes(sys, "treedbs", json_pack("{s:s}", "id", "tw_sj0"), 0, gobj);
    json_int_t meta_version = kw_get_int(gobj, json_array_get(meta, 0), "system_schema_version",
        0, KW_WILD_NUMBER);
    JSON_DECREF(meta)
    json_t *node = gobj_create_node(sys, "treedbs",
        json_pack("{s:s, s:i, s:i, s:I}", "id", db, "schema_version", 2, "c_schema_version", 2,
            "system_schema_version", meta_version),
        json_pack("{s:b}", "refs", 1), gobj);
    if(!node) {
        result += test_fail(gobj, db, "TEST FAIL: SJ, the stamped node could not be made", NULL);
    }
    JSON_DECREF(node)

    if(open_db(gobj, db, sh_literal(db, 2, "User v2"), TRUE) < 0) {
        return -1;
    }
    result += check_withdrawn(gobj, db, "TEST FAIL: SJ, the projection's own gaps were said",
        0, json_object());
    result += check_agree(gobj, db,
        "TEST FAIL: SJ, an imposed projection stamped before its topics was taken as done");
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  SK: with impose_c_schema, a draft over a COMPLETE projection is the
 *  operator's, and an open with the same literal keeps it: it is not
 *  taken for a projection that died.
 ***************************************************************************/
PRIVATE int scenario_imposed_draft_kept(hgobj gobj)
{
    const char *db = "tw_sk";
    int result = 0;

    if(open_db(gobj, db, sh_literal(db, 1, "User"), TRUE) < 0) {
        return -1;
    }
    result += edit_header(gobj, db, "users", "username", "Operator");
    close_db(gobj, db);

    if(open_db(gobj, db, sh_literal(db, 1, "User"), TRUE) < 0) {
        return -1;
    }
    result += check_header(gobj, db, "TEST FAIL: SK, the operator's draft was replaced",
        "users", "username", "User", "User", "Operator");
    result += check_draft_changed(gobj, db, "TEST FAIL: SK, the operator's draft is not a draft",
        json_pack("{s:b}", "users", 1));
    result += check_withdrawn(gobj, db, "TEST FAIL: SK, an open said a draft it kept",
        0, json_object());
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  LG: the move of a projection keyed by rowid (an older meta-schema) to
 *  qualified ids DIES at each of its writes. The next open completes the
 *  move: no legacy id is left, every node is at its qualified id, linked
 *  where it belongs, with its content (the operator's column included),
 *  and nothing is logged as an error. Before, a move that died left a
 *  qualified copy that the next move failed to create ("Node already
 *  exists"), and the legacy node stayed.
 ***************************************************************************/
/*
 *  The nodes are what the projector of 7.13.1 (the last before qualified
 *  ids) wrote, field for field: a topic {value, pkey, system_flag, tkey,
 *  topic_version, system_topic}, a column {value, header, fillspace, type,
 *  flag} (build_topic_projection / build_col_projection of 7.13.1), keyed by
 *  rowid, with no `order`: it did not exist before 7.14.0. Loaded by this
 *  release, a record gets the defaults of the fields it lacks, `order`
 *  9999 among them, and so does a node created here: the same node.
 */
PRIVATE int build_legacy_projection(hgobj gobj, const char *db)
{
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);
    char topic_rowid[NAME_MAX];
    snprintf(topic_rowid, sizeof(topic_rowid), "%s-7", db);

    json_t *treedb = gobj_create_node(sys, "treedbs",
        json_pack("{s:s, s:i, s:i, s:i}", "id", db, "schema_version", 1, "c_schema_version", 1,
            "system_schema_version", 1),
        json_pack("{s:b}", "refs", 1), gobj);
    json_t *topic = gobj_create_node(sys, "topics",
        json_pack("{s:s, s:s, s:s, s:s, s:s, s:i, s:b}", "id", topic_rowid, "value", "users",
            "pkey", "id", "system_flag", "sf_string_key", "tkey", "", "topic_version", 1,
            "system_topic", 0),
        json_pack("{s:b}", "refs", 1), gobj);
    if(!treedb || !topic ||
            gobj_link_nodes(sys, "topics", "treedbs", json_incref(treedb), "topics",
                json_incref(topic), gobj) < 0) {
        JSON_DECREF(treedb)
        JSON_DECREF(topic)
        return test_fail(gobj, db, "TEST FAIL: LG, the legacy projection could not be made", NULL);
    }
    static const char *cols[][4] = {
        {"70", "id",           "Id",       "required"},
        {"71", "username",     "Login",    ""},
        {"72", "operator_col", "Operator", ""},
        {NULL, NULL, NULL, NULL}
    };
    int result = 0;
    for(int i = 0; cols[i][0]; i++) {
        char rowid[NAME_MAX];
        snprintf(rowid, sizeof(rowid), "%s-%s", db, cols[i][0]);
        json_t *flag = empty_string(cols[i][3])?
            json_pack("[s]", "persistent") : json_pack("[s,s]", "persistent", cols[i][3]);
        json_t *col = gobj_create_node(sys, "cols",
            json_pack("{s:s, s:s, s:s, s:s, s:i, s:o}", "id", rowid, "value", cols[i][1],
                "header", cols[i][2], "type", "string", "fillspace", 10, "flag", flag),
            json_pack("{s:b}", "refs", 1), gobj);
        if(!col || gobj_link_nodes(sys, "cols", "topics", json_incref(topic), "cols",
                json_incref(col), gobj) < 0) {
            result += test_fail(gobj, db, "TEST FAIL: LG, a legacy column could not be made", NULL);
        }
        JSON_DECREF(col)
    }
    JSON_DECREF(treedb)
    JSON_DECREF(topic)
    return result;
}

PRIVATE json_t *lg_literal(const char *db)
{
    return schema_of(db, 1, json_pack("[o]",
        topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("Login")))));
}

PRIVATE int open_with_killed(hgobj gobj, const char *db, json_t *jn_schema, int kill_at) // owned
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if(pid < 0) {
        JSON_DECREF(jn_schema)
        return test_fail(gobj, db, "TEST FAIL: LG, fork() failed", json_string(strerror(errno)));
    }
    if(pid == 0) {
        child_takes_its_own_ring();
        priv->system_writes = 0;
        priv->kill_at_write = kill_at;
        watch_system_writes(gobj, TRUE);
        open_db(gobj, db, jn_schema, FALSE);
        _exit(3);
    }
    JSON_DECREF(jn_schema)
    int status = 0;
    if(waitpid(pid, &status, 0) < 0) {
        return test_fail(gobj, db, "TEST FAIL: LG, waitpid() failed", json_string(strerror(errno)));
    }
    if(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL) {
        return 1;
    }
    if(WIFEXITED(status) && WEXITSTATUS(status) == 3) {
        return 0;
    }
    return test_fail(gobj, db, "TEST FAIL: LG, the child process ended some other way",
        json_integer(status));
}

PRIVATE int scenario_legacy_move_killed(hgobj gobj)
{
    int result = 0;
    int kills = 0;
    for(int k = 1; k <= 40; k++) {
        char db[64];
        snprintf(db, sizeof(db), "tw_lg%d", k);
        if(build_legacy_projection(gobj, db) < 0) {
            return result - 1;
        }
        int killed = open_with_killed(gobj, db, lg_literal(db), k);
        if(killed < 0) {
            return result - 1;
        }
        restart_system(gobj);

        json_int_t e0 = log_count(gobj, "error");
        if(open_db(gobj, db, lg_literal(db), FALSE) < 0) {
            return result - 1;
        }
        if(log_count(gobj, "error") - e0 != 0) {
            result += test_fail(gobj, db, "TEST FAIL: LG, the move that completes logged an error",
                json_integer(k));
        }
        static const char *legacy[][2] = {
            {"topics", "-7"}, {"cols", "-70"}, {"cols", "-71"}, {"cols", "-72"}, {NULL, NULL}
        };
        for(int i = 0; legacy[i][0]; i++) {
            char id[NAME_MAX];
            snprintf(id, sizeof(id), "%s%s", db, legacy[i][1]);
            result += check_absent(gobj, db, "TEST FAIL: LG, a legacy node stayed", legacy[i][0], id);
        }
        char topic_id[NAME_MAX], ref[NAME_MAX + 16];
        snprintf(topic_id, sizeof(topic_id), "%s.users", db);
        snprintf(ref, sizeof(ref), "treedbs^%s^topics", db);
        if(!system_node_links(gobj, "topics", topic_id, "treedbs", ref)) {
            result += test_fail(gobj, db, "TEST FAIL: LG, the topic is not at its qualified id, linked",
                json_integer(k));
        }
        static const char *names[][2] = {
            {"id", "Id"}, {"username", "Login"}, {"operator_col", "Operator"}, {NULL, NULL}
        };
        for(int i = 0; names[i][0]; i++) {
            char col_id_[NAME_MAX];
            snprintf(col_id_, sizeof(col_id_), "%s.users.%s", db, names[i][0]);
            snprintf(ref, sizeof(ref), "topics^%s^cols", topic_id);
            json_t *col = system_node(gobj, "cols", col_id_);
            if(!col || !system_node_links(gobj, "cols", col_id_, "topics", ref) ||
                    strcmp(kw_get_str(gobj, col, "header", "", 0), names[i][1])!=0) {
                result += test_fail(gobj, db,
                    "TEST FAIL: LG, a column is not at its qualified id, linked, with its content",
                    json_pack("{s:s, s:i, s:O}", "col", col_id_, "k", k, "node", col? col : json_null()));
            }
            JSON_DECREF(col)
        }
        close_db(gobj, db);
        if(killed == 0) {
            break;
        }
        kills++;
    }
    if(kills < 5) {
        result += test_fail(gobj, "tw_lg", "TEST FAIL: LG, the move was killed at too few writes",
            json_integer(kills));
    }
    return result;
}

/***************************************************************************
 *  DC: the process dies TWICE. As CR, a child opens v2 and is killed at
 *  the write k1 of the projection; then a second child retries it and is
 *  killed at the write k2 of the retry, or completes it (k2 0); then the
 *  parent opens, twice. For every k1 of the projection: the retry not
 *  killed, and (with drafts) killed at its first write and at its last.
 *
 *  Every process says what it REPORTS (the warning of withdrawn work) in
 *  a file, the moment it logs it (dc_report_log_write): a report made by
 *  a child that dies is counted too. Across the whole sequence the
 *  operator's drafts (a header edited in `users`, a column added to
 *  `groups`) are reported EXACTLY ONCE, by whichever process completes
 *  the projection, and nothing that nobody did is reported: a retry that
 *  completes in the second child reports there, so a count made only in
 *  the parent reads it as one failure per k1. The parent's first open
 *  logs no error and one warning when it is the one that reports; its
 *  second open reports nothing. Without drafts nothing is reported
 *  anywhere (k2 1 and not killed).
 ***************************************************************************/
PRIVATE char dc_reports_path[PATH_MAX] = "";
PRIVATE const char *dc_who = "";
PRIVATE BOOL dc_append_failed = FALSE;   // a line of reports that could not be written

/*
 *  Append a line to the file of reports: {"who": ..., <what>}
 */
PRIVATE void dc_append(json_t *line) // owned
{
    char *s = json_dumps(line, JSON_COMPACT|JSON_SORT_KEYS);
    int fd = open(dc_reports_path, O_WRONLY|O_CREAT|O_APPEND|O_CLOEXEC, 0660);
    if(fd < 0 || !s || write(fd, s, strlen(s)) < 0 || write(fd, "\n", 1) < 0) {
        dc_append_failed = TRUE;    /*  a child cannot say it: its file says less, and the sequence fails  */
    }
    if(fd >= 0) {
        close(fd);
    }
    if(s) {
        gbmem_free(s);
    }
    JSON_DECREF(line)
}

PRIVATE int dc_report_log_write(void *h, int priority, const char *bf, size_t len)
{
    if(empty_string(dc_reports_path) ||
            !strstr(bf, "Schema from C withdrew work on the schema at open")) {
        return 0;
    }
    json_t *jn = json_loadb(bf, len, 0, 0);
    json_t *topics = NULL;
    const char *st = kw_get_str(0, jn, "topics", 0, 0);
    if(st) {
        /*
         *  A "%j" field reaches the log as a string with single quotes
         */
        char *copy = gbmem_strdup(st);
        for(char *c = copy; c && *c; c++) {
            if(*c == '\'') {
                *c = '"';
            }
        }
        topics = copy? json_loads(copy, 0, 0) : NULL;
        GBMEM_FREE(copy)
    }
    JSON_DECREF(jn)
    dc_append(json_pack("{s:s, s:o}", "who", dc_who, "topics", topics? topics : json_null()));
    return 0;
}

/*
 *  Open `db` with v2 in a CHILD process (`who`) killed at the `kill_at`-th
 *  write of __system__ (0: never). A child that completes says how many
 *  writes it made. 1 when it was killed, 0 when it completed, -1 on error.
 */
PRIVATE int dc_open_child(hgobj gobj, const char *db, int kill_at, const char *who)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if(pid < 0) {
        return test_fail(gobj, db, "TEST FAIL: DC, fork() failed", json_string(strerror(errno)));
    }
    if(pid == 0) {
        child_takes_its_own_ring();
        dc_who = who;
        priv->system_writes = 0;
        priv->kill_at_write = kill_at;
        watch_system_writes(gobj, TRUE);
        open_db(gobj, db, cr_v2(db), FALSE);
        dc_append(json_pack("{s:s, s:i}", "who", who, "writes", priv->system_writes));
        _exit(3);
    }

    int status = 0;
    if(waitpid(pid, &status, 0) < 0) {
        return test_fail(gobj, db, "TEST FAIL: DC, waitpid() failed", json_string(strerror(errno)));
    }
    if(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL) {
        return 1;
    }
    if(WIFEXITED(status) && WEXITSTATUS(status) == 3) {
        return 0;
    }
    return test_fail(gobj, db, "TEST FAIL: DC, the child process ended some other way",
        json_integer(status));
}

/*
 *  One sequence: k1, then k2 (0: the retry completes). `*p_retry_writes`
 *  gets the writes of a retry that completed. 1 when the first child was
 *  not killed at k1 (k1 is beyond the projection), else 0 or what failed.
 */
PRIVATE int dc_sequence(hgobj gobj, BOOL drafts, int k1, int k2, int *p_retry_writes)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    char db[NAME_MAX];
    snprintf(db, sizeof(db), "tw_dc%s_%d_%d", drafts? "d" : "n", k1, k2);
    char filename[NAME_MAX + sizeof(".reports")];
    snprintf(filename, sizeof(filename), "%s.reports", db);
    build_path(dc_reports_path, sizeof(dc_reports_path), priv->path_database, filename, NULL);
    unlink(dc_reports_path);

    dc_who = "prepare";
    if(cr_prepare(gobj, db, drafts) < 0) {
        dc_who = "";
        return -1;
    }
    int killed = dc_open_child(gobj, db, k1, "child1");
    if(killed != 1) {
        dc_who = "";
        return killed < 0? -1 : 1;
    }
    restart_system(gobj);
    int killed2 = dc_open_child(gobj, db, k2, "child2");
    if(killed2 < 0 || (k2 > 0 && killed2 != 1) || (k2 == 0 && killed2 != 0)) {
        dc_who = "";
        return test_fail(gobj, db, "TEST FAIL: DC, the second child did not end where it should",
            json_pack("{s:i, s:i, s:i}", "k1", k1, "k2", k2, "killed", killed2));
    }
    restart_system(gobj);

    dc_who = "parent1";
    json_int_t e0 = log_count(gobj, "error");
    json_int_t w0 = log_count(gobj, "warning");
    if(open_db(gobj, db, cr_v2(db), FALSE) < 0) {
        dc_who = "";
        return -1;
    }
    json_int_t errors = log_count(gobj, "error") - e0;
    json_int_t warnings = log_count(gobj, "warning") - w0;
    result += check_agree(gobj, db, "TEST FAIL: DC, the open after the two crashes did not complete");
    json_t *record = unfinished_record(gobj, db);
    if(record) {
        result += test_fail(gobj, db, "TEST FAIL: DC, a completed projection left its record",
            json_incref(record));
    }
    JSON_DECREF(record)
    close_db(gobj, db);
    dc_who = "parent2";
    if(open_db(gobj, db, cr_v2(db), FALSE) < 0) {
        result--;
    } else {
        close_db(gobj, db);
    }
    dc_who = "";

    /*
     *  What each process reported
     */
    json_t *times = json_object();      // {topic: times reported}
    json_t *kinds = json_object();      // {topic: kind}
    json_t *reports = json_array();     // the lines of the reports
    int parent1_reports = 0;
    int parent2_reports = 0;
    FILE *file = fopen(dc_reports_path, "r");
    char line[4096];
    while(file && fgets(line, sizeof(line), file)) {
        json_t *jn = json_loads(line, 0, 0);
        const char *who = kw_get_str(gobj, jn, "who", "", 0);
        if(strcmp(who, "child2")==0 && json_object_get(jn, "writes")) {
            *p_retry_writes = (int)kw_get_int(gobj, jn, "writes", 0, 0);
        }
        json_t *topics = json_object_get(jn, "topics");
        if(topics) {
            json_array_append(reports, jn);
            if(strcmp(who, "parent1")==0) {
                parent1_reports++;
            } else if(strcmp(who, "parent2")==0) {
                parent2_reports++;
            }
        }
        const char *topic; json_t *kind;
        json_object_foreach(topics, topic, kind) {
            json_object_set_new(times, topic,
                json_integer(json_integer_value(json_object_get(times, topic)) + 1));
            json_object_set(kinds, topic, kind);
        }
        JSON_DECREF(jn)
    }
    if(file) {
        fclose(file);
    }

    if(dc_append_failed) {
        dc_append_failed = FALSE;
        result += test_fail(gobj, db, "TEST FAIL: DC, a report could not be written to its file",
            json_string(dc_reports_path));
    }

    json_t *expected = drafts?
        json_pack("{s:s, s:s}", "users", "unsaved", "groups", "unsaved") : json_object();
    BOOL once = TRUE;
    const char *topic; json_t *jn_times;
    json_object_foreach(times, topic, jn_times) {
        if(json_integer_value(jn_times) != 1) {
            once = FALSE;
        }
    }
    if(!once || !json_equal(kinds, expected) || parent2_reports > 0) {
        result += test_fail(gobj, db,
            "TEST FAIL: DC, the drafts are not reported exactly once, or work nobody did is reported",
            json_pack("{s:i, s:i, s:O, s:O}", "k1", k1, "k2", k2, "reports", reports, "expected", expected));
    }
    if(errors != 0 || warnings != parent1_reports) {
        result += test_fail(gobj, db,
            "TEST FAIL: DC, the open after the two crashes logged errors, or warnings it did not report",
            json_pack("{s:i, s:i, s:I, s:I, s:i}", "k1", k1, "k2", k2,
                "errors", errors, "warnings", warnings, "reports", parent1_reports));
    }
    JSON_DECREF(expected)
    JSON_DECREF(times)
    JSON_DECREF(kinds)
    JSON_DECREF(reports)

    priv->dc_combos++;
    if(k2 == 0) {
        priv->dc_completed++;
    }
    return result;
}

/*
 *  ONE k1 per step (see ac_timeout): the loop runs between two
 */
PRIVATE int scenario_double_crash(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    BOOL drafts = (priv->dc_phase == 0)? TRUE : FALSE;

    if(priv->dc_k1 == 0) {
        if(priv->dc_phase == 0) {
            gobj_log_register_handler("dc_reports", 0, dc_report_log_write, 0);
            gobj_log_add_handler("dc_reports", "dc_reports", LOG_OPT_UP_WARNING, 0);
        }

        /*
         *  How many writes the projection makes: an open nobody kills
         */
        char db[NAME_MAX];
        snprintf(db, sizeof(db), "tw_dc%s_all", drafts? "d" : "n");
        result += cr_prepare(gobj, db, drafts);
        priv->system_writes = 0;
        priv->kill_at_write = 0;
        watch_system_writes(gobj, TRUE);
        if(open_db(gobj, db, cr_v2(db), FALSE) == 0) {
            close_db(gobj, db);
        }
        watch_system_writes(gobj, FALSE);
        priv->dc_writes = priv->system_writes;
        if(priv->dc_writes < 10) {
            return result + test_fail(gobj, db, "TEST FAIL: DC, too few writes seen",
                json_integer(priv->dc_writes));
        }
        priv->dc_k1 = 1;
    }

    /*
     *  First the retry that completes: it says how many writes a retry
     *  makes after a crash at k1, and so where its last write is. With
     *  drafts the retry is then killed at its first write and at its last
     *  (after the stamp, before the record is removed); without drafts
     *  only the retry that completes is run: nothing may be reported
     */
    int k1 = priv->dc_k1;
    int retry_writes = 0;
    int r = dc_sequence(gobj, drafts, k1, 0, &retry_writes);
    if(r == 1) {
        result += test_fail(gobj, "tw_dc", "TEST FAIL: DC, the first child was not killed at its write",
            json_integer(k1));
    } else {
        result += r;
        if(drafts && retry_writes < 1) {
            result += test_fail(gobj, "tw_dc", "TEST FAIL: DC, a retry that completed made no write",
                json_integer(k1));
        } else if(drafts) {
            int unused = 0;
            result += dc_sequence(gobj, drafts, k1, 1, &unused);
            if(retry_writes > 1) {
                result += dc_sequence(gobj, drafts, k1, retry_writes, &unused);
            }
        }
    }

    priv->dc_k1++;
    if(priv->dc_k1 <= priv->dc_writes) {
        priv->repeat_step = TRUE;
        return result;
    }
    printf("DC (%s): a projection of %d writes, killed at each one and its retry killed or not: %d sequences, %d where the retry completed\n",
        drafts? "with drafts" : "no drafts", priv->dc_writes, priv->dc_combos, priv->dc_completed);
    priv->dc_combos = 0;
    priv->dc_completed = 0;
    priv->dc_k1 = 0;
    priv->dc_phase++;
    if(priv->dc_phase < 2) {
        priv->repeat_step = TRUE;
    } else {
        gobj_log_del_handler("dc_reports");
        dc_reports_path[0] = 0;
    }
    return result;
}

/***************************************************************************
 *  7.25.4 PROJECTIONS THAT DIED, emulated write by write.
 *
 *  7.25.4 wrote the numbers of the treedb node FIRST, then, in the order
 *  of the literal, each topic it changed (an update, or a create and its
 *  link to the treedb) and each column of it that changed (an update, or
 *  a create and its link to the topic). It never deleted. A process that
 *  died after the n-th of those writes is emulated by making the first n
 *  of them here, as 7.25.4 made them: the nodes are what its
 *  build_topic_projection() and build_col_projection() wrote.
 ***************************************************************************/
PRIVATE json_t *old_topic_node(const char *db, const char *name, int topic_version, int order)
{
    char id[NAME_MAX];
    snprintf(id, sizeof(id), "%s.%s", db, name);
    return json_pack("{s:s, s:s, s:s, s:s, s:s, s:i, s:b, s:b, s:i}",
        "id", id, "value", name, "pkey", "id", "system_flag", "sf_string_key", "tkey", "",
        "topic_version", topic_version, "system_topic", 0, "main_topic", 0, "order", order);
}

PRIVATE json_t *old_col_node(const char *db, const char *topic, const char *name,
    const char *header, BOOL required, int order)
{
    char id[NAME_MAX];
    snprintf(id, sizeof(id), "%s.%s.%s", db, topic, name);
    json_t *flag = required?
        json_pack("[s,s]", "persistent", "required") : json_pack("[s]", "persistent");
    return json_pack("{s:s, s:s, s:s, s:i, s:s, s:o, s:i}",
        "id", id, "value", name, "header", header, "fillspace", 10, "type", "string",
        "flag", flag, "order", order);
}

PRIVATE json_t *old_write(const char *op, const char *system_topic, json_t *node) // node owned
{
    return json_pack("{s:s, s:s, s:o}", "op", op, "topic", system_topic, "node", node);
}

PRIVATE json_t *old_link(const char *parent_topic, const char *parent_id,
    const char *child_topic, const char *child_id)
{
    return json_pack("{s:s, s:s, s:s, s:s, s:s}", "op", "link",
        "parent_topic", parent_topic, "parent", parent_id, "topic", child_topic, "child", child_id);
}

/*
 *  The stamp 7.25.4 wrote first: an update of the treedb node, or its
 *  create (a first projection)
 */
PRIVATE json_t *old_stamp(hgobj gobj, const char *db, const char *op, int version)
{
    json_t *meta = gobj_list_nodes(gobj_find_service(SYSTEM_TREEDB, FALSE), "treedbs",
        json_pack("{s:s}", "id", db), 0, gobj);
    json_int_t meta_version = kw_get_int(gobj, json_array_get(meta, 0), "system_schema_version",
        0, KW_WILD_NUMBER);
    JSON_DECREF(meta)
    if(meta_version == 0) {
        meta_version = 18;  /*  a first projection: the treedb has no node yet  */
    }
    return old_write(op, "treedbs", json_pack("{s:s, s:i, s:i, s:I}", "id", db,
        "schema_version", version, "c_schema_version", version, "system_schema_version", meta_version));
}

/*
 *  Make the first `n` writes of `ops` (every one when n < 0). Return how
 *  many were made, -1 when one failed
 */
PRIVATE int run_old_writes(hgobj gobj, const char *db, json_t *ops, int n) // ops not owned
{
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);
    int made = 0;
    int idx; json_t *op;
    json_array_foreach(ops, idx, op) {
        if(n >= 0 && made >= n) {
            break;
        }
        const char *what = kw_get_str(gobj, op, "op", "", 0);
        const char *topic = kw_get_str(gobj, op, "topic", "", 0);
        int ret = 0;
        if(strcmp(what, "link")==0) {
            ret = gobj_link_nodes(sys, topic, kw_get_str(gobj, op, "parent_topic", "", 0),
                json_pack("{s:s}", "id", kw_get_str(gobj, op, "parent", "", 0)),
                topic, json_pack("{s:s}", "id", kw_get_str(gobj, op, "child", "", 0)), gobj);
        } else {
            json_t *node = strcmp(what, "create")==0?
                gobj_create_node(sys, topic, json_incref(json_object_get(op, "node")),
                    json_pack("{s:b}", "refs", 1), gobj) :
                gobj_update_node(sys, topic, json_incref(json_object_get(op, "node")),
                    json_pack("{s:b}", "refs", 1), gobj);
            ret = node? 0 : -1;
            JSON_DECREF(node)
        }
        if(ret < 0) {
            return test_fail(gobj, db, "TEST FAIL: a write of the emulated 7.25.4 projection failed",
                json_incref(op));
        }
        made++;
    }
    return made;
}

/*
 *  The record of the upgrade: removed, the store is one an older release
 *  left (no open of this release has been here)
 */
PRIVATE void remove_upgrade_marker(hgobj gobj, const char *db)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), priv->path_database, "__system__", "saved_schemas", NULL);
    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "%s.upgrade.json", db);
    if(file_exists(dir, filename)) {
        file_remove(dir, filename);
    }
}

/*
 *  delete-treedb: a sequence that ran leaves nothing in __system__, which
 *  every later open reads
 */
PRIVATE void drop_treedb(hgobj gobj, const char *db)
{
    json_t *jn_resp = treedb_cmd(gobj, db, "delete-treedb", json_pack("{s:b}", "force", 1));
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
        test_fail(gobj, db, "TEST FAIL: delete-treedb of a sequence that ran failed",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
}

/*
 *  What an open reports as withdrawn, written by every process that logs
 *  it (a child that dies too) into a file, as DC does
 */
PRIVATE int old_reports_count(hgobj gobj)
{
    int reports = 0;
    FILE *file = fopen(dc_reports_path, "r");
    char line[4096];
    while(file && fgets(line, sizeof(line), file)) {
        json_t *jn = json_loads(line, 0, 0);
        if(json_object_get(jn, "topics")) {
            reports++;
        }
        JSON_DECREF(jn)
    }
    if(file) {
        fclose(file);
    }
    return reports;
}

PRIVATE void old_reports_start(hgobj gobj, const char *db)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    static BOOL registered = FALSE;

    if(!registered) {
        gobj_log_register_handler("old_reports", 0, dc_report_log_write, 0);
        registered = TRUE;
    }
    char filename[NAME_MAX + sizeof(".reports")];
    snprintf(filename, sizeof(filename), "%s.reports", db);
    build_path(dc_reports_path, sizeof(dc_reports_path), priv->path_database, filename, NULL);
    unlink(dc_reports_path);
    gobj_log_add_handler("old_reports", "old_reports", LOG_OPT_UP_WARNING, 0);
}

PRIVATE void old_reports_stop(void)
{
    gobj_log_del_handler("old_reports");
    dc_reports_path[0] = 0;
}

/*
 *  Open `db` with `literal` in a CHILD killed at the `kill_at`-th write
 *  of __system__ (0: never). 1 when it was killed, 0 when it completed,
 *  -1 on error
 */
PRIVATE int open_child_killed(hgobj gobj, const char *db, json_t *literal, BOOL imposed,
    int kill_at, const char *who) // literal owned
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if(pid < 0) {
        JSON_DECREF(literal)
        return test_fail(gobj, db, "TEST FAIL: fork() failed", json_string(strerror(errno)));
    }
    if(pid == 0) {
        child_takes_its_own_ring();
        dc_who = who;
        priv->system_writes = 0;
        priv->kill_at_write = kill_at;
        watch_system_writes(gobj, TRUE);
        open_db(gobj, db, literal, imposed);
        _exit(3);
    }
    JSON_DECREF(literal)
    int status = 0;
    if(waitpid(pid, &status, 0) < 0) {
        return test_fail(gobj, db, "TEST FAIL: waitpid() failed", json_string(strerror(errno)));
    }
    if(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL) {
        return 1;
    }
    if(WIFEXITED(status) && WEXITSTATUS(status) == 3) {
        return 0;
    }
    return test_fail(gobj, db, "TEST FAIL: the child process ended some other way",
        json_integer(status));
}

/***************************************************************************
 *  O4C, O4A: a projection of 7.25.4 died after each of its writes, and
 *  the next open (this release) completes it. Nothing is reported --
 *  nobody did anything -- then or at the next literal, and the three
 *  homes of the schema agree:
 *
 *    O4C  the literal 2 changes the headers of TWO columns of `users`:
 *         killed with one written and the other not, the topic differs
 *         from the old file AND from the literal, and was reported as
 *         the operator's ("unsaved"). Only a NODE that differs from both
 *         is the operator's.
 *    O4A  the literal 2 adds a column to `users` and the topic `roles`:
 *         a create killed before its link leaves a node in no tree, and
 *         it was reported as the operator's.
 *
 *  Each with the record of the upgrade removed (the store is 7.25.4's)
 *  and kept (the first open of v1 was by this release). With
 *  `double_crash` (the record of the upgrade removed) the open that
 *  completes is killed too, at each of its writes, and the one after it
 *  completes: its record must keep what the projection that died was
 *  projecting, or the retry counts against the old file alone.
 ***************************************************************************/
PRIVATE json_t *o4c_literal(const char *db, int version, int topic_version,
    const char *h1, const char *h2)
{
    return schema_of(db, version, json_pack("[o,o]",
        topic_of("users", topic_version, json_pack("{s:o, s:o, s:o}", "id", col_id(),
            "username", col_str(h1), "email", col_str(h2))),
        topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
    ));
}

PRIVATE json_t *o4a_literal(const char *db, int version)
{
    if(version == 1) {
        return schema_of(db, 1, json_pack("[o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        ));
    }
    return schema_of(db, version, json_pack("[o,o]",
        topic_of("users", 2, json_pack("{s:o, s:o, s:o}", "id", col_id(),
            "username", col_str("User"), "email", col_str("Email"))),
        topic_of("roles", 1, json_pack("{s:o, s:o}", "id", col_id(), "rname", col_str("Role")))
    ));
}

PRIVATE json_t *o4_literal(const char *db, BOOL adds, int version)
{
    if(adds) {
        return o4a_literal(db, version);
    }
    if(version == 1) {
        return o4c_literal(db, 1, 1, "User", "Email");
    }
    return o4c_literal(db, version, 2, "User v2", "Email v2");
}

/*
 *  The writes of 7.25.4 opening the literal 2 over the projection of 1
 */
PRIVATE json_t *o4_writes(hgobj gobj, const char *db, BOOL adds)
{
    char users[NAME_MAX], roles[NAME_MAX], id[NAME_MAX + 16];
    snprintf(users, sizeof(users), "%s.users", db);
    snprintf(roles, sizeof(roles), "%s.roles", db);
    json_t *ops = json_array();
    json_array_append_new(ops, old_stamp(gobj, db, "update", 2));
    json_array_append_new(ops, old_write("update", "topics", old_topic_node(db, "users", 2, 0)));
    if(!adds) {
        json_array_append_new(ops, old_write("update", "cols",
            old_col_node(db, "users", "username", "User v2", FALSE, 1)));
        json_array_append_new(ops, old_write("update", "cols",
            old_col_node(db, "users", "email", "Email v2", FALSE, 2)));
        return ops;
    }
    json_array_append_new(ops, old_write("create", "cols",
        old_col_node(db, "users", "email", "Email", FALSE, 2)));
    snprintf(id, sizeof(id), "%s.email", users);
    json_array_append_new(ops, old_link("topics", users, "cols", id));
    json_array_append_new(ops, old_write("create", "topics", old_topic_node(db, "roles", 1, 1)));
    json_array_append_new(ops, old_link("treedbs", db, "topics", roles));
    json_array_append_new(ops, old_write("create", "cols", old_col_node(db, "roles", "id", "Id", TRUE, 0)));
    snprintf(id, sizeof(id), "%s.id", roles);
    json_array_append_new(ops, old_link("topics", roles, "cols", id));
    json_array_append_new(ops, old_write("create", "cols",
        old_col_node(db, "roles", "rname", "Role", FALSE, 1)));
    snprintf(id, sizeof(id), "%s.rname", roles);
    json_array_append_new(ops, old_link("topics", roles, "cols", id));
    return ops;
}

/*
 *  One sequence: v1 by this release, 7.25.4 opening v2 dies after `n`
 *  writes, then (`kill_at` > 0) this release opening v2 dies at its
 *  `kill_at`-th write, then this release opens v2, then v3.
 *  `*p_killed` says whether that child was killed.
 */
PRIVATE int o4_sequence(hgobj gobj, BOOL adds, BOOL keep_marker, int n, int kill_at, int *p_killed)
{
    int result = 0;
    char db[NAME_MAX];
    snprintf(db, sizeof(db), "tw_o4%s%s_%d_%d", adds? "a" : "c", keep_marker? "k" : "r", n, kill_at);
    const char *label = adds? "O4A" : "O4C";

    if(open_db(gobj, db, o4_literal(db, adds, 1), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, db);
    if(!keep_marker) {
        remove_upgrade_marker(gobj, db);
    }
    json_t *ops = o4_writes(gobj, db, adds);
    int made = run_old_writes(gobj, db, ops, n);
    JSON_DECREF(ops)
    if(made < 0) {
        return -1;
    }

    old_reports_start(gobj, db);
    *p_killed = 0;
    if(kill_at > 0) {
        *p_killed = open_child_killed(gobj, db, o4_literal(db, adds, 2), FALSE, kill_at, "child");
        if(*p_killed < 0) {
            old_reports_stop();
            return -1;
        }
        restart_system(gobj);
    }

    dc_who = "parent";
    json_int_t e0 = log_count(gobj, "error");
    if(open_db(gobj, db, o4_literal(db, adds, 2), FALSE) < 0) {
        old_reports_stop();
        return -1;
    }
    char what[256];
    snprintf(what, sizeof(what), "TEST FAIL: %s, the open after a projection of 7.25.4 that died "
        "reported work nobody did (n %d, kill %d)", label, n, kill_at);
    result += check_withdrawn(gobj, db, what, 0, json_object());
    snprintf(what, sizeof(what), "TEST FAIL: %s, the open after a projection of 7.25.4 that died "
        "did not complete it (n %d, kill %d)", label, n, kill_at);
    result += check_agree(gobj, db, what);
    close_db(gobj, db);

    json_t *v3 = o4_literal(db, adds, 2);
    json_object_set_new(v3, "schema_version", json_integer(3));
    if(open_db(gobj, db, v3, FALSE) < 0) {
        old_reports_stop();
        return -1;
    }
    snprintf(what, sizeof(what), "TEST FAIL: %s, the next literal reported work nobody did "
        "(n %d, kill %d)", label, n, kill_at);
    result += check_withdrawn(gobj, db, what, 0, json_object());
    close_db(gobj, db);

    int reports = old_reports_count(gobj);
    if(reports > 0 || log_count(gobj, "error") - e0 != 0) {
        result += test_fail(gobj, db, "TEST FAIL: a process reported work nobody did, or logged an error",
            json_pack("{s:s, s:i, s:i, s:i, s:I}", "scenario", label, "n", n, "kill", kill_at,
                "reports", reports, "errors", log_count(gobj, "error") - e0));
    }
    old_reports_stop();
    drop_treedb(gobj, db);
    return result;
}

PRIVATE int old_projection_died(hgobj gobj, BOOL adds, BOOL double_crash)
{
    int result = 0;
    json_t *ops = o4_writes(gobj, "x", adds);
    int total = (int)json_array_size(ops);
    JSON_DECREF(ops)
    for(int marker = 0; marker < (double_crash? 1 : 2); marker++) {
        for(int n = 1; n <= total; n++) {
            if(!double_crash) {
                int killed;
                result += o4_sequence(gobj, adds, marker? TRUE : FALSE, n, 0, &killed);
                continue;
            }
            for(int k = 1; k <= 40; k++) {
                int killed = 0;
                result += o4_sequence(gobj, adds, marker? TRUE : FALSE, n, k, &killed);
                if(!killed) {
                    break;
                }
            }
        }
    }
    return result;
}

PRIVATE int scenario_old_projection_died_changes(hgobj gobj)
{
    return old_projection_died(gobj, FALSE, FALSE);
}

PRIVATE int scenario_old_projection_died_adds(hgobj gobj)
{
    return old_projection_died(gobj, TRUE, FALSE);
}

PRIVATE int scenario_old_projection_died_twice(hgobj gobj)
{
    return old_projection_died(gobj, FALSE, TRUE) + old_projection_died(gobj, TRUE, TRUE);
}

/***************************************************************************
 *  FP: a FIRST projection of 7.25.4 over a schema file already at the
 *  literal's version -- every imposed treedb got its first projection so
 *  between 7.23.0 and 7.25.4, and so did every treedb from before 7.13.0
 *  -- died after its stamp, after each of its writes. The first open by
 *  this release restores what __system__ misses, from the literal, and
 *  says it; nothing is reported as the operator's, then or at the next
 *  literal, and the three homes agree. Once with the file running, once
 *  imposed, once with a newer literal installed at that first open.
 *
 *  Then the operator deletes a topic (a draft): an open with the same
 *  literal does NOT restore it, the record of the upgrade says this
 *  release has been here, and a stamped projection is a complete one.
 *
 *  Red before: the projection was never completed, `draft_changed` named
 *  every topic it missed, and the next literal reported them "unsaved".
 ***************************************************************************/
PRIVATE json_t *fp_literal(const char *db, int version)
{
    return schema_of(db, version, json_pack("[o,o]",
        topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"))),
        topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
    ));
}

PRIVATE json_t *fp_writes(hgobj gobj, const char *db)
{
    json_t *ops = json_array();
    json_array_append_new(ops, old_stamp(gobj, db, "create", 2));
    static const char *topics[][4] = {
        {"users", "username", "User", NULL},
        {"departments", "name", "Name", NULL},
        {NULL, NULL, NULL, NULL}
    };
    for(int t = 0; topics[t][0]; t++) {
        char topic_id[NAME_MAX + 16], col_id_[2 * NAME_MAX];
        snprintf(topic_id, sizeof(topic_id), "%s.%s", db, topics[t][0]);
        json_array_append_new(ops, old_write("create", "topics", old_topic_node(db, topics[t][0], 1, t)));
        json_array_append_new(ops, old_link("treedbs", db, "topics", topic_id));
        json_array_append_new(ops, old_write("create", "cols",
            old_col_node(db, topics[t][0], "id", "Id", TRUE, 0)));
        snprintf(col_id_, sizeof(col_id_), "%s.id", topic_id);
        json_array_append_new(ops, old_link("topics", topic_id, "cols", col_id_));
        json_array_append_new(ops, old_write("create", "cols",
            old_col_node(db, topics[t][0], topics[t][1], topics[t][2], FALSE, 1)));
        snprintf(col_id_, sizeof(col_id_), "%s.%s", topic_id, topics[t][1]);
        json_array_append_new(ops, old_link("topics", topic_id, "cols", col_id_));
    }
    return ops;
}

/*
 *  mode 0: the file runs; 1: imposed; 2: a newer literal is installed at
 *  the first open
 */
PRIVATE int fp_sequence(hgobj gobj, int mode, int n, int *p_total)
{
    int result = 0;
    BOOL imposed = (mode == 1)? TRUE : FALSE;
    char db[NAME_MAX];
    snprintf(db, sizeof(db), "tw_fp%d_%d", mode, n);

    if(open_db(gobj, db, fp_literal(db, 2), imposed) < 0) {
        return -1;
    }
    close_db(gobj, db);

    /*
     *  A store from before: no projection, no record of the upgrade, the
     *  file at the literal's version. Then 7.25.4 projects it and dies
     */
    json_t *ops = fp_writes(gobj, db);
    *p_total = (int)json_array_size(ops);
    drop_treedb(gobj, db);
    int made = run_old_writes(gobj, db, ops, n);
    JSON_DECREF(ops)
    if(made < 0) {
        return -1;
    }

    old_reports_start(gobj, db);
    json_int_t e0 = log_count(gobj, "error");
    json_t *first = fp_literal(db, mode == 2? 3 : 2);
    if(open_db(gobj, db, first, imposed) < 0) {
        old_reports_stop();
        return -1;
    }
    char what[256];
    snprintf(what, sizeof(what),
        "TEST FAIL: FP, the first open after a first projection of 7.25.4 that died reported "
        "work nobody did (mode %d, n %d)", mode, n);
    result += check_withdrawn(gobj, db, what, 0, json_object());
    snprintf(what, sizeof(what),
        "TEST FAIL: FP, the first open did not restore what the projection of 7.25.4 missed "
        "(mode %d, n %d)", mode, n);
    result += check_agree(gobj, db, what);
    close_db(gobj, db);

    if(open_db(gobj, db, fp_literal(db, 4), imposed) < 0) {
        old_reports_stop();
        return -1;
    }
    snprintf(what, sizeof(what), "TEST FAIL: FP, the next literal reported work nobody did "
        "(mode %d, n %d)", mode, n);
    result += check_withdrawn(gobj, db, what, 0, json_object());
    result += check_agree(gobj, db, what);
    int reports = old_reports_count(gobj);
    if(reports > 0 || log_count(gobj, "error") - e0 != 0) {
        result += test_fail(gobj, db, "TEST FAIL: FP, an open reported work nobody did, or logged an error",
            json_pack("{s:i, s:i, s:i, s:I}", "mode", mode, "n", n,
                "reports", reports, "errors", log_count(gobj, "error") - e0));
    }
    old_reports_stop();

    /*
     *  The operator's deletion, after the upgrade, is a draft: kept
     */
    if(!imposed) {
        result += delete_system_topic(gobj, db, "departments");
        close_db(gobj, db);
        if(open_db(gobj, db, fp_literal(db, 4), imposed) < 0) {
            return result - 1;
        }
        if(system_has_topic(gobj, db, "departments")) {
            result += test_fail(gobj, db,
                "TEST FAIL: FP, an open after the upgrade restored a topic the operator deleted",
                json_pack("{s:i, s:i}", "mode", mode, "n", n));
        }
        result += check_draft_changed(gobj, db,
            "TEST FAIL: FP, a topic the operator deleted after the upgrade is not a draft",
            json_pack("{s:b}", "departments", 1));
    }
    close_db(gobj, db);
    drop_treedb(gobj, db);
    return result;
}

PRIVATE int scenario_first_projection_of_older_release_died(hgobj gobj)
{
    int result = 0;
    for(int mode = 0; mode < 3; mode++) {
        int total = 1;
        for(int n = 1; n <= total; n++) {
            result += fp_sequence(gobj, mode, n, &total);
        }
    }
    return result;
}

/***************************************************************************
 *  OL: what 7.25.4 LEFT in __system__. v1 declares `users` (id, username,
 *  email) and `departments`; 7.25.4 opened v2, which drops `departments`
 *  and `users.email`, and never deleted: both stayed in __system__. After
 *  the upgrade:
 *
 *    - an open with the same literal shows no draft (`draft_changed` {}),
 *      withdraws nothing, and removes nothing (no literal is installed);
 *    - the next literal removes them and reports them apart, as
 *      `left_by_older_release`, never as "unsaved", with a WARNING naming
 *      the ids; the one after it reports nothing;
 *    - an edit the operator makes of one of them after the upgrade is the
 *      operator's: `departments` is then a draft, and reported "unsaved";
 *    - at a first open that installs a newer literal they are removed and
 *      reported the same way.
 *
 *  Red before: `draft_changed` {"departments": true, "users": true}, and
 *  the next literal reported them "unsaved".
 ***************************************************************************/
PRIVATE json_t *ol_literal(const char *db, int version)
{
    if(version == 1) {
        return schema_of(db, 1, json_pack("[o,o]",
            topic_of("users", 1, json_pack("{s:o, s:o, s:o}", "id", col_id(),
                "username", col_str("User"), "email", col_str("Email"))),
            topic_of("departments", 1, json_pack("{s:o, s:o}", "id", col_id(), "name", col_str("Name")))
        ));
    }
    return schema_of(db, version, json_pack("[o]",
        topic_of("users", 2, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
    ));
}

/*
 *  7.25.4 opened v2 over the projection of v1: the stamp, the topic
 *  `users` raised, the file installed; nothing deleted
 */
PRIVATE int ol_older_release(hgobj gobj, const char *db)
{
    if(open_db(gobj, db, ol_literal(db, 1), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, db);
    remove_upgrade_marker(gobj, db);
    json_t *ops = json_array();
    json_array_append_new(ops, old_stamp(gobj, db, "update", 2));
    json_array_append_new(ops, old_write("update", "topics", old_topic_node(db, "users", 2, 0)));
    int made = run_old_writes(gobj, db, ops, -1);
    JSON_DECREF(ops)
    if(made < 0) {
        return -1;
    }
    return write_schema_file(gobj, db, ol_literal(db, 2));
}

PRIVATE int scenario_left_by_older_release(hgobj gobj)
{
    int result = 0;
    for(int edited = 0; edited < 2; edited++) {
        char db[NAME_MAX];
        snprintf(db, sizeof(db), "tw_ol%s", edited? "e" : "");
        if(ol_older_release(gobj, db) < 0) {
            return result - 1;
        }

        json_int_t w0 = log_count(gobj, "warning");
        if(open_db(gobj, db, ol_literal(db, 2), FALSE) < 0) {
            return result - 1;
        }
        result += check_withdrawn(gobj, db,
            "TEST FAIL: OL, the first open withdrew something with the same literal", 0, json_object());
        result += check_agree(gobj, db,
            "TEST FAIL: OL, what 7.25.4 left is shown as a draft");
        if(!system_has_topic(gobj, db, "departments") || log_count(gobj, "warning") - w0 != 0) {
            result += test_fail(gobj, db,
                "TEST FAIL: OL, an open with the same literal removed what 7.25.4 left, or warned",
                json_integer(log_count(gobj, "warning") - w0));
        }
        if(edited) {
            result += edit_header(gobj, db, "departments", "name", "Operator name");
            result += check_draft_changed(gobj, db,
                "TEST FAIL: OL, an edit of what 7.25.4 left, after the upgrade, is not a draft",
                json_pack("{s:b}", "departments", 1));
        }
        close_db(gobj, db);

        w0 = log_count(gobj, "warning");
        if(open_db(gobj, db, ol_literal(db, 3), FALSE) < 0) {
            return result - 1;
        }
        result += check_withdrawn(gobj, db,
            "TEST FAIL: OL, what 7.25.4 left is not reported apart", 0,
            json_pack("{s:s, s:s}",
                "departments", edited? "unsaved" : "left_by_older_release",
                "users", "left_by_older_release"));
        result += check_agree(gobj, db, "TEST FAIL: OL, the next literal does not agree");
        if(system_has_topic(gobj, db, "departments") || log_count(gobj, "warning") - w0 != 2) {
            result += test_fail(gobj, db,
                "TEST FAIL: OL, the next literal did not remove what 7.25.4 left, or did not say it (2 warnings)",
                json_integer(log_count(gobj, "warning") - w0));
        }
        char email_id[2 * NAME_MAX];
        snprintf(email_id, sizeof(email_id), "%s.users.email", db);
        result += check_absent(gobj, db, "TEST FAIL: OL, a column 7.25.4 left stayed", "cols", email_id);
        close_db(gobj, db);

        if(open_db(gobj, db, ol_literal(db, 4), FALSE) < 0) {
            return result - 1;
        }
        result += check_withdrawn(gobj, db,
            "TEST FAIL: OL, the literal after it reported something", 0, json_object());
        close_db(gobj, db);
        drop_treedb(gobj, db);
    }

    /*
     *  The first open installs a newer literal
     */
    const char *db = "tw_ol3";
    if(ol_older_release(gobj, db) < 0) {
        return result - 1;
    }
    if(open_db(gobj, db, ol_literal(db, 3), FALSE) < 0) {
        return result - 1;
    }
    result += check_withdrawn(gobj, db,
        "TEST FAIL: OL, a first open that installs a literal did not report what 7.25.4 left apart", 0,
        json_pack("{s:s, s:s}", "departments", "left_by_older_release", "users", "left_by_older_release"));
    result += check_agree(gobj, db, "TEST FAIL: OL, a first open that installs a literal does not agree");
    close_db(gobj, db);
    drop_treedb(gobj, db);
    return result;
}

/***************************************************************************
 *  A saved-schema or save-schema answer, printed into a TEST FAIL. The
 *  schema answered, whose topics are a list: its topic `topic_name`, NULL
 *  when it has none. Return NOT YOURS
 ***************************************************************************/
PRIVATE json_t *answered_topic(hgobj gobj, json_t *jn_resp, const char *topic_name)
{
    json_t *topics = kw_get_list(gobj, jn_resp, "data`schema`topics", 0, 0);
    int idx; json_t *topic;
    json_array_foreach(topics, idx, topic) {
        if(strcmp(kw_get_str(gobj, topic, "id", "", 0), topic_name)==0) {
            return topic;
        }
    }
    return NULL;
}

/***************************************************************************
 *  OS: save-schema leaves out what an older release left. 7.25.4 opened v2
 *  over v1 (ol_older_release): `departments` and `users.email` stayed in
 *  __system__. After the upgrade, saved-schema answers `draft_changed` {},
 *  and a save:
 *
 *    - with no edit has nothing to save, and names what it left out
 *      (`left_by_older_release`); no saved schema is written;
 *    - after an edit of `users.username` publishes `users` WITHOUT
 *      `users.email`, and no `departments`: the apply puts neither back;
 *    - after an edit of the left topic `departments` (tw_ose) publishes
 *      it: the edit made it the operator's.
 *
 *  Red before: the save with no edit answered "saved ... schema_version 3"
 *  with `departments` and `users.email` in the schema, and the apply put
 *  them back in the file in use.
 ***************************************************************************/
PRIVATE int scenario_save_leaves_what_older_release_left(hgobj gobj)
{
    int result = 0;
    const char *db = "tw_os";
    char email_id[NAME_MAX];
    snprintf(email_id, sizeof(email_id), "%s.users.email", db);

    if(ol_older_release(gobj, db) < 0) {
        return -1;
    }
    if(open_db(gobj, db, ol_literal(db, 2), FALSE) < 0) {
        return -1;
    }
    result += check_draft_changed(gobj, db,
        "TEST FAIL: OS, what 7.25.4 left is shown as a draft", json_object());

    json_t *jn_resp = treedb_cmd(gobj, db, "save-schema", json_object());
    json_t *left = kw_get_list(gobj, jn_resp, "data`left_by_older_release", 0, 0);
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) != 0 ||
            !strstr(kw_get_str(gobj, jn_resp, "comment", "", 0), "nothing to save") ||
            json_array_size(left) != 4 ||
            json_list_str_index(left, email_id, FALSE) < 0 ||
            has_saved_schema(gobj, db)) {
        result += test_fail(gobj, db,
            "TEST FAIL: OS, a save with no edit published what 7.25.4 left", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)

    result += edit_header(gobj, db, "users", "username", "Operator user");
    jn_resp = treedb_cmd(gobj, db, "save-schema", json_object());
    json_t *users = answered_topic(gobj, jn_resp, "users");
    json_t *expected_versions = json_pack("{s:i}", "users", 3);
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) != 0 ||
            !users || kw_get_dict_value(gobj, users, "cols`email", 0, 0) ||
            !kw_get_dict_value(gobj, users, "cols`username", 0, 0) ||
            answered_topic(gobj, jn_resp, "departments") ||
            !json_equal(kw_get_dict(gobj, jn_resp, "data`topic_versions", 0, 0), expected_versions)) {
        result += test_fail(gobj, db,
            "TEST FAIL: OS, a save of an edit published what 7.25.4 left", json_incref(jn_resp));
    }
    JSON_DECREF(expected_versions)
    JSON_DECREF(jn_resp)
    result += apply_schema(gobj, db);
    close_db(gobj, db);

    if(open_db(gobj, db, ol_literal(db, 2), FALSE) < 0) {
        return result - 1;
    }
    json_t *file = load_schema_file(gobj, db);
    if(kw_get_int(gobj, file, "schema_version", 0, KW_WILD_NUMBER) != 3 ||
            topic_in(file, "departments") ||
            kw_get_dict_value(gobj, topic_in(file, "users"), "cols`email", 0, 0)) {
        result += test_fail(gobj, db,
            "TEST FAIL: OS, the apply put back what 7.25.4 left", json_incref(file));
    }
    JSON_DECREF(file)
    result += check_draft_changed(gobj, db,
        "TEST FAIL: OS, what 7.25.4 left is a draft after the apply", json_object());
    close_db(gobj, db);
    drop_treedb(gobj, db);

    /*
     *  An edit of what 7.25.4 left makes it the operator's: saved
     */
    db = "tw_ose";
    if(ol_older_release(gobj, db) < 0) {
        return result - 1;
    }
    if(open_db(gobj, db, ol_literal(db, 2), FALSE) < 0) {
        return result - 1;
    }
    result += edit_header(gobj, db, "departments", "name", "Operator name");
    jn_resp = treedb_cmd(gobj, db, "save-schema", json_object());
    json_t *departments = answered_topic(gobj, jn_resp, "departments");
    users = answered_topic(gobj, jn_resp, "users");
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) != 0 || !departments ||
            strcmp(kw_get_str(gobj, departments, "cols`name`header", "", 0), "Operator name")!=0 ||
            !users || kw_get_dict_value(gobj, users, "cols`email", 0, 0)) {
        result += test_fail(gobj, db,
            "TEST FAIL: OS, a save did not publish the edit of what 7.25.4 left, or published the rest",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    close_db(gobj, db);
    drop_treedb(gobj, db);
    return result;
}

/***************************************************************************
 *  DF: the file in use holds its topics as a DICT (a node opened with
 *  impose off before the draft model), and the projection comes FROM that
 *  file:
 *
 *    - A, a seed: `delete-treedb force=1` removed the projection, and the
 *      literal is not newer. `users` is projected, nothing is a draft;
 *    - B, a completion: the record of an unfinished projection says
 *      `departments` was not written, and the literal is behind the file.
 *      `departments` is written, the record goes, nothing is a draft.
 *
 *  Red before: the projector read the topics as a list only, wrote no
 *  topic and stamped the projection complete: A left `users` out of
 *  __system__, B removed the record with `departments` still missing, and
 *  both read as the operator's deletion (`draft_changed` named them).
 ***************************************************************************/
PRIVATE int topics_as_dict_in_file(hgobj gobj, const char *db)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char in_use_dir[PATH_MAX];
    build_path(in_use_dir, sizeof(in_use_dir), priv->path_database, db, NULL);
    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s.treedb_schema.json", db);
    json_t *in_use = load_json_from_file(gobj, in_use_dir, filename, 0);
    if(!in_use) {
        return test_fail(gobj, db, "TEST FAIL: DF, no schema file in use", NULL);
    }
    json_t *as_dict = json_object();
    int idx; json_t *topic;
    json_array_foreach(json_object_get(in_use, "topics"), idx, topic) {
        json_object_set(as_dict, kw_get_str(gobj, topic, "id", "", 0), topic);
    }
    json_object_set_new(in_use, "topics", as_dict);
    return save_json_to_file(gobj, in_use_dir, filename, 02770, 0660, 0, TRUE, FALSE, in_use);
}

PRIVATE int scenario_dict_file_projected(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    /*
     *  A: the seed
     */
    const char *db = "tw_dfa";
    if(open_db(gobj, db, schema_of(db, 1, json_pack("[o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return -1;
    }
    result += edit_header(gobj, db, "users", "username", "Operator user");
    result += save_schema(gobj, db);
    result += apply_schema(gobj, db);
    close_db(gobj, db);
    json_t *jn_resp = treedb_cmd(gobj, db, "delete-treedb", json_pack("{s:b}", "force", 1));
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: DF, delete-treedb failed", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    result += topics_as_dict_in_file(gobj, db);

    if(open_db(gobj, db, schema_of(db, 2, json_pack("[o]",
            topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return result - 1;
    }
    if(!system_has_topic(gobj, db, "users") ||
            !system_has_col(gobj, db, "users", "username")) {
        result += test_fail(gobj, db,
            "TEST FAIL: DF, a seed from a file whose topics are a dict projected no topic", NULL);
    }
    result += check_draft_changed(gobj, db,
        "TEST FAIL: DF, a seed from a dict file reads as a draft", json_object());
    close_db(gobj, db);
    drop_treedb(gobj, db);

    /*
     *  B: the completion
     */
    db = "tw_dfb";
    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return result - 1;
    }
    result += edit_header(gobj, db, "users", "username", "Operator user");
    result += save_schema(gobj, db);
    result += apply_schema(gobj, db);
    close_db(gobj, db);
    result += topics_as_dict_in_file(gobj, db);
    result += delete_system_topic(gobj, db, "departments");

    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), priv->path_database, "__system__", "saved_schemas", NULL);
    char record_file[NAME_MAX];
    snprintf(record_file, sizeof(record_file), "%s.unfinished.json", db);
    char departments_id[NAME_MAX], id_id[NAME_MAX], name_id[NAME_MAX];
    snprintf(departments_id, sizeof(departments_id), "%s.departments", db);
    snprintf(id_id, sizeof(id_id), "%s.departments.id", db);
    snprintf(name_id, sizeof(name_id), "%s.departments.name", db);
    save_json_to_file(gobj, dir, record_file, 02770, 0660, 0, TRUE, FALSE, json_pack(
        "{s:i, s:[], s:[s], s:[s,s,s], s:{}, s:{}}",
        "schema_version", 2,
        "not_removed",
        "not_written", departments_id,
        "leftovers", departments_id, id_id, name_id,
        "draft_kinds",
        "replaced_kinds"
    ));

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return result - 1;
    }
    json_t *record = unfinished_record(gobj, db);
    if(!system_has_topic(gobj, db, "departments") ||
            !system_has_col(gobj, db, "departments", "name") || record) {
        result += test_fail(gobj, db,
            "TEST FAIL: DF, the completion from a dict file wrote nothing, or kept its record",
            record? record : json_null());
        record = NULL;
    }
    JSON_DECREF(record)
    result += check_draft_changed(gobj, db,
        "TEST FAIL: DF, a completion from a dict file reads as a draft", json_object());
    close_db(gobj, db);
    drop_treedb(gobj, db);
    return result;
}

/***************************************************************************
 *  LQ: the FIRST open by this release of a projection keyed by rowid (from
 *  before 7.13.2), with a schema file in use (`users` id, username) and a
 *  topic an older release left (`departments`, no schema declares it):
 *
 *    - the first open moves the ids FIRST, then reads what an older
 *      release left: no WARNING (nothing is removed), `draft_changed` {}
 *      -- `users`, the file's topic, included: a node written before
 *      `order` existed says nothing about its place, and that is no
 *      reorder --, and the record of the upgrade names `departments` at its
 *      qualified ids and no rowid id;
 *    - a save keeps the order of the file: the columns of `users` (id,
 *      username, zeta, alpha) and the topics (users, groups), none of
 *      them alphabetical. Every node says `order` 9999, which is no
 *      position: each goes where the FILE IN USE declares it;
 *    - the next literal removes `departments` and reports it as
 *      `left_by_older_release`, never "unsaved".
 *
 *  Red before (the order): the save sorted by the 9999 of every node, a
 *  tie, so the nodes kept the order the store loaded them in --
 *  alphabetical by id -- and the saved schema moved every column and
 *  topic, a reorder nobody made, that apply-schema put in the file.
 *
 *  Red before: the record was built on the rowid ids, before the move of
 *  the same open. Every legacy node was "left" (the declared `users`
 *  too), the removal of all of them was said in a false WARNING, the
 *  record was written empty, and `departments` read as a draft, withdrawn
 *  "unsaved" by the next literal. And `users` read as a draft: the default
 *  `order` (9999) of its nodes against the position a projection writes
 *  today, so the next literal withdrew it "unsaved" too.
 ***************************************************************************/
PRIVATE json_t *lq_literal(const char *db, int v)
{
    return schema_of(db, v, json_pack("[o,o]",
        topic_of("users", v, json_pack("{s:o, s:o, s:o, s:o}",
            "id", col_id(), "username", col_str("Login"),
            "zeta", col_str("Zeta"), "alpha", col_str("Alpha"))),
        topic_of("groups", v, json_pack("{s:o}", "id", col_id()))));
}

/*
 *  What the legacy projection holds beyond build_legacy_projection(): the
 *  columns zeta and alpha of `users`, and the topic `groups`, as the
 *  projector of 7.13.1 wrote them
 */
PRIVATE int build_lq_legacy_nodes(hgobj gobj, const char *db)
{
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);
    int result = 0;
    char users_rowid[NAME_MAX], groups_rowid[NAME_MAX];
    snprintf(users_rowid, sizeof(users_rowid), "%s-7", db);
    snprintf(groups_rowid, sizeof(groups_rowid), "%s-9", db);

    json_t *treedb = gobj_get_node(sys, "treedbs", json_pack("{s:s}", "id", db),
        json_pack("{s:b}", "refs", 1), gobj);
    json_t *groups = gobj_create_node(sys, "topics",
        json_pack("{s:s, s:s, s:s, s:s, s:s, s:i, s:b}", "id", groups_rowid, "value", "groups",
            "pkey", "id", "system_flag", "sf_string_key", "tkey", "", "topic_version", 1,
            "system_topic", 0),
        json_pack("{s:b}", "refs", 1), gobj);
    if(!treedb || !groups ||
            gobj_link_nodes(sys, "topics", "treedbs", json_incref(treedb), "topics",
                json_incref(groups), gobj) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: LQ, the legacy topic groups could not be made", NULL);
    }
    static const char *cols[][5] = {
        {"73", "zeta",  "Zeta",  "",         "-7"},
        {"74", "alpha", "Alpha", "",         "-7"},
        {"90", "id",    "Id",    "required", "-9"},
        {NULL, NULL, NULL, NULL, NULL}
    };
    for(int i = 0; cols[i][0]; i++) {
        char rowid[NAME_MAX], topic_rowid[NAME_MAX];
        snprintf(rowid, sizeof(rowid), "%s-%s", db, cols[i][0]);
        snprintf(topic_rowid, sizeof(topic_rowid), "%s%s", db, cols[i][4]);
        json_t *flag = empty_string(cols[i][3])?
            json_pack("[s]", "persistent") : json_pack("[s,s]", "persistent", cols[i][3]);
        json_t *col = gobj_create_node(sys, "cols",
            json_pack("{s:s, s:s, s:s, s:s, s:i, s:o}", "id", rowid, "value", cols[i][1],
                "header", cols[i][2], "type", "string", "fillspace", 10, "flag", flag),
            json_pack("{s:b}", "refs", 1), gobj);
        if(!col || gobj_link_nodes(sys, "cols", "topics",
                json_pack("{s:s}", "id", topic_rowid), "cols", json_incref(col), gobj) < 0) {
            result += test_fail(gobj, db, "TEST FAIL: LQ, a legacy column could not be made", NULL);
        }
        JSON_DECREF(col)
    }
    JSON_DECREF(groups)
    JSON_DECREF(treedb)
    return result;
}

/*
 *  The names of a saved schema, in its order: the topics when `topic_name`
 *  is NULL, the columns of that topic otherwise, joined by ", "
 */
PRIVATE void saved_names(json_t *schema, const char *topic_name, char *bf, size_t bfsize)
{
    bf[0] = 0;
    size_t len = 0;
    int idx; json_t *topic;
    json_array_foreach(json_object_get(schema, "topics"), idx, topic) {
        const char *name = json_string_value(json_object_get(topic, "id"));
        if(!name) {
            continue;
        }
        if(!topic_name) {
            len += (size_t)snprintf(bf + len, bfsize - len, "%s%s", len? ", " : "", name);
            if(len >= bfsize) {
                return;
            }
            continue;
        }
        if(strcmp(name, topic_name)!=0) {
            continue;
        }
        const char *col_name; json_t *col;
        json_object_foreach(json_object_get(topic, "cols"), col_name, col) {
            len += (size_t)snprintf(bf + len, bfsize - len, "%s%s", len? ", " : "", col_name);
            if(len >= bfsize) {
                return;
            }
        }
    }
}

PRIVATE int scenario_legacy_ids_before_the_upgrade_record(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    const char *db = "tw_lq";
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    if(write_schema_file(gobj, db, lq_literal(db, 1)) < 0 ||
            build_legacy_projection(gobj, db) < 0 || build_lq_legacy_nodes(gobj, db) < 0) {
        return -1;
    }
    char tid[NAME_MAX], cid[NAME_MAX];
    snprintf(tid, sizeof(tid), "%s-8", db);
    snprintf(cid, sizeof(cid), "%s-80", db);
    json_t *treedb = gobj_get_node(sys, "treedbs", json_pack("{s:s}", "id", db),
        json_pack("{s:b}", "refs", 1), gobj);
    json_t *topic = gobj_create_node(sys, "topics",
        json_pack("{s:s, s:s, s:s, s:s, s:s, s:i, s:b}", "id", tid, "value", "departments",
            "pkey", "id", "system_flag", "sf_string_key", "tkey", "", "topic_version", 1,
            "system_topic", 0),
        json_pack("{s:b}", "refs", 1), gobj);
    json_t *col = gobj_create_node(sys, "cols",
        json_pack("{s:s, s:s, s:s, s:s, s:i, s:[s]}", "id", cid, "value", "name",
            "header", "Name", "type", "string", "fillspace", 10, "flag", "persistent"),
        json_pack("{s:b}", "refs", 1), gobj);
    if(!treedb || !topic || !col ||
            gobj_link_nodes(sys, "topics", "treedbs", json_incref(treedb), "topics",
                json_incref(topic), gobj) < 0 ||
            gobj_link_nodes(sys, "cols", "topics", json_incref(topic), "cols",
                json_incref(col), gobj) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: LQ, the legacy topic could not be made", NULL);
    }
    JSON_DECREF(col)
    JSON_DECREF(topic)
    JSON_DECREF(treedb)
    if(result < 0) {
        return result;
    }
    restart_system(gobj);

    json_int_t w0 = log_count(gobj, "warning");
    if(open_db(gobj, db, lq_literal(db, 1), FALSE) < 0) {
        return result - 1;
    }
    if(log_count(gobj, "warning") - w0 != 0) {
        result += test_fail(gobj, db,
            "TEST FAIL: LQ, the first open of a rowid projection warned",
            json_integer(log_count(gobj, "warning") - w0));
    }
    {
        json_t *rows = treedb_cmd(gobj, db, "save-schema", json_pack("{s:b}", "dry_run", 1));
        json_t *saved = treedb_cmd(gobj, db, "saved-schema", json_object());
        json_t *draft_changed = kw_get_dict(gobj, saved, "data`draft_changed", 0, 0);
        if(!draft_changed || json_object_size(draft_changed) > 0) {
            result += test_fail(gobj, db,
                "TEST FAIL: LQ, what an older release left in a rowid projection reads as a draft",
                json_pack("{s:O, s:O}", "saved_schema", saved, "save_dry_run", rows));
        }
        JSON_DECREF(saved)
        JSON_DECREF(rows)
    }


    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), priv->path_database, "__system__", "saved_schemas", NULL);
    char fn[NAME_MAX];
    snprintf(fn, sizeof(fn), "%s.upgrade.json", db);
    json_t *upgrade = load_json_from_file(gobj, dir, fn, 0);
    json_t *left = json_object_get(upgrade, "left_by_older_release");
    char departments_id[NAME_MAX];
    snprintf(departments_id, sizeof(departments_id), "%s.departments", db);
    BOOL rowid_named = FALSE;
    int idx; json_t *jn_id;
    json_array_foreach(left, idx, jn_id) {
        if(strchr(json_string_value(jn_id), '-')) {
            rowid_named = TRUE;
        }
    }
    if(json_list_str_index(left, departments_id, FALSE) < 0 || rowid_named) {
        result += test_fail(gobj, db,
            "TEST FAIL: LQ, the record of the upgrade does not name what was left at its qualified id",
            upgrade? json_incref(upgrade) : NULL);
    }
    JSON_DECREF(upgrade)
    close_db(gobj, db);

    restart_system(gobj);
    if(open_db(gobj, db, lq_literal(db, 1), FALSE) < 0) {
        return result - 1;
    }
    /*
     *  A save keeps the order of the file (dry run: an edit, the schema it
     *  would write, and the edit taken back). After a restart: the store
     *  loads the nodes sorted by id, not in the order the move wrote them
     */
    result += edit_header(gobj, db, "users", "username", "Edited");
    {
        json_t *rows = treedb_cmd(gobj, db, "save-schema", json_pack("{s:b}", "dry_run", 1));
        json_t *schema = kw_get_dict(gobj, rows, "data`schema", 0, 0);
        char topics[256], cols[256];
        saved_names(schema, NULL, topics, sizeof(topics));
        saved_names(schema, "users", cols, sizeof(cols));
        if(strcmp(topics, "users, groups")!=0 || strcmp(cols, "id, username, zeta, alpha")!=0) {
            result += test_fail(gobj, db,
                "TEST FAIL: LQ, a save of a projection written before `order` existed reorders it",
                json_pack("{s:s, s:s, s:O}", "topics", topics, "users_cols", cols,
                    "save_dry_run", rows));
        }
        JSON_DECREF(rows)
    }
    result += edit_header(gobj, db, "users", "username", "Login");
    close_db(gobj, db);

    if(open_db(gobj, db, lq_literal(db, 2), FALSE) < 0) {
        return result - 1;
    }
    /*
     *  `users` too: its column `operator_col` is no schema's, so an older
     *  release left it; the rest of `users` is the file
     */
    result += check_withdrawn(gobj, db,
        "TEST FAIL: LQ, the next literal reported what an older release left as operator work", 0,
        json_pack("{s:s, s:s}",
            "departments", "left_by_older_release",
            "users", "left_by_older_release"));
    close_db(gobj, db);
    drop_treedb(gobj, db);
    return result;
}

/***************************************************************************
 *  LX: the move of a rowid projection to qualified ids FAILS half way
 *  (the directory of the keys of `cols` is read-only: no qualified column
 *  can be created), at an open that installs a newer literal. The open
 *  projects NOTHING: the meta-schema version of the treedb node stays the
 *  old one, the projection is unfinished (save-schema refuses). With the
 *  directory writable again, the next open completes the move: no legacy
 *  id is left, the topic is at its qualified id, and the node says the
 *  meta-schema of today.
 *
 *  Red before: the move answered the nodes it moved even with failures,
 *  and the projection that followed stamped the meta-schema version (its
 *  reset on a failure too): the next open never moved what was left.
 ***************************************************************************/
PRIVATE json_int_t system_meta_version(hgobj gobj, const char *treedb_name)
{
    json_t *node = system_node(gobj, "treedbs", treedb_name);
    json_int_t v = node? kw_get_int(gobj, node, "system_schema_version", -1, KW_WILD_NUMBER) : -1;
    JSON_DECREF(node)
    return v;
}

PRIVATE int scenario_legacy_move_that_fails(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    const char *db = "tw_lx";

    if(write_schema_file(gobj, db, lg_literal(db)) < 0 || build_legacy_projection(gobj, db) < 0) {
        return -1;
    }
    restart_system(gobj);

    char keys_dir[PATH_MAX];
    build_path(keys_dir, sizeof(keys_dir), priv->path_database, "__system__", "cols", "keys", NULL);
    struct stat st;
    if(stat(keys_dir, &st) < 0) {
        return test_fail(gobj, db, "TEST FAIL: LX, the keys directory cannot be read",
            json_string(keys_dir));
    }
    json_t *lit2 = schema_of(db, 2, json_pack("[o]",
        topic_of("users", 2, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("Login")))));

    result += chmod_system_key(gobj, db, "cols", NULL, 0550);
    json_t *jn_resp = open_db_resp(gobj, db, json_incref(lit2));
    JSON_DECREF(jn_resp)
    json_int_t meta = system_meta_version(gobj, db);
    if(meta != 1) {
        result += test_fail(gobj, db,
            "TEST FAIL: LX, an open whose move of ids failed stamped the meta-schema version",
            json_integer(meta));
    }
    result += check_save_refused(gobj, db,
        "TEST FAIL: LX, save-schema did not refuse while the move of ids is not complete");
    close_db(gobj, db);
    if(chmod(keys_dir, st.st_mode & 07777) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: LX, chmod back failed", json_string(keys_dir));
    }
    restart_system(gobj);

    /*
     *  The retry is the first open that writes the record of the upgrade,
     *  and the record the failed move left names no id: `operator_col`,
     *  declared by no schema, is what an older release left, never the
     *  operator's work (and no error): removed and said so, two warnings. Red before: the record, with no
     *  `planned`, "named" every id (an ERROR each time it was asked), so
     *  `operator_col` was withdrawn "unsaved".
     */
    result += open_counting(gobj, db, lit2,
        "TEST FAIL: LX, the open that completes the move logged errors, or said other than what "
        "an older release left", 0, 2);
    result += check_withdrawn(gobj, db,
        "TEST FAIL: LX, what an older release left was withdrawn as operator work", 0,
        json_pack("{s:s}", "users", "left_by_older_release"));
    static const char *legacy[][2] = {
        {"topics", "-7"}, {"cols", "-70"}, {"cols", "-71"}, {"cols", "-72"}, {NULL, NULL}
    };
    for(int i = 0; legacy[i][0]; i++) {
        char id[NAME_MAX];
        snprintf(id, sizeof(id), "%s%s", db, legacy[i][1]);
        result += check_absent(gobj, db, "TEST FAIL: LX, a legacy node stayed", legacy[i][0], id);
    }
    char topic_id[NAME_MAX], ref[NAME_MAX + 16];
    snprintf(topic_id, sizeof(topic_id), "%s.users", db);
    snprintf(ref, sizeof(ref), "treedbs^%s^topics", db);
    json_t *record = unfinished_record(gobj, db);
    meta = system_meta_version(gobj, db);
    if(!system_node_links(gobj, "topics", topic_id, "treedbs", ref) || record || meta <= 1) {
        result += test_fail(gobj, db,
            "TEST FAIL: LX, the next open did not complete the move, or left the record",
            json_pack("{s:I, s:O}", "system_schema_version", meta, "record", record? record : json_null()));
    }
    JSON_DECREF(record)
    result += check_agree(gobj, db, "TEST FAIL: LX, the open that completes the move does not agree");
    close_db(gobj, db);
    drop_treedb(gobj, db);
    return result;
}

/***************************************************************************
 *  SV: a literal changed WITHOUT raising its schema_version (a column
 *  added) over a file that came from an earlier literal of the same
 *  number. The file runs, and it is said: ONE WARNING, at every open until
 *  the literal moves on; nothing when the literal IS the file.
 *
 *  Red before: the difference was compared only when __system__'s
 *  c_schema_version was another number, and here it is that number:
 *  nothing was said, and the column reached nothing.
 ***************************************************************************/
PRIVATE json_t *sv_literal(const char *db, BOOL with_email)
{
    json_t *cols = json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User"));
    if(with_email) {
        json_object_set_new(cols, "email", col_str("Email"));
    }
    return schema_of(db, 2, json_pack("[o]", topic_of("users", 1, cols)));
}

PRIVATE int scenario_same_version_other_content(hgobj gobj)
{
    int result = 0;
    const char *db = "tw_sv";
    if(open_db(gobj, db, sv_literal(db, FALSE), FALSE) < 0) {
        return -1;
    }
    close_db(gobj, db);
    for(int i = 0; i < 2; i++) {
        result += open_counting(gobj, db, sv_literal(db, TRUE),
            "TEST FAIL: SV, a literal with another content under the same number was not said",
            0, 1);
        close_db(gobj, db);
    }
    result += open_counting(gobj, db, sv_literal(db, FALSE),
        "TEST FAIL: SV, the literal that IS the file was said", 0, 0);
    close_db(gobj, db);
    drop_treedb(gobj, db);
    return result;
}

/***************************************************************************
 *  SVI: SV with `impose_c_schema`. An imposed literal installs nothing at
 *  the schema_version of the file (a tie goes to the file, see
 *  treedb_open_db()): the FILE runs, and it is said as without impose,
 *  ONE WARNING at every open, nothing when the literal IS the file. And a
 *  seed of __system__ (after delete-treedb) is made from the file then,
 *  not from the literal that does not run: no draft, and its
 *  c_schema_version is not the literal's.
 *
 *  Red before: nothing was said, and the seed projected the literal: its
 *  column read as a draft over the file, that a save would publish.
 ***************************************************************************/
PRIVATE int scenario_imposed_same_version_other_content(hgobj gobj)
{
    int result = 0;
    const char *db = "tw_svi";
    if(open_db(gobj, db, sv_literal(db, FALSE), TRUE) < 0) {
        return -1;
    }
    close_db(gobj, db);
    for(int i = 0; i < 2; i++) {
        result += open_counting_as(gobj, db, sv_literal(db, TRUE), TRUE,
            "TEST FAIL: SVI, an imposed literal with another content under the same number was not said",
            0, 1);
        close_db(gobj, db);
    }
    result += open_counting_as(gobj, db, sv_literal(db, FALSE), TRUE,
        "TEST FAIL: SVI, the imposed literal that IS the file was said", 0, 0);
    close_db(gobj, db);

    json_t *jn_resp = treedb_cmd(gobj, db, "delete-treedb", json_pack("{s:b}", "force", 1));
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: SVI, delete-treedb", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    result += open_counting_as(gobj, db, sv_literal(db, TRUE), TRUE,
        "TEST FAIL: SVI, the seed at an imposed tie was not said", 0, 1);
    result += check_draft_changed(gobj, db,
        "TEST FAIL: SVI, the seed at an imposed tie projected the literal that does not run",
        json_object());
    if(system_c_schema_version(gobj, db) == 2) {
        result += test_fail(gobj, db,
            "TEST FAIL: SVI, a seed from the file at an imposed tie claims the literal", NULL);
    }
    close_db(gobj, db);
    drop_treedb(gobj, db);
    return result;
}

/***************************************************************************
 *  OP: a column the operator adds with an `order` that is not its place
 *  (99, the third column of `users`). It is a draft. A save publishes it
 *  third, and writes that place into __system__ too: right after the save
 *  `draft_changed` is {} (the draft IS the saved schema), and applied and
 *  opened, the file runs it and `draft_changed` is {} again.
 *
 *  Red before: the save wrote the column third and left 99 in __system__.
 *  Compared with the saved schema (99 against 2), `users` read as unsaved
 *  right after its save, and as a draft over the file after the apply.
 ***************************************************************************/
PRIVATE json_t *op_literal(const char *db)
{
    return schema_of(db, 1, json_pack("[o]",
        topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))));
}

PRIVATE int scenario_draft_order_is_not_its_place(hgobj gobj)
{
    int result = 0;
    const char *db = "tw_op";
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    if(open_db(gobj, db, op_literal(db), FALSE) < 0) {
        return -1;
    }
    result += add_draft_col(gobj, db, "users", "email");
    json_t *ed = gobj_update_node(sys, "cols",
        json_pack("{s:s, s:i}", "id", "tw_op.users.email", "order", 99),
        json_pack("{s:b}", "refs", 1), gobj);
    if(!ed) {
        result += test_fail(gobj, db, "TEST FAIL: OP, the operator's order was refused", NULL);
    }
    JSON_DECREF(ed)
    result += check_draft_changed(gobj, db, "TEST FAIL: OP, the operator's column is not a draft",
        json_pack("{s:b}", "users", 1));
    result += save_schema(gobj, db);
    result += check_draft_changed(gobj, db,
        "TEST FAIL: OP, a draft whose order is not its place reads as unsaved right after its save",
        json_object());
    result += apply_schema(gobj, db);
    close_db(gobj, db);

    if(open_db(gobj, db, op_literal(db), FALSE) < 0) {
        return result - 1;
    }
    result += check_draft_changed(gobj, db,
        "TEST FAIL: OP, the applied draft reads as a draft over the file", json_object());
    close_db(gobj, db);
    drop_treedb(gobj, db);
    return result;
}

/***************************************************************************
 *  OM: the operator MOVES the column `departments.name` to `users` (as in
 *  MV) and saves. The node keeps its id, `tw_om.departments.name`, and is
 *  saved third in `users`: the save writes that place into the node it
 *  found by NAME, as the diff finds it. Right after the save
 *  `draft_changed` is {}; applied and opened, {} again, the store runs
 *  `name` in `users`, and a second save has nothing to save.
 *
 *  Red before: the place was written to the id composed from the names,
 *  `tw_om.users.name`, which is no node. The moved column kept `order` 1
 *  (tied with `username`), so `users` read as unsaved right after its
 *  save and as a draft after the apply, and every later save published it
 *  again.
 ***************************************************************************/
PRIVATE int scenario_moved_col_saved(hgobj gobj)
{
    int result = 0;
    const char *db = "tw_om";
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    if(gobj_unlink_nodes(sys, "cols",
            "topics", json_pack("{s:s}", "id", "tw_om.departments"),
            "cols", json_pack("{s:s}", "id", "tw_om.departments.name"), gobj) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: OM, the operator's unlink was refused", NULL);
    }
    if(gobj_link_nodes(sys, "cols",
            "topics", json_pack("{s:s}", "id", "tw_om.users"),
            "cols", json_pack("{s:s}", "id", "tw_om.departments.name"), gobj) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: OM, the operator's link was refused", NULL);
    }
    result += check_draft_changed(gobj, db, "TEST FAIL: OM, the move is not a draft",
        json_pack("{s:b, s:b}", "departments", 1, "users", 1));
    result += save_schema(gobj, db);
    result += check_draft_changed(gobj, db,
        "TEST FAIL: OM, a moved column reads as unsaved right after its save", json_object());
    result += apply_schema(gobj, db);
    close_db(gobj, db);

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return result - 1;
    }
    result += check_draft_changed(gobj, db,
        "TEST FAIL: OM, the applied move reads as a draft over the file", json_object());
    json_int_t tv;
    json_t *h = running_headers(gobj, db, "users", &tv);
    if(!json_object_get(h, "name")) {
        result += test_fail(gobj, db, "TEST FAIL: OM, the apply of the move reached nothing",
            json_pack("{s:I, s:O}", "topic_version", tv, "headers", h? h : json_null()));
    }
    JSON_DECREF(h)
    json_t *jn_resp = treedb_cmd(gobj, db, "save-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 ||
            !strstr(kw_get_str(gobj, jn_resp, "comment", "", 0), "nothing to save") ||
            kw_get_dict(gobj, jn_resp, "data`topic_versions", 0, 0)) {
        result += test_fail(gobj, db, "TEST FAIL: OM, a second save published the move again",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    close_db(gobj, db);
    drop_treedb(gobj, db);
    return result;
}

/***************************************************************************
 *  A save of nothing: "nothing to save", no topic_versions
 ***************************************************************************/
PRIVATE int check_nothing_to_save(hgobj gobj, const char *treedb_name, const char *label)
{
    json_t *jn_resp = treedb_cmd(gobj, treedb_name, "save-schema", json_object());
    int ret = 0;
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 ||
            !strstr(kw_get_str(gobj, jn_resp, "comment", "", 0), "nothing to save") ||
            kw_get_dict(gobj, jn_resp, "data`topic_versions", 0, 0)) {
        ret = test_fail(gobj, treedb_name, label, json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    return ret;
}

/***************************************************************************
 *  The save of a draft: the topics it publishes (`topic_versions`, owned)
 *  and the nodes it does not place (`not_placed`, owned, [id, ...]: what
 *  `places_not_written` must say)
 ***************************************************************************/
PRIVATE int save_publishing(hgobj gobj, const char *treedb_name, const char *label,
    json_t *topic_versions, // owned
    json_t *not_placed)     // owned
{
    json_t *jn_resp = treedb_cmd(gobj, treedb_name, "save-schema", json_object());
    int ret = 0;
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 ||
            !json_equal(kw_get_dict(gobj, jn_resp, "data`topic_versions", 0, 0), topic_versions) ||
            !json_equal(kw_get_list(gobj, jn_resp, "data`places_not_written", 0, 0), not_placed)) {
        ret = test_fail(gobj, treedb_name, label,
            json_pack("{s:O, s:O, s:O}",
                "expected_topic_versions", topic_versions,
                "expected_places_not_written", not_placed,
                "save_schema", jn_resp));
    }
    JSON_DECREF(jn_resp)
    JSON_DECREF(topic_versions)
    JSON_DECREF(not_placed)
    return ret;
}

/***************************************************************************
 *  SC: a column with TWO parents. The operator links
 *  `tw_sc.departments.name` to `users` TOO (the meta-schema's fkeys are
 *  lists: a node may hang from several parents, and NP's open takes it
 *  back only when a newer literal comes). The save publishes `users`, and
 *  says the column in `places_not_written`: its one `order` cannot say its
 *  place in both topics, so it keeps the one it has, and it is compared
 *  by no save. Right after the save `draft_changed` is {}, a save again
 *  publishes the same (`users` alone, at the same topic_version), and
 *  after the apply `draft_changed` is {} and a save has nothing to save.
 *
 *  ST: a topic with TWO parents. The operator links `tw_sb.extra` into
 *  `tw_sa`; it stays in `tw_sb` too. The save of `tw_sa` publishes it and
 *  does not place it; neither treedb reads a draft afterwards, a save of
 *  `tw_sa` again publishes `extra` alone, and `tw_sb` has nothing to save.
 *
 *  Red before: each save wrote the node's place in ITS parent, and the
 *  other parent read the node as moved. SC: saves of `tw_sc` flipped
 *  between `{users}` and `{users, departments}` for ever, raising the
 *  topic_versions of topics nobody edited. ST: each treedb's save made the
 *  other read `extra` as unsaved, and a save of `tw_sa` re-sorted
 *  `departments` too.
 ***************************************************************************/
PRIVATE int scenario_node_of_two_parents(hgobj gobj)
{
    int result = 0;
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    /*
     *  SC
     */
    const char *db = "tw_sc";
    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    if(gobj_link_nodes(sys, "cols",
            "topics", json_pack("{s:s}", "id", "tw_sc.users"),
            "cols", json_pack("{s:s}", "id", "tw_sc.departments.name"), gobj) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: SC, the operator's link was refused", NULL);
    }
    result += check_draft_changed(gobj, db, "TEST FAIL: SC, the second parent is not a draft",
        json_pack("{s:b}", "users", 1));
    for(int i = 0; i < 3; i++) {
        result += save_publishing(gobj, db,
            "TEST FAIL: SC, a save of a column of two parents published other topics, "
            "or did not say the column",
            json_pack("{s:i}", "users", 2),
            json_pack("[s]", "tw_sc.departments.name"));
        result += check_draft_changed(gobj, db,
            "TEST FAIL: SC, a column of two parents reads as unsaved right after its save",
            json_object());
    }
    result += apply_schema(gobj, db);
    close_db(gobj, db);
    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return result - 1;
    }
    result += check_draft_changed(gobj, db,
        "TEST FAIL: SC, the applied column of two parents reads as a draft", json_object());
    result += check_nothing_to_save(gobj, db,
        "TEST FAIL: SC, a save after the apply published the column of two parents again");
    close_db(gobj, db);
    drop_treedb(gobj, db);

    /*
     *  ST
     */
    if(open_db(gobj, "tw_sa", users_departments_v1("tw_sa"), FALSE) < 0) {
        return result - 1;
    }
    json_t *sb = schema_of("tw_sb", 1, json_pack("[o,o]",
        topic_of("extra", 1, json_pack("{s:o}", "id", col_id())),
        topic_of("other", 1, json_pack("{s:o}", "id", col_id()))));
    if(open_db(gobj, "tw_sb", sb, FALSE) < 0) {
        close_db(gobj, "tw_sa");
        return result - 1;
    }
    if(gobj_link_nodes(sys, "topics",
            "treedbs", json_pack("{s:s}", "id", "tw_sa"),
            "topics", json_pack("{s:s}", "id", "tw_sb.extra"), gobj) < 0) {
        result += test_fail(gobj, "tw_sa", "TEST FAIL: ST, the operator's link was refused", NULL);
    }
    result += check_draft_changed(gobj, "tw_sa", "TEST FAIL: ST, the linked topic is not a draft",
        json_pack("{s:b}", "extra", 1));
    for(int i = 0; i < 3; i++) {
        result += save_publishing(gobj, "tw_sa",
            "TEST FAIL: ST, a save of a topic of two parents published other topics, "
            "or did not say the topic",
            json_pack("{s:i}", "extra", 1),
            json_pack("[s]", "tw_sb.extra"));
        result += check_draft_changed(gobj, "tw_sa",
            "TEST FAIL: ST, a topic of two parents reads as unsaved right after its save",
            json_object());
        result += check_draft_changed(gobj, "tw_sb",
            "TEST FAIL: ST, the save of one parent made the other read the topic as moved",
            json_object());
        result += check_nothing_to_save(gobj, "tw_sb",
            "TEST FAIL: ST, the other parent published the topic of two parents");
    }
    close_db(gobj, "tw_sa");
    close_db(gobj, "tw_sb");
    drop_treedb(gobj, "tw_sa");
    drop_treedb(gobj, "tw_sb");
    return result;
}

/***************************************************************************
 *  DT: two topics with ONE name in one treedb. `tw_da` and `tw_db` both
 *  have `users`:
 *
 *    - linking `tw_db.users` into `tw_da` is refused, with ONE ERROR
 *      ("Treedb already has a topic with this name"), as a second column
 *      of one name in a topic is;
 *    - the same pair made by an autolink update, which the link guard
 *      does not see, is refused by save-schema: -1, naming both ids in
 *      the comment and in `data.twins`, with ONE ERROR, and nothing
 *      saved;
 *    - unlinked again, the save goes.
 *
 *  Red before: the link was accepted, `saved-schema` said nothing of it,
 *  and a save of an edit of `departments` published `tw_db`'s `users` (a
 *  schema is keyed by name: the rebuild kept the last of the two, the
 *  diff found the first) at the topic_version `tw_da`'s ran, so the apply
 *  reached nothing and nobody was told.
 ***************************************************************************/
PRIVATE int scenario_two_topics_of_one_name(hgobj gobj)
{
    int result = 0;
    hgobj sys = gobj_find_service(SYSTEM_TREEDB, FALSE);

    if(open_db(gobj, "tw_da", users_departments_v1("tw_da"), FALSE) < 0) {
        return -1;
    }
    if(open_db(gobj, "tw_db", users_departments_v1("tw_db"), FALSE) < 0) {
        close_db(gobj, "tw_da");
        return -1;
    }
    result += edit_header(gobj, "tw_db", "users", "username", "From B");

    json_int_t e0 = log_count(gobj, "error");
    int ret = gobj_link_nodes(sys, "topics",
        "treedbs", json_pack("{s:s}", "id", "tw_da"),
        "topics", json_pack("{s:s}", "id", "tw_db.users"), gobj);
    json_int_t e = log_count(gobj, "error") - e0;
    if(ret >= 0 || e != 1) {
        result += test_fail(gobj, "tw_da",
            "TEST FAIL: DT, a second topic of one name was linked into the treedb",
            json_pack("{s:i, s:I}", "ret", ret, "errors", e));
    }

    json_t *node = gobj_update_node(sys, "topics",
        json_pack("{s:s, s:[s,s]}",
            "id", "tw_db.users",
            "treedbs", "treedbs^tw_db^topics", "treedbs^tw_da^topics"),
        json_pack("{s:b}", "autolink", 1),
        gobj);
    if(!node) {
        result += test_fail(gobj, "tw_da", "TEST FAIL: DT, cannot set up the twin topics", NULL);
    }
    JSON_DECREF(node)
    result += edit_header(gobj, "tw_da", "departments", "name", "A edit");

    e0 = log_count(gobj, "error");
    json_t *jn_resp = treedb_cmd(gobj, "tw_da", "save-schema", json_object());
    e = log_count(gobj, "error") - e0;
    const char *comment = kw_get_str(gobj, jn_resp, "comment", "", 0);
    const char *first = kw_get_str(gobj, jn_resp, "data`twins`first", "", 0);
    const char *second = kw_get_str(gobj, jn_resp, "data`twins`second", "", 0);
    BOOL both = (strcmp(first, "tw_da.users")==0 && strcmp(second, "tw_db.users")==0) ||
        (strcmp(first, "tw_db.users")==0 && strcmp(second, "tw_da.users")==0);
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0 || e != 1 || !both ||
            !strstr(comment, "tw_da.users") || !strstr(comment, "tw_db.users")) {
        result += test_fail(gobj, "tw_da",
            "TEST FAIL: DT, a save of two topics of one name was not refused naming both",
            json_pack("{s:O, s:I}", "save_schema", jn_resp, "errors", e));
    }
    JSON_DECREF(jn_resp)
    jn_resp = treedb_cmd(gobj, "tw_da", "saved-schema", json_object());
    if(kw_get_bool(gobj, jn_resp, "data`saved", 0, 0)) {
        result += test_fail(gobj, "tw_da", "TEST FAIL: DT, the refused save wrote a saved schema",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)

    if(gobj_unlink_nodes(sys, "topics",
            "treedbs", json_pack("{s:s}", "id", "tw_da"),
            "topics", json_pack("{s:s}", "id", "tw_db.users"), gobj) < 0) {
        result += test_fail(gobj, "tw_da", "TEST FAIL: DT, cannot unlink the twin", NULL);
    }
    jn_resp = treedb_cmd(gobj, "tw_da", "save-schema", json_object());
    json_t *tv = kw_get_dict(gobj, jn_resp, "data`topic_versions", 0, 0);
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 ||
            !json_object_get(tv, "departments") || json_object_get(tv, "users")) {
        result += test_fail(gobj, "tw_da", "TEST FAIL: DT, the save of the treedb's own topics failed",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)

    close_db(gobj, "tw_da");
    close_db(gobj, "tw_db");
    drop_treedb(gobj, "tw_da");
    drop_treedb(gobj, "tw_db");
    return result;
}

/***************************************************************************
 *  KS: an open that installs a newer literal and does NOT open keeps the
 *  saved schema. The operator saved an edit of `users` (saved schema 2,
 *  over the file 1). A literal 3 with no `topics` is installed by the
 *  reconcile and refused by treedb_open_db() ("No topics found"), which
 *  wrote it over the file first. The saved schema stays, the operator's
 *  work: an INFO says it, `withdrawn_at_open` says `saved_schema_version`
 *  0, and `saved-schema` answers it `stale` (below the broken file 3, it
 *  cannot be applied). The operator puts the file 1 back and rolls the
 *  literal back to 1: the save is pending again, and apply-schema installs
 *  it.
 *
 *  Red before: the reconcile removed the saved schema BEFORE
 *  treedb_open_db() ran, and the refused literal took it with it.
 ***************************************************************************/
PRIVATE int scenario_failed_open_keeps_the_save(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    const char *db = "tw_ks";

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    result += edit_header(gobj, db, "users", "username", "Operator user");
    result += save_schema(gobj, db);
    close_db(gobj, db);

    json_t *jn_resp = open_db_resp(gobj, db, json_pack("{s:s, s:i}", "id", db, "schema_version", 3));
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0) {
        result += test_fail(gobj, db, "TEST FAIL: KS, the literal with no topics opened",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)

    char saved_dir[PATH_MAX];
    build_path(saved_dir, sizeof(saved_dir), priv->path_database, "__system__", "saved_schemas", NULL);
    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s.treedb_schema.json", db);
    json_t *saved = file_exists(saved_dir, filename)?
        load_json_from_file(gobj, saved_dir, filename, 0) : NULL;
    if(kw_get_int(gobj, saved, "schema_version", 0, KW_WILD_NUMBER) != 2) {
        result += test_fail(gobj, db, "TEST FAIL: KS, an open that did not open withdrew the saved schema",
            saved? json_incref(saved) : NULL);
    }
    JSON_DECREF(saved)
    result += check_withdrawn(gobj, db,
        "TEST FAIL: KS, an open that did not open says the saved schema withdrawn",
        0, json_pack("{s:s}", "users", "saved"));
    jn_resp = treedb_cmd(gobj, db, "saved-schema", json_object());
    if(kw_get_bool(gobj, jn_resp, "data`saved", 0, 0) ||
            !kw_get_bool(gobj, jn_resp, "data`stale", 0, 0)) {
        result += test_fail(gobj, db, "TEST FAIL: KS, the kept save below the broken file is not stale",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    close_db(gobj, db);

    result += write_schema_file(gobj, db, users_departments_v1(db));
    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return result - 1;
    }
    jn_resp = treedb_cmd(gobj, db, "saved-schema", json_object());
    if(!kw_get_bool(gobj, jn_resp, "data`saved", 0, 0) ||
            kw_get_int(gobj, jn_resp, "data`saved_schema_version", 0, 0) != 2 ||
            !kw_get_bool(gobj, jn_resp, "data`can_apply", 0, 0)) {
        result += test_fail(gobj, db, "TEST FAIL: KS, the kept save is not pending over the file put back",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    result += apply_schema(gobj, db);
    close_db(gobj, db);
    drop_treedb(gobj, db);
    return result;
}

/***************************************************************************
 *  BH: a store that runs a topic AHEAD of its schema file. The operator's
 *  apply of `users` ran (topic_version 2, with `email`); then a newer
 *  literal (schema_version 3) declares `users` at topic_version 1, without
 *  `email`. It is installed whole (so did 7.25.4), and tranger2 keeps
 *  running its own `users`: 2 is above 1. The file says one definition,
 *  the store runs another that only its topic_cols.json holds.
 *
 *    - every open that runs the file says it: ONE WARNING, naming the
 *      topic;
 *    - a save with NO edit has nothing to save (__system__ holds the
 *      file's columns, the draft is the file), and its answer names
 *      `users` in `store_ahead` ({topic_version 1, running_version 2}) and
 *      says what to do: edit the topic in __system__, or raise it from C;
 *    - a save of `users` edited publishes it past what RUNS
 *      (topic_version 3), so the apply installs it: the store runs the
 *      saved `users`, the next open says nothing, and a save names no
 *      topic in `store_ahead`.
 *
 *  Red before: nothing was said after the open that installed the
 *  literal, and the save published `users` at topic_version 2 (the
 *  file's 1, plus one): not above what ran, so the apply reached nothing,
 *  and the store went on running the old `users` in silence.
 ***************************************************************************/
PRIVATE json_t *bh_literal(const char *db, int v)
{
    return schema_of(db, v, json_pack("[o]",
        topic_of("users", 1, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))));
}

PRIVATE int scenario_file_behind_what_runs(hgobj gobj)
{
    int result = 0;
    const char *db = "tw_bh";

    if(open_db(gobj, db, bh_literal(db, 1), FALSE) < 0) {
        return -1;
    }
    result += add_draft_col(gobj, db, "users", "email");
    result += save_schema(gobj, db);
    result += apply_schema(gobj, db);
    close_db(gobj, db);
    if(open_db(gobj, db, bh_literal(db, 1), FALSE) < 0) {
        return result - 1;
    }
    close_db(gobj, db);
    if(open_db(gobj, db, bh_literal(db, 3), FALSE) < 0) {
        return result - 1;
    }
    json_int_t tv;
    json_t *h = running_headers(gobj, db, "users", &tv);
    if(tv != 2 || !json_object_get(h, "email")) {
        result += test_fail(gobj, db,
            "TEST FAIL: BH, the store does not run its own users ahead of the file",
            json_pack("{s:I, s:O}", "topic_version", tv, "headers", h? h : json_null()));
    }
    JSON_DECREF(h)
    close_db(gobj, db);

    for(int i = 0; i < 2; i++) {
        result += open_counting(gobj, db, bh_literal(db, 3),
            "TEST FAIL: BH, a store running a topic ahead of its file was not said", 0, 1);
        if(i == 0) {
            close_db(gobj, db);
        }
    }

    json_t *jn_resp = treedb_cmd(gobj, db, "save-schema", json_object());
    json_t *ahead = kw_get_dict(gobj, jn_resp, "data`store_ahead`users", 0, 0);
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 ||
            kw_get_dict(gobj, jn_resp, "data`topic_versions", 0, 0) ||
            !ahead ||
            kw_get_int(gobj, ahead, "topic_version", 0, KW_WILD_NUMBER) != 1 ||
            kw_get_int(gobj, ahead, "running_version", 0, KW_WILD_NUMBER) != 2 ||
            !strstr(kw_get_str(gobj, jn_resp, "comment", "", 0),
                "the store runs 'users' ahead of it with other columns")) {
        result += test_fail(gobj, db,
            "TEST FAIL: BH, a save with no edit did not name the topic the store runs ahead",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)

    result += edit_header(gobj, db, "users", "username", "Edited");
    jn_resp = treedb_cmd(gobj, db, "save-schema", json_object());
    json_int_t saved_tv = kw_get_int(gobj, jn_resp, "data`topic_versions`users", -1, KW_WILD_NUMBER);
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 || saved_tv != 3) {
        result += test_fail(gobj, db,
            "TEST FAIL: BH, the save did not publish users past what runs", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    result += apply_schema(gobj, db);
    close_db(gobj, db);

    result += open_counting(gobj, db, bh_literal(db, 3),
        "TEST FAIL: BH, the open after the apply said something", 0, 0);
    h = running_headers(gobj, db, "users", &tv);
    if(tv != 3 || json_object_get(h, "email") ||
            strcmp(json_string_value(json_object_get(h, "username"))?
                json_string_value(json_object_get(h, "username")) : "", "Edited")!=0) {
        result += test_fail(gobj, db,
            "TEST FAIL: BH, the apply of the saved users reached nothing",
            json_pack("{s:I, s:O}", "topic_version", tv, "headers", h? h : json_null()));
    }
    JSON_DECREF(h)
    jn_resp = treedb_cmd(gobj, db, "save-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 ||
            json_object_size(kw_get_dict(gobj, jn_resp, "data`store_ahead", 0, 0)) > 0) {
        result += test_fail(gobj, db,
            "TEST FAIL: BH, a save after the apply still names a topic the store runs ahead",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    close_db(gobj, db);
    drop_treedb(gobj, db);
    return result;
}

/***************************************************************************
 *  SW: save-schema writes the saved schema WHOLE: a new file renamed over
 *  the old one (its inode changes), no temporary file left. A save that
 *  cannot write it (the directory read-only) answers -1 and leaves the
 *  pending save as it was.
 *
 *  Red before: the saved schema was truncated and rewritten in place (the
 *  inode stayed): a save that died or hit a full disk halfway left a torn
 *  file where the pending save had been.
 ***************************************************************************/
PRIVATE int scenario_saved_schema_written_whole(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    const char *db = "tw_sw";

    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), priv->path_database, "__system__", "saved_schemas", NULL);
    char fn[NAME_MAX], fn_new[NAME_MAX + 8], path[PATH_MAX];
    snprintf(fn, sizeof(fn), "%s.treedb_schema.json", db);
    snprintf(fn_new, sizeof(fn_new), "%s.new", fn);
    build_path(path, sizeof(path), dir, fn, NULL);

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return -1;
    }
    result += edit_header(gobj, db, "users", "username", "First");
    result += save_schema(gobj, db);
    struct stat st1, st2;
    if(stat(path, &st1) < 0) {
        close_db(gobj, db);
        return result + test_fail(gobj, db, "TEST FAIL: SW, no saved schema", json_string(path));
    }
    result += edit_header(gobj, db, "users", "username", "Second");
    result += save_schema(gobj, db);
    if(stat(path, &st2) < 0 || st1.st_ino == st2.st_ino || file_exists(dir, fn_new)) {
        result += test_fail(gobj, db,
            "TEST FAIL: SW, the saved schema was rewritten in place, or a temporary was left",
            json_pack("{s:I, s:I}", "inode_before", (json_int_t)st1.st_ino,
                "inode_after", (json_int_t)st2.st_ino));
    }

    /*
     *  A save that cannot write leaves the pending one
     */
    struct stat st_dir;
    stat(dir, &st_dir);
    result += edit_header(gobj, db, "users", "username", "Third");
    if(chmod(dir, 0550) < 0) {
        result += test_fail(gobj, db, "TEST FAIL: SW, chmod failed", json_string(dir));
    }
    json_t *jn_resp = treedb_cmd(gobj, db, "save-schema", json_object());
    int ret = (int)kw_get_int(gobj, jn_resp, "result", 0, 0);
    JSON_DECREF(jn_resp)
    chmod(dir, st_dir.st_mode & 07777);
    json_t *saved = load_json_from_file(gobj, dir, fn, 0);
    json_t *users = saved? topic_in(saved, "users") : NULL;
    if(ret != -1 || !users ||
            strcmp(kw_get_str(gobj, users, "cols`username`header", "", 0), "Second")!=0) {
        result += test_fail(gobj, db,
            "TEST FAIL: SW, a save that could not write did not leave the pending save",
            json_pack("{s:i, s:O}", "result", ret, "saved", saved? saved : json_null()));
    }
    JSON_DECREF(saved)
    close_db(gobj, db);
    drop_treedb(gobj, db);
    return result;
}

/***************************************************************************
 *  GB: a schema command with no `treedb_name`, over two treedbs, whose kw
 *  carries a binary field (`gbuffer`): each treedb gets a kw copy with a
 *  reference of its own. The caller keeps its reference, and nothing is
 *  logged.
 *
 *  Red before: the kw of each treedb was a json_deep_copy(), which takes no
 *  reference of the gbuffer, and each answer released it: "BAD
 *  gbuf_decref()" and the caller's gbuffer freed.
 ***************************************************************************/
PRIVATE int scenario_kw_gbuffer_every_treedb(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    const char *dbs[] = {"tw_gb1", "tw_gb2", NULL};
    for(int i = 0; dbs[i]; i++) {
        if(open_db(gobj, dbs[i], schema_of(dbs[i], 1, json_pack("[o]",
                topic_of("users", 1, json_pack("{s:o}", "id", col_id())))), FALSE) < 0) {
            return -1;
        }
    }

    gbuffer_t *gbuf = gbuffer_create(16, 16);
    gbuffer_append_string(gbuf, "x");
    gbuffer_incref(gbuf);   /* two references: the test keeps one, the kw takes one */
    json_int_t e0 = log_count(gobj, "error");
    json_t *jn_resp = gobj_command(priv->gobj_treedbs, "saved-schema",
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf), gobj);
    int ret = (int)kw_get_int(gobj, jn_resp, "result", -1, 0);
    JSON_DECREF(jn_resp)
    int refcount = (int)gbuf->refcount;
    if(ret != 0 || refcount != 1 || log_count(gobj, "error") - e0 != 0) {
        result += test_fail(gobj, "tw_gb",
            "TEST FAIL: GB, a command for every treedb released the gbuffer of its kw once too often",
            json_pack("{s:i, s:i, s:I}", "result", ret, "refcount", refcount,
                "errors", log_count(gobj, "error") - e0));
    }
    while(refcount > 0) {
        refcount--;
        gbuffer_decref(gbuf);
    }

    for(int i = 0; dbs[i]; i++) {
        close_db(gobj, dbs[i]);
        drop_treedb(gobj, dbs[i]);
    }
    return result;
}

/***************************************************************************
 *  EV: EV_OPEN_TREEDB and EV_CLOSE_TREEDB are public and NOT implemented:
 *  each one is refused, and says it (an ERROR).
 *
 *  Red before: the open answered 0 and the close -1, and neither said
 *  anything.
 ***************************************************************************/
PRIVATE int scenario_public_events_not_implemented(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    const char *events[] = {"EV_OPEN_TREEDB", "EV_CLOSE_TREEDB", NULL};
    for(int i = 0; events[i]; i++) {
        gobj_event_t event = gclass_find_public_event(events[i], TRUE);
        if(!event) {
            result += test_fail(gobj, "tw_ev", "TEST FAIL: EV, the public event is not there",
                json_string(events[i]));
            continue;
        }
        json_int_t e0 = log_count(gobj, "error");
        int ret = gobj_send_event(priv->gobj_treedbs, event,
            json_pack("{s:s}", "treedb_name", "tw_ev"), gobj);
        if(ret != -1 || log_count(gobj, "error") - e0 != 1) {
            result += test_fail(gobj, "tw_ev",
                "TEST FAIL: EV, a public event that is not implemented was not refused loudly",
                json_pack("{s:s, s:i, s:I}", "event", events[i], "result", ret,
                    "errors", log_count(gobj, "error") - e0));
        }
    }
    return result;
}

/***************************************************************************
 *  Run every scenario
 ***************************************************************************/
PRIVATE int run_tests(hgobj gobj)
{
    int result = 0;

    result += scenario_removed_topic(gobj);
    result += scenario_tie_and_hook(gobj);
    result += scenario_equal_topic_version(gobj);
    result += scenario_drafts_withdrawn(gobj);
    result += scenario_literal_not_newer(gobj);
    result += scenario_renamed_topic(gobj);
    result += scenario_topic_not_raised(gobj);
    result += scenario_imposed_removed_topic(gobj);
    result += scenario_missing_topic_dir_is_no_apply(gobj);
    result += scenario_snapshot_holds_removed_topic(gobj);
    result += scenario_second_open_refused(gobj);
    result += scenario_apply_that_ran(gobj);
    result += scenario_seed_from_dynamic_file(gobj);
    result += scenario_client_store_locked(gobj);
    result += scenario_apply_record_unwritable(gobj);
    result += scenario_draft_while_unfinished(gobj);
    result += scenario_leftovers_on_every_path(gobj);
    result += scenario_seed_is_not_unfinished(gobj);
    result += scenario_apply_after_a_crash(gobj);
    result += scenario_failed_seed_is_retried(gobj);
    result += scenario_open_that_fails(gobj);
    result += scenario_draft_on_leftover_across_retries(gobj);
    result += scenario_draft_where_the_projection_fails(gobj);
    result += scenario_unreadable_unfinished_record(gobj);
    result += scenario_first_projection_stamped_last(gobj);
    result += scenario_failed_open_says_so(gobj);
    result += scenario_deleted_topic_draft(gobj);
    result += scenario_seed_that_died(gobj);
    result += scenario_saved_draft_across_retries(gobj);
    result += scenario_added_topic_draft(gobj);
    result += scenario_edited_leftover(gobj);
    result += scenario_failed_write_then_restart(gobj);
    result += scenario_failed_write_retried_in_process(gobj);
    result += scenario_unlinked_leftover_col(gobj);
    result += scenario_orphan_col_adopted(gobj);
    result += scenario_orphan_topic_adopted(gobj);
    result += scenario_leftovers_under_older_meta_schema(gobj);
    result += scenario_failed_link_of_new_topic(gobj);
    result += scenario_failed_take_leaves_the_orphans(gobj);
    result += scenario_dotted_treedb_names(gobj);
    result += scenario_column_moved_by_the_operator(gobj);

    /*
     *  CR: where the process dies decides what the retry logs, so its log
     *  is not compared line by line; each retry counts its errors and
     *  warnings instead (see scenario_crash_at_every_write)
     */
    gobj_log_del_handler("test_capture");
    result += scenario_crash_at_every_write(gobj, FALSE);
    result += scenario_crash_at_every_write(gobj, TRUE);

    /*
     *  The capture stays off: the late scenarios count what each open
     *  says (see ac_timeout)
     */
    return result;
}

/***************************************************************************
 *  The late scenarios, one per timeout (see ac_timeout).
 *  They count the errors and warnings of each open instead of comparing
 *  the log line by line (open_counting): several of them fork, or kill.
 ***************************************************************************/
PRIVATE int scenario_orphaned_leftover_edited_0(hgobj gobj)
{
    return scenario_orphaned_leftover_edited(gobj, 0);
}
PRIVATE int scenario_orphaned_leftover_edited_1(hgobj gobj)
{
    return scenario_orphaned_leftover_edited(gobj, 1);
}

PRIVATE int (*late_scenarios[])(hgobj gobj) = {
    scenario_leftover_moved,
    scenario_orphaned_leftover_edited_0,
    scenario_orphaned_leftover_edited_1,
    scenario_ambiguous_owner,
    scenario_ambiguous_with_failure_record,
    scenario_ambiguous_after_crash,
    scenario_delete_treedb_every_node,
    scenario_delete_treedb_killed,
    scenario_record_unwritable,
    scenario_record_lost,
    scenario_colliding_ids,
    scenario_gone_topic_col_undeletable,
    scenario_stamped_before_its_topics,
    scenario_stamped_before_its_columns,
    scenario_imposed_stamped_before_its_columns,
    scenario_imposed_stamped_before_its_topics,
    scenario_imposed_draft_kept,
    scenario_legacy_move_killed,
    scenario_double_crash,
    scenario_old_projection_died_changes,
    scenario_old_projection_died_adds,
    scenario_old_projection_died_twice,
    scenario_first_projection_of_older_release_died,
    scenario_left_by_older_release,
    scenario_save_leaves_what_older_release_left,
    scenario_dict_file_projected,
    scenario_legacy_ids_before_the_upgrade_record,
    scenario_legacy_move_that_fails,
    scenario_same_version_other_content,
    scenario_imposed_same_version_other_content,
    scenario_draft_order_is_not_its_place,
    scenario_moved_col_saved,
    scenario_node_of_two_parents,
    scenario_two_topics_of_one_name,
    scenario_failed_open_keeps_the_save,
    scenario_file_behind_what_runs,
    scenario_saved_schema_written_whole,
    scenario_kw_gbuffer_every_treedb,
    scenario_public_events_not_implemented,
    NULL
};




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  A write of __system__, as C_NODE publishes it (create, update, delete,
 *  link, unlink): the hook points of the scenarios that break a
 *  projection half way. `kill_at_write` kills the process there, as a
 *  crash would; `break_writes_of` makes every later write of a node of
 *  `topics` fail, from the moment it is created.
 ***************************************************************************/
PRIVATE int ac_system_write(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->system_writes++;
    if(priv->kill_at_write > 0 && priv->system_writes >= priv->kill_at_write) {
        kill(getpid(), SIGKILL);
    }

    if(!empty_string(priv->break_writes_of) && event == EV_TREEDB_NODE_CREATED &&
            strcmp(kw_get_str(gobj, kw, "topic_name", "", 0), "topics")==0) {
        const char *id = kw_get_str(gobj, json_object_get(kw, "node"), "id", "", 0);
        if(strcmp(id, priv->break_writes_of)==0) {
            priv->broken_fds = break_key_writes(gobj, "topics", id);
            priv->break_writes_of[0] = 0;
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_TIMEOUT — runs the test logic inside the event loop, then exits
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  ONE step per timeout: the loop runs between two, and completes what
     *  the treedbs closed in a step cancelled. The whole test in one
     *  callback filled the completion queue of io_uring, and nothing more
     *  could be submitted (yev_loop keeps such submissions now, "Submission
     *  queue full and the kernel takes nothing", until the loop runs). A
     *  late scenario that sets `repeat_step` runs again at the next step
     */
    priv->repeat_step = FALSE;
    if(priv->step == 0) {
        priv->result += run_tests(gobj);
        gobj_log_register_handler("counting", 0, counting_log_write, 0);
        gobj_log_add_handler("test_counting", "counting", LOG_OPT_UP_WARNING, 0);
    } else {
        priv->result += late_scenarios[priv->step - 1](gobj);
    }
    if(!priv->repeat_step) {
        priv->step++;
    }
    if(late_scenarios[priv->step - 1]) {
        set_timeout(priv->timer, 10);
        KW_DECREF(kw)
        return 0;
    }

    gobj_log_del_handler("test_counting");
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);
    if(priv->result == 0) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "All treedb literal wins tests PASSED",
            NULL
        );
    }
    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "Exit to die",
        NULL
    );
    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}




                    /***************************
                     *      Commands
                     ***************************/




/***************************************************************************
 *
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
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_LITERAL_WINS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *
 ***************************************************************************/
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

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_idle[] = {
        {EV_TIMEOUT,                ac_timeout,         0},
        {EV_TREEDB_NODE_CREATED,    ac_system_write,    0},
        {EV_TREEDB_NODE_UPDATED,    ac_system_write,    0},
        {EV_TREEDB_NODE_DELETED,    ac_system_write,    0},
        {EV_TREEDB_NODE_LINKED,     ac_system_write,    0},
        {EV_TREEDB_NODE_UNLINKED,   ac_system_write,    0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_TREEDB_NODE_CREATED,    0},
        {EV_TREEDB_NODE_UPDATED,    0},
        {EV_TREEDB_NODE_DELETED,    0},
        {EV_TREEDB_NODE_LINKED,     0},
        {EV_TREEDB_NODE_UNLINKED,   0},
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
        0  // gclass_flag
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_test_literal_wins(void)
{
    return create_gclass(C_TEST_LITERAL_WINS);
}
