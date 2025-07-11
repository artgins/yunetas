/***********************************************************************
 *          STATS_PARSER.C
 *
 *          Stats parser
 *
 *          Copyright (c) 2017-2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>
#include "gobj.h"
#include "stats_parser.h"

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
 *  The default stats parser if mt_stats() is not implemented.
 *  Return with build_command_response()
 ***************************************************************************/
PUBLIC json_t *stats_parser(hgobj gobj,
    const char *stats,
    json_t *kw,
    hgobj src
)
{
    /*--------------------------------------*
     *  Build standard stats
     *--------------------------------------*/
    json_t *jn_data = build_stats(
        gobj,
        stats,
        kw,     // owned
        src
    );

    return build_stats_response(
        gobj,
        0,          // result
        0,          // jn_comment
        0,          // jn_schema
        jn_data     // jn_data, owned
    );
}

/****************************************************************************
 *
 ****************************************************************************/
PRIVATE int reset_stats_callback(hgobj gobj, json_t *kw, const char *key, json_t *value)
{
    // TODO uso un default? de un schema? gobj_read_default_attr_value(gobj, it->name);
    if(json_is_array(value)) {
        json_object_set_new(kw, key, json_array());
    } else if(json_is_integer(value)) {
        json_object_set_new(kw, key, json_integer(0));
    } else if(json_is_boolean(value)) {
        json_object_set_new(kw, key, json_false());
    } else if(json_is_real(value)) {
        json_object_set_new(kw, key, json_real(0));
    } else if(json_is_null(value)) {
    }
    return 0;
}

/****************************************************************************
 *  Build stats from gobj's attributes with SFD_STATS... flag.
 ****************************************************************************/
PRIVATE json_t *_build_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src)
{
    if(!gobj) {
        KW_DECREF(kw);
        return 0;
    }

    // TODO check "__reset_stats__" authz
    // TODO check "__read_stats__" authz

    const sdata_desc_t *sdesc = gobj_attr_desc(gobj, NULL, FALSE);
    const sdata_desc_t *it;
    if(stats && strcmp(stats, "__reset__")==0) {
        /*-------------------------*
         *  Reset Stats in sdata
         *-------------------------*/
        gobj_reset_rstats_attrs(gobj);

        /*----------------------------*
         *  Reset Stats in jn_stats
         *----------------------------*/
        json_t *jn_stats = gobj_jn_stats(gobj);
        kw_walk(gobj, jn_stats, reset_stats_callback);

        stats = "";
    }

    /*-------------------------*
     *  Create stats json
     *-------------------------*/
    json_t *jn_data = json_object();

    /*-------------------------*
     *      Stats in sdata
     *-------------------------*/
    it = sdesc;
    while(it && it->name) {
        if(!(DTP_IS_NUMBER(it->type) || DTP_IS_STRING(it->type))) {
            it++;
            continue;
        }
        if(!empty_string(stats)) {
            if(!strstr(stats, it->name)) { // Parece que es para seleccionar por prefijo
                char prefix[32];
                char *p = strchr(it->name, '_');
                if(p) {
                    int ln = p - it->name;
                    snprintf(prefix, sizeof(prefix), "%.*s", ln, it->name);
                    p = strstr(stats, prefix);
                    if(!p) {
                        it++;
                        continue;
                    }
                }
            }
        }
        if(it->flag & (SDF_STATS|SDF_RSTATS|SDF_PSTATS)) {
            if(DTP_IS_REAL(it->type)) {
                json_object_set_new(
                    jn_data,
                    it->name,
                    json_real(gobj_read_real_attr(gobj, it->name))
                );
            } else if(DTP_IS_BOOLEAN(it->type)) {
                json_object_set_new(
                    jn_data,
                    it->name,
                    json_integer(gobj_read_bool_attr(gobj, it->name))
                );
            } else if(DTP_IS_INTEGER(it->type)) {
                json_object_set_new(
                    jn_data,
                    it->name,
                    json_integer(gobj_read_integer_attr(gobj, it->name))
                );
            } else if(DTP_IS_STRING(it->type)) {
                json_object_set_new(
                    jn_data,
                    it->name,
                    json_string(gobj_read_str_attr(gobj, it->name))
                );
            }
        }
        it++;
    }

    /*----------------------------*
     *      Stats in jn_stats
     *----------------------------*/
    const char *key;
    json_t *v;
    json_t *jn_stats = gobj_jn_stats(gobj); // Stats in gobj->jn_stats;
    json_object_foreach(jn_stats, key, v) {
        if(!empty_string(stats)) {
            if(strstr(stats, key)==0) {
                continue;
            }
        }
        json_object_set(jn_data, key, v);
    }

    KW_DECREF(kw)
    return jn_data;
}

/****************************************************************************
 *  Use by stats_parser(),
 *  Build stats from gobj's attributes with SDF_STATS|SDF_RSTATS|SDF_PSTATS flag.
 *  If `stats` == '__reset__' reset all SDF_RSTATS stats.
 *  It includes all gobj_bottom
 ****************************************************************************/
PUBLIC json_t *build_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src)
{
    json_t *jn_data = json_object();

    json_object_update_missing_new(jn_data, _build_stats(gobj, stats, json_incref(kw), src));

    hgobj gobj_bottom = gobj_bottom_gobj(gobj);
    while(gobj_bottom) {
        json_object_update_missing_new(jn_data, _build_stats(gobj_bottom, stats, json_incref(kw), src));

        gobj_bottom = gobj_bottom_gobj(gobj_bottom);
    }

    KW_DECREF(kw)
    return jn_data;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *build_stats_response(
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment, // owned, if null then not set
    json_t *jn_schema,  // owned, if null then not set
    json_t *jn_data     // owned, if null then not set
) {
    if(!jn_comment) {
        jn_comment = json_string("");
    }
    if(!jn_schema) {
        jn_schema = json_null();
    }
    if(!jn_data) {
        jn_data = json_null();
    }

    json_t *response = json_object();
    json_object_set_new(response, "result", json_integer(result));
    json_object_set_new(response, "comment", jn_comment);
    json_object_set_new(response, "schema", jn_schema);
    json_object_set_new(response, "data", jn_data);

    return response;
}
