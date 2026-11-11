/****************************************************************************
 *          C_GATE_ANSWERS.C
 *
 *          Driver of the test of the bound on the answers a gate holds for
 *          peers that do not read them, over real sockets. This driver is
 *          the upper level of the server C_PROT_MQTT2s (no broker, no
 *          queues, allow_anonymous), as the decoder of a gate is, and its
 *          C_TCP_S accepts on them, with the default max_unwritten_bytes.
 *          The clients are RAW: C_TCPs of this driver that write MQTT 3.1.1
 *          packets by hand. The send buffer of each gate socket is made
 *          small (SO_SNDBUF 4096), so the kernel takes little of what the
 *          gate writes to a peer that does not read.
 *
 *              1. 50 connections, each sending 10000 PINGREQ in one write
 *                 and never reading: every one is dropped, with ONE
 *                 WARNING each, and the memory of all of them grows less
 *                 than 8 MB.
 *              2. One connection subscribes 100 filters (granted), stops
 *                 reading, and sends SUBSCRIBEs of 16000 filters, refused
 *                 by the protocol past max_subscriptions_per_client: each
 *                 is answered by a SUBACK of 16 KB. It is dropped, and the
 *                 memory grows less than 1 MB. After the drop the gate says
 *                 nothing more of the MQTT connection: the transport, whose
 *                 write in flight the peer never takes, says its own abort
 *                 at timeout_stop_tx (1 s here).
 *
 *          Up to 7.30.26 nothing bounded the answers. The bound that came
 *          after it counted the PACKETS read since the send queue was
 *          last empty: a peer that sent exactly 10000 PINGREQ was never
 *          checked (about 3.7 MB held per connection, and a pool of
 *          anonymous connections ended the process), and a SUBACK of 16 KB
 *          counted as one packet (156 MB per connection). Red there: case
 *          1 not dropped; case 2 not dropped. And the drop was said three
 *          times: its WARNING, "Timeout waiting mqtt disconnected" at
 *          timeout_close, and the transport's abort.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <sys/socket.h>
#include <yunetas.h>
#include <c_prot_mqtt2.h>
#include "c_gate_answers.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define GATE_URL            "tcp://127.0.0.1:18122"

#define N_CONN              50
#define FLOOD_PINGREQ       10000       // the bound of packets before: never checked
#define MAX_GROWTH_1        (8*1024*1024)

#define GRANTED_FILTERS     100         // max_subscriptions_per_client
#define BIG_FILTERS         16000       // a SUBACK of 16 KB
#define BIG_SUBSCRIBES      150
#define MAX_GROWTH_2        (1024*1024)

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
GOBJ_DECLARE_EVENT(EV_STOP_CLIENTS);

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
typedef struct {
    hgobj client;           // the raw client
    gbuffer_t *rx;          // what it got, not parsed yet
    BOOL connack;
    BOOL closed;            // its EV_DISCONNECTED
} raw_client_t;

typedef struct _PRIVATE_DATA {
    hgobj timer;
    hgobj timer_mem;        // samples the memory while a flood is processed
    hgobj tcp_s;
    raw_client_t clients[N_CONN];
    int phase;
    int gate_closed;        // the gates' EV_ON_CLOSE
    int clients_closed;
    int big_sent;           // case 2: SUBSCRIBEs of 16000 filters sent
    BOOL flooding;          // case 2: a SUBSCRIBE is sent at each EV_TX_READY of the client
    size_t mem_base;
    size_t mem_growth;      // the most the memory grew during the flood
    size_t mem_growth_1;    // the one of the case 1, said at the end
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

    for(int i=0; i<N_CONN; i++) {
        char name[NAME_MAX];
        snprintf(name, sizeof(name), "gate_input-%d", i);
        hgobj prot = gobj_create(
            name,
            C_PROT_MQTT2,
            json_pack("{s:b, s:b, s:i}",
                "iamServer", 1,
                "allow_anonymous", 1,
                "timeout_close", 500
            ),
            gobj
        );
        hgobj tcp = gobj_create(name, C_TCP,
            json_pack("{s:i}", "timeout_stop_tx", 1000),
            prot
        );
        gobj_set_bottom_gobj(prot, tcp);

        snprintf(name, sizeof(name), "raw_client-%d", i);
        priv->clients[i].client = gobj_create(
            name,
            C_TCP,
            json_pack("{s:s}", "url", GATE_URL),
            gobj
        );
        priv->clients[i].rx = gbuffer_create(4*1024, 16*1024*1024);
    }

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
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int i=0; i<N_CONN; i++) {
        GBUFFER_DECREF(priv->clients[i].rx)
    }
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    gobj_start(priv->timer_mem);
    hgobj child = gobj_first_child(gobj);
    while(child) {
        if(gobj_gclass_name(child) == C_PROT_MQTT2) {
            gobj_start(child);  // a server waits for the C_TCP_S to give it its connection
        }
        child = gobj_next_child(child);
    }
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
    for(int i=0; i<N_CONN; i++) {
        if(gobj_is_running(priv->clients[i].client)) {
            gobj_stop(priv->clients[i].client);
        }
    }
    if(gobj_is_running(priv->tcp_s)) {
        gobj_stop(priv->tcp_s);
    }
    hgobj child = gobj_first_child(gobj);
    while(child) {
        if(gobj_gclass_name(child) == C_PROT_MQTT2 && gobj_is_running(child)) {
            gobj_stop(child);   // it stops its bottom
        }
        child = gobj_next_child(child);
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
 *  The index of a raw client, -1 if src is not one
 ***************************************************************************/
PRIVATE int client_index(hgobj gobj, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int i=0; i<N_CONN; i++) {
        if(priv->clients[i].client == src) {
            return i;
        }
    }
    return -1;
}

/***************************************************************************
 *  Write a client's bytes to the gate (gbuf owned)
 ***************************************************************************/
PRIVATE void client_sends_gbuf(hgobj client, gbuffer_t *gbuf)
{
    gobj_send_event(
        client,
        EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        client
    );
}

/***************************************************************************
 *  The remaining length of a packet, its bytes put at gbuf
 ***************************************************************************/
PRIVATE void append_remaining_length(gbuffer_t *gbuf, size_t remaining)
{
    do {
        uint8_t b = (uint8_t)(remaining % 128);
        remaining /= 128;
        if(remaining > 0) {
            b |= 0x80;
        }
        gbuffer_append_char(gbuf, (char)b);
    } while(remaining > 0);
}

/***************************************************************************
 *  CONNECT, MQTT 3.1.1, clean session, no user
 ***************************************************************************/
PRIVATE void client_connect(hgobj client, int idx)
{
    static const uint8_t head311[] = {
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04,   // protocol name, level 4 (3.1.1)
        0x02,                                   // flags: clean session
        0x00, 0x00                              // keep alive 0: no timer
    };
    char client_id[32];
    snprintf(client_id, sizeof(client_id), "answers_cl%d", idx);
    size_t remaining = sizeof(head311) + 2 + strlen(client_id);
    gbuffer_t *gbuf = gbuffer_create(remaining + 2, remaining + 2);

    gbuffer_append_char(gbuf, 0x10);
    append_remaining_length(gbuf, remaining);
    gbuffer_append(gbuf, (void *)head311, sizeof(head311));
    gbuffer_append_char(gbuf, 0x00);
    gbuffer_append_char(gbuf, (char)strlen(client_id));
    gbuffer_append_string(gbuf, client_id);
    client_sends_gbuf(client, gbuf);
}

/***************************************************************************
 *  `count` PINGREQ in one write
 ***************************************************************************/
PRIVATE void client_sends_pingreqs(hgobj client, int count)
{
    size_t len = 2 * (size_t)count;
    gbuffer_t *gbuf = gbuffer_create(len, len);
    for(int i=0; i<count; i++) {
        gbuffer_append_char(gbuf, (char)0xC0);  // PINGREQ
        gbuffer_append_char(gbuf, 0x00);
    }
    client_sends_gbuf(client, gbuf);
}

/***************************************************************************
 *  SUBSCRIBE of the filters cmnd/x<i>/#, i from 0 to GRANTED_FILTERS-1
 ***************************************************************************/
PRIVATE void client_subscribes_granted(hgobj client)
{
    size_t remaining = 2;
    for(int i=0; i<GRANTED_FILTERS; i++) {
        char filter[32];
        remaining += 2 + (size_t)snprintf(filter, sizeof(filter), "cmnd/x%d/#", i) + 1;
    }
    gbuffer_t *gbuf = gbuffer_create(remaining + 5, remaining + 5);
    gbuffer_append_char(gbuf, (char)0x82);      // SUBSCRIBE
    append_remaining_length(gbuf, remaining);
    gbuffer_append_char(gbuf, 0x00);
    gbuffer_append_char(gbuf, 0x01);            // packet id 1
    for(int i=0; i<GRANTED_FILTERS; i++) {
        char filter[32];
        int flen = snprintf(filter, sizeof(filter), "cmnd/x%d/#", i);
        gbuffer_append_char(gbuf, 0x00);
        gbuffer_append_char(gbuf, (char)flen);
        gbuffer_append(gbuf, filter, (size_t)flen);
        gbuffer_append_char(gbuf, 0x00);        // QoS 0
    }
    client_sends_gbuf(client, gbuf);
}

/***************************************************************************
 *  SUBSCRIBE of BIG_FILTERS times the filter "a": past
 *  max_subscriptions_per_client, every one is refused by the protocol
 ***************************************************************************/
PRIVATE void client_subscribes_big(hgobj client, uint16_t mid)
{
    size_t remaining = 2 + (size_t)BIG_FILTERS * 4;
    gbuffer_t *gbuf = gbuffer_create(remaining + 5, remaining + 5);
    gbuffer_append_char(gbuf, (char)0x82);      // SUBSCRIBE
    append_remaining_length(gbuf, remaining);
    gbuffer_append_char(gbuf, (char)(mid >> 8));
    gbuffer_append_char(gbuf, (char)(mid & 0xFF));
    for(int i=0; i<BIG_FILTERS; i++) {
        gbuffer_append_char(gbuf, 0x00);
        gbuffer_append_char(gbuf, 0x01);
        gbuffer_append_char(gbuf, 'a');
        gbuffer_append_char(gbuf, 0x00);        // QoS 0
    }
    client_sends_gbuf(client, gbuf);
}

/***************************************************************************
 *  What the gate wrote to a client
 ***************************************************************************/
PRIVATE void parse_rx(hgobj gobj, int idx)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    raw_client_t *cl = &priv->clients[idx];

    while(gbuffer_leftbytes(cl->rx) >= 2) {
        const uint8_t *p = gbuffer_cur_rd_pointer(cl->rx);
        size_t avail = gbuffer_leftbytes(cl->rx);
        size_t remaining = 0;
        size_t multiplier = 1;
        size_t hdr = 1;
        BOOL complete = FALSE;
        while(hdr < avail && hdr < 5) {
            uint8_t b = p[hdr++];
            remaining += (b & 0x7F) * multiplier;
            multiplier *= 128;
            if(!(b & 0x80)) {
                complete = TRUE;
                break;
            }
        }
        if(!complete || hdr + remaining > avail) {
            return; // incomplete
        }
        uint8_t type = p[0] >> 4;
        gbuffer_get(cl->rx, hdr + remaining);

        if(type == 2) {         // CONNACK
            cl->connack = TRUE;
            if(priv->phase == 1) {
                /*
                 *  The client stops reading, and floods
                 */
                gobj_write_bool_attr(cl->client, "rx_paused", TRUE);
                client_sends_pingreqs(cl->client, FLOOD_PINGREQ);
            } else {
                client_subscribes_granted(cl->client);
            }
        } else if(type == 9 && priv->phase == 2 && !priv->flooding) {    // SUBACK of the granted
            gobj_write_bool_attr(cl->client, "rx_paused", TRUE);
            priv->mem_base = get_cur_system_memory();
            priv->mem_growth = 0;
            set_timeout_periodic(priv->timer_mem, 20);
            priv->flooding = TRUE;
            priv->big_sent = 1;
            client_subscribes_big(cl->client, 2);
        }
    }
    if(gbuffer_leftbytes(cl->rx) == 0) {
        gbuffer_clear(cl->rx);  // what was parsed is not kept
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
 *  A case went wrong: say it
 ***************************************************************************/
PRIVATE void case_failed(hgobj gobj, const char *what)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", what,
        "phase",        "%d", priv->phase,
        "gate_closed",  "%d", priv->gate_closed,
        "clients_closed","%d", priv->clients_closed,
        "big_sent",     "%d", priv->big_sent,
        "mem_growth",   "%lu", (unsigned long)priv->mem_growth,
        NULL
    );
}

/***************************************************************************
 *  End of a case's flood: the clients read again, to see their close
 ***************************************************************************/
PRIVATE void unpause_clients(hgobj gobj, int n)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int i=0; i<n; i++) {
        gobj_write_bool_attr(priv->clients[i].client, "rx_paused", FALSE);
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The phases. Each case waits for the closes of its connections; the
 *  timer is the guard that names the case that did not end.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->phase) {
        case 0:
            priv->phase = 1;
            priv->mem_base = get_cur_system_memory();
            priv->mem_growth = 0;
            set_timeout_periodic(priv->timer_mem, 20);
            for(int i=0; i<N_CONN; i++) {
                gobj_start(priv->clients[i].client);   // CONNECT at its EV_CONNECTED
            }
            set_timeout(priv->timer, 10000);
            break;

        case 1:
            clear_timeout(priv->timer_mem);
            if(priv->gate_closed != N_CONN || priv->clients_closed != N_CONN) {
                case_failed(gobj, "TEST: 1. connections at the old bound, not reading, were not all dropped");
                unpause_clients(gobj, N_CONN);
                set_yuno_must_die();
                break;
            }
            if(priv->mem_growth > MAX_GROWTH_1) {
                case_failed(gobj, "TEST: 1. the memory of the connections grew past its bound");
            }
            priv->mem_growth_1 = priv->mem_growth;

            priv->phase = 2;
            priv->gate_closed = 0;
            priv->clients_closed = 0;
            gbuffer_clear(priv->clients[0].rx);
            priv->clients[0].connack = FALSE;
            priv->clients[0].closed = FALSE;
            gobj_write_bool_attr(priv->clients[0].client, "rx_paused", FALSE);
            gobj_start(priv->clients[0].client);
            set_timeout(priv->timer, 15000);
            break;

        case 2:
            clear_timeout(priv->timer_mem);
            priv->flooding = FALSE;
            if(priv->gate_closed != 1 || priv->clients_closed != 1) {
                case_failed(gobj, "TEST: 2. a peer that does not read its big SUBACKs was not dropped");
                unpause_clients(gobj, 1);
                set_yuno_must_die();
                break;
            }
            if(priv->mem_growth > MAX_GROWTH_2) {
                case_failed(gobj, "TEST: 2. the memory of the big SUBACKs grew past its bound");
            }
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "TEST: gate answers done",
                "mem_growth_pingreq",   "%lu", (unsigned long)priv->mem_growth_1,
                "mem_growth_suback",    "%lu", (unsigned long)priv->mem_growth,
                "big_subscribes_sent",  "%d", priv->big_sent,
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
 *  A client is connected: CONNECT
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    int idx = client_index(gobj, src);
    if(idx >= 0) {
        client_connect(src, idx);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the gate wrote to a client
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int idx = client_index(gobj, src);
    if(idx >= 0) {
        gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
        if(gbuf) {
            gbuffer_append_gbuf(priv->clients[idx].rx, gbuf);
        }
        parse_rx(gobj, idx);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A client's write is done: case 2 sends its next big SUBSCRIBE
 ***************************************************************************/
PRIVATE int ac_tx_ready(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase == 2 && priv->flooding && src == priv->clients[0].client &&
            priv->big_sent < BIG_SUBSCRIBES) {
        priv->big_sent++;
        client_subscribes_big(src, (uint16_t)(priv->big_sent + 1));
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A client's connection closed. The clients are stopped from an event
 *  posted to this driver, not inside their own publication.
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int idx = client_index(gobj, src);
    if(idx >= 0 && !priv->clients[idx].closed) {
        priv->clients[idx].closed = TRUE;
        priv->clients_closed++;
        gobj_post_event(gobj, EV_STOP_CLIENTS, 0, gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Stop the clients that closed, before they reconnect, and check the case
 *  once all ends are closed
 ***************************************************************************/
PRIVATE int ac_stop_clients(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int i=0; i<N_CONN; i++) {
        if(priv->clients[i].closed && gobj_is_running(priv->clients[i].client)) {
            gobj_stop(priv->clients[i].client);
        }
    }
    int n = (priv->phase == 1)? N_CONN : 1;
    if(priv->gate_closed == n && priv->clients_closed == n) {
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
 *  A CONNECT: no session kept. The gate's socket gets a small send buffer
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    int fd = (int)gobj_read_integer_attr(gobj_bottom_gobj(src), "fd_clisrv");
    int sndbuf = 4096;
    if(fd <= 0 || setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: cannot make the send buffer of the gate small",
            "fd",           "%d", fd,
            NULL
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A gate's connection closed. Its client, which reads nothing, reads
 *  again now, to see its close.
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gate_closed++;
    measure_memory(gobj);
    int n = (priv->phase == 1)? N_CONN : 1;
    if(priv->gate_closed == n) {
        clear_timeout(priv->timer_mem);
        unpause_clients(gobj, n);
    }
    if(priv->gate_closed == n && priv->clients_closed == n) {
        set_timeout(priv->timer, 100);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the protocol asks the upper level: every message is taken, every
 *  filter that goes up is granted
 ***************************************************************************/
PRIVATE int ac_on_iev_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_event_t iev_event = (gobj_event_t)(uintptr_t)kw_get_int(
        gobj, kw, "__iev_event__", 0, KW_REQUIRED
    );
    int ret = 0;

    if(iev_event == EV_MQTT_MESSAGE) {
        ret = 1;

    } else if(iev_event == EV_MQTT_SUBSCRIBE) {
        gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED);
        json_t *subs = kw_get_list(gobj, kw, "subs", 0, KW_REQUIRED);
        size_t n = json_array_size(subs);
        for(size_t i=0; i<n; i++) {
            gbuffer_append_char(gbuf, 0x00);
        }
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
GOBJ_DEFINE_GCLASS(C_GATE_ANSWERS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_STOP_CLIENTS);

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
        {EV_STOP_CLIENTS,       ac_stop_clients,    0},
        {EV_TX_READY,           ac_tx_ready,        0},
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
        {EV_STOP_CLIENTS,       0},
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
PUBLIC int register_c_gate_answers(void)
{
    return create_gclass(C_GATE_ANSWERS);
}
