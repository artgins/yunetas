/****************************************************************************
 *          C_BROKER_TREEDB_SRC.C
 *
 *          Driver of the test of the treedb events of the broker.
 *
 *          C_MQTT_BROKER subscribes to EV_TREEDB_NODE_CREATED / _UPDATED /
 *          _DELETED of its own treedb and turns a change of its topic
 *          `users` into the administration of the users of authz. Those
 *          three names are public (C_NODE), so a peer of a gate of the
 *          broker can send them, and C_IOGATE publishes them again as its
 *          own. This driver sends them to the broker in-process, from
 *          itself (a gobj that is not the treedb of the broker):
 *
 *              - DELETED `admin`               -> admin stays enabled
 *              - UPDATED `carol` enabled:true  -> carol stays disabled
 *              - CREATED `mallory`             -> no authz user mallory
 *
 *          each one answered -1, and ONE WARNING for the three (the brake).
 *          Then the same changes made in the treedb of the broker (the
 *          events published by the treedb itself) must reach authz: `bob`
 *          created, disabled, enabled again, deleted (disabled).
 *
 *          Up to 7.30.26 the broker obeyed the forged events: admin
 *          disabled, carol enabled, mallory created.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include "c_broker_treedb_src.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BROKER_SERVICE          "mqtt_broker"
#define BROKER_TREEDB_SERVICE   "treedb_mqtt_broker"
#define AUTHZ_TREEDB_SERVICE    "treedb_authzs"
#define REFUSED_MSG             "treedb event not published by the treedb of the broker, refused"

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type----------name-----------flag----default-----description---------*/
SDATA (DTP_POINTER,   "user_data",   0,      0,          "user data"),
SDATA (DTP_POINTER,   "subscriber",  0,      0,          "subscriber of output-events"),
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
    hgobj timer;
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

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
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
    gobj_stop(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 500);  // the broker and its treedb are up

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  The state of a user of authz: 1 exists and enabled, 0 exists and
 *  disabled, -1 does not exist
 ***************************************************************************/
PRIVATE int authz_user_state(hgobj gobj, const char *username)
{
    json_t *user = gobj_get_node(
        gobj_find_service(AUTHZ_TREEDB_SERVICE, TRUE),
        "users",
        json_pack("{s:s}", "id", username),
        0,
        gobj
    );
    if(!user) {
        return -1;
    }
    int state = kw_get_bool(gobj, user, "disabled", 0, 0)? 0 : 1;
    JSON_DECREF(user)
    return state;
}

/***************************************************************************
 *  Check the state of a user of authz, log an ERROR (the test fails) if
 *  it is not the expected one
 ***************************************************************************/
PRIVATE int check_user(hgobj gobj, const char *casename, const char *username, int expected)
{
    int state = authz_user_state(gobj, username);
    if(state != expected) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "TEST: wrong state of an authz user (1 enabled, 0 disabled, -1 absent)",
            "case",         "%s", casename,
            "username",     "%s", username,
            "expected",     "%d", expected,
            "got",          "%d", state,
            NULL
        );
        return -1;
    }
    printf("ok   %-60s %s=%d\n", casename, username, state);
    return 0;
}

/***************************************************************************
 *  A treedb event of the topic `users` of the broker, sent by this gobj
 ***************************************************************************/
PRIVATE int send_forged(hgobj gobj, hgobj broker, gobj_event_t event, json_t *node)
{
    int ret = gobj_send_event(
        broker,
        event,
        json_pack("{s:s, s:s, s:o}",
            "treedb_name",  BROKER_TREEDB_SERVICE,
            "topic_name",   "users",
            "node",         node
        ),
        gobj
    );
    if(ret != -1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "TEST: a forged treedb event was not refused",
            "event",        "%s", event,
            "ret",          "%d", ret,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Write the field `enabled` of a user of the treedb of the broker
 ***************************************************************************/
PRIVATE int set_broker_user_enabled(hgobj gobj, hgobj treedb, const char *username, BOOL enabled)
{
    json_t *node = gobj_update_node(
        treedb,
        "users",
        json_pack("{s:s, s:b}", "id", username, "enabled", enabled),
        0,
        gobj
    );
    if(!node) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "TEST: cannot update a user of the treedb of the broker",
            "username",     "%s", username,
            NULL
        );
        return -1;
    }
    JSON_DECREF(node)
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The broker and authz are up: run the cases, then the end
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    hgobj broker = gobj_find_service(BROKER_SERVICE, TRUE);
    hgobj treedb = gobj_find_service(BROKER_TREEDB_SERVICE, TRUE);

    check_user(gobj, "seeded: admin enabled", "admin", 1);
    check_user(gobj, "seeded: carol disabled", "carol", 0);
    check_user(gobj, "seeded: no mallory", "mallory", -1);

    /*-----------------------------------------------------------*
     *  Forged: the events sent by a gobj that is not the treedb
     *  of the broker. One WARNING for the three (the brake).
     *-----------------------------------------------------------*/
    set_expected_results(
        "forged treedb events",
        json_pack("[{s:s}]",
            "msg", REFUSED_MSG
        ),
        NULL, NULL, 1
    );
    send_forged(gobj, broker, EV_TREEDB_NODE_DELETED,
        json_pack("{s:s}", "id", "admin")
    );
    send_forged(gobj, broker, EV_TREEDB_NODE_UPDATED,
        json_pack("{s:s, s:b}", "id", "carol", "enabled", 1)
    );
    send_forged(gobj, broker, EV_TREEDB_NODE_CREATED,
        json_pack("{s:s}", "id", "mallory")
    );
    int forged_logs = test_json(NULL);
    set_expected_results(
        "test_mqtt_broker_treedb_src",
        json_array(),   // nothing else
        NULL, NULL, 1
    );
    if(forged_logs < 0) {
        // Said after the expected results of the rest: so it fails the test
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "TEST: not one WARNING for the forged treedb events",
            NULL
        );
    }

    check_user(gobj, "forged DELETED: admin still enabled", "admin", 1);
    check_user(gobj, "forged UPDATED: carol still disabled", "carol", 0);
    check_user(gobj, "forged CREATED: no mallory", "mallory", -1);

    /*-----------------------------------------------------------*
     *  The real ones: the same changes made in the treedb of
     *  the broker reach authz
     *-----------------------------------------------------------*/
    json_t *bob = gobj_create_node(
        treedb,
        "users",
        json_pack("{s:s}", "id", "bob"),
        0,
        gobj
    );
    if(!bob) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "TEST: cannot create bob in the treedb of the broker",
            NULL
        );
    }
    JSON_DECREF(bob)
    check_user(gobj, "treedb CREATED: bob created and enabled", "bob", 1);

    set_broker_user_enabled(gobj, treedb, "bob", FALSE);
    check_user(gobj, "treedb UPDATED enabled:false: bob disabled", "bob", 0);

    set_broker_user_enabled(gobj, treedb, "bob", TRUE);
    check_user(gobj, "treedb UPDATED enabled:true: bob enabled", "bob", 1);

    if(gobj_delete_node(treedb, "users", json_pack("{s:s}", "id", "bob"), 0, gobj) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "TEST: cannot delete bob in the treedb of the broker",
            NULL
        );
    }
    check_user(gobj, "treedb DELETED: bob disabled", "bob", 0);

    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}




                    /***************************
                     *      FSM
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_play    = mt_play,
    .mt_pause   = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_BROKER_TREEDB_SRC);

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
        {EV_TIMEOUT,            ac_timeout,         0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
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
        0,              // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,              // authz_table
        0,              // command_table
        s_user_trace_level,
        0               // gcflag_t
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_broker_treedb_src(void)
{
    return create_gclass(C_BROKER_TREEDB_SRC);
}
