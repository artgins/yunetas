/***********************************************************************
 *          C_CHANNEL.C
 *          Channel GClass.
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include <command_parser.h>
#include "msg_ievent.h"
#include "c_channel.h"


/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE json_t *local_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/

PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description---------- */
SDATA (DTP_BOOLEAN,     "opened",           SDF_RD,             0,          "Channel opened (opened is higher level than connected"),
SDATA (DTP_STRING,      "__username__",     SDF_RD,             "",         "Username"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
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
    json_int_t txMsgs;
    json_int_t rxMsgs;
    json_int_t last_txMsgs;
    json_int_t last_rxMsgs;

    json_int_t txMsgsec;
    json_int_t rxMsgsec;
    json_int_t maxtxMsgsec;
    json_int_t maxrxMsgsec;

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
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    hgobj gobj_bottom = gobj_bottom_gobj(gobj);
    if(gobj_bottom && gobj_is_running(gobj_bottom)) {
        gobj_stop(gobj_bottom);
    }
    return 0;
}

/***************************************************************************
 *      Framework Method stats
 ***************************************************************************/
PRIVATE json_t *mt_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src)
{
    return local_stats(gobj, stats, kw, src);
}



                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *local_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(stats && strcmp(stats, "__reset__")==0) {
        priv->txMsgs = 0;
        priv->rxMsgs = 0;
        priv->last_txMsgs = 0;
        priv->last_rxMsgs = 0;

        priv->txMsgsec = 0;
        priv->rxMsgsec = 0;
        priv->maxtxMsgsec = 0;
        priv->maxrxMsgsec = 0;

        priv->last_ms = 0;
    }

    json_t *jn_data = json_object();

    /*
     *  Local stats
     */
    uint64_t ms = time_in_milliseconds_monotonic();
    if(!priv->last_ms) {
        priv->last_ms = ms;
    }
    json_int_t t = (json_int_t)(ms - priv->last_ms)/1000;
    if(t>0) {
        json_int_t txMsgsec = priv->txMsgs - priv->last_txMsgs;
        json_int_t rxMsgsec = priv->rxMsgs - priv->last_rxMsgs;

        txMsgsec /= t;
        rxMsgsec /= t;

        json_int_t maxtxMsgsec = priv->maxtxMsgsec;
        json_int_t maxrxMsgsec = priv->maxrxMsgsec;
        if(txMsgsec > maxtxMsgsec) {
            priv->maxtxMsgsec =  txMsgsec;
        }
        if(rxMsgsec > maxrxMsgsec) {
            priv->maxrxMsgsec = rxMsgsec;
        }

        priv->txMsgsec = txMsgsec;
        priv->rxMsgsec = rxMsgsec;
    }

    priv->last_ms = ms;
    priv->last_txMsgs = priv->txMsgs;
    priv->last_rxMsgs = priv->rxMsgs;

    json_object_set_new(jn_data, "txMsgs", json_integer(priv->txMsgs));
    json_object_set_new(jn_data, "rxMsgs", json_integer(priv->rxMsgs));
    json_object_set_new(jn_data, "txMsgsec", json_integer(priv->txMsgsec));
    json_object_set_new(jn_data, "rxMsgsec", json_integer(priv->rxMsgsec));
    json_object_set_new(jn_data, "maxtxMsgsec", json_integer(priv->maxtxMsgsec));
    json_object_set_new(jn_data, "maxrxMsgsec", json_integer(priv->maxrxMsgsec));

    KW_DECREF(kw)
    return jn_data;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    /*
     *  This action is called only in ST_CLOSED state.
     *  The first tube being connected will cause this.
     */
    if(gobj_trace_level(gobj) & TRACE_CONNECTION) {
        gobj_log_info(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_OPEN_CLOSE,
            "msg",              "%s", "ON_OPEN",
            "tube",             "%s", gobj_name(src),
            NULL
        );
    }
    gobj_write_bool_attr(gobj, "opened", TRUE);

    /*
     *  CHILD subscription model
     */
    if(gobj_is_service(gobj)) {
        return gobj_publish_event(gobj, event, kw);  // reuse kw
    } else {
        return gobj_send_event(gobj_parent(gobj), event, kw, gobj); // reuse kw
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    if(gobj_trace_level(gobj) & TRACE_CONNECTION) {
        gobj_log_info(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_OPEN_CLOSE,
            "msg",              "%s", "ON_CLOSE",
            "tube",             "%s", gobj_name(src),
            NULL
        );
    }
    gobj_change_state(gobj, ST_CLOSED);
    gobj_write_bool_attr(gobj, "opened", FALSE);

    /*
     *  CHILD subscription model
     */
    if(gobj_is_service(gobj)) {
        return gobj_publish_event(gobj, event, kw);  // reuse kw
    } else {
        return gobj_send_event(gobj_parent(gobj), event, kw, gobj); // reuse kw
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(
            gobj,
            kw, // not own
            "%s", gobj_short_name(src)
        );
    }
    priv->rxMsgs++;

    /*
     *  CHILD subscription model
     */
    if(gobj_is_service(gobj)) {
        return gobj_publish_event(gobj, event, kw);  // reuse kw
    } else {
        return gobj_send_event(gobj_parent(gobj), event, kw, gobj); // reuse kw
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_iev_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(
            gobj,
            kw, // not own
            "%s", gobj_short_name(src)
        );
    }
    priv->rxMsgs++;

    /*
     *  CHILD subscription model
     */
    if(gobj_is_service(gobj)) {
        return gobj_publish_event(gobj, event, kw);  // reuse kw
    } else {
        return gobj_send_event(gobj_parent(gobj), event, kw, gobj); // reuse kw
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_id(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(
            gobj,
            kw, // not own
            "%s", gobj_short_name(src)
        );
    }
    priv->rxMsgs++;

    /*
     *  CHILD subscription model
     */
    if(gobj_is_service(gobj)) {
        return gobj_publish_event(gobj, event, kw);  // reuse kw
    } else {
        return gobj_send_event(gobj_parent(gobj), event, kw, gobj); // reuse kw
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_id_nak(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(
            gobj,
            kw, // not own
            "%s", gobj_short_name(src)
        );
    }
    priv->rxMsgs++;

    /*
     *  CHILD subscription model
     */
    if(gobj_is_service(gobj)) {
        return gobj_publish_event(gobj, event, kw);  // reuse kw
    } else {
        return gobj_send_event(gobj_parent(gobj), event, kw, gobj); // reuse kw
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    hgobj gobj_bottom = gobj_bottom_gobj(gobj);
    if(!gobj_bottom) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "No bottom gobj",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(
            gobj,
            kw, // not own
            "%s => %s",
            gobj_short_name(gobj),
            gobj_short_name(gobj_bottom)
        );
    }
    KW_INCREF(kw)

    // TODO cache if EV_SEND_MESSAGE or EV_TX_DATA, use new mt_set_bottom_gobj()
    if(gobj_has_event(gobj_bottom, EV_SEND_MESSAGE, 0)) {
        priv->txMsgs++;
        ret = gobj_send_event(gobj_bottom, EV_SEND_MESSAGE, kw, gobj);
    } else if(gobj_has_event(gobj_bottom, EV_TX_DATA, 0)) {
        priv->txMsgs++;
        ret = gobj_send_event(gobj_bottom, EV_TX_DATA, kw, gobj);
    } else {
        kw_decref(kw);
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Bottom without EV_SEND_MESSAGE or EV_TX_DATA input event",
            "child",        "%s", gobj_short_name(gobj_bottom),
            NULL
        );
        ret = -1;
    }

    KW_DECREF(kw)
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_send_iev(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj gobj_bottom = gobj_bottom_gobj(gobj);
    if(!gobj_bottom) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "No bottom gobj",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    gobj_event_t iev_event = (gobj_event_t)(uintptr_t)kw_get_int(
        gobj, kw, "event", 0, KW_REQUIRED
    );
    json_t *iev_kw = kw_get_dict(gobj, kw, "kw", 0, KW_REQUIRED|KW_EXTRACT);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(
            gobj,
            iev_kw, // not own
            "%s: %s => %s",
            iev_event,
            gobj_short_name(gobj),
            gobj_short_name(gobj_bottom)
        );
    }

    int ret = 0;

    // TODO cache if EV_SEND_MESSAGE or EV_TX_DATA, use new mt_set_bottom_gobj()
    if(gobj_has_event(gobj_bottom, iev_event, 0)) {
        priv->txMsgs++;
        ret = gobj_send_event(gobj_bottom, iev_event, kw_incref(iev_kw), gobj);
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Bottom without iev_event",
            "child",        "%s", gobj_short_name(gobj_bottom),
            "iev_event",    "%s", iev_event,
            NULL
        );
        ret = -1;
    }

    KW_DECREF(iev_kw)
    KW_DECREF(kw)
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    hgobj gobj_bottom = gobj_bottom_gobj(gobj);
    if(!gobj_bottom) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "No bottom gobj",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    gobj_send_event(gobj_bottom, EV_DROP, 0, gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  This event comes from clisrv TCP gobjs
 *  that haven't found a free server link.
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)

    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }

    return 0;
}


/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_enable  = mt_enable,
    .mt_disable = mt_disable,
    .mt_stats   = mt_stats,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_CHANNEL);

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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_closed[] = {
        {EV_ON_OPEN,            ac_on_open,         ST_OPENED},
        {EV_STOPPED,            ac_stopped,         0},
        {0,0,0}
    };

    ev_action_t st_opened[] = {
        {EV_ON_MESSAGE,         ac_on_message,      0},
        {EV_ON_IEV_MESSAGE,     ac_on_iev_message,  0},
        {EV_SEND_MESSAGE,       ac_send_message,    0},
        {EV_SEND_IEV,           ac_send_iev,        0},
        {EV_ON_ID,              ac_on_id,           0},
        {EV_ON_ID_NAK,          ac_on_id_nak,       0},
        {EV_DROP,               ac_drop,            0},
        {EV_ON_OPEN,            0,                  0},
        {EV_ON_CLOSE,           ac_on_close,        0},
        {EV_STOPPED,            ac_stopped,         0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_CLOSED,             st_closed},
        {ST_OPENED,             st_opened},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_MESSAGE,             EVF_OUTPUT_EVENT},
        {EV_ON_IEV_MESSAGE,         EVF_OUTPUT_EVENT},
        {EV_SEND_MESSAGE,           0},
        {EV_SEND_IEV,               0},
        {EV_ON_ID,                  EVF_OUTPUT_EVENT},
        {EV_ON_ID_NAK,              EVF_OUTPUT_EVENT},
        {EV_ON_OPEN,                EVF_OUTPUT_EVENT},
        {EV_ON_CLOSE,               EVF_OUTPUT_EVENT},

        // top input
        {EV_DROP,                   0},

        // internal
        {EV_STOPPED,                0},

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
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,
        0   // gcflag_t
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
PUBLIC int register_c_channel(void)
{
    return create_gclass(C_CHANNEL);
}
