/***********************************************************************
 *          C_CHANNEL.C
 *          Channel GClass.
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>
#include "c_channel.h"


/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef enum {
    st_open     = 0x0001,

} my_user_data2_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
//TODO queue tx

PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag----------------default---------description---------- */
SDATA (ASN_OCTET_STR,   "model",            SDF_RD,             "one-over-all", "Distribution model over tubes: one-over-all|one-over-one|all-over-all"),

SDATA (ASN_OCTET_STR,   "lHost",            SDF_RD,             0,              "Local host bind filter"),
SDATA (ASN_OCTET_STR,   "lPort",            SDF_RD,             0,              "Local port bind filter"),
// TODO remove this when all configuration jsons have allowd_urls,denied_urls deleted
SDATA (ASN_JSON,        "allowd_urls",      SDF_RD,             0,              "DEPRECATED Peer allowed url's filter"),
SDATA (ASN_JSON,        "denied_urls",      SDF_RD,             0,              "DEPRECATED Peer denied url's filter"),
SDATA (ASN_INTEGER,     "timeout",          SDF_RD,             1*1000,         "Timeout"),
SDATA (ASN_BOOLEAN,     "opened",           SDF_RD,             0,              "Channel opened (opened is higher level than connected"),
SDATA (ASN_COUNTER64,   "txMsgs",           SDF_WR,             0,              "Messages transmitted"),
SDATA (ASN_COUNTER64,   "rxMsgs",           SDF_WR,             0,              "Messages received"),
SDATA (ASN_COUNTER64,   "txMsgsec",         SDF_WR,             0,              "Messages by second"),
SDATA (ASN_COUNTER64,   "rxMsgsec",         SDF_WR,             0,              "Messages by second"),
SDATA (ASN_COUNTER64,   "maxtxMsgsec",      SDF_WR,             0,              "Max Messages by second"),
SDATA (ASN_COUNTER64,   "maxrxMsgsec",      SDF_WR,             0,              "Max Messages by second"),
SDATA (ASN_OCTET_STR,   "__username__",     SDF_RD,             "",             "Username"),
SDATA (ASN_POINTER,     "user_data",        0,                  0,              "user data"),
SDATA (ASN_POINTER,     "user_data2",       0,                  0,              "more user data"),
SDATA (ASN_POINTER,     "subscriber",       0,                  0,              "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_CONNECTION = 0x0001,
    TRACE_MESSAGES    = 0x0002,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"connection",      "Trace connections of channels"},
{"messages",        "Trace messages of channels"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    uint64_t *ptxMsgs;
    uint64_t *prxMsgs;
    uint64_t last_txMsgs;
    uint64_t last_rxMsgs;
    uint64_t last_ms;
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

    priv->timer = gobj_create("", GCLASS_TIMER, 0, gobj);
    priv->ptxMsgs = gobj_danger_attr_ptr(gobj, "txMsgs");
    priv->prxMsgs = gobj_danger_attr_ptr(gobj, "rxMsgs");

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);
}

/***************************************************************************
 *      Framework Method enable
 ***************************************************************************/
PRIVATE int mt_enable(hgobj gobj)
{
    if(!gobj_is_running(gobj)) {
        return gobj_start(gobj); // Do gobj_start_tree()
    }
    return 0;
}

/***************************************************************************
 *      Framework Method disable
 ***************************************************************************/
PRIVATE int mt_disable(hgobj gobj)
{
    if(gobj_is_running(gobj)) {
        return gobj_stop(gobj); // gobj_stop_tree
    }
    return 0;
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    gobj_start_tree(gobj);
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
    gobj_stop_tree(gobj);
    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL all_childs_closed(hgobj gobj)
{
    BOOL all_closed = TRUE;

    hgobj child; rc_instance_t *i_child;
    i_child = gobj_first_child(gobj, &child);

    while(i_child) {
        my_user_data2_t my_user_data2 = (my_user_data2_t)gobj_read_pointer_attr(child, "user_data2");
        if(my_user_data2 & st_open) { // V547 Expression 'my_user_data2 & st_open' is always true.
            return FALSE;
        }

        i_child = gobj_next_child(i_child, &child);
    }

    return all_closed;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    my_user_data2_t my_user_data2 = (my_user_data2_t)gobj_read_pointer_attr(src, "user_data2");
    my_user_data2 |= st_open;
    gobj_write_pointer_attr(src, "user_data2", (void *)my_user_data2);

    /*
     *  This action is called only in ST_CLOSED state.
     *  The first tube being connected will cause this.
     */
    if(gobj_trace_level(gobj) & TRACE_CONNECTION) {
        log_info(0,
            "gobj",             "%s", gobj_full_name(gobj),
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_OPEN_CLOSE,
            "msg",              "%s", "ON_OPEN",
            "tube",             "%s", gobj_name(src),
            NULL
        );
    }
    gobj_write_bool_attr(gobj, "opened", TRUE);

    set_timeout_periodic(priv->timer,gobj_read_int32_attr(gobj, "timeout"));

    gobj_publish_event(gobj, event, kw);  // reuse kw
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    my_user_data2_t my_user_data2 = (my_user_data2_t)gobj_read_pointer_attr(src, "user_data2");
    my_user_data2 &= ~st_open;
    gobj_write_pointer_attr(src, "user_data2", (void *)my_user_data2);

    if(all_childs_closed(gobj)) {
        if(gobj_trace_level(gobj) & TRACE_CONNECTION) {
            log_info(0,
                "gobj",             "%s", gobj_full_name(gobj),
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_OPEN_CLOSE,
                "msg",              "%s", "ON_CLOSE",
                "tube",             "%s", gobj_name(src),
                NULL
            );
        }
        gobj_change_state(gobj, "ST_CLOSED");
        gobj_write_bool_attr(gobj, "opened", FALSE);
        gobj_publish_event(gobj, event, 0);
    }

    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        log_debug_json(
            LOG_DUMP_INPUT,
            kw, // not own
            "%s", gobj_short_name(src)
        );
    }
    (*priv->prxMsgs)++;

    gobj_publish_event(gobj, event, kw); // reuse kw
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_id(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        log_debug_json(
            LOG_DUMP_INPUT,
            kw, // not own
            "%s", gobj_short_name(src)
        );
    }
    (*priv->prxMsgs)++;

    gobj_publish_event(gobj, event, kw); // reuse kw
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_id_nak(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        log_debug_json(
            LOG_DUMP_INPUT,
            kw, // not own
            "%s", gobj_short_name(src)
        );
    }
    (*priv->prxMsgs)++;

    gobj_publish_event(gobj, event, kw); // reuse kw
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    // TODO este manejo de tubes no está bien!, los child podrían ser timers, o su putam.
    hgobj child; rc_instance_t *i_child;
    i_child = gobj_first_child(gobj, &child);
    while(i_child) {
        my_user_data2_t my_user_data2 = (my_user_data2_t)gobj_read_pointer_attr(child, "user_data2");
        if(my_user_data2 & st_open) { // V547 Expression 'my_user_data2 & st_open' is always true.
            if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
                log_debug_json(
                    LOG_DUMP_OUTPUT,
                    kw, // not own
                    "%s", gobj_short_name(child)
                );
            }
            KW_INCREF(kw);
            if(gobj_event_in_input_event_list(child, "EV_SEND_MESSAGE", 0)) {
                ret += gobj_send_event(child, "EV_SEND_MESSAGE", kw, gobj);
            } else if(gobj_event_in_input_event_list(child, "EV_TX_DATA", 0)) {
                ret += gobj_send_event(child, "EV_TX_DATA", kw, gobj);
            } else {
                kw_decref(kw);
                log_error(LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "Child without EV_SEND_MESSAGE or EV_TX_DATA input event",
                    "child",        "%s", gobj_short_name(child),
                    NULL
                );
                break;
            }
            (*priv->ptxMsgs)++;
            break;  // TODO de momento one-over-all
        }
        i_child = gobj_next_child(i_child, &child);
    }
    KW_DECREF(kw);

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    //TODO si envian el tube to drop, úsalo, no cortes todos los tube

    hgobj child; rc_instance_t *i_child;
    i_child = gobj_first_child(gobj, &child);

    while(i_child) {
        my_user_data2_t my_user_data2 = (my_user_data2_t)gobj_read_pointer_attr(child, "user_data2");
        if(my_user_data2 & st_open) { // TODO V547 Expression 'my_user_data2 & st_open' is always true.
            gobj_send_event(child, "EV_DROP", 0, gobj);
        }
        i_child = gobj_next_child(i_child, &child);
    }

    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  This event comes from clisrv TCP gobjs
 *  that haven't found a free server link.
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw);

    if(gobj_typeof_subgclass(src, GCLASS_TCP0_NAME)) {
        gobj_destroy(src);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint64_t ms = time_in_miliseconds();
    if(!priv->last_ms) {
        priv->last_ms = ms;
    }
    uint64_t t = (ms - priv->last_ms);
    if(t>0) {
        uint64_t txMsgsec = *(priv->ptxMsgs) - priv->last_txMsgs;
        uint64_t rxMsgsec = *(priv->prxMsgs) - priv->last_rxMsgs;

        txMsgsec *= 1000;
        rxMsgsec *= 1000;
        txMsgsec /= t;
        rxMsgsec /= t;

        uint64_t maxtxMsgsec = gobj_read_uint64_attr(gobj, "maxtxMsgsec");
        uint64_t maxrxMsgsec = gobj_read_uint64_attr(gobj, "maxrxMsgsec");
        if(txMsgsec > maxtxMsgsec) {
            gobj_write_uint64_attr(gobj, "maxtxMsgsec", txMsgsec);
        }
        if(rxMsgsec > maxrxMsgsec) {
            gobj_write_uint64_attr(gobj, "maxrxMsgsec", rxMsgsec);
        }

        gobj_write_uint64_attr(gobj, "txMsgsec", txMsgsec);
        gobj_write_uint64_attr(gobj, "rxMsgsec", rxMsgsec);
    }
    priv->last_ms = ms;
    priv->last_txMsgs = *(priv->ptxMsgs);
    priv->last_rxMsgs = *(priv->prxMsgs);

    JSON_DECREF(kw);
    return 0;
}


/***************************************************************************
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = {
    // top input
    {"EV_SEND_MESSAGE",             0,  0,  "Send a message"},
    {"EV_DROP",                     0,  0,  "Drop connection"},
    // bottom input
    {"EV_ON_MESSAGE",               0,  0,  0},
    {"EV_ON_ID",                    0,  0,  0},
    {"EV_ON_ID_NAK",                0,  0,  0},
    {"EV_ON_OPEN",                  0,  0,  0},
    {"EV_ON_CLOSE",                 0,  0,  0},
    {"EV_ON_SETUP",                 0,  0,  0},
    {"EV_ON_SETUP_COMPLETE",        0,  0,  0},
    // internal
    {"EV_TIMEOUT",                  0,  0,  0},
    {"EV_STOPPED",                  0,  0,  0},
    {NULL, 0}
};
PRIVATE const EVENT output_events[] = {
    {"EV_ON_MESSAGE",               0,   0,  "Message received"},
    {"EV_ON_ID",                    0,   0,  "Id received"},
    {"EV_ON_ID_NAK",                0,   0,  "Id refused"},
    {"EV_ON_OPEN",                  0,   0,  "Channel opened"},
    {"EV_ON_CLOSE",                 0,   0,  "Channel closed"},
    {NULL, 0}
};
PRIVATE const char *state_names[] = {
    "ST_CLOSED",
    "ST_OPENED",
    NULL
};

PRIVATE EV_ACTION ST_CLOSED[] = {
    {"EV_ON_OPEN",              ac_on_open,         "ST_OPENED"},
    {"EV_TIMEOUT",              ac_timeout,         0},
    {"EV_ON_SETUP",             0,                  0},
    {"EV_ON_SETUP_COMPLETE",    0,                  0},
    {"EV_STOPPED",              ac_stopped,         0},
    {0,0,0}
};
PRIVATE EV_ACTION ST_OPENED[] = {
    {"EV_ON_MESSAGE",           ac_on_message,      0},
    {"EV_SEND_MESSAGE",         ac_send_message,    0},
    {"EV_ON_ID",                ac_on_id,           0},
    {"EV_ON_ID_NAK",            ac_on_id_nak,       0},
    {"EV_TIMEOUT",              ac_timeout,         0},
    {"EV_DROP",                 ac_drop,            0},
    {"EV_ON_OPEN",              0,                  0},
    {"EV_ON_CLOSE",             ac_on_close,        0},
    {"EV_STOPPED",              ac_stopped,         0},
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_CLOSED,
    ST_OPENED,
    NULL
};

PRIVATE FSM fsm = {
    input_events,
    output_events,
    state_names,
    states,
};

/***************************************************************************
 *              GClass
 ***************************************************************************/
/*---------------------------------------------*
 *              Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {0, 0, 0}
};

/*---------------------------------------------*
 *              GClass
 *---------------------------------------------*/
PRIVATE GCLASS _gclass = {
    0,  // base
    GCLASS_CHANNEL_NAME,
    &fsm,
    {
        mt_create,
        0, //mt_create2,
        0, //mt_destroy,
        mt_start,
        mt_stop,
        0, //mt_play,
        0, //mt_pause,
        0, //mt_writing,
        0, //mt_reading,
        0, //mt_subscription_added,
        0, //mt_subscription_deleted,
        0, //mt_child_added,
        0, //mt_child_removed,
        0, //mt_stats,
        0, //mt_command,
        0, //mt_inject_event,
        0, //mt_create_resource,
        0, //mt_list_resource,
        0, //mt_save_resource,
        0, //mt_delete_resource,
        0, //mt_future21
        0, //mt_future22
        0, //mt_get_resource
        0, //mt_state_changed,
        0, //mt_authenticate,
        0, //mt_list_childs,
        0, //mt_stats_updated,
        mt_disable,
        mt_enable,
        0, //mt_trace_on,
        0, //mt_trace_off,
        0, //mt_gobj_created,
        0, //mt_future33,
        0, //mt_future34,
        0, //mt_publish_event,
        0, //mt_publication_pre_filter,
        0, //mt_publication_filter,
        0, //mt_authz_checker,
        0, //mt_future39,
        0, //mt_create_node,
        0, //mt_update_node,
        0, //mt_delete_node,
        0, //mt_link_nodes,
        0, //mt_future44,
        0, //mt_unlink_nodes,
        0, //mt_topic_jtree,
        0, //mt_get_node,
        0, //mt_list_nodes,
        0, //mt_shoot_snap,
        0, //mt_activate_snap,
        0, //mt_list_snaps,
        0, //mt_treedbs,
        0, //mt_treedb_topics,
        0, //mt_topic_desc,
        0, //mt_topic_links,
        0, //mt_topic_hooks,
        0, //mt_node_parents,
        0, //mt_node_childs,
        0, //mt_list_instances,
        0, //mt_node_tree,
        0, //mt_topic_size,
        0, //mt_future62,
        0, //mt_future63,
        0, //mt_future64
    },
    lmt,
    tattr_desc,
    sizeof(PRIVATE_DATA),
    0, // acl
    s_user_trace_level,
    0, // cmds
    0, // gcflag
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC GCLASS *gclass_channel(void)
{
    return &_gclass;
}
