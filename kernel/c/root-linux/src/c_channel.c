/***********************************************************************
 *          C_CHANNEL.C
 *          Channel GClass.
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>

#include <command_parser.h>
#include "msg_ievent.h"
#include "c_timer.h"
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
SDATA (DTP_BOOLEAN,     "opened",           SDF_RD,             0,              "Channel opened (opened is higher level than connected"),
SDATA (DTP_INTEGER,     "timeout",          SDF_RD,             "1000",         "Timeout"),
SDATA (DTP_INTEGER,     "txMsgs",       SDF_RD|SDF_RSTATS,      0,          "Messages transmitted"),
SDATA (DTP_INTEGER,     "rxMsgs",       SDF_RD|SDF_RSTATS,      0,          "Messages received"),

SDATA (DTP_INTEGER,     "txMsgsec",     SDF_RD|SDF_RSTATS,      0,          "Messages by second"),
SDATA (DTP_INTEGER,     "rxMsgsec",     SDF_RD|SDF_RSTATS,      0,          "Messages by second"),
SDATA (DTP_INTEGER,     "maxtxMsgsec",  SDF_WR|SDF_RSTATS,      0,          "Max Tx Messages by second"),
SDATA (DTP_INTEGER,     "maxrxMsgsec",  SDF_WR|SDF_RSTATS,      0,          "Max Rx Messages by second"),
SDATA (DTP_STRING,      "__username__",     SDF_RD,             "",             "Username"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,              "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,              "subscriber of output-events. Not a child gobj."),
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
    json_int_t txMsgs;
    json_int_t rxMsgs;
    json_int_t last_txMsgs;
    json_int_t last_rxMsgs;
    uint64_t last_ms;
} PRIVATE_DATA;

PRIVATE hgclass __gclass__ = 0;




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

/***************************************************************************
 *      Framework Method reading
 ***************************************************************************/
PRIVATE SData_Value_t mt_reading(hgobj gobj, const char *name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    SData_Value_t v = {0,{0}};
    if(strcmp(name, "txMsgs")==0) {
        v.found = 1;
        v.v.i = priv->txMsgs;
    } else if(strcmp(name, "rxMsgs")==0) {
        v.found = 1;
        v.v.i = priv->rxMsgs;
    }
    return v;
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

    hgobj child = gobj_first_child(gobj);

    while(child) {
        if(gobj_gclass_name(child)==C_TIMER) {
            child = gobj_next_child(child);
            continue;
        }
        BOOL connected = gobj_read_bool_attr(child, "connected");
        if(connected) {
            return FALSE;
        }

        child = gobj_next_child(child);
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

    set_timeout_periodic(priv->timer,gobj_read_integer_attr(gobj, "timeout"));

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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    if(all_childs_closed(gobj)) {
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

    JSON_DECREF(kw)
    return 0;
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
    if(gobj_has_event(gobj_bottom, EV_SEND_MESSAGE, 0)) {
        ret = gobj_send_event(gobj_bottom, EV_SEND_MESSAGE, kw, gobj);
    } else if(gobj_has_event(gobj_bottom, EV_TX_DATA, 0)) {
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
    priv->txMsgs++;

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
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint64_t ms = time_in_miliseconds_monotonic();
    if(!priv->last_ms) {
        priv->last_ms = ms;
    }
    json_int_t t = (json_int_t)(ms - priv->last_ms);
    if(t>0) {
        json_int_t txMsgsec = priv->txMsgs - priv->last_txMsgs;
        json_int_t rxMsgsec = priv->rxMsgs - priv->last_rxMsgs;

        txMsgsec *= 1000;
        rxMsgsec *= 1000;
        txMsgsec /= t;
        rxMsgsec /= t;

        uint64_t maxtxMsgsec = gobj_read_integer_attr(gobj, "maxtxMsgsec");
        uint64_t maxrxMsgsec = gobj_read_integer_attr(gobj, "maxrxMsgsec");
        if(txMsgsec > maxtxMsgsec) {
            gobj_write_integer_attr(gobj, "maxtxMsgsec", txMsgsec);
        }
        if(rxMsgsec > maxrxMsgsec) {
            gobj_write_integer_attr(gobj, "maxrxMsgsec", rxMsgsec);
        }

        gobj_write_integer_attr(gobj, "txMsgsec", txMsgsec);
        gobj_write_integer_attr(gobj, "rxMsgsec", rxMsgsec);
    }
    priv->last_ms = ms;
    priv->last_txMsgs = priv->txMsgs;
    priv->last_rxMsgs = priv->rxMsgs;

    JSON_DECREF(kw)
    return 0;
}


/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_enable = mt_enable,
    .mt_disable = mt_disable,
    .mt_reading = mt_reading,
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
        {EV_TIMEOUT_PERIODIC,   ac_timeout,         0},
        {EV_STOPPED,            ac_stopped,         0},
        {0,0,0}
    };

    ev_action_t st_opened[] = {
        {EV_ON_MESSAGE,         ac_on_message,      0},
        {EV_SEND_MESSAGE,       ac_send_message,    0},
        {EV_ON_ID,              ac_on_id,           0},
        {EV_ON_ID_NAK,          ac_on_id_nak,       0},
        {EV_TIMEOUT_PERIODIC,   ac_timeout,         0},
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
        {EV_ON_ID,                  EVF_OUTPUT_EVENT},
        {EV_ON_ID_NAK,              EVF_OUTPUT_EVENT},
        {EV_ON_OPEN,                EVF_OUTPUT_EVENT},
        {EV_ON_CLOSE,               EVF_OUTPUT_EVENT},

        // top input
        {EV_SEND_MESSAGE,           0},
        {EV_DROP,                   0},

        // internal
        {EV_STOPPED,                0},
        {EV_TIMEOUT_PERIODIC,       0},

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
        tattr_desc,
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
