/****************************************************************************
 *          C_ACL.C
 *
 *          Self-contained MQTT broker publish/subscribe ACL enforcement test.
 *
 *          Drives the exact enforcement path a wire PUBLISH/SUBSCRIBE takes:
 *          c_prot_mqtt2 asks the broker over EV_MQTT_ACL_CHECK (0=allow,
 *          -1=deny). This test authors the ACL data model in the broker's
 *          treedb (a group with publish_acl/subscribe_acl patterns + a client
 *          linked to it) and fires EV_MQTT_ACL_CHECK directly at the broker,
 *          asserting the verdict for matching / non-matching topics, the
 *          allow-all fallbacks (enforcement off, group with no patterns),
 *          and the unknown-client deny.
 *
 *          No sockets: the ACL decision is a direct broker event, so the test
 *          is deterministic.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include <c_mqtt_broker.h>
#include <c_prot_mqtt2.h>
#include "c_acl.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BROKER_SERVICE  "mqtt_broker"
#define TREEDB_SERVICE  "treedb_mqtt_broker"

/*---------------------------------------------*
 *      Attributes (none needed)
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
} PRIVATE_DATA;




                    /***************************
                     *      Helpers
                     ***************************/




/***************************************************************************
 *  Fire EV_MQTT_ACL_CHECK at the broker and compare the verdict.
 *  Returns 0 if the verdict matches `expected` (0=allow / -1=deny),
 *  -1 (and logs an error → fails the test) otherwise.
 ***************************************************************************/
PRIVATE int check_acl(
    hgobj gobj,
    hgobj broker,
    const char *client_id,
    const char *topic,
    const char *access,     // "write" (publish) or "read" (subscribe)
    int expected,
    const char *casename
)
{
    int verdict = gobj_send_event(
        broker,
        EV_MQTT_ACL_CHECK,
        json_pack("{s:s, s:s, s:s}",
            "client_id",    client_id,
            "topic",        topic,
            "access",       access
        ),
        gobj
    );

    if(verdict != expected) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "MQTT ACL enforcement FAILED",
            "case",         "%s", casename,
            "client_id",    "%s", client_id,
            "topic",        "%s", topic,
            "access",       "%s", access,
            "expected",     "%d", expected,
            "got",          "%d", verdict,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Author the ACL data model in the broker's treedb.
 *
 *    client_groups:
 *      g_limited  publish_acl=["allowed/#"]  subscribe_acl=["sub/+/ok"]
 *      g_open     (no patterns → allow-all)
 *    clients:
 *      c_limited  → g_limited
 *      c_open     → g_open
 *      (c_unknown is intentionally NOT created)
 ***************************************************************************/
PRIVATE int author_acl_model(hgobj gobj, hgobj treedb)
{
    int ret = 0;

    /*
     *  Groups
     */
    json_t *g_limited = gobj_create_node(
        treedb,
        "client_groups",
        json_pack("{s:s, s:s, s:[s], s:[s]}",
            "id",               "g_limited",
            "language",         "en",
            "publish_acl",      "allowed/#",
            "subscribe_acl",    "sub/+/ok"
        ),
        NULL,
        gobj
    );
    json_t *g_open = gobj_create_node(
        treedb,
        "client_groups",
        json_pack("{s:s, s:s, s:[], s:[]}",
            "id",               "g_open",
            "language",         "en",
            "publish_acl",
            "subscribe_acl"
        ),
        NULL,
        gobj
    );

    /*
     *  Clients
     */
    json_t *c_limited = gobj_create_node(
        treedb,
        "clients",
        json_pack("{s:s}", "id", "c_limited"),
        NULL,
        gobj
    );
    json_t *c_open = gobj_create_node(
        treedb,
        "clients",
        json_pack("{s:s}", "id", "c_open"),
        NULL,
        gobj
    );

    if(!g_limited || !g_open || !c_limited || !c_open) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "MQTT ACL test: cannot author treedb nodes",
            NULL
        );
        JSON_DECREF(g_limited)
        JSON_DECREF(g_open)
        JSON_DECREF(c_limited)
        JSON_DECREF(c_open)
        return -1;
    }

    /*
     *  Link each client to its group (hook "clients" on client_groups →
     *  sets the child clients.client_groups fkey, which mqtt_acl_check reads).
     *  link_nodes owns the parent/child records.
     */
    ret += gobj_link_nodes(
        treedb,
        "clients",          // hook (on parent client_groups)
        "client_groups",    // parent topic
        g_limited,          // owned
        "clients",          // child topic
        c_limited,          // owned
        gobj
    );
    ret += gobj_link_nodes(
        treedb,
        "clients",
        "client_groups",
        g_open,             // owned
        "clients",
        c_open,             // owned
        gobj
    );

    if(ret < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "MQTT ACL test: cannot link client to group",
            NULL
        );
    }
    return ret;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Timer fired: broker + treedb are up. Author the model and run the matrix.
 ***************************************************************************/
PRIVATE int ac_run(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    hgobj broker = gobj_find_service(BROKER_SERVICE, TRUE);
    hgobj treedb = gobj_find_service(TREEDB_SERVICE, TRUE);

    if(!broker || !treedb) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "MQTT ACL test: broker/treedb service not found",
            NULL
        );
        set_yuno_must_die();
        JSON_DECREF(kw)
        return -1;
    }

    if(author_acl_model(gobj, treedb) < 0) {
        set_yuno_must_die();
        JSON_DECREF(kw)
        return -1;
    }

    /*-----------------------------------------------------------------*
     *  Enforcement ON (the broker was created with enable_acl=true)
     *-----------------------------------------------------------------*/
    /* publish-side (write) */
    check_acl(gobj, broker, "c_limited", "allowed/data",     "write",  0, "limited: matching publish topic allowed");
    check_acl(gobj, broker, "c_limited", "allowed/a/b/c",    "write",  0, "limited: '#' wildcard publish allowed");
    check_acl(gobj, broker, "c_limited", "allowed",          "write",  0, "limited: '#' matches the parent level too (MQTT spec)");
    check_acl(gobj, broker, "c_limited", "forbidden/data",   "write", -1, "limited: non-matching publish topic DENIED");
    check_acl(gobj, broker, "c_limited", "other",            "write", -1, "limited: unrelated publish topic DENIED");

    /* subscribe-side (read) */
    check_acl(gobj, broker, "c_limited", "sub/x/ok",         "read",   0, "limited: matching subscribe filter allowed");
    check_acl(gobj, broker, "c_limited", "sub/x/bad",        "read",  -1, "limited: non-matching subscribe filter DENIED");

    /* group with no patterns → allow-all (backward compat) */
    check_acl(gobj, broker, "c_open",    "anything/at/all",  "write",  0, "open group (no patterns): publish allow-all");
    check_acl(gobj, broker, "c_open",    "anything/at/all",  "read",   0, "open group (no patterns): subscribe allow-all");

    /* unknown client with ACL on → deny */
    check_acl(gobj, broker, "c_unknown", "allowed/data",     "write", -1, "unknown client with ACL on DENIED");

    /*-----------------------------------------------------------------*
     *  Enforcement OFF → everything allowed (historical behaviour)
     *-----------------------------------------------------------------*/
    gobj_write_bool_attr(broker, "enable_acl", FALSE);
    check_acl(gobj, broker, "c_limited", "forbidden/data",   "write",  0, "enable_acl off: previously-denied publish now allowed");
    check_acl(gobj, broker, "c_unknown", "forbidden/data",   "write",  0, "enable_acl off: unknown client allowed");
    gobj_write_bool_attr(broker, "enable_acl", TRUE);

    set_yuno_must_die();
    JSON_DECREF(kw)
    return 0;
}




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

    gobj_stop(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /* Give the broker + treedb a moment to finish starting/playing */
    set_timeout(priv->timer, 500);
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
                     *      FSM
                     ***************************/




PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_play    = mt_play,
    .mt_pause   = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_ACL);

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

    ev_action_t st_idle[] = {
        {EV_TIMEOUT,    ac_run,     0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,    0},
        {0, 0}
    };

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
        0,              // s_user_trace_level
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
PUBLIC int register_c_acl(void)
{
    return create_gclass(C_ACL);
}
