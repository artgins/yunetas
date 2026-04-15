/***********************************************************************
 *          C_AUTH_BFF_YUNO.C
 *          auth_bff yuno top-level GClass.
 *
 *          Top-level orchestrator for the auth_bff yuno.  Idle by
 *          default; on mt_play it locates the __bff_side__ service
 *          (declared by the batch configuration) and starts its tree;
 *          on mt_pause it stops the tree again.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <c_auth_bff.h>     /* C_AUTH_BFF gclass name */
#include "c_auth_bff_yuno.h"

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_bff_status(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};

PRIVATE const char *a_help[]    = {"h", "?", 0};
PRIVATE const char *a_status[]  = {"bff-status", "view-bff", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn-----------------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,               "Command's help"),
SDATACM (DTP_SCHEMA,    "view-bff-status",  a_status,           0,              cmd_view_bff_status,    "Snapshot of every C_AUTH_BFF instance: queues, pending tasks and active Keycloak round-trip"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
PRIVATE const trace_level_t s_user_trace_level[16] = {
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj gobj_bff_side;
} PRIVATE_DATA;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method play
 *  Yuneta rule:
 *  If service has mt_play then start only the service gobj.
 *      (Let mt_play be responsible to start their tree)
 *  If service has not mt_play then start the tree with gobj_start_tree().
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-------------------------*
     *      Get bff side
     *-------------------------*/
    priv->gobj_bff_side = gobj_find_service("__bff_side__", TRUE);
    if(!priv->gobj_bff_side) {
        // Error already logged
        return -1;
    }

    /*--------------------------------*
     *      Start
     *--------------------------------*/
    if(!gobj_is_running(priv->gobj_bff_side)) {
        gobj_start_tree(priv->gobj_bff_side);
    }
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-----------------------------*
     *      Stop bff side
     *-----------------------------*/
    if(priv->gobj_bff_side) {
        EXEC_AND_RESET(gobj_stop_tree, priv->gobj_bff_side);
    }

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
 *  view-bff-status — snapshot every C_AUTH_BFF instance inside __bff_side__.
 *
 *  Walks the bff_side subtree, calls c_auth_bff_get_status() on every
 *  C_AUTH_BFF gobj, and returns:
 *
 *      {
 *          "bff_side":   "__bff_side__",
 *          "totals": {
 *              "instances":      25,
 *              "processing":     2,
 *              "with_active_http": 2,
 *              "queued":         5,
 *              "max_q_count":    3
 *          },
 *          "instances": [ <one record per C_AUTH_BFF>, ... ]
 *      }
 ***************************************************************************/
PRIVATE json_t *cmd_view_bff_status(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  bff_side may not be cached yet if the yuno is in stopped state
     *  (mt_play hasn't run).  Look it up on demand to keep the command
     *  usable as long as __bff_side__ exists in the service registry.
     */
    hgobj bff_side = priv->gobj_bff_side;
    if(!bff_side) {
        bff_side = gobj_find_service("__bff_side__", FALSE);
    }
    if(!bff_side) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "__bff_side__ service not found",
            NULL
        );
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("__bff_side__ service not found"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *jn_filter = json_pack("{s:s}",
        "__gclass_name__", C_AUTH_BFF
    );
    json_t *dl_children = gobj_match_children_tree(bff_side, jn_filter);

    int n_instances        = 0;
    int n_processing       = 0;
    int n_with_active_http = 0;
    int n_queued           = 0;
    int max_q_count        = 0;

    json_t *jn_instances = json_array();
    int idx;
    json_t *jn_child;
    json_array_foreach(dl_children, idx, jn_child) {
        hgobj child = (hgobj)(size_t)json_integer_value(jn_child);

        /*
         *  Each per-channel C_AUTH_BFF exposes a "view-status" command
         *  that returns its own snapshot.  We invoke it via gobj_command
         *  and unwrap the standard {result, comment, schema, data}
         *  envelope to get the per-instance dict.
         */
        json_t *resp = gobj_command(child, "view-status", json_object(), gobj);
        if(!resp) {
            continue;
        }
        json_t *jn_status = kw_get_dict(gobj, resp, "data", NULL, KW_EXTRACT);
        JSON_DECREF(resp)
        if(!jn_status) {
            continue;
        }

        n_instances++;
        if(json_is_true(json_object_get(jn_status, "processing"))) {
            n_processing++;
        }
        if(json_is_true(json_object_get(jn_status, "has_active_http"))) {
            n_with_active_http++;
        }
        int q_count = (int)json_integer_value(
            json_object_get(jn_status, "q_count"));
        n_queued += q_count;
        if(q_count > max_q_count) {
            max_q_count = q_count;
        }

        json_array_append_new(jn_instances, jn_status);
    }
    gobj_free_iter(dl_children);

    json_t *jn_data = json_pack(
        "{s:s, s:{s:i, s:i, s:i, s:i, s:i}, s:o}",
        "bff_side",         gobj_short_name(bff_side),
        "totals",
            "instances",        n_instances,
            "processing",       n_processing,
            "with_active_http", n_with_active_http,
            "queued",           n_queued,
            "max_q_count",      max_q_count,
        "instances",        jn_instances
    );

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_data,
        kw  // owned
    );
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create      = mt_create,
    .mt_destroy     = mt_destroy,
    .mt_start       = mt_start,
    .mt_stop        = mt_stop,
    .mt_play        = mt_play,
    .mt_pause       = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_AUTH_BFF_YUNO);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_STOPPED,        0,                  0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_STOPPED,        0},
        {NULL, 0}
    };

    /*----------------------------------------*
     *          Register GClass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,              // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,              // acl
        command_table,
        s_user_trace_level,
        0               // gcflags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_auth_bff_yuno(void)
{
    return create_gclass(C_AUTH_BFF_YUNO);
}
