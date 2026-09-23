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
 *  open-treedb, the answer as it is. Return is YOURS
 ***************************************************************************/
PRIVATE json_t *open_db_resp(hgobj gobj, const char *treedb_name, json_t *jn_schema) // owned
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = json_pack("{s:s, s:i, s:s, s:o}",
        "filename_mask", "%Y",
        "exit_on_error", 0,
        "treedb_name", treedb_name,
        "treedb_schema", jn_schema
    );
    return gobj_command(priv->gobj_treedbs, "open-treedb", kw, gobj);
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
 *  is not made (it answered applied, and the record was lost in silence).
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
 *  M1 + L2: an operator's work in __system__ while a projection is
 *  unfinished. The leftover (departments, held by a snapshot) is nobody's
 *  draft: saved-schema does not name it in `draft_changed`, and nothing
 *  withdraws it. The column the operator ADDS to users meanwhile is a
 *  draft: the retry of the projection replaces it, and SAYS so
 *  ("unsaved"). It was deleted in silence: every row that __system__ had
 *  and the file did not was taken for a leftover. A column the operator
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
        "TEST FAIL: M1/L2, the leftover of an unfinished projection reads as a draft", json_object());

    result += add_draft_col(gobj, db, "users", "email");
    result += check_draft_changed(gobj, db,
        "TEST FAIL: M1/L2, the operator's column is not the only draft",
        json_pack("{s:b}", "users", 1));
    close_db(gobj, db);

    if(open_db(gobj, db, schema_of(db, 2, json_pack("[o]",
            topic_of("users", 2, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return result - 1;
    }
    if(system_has_col(gobj, db, "users", "email")) {
        result += test_fail(gobj, db,
            "TEST FAIL: M1, the retry of the projection did not replace the operator's column", NULL);
    }
    result += check_withdrawn(gobj, db,
        "TEST FAIL: M1, the operator's column went and nothing said it",
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
        "TEST FAIL: M1, a column added to a leftover topic is not a draft",
        json_pack("{s:b}", "departments", 1));
    close_db(gobj, db);

    result += delete_system_snap(gobj, db, "m1");
    if(open_db(gobj, db, schema_of(db, 2, json_pack("[o]",
            topic_of("users", 2, json_pack("{s:o, s:o}", "id", col_id(), "username", col_str("User")))
        )), FALSE) < 0) {
        return result - 1;
    }
    result += check_agree(gobj, db, "TEST FAIL: M1, the projection was not completed");
    result += check_withdrawn(gobj, db,
        "TEST FAIL: M1, completing the projection did not say the column added to the leftover",
        0, json_pack("{s:s}", "departments", "unsaved"));
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  M2: the leftovers of an unfinished projection are not withdrawn work on
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
        result += check_withdrawn(gobj, db, "TEST FAIL: M2, an unfinished projection withdrew work",
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
            result += test_fail(gobj, db, "TEST FAIL: M2, the leftover is still in __system__", NULL);
        }
        result += check_withdrawn(gobj, db,
            imposed? "TEST FAIL: M2, the imposed retry reported the leftover as withdrawn work" :
                "TEST FAIL: M2, a newer literal reported the leftover as withdrawn work",
            0, json_object());
        close_db(gobj, db);
    }
    return result;
}

/***************************************************************************
 *  L1: a projection whose c_schema_version is 0 because it was SEEDED from
 *  a dynamic file (not because anything was left unfinished), then the
 *  developer takes that file into C, same schema_version. Nothing is
 *  installed, nothing is projected: the operator's drafts stay, nothing is
 *  said. It read as "left unfinished by an earlier open": the header draft
 *  was overwritten and reported, the added column deleted in silence.
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
        result += test_fail(gobj, db, "TEST FAIL: L1, delete-treedb", json_incref(jn_resp));
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
        result += test_fail(gobj, db, "TEST FAIL: L1, the operator's column went", NULL);
    }
    result += check_header(gobj, db, "TEST FAIL: L1, the operator's header draft was overwritten",
        "users", "username", "Operator user", "Operator user", "Draft header");
    result += check_withdrawn(gobj, db, "TEST FAIL: L1, a literal equal to the file withdrew work",
        0, json_object());
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  L3: the process died between the record of an apply and its rename.
 *  The record on disk is then of the apply that never happened, and its
 *  `previous` is the record of the file still in use (an apply that RAN).
 *  The next apply must keep the topics of that one: a newer literal then
 *  reports `users` as "in_use". It kept nothing, because it only looked at
 *  the record itself, not at its `previous`.
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
        result += test_fail(gobj, db, "TEST FAIL: L3, the apply that ran is not recorded in_use",
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
        "TEST FAIL: L3, the apply after a crash lost the topics of the apply that ran",
        0, json_pack("{s:s, s:s}", "users", "in_use", "departments", "applied"));
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  L4: a projection SEEDED from the file that fails (here a column with a
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
            "TEST FAIL: L4, the partial projection of a failed seed reads as a draft", json_object());
        jn_resp = treedb_cmd(gobj, db, "save-schema", json_object());
        if(kw_get_int(gobj, jn_resp, "result", -1, 0) >= 0) {
            result += test_fail(gobj, db,
                "TEST FAIL: L4, save-schema published a failed seed", json_incref(jn_resp));
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
        result += test_fail(gobj, db, "TEST FAIL: L4, the seed was not completed", NULL);
    }
    result += check_draft_changed(gobj, db,
        "TEST FAIL: L4, a completed seed differs from the file", json_object());
    jn_resp = treedb_cmd(gobj, db, "saved-schema", json_object());
    if(json_array_size(kw_get_list(gobj, jn_resp, "data`unfinished_projection", 0, 0)) != 0) {
        result += test_fail(gobj, db, "TEST FAIL: L4, a completed seed still reads as unfinished",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  L6: an open that fails is never "Treedb opened!". Refused by C_TREEDB
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
        result += test_fail(gobj, db, "TEST FAIL: L6, a refused schema does not name the yuno",
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
        result += test_fail(gobj, db, "TEST FAIL: L6, an open that failed answered as opened",
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
 *  M1b: a column the operator adds to the LEFTOVER topic stays a draft
 *  through every retry that cannot finish, and is reported by the open
 *  that removes it. A retry while the snapshot still held the topic took
 *  the column into the record's leftovers: saved-schema stopped showing
 *  it, and the open that completed the projection deleted it in silence
 *  (eighth independent review, R7).
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
        "TEST FAIL: M1b, a column added to a leftover topic is not a draft",
        json_pack("{s:b}", "departments", 1));
    close_db(gobj, db);

    /*
     *  A retry while the snapshot still holds departments
     */
    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    result += check_draft_changed(gobj, db,
        "TEST FAIL: M1b, a retry that cannot finish took the operator's column for a leftover",
        json_pack("{s:b}", "departments", 1));
    result += check_withdrawn(gobj, db,
        "TEST FAIL: M1b, a retry that removed nothing reported withdrawn work",
        0, json_object());
    json_t *record = unfinished_record(gobj, db);
    json_t *leftovers = json_object_get(record, "leftovers");
    int idx; json_t *jn_id;
    json_array_foreach(leftovers, idx, jn_id) {
        if(strcmp(json_string_value(jn_id)? json_string_value(jn_id) : "", "tw_m1b.departments.budget")==0) {
            result += test_fail(gobj, db,
                "TEST FAIL: M1b, the record names the operator's column as a leftover",
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
        result += test_fail(gobj, db, "TEST FAIL: M1b, the projection was not completed", NULL);
    }
    result += check_agree(gobj, db, "TEST FAIL: M1b, the projection was not completed");
    result += check_withdrawn(gobj, db,
        "TEST FAIL: M1b, the operator's column went and nothing said it",
        0, json_pack("{s:s}", "departments", "unsaved"));
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  M2b: the report of a draft does not depend on whether the write that
 *  replaces it succeeds. A draft stays a draft while a projection cannot
 *  replace it, and it is reported ONCE, by the open that replaces it.
 *
 *      tw_m2r  a draft on a topic the literal removes (a column added and
 *              a header edited), and the delete is refused: it was never
 *              reported (eighth independent review, R8);
 *      tw_m2c  a column the operator added to a topic the literal keeps,
 *              and its delete is refused: it was reported at once, while
 *              it was still there, and deleted in silence later.
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
                "TEST FAIL: M2b, a draft still in __system__ was reported as withdrawn",
                0, json_object());
            result += check_draft_changed(gobj, db,
                "TEST FAIL: M2b, a draft the projection could not replace is no longer a draft",
                json_pack("{s:b}", topic, 1));
            close_db(gobj, db);
        }

        result += delete_system_snap(gobj, db, db);
        if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
            return result - 1;
        }
        result += check_agree(gobj, db, "TEST FAIL: M2b, the projection was not completed");
        result += check_withdrawn(gobj, db,
            "TEST FAIL: M2b, the open that replaced the draft did not say it",
            0, json_pack("{s:s}", topic, "unsaved"));
        close_db(gobj, db);
    }
    return result;
}

/***************************************************************************
 *  L1b: a record of an unfinished projection that cannot be read (torn)
 *  still says the projection is unfinished: saved-schema answers it,
 *  save-schema refuses, and the next open retries and writes it again.
 *  It read as "no leftovers" and save-schema published the leftover
 *  (eighth independent review, R12).
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
        result += test_fail(gobj, db, "TEST FAIL: L1b, cannot tear the record", NULL);
    }
    if(fd >= 0) {
        close(fd);
    }

    json_t *jn_resp = treedb_cmd(gobj, db, "saved-schema", json_object());
    if(json_array_size(kw_get_list(gobj, jn_resp, "data`unfinished_projection", 0, 0)) == 0) {
        result += test_fail(gobj, db,
            "TEST FAIL: L1b, an unreadable record reads as a finished projection", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    jn_resp = treedb_cmd(gobj, db, "save-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) >= 0) {
        result += test_fail(gobj, db,
            "TEST FAIL: L1b, save-schema published past an unreadable record", json_incref(jn_resp));
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
            "TEST FAIL: L1b, the retry did not write the record again", json_incref(record));
    }
    JSON_DECREF(record)
    close_db(gobj, db);

    result += delete_system_snap(gobj, db, "l1b");
    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    if(system_has_topic(gobj, db, "departments")) {
        result += test_fail(gobj, db, "TEST FAIL: L1b, the projection was not completed", NULL);
    }
    result += check_agree(gobj, db, "TEST FAIL: L1b, the projection was not completed");

    /*
     *  What the torn record left is unknown: it is taken for a draft, and
     *  reported, never deleted in silence
     */
    result += check_withdrawn(gobj, db,
        "TEST FAIL: L1b, what a torn record left went and nothing said it",
        0, json_pack("{s:s}", "departments", "unsaved"));
    record = unfinished_record(gobj, db);
    if(record) {
        result += test_fail(gobj, db,
            "TEST FAIL: L1b, a completed projection left its record", json_incref(record));
    }
    JSON_DECREF(record)
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  L2b: the node of a treedb in __system__ is created WITHOUT its numbers
 *  (schema_version and c_schema_version 0), and stamped last, when the
 *  whole projection is written. It was created with them: a crash before
 *  the topics were written left a projection that said it was of the
 *  literal, and the next open took it as done -- __system__ with no
 *  topics, and save-schema published an empty schema (eighth independent
 *  review, R11). The first record of the node, on disk, is what says it.
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
            "TEST FAIL: L2b, the node of the treedb was created with its numbers, before its topics",
            json_incref(data));
    }
    if(system_c_schema_version(gobj, db) != 1) {
        result += test_fail(gobj, db, "TEST FAIL: L2b, the complete projection was not stamped",
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
 *  L3b: after an open that failed (treedb_open_db() refused the schema
 *  file in use), a second open says THAT, not "already open", and the
 *  `treedbs` row says the treedb did not open.
 *
 *  L4b: every answer of open-treedb and close-treedb names the yuno.
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
        result += test_fail(gobj, db, "TEST FAIL: L3b, a failed open answered as opened",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)

    jn_resp = open_db_resp(gobj, db, users_only_v2(db));
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0) {
        result += test_fail(gobj, db, "TEST FAIL: L3b, a second open was accepted",
            json_incref(jn_resp));
    }
    result += check_comment_prefix(gobj, db,
        "TEST FAIL: L3b, the second open does not say that the first one failed",
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
        result += test_fail(gobj, db, "TEST FAIL: L3b, the treedbs row does not say it did not open",
            json_incref(jn_rows));
    }
    JSON_DECREF(jn_rows)

    /*
     *  delete-treedb says it did not open, and to close-treedb it first:
     *  it said "while it is OPEN"
     */
    jn_resp = treedb_cmd(gobj, db, "delete-treedb", json_pack("{s:b}", "force", 1));
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0) {
        result += test_fail(gobj, db, "TEST FAIL: L3b, delete-treedb of a treedb that did not open",
            json_incref(jn_resp));
    }
    result += check_comment_prefix(gobj, db,
        "TEST FAIL: L3b, delete-treedb does not say the treedb did not open",
        jn_resp, "did not open");

    /*
     *  The recovery is clean: close-treedb logs nothing (it logged "TreeDB
     *  not found" twice, with stacks), see the expected log list
     */
    jn_resp = gobj_command(priv->gobj_treedbs, "close-treedb",
        json_pack("{s:s, s:b}", "treedb_name", db, "force", 1), gobj);
    result += check_comment_prefix(gobj, db, "TEST FAIL: L4b, close-treedb does not name the yuno",
        jn_resp, NULL);

    /*
     *  L4b: the refusals of the parameters name the yuno too
     */
    jn_resp = gobj_command(priv->gobj_treedbs, "open-treedb",
        json_pack("{s:s}", "filename_mask", "%Y"), gobj);
    result += check_comment_prefix(gobj, db,
        "TEST FAIL: L4b, open-treedb with no treedb_name does not name the yuno", jn_resp, NULL);
    jn_resp = gobj_command(priv->gobj_treedbs, "open-treedb",
        json_pack("{s:s, s:s}", "treedb_name", db, "filename_mask", ""), gobj);
    result += check_comment_prefix(gobj, db,
        "TEST FAIL: L4b, open-treedb with no filename_mask does not name the yuno", jn_resp, NULL);
    jn_resp = gobj_command(priv->gobj_treedbs, "close-treedb",
        json_pack("{s:b}", "force", 1), gobj);
    result += check_comment_prefix(gobj, db,
        "TEST FAIL: L4b, close-treedb with no treedb_name does not name the yuno", jn_resp, NULL);
    jn_resp = gobj_command(priv->gobj_treedbs, "close-treedb",
        json_pack("{s:s, s:b}", "treedb_name", "tw_nothing", "force", 1), gobj);
    result += check_comment_prefix(gobj, db,
        "TEST FAIL: L4b, close-treedb of an unknown treedb does not name the yuno", jn_resp, NULL);
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
 *  N7: the operator's draft DELETES a whole topic in __system__, and a
 *  newer literal declares it: the projection re-creates it, and that
 *  replaces the draft, so it is reported -- "unsaved" (tw_n7u), or
 *  "saved" when a save published the deletion (tw_n7s, with the withdrawn
 *  saved schema). It was reported as nothing: no topic, no warning.
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
            "TEST FAIL: N7, a topic the operator deleted is not a draft",
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
            result += test_fail(gobj, db, "TEST FAIL: N7, the literal did not re-create the topic", NULL);
        }
        result += check_agree(gobj, db, "TEST FAIL: N7, the projection is not the literal");
        result += check_withdrawn(gobj, db,
            "TEST FAIL: N7, the deletion the literal replaced was not reported",
            saved? 2 : 0, json_pack("{s:s}", "departments", saved? "saved" : "unsaved"));
        close_db(gobj, db);
    }
    return result;
}

/***************************************************************************
 *  N1: a seed that died before its end (after delete-treedb, the node of
 *  the treedb is created with 0 / 0 and nothing else; emulated by hand)
 *  is completed by the next open, although the file runs and there is no
 *  record: nothing is reported, and __system__ is the file again. It was
 *  never retried: every topic read as a draft, and save-schema published
 *  a schema with no topics, which left a treedb that could not open.
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
        result += test_fail(gobj, db, "TEST FAIL: N1, delete-treedb", json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    json_t *node = gobj_create_node(sys, "treedbs",
        json_pack("{s:s, s:i, s:i, s:I}", "id", db, "schema_version", 0, "c_schema_version", 0,
            "system_schema_version", meta_version),
        json_pack("{s:b}", "refs", 1), gobj);
    if(!node) {
        result += test_fail(gobj, db, "TEST FAIL: N1, cannot emulate the seed that died", NULL);
    }
    JSON_DECREF(node)

    if(open_db(gobj, db, users_departments_v1(db), FALSE) < 0) {
        return result - 1;
    }
    if(!system_has_topic(gobj, db, "users") || !system_has_topic(gobj, db, "departments") ||
            system_c_schema_version(gobj, db) != 1) {
        result += test_fail(gobj, db, "TEST FAIL: N1, the seed that died was not completed",
            json_integer(system_c_schema_version(gobj, db)));
    }
    result += check_agree(gobj, db, "TEST FAIL: N1, the seed that died was not completed");
    result += check_withdrawn(gobj, db, "TEST FAIL: N1, a seed that died reported withdrawn work",
        0, json_object());

    /*
     *  The operator deletes every topic: save-schema refuses
     */
    result += delete_system_topic(gobj, db, "users");
    result += delete_system_topic(gobj, db, "departments");
    jn_resp = treedb_cmd(gobj, db, "save-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0 ||
            !strstr(kw_get_str(gobj, jn_resp, "comment", "", 0), "no topics")) {
        result += test_fail(gobj, db, "TEST FAIL: N1, save-schema saved a schema with no topics",
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
        result += test_fail(gobj, db, "TEST FAIL: N1, cannot write the saved schema", NULL);
    }
    jn_resp = treedb_cmd(gobj, db, "apply-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0 ||
            kw_get_bool(gobj, jn_resp, "data`applied", 0, 0) ||
            !strstr(kw_get_str(gobj, jn_resp, "comment", "", 0), "no topics")) {
        result += test_fail(gobj, db, "TEST FAIL: N1, apply-schema applied a schema with no topics",
            json_incref(jn_resp));
    }
    JSON_DECREF(jn_resp)
    json_t *file = load_schema_file(gobj, db);
    if(kw_get_int(gobj, file, "schema_version", 0, KW_WILD_NUMBER) != 1) {
        result += test_fail(gobj, db, "TEST FAIL: N1, the file in use changed", json_incref(file));
    }
    JSON_DECREF(file)
    file_remove(saved_dir, filename);
    close_db(gobj, db);
    return result;
}

/***************************************************************************
 *  N4: a SAVED draft on the topic the literal removes, and a snapshot
 *  refuses the delete. The first open withdraws the saved schema and says
 *  that; the draft stays one. The open that finally replaces it says it
 *  as "saved" -- the kind is kept in the record. It said "unsaved": the
 *  saved schema was gone by then.
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
        "TEST FAIL: N4, the first open did not say the saved schema it withdrew",
        2, json_object());
    result += check_draft_changed(gobj, db,
        "TEST FAIL: N4, a draft the projection could not replace is no longer a draft",
        json_pack("{s:b}", "departments", 1));
    json_t *record = unfinished_record(gobj, db);
    json_t *expected_kinds = json_pack("{s:s}", "departments", "saved");
    if(!json_equal(json_object_get(record, "draft_kinds"), expected_kinds)) {
        result += test_fail(gobj, db, "TEST FAIL: N4, the record does not keep the kind of the draft",
            json_incref(record));
    }
    JSON_DECREF(expected_kinds)
    JSON_DECREF(record)
    close_db(gobj, db);

    result += delete_system_snap(gobj, db, "n4");
    if(open_db(gobj, db, users_only_v2(db), FALSE) < 0) {
        return result - 1;
    }
    result += check_agree(gobj, db, "TEST FAIL: N4, the projection was not completed");
    result += check_withdrawn(gobj, db,
        "TEST FAIL: N4, a saved draft was reported as unsaved",
        0, json_pack("{s:s}", "departments", "saved"));
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
