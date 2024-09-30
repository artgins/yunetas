/***********************************************************************
 *          DBSIMPLE.C
 *
 *          Simple DB file for persistent attributes
 *
 *          Copyright (c) 2015 Niyamaka, 2024- ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>
#include <kwid.h>
#include "dbsimple.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char *get_persist_filename(hgobj gobj, char *bf, size_t bfsize, const char *label)
{
    snprintf(bf, bfsize, "%s-%s-%s.json",
        gobj_gclass_name(gobj),
        gobj_name(gobj),
        label
    );
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *load_json(
    hgobj gobj
)
{
    char filename[NAME_MAX];
    get_persist_filename(gobj, filename, sizeof(filename), "persistent-attrs");

    if(empty_string(filename) || access(filename, 0)!=0) {
        // No persistent attrs saved
        return 0;
    }

    size_t flags = 0;
    json_error_t error;
    json_t *jn_device = json_load_file(filename, flags, &error);
    return jn_device;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int save_json(
    hgobj gobj,
    json_t *jn // owned
)
{
    char filename[NAME_MAX];
    get_persist_filename(gobj, filename, sizeof(filename), "persistent-attrs");

    int ret = json_dump_file(
        jn,
        filename,
        JSON_INDENT(4)
    );
    if(ret < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "Cannot save device json database",
            "path",         "%s", filename,
            NULL
        );
        JSON_DECREF(jn)
        return -1;
    }
    JSON_DECREF(jn)
    return 0;
}

/***************************************************************************
   Load writable persistent attributes from simple file db
 ***************************************************************************/
PUBLIC int db_load_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
) {
    json_t *jn_file = load_json(gobj);
    if(jn_file) {
        json_t *attrs = kw_clone_by_keys(
            gobj,
            jn_file,    // owned
            keys,       // owned
            FALSE
        );

        gobj_write_attrs(gobj, attrs, SDF_PERSIST, 0);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int db_save_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
) {
    json_t *jn_attrs = gobj_read_attrs(gobj, SDF_PERSIST, 0);
    json_t *attrs = kw_clone_by_keys(
        gobj,
        jn_attrs,   // owned
        keys,       // owned
        FALSE
    );

    json_t *jn_file = load_json(gobj);
    if(jn_file) {
        json_object_update_missing(attrs, jn_file);
        JSON_DECREF(jn_file)
    }

    save_json(
        gobj,
        attrs  // owned
    );

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int db_remove_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
) {
    json_t *jn_file = load_json(gobj);

    json_t *attrs = kw_clone_by_not_keys(
        gobj,
        jn_file,    // owned
        keys,       // owned
        FALSE
    );

    save_json(
        gobj,
        attrs  // owned
    );

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *db_list_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
) {
    json_t *jn_file = load_json(gobj);

    json_t *attrs = kw_clone_by_keys(
        gobj,
        jn_file,    // owned
        keys,       // owned
        FALSE
    );
    return attrs;
}
