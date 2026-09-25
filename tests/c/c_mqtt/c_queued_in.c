/****************************************************************************
 *          C_QUEUED_IN.C
 *
 *          Driver of the test of the incoming QoS 2 messages QUEUED at the
 *          reload of a persistent session.
 *
 *          The client is RAW: a C_TCP of this gclass that writes the MQTT
 *          3.1.1 packets by hand, so every packet the broker answers is seen
 *          as it is on the wire.
 *
 *              1. Session 1 (clean session 0, max_inflight_messages 20 in
 *                 the broker): four QoS 2 PUBLISH, packet ids 1..4. The
 *                 broker answers four PUBREC and keeps them waiting for
 *                 their PUBREL. The client goes away without PUBREL.
 *              2. The broker's max_inflight_messages goes down to 2.
 *              3. Session 2 (the same client id): the broker reloads the
 *                 four messages, 1 and 2 in flight, 3 and 4 QUEUED. The
 *                 client sends PUBREL 1, 2, 3 and 4.
 *
 *          As mosquitto does (db__message_write_queued_in), a queued
 *          message goes in flight when a slot frees, keeping ITS packet
 *          id, and its PUBREC is sent then:
 *
 *              PUBREL 1 -> PUBREC 3, PUBCOMP 1
 *              PUBREL 2 -> PUBREC 4, PUBCOMP 2
 *              PUBREL 3 -> PUBCOMP 3
 *              PUBREL 4 -> PUBCOMP 4
 *
 *          and no error is logged. Up to 7.25.4 the broker moved a queued
 *          message when the in-flight list was NOT empty (the test of the
 *          quota was inverted: mosquitto stops when the quota is spent,
 *          the broker stopped when nothing was in flight), gave it a NEW
 *          packet id, and sent the PUBREC with that id: the PUBREL of the
 *          client for its own id found nothing ("Message not found"), and
 *          the message was never released to its subscribers.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include <c_prot_mqtt2.h>
#include "c_queued_in.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BROKER_URL      "tcp://127.0.0.1:18112"
#define CLIENT_ID       "qin_client"
#define TOPIC           "t/q"

/*
 *  What session 2 must receive after its CONNACK
 */
PRIVATE const uint8_t expected_session2[] = {
    0x50, 0x02, 0x00, 0x03,     // PUBREC 3: in flight when 1 is released
    0x70, 0x02, 0x00, 0x01,     // PUBCOMP 1
    0x50, 0x02, 0x00, 0x04,     // PUBREC 4: in flight when 2 is released
    0x70, 0x02, 0x00, 0x02,     // PUBCOMP 2
    0x70, 0x02, 0x00, 0x03,     // PUBCOMP 3
    0x70, 0x02, 0x00, 0x04,     // PUBCOMP 4
};

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void send_bytes(hgobj gobj, const uint8_t *bf, size_t len);
PRIVATE void send_connect(hgobj gobj);
PRIVATE void set_broker_max_inflight(hgobj gobj, int max_inflight);

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
    hgobj gobj_tcp;
    int session;            // 1 or 2
    int phase;
    gbuffer_t *rx;          // what the broker sent in this session
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
    priv->gobj_tcp = gobj_create(
        "raw_client",
        C_TCP,
        json_pack("{s:s}", "url", BROKER_URL),
        gobj
    );
    priv->rx = gbuffer_create(1024, 1024);
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBUFFER_DECREF(priv->rx)
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
    if(gobj_is_running(priv->gobj_tcp)) {
        gobj_stop(priv->gobj_tcp);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  1. Session 1, once the broker listens
     */
    priv->session = 1;
    set_timeout(priv->timer, 300);

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
 *  Write raw bytes to the broker
 ***************************************************************************/
PRIVATE void send_bytes(hgobj gobj, const uint8_t *bf, size_t len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(len, len);
    gbuffer_append(gbuf, (void *)bf, len);
    gobj_send_event(
        priv->gobj_tcp,
        EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        gobj
    );
}

/***************************************************************************
 *  CONNECT, MQTT 3.1.1, clean session 0, no user: a persistent session
 ***************************************************************************/
PRIVATE void send_connect(hgobj gobj)
{
    static const uint8_t connect[] = {
        0x10, 22,                               // CONNECT, remaining length
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04,   // protocol name, level 4 (3.1.1)
        0x00,                                   // flags: clean session 0
        0x00, 0x3C,                             // keep alive 60 s
        0x00, 10, 'q', 'i', 'n', '_', 'c', 'l', 'i', 'e', 'n', 't'
    };
    send_bytes(gobj, connect, sizeof(connect));
}

/***************************************************************************
 *  The max_inflight_messages of the broker's side of every channel
 ***************************************************************************/
PRIVATE void set_broker_max_inflight(hgobj gobj, int max_inflight)
{
    hgobj input_side = gobj_find_service("__input_side__", TRUE);
    for(hgobj channel = gobj_first_child(input_side); channel; channel = gobj_next_child(channel)) {
        for(hgobj child = gobj_first_child(channel); child; child = gobj_next_child(child)) {
            if(gobj_gclass_name(child) == C_PROT_MQTT2) {
                gobj_write_integer_attr(child, "max_inflight_messages", max_inflight);
            }
        }
    }
}

/***************************************************************************
 *  A printable dump of what was received
 ***************************************************************************/
PRIVATE void print_bytes(char *bf, size_t bfsize, const uint8_t *p, size_t len)
{
    bf[0] = 0;
    size_t ln = 0;
    for(size_t i = 0; i < len && ln + 4 < bfsize; i++) {
        ln += (size_t)snprintf(bf + ln, bfsize - ln, "%02X ", p[i]);
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The phases
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase++;

    switch(priv->phase) {
        case 1:
            /*
             *  1. Session 1: connect (CONNECT goes at EV_CONNECTED)
             */
            gobj_start(priv->gobj_tcp);
            set_timeout(priv->timer, 500);
            break;

        case 2:
            {
                /*
                 *  Session 1 had its CONNACK and its four PUBREC: it goes away
                 *  without PUBREL. Then the broker takes 2 in flight, and
                 *  session 2 connects.
                 */
                static const uint8_t session1_expected[] = {
                    0x20, 0x02, 0x00, 0x00,     // CONNACK, no session present
                    0x50, 0x02, 0x00, 0x01,
                    0x50, 0x02, 0x00, 0x02,
                    0x50, 0x02, 0x00, 0x03,
                    0x50, 0x02, 0x00, 0x04,
                };
                size_t len = gbuffer_leftbytes(priv->rx);
                const uint8_t *p = gbuffer_cur_rd_pointer(priv->rx);
                if(len != sizeof(session1_expected) || memcmp(p, session1_expected, len) != 0) {
                    char dump[256];
                    print_bytes(dump, sizeof(dump), p, len);
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "TEST: session 1 did not get its CONNACK and four PUBREC",
                        "received",     "%s", dump,
                        NULL
                    );
                }
                gbuffer_clear(priv->rx);
                gobj_stop(priv->gobj_tcp);
                set_broker_max_inflight(gobj, 2);
                priv->session = 2;
                set_timeout(priv->timer, 300);
            }
            break;

        case 3:
            gobj_start(priv->gobj_tcp);
            set_timeout(priv->timer, 500);
            break;

        case 4:
            {
                /*
                 *  3. Session 2 had its CONNACK: release the four messages
                 */
                static const uint8_t connack[] = {0x20, 0x02, 0x01, 0x00};   // session present
                size_t len = gbuffer_leftbytes(priv->rx);
                const uint8_t *p = gbuffer_cur_rd_pointer(priv->rx);
                if(len != sizeof(connack) || memcmp(p, connack, len) != 0) {
                    char dump[256];
                    print_bytes(dump, sizeof(dump), p, len);
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "TEST: session 2 did not get its CONNACK with the session present",
                        "received",     "%s", dump,
                        NULL
                    );
                }
                gbuffer_clear(priv->rx);

                static const uint8_t pubrels[] = {
                    0x62, 0x02, 0x00, 0x01,
                    0x62, 0x02, 0x00, 0x02,
                    0x62, 0x02, 0x00, 0x03,
                    0x62, 0x02, 0x00, 0x04,
                };
                send_bytes(gobj, pubrels, sizeof(pubrels));
                set_timeout(priv->timer, 500);
            }
            break;

        default:
            {
                size_t len = gbuffer_leftbytes(priv->rx);
                const uint8_t *p = gbuffer_cur_rd_pointer(priv->rx);
                char dump[256];
                print_bytes(dump, sizeof(dump), p, len);
                if(len != sizeof(expected_session2) || memcmp(p, expected_session2, len) != 0) {
                    char expected[256];
                    print_bytes(expected, sizeof(expected), expected_session2, sizeof(expected_session2));
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "TEST: the queued messages did not go in flight with their own packet id",
                        "received",     "%s", dump,
                        "expected",     "%s", expected,
                        NULL
                    );
                } else {
                    gobj_log_info(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INFO,
                        "msg",          "%s", "TEST: the queued messages went in flight with their own packet id",
                        "received",     "%s", dump,
                        NULL
                    );
                }
                gobj_stop(priv->gobj_tcp);
                set_yuno_must_die();
            }
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Connected: CONNECT, and in session 1 the four QoS 2 PUBLISH
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    send_connect(gobj);

    if(priv->session == 1) {
        uint8_t publish[] = {
            0x34, 8,                        // PUBLISH, QoS 2, remaining length
            0x00, 3, 't', '/', 'q',         // topic
            0x00, 0x00,                     // packet id, set below
            'p'                             // payload
        };
        for(uint8_t mid = 1; mid <= 4; mid++) {
            publish[8] = mid;
            send_bytes(gobj, publish, sizeof(publish));
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the broker sends
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(gbuf) {
        gbuffer_append_gbuf(priv->rx, gbuf);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The transport's other events: nothing to do
 ***************************************************************************/
PRIVATE int ac_transport_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
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
GOBJ_DEFINE_GCLASS(C_QUEUED_IN);

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
        {EV_CONNECTED,          ac_connected,       0},
        {EV_RX_DATA,            ac_rx_data,         0},
        {EV_DISCONNECTED,       ac_transport_event, 0},
        {EV_TX_READY,           ac_transport_event, 0},
        {EV_STOPPED,            ac_transport_event, 0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
        {EV_CONNECTED,          0},
        {EV_RX_DATA,            0},
        {EV_DISCONNECTED,       0},
        {EV_TX_READY,           0},
        {EV_STOPPED,            0},
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
PUBLIC int register_c_queued_in(void)
{
    return create_gclass(C_QUEUED_IN);
}
