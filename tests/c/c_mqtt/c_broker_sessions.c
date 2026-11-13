/****************************************************************************
 *          C_BROKER_SESSIONS.C
 *
 *          Driver of the test of what a session of the broker keeps across a
 *          take-over, and of the in-flight limit of a session with queues.
 *          The clients are RAW plain sockets of this driver that write the
 *          MQTT packets by hand, read every 20 ms, and never PUBACK what the
 *          broker sends them. One client id per scenario:
 *
 *              1. MQTT 3.1.1, then 2. MQTT 5: A connects with a session that
 *                 is kept (clean session 0; MQTT 5: Session Expiry 3600),
 *                 subscribes to q<n>/# QoS 1 and publishes 5 QoS 1 messages
 *                 to it: it gets them back, and does not acknowledge them.
 *                 B connects with the same client id and a CLEAN session:
 *                 CONNACK with session present 0, and NOTHING more: a clean
 *                 session starts empty [MQTT-3.1.2-6] (3.1.1),
 *                 [MQTT-3.1.2-4] (5.0).
 *              3. MQTT 3.1.1, then 4. MQTT 5: the same, but B keeps the
 *                 session too: CONNACK with session present 1, and the 5
 *                 messages again (as before the fix).
 *              5. MQTT 5: A as in 2, then it sends a packet the broker does
 *                 not take (0xF0): the broker answers DISCONNECT and drops
 *                 it, its session kept with the 5 messages. B comes with a
 *                 clean session when A is gone: NOTHING (the broker deletes
 *                 the queues of a session not kept with no connection).
 *              6. MQTT 3.1.1: C connects (clean session) and sends 21 QoS 2
 *                 PUBLISH without PUBREL, one more than the broker's
 *                 max_inflight_messages (20): at most 20 PUBREC, then it is
 *                 dropped with ONE WARNING "Mqtt: QoS 2 PUBLISH past
 *                 max_inflight_messages (Receive Maximum), disconnecting".
 *              7. MQTT 3.1.1, then 8: A as in 1, then STALLED: a receive
 *                 buffer of 4 KB (and the broker's side a send buffer of
 *                 4 KB), it reads nothing more and sends 5 SUBSCRIBEs of 8000
 *                 times "a" and 0xF0: the broker's close waits for 40 KB of
 *                 SUBACKs it cannot write. B takes the session over while A
 *                 is still closing: with a clean session (7) NOTHING, keeping
 *                 it (8) the 5 messages.
 *
 *          9 to 18 (takeover_cases[]): what the CLOSE of a connection does
 *          to the queues of its session. Here B acknowledges what it gets,
 *          and stays 2.5 s, so the queue backup (every second) and the acks
 *          use the queues B opened. A comes with MQTT 5, Clean Start 1 and
 *          Session Expiry 300 unless said otherwise: a session that
 *          outlives its connection, Clean Start cleaning at the CONNECT only.
 *              9, 10. A is taken over, still connected, by B keeping the
 *                 session (MQTT 5 expiry 300, then MQTT 3.1.1): session
 *                 present 1, the 5 messages, and no ERROR.
 *              11. A has 25 messages (20 in flight, 5 queued), sends
 *                 DISCONNECT and closes; C publishes 5 more; B comes back
 *                 keeping the session: session present 1 and the 30
 *                 [MQTT-4.4.0-1].
 *              12. A has 30 messages and STALLS as in 7 (MQTT 5 SUBSCRIBEs
 *                 and 0xF0): B keeps the session while A is closing: the 30.
 *              13. A sends DISCONNECT with Session Expiry 0 and stays:
 *                 the server closes it after timeout_close, with ONE
 *                 WARNING "Client did not close after its DISCONNECT,
 *                 closed by the server", and the session ends with that
 *                 close [MQTT-3.14.2.2.2]; B, keeping a session, gets
 *                 session present 0 and nothing, as in 17.
 *              14. The same with a keepalive of 1 s: closed at
 *                 timeout_close too, not at the keepalive (no keepalive
 *                 WARNING, no DISCONNECT of the server); B: session present
 *                 0, nothing. Up to 7.31.1 the server waited for the client
 *                 to close: B got the session A had expired (13), and A was
 *                 closed by its keepalive with a WARNING and a DISCONNECT
 *                 0x8D of the server (14).
 *              15 to 18. What is not kept is still deleted at the close: A
 *                 with MQTT 3.1.1 and a clean session (15), MQTT 5 with no
 *                 Session Expiry (16), Session Expiry 300 and a DISCONNECT
 *                 with 0 (17), Session Expiry 1 and B 3 s later (18: the
 *                 broker's expiry deletes it); B keeping: session present 0
 *                 and nothing.
 *
 *          Up to 7.30.26 B of 1, 2 and 7 got the 5 messages of A's session:
 *          the broker deleted the queues of a session not kept only when it
 *          had no connection, and a connection told to go by a take-over
 *          deleted them only when IT had a clean session (and one still
 *          closing left them to the end of its close). And C of 6 was
 *          dropped with no log (the path without queues said its WARNING).
 *          And the close of A deleted the queue topics whenever A had come
 *          with Clean Start 1, its session kept or not: in 9, 10, 12, 13
 *          and 14 under the queues B had open (their backup read the freed
 *          topic every second: ERRORs, and in 12 a SIGSEGV), and in 11 B got
 *          5 of 30.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <yunetas.h>
#include "c_broker_sessions.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BROKER_PORT         18126

#define N_MESSAGES          5
#define QOS2_PAST_FLIGHT    21          // max_inflight_messages of the broker (20) + 1
#define STEP_MSEC           5000        // a step that takes longer failed
#define SETTLE_MSEC         1000        // what B gets after its CONNACK
#define RX_MAX              (64*1024)
#define BIG_SUBSCRIBES      5
#define BIG_FILTERS         8000        // a SUBACK of 8 KB: 40 KB that a stalled A never takes
#define BIG_REMAINING       (2 + BIG_FILTERS * 4)
#define BIG_REMAINING5      (BIG_REMAINING + 1)     // MQTT 5: the properties of the SUBSCRIBE
#define MAX_INFLIGHT        20          // max_inflight_messages of the broker
#define BACKUP_MSEC         2500        // B stays: the queue backup looks every second

enum {
    SC_CLEAN_311 = 0,
    SC_CLEAN_5,
    SC_KEEP_311,
    SC_KEEP_5,
    SC_CLOSING_CLEAN_5,
    SC_QOS2_FLIGHT_311,
    SC_STALLED_CLEAN_311,
    SC_STALLED_KEEP_311,
    SC_TAKEOVERS            // the first of takeover_cases[]
};

/*
 *  How A ends, in takeover_cases[]
 */
typedef enum {
    A_LIVE = 0,             // stays connected until B's CONNACK
    A_LIVE_DISC0,           // DISCONNECT with Session Expiry 0, and stays: the server closes it at timeout_close
    A_KEEPALIVE_DISC0,      // the same with a keepalive of 1 s: closed at timeout_close, not at the keepalive
    A_DISC,                 // DISCONNECT, and closes
    A_DISC0,                // DISCONNECT with Session Expiry 0, and closes
    A_STALLED_F0,           // stalls (40 KB of SUBACKs never read) and 0xF0
} a_end_t;

typedef struct {
    int a_version;
    BOOL a_clean;
    int a_expiry;           // -1 no Session Expiry property
    int a_keepalive;
    int n_msgs;             // A publishes them to itself and never acks
    a_end_t a_end;
    int wait_msec;          // from A's end to B
    int n_extra;            // C publishes them after A's end
    int b_version;
    BOOL b_clean;
    int b_expiry;
    int want_sp;
    int want_msgs;
} takeover_case_t;

PRIVATE const takeover_case_t takeover_cases[] = {
/*   A: ver clean expiry ka  msgs  end              wait  extra B: ver clean expiry  sp  msgs */
    {    5, 1,    300,   0,  5,    A_LIVE,          0,    0,       5,  0,    300,    1,  5},     // 9
    {    5, 1,    300,   0,  5,    A_LIVE,          0,    0,       4,  0,    -1,     1,  5},     // 10
    {    5, 1,    300,   0,  25,   A_DISC,          300,  5,       5,  0,    300,    1,  30},    // 11
    {    5, 1,    300,   0,  30,   A_STALLED_F0,    500,  0,       5,  0,    300,    1,  30},    // 12
    {    5, 1,    300,   0,  5,    A_LIVE_DISC0,    300,  0,       5,  0,    300,    0,  0},     // 13
    {    5, 1,    300,   1,  5,    A_KEEPALIVE_DISC0, 0,  0,       5,  0,    300,    0,  0},     // 14
    {    4, 1,    -1,    0,  5,    A_DISC,          300,  0,       4,  0,    -1,     0,  0},     // 15
    {    5, 1,    -1,    0,  5,    A_DISC,          300,  0,       5,  0,    300,    0,  0},     // 16
    {    5, 1,    300,   0,  5,    A_DISC0,         300,  0,       5,  0,    300,    0,  0},     // 17
    {    5, 1,    1,     0,  5,    A_DISC,          3000, 0,       5,  0,    300,    0,  0},     // 18
};
#define SC_END  (SC_TAKEOVERS + (int)(sizeof(takeover_cases)/sizeof(takeover_cases[0])))

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    int fd;
    uint8_t rx[RX_MAX];
    size_t rx_len;
    BOOL closed;            // the broker closed it
    int connack;
    int connack_sp;         // session present of the last CONNACK
    int suback;
    int puback;
    int pubrec;
    int publish;
    int disconnect;
    BOOL no_read;           // stalled: reads nothing more
    BOOL ack;               // acknowledges (PUBACK) every QoS 1 PUBLISH it gets
    const uint8_t *tx;      // what is still to be written, as the kernel takes it
    size_t tx_len;
    size_t tx_off;
} client_t;

/*
 *  A's burst when it stalls: 5 SUBSCRIBEs of 8000 times "a" (MQTT 3.1.1) and
 *  0xF0 (a type the server does not take: the close, which waits for the
 *  SUBACKs to be written). Not of gbmem: built once.
 */
PRIVATE uint8_t burst[BIG_SUBSCRIBES * (4 + BIG_REMAINING) + 2];
PRIVATE uint8_t burst5[BIG_SUBSCRIBES * (4 + BIG_REMAINING5) + 2];     // the same in MQTT 5

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void close_clients(hgobj gobj);

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
    int scenario;
    int step;
    uint64_t t_step;        // the deadline of the step
    uint64_t t_settle;
    BOOL failed;
    client_t a;
    client_t b;
    client_t c;             // the publisher of n_extra
    BOOL extra_sent;
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
    priv->a.fd = -1;
    priv->b.fd = -1;
    priv->c.fd = -1;

    for(int v5=0; v5<2; v5++) {
        uint8_t *bf = v5? burst5 : burst;
        size_t remaining = v5? BIG_REMAINING5 : BIG_REMAINING;
        size_t n = 0;
        for(int k=0; k<BIG_SUBSCRIBES; k++) {
            bf[n++] = 0x82;                         // SUBSCRIBE, remaining length in 3 bytes
            bf[n++] = (uint8_t)((remaining & 0x7F) | 0x80);
            bf[n++] = (uint8_t)(((remaining >> 7) & 0x7F) | 0x80);
            bf[n++] = (uint8_t)(remaining >> 14);
            bf[n++] = 0x00;
            bf[n++] = (uint8_t)(k + 10);            // packet id
            if(v5) {
                bf[n++] = 0x00;                     // properties
            }
            for(int i=0; i<BIG_FILTERS; i++) {
                bf[n++] = 0x00;
                bf[n++] = 0x01;
                bf[n++] = 'a';
                bf[n++] = 0x00;                     // QoS 0
            }
        }
        bf[n++] = 0xF0;                             // a type the server does not take
        bf[n++] = 0x00;
    }
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    close_clients(gobj);
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
    close_clients(gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->t_step = start_msectimer(STEP_MSEC);
    priv->t_settle = start_msectimer(300);     // the broker listening
    set_timeout_periodic(priv->timer, 20);

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
 *  A scenario went wrong: say it, once, and end
 ***************************************************************************/
PRIVATE void case_failed(hgobj gobj, const char *what)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->failed) {
        return;
    }
    priv->failed = TRUE;
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", what,
        "scenario",     "%d", priv->scenario + 1,
        "step",         "%d", priv->step,
        "a_connack",    "%d", priv->a.connack,
        "a_suback",     "%d", priv->a.suback,
        "a_puback",     "%d", priv->a.puback,
        "a_pubrec",     "%d", priv->a.pubrec,
        "a_publish",    "%d", priv->a.publish,
        "a_disconnect", "%d", priv->a.disconnect,
        "a_closed",     "%d", priv->a.closed,
        "b_connack",    "%d", priv->b.connack,
        "b_session_present", "%d", priv->b.connack_sp,
        "b_publish",    "%d", priv->b.publish,
        "c_puback",     "%d", priv->c.puback,
        NULL
    );
    clear_timeout(priv->timer);
    set_yuno_must_die();
}

/***************************************************************************
 *  The remaining length of a packet
 ***************************************************************************/
PRIVATE size_t put_remaining_length(uint8_t *bf, size_t remaining)
{
    size_t n = 0;
    do {
        uint8_t b = (uint8_t)(remaining % 128);
        remaining /= 128;
        if(remaining > 0) {
            b |= 0x80;
        }
        bf[n++] = b;
    } while(remaining > 0);
    return n;
}

/***************************************************************************
 *  Send a packet: its fixed header and its body
 ***************************************************************************/
PRIVATE void send_packet(hgobj gobj, client_t *cl, uint8_t type, const uint8_t *body, size_t len)
{
    uint8_t bf[512];
    size_t n = 0;
    bf[n++] = type;
    n += put_remaining_length(bf + n, len);
    if(n + len > sizeof(bf)) {
        case_failed(gobj, "TEST: packet too big");
        return;
    }
    memcpy(bf + n, body, len);
    n += len;
    if(send(cl->fd, bf, n, MSG_NOSIGNAL) != (ssize_t)n) {
        case_failed(gobj, "TEST: cannot send a packet");
    }
}

/***************************************************************************
 *  Connect a raw client and send its CONNECT: clean session / Clean Start,
 *  MQTT 5 Session Expiry Interval (-1 none), keep alive
 ***************************************************************************/
PRIVATE void client_connect(
    hgobj gobj,
    client_t *cl,
    int version,
    const char *client_id,
    BOOL clean,
    int expiry,
    int keepalive,
    int rcvbuf
) {
    memset(cl, 0, sizeof(*cl));
    cl->fd = socket(AF_INET, SOCK_STREAM, 0);
    if(cl->fd >= 0 && rcvbuf > 0 &&
            setsockopt(cl->fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) < 0) {
        case_failed(gobj, "TEST: cannot make the receive buffer small");
        return;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BROKER_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if(cl->fd < 0 || connect(cl->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot connect the raw client",
            "errno",        "%s", strerror(errno),
            NULL
        );
        case_failed(gobj, "TEST: cannot connect");
        return;
    }

    uint8_t body[128];
    size_t n = 0;
    body[n++] = 0x00; body[n++] = 0x04;
    memcpy(body + n, "MQTT", 4); n += 4;
    body[n++] = (uint8_t)version;
    body[n++] = clean? 0x02 : 0x00;                 // clean session / clean start
    body[n++] = (uint8_t)(keepalive >> 8); body[n++] = (uint8_t)(keepalive & 0xFF);
    if(version == 5) {
        if(expiry >= 0) {
            body[n++] = 5;                          // properties
            body[n++] = 0x11;                       // Session Expiry Interval
            body[n++] = (uint8_t)(expiry >> 24); body[n++] = (uint8_t)(expiry >> 16);
            body[n++] = (uint8_t)(expiry >> 8);  body[n++] = (uint8_t)(expiry & 0xFF);
        } else {
            body[n++] = 0;
        }
    }
    size_t idlen = strlen(client_id);
    body[n++] = 0x00; body[n++] = (uint8_t)idlen;
    memcpy(body + n, client_id, idlen); n += idlen;
    send_packet(gobj, cl, 0x10, body, n);
}

/***************************************************************************
 *  SUBSCRIBE of a filter, QoS 1
 ***************************************************************************/
PRIVATE void client_subscribe(hgobj gobj, client_t *cl, int version, const char *filter)
{
    uint8_t body[128];
    size_t n = 0;
    body[n++] = 0x00; body[n++] = 0x01;             // packet id 1
    if(version == 5) {
        body[n++] = 0;                              // properties
    }
    size_t flen = strlen(filter);
    body[n++] = 0x00; body[n++] = (uint8_t)flen;
    memcpy(body + n, filter, flen); n += flen;
    body[n++] = 0x01;                               // QoS 1
    send_packet(gobj, cl, 0x82, body, n);
}

/***************************************************************************
 *  PUBLISH of QoS 1 (0x32) or 2 (0x34)
 ***************************************************************************/
PRIVATE void client_publish(hgobj gobj, client_t *cl, int version, uint8_t type, const char *topic, uint16_t mid)
{
    uint8_t body[128];
    size_t n = 0;
    size_t tlen = strlen(topic);
    body[n++] = 0x00; body[n++] = (uint8_t)tlen;
    memcpy(body + n, topic, tlen); n += tlen;
    body[n++] = (uint8_t)(mid >> 8); body[n++] = (uint8_t)(mid & 0xFF);
    if(version == 5) {
        body[n++] = 0;                              // properties
    }
    memcpy(body + n, "msg", 3); n += 3;
    send_packet(gobj, cl, type, body, n);
}

/***************************************************************************
 *  Read what a client has, and count its packets
 ***************************************************************************/
PRIVATE void client_read(hgobj gobj, client_t *cl)
{
    if(cl->fd < 0 || cl->closed || cl->no_read) {
        return;
    }
    while(cl->rx_len < sizeof(cl->rx)) {
        ssize_t got = recv(cl->fd, cl->rx + cl->rx_len, sizeof(cl->rx) - cl->rx_len, MSG_DONTWAIT);
        if(got < 0) {
            if(errno != EAGAIN && errno != EWOULDBLOCK) {
                cl->closed = TRUE;
            }
            break;
        }
        if(got == 0) {
            cl->closed = TRUE;
            break;
        }
        cl->rx_len += (size_t)got;
    }

    size_t p = 0;
    while(cl->rx_len - p >= 2) {
        size_t remaining = 0, mult = 1, i = 1;
        BOOL complete = FALSE;
        while(p + i < cl->rx_len && i < 5) {
            uint8_t b = cl->rx[p + i];
            remaining += (b & 0x7F) * mult;
            mult *= 128;
            i++;
            if(!(b & 0x80)) {
                complete = TRUE;
                break;
            }
        }
        if(!complete || p + i + remaining > cl->rx_len) {
            break;
        }
        uint8_t *body = cl->rx + p + i;
        switch(cl->rx[p] >> 4) {
            case 2:
                cl->connack++;
                cl->connack_sp = (remaining >= 1)? (body[0] & 0x01) : -1;
                break;
            case 3:
                cl->publish++;
                if(cl->ack && ((cl->rx[p] >> 1) & 0x03) == 1 && remaining >= 4) {
                    size_t tlen = ((size_t)body[0] << 8) | body[1];
                    if(2 + tlen + 2 > remaining) {
                        case_failed(gobj, "TEST: a PUBLISH too short for its packet id");
                        return;
                    }
                    uint8_t puback[4] = {0x40, 0x02, body[2 + tlen], body[2 + tlen + 1]};
                    if(send(cl->fd, puback, sizeof(puback), MSG_NOSIGNAL) != (ssize_t)sizeof(puback)) {
                        case_failed(gobj, "TEST: cannot send PUBACK");
                        return;
                    }
                }
                break;
            case 4:
                cl->puback++;
                break;
            case 5:
                cl->pubrec++;
                break;
            case 9:
                cl->suback++;
                break;
            case 14:
                cl->disconnect++;
                break;
            default:
                break;
        }
        p += i + remaining;
    }
    if(p > 0) {
        memmove(cl->rx, cl->rx + p, cl->rx_len - p);
        cl->rx_len -= p;
    }
}

/***************************************************************************
 *  Write what the kernel takes of what a client still has to write
 ***************************************************************************/
PRIVATE void client_flush(hgobj gobj, client_t *cl)
{
    if(cl->fd < 0 || !cl->tx || cl->tx_off >= cl->tx_len) {
        return;
    }
    ssize_t sent = send(cl->fd, cl->tx + cl->tx_off, cl->tx_len - cl->tx_off, MSG_DONTWAIT|MSG_NOSIGNAL);
    if(sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        case_failed(gobj, "TEST: a raw client cannot write");
        return;
    }
    if(sent > 0) {
        cl->tx_off += (size_t)sent;
    }
}

/***************************************************************************
 *  Give a small send buffer to the broker's side of the connection of a
 *  raw client: the socket of this process whose local port is the
 *  broker's and whose peer is the client
 ***************************************************************************/
PRIVATE void small_send_buffer_of_broker(hgobj gobj, int fd_client)
{
    struct sockaddr_in local;
    socklen_t len = sizeof(local);
    if(getsockname(fd_client, (struct sockaddr *)&local, &len) < 0) {
        case_failed(gobj, "TEST: getsockname() FAILED");
        return;
    }
    for(int fd=3; fd<FD_SETSIZE; fd++) {
        struct sockaddr_in me, peer;
        socklen_t lme = sizeof(me), lpeer = sizeof(peer);
        if(fd == fd_client ||
                getsockname(fd, (struct sockaddr *)&me, &lme) < 0 ||
                getpeername(fd, (struct sockaddr *)&peer, &lpeer) < 0 ||
                me.sin_family != AF_INET ||
                ntohs(me.sin_port) != BROKER_PORT ||
                peer.sin_port != local.sin_port) {
            continue;
        }
        int sndbuf = 4096;
        if(setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) < 0) {
            case_failed(gobj, "TEST: cannot make the send buffer of the broker small");
        }
        return;
    }
    case_failed(gobj, "TEST: the broker's side of the raw client not found");
}

/***************************************************************************
 *  Close the raw clients
 ***************************************************************************/
PRIVATE void close_clients(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->a.fd >= 0) {
        close(priv->a.fd);
        priv->a.fd = -1;
    }
    if(priv->b.fd >= 0) {
        close(priv->b.fd);
        priv->b.fd = -1;
    }
    if(priv->c.fd >= 0) {
        close(priv->c.fd);
        priv->c.fd = -1;
    }
}

/***************************************************************************
 *  The next step, with its deadline
 ***************************************************************************/
PRIVATE void next_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->step++;
    priv->t_step = start_msectimer(STEP_MSEC);
}

/***************************************************************************
 *  The steps of takeover_cases[]
 ***************************************************************************/
PRIVATE void run_takeover_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int sc = priv->scenario;
    const takeover_case_t *tc = &takeover_cases[sc - SC_TAKEOVERS];
    BOOL stalled = (tc->a_end == A_STALLED_F0);
    char client_id[32];
    char filter[32];
    char topic[32];
    snprintf(client_id, sizeof(client_id), "sessions%d", sc + 1);
    snprintf(filter, sizeof(filter), "q%d/#", sc + 1);
    snprintf(topic, sizeof(topic), "q%d/x", sc + 1);

    switch(priv->step) {
        case 0:
            client_connect(gobj, &priv->a, tc->a_version, client_id,
                tc->a_clean, tc->a_expiry, tc->a_keepalive, stalled? 4096 : 0);
            next_step(gobj);
            break;
        case 1:
            if(priv->a.connack == 1) {
                if(stalled) {
                    small_send_buffer_of_broker(gobj, priv->a.fd);
                }
                client_subscribe(gobj, &priv->a, tc->a_version, filter);
                for(int m=1; m<=tc->n_msgs; m++) {
                    client_publish(gobj, &priv->a, tc->a_version, 0x32, topic, (uint16_t)m);
                }
                next_step(gobj);
            }
            break;
        case 2:
            if(priv->a.suback == 1 && priv->a.puback == tc->n_msgs &&
                    priv->a.publish == MIN(tc->n_msgs, MAX_INFLIGHT)) {
                static const uint8_t disconnect[2] = {0xE0, 0x00};
                static const uint8_t disconnect0[9] = {     // Session Expiry Interval 0
                    0xE0, 0x07, 0x00, 0x05, 0x11, 0x00, 0x00, 0x00, 0x00
                };
                switch(tc->a_end) {
                    case A_LIVE:
                        break;
                    case A_DISC:
                    case A_DISC0:
                    case A_LIVE_DISC0:
                    case A_KEEPALIVE_DISC0:
                        {
                            BOOL with0 = (tc->a_end != A_DISC);
                            const uint8_t *d = with0? disconnect0 : disconnect;
                            size_t dlen = with0? sizeof(disconnect0) : sizeof(disconnect);
                            if(send(priv->a.fd, d, dlen, MSG_NOSIGNAL) != (ssize_t)dlen) {
                                case_failed(gobj, "TEST: cannot send DISCONNECT");
                                return;
                            }
                            if(tc->a_end == A_DISC || tc->a_end == A_DISC0) {
                                close(priv->a.fd);
                                priv->a.fd = -1;
                            }
                        }
                        break;
                    case A_STALLED_F0:
                        priv->a.no_read = TRUE;
                        priv->a.tx = burst5;
                        priv->a.tx_len = sizeof(burst5);
                        priv->a.tx_off = 0;
                        client_flush(gobj, &priv->a);
                        break;
                }
                priv->t_settle = start_msectimer(tc->wait_msec);
                if(tc->n_extra > 0) {
                    char c_id[40];
                    snprintf(c_id, sizeof(c_id), "%s-c", client_id);
                    client_connect(gobj, &priv->c, 4, c_id, TRUE, -1, 0, 0);
                    priv->extra_sent = FALSE;
                }
                next_step(gobj);
            }
            break;
        case 3:
            if(stalled && priv->a.tx_off < priv->a.tx_len) {
                break;
            }
            if(!test_msectimer(priv->t_settle)) {
                break;
            }
            if((tc->a_end == A_LIVE_DISC0 || tc->a_end == A_KEEPALIVE_DISC0) && !priv->a.closed) {
                break;      // the server closes A at timeout_close after its DISCONNECT: B comes after it
            }
            if(tc->n_extra > 0) {
                if(!priv->extra_sent) {
                    if(priv->c.connack != 1) {
                        break;
                    }
                    for(int m=1; m<=tc->n_extra; m++) {
                        client_publish(gobj, &priv->c, 4, 0x32, topic, (uint16_t)m);
                    }
                    priv->extra_sent = TRUE;
                    break;
                }
                if(priv->c.puback != tc->n_extra) {
                    break;
                }
            }
            client_connect(gobj, &priv->b, tc->b_version, client_id,
                tc->b_clean, tc->b_expiry, 0, 0);
            priv->b.ack = TRUE;
            next_step(gobj);
            break;
        case 4:
            if(priv->b.connack == 1) {
                if(priv->a.fd >= 0) {
                    close(priv->a.fd);  // A, taken over, goes
                    priv->a.fd = -1;
                }
                priv->t_settle = start_msectimer(BACKUP_MSEC);
                next_step(gobj);
            }
            break;
        case 5:
            if(test_msectimer(priv->t_settle)) {
                if(priv->b.connack_sp != tc->want_sp) {
                    case_failed(gobj, "TEST: B got a wrong session present");
                    return;
                }
                if(priv->b.publish != tc->want_msgs) {
                    case_failed(gobj, "TEST: B did not get the messages of the session");
                    return;
                }
                static const uint8_t disconnect[2] = {0xE0, 0x00};
                if(send(priv->b.fd, disconnect, sizeof(disconnect), MSG_NOSIGNAL) != 2) {
                    case_failed(gobj, "TEST: cannot send DISCONNECT");
                    return;
                }
                close_clients(gobj);
                priv->scenario++;
                priv->step = 0;
                priv->t_step = start_msectimer(STEP_MSEC);
            }
            break;
    }
}

/***************************************************************************
 *  The steps of the scenarios
 ***************************************************************************/
PRIVATE void run_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int sc = priv->scenario;
    if(sc >= SC_TAKEOVERS) {
        run_takeover_step(gobj);
        return;
    }
    int version = (sc == SC_CLEAN_5 || sc == SC_KEEP_5 || sc == SC_CLOSING_CLEAN_5)? 5 : 4;
    BOOL b_keeps = (sc == SC_KEEP_311 || sc == SC_KEEP_5 || sc == SC_STALLED_KEEP_311);
    BOOL stalled = (sc == SC_STALLED_CLEAN_311 || sc == SC_STALLED_KEEP_311);
    char client_id[32];
    char filter[32];
    char topic[32];
    snprintf(client_id, sizeof(client_id), "sessions%d", sc + 1);
    snprintf(filter, sizeof(filter), "q%d/#", sc + 1);
    snprintf(topic, sizeof(topic), "q%d/x", sc + 1);

    if(sc == SC_QOS2_FLIGHT_311) {
        switch(priv->step) {
            case 0:
                client_connect(gobj, &priv->a, 4, client_id, TRUE, -1, 0, 0);
                next_step(gobj);
                break;
            case 1:
                if(priv->a.connack == 1) {
                    for(int m=1; m<=QOS2_PAST_FLIGHT; m++) {
                        client_publish(gobj, &priv->a, 4, 0x34, topic, (uint16_t)m);
                    }
                    next_step(gobj);
                }
                break;
            case 2:
                if(priv->a.closed) {
                    /*
                     *  Not a count of PUBREC: the broker resets the
                     *  connection (the 21st PUBLISH is not read), and a
                     *  reset can discard what the client had not read yet
                     */
                    if(priv->a.pubrec > QOS2_PAST_FLIGHT - 1) {
                        case_failed(gobj, "TEST: 6. a PUBREC past max_inflight_messages");
                        return;
                    }
                    close_clients(gobj);
                    priv->scenario++;
                    priv->step = 0;
                    priv->t_step = start_msectimer(STEP_MSEC);
                }
                break;
        }
        return;
    }

    switch(priv->step) {
        case 0:
            client_connect(gobj, &priv->a, version, client_id, FALSE, 3600, 0, stalled? 4096 : 0);
            next_step(gobj);
            break;
        case 1:
            if(priv->a.connack == 1) {
                if(stalled) {
                    small_send_buffer_of_broker(gobj, priv->a.fd);
                }
                client_subscribe(gobj, &priv->a, version, filter);
                for(int m=1; m<=N_MESSAGES; m++) {
                    client_publish(gobj, &priv->a, version, 0x32, topic, (uint16_t)m);
                }
                next_step(gobj);
            }
            break;
        case 2:
            if(priv->a.suback == 1 && priv->a.puback == N_MESSAGES && priv->a.publish == N_MESSAGES) {
                if(sc == SC_CLOSING_CLEAN_5) {
                    static const uint8_t not_taken[2] = {0xF0, 0x00};
                    if(send(priv->a.fd, not_taken, sizeof(not_taken), MSG_NOSIGNAL) != 2) {
                        case_failed(gobj, "TEST: cannot send 0xF0");
                        return;
                    }
                }
                if(stalled) {
                    priv->a.no_read = TRUE;
                    priv->a.tx = burst;
                    priv->a.tx_len = sizeof(burst);
                    priv->a.tx_off = 0;
                    client_flush(gobj, &priv->a);
                    priv->t_settle = start_msectimer(500);  // the close waiting for the SUBACKs
                }
                next_step(gobj);
            }
            break;
        case 3:
            if(stalled && (priv->a.tx_off < priv->a.tx_len || !test_msectimer(priv->t_settle))) {
                break;
            }
            if(sc != SC_CLOSING_CLEAN_5 || priv->a.disconnect == 1) {
                client_connect(gobj, &priv->b, version, client_id, !b_keeps, b_keeps? 3600 : -1, 0, 0);
                next_step(gobj);
            }
            break;
        case 4:
            if(priv->b.connack == 1) {
                close(priv->a.fd);  // A, taken over, goes
                priv->a.fd = -1;
                priv->t_settle = start_msectimer(SETTLE_MSEC);
                next_step(gobj);
            }
            break;
        case 5:
            if(test_msectimer(priv->t_settle)) {
                int want = b_keeps? N_MESSAGES : 0;
                if(priv->b.connack_sp != (b_keeps? 1 : 0)) {
                    case_failed(gobj, "TEST: B got a wrong session present");
                    return;
                }
                if(priv->b.publish != want) {
                    case_failed(gobj, b_keeps?
                        "TEST: B kept the session and did not get its messages" :
                        "TEST: B started a clean session and got the messages of the old one"
                    );
                    return;
                }
                static const uint8_t disconnect[2] = {0xE0, 0x00};
                if(send(priv->b.fd, disconnect, sizeof(disconnect), MSG_NOSIGNAL) != 2) {
                    case_failed(gobj, "TEST: cannot send DISCONNECT");
                    return;
                }
                close_clients(gobj);
                priv->scenario++;
                priv->step = 0;
                priv->t_step = start_msectimer(STEP_MSEC);
            }
            break;
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The clients read, and the scenario goes on
 ***************************************************************************/
PRIVATE int ac_timeout_periodic(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    client_flush(gobj, &priv->a);
    client_read(gobj, &priv->a);
    client_read(gobj, &priv->b);
    client_read(gobj, &priv->c);

    if(priv->scenario == SC_END) {
        clear_timeout(priv->timer);
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "TEST: broker sessions done",
            NULL
        );
        set_yuno_must_die();
    } else if(test_msectimer(priv->t_step)) {
        case_failed(gobj, "TEST: a step did not end");
    } else if(priv->scenario > 0 || priv->step > 0 || test_msectimer(priv->t_settle)) {
        run_step(gobj);     // the first step waits for the broker to listen
    }

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
GOBJ_DEFINE_GCLASS(C_BROKER_SESSIONS);

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
        {EV_TIMEOUT_PERIODIC,   ac_timeout_periodic,0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT_PERIODIC,   0},
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
PUBLIC int register_c_broker_sessions(void)
{
    return create_gclass(C_BROKER_SESSIONS);
}
