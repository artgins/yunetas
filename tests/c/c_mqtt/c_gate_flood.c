/****************************************************************************
 *          C_GATE_FLOOD.C
 *
 *          Driver of the test of a peer that asks a gate for more answers
 *          than it reads, over a real socket. This driver is the upper level
 *          of a server C_PROT_MQTT2 (no broker, no queues, allow_anonymous),
 *          as the decoder of a gate is, and its C_TCP_S accepts on that
 *          protocol, with the default max_unwritten_packets (10000). The
 *          client is RAW: a C_TCP of this driver that writes MQTT 3.1.1
 *          packets by hand.
 *
 *              1. A client that reads its answers: four bursts of 3000
 *                 PINGREQ, each one written after the 3000 PINGRESP of the
 *                 one before. 12000 answers in all, more than the bound:
 *                 it gets them all and is never dropped.
 *              2. A flood of 131072 PINGREQ (256 KB) in one write, the
 *                 client not reading (rx_paused): the gate drops it, with
 *                 ONE WARNING, and its memory does not grow past 8 MB.
 *              3. The same with 40000 QoS 1 PUBLISH (320 KB): dropped,
 *                 one WARNING, memory bounded.
 *              4. 20000 QoS 0 PUBLISH in one write, then a PINGREQ: QoS 0
 *                 asks for no answer, so nothing waits to be written, and
 *                 the client is not dropped: the gate takes the 20000
 *                 messages and answers the PINGREQ. The bound is on the
 *                 send queue, not on the packets read.
 *
 *          Each PINGREQ (2 bytes) queued a PINGRESP in the send queue of
 *          the C_TCP, which has no bound, at about 340 bytes of memory per
 *          answer not yet written, and a 4 KB read answers 2048 of them
 *          while the C_TCP writes one per turn of the loop. Up to 7.30.26 a
 *          gate grew by about 400 MB per 2 MB of PINGREQ, whether the peer
 *          read or not, until the process died. Red on 7.30.26: case 2 not
 *          dropped, the memory grown by 39 MB; case 3 (run alone) not
 *          dropped, 13.6 MB.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include <c_prot_mqtt2.h>
#include "c_gate_flood.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define GATE_URL            "tcp://127.0.0.1:18116"
#define CLIENT_ID           "flood_cl"

#define BURST_PINGREQ       3000
#define BURSTS              4
#define FLOOD_PINGREQ       131072      // 256 KB
#define FLOOD_PUBLISH       40000       // 320 KB
#define STREAM_QOS0         20000       // 120 KB
#define MAX_MEMORY_GROWTH   (8*1024*1024)   // 10000 answers waiting: about 3 MB

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
GOBJ_DECLARE_EVENT(EV_STOP_CLIENT);

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
    hgobj timer_mem;        // samples the memory while a flood is processed
    hgobj prot;             // the gate's side: C_PROT_MQTT2 server, its bottom a C_TCP
    hgobj tcp_s;
    hgobj client;           // the raw client
    int phase;
    gbuffer_t *rx;          // what the client got, not parsed yet
    BOOL connack;
    int bursts;
    int pingresp;
    int puback;
    int messages;           // QoS 1 messages the gate took
    BOOL gate_closed;       // the gate's EV_ON_CLOSE
    BOOL client_closed;
    size_t mem_base;
    size_t mem_growth;      // the most the memory grew during the flood
    size_t mem_growth_2;    // the one of the case 2, said at the end
    size_t mem_growth_3;    // the one of the case 3, said at the end
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
    priv->timer_mem = gobj_create_pure_child("memory", C_TIMER, 0, gobj);
    priv->rx = gbuffer_create(4*1024, 64*1024);

    priv->prot = gobj_create(
        "gate_input",
        C_PROT_MQTT2,
        json_pack("{s:b, s:b}",
            "iamServer", 1,
            "allow_anonymous", 1
        ),
        gobj
    );
    hgobj tcp = gobj_create("gate_input", C_TCP, 0, priv->prot);
    gobj_set_bottom_gobj(priv->prot, tcp);

    priv->tcp_s = gobj_create(
        "gate_server",
        C_TCP_S,
        json_pack("{s:s, s:{s:{s:s}}}",
            "url", GATE_URL,
            "child_tree_filter",
                "kw",
                    "__gclass_name__", C_PROT_MQTT2
        ),
        gobj
    );

    priv->client = gobj_create(
        "raw_client",
        C_TCP,
        json_pack("{s:s}", "url", GATE_URL),
        gobj
    );
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
    gobj_start(priv->timer_mem);
    gobj_start(priv->prot);     // a server waits for the C_TCP_S to give it its connection
    gobj_start(priv->tcp_s);

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
    clear_timeout(priv->timer_mem);
    gobj_stop(priv->timer_mem);
    if(gobj_is_running(priv->client)) {
        gobj_stop(priv->client);
    }
    if(gobj_is_running(priv->tcp_s)) {
        gobj_stop(priv->tcp_s);
    }
    if(gobj_is_running(priv->prot)) {
        gobj_stop(priv->prot);  // it stops its bottom
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 300);      // once the server listens

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    clear_timeout(priv->timer_mem);

    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Write the client's bytes to the gate (gbuf owned)
 ***************************************************************************/
PRIVATE void client_sends_gbuf(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_send_event(
        priv->client,
        EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        gobj
    );
}

/***************************************************************************
 *  CONNECT, MQTT 3.1.1, clean session, no user
 ***************************************************************************/
PRIVATE void client_connect(hgobj gobj)
{
    static const uint8_t head311[] = {
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04,   // protocol name, level 4 (3.1.1)
        0x02,                                   // flags: clean session
        0x00, 0x3C                              // keep alive 60 s
    };
    size_t len = 2 + sizeof(head311) + 2 + strlen(CLIENT_ID);
    gbuffer_t *gbuf = gbuffer_create(len, len);

    gbuffer_append_char(gbuf, 0x10);
    gbuffer_append_char(gbuf, (char)(sizeof(head311) + 2 + strlen(CLIENT_ID)));
    gbuffer_append(gbuf, (void *)head311, sizeof(head311));
    gbuffer_append_char(gbuf, 0x00);
    gbuffer_append_char(gbuf, (char)strlen(CLIENT_ID));
    gbuffer_append_string(gbuf, CLIENT_ID);
    client_sends_gbuf(gobj, gbuf);
}

/***************************************************************************
 *  `count` PINGREQ in one write
 ***************************************************************************/
PRIVATE void client_sends_pingreqs(hgobj gobj, int count)
{
    size_t len = 2 * (size_t)count;
    gbuffer_t *gbuf = gbuffer_create(len, len);
    for(int i=0; i<count; i++) {
        gbuffer_append_char(gbuf, (char)0xC0);  // PINGREQ
        gbuffer_append_char(gbuf, 0x00);
    }
    client_sends_gbuf(gobj, gbuf);
}

/***************************************************************************
 *  `count` QoS 1 PUBLISH of topic "t", payload "x", in one write
 ***************************************************************************/
PRIVATE void client_sends_publishes(hgobj gobj, int count)
{
    size_t len = 8 * (size_t)count;
    gbuffer_t *gbuf = gbuffer_create(len, len);
    for(int i=0; i<count; i++) {
        uint16_t mid = (uint16_t)((i % 65535) + 1);
        gbuffer_append_char(gbuf, 0x32);        // PUBLISH QoS 1
        gbuffer_append_char(gbuf, 6);
        gbuffer_append_char(gbuf, 0x00);
        gbuffer_append_char(gbuf, 0x01);
        gbuffer_append_char(gbuf, 't');
        gbuffer_append_char(gbuf, (char)(mid >> 8));
        gbuffer_append_char(gbuf, (char)(mid & 0xFF));
        gbuffer_append_char(gbuf, 'x');
    }
    client_sends_gbuf(gobj, gbuf);
}

/***************************************************************************
 *  `count` QoS 0 PUBLISH of topic "t", payload "x", and a PINGREQ, in one
 *  write
 ***************************************************************************/
PRIVATE void client_sends_qos0_stream(hgobj gobj, int count)
{
    size_t len = 6 * (size_t)count + 2;
    gbuffer_t *gbuf = gbuffer_create(len, len);
    for(int i=0; i<count; i++) {
        gbuffer_append_char(gbuf, 0x30);        // PUBLISH QoS 0
        gbuffer_append_char(gbuf, 4);
        gbuffer_append_char(gbuf, 0x00);
        gbuffer_append_char(gbuf, 0x01);
        gbuffer_append_char(gbuf, 't');
        gbuffer_append_char(gbuf, 'x');
    }
    gbuffer_append_char(gbuf, (char)0xC0);      // PINGREQ
    gbuffer_append_char(gbuf, 0x00);
    client_sends_gbuf(gobj, gbuf);
}

/***************************************************************************
 *  What the gate wrote to the client
 ***************************************************************************/
PRIVATE void parse_rx(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    while(gbuffer_leftbytes(priv->rx) >= 2) {
        const uint8_t *p = gbuffer_cur_rd_pointer(priv->rx);
        size_t avail = gbuffer_leftbytes(priv->rx);
        size_t remaining = p[1];    // < 128 here
        if(2 + remaining > avail) {
            return; // incomplete
        }
        uint8_t type = p[0] >> 4;
        gbuffer_get(priv->rx, 2 + remaining);

        switch(type) {
            case 2:     // CONNACK
                priv->connack = TRUE;
                if(priv->phase == 1) {
                    priv->bursts = 1;
                    client_sends_pingreqs(gobj, BURST_PINGREQ);
                } else if(priv->phase == 4) {
                    client_sends_qos0_stream(gobj, STREAM_QOS0);
                } else {
                    /*
                     *  The client stops reading, and floods
                     */
                    gobj_write_bool_attr(priv->client, "rx_paused", TRUE);
                    priv->mem_base = get_cur_system_memory();
                    set_timeout_periodic(priv->timer_mem, 20);
                    if(priv->phase == 2) {
                        client_sends_pingreqs(gobj, FLOOD_PINGREQ);
                    } else {
                        client_sends_publishes(gobj, FLOOD_PUBLISH);
                    }
                }
                break;

            case 4:     // PUBACK
                priv->puback++;
                break;

            case 13:    // PINGRESP
                priv->pingresp++;
                if(priv->phase == 4) {
                    gobj_post_event(gobj, EV_STOP_CLIENT, 0, gobj);
                }
                if(priv->phase == 1 && priv->pingresp == priv->bursts * BURST_PINGREQ) {
                    if(priv->bursts < BURSTS) {
                        priv->bursts++;
                        client_sends_pingreqs(gobj, BURST_PINGREQ);
                    } else {
                        gobj_post_event(gobj, EV_STOP_CLIENT, 0, gobj);
                    }
                }
                break;

            default:
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "TEST: a packet not expected",
                    "phase",        "%d", priv->phase,
                    "type",         "%d", (int)type,
                    NULL
                );
                break;
        }
    }
}

/***************************************************************************
 *  The most the memory grew since the flood started
 ***************************************************************************/
PRIVATE void measure_memory(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t cur = get_cur_system_memory();
    if(cur > priv->mem_base && cur - priv->mem_base > priv->mem_growth) {
        priv->mem_growth = cur - priv->mem_base;
    }
}

/***************************************************************************
 *  Start a case: a new connection of the client
 ***************************************************************************/
PRIVATE void start_case(hgobj gobj, int phase)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = phase;
    gbuffer_clear(priv->rx);
    priv->connack = FALSE;
    priv->bursts = 0;
    priv->pingresp = 0;
    priv->puback = 0;
    priv->messages = 0;
    priv->gate_closed = FALSE;
    priv->client_closed = FALSE;
    priv->mem_base = 0;
    priv->mem_growth = 0;
    gobj_write_bool_attr(priv->client, "rx_paused", FALSE);
    gobj_start(priv->client);       // CONNECT at its EV_CONNECTED
    set_timeout(priv->timer, 5000);
}

/***************************************************************************
 *  A case went wrong: say it, and end
 ***************************************************************************/
PRIVATE void case_failed(hgobj gobj, const char *what)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", what,
        "phase",        "%d", priv->phase,
        "connack",      "%d", priv->connack,
        "pingresp",     "%d", priv->pingresp,
        "puback",       "%d", priv->puback,
        "messages",     "%d", priv->messages,
        "gate_closed",  "%d", priv->gate_closed,
        "client_closed","%d", priv->client_closed,
        "mem_growth",   "%lu", (unsigned long)priv->mem_growth,
        "gate_rx_bytes","%ld", (long)gobj_read_integer_attr(gobj_bottom_gobj(priv->prot), "rxBytes"),
        "gate_tx_msgs", "%ld", (long)gobj_read_integer_attr(gobj_bottom_gobj(priv->prot), "txMsgs"),
        "gate_tx_queue","%ld", (long)gobj_read_integer_attr(gobj_bottom_gobj(priv->prot), "cur_tx_queue"),
        "client_tx_bytes","%ld", (long)gobj_read_integer_attr(priv->client, "txBytes"),
        NULL
    );
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The phases. Each case waits for the close of its connection; the
 *  timer is the guard that names the case that did not end.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->phase) {
        case 0:
            start_case(gobj, 1);
            break;

        case 1:
            if(!priv->gate_closed || priv->pingresp != BURSTS * BURST_PINGREQ) {
                case_failed(gobj, "TEST: 1. a client that reads its answers did not get them all");
                set_yuno_must_die();
                break;
            }
            start_case(gobj, 2);
            break;

        case 2:
        case 3:
            clear_timeout(priv->timer_mem);
            if(!priv->gate_closed) {
                case_failed(gobj, (priv->phase == 2)?
                    "TEST: 2. a flood of PINGREQ was not dropped" :
                    "TEST: 3. a flood of QoS 1 PUBLISH was not dropped"
                );
                set_yuno_must_die();
                break;
            }
            if(priv->mem_growth > MAX_MEMORY_GROWTH) {
                case_failed(gobj, "TEST: the memory of the flood grew past its bound");
            }
            if(priv->phase == 2) {
                priv->mem_growth_2 = priv->mem_growth;
                start_case(gobj, 3);
                break;
            }
            priv->mem_growth_3 = priv->mem_growth;
            start_case(gobj, 4);
            break;

        case 4:
            if(!priv->gate_closed || priv->pingresp != 1 || priv->messages != STREAM_QOS0) {
                case_failed(gobj, "TEST: 4. a client that streams QoS 0 was dropped, or lost messages");
                set_yuno_must_die();
                break;
            }
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "TEST: gate flood done",
                "mem_growth_pingreq",   "%lu", (unsigned long)priv->mem_growth_2,
                "mem_growth_publish",   "%lu", (unsigned long)priv->mem_growth_3,
                NULL
            );
            set_yuno_must_die();
            break;

        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST: phase unknown",
                "phase",        "%d", priv->phase,
                NULL
            );
            set_yuno_must_die();
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A sample of the memory during a flood
 ***************************************************************************/
PRIVATE int ac_timeout_periodic(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    measure_memory(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The client is connected: CONNECT
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->client) {
        client_connect(gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the gate wrote to the client
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(gbuf) {
        gbuffer_append_gbuf(priv->rx, gbuf);
    }
    parse_rx(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The client's connection closed. It is stopped from an event posted to
 *  this driver, not inside its own publication.
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->client) {
        gobj_post_event(gobj, EV_STOP_CLIENT, 0, gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Stop the client before it reconnects, and check the case once both
 *  ends are closed
 ***************************************************************************/
PRIVATE int ac_stop_client(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_is_running(priv->client)) {
        gobj_stop(priv->client);
    }
    priv->client_closed = TRUE;
    if(priv->gate_closed) {
        set_timeout(priv->timer, 100);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The transports' other events: nothing to do
 ***************************************************************************/
PRIVATE int ac_transport_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A CONNECT: no session kept
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The gate's connection closed. A flooding client, which reads nothing,
 *  reads again now, to see its close.
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gate_closed = TRUE;
    if(priv->phase >= 2) {
        measure_memory(gobj);
        clear_timeout(priv->timer_mem);
        gobj_write_bool_attr(priv->client, "rx_paused", FALSE);
    }
    if(priv->client_closed) {
        set_timeout(priv->timer, 100);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the protocol asks the upper level: every message is taken
 ***************************************************************************/
PRIVATE int ac_on_iev_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_event_t iev_event = (gobj_event_t)(uintptr_t)kw_get_int(
        gobj, kw, "__iev_event__", 0, KW_REQUIRED
    );
    int ret = 0;

    if(iev_event == EV_MQTT_MESSAGE) {
        priv->messages++;
        ret = 1;
    }

    KW_DECREF(kw)
    return ret;
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
GOBJ_DEFINE_GCLASS(C_GATE_FLOOD);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_STOP_CLIENT);

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
        {EV_TIMEOUT_PERIODIC,   ac_timeout_periodic,0},
        {EV_CONNECTED,          ac_connected,       0},
        {EV_RX_DATA,            ac_rx_data,         0},
        {EV_DISCONNECTED,       ac_disconnected,    0},
        {EV_STOP_CLIENT,        ac_stop_client,     0},
        {EV_TX_READY,           ac_transport_event, 0},
        {EV_STOPPED,            ac_transport_event, 0},
        {EV_ON_OPEN,            ac_on_open,         0},
        {EV_ON_CLOSE,           ac_on_close,        0},
        {EV_ON_IEV_MESSAGE,     ac_on_iev_message,  0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
        {EV_TIMEOUT_PERIODIC,   0},
        {EV_CONNECTED,          0},
        {EV_RX_DATA,            0},
        {EV_DISCONNECTED,       0},
        {EV_STOP_CLIENT,        0},
        {EV_TX_READY,           0},
        {EV_STOPPED,            0},
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
PUBLIC int register_c_gate_flood(void)
{
    return create_gclass(C_GATE_FLOOD);
}
