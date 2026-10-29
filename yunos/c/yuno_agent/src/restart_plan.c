/****************************************************************************
 *          restart_plan.c
 *
 *          What a restart of the node does with each yuno. See
 *          restart_plan.h.
 *
 *          Up to 7.28.10 activate-snap and deactivate-snap SIGKILLed every
 *          yuno of the node and launched again the enabled ones, whatever
 *          changed: a `yunetas upgrade-yunos` of one binary bounced every
 *          yuno. The reload of the agent's treedb is what forced it: it
 *          loses what the agent knows of a running yuno (in memory only), so
 *          a yuno left alive would have been taken for stopped and launched
 *          beside itself. Now the yunos whose launch did not change keep
 *          their runtime across the reload, and only the others go.
 *
 *          "Did not change" is against what the yuno was LAUNCHED with,
 *          written by run_yuno() beside the files it is launched with
 *          (note_yuno_launch()), not against the treedb as it is when the
 *          restart begins: up to 7.29.1 an update-config
 *          of the version in use, which waits for the next launch, was
 *          already in the treedb on both sides of the reload, and the yuno
 *          was kept on its old configuration with no log.
 *
 *          It lives out of c_agent.c, which no test compiles
 *          (tests/c/c_agent_find_new_yunos).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***************************************************************************/
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>

#include "find_new_yunos.h"
#include "yuno_config_file.h"
#include "restart_plan.h"

/***************************************************************************
 *  TRUE when the column description `jn_col` has the flag `flag`
 ***************************************************************************/
PRIVATE BOOL col_has_flag(json_t *jn_col, const char *flag)
{
    json_t *jn_flags = json_object_get(jn_col, "flag");
    size_t idx; json_t *jn_flag;
    json_array_foreach(jn_flags, idx, jn_flag) {
        if(json_is_string(jn_flag) && strcmp(json_string_value(jn_flag), flag)==0) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  Saved columns that are no input of a launch, left out of the signature:
 *  with them a restart killed yunos whose launch had not changed.
 ***************************************************************************/
PRIVATE const char *not_launch_columns[][2] = {
    {"yunos",           "must_play"},       // played after the launch (play-yuno)
    {"yunos",           "traced"},          // set on a running yuno (trace-on-yuno)
    {"yunos",           "start_priority"},  // the order of the launches, not a launch
    {"binaries",        "yunos"},           // the rows linked: another release linked to it
    {"configurations",  "yunos"},           // the same
    {0, 0}
};

PRIVATE BOOL is_a_launch_column(const char *topic_name, const char *col)
{
    for(int i=0; not_launch_columns[i][0]; i++) {
        if(strcmp(not_launch_columns[i][0], topic_name)==0 &&
                strcmp(not_launch_columns[i][1], col)==0) {
            return FALSE;
        }
    }
    return TRUE;
}

/***************************************************************************
 *  The columns of `record` (not owned) of the topic `topic_name` that are
 *  saved and an input of its launch (`saved` TRUE) or that live in memory
 *  only (FALSE). New dict, or json null for a NULL record.
 ***************************************************************************/
PRIVATE json_t *record_columns(
    hgobj gobj,
    hgobj gobj_resource,
    const char *topic_name,
    json_t *record,
    BOOL saved
)
{
    if(!record) {
        return json_null();
    }
    json_t *tranger = gobj_read_pointer_attr(gobj_resource, "tranger");
    json_t *jn_cols = tranger2_dict_topic_desc_cols(tranger, topic_name);
    if(!jn_cols) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "cannot get the columns of a topic",
            "topic_name",   "%s", topic_name,
            NULL
        );
        return json_null();
    }

    json_t *jn_out = json_object();
    const char *col; json_t *jn_col;
    json_object_foreach(jn_cols, col, jn_col) {
        if(strcmp(col, "_geometry")==0) {
            continue;
        }
        BOOL is_saved = col_has_flag(jn_col, "persistent") || col_has_flag(jn_col, "fkey");
        BOOL is_hook = col_has_flag(jn_col, "hook");
        if(saved? !is_saved : (is_saved || is_hook)) {
            continue;
        }
        if(saved && !is_a_launch_column(topic_name, col)) {
            continue;
        }
        json_t *jn_value = json_object_get(record, col);
        if(jn_value) {
            json_object_set(jn_out, col, jn_value);
        }
    }
    JSON_DECREF(jn_cols)
    return jn_out;
}

/***************************************************************************
 *  See restart_plan.h
 ***************************************************************************/
PUBLIC json_t *yuno_launch_signature(
    hgobj gobj,
    hgobj gobj_resource,
    json_t *yuno
)
{
    const char *yuno_id = kw_get_str(gobj, yuno, "id", "", KW_REQUIRED);
    const char *yuno_role = kw_get_str(gobj, yuno, "yuno_role", "", KW_REQUIRED);
    const char *yuno_name = kw_get_str(gobj, yuno, "yuno_name", "", KW_REQUIRED);
    const char *role_version = kw_get_str(gobj, yuno, "role_version", "", KW_REQUIRED);
    const char *name_version = kw_get_str(gobj, yuno, "name_version", "", KW_REQUIRED);

    json_t *jn_signature = json_object();
    json_object_set_new(jn_signature, "yuno",
        record_columns(gobj, gobj_resource, "yunos", yuno, TRUE));

    json_t *binary = empty_string(role_version)? NULL :
        find_binary_version(gobj, gobj_resource, yuno_role, role_version);
    json_object_set_new(jn_signature, "binary",
        record_columns(gobj, gobj_resource, "binaries", binary, TRUE));
    JSON_DECREF(binary)

    json_t *configuration = empty_string(name_version)? NULL :
        find_configuration_version(gobj, gobj_resource, yuno_role, yuno_name, name_version);
    json_object_set_new(jn_signature, "configuration",
        record_columns(gobj, gobj_resource, "configurations", configuration, TRUE));
    JSON_DECREF(configuration)

    json_t *realm = NULL;
    const char *realm_id = json_string_value(json_array_get(json_object_get(yuno, "realm_id"), 0));
    if(!empty_string(realm_id)) {
        realm = gobj_get_node(
            gobj_resource,
            "realms",
            json_pack("{s:s}", "id", realm_id),
            json_pack("{s:b}", "only_id", 1),
            gobj
        );
    }
    json_object_set_new(jn_signature, "realm",
        record_columns(gobj, gobj_resource, "realms", realm, TRUE));
    JSON_DECREF(realm)

    json_t *services = gobj_list_nodes(
        gobj_resource,
        "public_services",
        json_pack("{s:s}", "yuno_id", yuno_id),    // filter
        json_pack("{s:b}", "only_id", 1),
        gobj
    );
    json_t *jn_services = json_object();
    size_t idx; json_t *service;
    json_array_foreach(services, idx, service) {
        json_object_set_new(jn_services, kw_get_str(gobj, service, "id", "", 0),
            record_columns(gobj, gobj_resource, "public_services", service, TRUE));
    }
    JSON_DECREF(services)
    json_object_set_new(jn_signature, "public_services", jn_services);

    return jn_signature;
}

/***************************************************************************
 *  The name of the file of a yuno's bin/ that keeps its launch signature
 ***************************************************************************/
PRIVATE int build_launch_file_name(
    hgobj gobj,
    char *bf,
    size_t bfsize,
    const char *role_plus_name
)
{
    int n = snprintf(bf, bfsize, "%s.launch.json", role_plus_name);
    if(n < 0 || (size_t)n >= bfsize) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "name of the launch file too long",
            "role_plus_name", "%s", role_plus_name,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  See restart_plan.h
 ***************************************************************************/
PUBLIC int note_yuno_launch(
    hgobj gobj,
    hgobj gobj_resource,
    const char *yuno_id,
    const char *bin_path,
    const char *role_plus_name
)
{
    char filename[NAME_MAX];
    char path[PATH_MAX];
    if(build_launch_file_name(gobj, filename, sizeof(filename), role_plus_name)<0) {
        // Error already logged
        return -1;
    }
    build_path(path, sizeof(path), bin_path, filename, NULL);

    json_t *yuno = gobj_get_node(
        gobj_resource,
        "yunos",
        json_pack("{s:s}", "id", yuno_id),
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );
    if(!yuno) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "yuno launched not found: a restart will launch it again",
            "yuno_id",      "%s", yuno_id,
            NULL
        );
        unlink(path);   // the signature of another launch would answer for this one
        return -1;
    }
    json_t *jn_signature = yuno_launch_signature(gobj, gobj_resource, yuno);
    JSON_DECREF(yuno)

    /*
     *  Exactly as it is: up to 7.29.2 it went through json2str() (reals at
     *  12 digits: a real with more never compared equal) into a gbuffer of
     *  256 KB (a larger configuration was cut): the yuno was restarted at
     *  every restart of the node.
     */
    if(write_yuno_json_file(gobj, jn_signature, path)<0) {  // jn_signature owned
        // Error already logged
        unlink(path);   // the signature of another launch would answer for this one
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  See restart_plan.h
 ***************************************************************************/
PUBLIC json_t *yuno_launched_signature(
    hgobj gobj,
    const char *bin_path,
    const char *role_plus_name
)
{
    char filename[NAME_MAX];
    if(build_launch_file_name(gobj, filename, sizeof(filename), role_plus_name)<0) {
        // Error already logged
        return NULL;
    }
    return load_json_from_file(gobj, bin_path, filename, 0);  // NULL: none (silent), or logged
}

/***************************************************************************
 *  See restart_plan.h
 ***************************************************************************/
PUBLIC json_t *yuno_runtime_fields(
    hgobj gobj,
    hgobj gobj_resource,
    json_t *yuno
)
{
    return record_columns(gobj, gobj_resource, "yunos", yuno, FALSE);
}

/***************************************************************************
 *  See restart_plan.h
 ***************************************************************************/
PUBLIC const char *restart_decision(
    json_t *signature_launched,
    json_t *signature_after
)
{
    if(!signature_after) {
        return "stop";
    }
    json_t *jn_yuno = json_object_get(signature_after, "yuno");
    if(json_is_true(json_object_get(jn_yuno, "yuno_disabled"))) {
        return "stop";      // a disabled yuno does not run (disable-yuno kills it)
    }
    if(signature_launched && json_equal(signature_launched, signature_after)) {
        return "keep";
    }
    return "restart";   // changed, or not launched by this agent: not known
}
