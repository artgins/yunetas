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
 *  fkey from the child. The treedb opens -- the per-topic merge kept the
 *  parent from the file, with a hook to a column that no longer existed,
 *  and the treedb could never open again (fourth independent review) --
 *  and the parent is gone from the file, from what runs and from
 *  __system__. The same literal opens again, saying nothing.
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
 *  L-2: a topic whose store directory is gone (its topic_var.json with it)
 *  is no APPLIED topic: nobody applied anything. It read as "applied" when
 *  the kind was inferred from the file's topic_version being above the
 *  store's -- 0 for a topic with no topic_var.json. The literal changes it:
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
    result += check_agree(gobj, db, "TEST FAIL: L-2, a topic whose directory is gone");
    result += check_withdrawn(gobj, db, "TEST FAIL: L-2, a topic with no topic_var.json read as applied",
        0, json_object());
    close_db(gobj, db);
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

    if(result == 0) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "All treedb literal wins tests PASSED",
            NULL
        );
    }

    return result;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  EV_TIMEOUT — runs the test logic inside the event loop, then exits
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    run_tests(gobj);

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
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,        0},
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
