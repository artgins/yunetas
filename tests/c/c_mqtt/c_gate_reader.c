/****************************************************************************
 *          C_GATE_READER.C
 *
 *          Driver of the test of the memory a gate holds for peers that
 *          pipeline their packets and READ every answer, over real sockets.
 *          This driver is the upper level of the server C_PROT_MQTT2s (no
 *          broker, no queues, allow_anonymous), as the decoder of a gate is,
 *          and its C_TCP_S accepts on them, with the default
 *          max_unwritten_bytes. The clients are RAW plain sockets of this
 *          driver, with the default buffers of the kernel: each sends a
 *          CONNECT of MQTT 3.1.1 and then PINGREQ without end, keeping up
 *          to 512 KB of them (262144) not yet answered, and reads
 *          everything it gets, every 20 ms. The window bounds the drain at
 *          the end (the kernel's buffers alone take some MB of PINGREQ),
 *          and is far more than one read of the gate, so its reading is
 *          paused with more to read every time.
 *
 *              20 connections for 20 s. The reading of each is paused by the
 *              bound on its answers again and again, and resumed when they
 *              are written. None is dropped; the bytes each one gets are its
 *              CONNACK and then one PINGRESP for each of its PINGREQ, in
 *              order; and the memory, sampled every 20 ms, grows less than
 *              5 MB (measured: 2.6 MB, the bound and one read for each
 *              connection). At the end the clients stop sending, get the
 *              PINGRESP still owed to them, and close.
 *
 *          What fails, in the run and in the drain alike: a reader owed
 *          answers that gets no byte for 1 s, in 10 looks or more. The
 *          gate and the readers share one loop, so a reader that is owed
 *          gets bytes at every look (20 ms) at any speed of the machine:
 *          no look found one without bytes, at a full CPU, at an eighth of
 *          one, and under ASan at a quarter of one. A reading not resumed
 *          (an EV_RX_RESUME lost) waits for the look of timeout_close, 3 s,
 *          or for ever: red in about 1 s, though the other readers go on.
 *          The drain (about 17 s here) fails too when it is not over in
 *          300 s: a drain that is slow but goes on is not a failure. Up to
 *          the first version of this test the drain had 60 s in all and
 *          failed under ASan; up to the second it failed only when NO
 *          reader got a byte for 10 s, so a resume lost now and then,
 *          rescued at 3 s, passed (in 275-296 s, the drain at half speed).
 *
 *          In the first pause (never released) the resume let the C_TCP
 *          read again BEFORE the rest held was processed: that rest paused
 *          the reading again, after about 331 PINGREQ, and the read the
 *          C_TCP had just started came while paused and went behind it.
 *          The rest held grew by about 3.4 KB on each pause and was copied
 *          whole each time: the memory without bound (here, up to the
 *          window of each client), the CPU at 100 %, and no WARNING. Red
 *          there: the memory past 5 MB within seconds.
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
#include <c_prot_mqtt2.h>
#include "c_gate_reader.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define GATE_URL            "tcp://127.0.0.1:18125"
#define GATE_PORT           18125

#define N_CONN              20
#define RUN_MSEC            20000
#define WINDOW              (512*1024)  // PINGREQ bytes not yet answered, at most
#define READER_STALL_MSEC   1000        // a reader owed answers that gets no byte for so long fails,
#define READER_STALL_LOOKS  10          // and in so many looks (a margin for a late look after a pause)
#define DRAIN_MAX_MSEC      300000      // the drain fails when it is not over by then
#define MAX_GROWTH          (5*1024*1024)

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void close_raw_clients(hgobj gobj);

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
    hgobj timer_io;         // the clients write and read, and the memory is sampled
    hgobj tcp_s;
    int fd[N_CONN];         // the raw clients
    uint64_t tx_bytes[N_CONN];  // of PINGREQ handed to the kernel
    uint64_t rx_bytes[N_CONN];  // got, the CONNACK included
    uint64_t t_reader_stall[N_CONN];    // a reader owed answers fails when it fires with no byte got
    unsigned empty_looks[N_CONN];       // and the looks that found it so
    uint64_t t_rx_last[N_CONN]; // ms (monotonic) of its last byte, or of the last look with nothing owed
    uint64_t wait_max;          // the longest a reader owed answers waited for a byte, ms
    int phase;
    int gate_closed;        // the gates' EV_ON_CLOSE
    BOOL failed;
    size_t mem_base;
    size_t mem_growth;      // the most the memory grew
} PRIVATE_DATA;

/*
 *  A stream of PINGREQ (0xC0 0x00) to write from, and a buffer to read
 *  into: not of gbmem, so the memory measured is the gate's only
 */
PRIVATE uint8_t pingreqs[64*1024];
PRIVATE uint8_t rx_buffer[64*1024];




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
    priv->timer_io = gobj_create_pure_child("io", C_TIMER, 0, gobj);

    for(int i=0; i<N_CONN; i++) {
        char name[NAME_MAX];
        snprintf(name, sizeof(name), "gate_input-%d", i);
        hgobj prot = gobj_create(
            name,
            C_PROT_MQTT2,
            json_pack("{s:b, s:b}",
                "iamServer", 1,
                "allow_anonymous", 1
            ),
            gobj
        );
        hgobj tcp = gobj_create(name, C_TCP, 0, prot);
        gobj_set_bottom_gobj(prot, tcp);
        priv->fd[i] = -1;
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

    for(size_t i=0; i<sizeof(pingreqs); i+=2) {
        pingreqs[i] = 0xC0;
        pingreqs[i+1] = 0x00;
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
    gobj_start(priv->timer_io);
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
    clear_timeout(priv->timer_io);
    gobj_stop(priv->timer_io);
    close_raw_clients(gobj);
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
    clear_timeout(priv->timer_io);

    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  A case went wrong: say it, once, and end
 ***************************************************************************/
PRIVATE void case_failed(hgobj gobj, const char *what, int idx)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->failed) {
        return;
    }
    priv->failed = TRUE;
    uint64_t tx = 0, rx = 0;
    for(int i=0; i<N_CONN; i++) {
        tx += priv->tx_bytes[i];
        rx += priv->rx_bytes[i];
    }
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", what,
        "phase",        "%d", priv->phase,
        "client",       "%d", idx,
        "tx_bytes",     "%lu", (unsigned long)(idx >= 0? priv->tx_bytes[idx] : 0),
        "rx_bytes",     "%lu", (unsigned long)(idx >= 0? priv->rx_bytes[idx] : 0),
        "tx_bytes_all", "%lu", (unsigned long)tx,
        "rx_bytes_all", "%lu", (unsigned long)rx,
        "gate_closed",  "%d", priv->gate_closed,
        "mem_growth",   "%lu", (unsigned long)priv->mem_growth,
        "wait_max_ms",  "%lu", (unsigned long)priv->wait_max,
        NULL
    );
    clear_timeout(priv->timer_io);
    clear_timeout(priv->timer);
    set_yuno_must_die();
}

/***************************************************************************
 *  A raw client with the default buffers, its CONNECT sent (MQTT 3.1.1,
 *  clean session, keep alive 0)
 ***************************************************************************/
PRIVATE int connect_raw_client(hgobj gobj, int idx)
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
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(GATE_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
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

    uint8_t connect[32] = {
        0x10, 0,                                // CONNECT, remaining length below
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04,   // protocol name, level 4 (3.1.1)
        0x02,                                   // flags: clean session
        0x00, 0x00                              // keep alive 0: no timer
    };
    char client_id[16];
    int idlen = snprintf(client_id, sizeof(client_id), "reader%d", idx);
    size_t len = 12;
    connect[len++] = 0x00;
    connect[len++] = (uint8_t)idlen;
    memcpy(connect + len, client_id, (size_t)idlen);
    len += (size_t)idlen;
    connect[1] = (uint8_t)(len - 2);
    if(send(fd, connect, len, MSG_NOSIGNAL) != (ssize_t)len) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot send the CONNECT",
            "errno",        "%s", strerror(errno),
            NULL
        );
        close(fd);
        return -1;
    }
    return fd;
}

/***************************************************************************
 *  A client writes PINGREQ, as much as its socket takes (while running), or
 *  the half PINGREQ left (while draining), and reads what it has: its
 *  CONNACK, then PINGRESPs, checked byte by byte
 ***************************************************************************/
PRIVATE void client_io(hgobj gobj, int idx)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int fd = priv->fd[idx];
    static const uint8_t connack[4] = {0x20, 0x02, 0x00, 0x00};

    size_t off = (size_t)(priv->tx_bytes[idx] % 2);
    uint64_t answered = priv->rx_bytes[idx] > 4? priv->rx_bytes[idx] - 4 : 0;
    uint64_t owed = priv->tx_bytes[idx] - answered;
    if((priv->phase == 1 && owed < WINDOW) || off) {
        size_t len = (priv->phase == 1)? sizeof(pingreqs) - off : 1;
        if(priv->phase == 1 && len > WINDOW - owed) {
            len = (size_t)(WINDOW - owed);
        }
        ssize_t sent = send(fd, pingreqs + off, len, MSG_DONTWAIT|MSG_NOSIGNAL);
        if(sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            case_failed(gobj, "TEST: a reader cannot write: dropped by the gate", idx);
            return;
        }
        if(sent > 0) {
            priv->tx_bytes[idx] += (uint64_t)sent;
        }
    }

    while(1) {
        ssize_t got = recv(fd, rx_buffer, sizeof(rx_buffer), MSG_DONTWAIT);
        if(got < 0) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            case_failed(gobj, "TEST: a reader cannot read: dropped by the gate", idx);
            return;
        }
        if(got == 0) {
            case_failed(gobj, "TEST: a reader was closed by the gate", idx);
            return;
        }
        for(ssize_t k=0; k<got; k++) {
            uint64_t p = priv->rx_bytes[idx] + (uint64_t)k;
            uint8_t expected;
            if(p < sizeof(connack)) {
                expected = connack[p];
            } else {
                expected = ((p - sizeof(connack)) % 2 == 0)? 0xD0 : 0x00;    // PINGRESP
            }
            if(rx_buffer[k] != expected) {
                priv->rx_bytes[idx] = p;
                case_failed(gobj, "TEST: a reader got a byte that is not its answer", idx);
                return;
            }
        }
        priv->rx_bytes[idx] += (uint64_t)got;
    }
}

/***************************************************************************
 *  Close the raw clients
 ***************************************************************************/
PRIVATE void close_raw_clients(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int i=0; i<N_CONN; i++) {
        if(priv->fd[i] >= 0) {
            close(priv->fd[i]);
            priv->fd[i] = -1;
        }
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The phases: 1 run, 2 drain (the answers still owed), 3 the closes. The
 *  timer of 2 and 3 is the guard that names the phase that did not end.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->phase) {
        case 0:
            priv->phase = 1;
            priv->mem_base = get_cur_system_memory();
            priv->mem_growth = 0;
            for(int i=0; i<N_CONN; i++) {
                priv->fd[i] = connect_raw_client(gobj, i);
                if(priv->fd[i] < 0) {
                    case_failed(gobj, "TEST: a reader cannot connect", i);
                    break;
                }
                priv->t_rx_last[i] = time_in_milliseconds_monotonic();
                priv->t_reader_stall[i] = start_msectimer(READER_STALL_MSEC);
            }
            if(!priv->failed) {
                set_timeout_periodic(priv->timer_io, 20);
                set_timeout(priv->timer, RUN_MSEC);
            }
            break;

        case 1:
            priv->phase = 2;
            set_timeout(priv->timer, DRAIN_MAX_MSEC);
            break;

        case 2:
            case_failed(gobj, "TEST: the readers did not get every PINGRESP in 300 s", -1);
            break;

        case 3:
            if(priv->gate_closed != N_CONN) {
                case_failed(gobj, "TEST: the gate did not see the closes of the readers", -1);
                break;
            }
            {
                uint64_t pingreq = 0;
                for(int i=0; i<N_CONN; i++) {
                    pingreq += priv->tx_bytes[i] / 2;
                }
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INFO,
                    "msg",          "%s", "TEST: gate readers done",
                    "connections",  "%d", N_CONN,
                    "pingreq",      "%lu", (unsigned long)pingreq,
                    "mem_growth",   "%lu", (unsigned long)priv->mem_growth,
                    "wait_max_ms",  "%lu", (unsigned long)priv->wait_max,
                    NULL
                );
            }
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
 *  The clients write and read, and the memory is sampled. Draining, once
 *  every client got the PINGRESP of each of its PINGREQ, they close.
 ***************************************************************************/
PRIVATE int ac_timeout_periodic(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t cur = get_cur_system_memory();
    if(cur > priv->mem_base && cur - priv->mem_base > priv->mem_growth) {
        priv->mem_growth = cur - priv->mem_base;
    }
    if(priv->mem_growth > MAX_GROWTH) {
        case_failed(gobj, "TEST: the memory of the readers grew past its bound", -1);
        KW_DECREF(kw)
        return 0;
    }

    BOOL all_answered = TRUE;
    uint64_t now = time_in_milliseconds_monotonic();
    for(int i=0; i<N_CONN && !priv->failed; i++) {
        uint64_t rx_before = priv->rx_bytes[i];
        client_io(gobj, i);
        if(priv->failed) {
            break;
        }
        if(priv->rx_bytes[i] != 4 + priv->tx_bytes[i] || priv->tx_bytes[i] % 2) {
            all_answered = FALSE;
        }

        /*
         *  A reader owed answers gets bytes at every look, at any speed of
         *  the machine (the gate and the readers share this loop); the
         *  looks are a margin for one that comes late after a pause of the
         *  whole process. One that waits is a reading not resumed: rescued
         *  only by the look of timeout_close (3 s), or never.
         */
        BOOL owed = priv->rx_bytes[i] < 4 + priv->tx_bytes[i] - priv->tx_bytes[i] % 2;
        if(priv->rx_bytes[i] > rx_before || !owed) {
            priv->t_rx_last[i] = now;
            priv->t_reader_stall[i] = start_msectimer(READER_STALL_MSEC);
            priv->empty_looks[i] = 0;
        } else {
            priv->empty_looks[i]++;
            if(now - priv->t_rx_last[i] > priv->wait_max) {
                priv->wait_max = now - priv->t_rx_last[i];
            }
            if(test_msectimer(priv->t_reader_stall[i]) && priv->empty_looks[i] >= READER_STALL_LOOKS) {
                case_failed(gobj, "TEST: a reader owed answers got no byte in 1 s: its reading not resumed", i);
                break;
            }
        }
    }

    if(priv->phase == 2 && all_answered && !priv->failed) {
        priv->phase = 3;
        clear_timeout(priv->timer_io);
        close_raw_clients(gobj);
        set_timeout(priv->timer, 5000);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A gate's connection closed: only after the reader closed it
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gate_closed++;
    if(priv->phase != 3) {
        case_failed(gobj, "TEST: the gate closed a reader", -1);
    } else if(priv->gate_closed == N_CONN) {
        set_timeout(priv->timer, 100);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The other events of the transports: nothing to do
 ***************************************************************************/
PRIVATE int ac_transport_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the protocol asks the upper level: nothing comes but PINGREQ
 ***************************************************************************/
PRIVATE int ac_on_iev_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "TEST: a reader sends PINGREQ only",
        NULL
    );

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
GOBJ_DEFINE_GCLASS(C_GATE_READER);

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
        {EV_TIMEOUT_PERIODIC,   ac_timeout_periodic,0},
        {EV_STOPPED,            ac_transport_event, 0},
        {EV_ON_OPEN,            ac_transport_event, 0},
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
PUBLIC int register_c_gate_reader(void)
{
    return create_gclass(C_GATE_READER);
}
