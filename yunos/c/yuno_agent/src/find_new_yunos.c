/****************************************************************************
 *          find_new_yunos.c
 *
 *          The rows of the agent's find-new-yunos command. See
 *          find_new_yunos.h.
 *
 *          Up to 7.25.4 the preview listed, as "would be created", every
 *          yuno with a newer release, also the ones a previous
 *          `find-new-yunos create=1` had already registered and nothing had
 *          promoted yet (a resumed upgrade): the old release stays the
 *          primary until `deactivate-snap`, so it keeps matching. And
 *          `create=1` then failed on each of them with "Yuno already exists".
 *          Each row now says whether its instance exists.
 *
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2025-2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "find_new_yunos.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define SDATA_GET_ID(hs)  kw_get_str(gobj, (hs), "id", "", KW_REQUIRED)
#define SDATA_GET_STR(hs, field)  kw_get_str(gobj, (hs), (field), "", KW_REQUIRED)
#define SDATA_GET_BOOL(hs, field)  kw_get_bool(gobj, (hs), (field), 0, KW_REQUIRED)

/***************************************************************************
 *  See find_new_yunos.h
 ***************************************************************************/
PUBLIC int build_yuno_release_name(
    hgobj gobj,
    char *bf,
    size_t bfsize,
    const char *role_version,
    const char *name_version
)
{
    int n = snprintf(bf, bfsize, "%s-%s", role_version, name_version);
    if(n < 0 || (size_t)n >= bfsize) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "yuno release name too long",
            "role_version", "%s", role_version,
            "name_version", "%s", name_version,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Take a candidate release only when it moves FORWARD, and say out loud
 *  which way it moves. Returns TRUE to take it.
 *
 *  A version is only ever supposed to go forward, and until now the
 *  comparison that picked the candidate was the ONLY thing standing between
 *  a deploy and a downgrade. When that comparison was wrong -- get_n_v()
 *  overflowed an int and read 1.9.0.0-2 as NEGATIVE -- nothing else noticed:
 *  the agent re-appended the older release as the primary at every restart
 *  for eleven days on a client node, and no log line said so, because a
 *  comparison that comes out backwards says nothing.
 *
 *  So the direction is named in the log whichever way it goes, and going
 *  backwards needs `force=1` from whoever is asking.
 ***************************************************************************/
PRIVATE BOOL accept_new_release(
    hgobj gobj,
    const char *what,       // "binary" or "config", for the log
    const char *id,
    const char *current,
    const char *candidate,
    BOOL force
)
{
    if(version_cmp(candidate, current) > 0) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_STARTUP,
            "msg",          "%s", "new release found",
            "what",         "%s", what,
            "id",           "%s", id,
            "from",         "%s", current,
            "to",           "%s", candidate,
            NULL
        );
        return TRUE;
    }

    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_STARTUP,
        "msg",          "%s", force?
                                "release does NOT move forward, taken anyway (force=1)":
                                "release does NOT move forward, NOT taken",
        "what",         "%s", what,
        "id",           "%s", id,
        "from",         "%s", current,
        "to",           "%s", candidate,
        NULL
    );

    return force?TRUE:FALSE;
}

/***************************************************************************
 *  Does the yuno instance at `yuno_release` exist already? The filter of
 *  create-yuno's "Yuno already exists" guard, plus the id for a multiple
 *  yuno (create-yuno does not guard those, and without the id another
 *  instance of the same role and name would answer for this one).
 ***************************************************************************/
PRIVATE BOOL yuno_release_exists(
    hgobj gobj,
    hgobj gobj_resource,
    const char *id,
    const char *realm_id,
    const char *yuno_role,
    const char *yuno_name,
    const char *yuno_release,
    BOOL multiple,
    hgobj src
)
{
    json_t *kw_find = json_pack("{s:s, s:s, s:s, s:s}",
        "realm_id", realm_id,
        "yuno_role", yuno_role,
        "yuno_name", yuno_name,
        "yuno_release", yuno_release
    );
    if(multiple) {
        json_object_set_new(kw_find, "id", json_string(id));
    }
    json_t *iter_find = gobj_list_nodes(
        gobj_resource,
        "yunos",
        kw_find, // filter
        json_pack("{s:b, s:b}", "include-instances", 1, "only_id", 1),
        src
    );
    BOOL exists = json_array_size(iter_find)>0?TRUE:FALSE;
    JSON_DECREF(iter_find)
    return exists;
}

/***************************************************************************
 *  See find_new_yunos.h
 ***************************************************************************/
PUBLIC json_t *find_new_yunos(
    hgobj gobj,
    hgobj gobj_resource,
    json_t *filter,     // owned
    BOOL force,
    hgobj src
)
{
    json_t *jn_rows = json_array();

    /*
     *  Get a iter of matched resources.
     */
    json_t *iter = gobj_list_nodes(
        gobj_resource,
        "yunos",
        filter, // owned
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    int idx; json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        const char *id = SDATA_GET_ID(yuno);
        const char *realm_id = SDATA_GET_STR(yuno, "realm_id`0");
        const char *yuno_role = SDATA_GET_STR(yuno, "yuno_role");
        const char *yuno_name = SDATA_GET_STR(yuno, "yuno_name");
        const char *role_version = SDATA_GET_STR(yuno, "role_version");
        const char *name_version = SDATA_GET_STR(yuno, "name_version");

        /*
         *  Find a greater config version
         */
        char config_name[NAME_MAX];
        snprintf(config_name, sizeof(config_name), "%s.%s", yuno_role, yuno_name);
        json_t *configs = gobj_list_instances(
            gobj_resource,
            "configurations",
            "",
            json_pack("{s:s}", "id", config_name),
            json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
            src
        );
        json_t *config_found = 0;
        int ix; json_t *config;
        json_array_foreach(configs, ix, config) {
            const char *name_version_ = SDATA_GET_STR(config, "version");
            if(config_found) {
                if(version_cmp(SDATA_GET_STR(config_found, "version"), name_version_) < 0) {
                    config_found = config;
                }
            } else {
                if(version_cmp(name_version, name_version_) < 0) {
                    config_found = config;
                }
            }
        }
        json_incref(config_found);
        JSON_DECREF(configs);

        /*
         *  Find a greater role version
         */
        json_t *binaries = gobj_list_instances(
            gobj_resource,
            "binaries",
            "",
            json_pack("{s:s}", "id", yuno_role),
            json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
            src
        );
        json_t *binary_found = 0;
        json_t *binary;
        json_array_foreach(binaries, ix, binary) {
            const char *role_version_ = SDATA_GET_STR(binary, "version");
            if(binary_found) {
                if(version_cmp(SDATA_GET_STR(binary_found, "version"), role_version_) < 0) {
                    binary_found = binary;
                }
            } else {
                if(version_cmp(role_version, role_version_) < 0) {
                    binary_found = binary;
                }
            }
        }
        json_incref(binary_found);
        JSON_DECREF(binaries);

        /*
         *  Both picks above are "the greatest one, if greater than what we
         *  run". Check the direction ONE more time, on its own, so it lands
         *  in the log either way and a backwards move needs force=1. See
         *  accept_new_release().
         */
        if(config_found) {
            if(!accept_new_release(
                    gobj, "config", id, name_version,
                    SDATA_GET_STR(config_found, "version"), force)) {
                JSON_DECREF(config_found)
            }
        }
        if(binary_found) {
            if(!accept_new_release(
                    gobj, "binary", id, role_version,
                    SDATA_GET_STR(binary_found, "version"), force)) {
                JSON_DECREF(binary_found)
            }
        }

        if(!config_found && !binary_found) {
            continue;
        }
        const char *new_name_version = config_found?
            SDATA_GET_STR(config_found, "version"):
            SDATA_GET_STR(yuno, "name_version");

        const char *new_role_version = binary_found?
            SDATA_GET_STR(binary_found, "version"):
            SDATA_GET_STR(yuno, "role_version");

        BOOL multiple = SDATA_GET_BOOL(yuno, "yuno_multiple");

        char yuno_release[NAME_MAX];
        BOOL registered = FALSE;
        if(build_yuno_release_name(
                gobj, yuno_release, sizeof(yuno_release),
                new_role_version, new_name_version)<0) {
            // Error already logged; the row is offered as new, create-yuno judges it
        } else {
            registered = yuno_release_exists(
                gobj,
                gobj_resource,
                id,
                realm_id,
                yuno_role,
                yuno_name,
                yuno_release,
                multiple,
                src
            );
        }

        /*
         *  Inherit the operator-set node placement from the prior primary row.
         *  Without this a version-bump deploy would reset start_priority /
         *  sched_priority / cpu_core to the schema defaults, collapsing the
         *  launch tiers and forcing a re-run of tools/agent/set_start_priorities.py.
         */
        json_t *jn_command = json_sprintf(
            "create-yuno id=%s realm_id=%s yuno_role=%s role_version=%s "
            "yuno_name=%s name_version=%s yuno_tag=%s yuno_multiple=%d "
            "start_priority=%d sched_priority=%d cpu_core=%d",
            id,
            realm_id,
            yuno_role,
            new_role_version,
            yuno_name,
            new_name_version,
            SDATA_GET_STR(yuno, "yuno_tag"),
            multiple,
            (int)kw_get_int(gobj, yuno, "start_priority", 5, KW_REQUIRED),
            (int)kw_get_int(gobj, yuno, "sched_priority", 20, KW_REQUIRED),
            (int)kw_get_int(gobj, yuno, "cpu_core", 0, KW_REQUIRED)
        );

        json_t *jn_row = json_object();
        json_object_set_new(jn_row, "command", jn_command);
        json_object_set_new(jn_row, "registered", json_boolean(registered));
        json_array_append_new(jn_rows, jn_row);

        json_decref(binary_found);
        json_decref(config_found);
    }
    json_decref(iter);

    return jn_rows;
}
