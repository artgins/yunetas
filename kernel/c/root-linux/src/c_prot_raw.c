/***********************************************************************
 *          C_PROT_RAW.C
 *          Prot_raw GClass.
 *
 *          Raw protocol, no insert/deletion of headers
 *
 *          Copyright (c) 2017 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_states.h>
#include <helpers.h>
#include "c_prot_raw.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag------------default---------description---------- */
SDATA (DTP_BOOLEAN,     "connected",        SDF_RD|SDF_STATS,0,              "Connection state. Important filter!"),
SDATA (DTP_POINTER,     "user_data",        0,              0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,              0,              "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,              0,              "subscriber of output-events. If it's null then subscriber is the parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_USER = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"trace_user",        "Trace user description"},
{0, 0},
};


/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    const char *on_open_event_name;
    const char *on_close_event_name;
    const char *on_message_event_name;
    int inform_on_close;
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

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber)
        subscriber = gobj_parent(gobj);
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(on_open_event_name,    gobj_read_str_attr)
    SET_PRIV(on_close_event_name,   gobj_read_str_attr)
    SET_PRIV(on_message_event_name, gobj_read_str_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
//     PRIVATE_DATA *priv = gobj_priv_data(gobj);
//
//     IF_EQ_SET_PRIV(sample_int,              gobj_read_integer_attr)
//     ELIF_EQ_SET_PRIV(sample_str,            gobj_read_str_attr)
//     END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    gobj_start_children(gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    gobj_stop_children(gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}




            /***************************
             *      Local Methods
             ***************************/




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_bool_attr(gobj, "connected", TRUE);
    gobj_change_state(gobj, ST_SESSION);

    priv->inform_on_close = TRUE;
    if(!empty_string(priv->on_open_event_name)) {
        gobj_publish_event(gobj, priv->on_open_event_name, 0);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_is_volatil(src)) {
        gobj_set_bottom_gobj(gobj, 0);
    }
    gobj_write_bool_attr(gobj, "connected", FALSE);

    if(priv->inform_on_close) {
        priv->inform_on_close = FALSE;
        if(!empty_string(priv->on_close_event_name)) {
            gobj_publish_event(gobj, priv->on_close_event_name, 0);
        }
    }

    KW_DECREF(kw);
    return 0;
}

/********************************************************************
 *
 ********************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gobj_publish_event(gobj, priv->on_message_event_name, kw); // reuse kw
    return 0;
}

/********************************************************************
 *
 ********************************************************************/
PRIVATE int ac_send_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    return gobj_send_event(gobj_bottom_gobj(gobj), EV_TX_DATA, kw, gobj);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Child stopped
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
    JSON_DECREF(kw);
    return 0;
}

/***********************************************************************
 *          FSM
 ***********************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_writing = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_PROT_RAW);

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
    ev_action_t st_disconnected[] = {
        {EV_CONNECTED,         ac_connected,              0},
        {EV_DISCONNECTED,      ac_disconnected,           0},
        {EV_STOPPED,           ac_stopped,                0},
        {0,0,0}
    };
    ev_action_t st_wait_connected[] = {
        {EV_CONNECTED,         ac_connected,              0},
        {EV_DISCONNECTED,      ac_disconnected,           ST_DISCONNECTED},
        {0,0,0}
    };
    ev_action_t st_session[] = {
        {EV_RX_DATA,           ac_rx_data,                0},
        {EV_SEND_MESSAGE,      ac_send_message,           0},
        {EV_TX_READY,          0,                         0},
        {EV_DROP,              ac_drop,                   0},
        {EV_DISCONNECTED,      ac_disconnected,           ST_DISCONNECTED},
        {0,0,0}
    };

    states_t states[] = {
        {ST_DISCONNECTED,      st_disconnected},
        {ST_WAIT_CONNECTED,    st_wait_connected},
        {ST_SESSION,           st_session},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_RX_DATA,           0},
        {EV_SEND_MESSAGE,      0},
        {EV_ON_OPEN,           EVF_OUTPUT_EVENT},
        {EV_ON_CLOSE,          EVF_OUTPUT_EVENT},
        {EV_ON_MESSAGE,        EVF_OUTPUT_EVENT},
        {EV_CONNECTED,         0},
        {EV_DISCONNECTED,      0},
        {EV_DROP,              0},
        {EV_TX_READY,          0},
        {EV_STOPPED,           0},

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
        0,  // lmt
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,  // authz_table
        0,  // command_table
        s_user_trace_level,
        0   // gcflag
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_prot_raw(void)
{
    return create_gclass(C_PROT_RAW);
}
