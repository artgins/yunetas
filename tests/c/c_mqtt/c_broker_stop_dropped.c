/****************************************************************************
 *          C_BROKER_STOP_DROPPED.C
 *
 *          Driver of the test of a broker that stops while connections it
 *          dropped, or a subscriber that does not read, are still held by
 *          their transport. Four RAW clients on plain sockets of this
 *          driver, each with a receive buffer of 4 KB (SO_RCVBUF); the
 *          broker's side of A, B and C with a send buffer of 4 KB
 *          (SO_SNDBUF), so what the broker writes waits in its C_TCP. A and
 *          B do not read. Each writes a CONNECT (clean session) and 20
 *          SUBSCRIBEs of 16000 times the filter "a", each
 *          answered by a SUBACK of 16 KB (a big answer: a socket that does
 *          not read stops taking it, where it keeps taking a trickle of
 *          PINGRESPs):
 *
 *              A (MQTT 3.1.1), B (MQTT 5): the bound on the answers pauses
 *              their reading, and as they take nothing, timeout_close (500
 *              ms here) later the broker drops them, with the WARNING "Mqtt:
 *              the peer does not read its answers, connection dropped". Their
 *              C_TCP then waits timeout_stop_tx (10 s) for the write in
 *              flight, which the clients never take.
 *
 *              C (MQTT 3.1.1) subscribes to s/# QoS 1 and does not read;
 *              D (MQTT 3.1.1, reading) publishes 20000 messages to s/x, QoS
 *              0 and 1 in turn. C is not dropped (what a broker forwards is
 *              not an answer): its C_TCP queue holds the QoS 0 forwards, and
 *              its queue of the protocol the QoS 1 ones, in flight or
 *              queued.
 *
 *          3 s after the clients, the yuno stops, as a kill-yuno or an
 *          update stops it. The clients keep their sockets open through the
 *          stop (they are closed when the driver is destroyed), so the
 *          transports are still stopping when the tree is destroyed, and
 *          their EV_DISCONNECTED never comes. Up to 7.30.26 the queues of
 *          the sessions, opened at their CONNECT and closed only by
 *          that EV_DISCONNECTED, leaked: "system memory not free" (2 x 768
 *          bytes per session; and C, with the QoS 1 messages its queue
 *          held: the leak seen with a subscriber that does not read when
 *          QoS 0 and QoS 1 publications mix, as the QoS 0 forwards hold
 *          its C_TCP's stop and the QoS 1 ones fill its queue) and exit
 *          255. Now the protocol frees them at its destroy.
 *
 *          The test passes when the yuno ends with no ERROR and no leak.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <yunetas.h>
#include "c_broker_stop_dropped.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BROKER_PORT         18127
#define BIG_SUBSCRIBES      20
#define BIG_FILTERS         16000       // a SUBACK of 16 KB
#define N_CLIENTS           4
#define N_PUBLISH           20000       // QoS 0 and 1, one each in turn

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void close_raw_clients(hgobj gobj);
PRIVATE void flush_raw_clients(hgobj gobj);

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
    int fd[N_CLIENTS];      // the raw clients A, B, C, D
    gbuffer_t *pending[N_CLIENTS];  // what the kernel did not take yet
    int ticks;              // of the wait before the stop
    int phase;
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
    for(int i=0; i<N_CLIENTS; i++) {
        priv->fd[i] = -1;
    }
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    close_raw_clients(gobj);
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
 *  A raw client that does not read: a socket with a receive buffer of
 *  4 KB, connected. Its bytes are written later (send_raw_client()), once
 *  the broker's side of the connection has a small send buffer too.
 ***************************************************************************/
PRIVATE int connect_raw_client(hgobj gobj)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: socket() FAILED",
            "errno",        "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    int rcvbuf = 4096;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BROKER_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if(setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) < 0 ||
            connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot connect the raw client",
            "errno",        "%s", strerror(errno),
            NULL
        );
        close(fd);
        return -1;
    }
    return fd;
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
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: getsockname() FAILED",
            "errno",        "%s", strerror(errno),
            NULL
        );
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
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "TEST: cannot make the send buffer of the broker small",
                "errno",        "%s", strerror(errno),
                NULL
            );
        }
        return;
    }
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "TEST: the broker's side of the raw client not found",
        NULL
    );
}

/***************************************************************************
 *  The raw client writes its CONNECT and its SUBSCRIBEs, as the kernel
 *  takes them (flush_raw_clients())
 ***************************************************************************/
PRIVATE void send_raw_client(hgobj gobj, int fd, int client)
{
    if(fd < 0) {
        return; // Error already logged
    }
    if(client != 3) {
        small_send_buffer_of_broker(gobj, fd);  // D, the publisher, reads
    }

    BOOL mqtt5 = (client == 1);
    char client_id[32];
    snprintf(client_id, sizeof(client_id), "dropped_cl%c", 'A' + client);
    size_t idlen = strlen(client_id);
    size_t sub_remaining = 2 + (mqtt5? 1 : 0) + (size_t)BIG_FILTERS * 4;
    gbuffer_t *gbuf = gbuffer_create(4096, 4*1024*1024);
    gbuffer_append_char(gbuf, 0x10);    // CONNECT
    gbuffer_append_char(gbuf, (char)(10 + (mqtt5? 1 : 0) + 2 + idlen));
    static const uint8_t head[] = {
        0x00, 0x04, 'M', 'Q', 'T', 'T'
    };
    gbuffer_append(gbuf, (void *)head, sizeof(head));
    gbuffer_append_char(gbuf, mqtt5? 0x05 : 0x04);  // level
    gbuffer_append_char(gbuf, 0x02);                // flags: clean session
    gbuffer_append_char(gbuf, 0x00);                // keep alive 0: no timer
    gbuffer_append_char(gbuf, 0x00);
    if(mqtt5) {
        gbuffer_append_char(gbuf, 0x00);            // properties length
    }
    gbuffer_append_char(gbuf, 0x00);
    gbuffer_append_char(gbuf, (char)idlen);
    gbuffer_append(gbuf, (void *)client_id, idlen);

    if(client == 2) {
        static const uint8_t subscribe[] = {
            0x82, 0x08, 0x00, 0x01, 0x00, 0x03, 's', '/', '#', 0x01     // s/# QoS 1
        };
        gbuffer_append(gbuf, (void *)subscribe, sizeof(subscribe));

    } else if(client == 3) {
        char payload[100];
        memset(payload, 'p', sizeof(payload));
        for(int m=1; m<=N_PUBLISH; m++) {
            int qos = m % 2;
            gbuffer_append_char(gbuf, (char)(0x30 | (qos << 1)));
            gbuffer_append_char(gbuf, (char)(2 + 3 + (qos? 2 : 0) + sizeof(payload)));
            gbuffer_append_char(gbuf, 0x00);
            gbuffer_append_char(gbuf, 0x03);
            gbuffer_append(gbuf, "s/x", 3);
            if(qos) {
                gbuffer_append_char(gbuf, (char)((m >> 8) & 0xFF));
                gbuffer_append_char(gbuf, (char)(m & 0xFF));
            }
            gbuffer_append(gbuf, payload, sizeof(payload));
        }

    } else {
        for(int k=0; k<BIG_SUBSCRIBES; k++) {
            gbuffer_append_char(gbuf, (char)0x82);  // SUBSCRIBE, remaining length in 3 bytes
            gbuffer_append_char(gbuf, (char)((sub_remaining & 0x7F) | 0x80));
            gbuffer_append_char(gbuf, (char)(((sub_remaining >> 7) & 0x7F) | 0x80));
            gbuffer_append_char(gbuf, (char)(sub_remaining >> 14));
            gbuffer_append_char(gbuf, 0x00);
            gbuffer_append_char(gbuf, (char)(k + 1));   // packet id
            if(mqtt5) {
                gbuffer_append_char(gbuf, 0x00);    // properties length
            }
            for(int i=0; i<BIG_FILTERS; i++) {
                gbuffer_append_char(gbuf, 0x00);
                gbuffer_append_char(gbuf, 0x01);
                gbuffer_append_char(gbuf, 'a');
                gbuffer_append_char(gbuf, 0x00);    // QoS 0
            }
        }
    }

    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->pending[client] = gbuf;
    flush_raw_clients(gobj);
}

/***************************************************************************
 *  Write what the kernel takes of what the raw clients have to send; the
 *  rest goes at the next turn of the timer
 ***************************************************************************/
PRIVATE void flush_raw_clients(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->fd[3] >= 0) {
        char bf[4096];  // D reads its PUBACKs: it is not a peer that does not read
        ssize_t got;
        do {
            got = recv(priv->fd[3], bf, sizeof(bf), MSG_DONTWAIT);
        } while(got > 0);
    }

    for(int i=0; i<N_CLIENTS; i++) {
        gbuffer_t *gbuf = priv->pending[i];
        if(!gbuf || priv->fd[i] < 0) {
            continue;
        }
        ssize_t sent = send(priv->fd[i], gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf), MSG_DONTWAIT);
        if(sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "TEST: the raw client cannot write",
                "client",       "%d", i,
                "errno",        "%s", strerror(errno),
                NULL
            );
            GBUFFER_DECREF(priv->pending[i])
            continue;
        }
        if(sent > 0) {
            gbuffer_get(gbuf, (size_t)sent);
        }
        if(gbuffer_leftbytes(gbuf) == 0) {
            GBUFFER_DECREF(priv->pending[i])
        }
    }
}




/***************************************************************************
 *  Close the raw clients: at the destroy of the driver, after the stop
 ***************************************************************************/
PRIVATE void close_raw_clients(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int i=0; i<N_CLIENTS; i++) {
        if(priv->fd[i] >= 0) {
            close(priv->fd[i]);
            priv->fd[i] = -1;
        }
        GBUFFER_DECREF(priv->pending[i])
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The phases: the two clients, then the stop of the yuno
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase++;
    switch(priv->phase) {
        case 1:
        case 2:
        case 3:
        case 4:
            if(priv->phase > 1) {
                send_raw_client(gobj, priv->fd[priv->phase - 2], priv->phase - 2);
            }
            priv->fd[priv->phase - 1] = connect_raw_client(gobj);
            set_timeout(priv->timer, 100);  // once the broker accepted it
            break;

        case 5:
            send_raw_client(gobj, priv->fd[3], 3);
            set_timeout(priv->timer, 100);
            break;

        case 6:
            flush_raw_clients(gobj);
            priv->ticks++;
            if(priv->ticks < 30) {      // 3 s: both dropped (timeout_close 500)
                priv->phase--;
                set_timeout(priv->timer, 100);
                break;
            }
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "TEST: the yuno stops with the dropped connections still stopping",
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
GOBJ_DEFINE_GCLASS(C_BROKER_STOP_DROPPED);

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
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
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
PUBLIC int register_c_broker_stop_dropped(void)
{
    return create_gclass(C_BROKER_STOP_DROPPED);
}
