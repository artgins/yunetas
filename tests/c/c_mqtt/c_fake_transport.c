/****************************************************************************
 *          C_FAKE_TRANSPORT.C
 *
 *          A transport with no socket, for the tests that drive an MQTT
 *          protocol gclass by hand: it is set as the BOTTOM of the protocol
 *          gobj, what the protocol writes (EV_TX_DATA) goes to the `sink`
 *          gobj as EV_RX_DATA, and a drop (EV_DROP) comes back to the
 *          protocol as EV_DISCONNECTED on the next cycle of the loop, as a
 *          C_TCP does when its socket closes.
 *
 *          The driver writes the peer's bytes into the protocol itself:
 *
 *              gobj_send_event(prot, EV_CONNECTED, 0, fake);
 *              gobj_send_event(prot, EV_RX_DATA, {"gbuffer": gbuf}, fake);
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "c_fake_transport.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type----------name-----------flag------------default---------description---------*/
SDATA (DTP_STRING,    "peername",    SDF_RD,         "127.0.0.1:1", "Peer, as a C_TCP says it"),
SDATA (DTP_STRING,    "sockname",    SDF_RD,         "127.0.0.1:2", "Local, as a C_TCP says it"),
SDATA (DTP_POINTER,   "sink",        0,              0,             "Who receives what the protocol writes, as EV_RX_DATA"),
SDATA (DTP_INTEGER,   "drops",       SDF_RD,         "0",           "EV_DROPs received"),
SDATA (DTP_POINTER,   "user_data",   0,              0,             "user data"),
SDATA (DTP_POINTER,   "subscriber",  0,              0,             "subscriber of output-events"),
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
    hgobj sink;
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

    priv->sink = gobj_read_pointer_attr(gobj, "sink");
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(strcmp(path, "sink")==0) {
        priv->sink = gobj_read_pointer_attr(gobj, "sink");
    }
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




                    /***************************
                     *      Local Methods
                     ***************************/




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  What the protocol writes goes to the sink
 ***************************************************************************/
PRIVATE int ac_tx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->sink) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: fake transport without sink",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }
    return gobj_send_event(priv->sink, EV_RX_DATA, kw, gobj);
}

/***************************************************************************
 *  A drop closes the "socket": the protocol hears it on the next cycle
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_write_integer_attr(gobj, "drops", gobj_read_integer_attr(gobj, "drops") + 1);
    gobj_post_event(gobj_parent(gobj), EV_DISCONNECTED, 0, gobj);

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
    .mt_writing = mt_writing,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_FAKE_TRANSPORT);

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
        {EV_TX_DATA,            ac_tx_data,         0},
        {EV_DROP,               ac_drop,            0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TX_DATA,            0},
        {EV_DROP,               0},
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
PUBLIC int register_c_fake_transport(void)
{
    return create_gclass(C_FAKE_TRANSPORT);
}
