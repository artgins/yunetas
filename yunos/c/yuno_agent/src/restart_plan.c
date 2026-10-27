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
 *          It lives out of c_agent.c, which no test compiles
 *          (tests/c/c_agent_find_new_yunos).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***************************************************************************/
#include <string.h>

#include "find_new_yunos.h"
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
 *  The columns of `record` (not owned) of the topic `topic_name` that are
 *  saved (`saved` TRUE) or that live in memory only (FALSE). New dict, or
 *  json null for a NULL record.
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
    json_t *signature_before,
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
    if(json_equal(signature_before, signature_after)) {
        return "keep";
    }
    return "restart";
}
