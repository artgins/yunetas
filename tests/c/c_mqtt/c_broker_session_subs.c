/****************************************************************************
 *          C_BROKER_SESSION_SUBS.C
 *
 *          Driver of the test of what the subscriptions of a persistent
 *          session write to the treedb, of their limit per session, and of
 *          the share groups that a delivery walks. The clients are RAW plain
 *          sockets of this driver that write the MQTT packets by hand and
 *          read every 20 ms. The broker's C_PROT_MQTT2 has
 *          max_subscriptions_per_client 5.
 *
 *              A, MQTT 3.1.1, clean session 0 (a persistent session):
 *              1. SUBSCRIBE f/1, f/2, f/3 QoS 1: granted.
 *              2. 20 SUBSCRIBEs of f/1 QoS 1, the entry the session holds:
 *                 the record of the session is NOT written again (its
 *                 i_rowid does not move).
 *              3. SUBSCRIBE f/1 QoS 0: the entry changed, ONE write.
 *              4. DISCONNECT, a new connection keeping the session (session
 *                 present 1), SUBSCRIBE f/1, f/4, f/5, f/6: f/1 replaces,
 *                 f/4 and f/5 make 5, f/6 is past the 5 of the session:
 *                 0x80, with ONE WARNING "Mqtt: SUBSCRIBE past
 *                 max_subscriptions_per_client of the session, filters
 *                 refused". The connection itself holds 4.
 *              B, MQTT 5, Session Expiry 300:
 *              5. SUBSCRIBE g/1 .. g/5, DISCONNECT, a new connection, then
 *                 SUBSCRIBE g/1, g/6: g/1 granted, g/6 0x97 Quota exceeded
 *                 (its WARNING is inside the 10 s of the first: not said).
 *              C, MQTT 3.1.1, clean session 1:
 *              6. SUBSCRIBE $share/g1/sh/x and $share/g3/sh/x QoS 0, then
 *                 PUBLISH sh/x QoS 0: C gets it twice, once per group.
 *              7. UNSUBSCRIBE $share/g3/sh/x: one group left
 *                 (`shared-subs`). DISCONNECT: the close of its temporary
 *                 session removes the other: no group left.
 *
 *          Up to 7.31.29 step 2 wrote the session 20 times (about 30 KB
 *          each with 100 filters of 250 bytes), and the limit counted only
 *          the filters of the connection: 4 granted f/6, 5 granted g/6, and
 *          a session grew by up to 100 filters on each reconnection.
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
#include <yunetas.h>
#include "c_broker_session_subs.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BROKER_PORT         18133
#define BROKER_SERVICE      "mqtt_broker"
#define TREEDB_SERVICE      "treedb_mqtt_broker"

#define REPEATS             20          // SUBSCRIBEs of a filter held, unchanged
#define STEP_MSEC           5000        // a step that takes longer failed
#define RX_MAX              (16*1024)
#define MAX_CODES           16

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    int fd;
    int version;
    uint8_t rx[RX_MAX];
    size_t rx_len;
    BOOL closed;            // the broker closed it
    int connack;
    int connack_sp;         // session present of the last CONNACK
    int suback;
    int unsuback;
    int publish;
    uint8_t codes[MAX_CODES];   // the reason codes of the last SUBACK
    int n_codes;
} client_t;

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
    int step;
    uint64_t t_step;        // the deadline of the step
    uint64_t t_settle;
    BOOL failed;
    json_int_t rowid0;      // i_rowid of A's session before the repeated SUBSCRIBEs
    client_t a;
    client_t b;
    client_t c;
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
 *  A step did not end: say it, once, and end
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
        "step",         "%d", priv->step,
        "a_connack",    "%d", priv->a.connack,
        "a_suback",     "%d", priv->a.suback,
        "b_connack",    "%d", priv->b.connack,
        "b_suback",     "%d", priv->b.suback,
        "c_suback",     "%d", priv->c.suback,
        "c_publish",    "%d", priv->c.publish,
        NULL
    );
    clear_timeout(priv->timer);
    set_yuno_must_die();
}

/***************************************************************************
 *  A check of a step: an ERROR (the test fails) when it does not hold,
 *  and the test goes on, so one run shows every check that fails
 ***************************************************************************/
PRIVATE void check(hgobj gobj, BOOL ok, const char *what, json_int_t got, json_int_t expected)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!ok) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", what,
            "step",         "%d", priv->step,
            "got",          "%ld", (long)got,
            "expected",     "%ld", (long)expected,
            NULL
        );
    }
}

/***************************************************************************
 *  The reason codes of the last SUBACK of a client are these
 ***************************************************************************/
PRIVATE void check_codes(hgobj gobj, client_t *cl, const uint8_t *expected, int n)
{
    check(gobj, cl->n_codes == n, "TEST: SUBACK with another count of codes", cl->n_codes, n);
    for(int i=0; i<n && i<cl->n_codes; i++) {
        check(gobj, cl->codes[i] == expected[i], "TEST: SUBACK code", cl->codes[i], expected[i]);
    }
}

/***************************************************************************
 *  The i_rowid of the record of a session: it moves with each write
 ***************************************************************************/
PRIVATE json_int_t session_rowid(hgobj gobj, const char *client_id)
{
    json_t *session = gobj_get_node(
        gobj_find_service(TREEDB_SERVICE, TRUE),
        "sessions",
        json_pack("{s:s}", "id", client_id),
        json_pack("{s:b}", "with_metadata", 1),
        gobj
    );
    if(!session) {
        case_failed(gobj, "TEST: session not found");
        return -1;
    }
    json_int_t rowid = kw_get_int(gobj, session, "__md_treedb__`i_rowid", -1, KW_REQUIRED);
    JSON_DECREF(session)
    return rowid;
}

/***************************************************************************
 *  How many share groups the broker has (`shared-subs`)
 ***************************************************************************/
PRIVATE int shared_groups(hgobj gobj)
{
    json_t *response = gobj_command(
        gobj_find_service(BROKER_SERVICE, TRUE), "shared-subs", 0, gobj
    );
    json_t *data = kw_get_dict(gobj, response, "data", 0, KW_REQUIRED);
    int n = data? (int)json_object_size(data) : -1;
    JSON_DECREF(response)
    return n;
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
    if(len > 0) {
        memcpy(bf + n, body, len);
        n += len;
    }
    if(send(cl->fd, bf, n, MSG_NOSIGNAL) != (ssize_t)n) {
        case_failed(gobj, "TEST: cannot send a packet");
    }
}

/***************************************************************************
 *  A string of MQTT: its length and its bytes
 ***************************************************************************/
PRIVATE size_t put_string(uint8_t *bf, const char *s)
{
    size_t len = strlen(s);
    bf[0] = (uint8_t)(len >> 8);
    bf[1] = (uint8_t)(len & 0xFF);
    memcpy(bf + 2, s, len);
    return 2 + len;
}

/***************************************************************************
 *  Connect a raw client and send its CONNECT: clean session / Clean Start,
 *  MQTT 5 Session Expiry Interval (-1 none)
 ***************************************************************************/
PRIVATE void client_connect(
    hgobj gobj,
    client_t *cl,
    int version,
    const char *client_id,
    BOOL clean,
    int expiry
) {
    if(cl->fd >= 0) {
        close(cl->fd);
    }
    memset(cl, 0, sizeof(*cl));
    cl->version = version;
    cl->fd = socket(AF_INET, SOCK_STREAM, 0);
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
    n += put_string(body + n, "MQTT");
    body[n++] = (uint8_t)version;
    body[n++] = clean? 0x02 : 0x00;                 // clean session / clean start
    body[n++] = 0x00; body[n++] = 60;               // keep alive
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
    n += put_string(body + n, client_id);
    send_packet(gobj, cl, 0x10, body, n);
}

/***************************************************************************
 *  SUBSCRIBE of some filters, all with one QoS
 ***************************************************************************/
PRIVATE void client_subscribe(
    hgobj gobj,
    client_t *cl,
    uint16_t mid,
    const char **filters,
    int n_filters,
    int qos
) {
    uint8_t body[384];
    size_t n = 0;
    body[n++] = (uint8_t)(mid >> 8); body[n++] = (uint8_t)(mid & 0xFF);
    if(cl->version == 5) {
        body[n++] = 0;                              // properties
    }
    for(int i=0; i<n_filters; i++) {
        n += put_string(body + n, filters[i]);
        body[n++] = (uint8_t)qos;
    }
    send_packet(gobj, cl, 0x82, body, n);
}

/***************************************************************************
 *  UNSUBSCRIBE of a filter (MQTT 3.1.1)
 ***************************************************************************/
PRIVATE void client_unsubscribe(hgobj gobj, client_t *cl, uint16_t mid, const char *filter)
{
    uint8_t body[128];
    size_t n = 0;
    body[n++] = (uint8_t)(mid >> 8); body[n++] = (uint8_t)(mid & 0xFF);
    n += put_string(body + n, filter);
    send_packet(gobj, cl, 0xA2, body, n);
}

/***************************************************************************
 *  PUBLISH of QoS 0 (MQTT 3.1.1)
 ***************************************************************************/
PRIVATE void client_publish0(hgobj gobj, client_t *cl, const char *topic)
{
    uint8_t body[128];
    size_t n = 0;
    n += put_string(body + n, topic);
    memcpy(body + n, "msg", 3); n += 3;
    send_packet(gobj, cl, 0x30, body, n);
}

/***************************************************************************
 *  DISCONNECT, and close
 ***************************************************************************/
PRIVATE void client_disconnect(hgobj gobj, client_t *cl)
{
    if(cl->version == 5) {
        uint8_t body[2] = {0x00, 0x00};             // Normal disconnection, no properties
        send_packet(gobj, cl, 0xE0, body, sizeof(body));
    } else {
        send_packet(gobj, cl, 0xE0, NULL, 0);
    }
    close(cl->fd);
    cl->fd = -1;
}

/***************************************************************************
 *  Read what a client has, and count its packets
 ***************************************************************************/
PRIVATE void client_read(hgobj gobj, client_t *cl)
{
    if(cl->fd < 0 || cl->closed) {
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
                break;
            case 9:
                {
                    cl->suback++;
                    size_t first = (cl->version == 5)? 3 : 2;  // packet id, properties (none)
                    cl->n_codes = 0;
                    for(size_t k=first; k<remaining && cl->n_codes < MAX_CODES; k++) {
                        cl->codes[cl->n_codes++] = body[k];
                    }
                }
                break;
            case 11:
                cl->unsuback++;
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
 *  Run the current step: each one waits for what the previous one sent
 ***************************************************************************/
PRIVATE void run_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    client_t *a = &priv->a, *b = &priv->b, *c = &priv->c;

    switch(priv->step) {
        case 0:
            client_connect(gobj, a, 4, "ss_a", FALSE, -1);
            next_step(gobj);
            break;
        case 1:
            if(a->connack == 1) {
                const char *f[] = {"f/1", "f/2", "f/3"};
                client_subscribe(gobj, a, 1, f, 3, 1);
                next_step(gobj);
            }
            break;
        case 2:
            if(a->suback == 1) {
                const uint8_t want[] = {1, 1, 1};
                check_codes(gobj, a, want, 3);
                priv->rowid0 = session_rowid(gobj, "ss_a");
                const char *f[] = {"f/1"};
                for(int k=0; k<REPEATS; k++) {
                    client_subscribe(gobj, a, (uint16_t)(k + 2), f, 1, 1);
                }
                next_step(gobj);
            }
            break;
        case 3:
            if(a->suback == 1 + REPEATS) {
                json_int_t rowid = session_rowid(gobj, "ss_a");
                check(gobj, rowid == priv->rowid0,
                    "TEST: a SUBSCRIBE of a filter held, unchanged, wrote the session",
                    rowid, priv->rowid0
                );
                const char *f[] = {"f/1"};
                client_subscribe(gobj, a, 100, f, 1, 0);
                next_step(gobj);
            }
            break;
        case 4:
            if(a->suback == 2 + REPEATS) {
                const uint8_t want[] = {0};
                check_codes(gobj, a, want, 1);
                json_int_t rowid = session_rowid(gobj, "ss_a");
                check(gobj, rowid == priv->rowid0 + 1,
                    "TEST: a SUBSCRIBE that changes an entry did not write the session once",
                    rowid, priv->rowid0 + 1
                );
                client_disconnect(gobj, a);
                client_connect(gobj, a, 4, "ss_a", FALSE, -1);
                next_step(gobj);
            }
            break;
        case 5:
            if(a->connack == 1) {
                check(gobj, a->connack_sp == 1, "TEST: A's session not present", a->connack_sp, 1);
                const char *f[] = {"f/1", "f/4", "f/5", "f/6"};
                client_subscribe(gobj, a, 1, f, 4, 1);
                next_step(gobj);
            }
            break;
        case 6:
            if(a->suback == 1) {
                const uint8_t want[] = {1, 1, 1, 0x80};
                check_codes(gobj, a, want, 4);
                client_connect(gobj, b, 5, "ss_b", FALSE, 300);
                next_step(gobj);
            }
            break;
        case 7:
            if(b->connack == 1) {
                const char *f[] = {"g/1", "g/2", "g/3", "g/4", "g/5"};
                client_subscribe(gobj, b, 1, f, 5, 1);
                next_step(gobj);
            }
            break;
        case 8:
            if(b->suback == 1) {
                const uint8_t want[] = {1, 1, 1, 1, 1};
                check_codes(gobj, b, want, 5);
                client_disconnect(gobj, b);
                client_connect(gobj, b, 5, "ss_b", FALSE, 300);
                next_step(gobj);
            }
            break;
        case 9:
            if(b->connack == 1) {
                check(gobj, b->connack_sp == 1, "TEST: B's session not present", b->connack_sp, 1);
                const char *f[] = {"g/1", "g/6"};
                client_subscribe(gobj, b, 1, f, 2, 1);
                next_step(gobj);
            }
            break;
        case 10:
            if(b->suback == 1) {
                const uint8_t want[] = {1, 0x97};
                check_codes(gobj, b, want, 2);
                client_connect(gobj, c, 4, "ss_c", TRUE, -1);
                next_step(gobj);
            }
            break;
        case 11:
            if(c->connack == 1) {
                const char *f[] = {"$share/g1/sh/x", "$share/g3/sh/x"};
                client_subscribe(gobj, c, 1, f, 2, 0);
                next_step(gobj);
            }
            break;
        case 12:
            if(c->suback == 1) {
                const uint8_t want[] = {0, 0};
                check_codes(gobj, c, want, 2);
                check(gobj, shared_groups(gobj) == 2, "TEST: share groups", shared_groups(gobj), 2);
                client_publish0(gobj, c, "sh/x");
                next_step(gobj);
            }
            break;
        case 13:
            if(c->publish == 2) {
                client_unsubscribe(gobj, c, 2, "$share/g3/sh/x");
                next_step(gobj);
            }
            break;
        case 14:
            if(c->unsuback == 1) {
                check(gobj, c->publish == 2, "TEST: C got its message once per group", c->publish, 2);
                check(gobj, shared_groups(gobj) == 1, "TEST: share groups after the UNSUBSCRIBE",
                    shared_groups(gobj), 1
                );
                client_disconnect(gobj, c);
                next_step(gobj);
            }
            break;
        case 15:
            if(shared_groups(gobj) == 0) {
                next_step(gobj);
            }
            break;
        default:
            break;
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Every 20 ms: read the clients, then the step
 ***************************************************************************/
PRIVATE int ac_timeout_periodic(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    client_read(gobj, &priv->a);
    client_read(gobj, &priv->b);
    client_read(gobj, &priv->c);

    if(priv->step == 16) {
        clear_timeout(priv->timer);
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "TEST: broker session subscriptions done",
            NULL
        );
        set_yuno_must_die();
    } else if(test_msectimer(priv->t_step)) {
        case_failed(gobj, "TEST: a step did not end");
    } else if(priv->step > 0 || test_msectimer(priv->t_settle)) {
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
GOBJ_DEFINE_GCLASS(C_BROKER_SESSION_SUBS);

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
PUBLIC int register_c_broker_session_subs(void)
{
    return create_gclass(C_BROKER_SESSION_SUBS);
}
