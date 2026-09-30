/****************************************************************************
 *          C_CLIENT_PUBREC.C
 *
 *          Driver of the test of the PUBRECs that C_PROT_MQTT2, as a CLIENT,
 *          receives from its broker. No broker and no socket: the protocol
 *          gobj is a child of this driver, its bottom is a C_FAKE_TRANSPORT,
 *          and this driver plays the broker, writing its MQTT 5 packets by
 *          hand. The client keeps its queues in a C_TRANGER of the driver.
 *
 *              1. The client CONNECTs, and the "broker" accepts (CONNACK).
 *              2. PUBREC of a packet id the client never used: the peer's.
 *                 The client must say it once, as a WARNING ("Mqtt: Received
 *                 PUBREC for an unknown packet ID", with client_id, mid and
 *                 peername), and answer PUBREL. Up to 7.25.20 the client
 *                 also logged an ERROR "Message not found", with a stack.
 *              3. The client publishes a QoS 1 message, and the "broker"
 *                 answers PUBREC, the ack of QoS 2: a protocol error of the
 *                 peer. The client must say "QoS mismatch" as a WARNING
 *                 that names the client (main checks client_id in the log;
 *                 up to 7.25.20 it named only the peer) and answer
 *                 DISCONNECT 0x82.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include <c_prot_mqtt2.h>
#include "c_fake_transport.h"
#include "c_client_pubrec.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define UNKNOWN_MID         77
#define QUEUES_DIR          "/tmp/test_mqtt_client_pubrec/qmsgs"

/*
 *  Shared with main_client_pubrec.c, which scans the log for it
 */
const char *client_pubrec_client_id = "pubrec_cl";

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
    hgobj gobj_tranger;
    hgobj prot;
    hgobj fake;
    int phase;
    gbuffer_t *rx;          // what the client wrote, not parsed yet

    int n_connect;
    int n_open;
    int pubrel_mid;         // -1 none
    int publish_mid;        // -1 none
    int publish_qos;
    int disconnect_reason;  // -1 none
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
    priv->rx = gbuffer_create(4*1024, 4*1024);
    priv->pubrel_mid = -1;
    priv->publish_mid = -1;
    priv->disconnect_reason = -1;

    priv->gobj_tranger = gobj_create_service(
        "tranger_queues",
        C_TRANGER,
        json_pack("{s:s, s:b}",
            "path", QUEUES_DIR,
            "master", 1
        ),
        gobj
    );

    priv->prot = gobj_create(
        "mqtt_client",
        C_PROT_MQTT2,
        json_pack("{s:b, s:s}",
            "iamServer", 0,
            "mqtt_client_id", client_pubrec_client_id
        ),
        gobj
    );

    priv->fake = gobj_create(
        "fake_transport",
        C_FAKE_TRANSPORT,
        json_pack("{s:I}", "sink", (json_int_t)(uintptr_t)gobj),
        priv->prot
    );
    gobj_set_bottom_gobj(priv->prot, priv->fake);
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
    gobj_start(priv->gobj_tranger);
    gobj_write_pointer_attr(
        priv->prot,
        "tranger_queues",
        gobj_read_pointer_attr(priv->gobj_tranger, "tranger")
    );
    gobj_start(priv->prot);     // it starts its bottom

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
    if(gobj_is_running(priv->prot)) {
        gobj_stop(priv->prot);
    }
    if(gobj_is_running(priv->gobj_tranger)) {
        gobj_stop(priv->gobj_tranger);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 100);

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
 *  Write the broker's bytes into the client
 ***************************************************************************/
PRIVATE void broker_sends(hgobj gobj, const uint8_t *bf, size_t len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(len, len);
    gbuffer_append(gbuf, (void *)bf, len);
    gobj_send_event(
        priv->prot,
        EV_RX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        priv->fake
    );
}

/***************************************************************************
 *  Parse what the client wrote
 ***************************************************************************/
PRIVATE void parse_rx(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    while(gbuffer_leftbytes(priv->rx) >= 2) {
        const uint8_t *p = gbuffer_cur_rd_pointer(priv->rx);
        size_t avail = gbuffer_leftbytes(priv->rx);
        uint32_t remaining = 0;
        uint32_t multiplier = 1;
        size_t hdr = 1;
        while(hdr < avail) {
            uint8_t b = p[hdr++];
            remaining += (b & 0x7F) * multiplier;
            multiplier *= 128;
            if(!(b & 0x80)) {
                break;
            }
        }
        if(hdr + remaining > avail) {
            return; // incomplete
        }
        uint8_t type = p[0] >> 4;
        const uint8_t *v = p + hdr;
        switch(type) {
            case 1:     // CONNECT
                priv->n_connect++;
                break;
            case 3:     // PUBLISH
                {
                    int qos = (p[0] >> 1) & 0x03;
                    uint16_t topic_len = (uint16_t)((v[0] << 8) | v[1]);
                    if(qos > 0) {
                        const uint8_t *m = v + 2 + topic_len;
                        priv->publish_mid = (uint16_t)((m[0] << 8) | m[1]);
                        priv->publish_qos = qos;
                    }
                }
                break;
            case 6:     // PUBREL
                if(remaining >= 2) {
                    priv->pubrel_mid = (v[0] << 8) | v[1];
                }
                break;
            case 14:    // DISCONNECT
                priv->disconnect_reason = remaining > 0? v[0] : 0;
                break;
            default:
                break;
        }
        gbuffer_get(priv->rx, hdr + remaining);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_error(hgobj gobj, const char *msg, int got, int expected)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", msg,
        "got",          "%d", got,
        "expected",     "%d", expected,
        NULL
    );
}

/***************************************************************************
 *  PUBREC of `mid`, MQTT 5 short form
 ***************************************************************************/
PRIVATE void broker_sends_pubrec(hgobj gobj, uint16_t mid)
{
    uint8_t pubrec[] = {0x50, 0x02, (uint8_t)(mid >> 8), (uint8_t)(mid & 0xFF)};
    broker_sends(gobj, pubrec, sizeof(pubrec));
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
            {
                /*
                 *  1. CONNECT, CONNACK
                 */
                gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);
                parse_rx(gobj);
                if(priv->n_connect != 1) {
                    test_error(gobj, "TEST: the client did not CONNECT", priv->n_connect, 1);
                }
                static const uint8_t connack[] = {
                    0x20, 0x03,     // CONNACK
                    0x00,           // no session present
                    0x00,           // Success
                    0x00            // properties length
                };
                broker_sends(gobj, connack, sizeof(connack));
                if(priv->n_open != 1) {
                    test_error(gobj, "TEST: the client did not open", priv->n_open, 1);
                }

                /*
                 *  2. PUBREC of a packet id never used
                 */
                broker_sends_pubrec(gobj, UNKNOWN_MID);
                parse_rx(gobj);
                if(priv->pubrel_mid != UNKNOWN_MID) {
                    test_error(gobj,
                        "TEST: a PUBREC of an unknown packet id was not answered with PUBREL",
                        priv->pubrel_mid, UNKNOWN_MID
                    );
                }

                /*
                 *  3. A QoS 1 PUBLISH of the client
                 */
                gbuffer_t *gbuf = gbuffer_create(1, 1);
                gbuffer_append_string(gbuf, "p");
                gobj_send_event(
                    priv->prot,
                    EV_MQTT_PUBLISH,
                    json_pack("{s:s, s:i, s:I}",
                        "topic", "t/p",
                        "qos", 1,
                        "gbuffer", (json_int_t)(uintptr_t)gbuf  // the kw owns it
                    ),
                    gobj
                );
                set_timeout(priv->timer, 100);
            }
            break;

        case 2:
            /*
             *  3. ... and PUBREC of it, the ack of QoS 2
             */
            parse_rx(gobj);
            if(priv->publish_mid < 0 || priv->publish_qos != 1) {
                test_error(gobj,
                    "TEST: the client did not send the QoS 1 PUBLISH",
                    priv->publish_qos, 1
                );
            } else {
                broker_sends_pubrec(gobj, (uint16_t)priv->publish_mid);
                parse_rx(gobj);
                if(priv->disconnect_reason != 0x82) {
                    test_error(gobj,
                        "TEST: a PUBREC of a QoS 1 message was not answered with DISCONNECT 0x82",
                        priv->disconnect_reason, 0x82
                    );
                }
            }
            set_timeout(priv->timer, 100);
            break;

        default:
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "TEST: client pubrec done",
                "pubrel",       "%d", priv->pubrel_mid,
                "disconnect",   "%d", priv->disconnect_reason,
                NULL
            );
            set_yuno_must_die();
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the client writes, through the fake transport
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
 *  The client is in session
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->n_open++;

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The client's other output events: nothing to do
 ***************************************************************************/
PRIVATE int ac_prot_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
GOBJ_DEFINE_GCLASS(C_CLIENT_PUBREC);

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
        {EV_RX_DATA,            ac_rx_data,         0},
        {EV_ON_OPEN,            ac_on_open,         0},
        {EV_ON_CLOSE,           ac_prot_event,      0},
        {EV_ON_IEV_MESSAGE,     ac_prot_event,      0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
        {EV_RX_DATA,            0},
        {EV_ON_OPEN,            0},
        {EV_ON_CLOSE,           0},
        {EV_ON_IEV_MESSAGE,     0},
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
PUBLIC int register_c_client_pubrec(void)
{
    return create_gclass(C_CLIENT_PUBREC);
}
