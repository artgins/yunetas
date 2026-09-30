/****************************************************************************
 *          C_WRONG_ACK.C
 *
 *          Driver of the test of the acks of the wrong QoS that a client
 *          sends to the broker, of the CONNECT decode trace, and of the
 *          auth_data attr of C_PROT_MQTT2 as it is SHOWN.
 *
 *          The client is RAW: a C_TCP of this gclass that writes the MQTT 5
 *          packets by hand.
 *
 *              1. auth_data (MQTT 5 AUTH data) is a secret: written on the
 *                 broker's C_PROT_MQTT2, `view-attrs attribute=auth_data`
 *                 and `view-gobj` must show it as "********". Up to 7.25.20
 *                 it lacked SDF_SECRET and showed in clear.
 *              2. CONNECT with a username and a 300-byte password, with the
 *                 "show-decode" trace of C_PROT_MQTT2 armed (main checks
 *                 the log: the password must never appear, and the
 *                 username must be printed as it is, without the bytes
 *                 that follow it in the packet). Then SUBSCRIBE t/w QoS 1
 *                 and PUBLISH t/w QoS 1 (packet id 20): the broker acks it
 *                 and delivers it back with a packet id of its own.
 *              3. PUBREL 20, the release of a QoS 2 message, for the QoS 1
 *                 message: the broker holds no QoS 2 message 20. It must
 *                 answer PUBCOMP 20 (as mosquitto: it may be a PUBREL
 *                 repeated after a reconnection) and say it as a WARNING of
 *                 the peer. Up to 7.25.20 it was an ERROR with a stack.
 *              4. PUBREC, the ack of QoS 2, for the QoS 1 message of the
 *                 broker: a protocol error of the peer. The broker must
 *                 answer DISCONNECT 0x82 and say "QoS mismatch" as a
 *                 WARNING. Up to 7.25.20 it was an ERROR, with no client_id
 *                 nor peername.
 *              5. A second raw client sends the same CONNECT with one byte
 *                 too many: the broker refuses it ("Mqtt: too much data")
 *                 and dumps the malformed frame, at every trace level. The
 *                 dump must not carry the password (main checks the log).
 *                 Up to 7.25.20 the whole CONNECT was dumped, password and
 *                 all.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include <c_prot_mqtt2.h>
#include "c_wrong_ack.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BROKER_URL          "tcp://127.0.0.1:18117"
#define INPUT_SIDE          "__input_side__"
#define BROKER_PROT_NAME    "18117-1"
#define CLIENT_MID          20
#define AUTH_DATA           "QVVUSC1EQVRBLVNIT1VMRC1OT1QtQkUtU0hPV04="
#define MASKED              "********"

/*
 *  Shared with main_wrong_ack.c, which scans the log for them
 */
const char *wrong_ack_username = "wack_user";
const char *wrong_ack_password_needle = "PASSWORD-SHOULD-NOT-BE-LOGGED";
const char wrong_ack_password_fill = 'p';     // the rest of the password
#define PASSWORD_LEN        300     // > 255: its length has a non-zero high byte

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void send_bytes(hgobj gobj, hgobj tcp, const uint8_t *bf, size_t len);
PRIVATE void send_ack(hgobj gobj, uint8_t type, uint16_t mid);

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
    hgobj gobj_tcp2;        // the client of the malformed CONNECT
    int phase;
    gbuffer_t *rx;          // what the broker sent, not parsed yet

    int connack_reason;     // -1 none
    int n_puback;           // PUBACKs of CLIENT_MID
    int broker_mid;         // packet id of the PUBLISH of the broker, -1 none
    int broker_qos;
    int n_pubcomp;          // PUBCOMPs of CLIENT_MID
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
    priv->gobj_tcp = gobj_create(
        "raw_client",
        C_TCP,
        json_pack("{s:s}", "url", BROKER_URL),
        gobj
    );
    priv->gobj_tcp2 = gobj_create(
        "raw_client2",
        C_TCP,
        json_pack("{s:s}", "url", BROKER_URL),
        gobj
    );
    priv->rx = gbuffer_create(4*1024, 4*1024);
    priv->connack_reason = -1;
    priv->broker_mid = -1;
    priv->disconnect_reason = -1;
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
    if(gobj_is_running(priv->gobj_tcp2)) {
        gobj_stop(priv->gobj_tcp2);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 300);  // once the broker listens

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
PRIVATE void send_bytes(hgobj gobj, hgobj tcp, const uint8_t *bf, size_t len)
{
    gbuffer_t *gbuf = gbuffer_create(len, len);
    gbuffer_append(gbuf, (void *)bf, len);
    gobj_send_event(
        tcp,
        EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        gobj
    );
}

/***************************************************************************
 *  A two-byte ack of a packet id, MQTT 5 short form
 ***************************************************************************/
PRIVATE void send_ack(hgobj gobj, uint8_t type, uint16_t mid)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint8_t ack[] = {type, 0x02, (uint8_t)(mid >> 8), (uint8_t)(mid & 0xFF)};
    send_bytes(gobj, priv->gobj_tcp, ack, sizeof(ack));
}

/***************************************************************************
 *  CONNECT with username and a PASSWORD_LEN password, and `extra` bytes
 *  too many after it
 ***************************************************************************/
PRIVATE size_t build_connect(uint8_t *bf, size_t extra)
{
    static const uint8_t connect_head[] = {
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x05,   // protocol name, level 5
        0xC2,                                   // flags: username, password, clean start
        0x00, 0x3C,                             // keep alive 60 s
        0x00,                                   // properties length
        0x00, 7, 'w', 'a', 'c', 'k', '_', 'c', 'l'
    };
    size_t ulen = strlen(wrong_ack_username);
    size_t nlen = strlen(wrong_ack_password_needle);
    size_t remaining = sizeof(connect_head) + 2 + ulen + 2 + PASSWORD_LEN + extra;
    size_t n = 0;

    bf[n++] = 0x10;                                     // CONNECT
    bf[n++] = (uint8_t)((remaining & 0x7F) | 0x80);     // remaining length, 2 bytes
    bf[n++] = (uint8_t)(remaining >> 7);
    memcpy(bf + n, connect_head, sizeof(connect_head));
    n += sizeof(connect_head);
    bf[n++] = 0x00;
    bf[n++] = (uint8_t)ulen;
    memcpy(bf + n, wrong_ack_username, ulen);
    n += ulen;
    bf[n++] = (uint8_t)(PASSWORD_LEN >> 8);
    bf[n++] = (uint8_t)(PASSWORD_LEN & 0xFF);
    memcpy(bf + n, wrong_ack_password_needle, nlen);
    memset(bf + n + nlen, wrong_ack_password_fill, PASSWORD_LEN - nlen);
    n += PASSWORD_LEN;
    memset(bf + n, 0, extra);
    n += extra;
    return n;
}

/***************************************************************************
 *  Parse what the broker sent
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
            case 2:     // CONNACK
                if(remaining >= 2) {
                    priv->connack_reason = v[1];
                }
                break;
            case 3:     // PUBLISH
                {
                    int qos = (p[0] >> 1) & 0x03;
                    uint16_t topic_len = (uint16_t)((v[0] << 8) | v[1]);
                    if(qos > 0) {
                        const uint8_t *m = v + 2 + topic_len;
                        priv->broker_mid = (uint16_t)((m[0] << 8) | m[1]);
                        priv->broker_qos = qos;
                    }
                }
                break;
            case 4:     // PUBACK
                if(remaining >= 2 && ((v[0] << 8) | v[1]) == CLIENT_MID) {
                    priv->n_puback++;
                }
                break;
            case 7:     // PUBCOMP
                if(remaining >= 2 && ((v[0] << 8) | v[1]) == CLIENT_MID) {
                    priv->n_pubcomp++;
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
 *  The yuno's `command` must show the value at `path` as MASKED
 ***************************************************************************/
PRIVATE void check_shown_masked(
    hgobj gobj,
    const char *command,
    json_t *kw_command, // owned
    const char *path
) {
    json_t *response = gobj_command(gobj_yuno(), command, kw_command, gobj);
    if(kw_get_int(gobj, response, "result", -1, 0) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: yuno command FAILED",
            "command",      "%s", command,
            "comment",      "%s", kw_get_str(gobj, response, "comment", "", 0),
            NULL
        );
        JSON_DECREF(response)
        return;
    }
    const char *shown = kw_get_str(gobj, response, path, "", 0);
    if(strcmp(shown, MASKED) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: auth_data is not shown masked",
            "command",      "%s", command,
            "shown",        "%s", shown,
            NULL
        );
    }
    JSON_DECREF(response)
}

/***************************************************************************
 *  1. auth_data of the broker's C_PROT_MQTT2, as view-attrs and view-gobj
 *  show it
 ***************************************************************************/
PRIVATE void check_auth_data_masked(hgobj gobj)
{
    hgobj input_side = gobj_find_service(INPUT_SIDE, TRUE);
    hgobj channel = input_side? gobj_child_by_name(input_side, BROKER_PROT_NAME) : NULL;
    hgobj prot = channel? gobj_child_by_name(channel, BROKER_PROT_NAME) : NULL;
    if(!prot) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: C_PROT_MQTT2 of the broker not found",
            NULL
        );
        return;
    }

    gobj_write_str_attr(prot, "auth_data", AUTH_DATA);

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "data`%s", gobj_short_name(prot));
    check_shown_masked(
        gobj,
        "view-attrs",
        json_pack("{s:s, s:s}",
            "gobj", gobj_full_name(prot),
            "attribute", "auth_data"
        ),
        path
    );
    check_shown_masked(
        gobj,
        "view-gobj",
        json_pack("{s:s}",
            "gobj", gobj_full_name(prot)
        ),
        "data`attrs`auth_data"
    );

    gobj_write_str_attr(prot, "auth_data", "");
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
            check_auth_data_masked(gobj);

            gobj_set_gclass_trace(gclass_find_by_name(C_PROT_MQTT2), "show-decode", TRUE);
            gobj_start(priv->gobj_tcp);     // CONNECT goes at EV_CONNECTED
            set_timeout(priv->timer, 500);
            break;

        case 2:
            /*
             *  2. Connected, the QoS 1 message acked and delivered back
             */
            parse_rx(gobj);
            if(priv->connack_reason != 0) {
                test_error(gobj, "TEST: CONNECT not accepted", priv->connack_reason, 0);
            }
            if(priv->n_puback != 1) {
                test_error(gobj, "TEST: the QoS 1 PUBLISH was not acked", priv->n_puback, 1);
            }
            if(priv->broker_mid < 0 || priv->broker_qos != 1) {
                test_error(gobj,
                    "TEST: the broker did not deliver the message with QoS 1",
                    priv->broker_qos, 1
                );
            }

            /*
             *  3. PUBREL of the QoS 1 message
             */
            send_ack(gobj, 0x62, CLIENT_MID);
            set_timeout(priv->timer, 500);
            break;

        case 3:
            parse_rx(gobj);
            if(priv->n_pubcomp != 1) {
                test_error(gobj,
                    "TEST: a PUBREL of an unknown QoS 2 message was not answered with PUBCOMP",
                    priv->n_pubcomp, 1
                );
            }
            if(priv->disconnect_reason != -1) {
                test_error(gobj,
                    "TEST: a PUBREL of an unknown QoS 2 message disconnected",
                    priv->disconnect_reason, -1
                );
            }

            /*
             *  4. PUBREC of the QoS 1 message of the broker
             */
            if(priv->broker_mid >= 0) {
                send_ack(gobj, 0x50, (uint16_t)priv->broker_mid);
            }
            set_timeout(priv->timer, 500);
            break;

        case 4:
            parse_rx(gobj);
            if(priv->disconnect_reason != 0x82) {
                test_error(gobj,
                    "TEST: a PUBREC of a QoS 1 message was not answered with DISCONNECT 0x82",
                    priv->disconnect_reason, 0x82
                );
            }
            gobj_set_gclass_trace(gclass_find_by_name(C_PROT_MQTT2), "show-decode", FALSE);

            /*
             *  5. The malformed CONNECT, from a second client
             */
            gobj_start(priv->gobj_tcp2);    // CONNECT goes at EV_CONNECTED
            set_timeout(priv->timer, 500);
            break;

        default:
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "TEST: wrong ack done",
                "pubcomp",      "%d", priv->n_pubcomp,
                "disconnect",   "%d", priv->disconnect_reason,
                NULL
            );
            if(gobj_is_running(priv->gobj_tcp)) {
                gobj_stop(priv->gobj_tcp);
            }
            if(gobj_is_running(priv->gobj_tcp2)) {
                gobj_stop(priv->gobj_tcp2);
            }
            set_yuno_must_die();
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Connected: CONNECT with username and password, SUBSCRIBE, and a QoS 1
 *  PUBLISH. The second client: the CONNECT with one byte too many.
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint8_t bf[512];

    if(src == priv->gobj_tcp2) {
        send_bytes(gobj, src, bf, build_connect(bf, 1));
        KW_DECREF(kw)
        return 0;
    }

    send_bytes(gobj, src, bf, build_connect(bf, 0));

    static const uint8_t subscribe[] = {
        0x82, 9,                                // SUBSCRIBE, remaining length
        0x00, 0x01,                             // packet id
        0x00,                                   // properties length
        0x00, 3, 't', '/', 'w',                 // topic filter
        0x01                                    // QoS 1
    };
    send_bytes(gobj, src, subscribe, sizeof(subscribe));

    static const uint8_t publish[] = {
        0x32, 9,                                // PUBLISH, QoS 1, remaining length
        0x00, 3, 't', '/', 'w',                 // topic
        0x00, CLIENT_MID,                       // packet id
        0x00,                                   // properties length
        'p'                                     // payload
    };
    send_bytes(gobj, src, publish, sizeof(publish));

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
    if(gbuf && src == priv->gobj_tcp) {
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
GOBJ_DEFINE_GCLASS(C_WRONG_ACK);

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
PUBLIC int register_c_wrong_ack(void)
{
    return create_gclass(C_WRONG_ACK);
}
