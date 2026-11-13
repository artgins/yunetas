/****************************************************************************
 *          c_tcp.c
 *
 *          TCP Transport: tcp,ssl,...
 *          Low level linux with io_uring
 *
 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include <ytls.h>
#include <yev_loop.h>
#include "c_timer.h"
#include "c_yuno.h"
#include "c_tcp.h"


/*
    ONE write in flight per connection, TLS included.

    A write that io_uring completes short is re-armed with its rest, and
    that rest must reach the socket before anything written later. The
    plain path writes the next gbuffer only when the current one completed
    (gbuf_txing). The TLS path could not rely on that: ytls hands over its
    encrypted output from several places (the message being written, the
    handshake, post-handshake records), and each one used to start its own
    write at once. With two in flight, a short one sent its rest AFTER the
    other and the peer failed the record MAC ("bad record mac"). Encrypted
    output now waits in dl_tx_encrypted while a write is in flight, and the
    next clear message is taken only when that queue is empty.

    Secrets and the `traffic` trace. The trace dumps what goes out in
    clear, before TLS, so a credential sent as data (an SMTP AUTH line)
    lands in the log. Send it with `"__secret__": true` in the kw of
    EV_TX_DATA (or mark the gbuffer with gbuffer_set_secret()): the flag
    goes on the gbuffer, survives the tx queues of a connection not yet
    up, and the dump prints "<N bytes hidden>" instead of the bytes.

        json_t *kw_tx = json_pack("{s:I, s:b}",
            "gbuffer", (json_int_t)(uintptr_t)gbuf,
            "__secret__", 1
        );
        gobj_send_event(gobj_bottom_gobj(gobj), EV_TX_DATA, kw_tx, gobj);

    What is RECEIVED is dumped as it comes: the sender cannot flag it.

    The TLS handshake has a deadline, `timeout_tls_handshake` (10 s by
    default, 0: none), on a server and on a client: armed on the C_TIMER of
    the gobj when ST_WAIT_HANDSHAKE begins (a clisrv with TLS gets its
    timer at its start), cleared when the handshake ends or the connection
    does. Its EV_TIMEOUT drops the connection with a warning naming the
    peer, one per 10 s for the whole yuno, with the count of the ones not
    written ("suppressed"); a client then reconnects as after any failed
    connection. The count of a window is said when the window closes: in
    the next warning when it comes within the next 10 s, else in a warning
    of its own, at that next deadline or at the stop of any C_TCP (the end
    of the yuno included). Up to 7.30.18 the deadlines after the last
    warning were said by nobody when no other came, and when one came, even
    hours later, they were added to its count. While the handshake runs
    the timer is its deadline only: the inactivity timer of a client is
    armed in ST_CONNECTED. It costs
    nothing per read. Before it (up to 7.30.16), a peer that sent nothing
    held a connection for ever: EV_CONNECTED comes only after the
    handshake, so no deadline of the host above (C_WEBSOCKET's
    timeout_handshake) ever started.

    This gclass works with two type of TCP clients:
            - cli (pure client)
            - clisrv (client of server)

            ┌───────────────────────────┐
            │       STOPPED             │
            └───────────────────────────┘

            ┌───────────────────────────┐
            │       DISCONNECTED        │   (Only cli)
            └───────────────────────────┘
                        |
            ┌───────────────────────────┐
            │       WAIT_DISCONNECTED   │
            └───────────────────────────┘
                        |
            ┌───────────────────────────┐
            │       WAIT_STOPPED        │
            └───────────────────────────┘
                        |
            ┌───────────────────────────┐
            │       WAIT_CONNECTED      │   (Only cli)
            └───────────────────────────┘
                        |
            ┌───────────────────────────┐
            │       CONNECTED           │
            └───────────────────────────┘
                        └───────────────────┐
                                ┌───────────────────────────┐
                                │       WAIT_HANDSHAKE      │
                                └───────────────────────────┘
                                            │
                                ┌───────────────────────────┐
                                │       IDLE                │
                                └───────────────────────────┘

    The stop contract: every stop of a running C_TCP ends in ST_STOPPED and
    publishes EV_STOPPED ONCE, to its parent or subscriber -- synchronously,
    inside gobj_stop(), when nothing was in flight, or later, from
    ST_WAIT_STOPPED, when the last operation completes. A host waits for it
    to destroy a volatile transport (if(gobj_is_volatil(src)) gobj_destroy).
    This holds for a client already disconnected (waiting for its reconnect
    timer) too: up to 7.25.4 that stop published nothing.

    So EV_STOPPED is the LAST thing a C_TCP does with itself: the host may
    have destroyed it when the publish returns, also when the stop comes
    from inside one of its own callbacks (a host that drops the connection
    in EV_RX_DATA). Nothing of the gobj is touched after it, and a caller
    that may have run a stop (try_to_stop_yevents(), ytls's -2222) returns
    at once. Up to 7.25.21 a volatile clisrv dropped in its EV_RX_DATA read
    its url after the publish, and over TLS took -2222 for a TLS error and
    wrote the cause and stopped the destroyed gobj again.

    Flow control of the host: `rx_paused`. Written TRUE, no more reads of
    data are started: the read in flight still ends and publishes its data,
    and the reads of a TLS handshake go on. Written FALSE, the read held by
    the pause starts again. A relay (sgateway) pauses the side it reads
    while the other side has data to write, and resumes it at the
    EV_TX_READY of that side, so the queue of the slow side does not grow
    with what the fast side reads. While the reading is paused the end of
    the connection by the peer is seen when it resumes (a write still sees
    it at once).

        gobj_write_bool_attr(tcp_in, "rx_paused", TRUE);    // after sending its data
        ...
        gobj_write_bool_attr(tcp_in, "rx_paused", FALSE);   // EV_TX_READY of the other side

    What is held to be sent. One message of the host is written at a time;
    the rest wait in the tx queue. `txMsgs` counts a message when it is
    taken to be written, in clear and over TLS alike (up to 7.30.26 only in
    clear): it moves as the peer takes what was written. `cur_tx_queue` is
    the messages waiting, and `cur_tx_bytes` the bytes of those and of the
    one being written, until its write ends: the memory a peer that does not
    read makes this yuno hold, whatever the size of each message. A host
    bounds a peer by it (through the bottom chain of a channel too):

        if(gobj_read_integer_attr(gobj_channel, "cur_tx_bytes") >= max_bytes) {
            gobj_send_event(gobj_channel, EV_DROP, 0, gobj);
        }

    The write in flight is kept (yev_tx, one at a time). A host that
    destroys its C_TCP before EV_STOPPED (in the turn of its stop, with a
    write in flight) gets it canceled at the destroy, and the loop frees it
    with no callback. Up to 7.29.3 the write events were not kept, and their
    completion ran on the freed gobj.
 */

/***************************************************************
 *              Constants
 ***************************************************************/
#define IS_CLI      (!empty_string(priv->url))
#define IS_CLISRV   (priv->__clisrv__)

/*
 *  The least time between connections when timeout_between_connections is
 *  0: a timer of 0 is no timer (C_TIMER stops), so 0 never reconnected, and
 *  said nothing. The same floor as gobj-js's C_IEVENT_CLI (RETRY_FLOOR_MS).
 */
#define RECONNECT_FLOOR_MS  1000

/*
 *  The warning of a TLS handshake not ended in time is the peer's to
 *  cause, as often as it connects: one per interval for the whole yuno,
 *  with the count of the ones not written (see ac_timeout_tls_handshake()).
 */
#define PEER_LOG_INTERVAL_MS (10*1000)

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void try_to_stop_yevents(hgobj gobj); // IDEMPOTENT
PRIVATE void bound_the_write_in_flight(hgobj gobj);
PRIVATE void log_closing_drops(hgobj gobj);
PRIVATE void set_connected(hgobj gobj, int fd);
PRIVATE void set_inactivity_timeout(hgobj gobj);
PRIVATE void resume_reading(hgobj gobj);
PRIVATE void say_suppressed_handshakes(hgobj gobj);
PRIVATE int start_pending_writes(hgobj gobj);
PRIVATE int start_write_event(hgobj gobj, yev_event_h yev_write_event);
PRIVATE void release_the_write(hgobj gobj, yev_event_h yev_write_event);
PRIVATE void take_the_message_to_write(hgobj gobj, gbuffer_t *gbuf);
PRIVATE void release_the_message_written(hgobj gobj);
PRIVATE void flush_the_tx_queue(hgobj gobj);
PRIVATE BOOL takes_writes(hgobj gobj);
PRIVATE BOOL tx_ready_is_coming(hgobj gobj);
PRIVATE int yev_callback(yev_event_h yev_event);
PRIVATE int ytls_on_handshake_done_callback(hgobj gobj, int error);
PUBLIC int ytls_on_clear_data_callback(hgobj gobj, gbuffer_t *gbuf);
PRIVATE int ytls_on_encrypted_data_callback(hgobj gobj, gbuffer_t *gbuf);
PRIVATE void set_disconnect_cause(hgobj gobj, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
PRIVATE void set_tls_disconnect_cause(hgobj gobj, const char *what);
PRIVATE const char *tls_last_error(hgobj gobj);

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE const sdata_desc_t attrs_table[] = { // WARNING repeated in c_tcp/c_esp_transport
/*-ATTR-type--------name----------------flag------------default-----description---------- */
SDATA (DTP_BOOLEAN, "__clisrv__",       SDF_STATS,      "FALSE",    "Client of tcp server. This tcp obj is created or configured by C_TCP_S, Check if the '__clisrv__' is TRUE to know if this is a server (client channel) tcp gobj."),
SDATA (DTP_STRING,  "url",              SDF_RD,         "",         "Url to connect in the case of tcp gobj client. Check if the 'url' is not empty to know if this is a client tcp gobj."),
SDATA (DTP_BOOLEAN, "use_ssl",          SDF_RD,         "FALSE",    "True if schema is secure. Set internally if client, externally is clisrv"),
SDATA (DTP_DICT,    "crypto",           SDF_RD,         "{}",       "Crypto config"),
SDATA (DTP_POINTER, "ytls",             0,              0,          "TLS handler"),
SDATA (DTP_INTEGER, "fd_clisrv",        SDF_RD,         0,          "socket fd of clisrv"),
SDATA (DTP_INTEGER, "fd_listen",        SDF_RD,         0,          "socket accept listen"),
SDATA (DTP_POINTER, "tcp_s",            0,              0,          "The C_TCP_S whose connection this clisrv holds, written by it: at the accept (legacy method) or at its start (new method). What its connection stats count by"),

SDATA (DTP_INTEGER, "connxs",           SDF_STATS,      "0",        "connection counter"),
SDATA (DTP_BOOLEAN, "connected",        SDF_VOLATIL|SDF_STATS, "FALSE", "Connection state. Important filter!"),
SDATA (DTP_BOOLEAN, "secure_connected", SDF_VOLATIL|SDF_STATS, "FALSE", "Connection state"),
SDATA (DTP_STRING,  "schema",           SDF_RD,         "",         "schema, decoded from url. Set internally"),
SDATA (DTP_STRING,  "jwt",              SDF_RD|SDF_SECRET,         "",         "TODO. Access with token JWT"),
SDATA (DTP_STRING,  "cert_pem",         SDF_PERSIST,    "",         "SSL server certificate, PEM format"),
SDATA (DTP_BOOLEAN, "skip_cert_cn",     SDF_RD,         "TRUE",     "Skip verification of cert common name"),

SDATA (DTP_BOOLEAN, "no_tx_ready_event",SDF_RD,         0,          "Set TRUE if you don't want EV_TX_READY event"),

SDATA (DTP_INTEGER, "max_tx_queue",     SDF_WR,         0,          "Maximum messages in tx queue. Default is 0: no limit."),
SDATA (DTP_INTEGER, "cur_tx_queue",     SDF_RD,         0,          "Current messages in tx queue"),
SDATA (DTP_INTEGER, "cur_tx_bytes",     SDF_RD,         0,          "Bytes of the messages of the host held to be sent: the ones waiting in the tx queue and the one being written, until its write ends. What a slow peer makes this yuno hold, whatever the size of each message"),
SDATA (DTP_BOOLEAN, "rx_paused",        SDF_WR,         "FALSE",    "Flow control of the host. TRUE: no more reads of data are started (the read in flight still ends and publishes its data; the reads of a TLS handshake go on). FALSE: the read held by the pause starts again. A relay pauses the side it reads while the other side has data to write, and resumes it at the EV_TX_READY of that side"),

SDATA (DTP_INTEGER, "rx_buffer_size",   SDF_PERSIST,    "4096", "Rx buffer size"),
SDATA (DTP_INTEGER, "timeout_between_connections", SDF_RD, "2000", "Idle timeout to wait between attempts of connection, in milliseconds"),
SDATA (DTP_INTEGER, "timeout_between_connections_max", SDF_RD, "0", "If > timeout_between_connections, reconnect uses exponential backoff from the base up to this cap (ms), resetting to base once a connection is established. 0 = disabled (legacy fixed interval)."),
SDATA (DTP_INTEGER, "timeout_inactivity", SDF_RD,       "-1", "Inactivity timeout in milliseconds to close the connection. Reconnect when new data arrived. With -1 never close."),
SDATA (DTP_INTEGER, "timeout_stop_tx",  SDF_RD,         "10000", "A stop (or a drop) waits for the write in flight: a peer that does not take its data for this many ms aborts the connection (TCP_USER_TIMEOUT), so the stop ends. 0: no bound"),
SDATA (DTP_INTEGER, "timeout_tls_handshake", SDF_RD,    "10000", "A TLS handshake that has not ended in this many ms drops the connection, with a warning: a peer that sends nothing, or too little, holds no connection longer. A client then reconnects as after any failed connection. 0: no deadline"),
SDATA (DTP_BOOLEAN, "connect_on_start", SDF_RD,         "TRUE",     "Client: connect when started. FALSE: stay in ST_DISCONNECTED until the owner sends EV_CONNECT (connections on demand)"),
SDATA (DTP_STRING,  "disconnect_cause", SDF_RD|SDF_STATS, "",       "Why the last connection, or the last attempt to connect, ended: the error of the socket, a TLS failure with its reason, 'Local dropping', 'Inactivity timeout', 'Local stop'. The first cause of an end is kept. Emptied when a connection attempt starts (EV_CONNECT or the reconnect timer) and when a connection begins"),

SDATA (DTP_INTEGER, "txBytes",          SDF_RSTATS,     "0", "Messages transmitted"),
SDATA (DTP_INTEGER, "rxBytes",          SDF_RSTATS,     "0", "Messages received"),
SDATA (DTP_INTEGER, "txMsgs",           SDF_RSTATS,     "0", "Messages of the host taken to be written (out of the tx queue), in clear and over TLS alike"),
SDATA (DTP_INTEGER, "rxMsgs",           SDF_RSTATS,     "0", "Messages received"),
SDATA (DTP_INTEGER, "max_tx_in_progress",SDF_RSTATS,    "0", "Most writes ever in flight at once: 1 is the rule, more would let a short write's rest go out of order"),
SDATA (DTP_INTEGER, "txMsgsDroppedAtStop",SDF_RSTATS,   "0", "Messages queued to send and dropped by a local stop (said with an INFO at the stop)"),
SDATA (DTP_STRING,  "peername",         SDF_VOLATIL|SDF_STATS, "",  "Peername"),
SDATA (DTP_STRING,  "sockname",         SDF_VOLATIL|SDF_STATS, "",  "Sockname"),
SDATA (DTP_INTEGER, "subscriber",       0,              0,          "subscriber of output-events. Default if null is parent."),

SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendant value!
 *  required paired correlative strings
 *  in s_user_trace_level
 *---------------------------------------------*/
enum {
    TRACE_CONNECT_DISCONNECT    = 0x0001,
    TRACE_TRAFFIC               = 0x0002,
    TRACE_TLS                   = 0x0004,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"connections",     "Trace connections and disconnections"},
{"traffic",         "Trace dump traffic"},
{"tls",             "Trace tls"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
/*
 *  A publish of an event hands control to the subscribers, which may stop
 *  the connection or, through its host, destroy the gobj (a volatile
 *  clisrv): the call that published keeps a marker on its stack, the
 *  markers are chained, and mt_destroy() clears them all. Read after the
 *  publish: FALSE, the gobj is gone and nothing of it may be touched.
 */
typedef struct alive_s {
    BOOL alive;
    struct alive_s *prev;
} alive_t;

typedef struct _PRIVATE_DATA {
    hgobj gobj_timer;               // a client's (reconnect, inactivity); with TLS, also the handshake's deadline
    BOOL __clisrv__;
    const char *url;
    yev_event_h yev_connect; // Used if not __clisrv__ (pure tcp client)
    yev_event_h yev_reading;
    yev_event_h yev_accept;
    int fd_clisrv;
    int fd_listen;
    json_int_t timeout_inactivity;
    json_int_t reconnect_backoff;   // current exponential-backoff delay (ms); 0 = reset to base on next use
    BOOL inform_disconnection;
    BOOL idle_closed;           // TRUE when the disconnect is an inactivity idle-close (don't auto-reconnect)
    BOOL reconnect_floor_said;  // the warning of a 0 between connections, once per gobj

    json_int_t connxs;
    json_int_t txMsgs;
    json_int_t rxMsgs;
    json_int_t txBytes;
    json_int_t rxBytes;
    json_int_t txMsgsDroppedAtStop;

    BOOL use_ssl;
    hytls ytls;
    hsskt sskt;

    dl_list_t dl_tx;
    gbuffer_t *gbuf_txing;
    json_int_t txing_bytes;         // the bytes gbuf_txing had when it was taken
    json_int_t cur_tx_bytes;        // dl_tx and gbuf_txing, kept as they change (cur_tx_bytes)
    json_int_t max_tx_queue;
    json_int_t tx_queue_dropped;    // writes dropped by a full queue, said 1 of max_tx_queue
    dl_list_t dl_tx_encrypted;      // TLS output waiting for the write in flight

    BOOL no_tx_ready_event;
    BOOL tx_ready_posted;           // an EV_TX_READY posted to itself (post_tx_ready())
    int tx_in_progress;
    json_int_t max_tx_in_progress;
    yev_event_h yev_tx;             // the write in flight: one at a time (see the header)

    BOOL rx_paused;                 // the host's flow control (see resume_reading())
    BOOL rx_held;                   // a read not started again because of rx_paused

    json_int_t closing_dropped_msgs;    // EV_TX_DATA while closing, said when the close ends
    json_int_t closing_dropped_bytes;

    alive_t *alive;                 // the markers of the publishes in progress (see alive_t)
} PRIVATE_DATA;

/*
 *  Not of one connection: a peer that opens many mute connections, each
 *  one a C_TCP of its own (a channel, or a volatile clisrv), is one cause
 */
PRIVATE uint64_t handshake_log_t_next = 0;      // msectimer: no warning before it
PRIVATE json_int_t handshake_log_suppressed = 0;  // not written since the last one






                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    dl_init(&priv->dl_tx, gobj);
    dl_init(&priv->dl_tx_encrypted, gobj);

    if(IS_CLI) {
        priv->gobj_timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    }

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)(uintptr_t)gobj_read_integer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    SET_PRIV(timeout_inactivity,    gobj_read_integer_attr)
    SET_PRIV(ytls,                  (hytls)(size_t)gobj_read_integer_attr)
    SET_PRIV(use_ssl,               gobj_read_bool_attr)
    SET_PRIV(__clisrv__,            gobj_read_bool_attr)
    SET_PRIV(fd_clisrv,             (int)gobj_read_integer_attr)
    SET_PRIV(fd_listen,             (int)gobj_read_integer_attr)
    SET_PRIV(url,                   gobj_read_str_attr)
    SET_PRIV(no_tx_ready_event,     gobj_read_bool_attr)
    SET_PRIV(max_tx_queue,          gobj_read_integer_attr)
    SET_PRIV(rx_paused,             gobj_read_bool_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout_inactivity,      gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(ytls,                  (hytls)(size_t)gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(use_ssl,               gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(__clisrv__,            gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(fd_clisrv,             (int)gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(fd_listen,             (int)gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(url,                   gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(no_tx_ready_event,     gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(max_tx_queue,          gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(rx_paused,             gobj_read_bool_attr)
    END_EQ_SET_PRIV()

    if(path && strcmp(path, "rx_paused")==0 && !priv->rx_paused) {
        resume_reading(gobj);
    }

    /*
     *  `stats=__reset__` writes the defaults of the SDF_RSTATS attrs: the
     *  counters live in priv (mt_reading), so they are zeroed here. Up to
     *  7.25.21 they were not, and kept counting from where they were
     */
    if(path && strcmp(path, "txBytes")==0) {
        priv->txBytes = 0;
    } else if(path && strcmp(path, "rxBytes")==0) {
        priv->rxBytes = 0;
    } else if(path && strcmp(path, "txMsgs")==0) {
        priv->txMsgs = 0;
    } else if(path && strcmp(path, "rxMsgs")==0) {
        priv->rxMsgs = 0;
    } else if(path && strcmp(path, "max_tx_in_progress")==0) {
        priv->max_tx_in_progress = 0;
    } else if(path && strcmp(path, "txMsgsDroppedAtStop")==0) {
        priv->txMsgsDroppedAtStop = 0;
    }
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_state_t state = gobj_current_state(gobj);
    if(!(state == ST_STOPPED || state == ST_DISCONNECTED)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Initial wrong tcp state",
            "state",        "%s", gobj_current_state(gobj),
            NULL
        );
        return -1;
    }

    if(priv->yev_connect) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "yev_connect ALREADY exists",
            "state",        "%s", gobj_current_state(gobj),
            NULL
        );
        return -1;
    }

    if(priv->__clisrv__) {
        /*
         *  clisrv
         *  It's already connected when is created
         */
        if(priv->use_ssl && !priv->gobj_timer) {
            // The deadline of the TLS handshake (timeout_tls_handshake): a server needs it too
            priv->gobj_timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
        }

        if(priv->fd_clisrv > 0) {
            /*
             *  Legacy method
             */
            set_connected(gobj, priv->fd_clisrv);

        } else {
            /*
             *  New method
             */
            if(priv->fd_listen > 0) {
                EXEC_AND_RESET(yev_destroy_event, priv->yev_accept)   // of the last start, stopped
                priv->yev_accept = yev_dup2_accept_event(
                    yuno_event_loop(),
                    yev_callback, // if return -1 the loop in yev_loop_run will break;
                    priv->fd_listen,
                    gobj
                );
                if(priv->yev_accept) {
                    yev_start_event(priv->yev_accept);
                    /*
                     *  Waiting for its accept, as set_disconnected() leaves
                     *  it. Up to 7.25.20 a start after a stop stayed in
                     *  ST_STOPPED with the accept armed: the next stop
                     *  returned at once (try_to_stop_yevents()), the accept
                     *  was never canceled, and it kept the listening socket
                     *  of its C_TCP_S bound.
                     */
                    gobj_change_state(gobj, ST_DISCONNECTED);
                }

            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "New method (listen) without fd_listen",
                    NULL
                );
            }
        }

    } else {
        /*
         *  cli
         *  Decode url to connect, create yev_create_connect_event
         */
        if(state == ST_STOPPED) {
            gobj_change_state(gobj, ST_DISCONNECTED);
        }

        /*
         *  Parse url to save schema
         *  It's parsing again in yev_setup_connect_event
         */
        char schema[40]; char host[120]; char port[40];
        if(parse_url(
            gobj,
            gobj_read_str_attr(gobj, "url"),
            schema, sizeof(schema),
            host, sizeof(host),
            port, sizeof(port),
            0, 0,
            0, 0,
            FALSE
        )<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Parsing url failed",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                NULL
            );
        }
        gobj_write_str_attr(gobj, "schema", schema);

        const char *url = gobj_read_str_attr(gobj, "url");
        priv->yev_connect = yev_create_connect_event(
            yuno_event_loop(),
            yev_callback,
            url,    // client_url
            NULL,   // local bind
            0,      // ai_family AF_UNSPEC
            0,      // ai_flags AI_V4MAPPED | AI_ADDRCONFIG
            gobj
        );

        if(!priv->yev_connect) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Cannot connect tcp gobj",
                NULL
            );
            gobj_write_str_attr(gobj, "disconnect_cause", "");    // a new start
            set_disconnect_cause(gobj, "Cannot create the connect");
            try_to_stop_yevents(gobj);
            return -1;
        }
    }

    if(IS_CLI) {
        /*
         *  cli
         *  try to connect
         */
        gobj_start(priv->gobj_timer);

        if(!gobj_read_bool_attr(gobj, "connect_on_start")) {
            /*
             *  The owner connects, with EV_CONNECT, when it has something to
             *  send. A client that connected at every start logged in to its
             *  server with nothing to say (C_SMTP_SESSION).
             */
        } else if(priv->timeout_inactivity > 0) {
            /*
             *  don't connect until arrives data to transmit, except first time
             */
            if(priv->connxs > 0) {
                // First connection already happened
            } else {
                // But connect once time at least.
                gobj_send_event(gobj, EV_CONNECT, 0, gobj);
            }
        } else {
            gobj_send_event(gobj, EV_CONNECT, 0, gobj);
        }
    }

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gobj_timer && gobj_is_running(priv->gobj_timer)) {
        gobj_stop(priv->gobj_timer);
    }

    say_suppressed_handshakes(gobj);

    if(!gobj_in_this_state(gobj, ST_STOPPED) && !gobj_in_this_state(gobj, ST_DISCONNECTED)) {
        set_disconnect_cause(gobj, "Local stop");
    }

    /*
     *  Released BEFORE the stop of the yevents, which is the last use of
     *  the gobj here: that stop can end in EV_STOPPED, where the host of a
     *  volatile C_TCP (a clisrv of the legacy method in a channel with no
     *  C_TCP) destroys it. The volatile attrs are reset where the stop
     *  ends (set_disconnected()), so its trace still says the peer.
     */
    if(priv->sskt) {
        ytls_free_secure_filter(priv->ytls, priv->sskt);
        priv->sskt = 0;
    }
    if(IS_CLI) {
        EXEC_AND_RESET(ytls_cleanup, priv->ytls)
    }

    /*
     *  What was queued and never written goes with the stop: said, once,
     *  an INFO (the stop is ours, not a fault of the peer). The messages of
     *  the host (dl_tx) are counted, also in txMsgsDroppedAtStop. The TLS
     *  output waiting behind the write in flight (dl_tx_encrypted: the rest
     *  of the message being written, or records of the TLS session) is not
     *  a message of the host: it is said in bytes, dropped_tls_bytes, and
     *  not counted as messages. Up to 7.30.26 both went in silence.
     */
    if(dl_size(&priv->dl_tx) > 0 || dl_size(&priv->dl_tx_encrypted) > 0) {
        json_int_t dropped_msgs = 0;
        json_int_t dropped_bytes = 0;
        json_int_t dropped_tls_bytes = 0;
        gbuffer_t *gbuf = dl_first(&priv->dl_tx);
        while(gbuf) {
            dropped_msgs++;
            dropped_bytes += (json_int_t)gbuffer_leftbytes(gbuf);
            gbuf = dl_next(gbuf);
        }
        gbuf = dl_first(&priv->dl_tx_encrypted);
        while(gbuf) {
            dropped_tls_bytes += (json_int_t)gbuffer_leftbytes(gbuf);
            gbuf = dl_next(gbuf);
        }
        priv->txMsgsDroppedAtStop += dropped_msgs;
        gobj_log_info(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",              "%s", "tcp data queued at a local stop, dropped",
            "url",              "%s", gobj_read_str_attr(gobj, "url"),
            "peername",         "%s", gobj_read_str_attr(gobj, "peername"),
            "dropped_msgs",     "%ld", (long)dropped_msgs,
            "dropped_bytes",    "%ld", (long)dropped_bytes,
            "dropped_tls_bytes", "%ld", (long)dropped_tls_bytes,
            NULL
        );
    }

    release_the_message_written(gobj);
    flush_the_tx_queue(gobj);
    dl_flush(&priv->dl_tx_encrypted, (fnfree)gbuffer_decref);

    try_to_stop_yevents(gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(alive_t *marker = priv->alive; marker; marker = marker->prev) {
        marker->alive = FALSE;  // destroyed inside a publish: its caller must not go on
    }

    // TODO someday review stopping
    // This would be in mt_stop, but by now not full control of stopping uring events
    // Other gclass's managing io_uring events with the same problem

    /*
     *  A C_TCP destroyed before its stop ended (in ST_WAIT_STOPPED) is no
     *  misuse and says nothing: it is how every yuno ends (stop_services()
     *  and then gobj_end(), with no turn of the loop between them), and a
     *  host may take the same shortcut. What is in flight is canceled here,
     *  and the loop frees it when its completion comes, with no callback.
     *  A destroy of a running gobj is said by gobj_destroy(), which stops
     *  it first.
     */

    log_closing_drops(gobj);    // destroyed in ST_WAIT_STOPPED: its close never ended

    /*
     *  A clisrv destroyed before its stop ended never publishes its
     *  EV_STOPPED: its C_TCP_S, which may be waiting for it to end a stop of
     *  its own, is told, on the next cycle (posted: this gobj is going)
     */
    hgobj tcp_s = priv->__clisrv__? (hgobj)gobj_read_pointer_attr(gobj, "tcp_s") : 0;
    if(tcp_s && !gobj_in_this_state(gobj, ST_STOPPED)) {
        gobj_post_event(tcp_s, EV_STOPPED, json_object(), tcp_s);
    }

    /*
     *  The write in flight of a C_TCP destroyed before its stop ended (the
     *  end of a yuno, or a host that did not wait for EV_STOPPED): canceled,
     *  and freed by the loop when its completion comes, with no callback,
     *  so that completion never reaches the freed gobj.
     */
    if(priv->yev_tx) {
        if(!yev_event_is_stopped(priv->yev_tx)) {
            yev_stop_event(priv->yev_tx);
        }
        EXEC_AND_RESET(yev_destroy_event, priv->yev_tx)
    }
    EXEC_AND_RESET(yev_destroy_event, priv->yev_connect)
    EXEC_AND_RESET(yev_destroy_event, priv->yev_reading)
    EXEC_AND_RESET(yev_destroy_event, priv->yev_accept)

    // A clisrv destroyed before its stop ended: its socket closed as try_to_stop_yevents() does
    if(priv->fd_clisrv > 0) {
        shutdown(priv->fd_clisrv, SHUT_WR);
        close(priv->fd_clisrv);
        priv->fd_clisrv = -1;
    }
}

/***************************************************************************
 *      Framework Method reading
 ***************************************************************************/
PRIVATE SData_Value_t mt_reading(hgobj gobj, const char *name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    SData_Value_t v = {0,{0}};
    if(strcmp(name, "txBytes")==0) {
        v.found = 1;
        v.v.i = priv->txBytes;
    } else if(strcmp(name, "rxBytes")==0) {
        v.found = 1;
        v.v.i = priv->rxBytes;
    } else if(strcmp(name, "txMsgs")==0) {
        v.found = 1;
        v.v.i = priv->txMsgs;
    } else if(strcmp(name, "rxMsgs")==0) {
        v.found = 1;
        v.v.i = priv->rxMsgs;
    } else if(strcmp(name, "cur_tx_queue")==0) {
        v.found = 1;
        v.v.i = (json_int_t)dl_size(&priv->dl_tx);
    } else if(strcmp(name, "cur_tx_bytes")==0) {
        v.found = 1;
        v.v.i = priv->cur_tx_bytes;
    } else if(strcmp(name, "max_tx_in_progress")==0) {
        v.found = 1;
        v.v.i = priv->max_tx_in_progress;
    } else if(strcmp(name, "txMsgsDroppedAtStop")==0) {
        v.found = 1;
        v.v.i = priv->txMsgsDroppedAtStop;
    } else if(strcmp(name, "connxs")==0) {
        v.found = 1;
        v.v.i = priv->connxs;
    }

    return v;
}




                    /***************************
                     *      Local methods
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int get_peer_and_sock_name(hgobj gobj, int fd)
{
    char temp[60];

    get_peername(temp, sizeof(temp), fd);
    gobj_write_str_attr(gobj, "peername", temp);

    get_sockname(temp, sizeof(temp), fd);
    gobj_write_str_attr(gobj, "sockname", temp);

    return 0;
}

/***************************************************************************
 *  The marker of a publish in progress (see alive_t)
 ***************************************************************************/
PRIVATE void alive_push(PRIVATE_DATA *priv, alive_t *marker)
{
    marker->alive = TRUE;
    marker->prev = priv->alive;
    priv->alive = marker;
}

PRIVATE void alive_pop(PRIVATE_DATA *priv, alive_t *marker)
{
    priv->alive = marker->prev;     // only while the gobj lives
}

/***************************************************************************
 *  Arm the inactivity timer (client only, one-shot, reset on each activity).
 *  No-op unless this is a pure tcp client with timeout_inactivity > 0.
 *  Only in ST_CONNECTED: the timer is the deadline of the TLS handshake in
 *  ST_WAIT_HANDSHAKE, and up to 7.30.17 each byte read there re-armed it
 *  with the inactivity time, so a peer that sent a byte now and then kept
 *  the handshake open past its deadline.
 ***************************************************************************/
PRIVATE void set_inactivity_timeout(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(IS_CLI && priv->gobj_timer && priv->timeout_inactivity > 0 &&
            gobj_in_this_state(gobj, ST_CONNECTED)) {
        set_timeout(priv->gobj_timer, priv->timeout_inactivity);
    }
}

/***************************************************************************
 *  The host resumes the reading it paused (rx_paused): the read held by
 *  the pause starts again. Only a held read: one in flight goes on as it
 *  is, and the one of a publish in progress is started by its callback.
 ***************************************************************************/
PRIVATE void resume_reading(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->rx_held) {
        return;
    }
    priv->rx_held = FALSE;
    if(priv->yev_reading && yev_event_is_idle(priv->yev_reading) &&
            gobj_in_this_state(gobj, ST_CONNECTED)) {
        gbuffer_clear(yev_get_gbuf(priv->yev_reading));
        yev_start_event(priv->yev_reading); // on failure: logged by yev_loop
    }
}

/***************************************************************************
 *  The deadlines of a closed window that no warning said (see
 *  ac_timeout_tls_handshake()): said in a warning of their own, when the
 *  window has closed or the yuno ends
 ***************************************************************************/
PRIVATE void say_suppressed_handshakes(hgobj gobj)
{
    if(handshake_log_suppressed == 0) {
        return;
    }
    if(!test_msectimer(handshake_log_t_next) && !gobj_is_shutdowning()) {
        return;
    }
    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PROTOCOL,
        "msg",          "%s", "TLS handshakes not ended in time, connections dropped, not said in their window",
        "suppressed",   "%ld", (long)handshake_log_suppressed,
        NULL
    );
    handshake_log_suppressed = 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void set_connected(hgobj gobj, int fd)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_str_attr(gobj, "disconnect_cause", "");    // a clisrv gets no EV_CONNECT

    log_closing_drops(gobj);    // a new connection starts its own count

    gobj_write_bool_attr(gobj, "connected", TRUE);
    get_peer_and_sock_name(gobj, fd);

    /*
     *  Info of "connected"
     */
    uint32_t trace_level = gobj_trace_level(gobj);
    if(trace_level & TRACE_CONNECT_DISCONNECT) {
        if(IS_CLI) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "Connected",
                "msg2",         "%s", "Connected To 🔵",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                "fd",           "%d", fd,
                NULL
            );
        } else {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "Connected",
                "msg2",         "%s", "Connected From 🔵",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                "fd",           "%d", fd,
                NULL
            );
        }
    }

    priv->connxs++;

    if(priv->gobj_timer) {
        clear_timeout(priv->gobj_timer);
    }

    /*-------------------------------*
     *      Setup reading event
     *-------------------------------*/
    if(!priv->yev_reading) {
        json_int_t rx_buffer_size = gobj_read_integer_attr(gobj, "rx_buffer_size");
        priv->yev_reading = yev_create_read_event(
            yuno_event_loop(),
            yev_callback,
            gobj,
            fd,
            gbuffer_create(rx_buffer_size, rx_buffer_size)
        );
    }

    if(priv->yev_reading) {
        yev_set_fd(priv->yev_reading, fd);

        if(!yev_get_gbuf(priv->yev_reading)) {
            json_int_t rx_buffer_size = gobj_read_integer_attr(gobj, "rx_buffer_size");
            yev_set_gbuffer(priv->yev_reading, gbuffer_create(rx_buffer_size, rx_buffer_size));
        } else {
            gbuffer_clear(yev_get_gbuf(priv->yev_reading));
        }

        priv->rx_held = FALSE;
        yev_start_event(priv->yev_reading);
    }

    /*---------------------------*
     *  Secure or clear traffic
     **---------------------------*/
    if(priv->use_ssl) {
        /*---------------------------*
         *      Secure traffic
         **---------------------------*/
        gobj_change_state(gobj, ST_WAIT_HANDSHAKE);

        /*
         *  The deadline of the handshake. Cleared when it ends
         *  (set_secure_connected()) or the connection does
         *  (try_to_stop_yevents()). Up to 7.30.16 there was none: EV_CONNECTED
         *  comes only after the handshake, so a host's own deadline
         *  (C_WEBSOCKET's timeout_handshake) never started, and a peer that
         *  said nothing held the connection, and its channel, for ever.
         */
        json_int_t timeout_tls_handshake = gobj_read_integer_attr(gobj, "timeout_tls_handshake");
        if(priv->gobj_timer && timeout_tls_handshake > 0) {
            set_timeout(priv->gobj_timer, timeout_tls_handshake);
        }

        priv->inform_disconnection = FALSE;

        priv->sskt = ytls_new_secure_filter(
            priv->ytls,
            ytls_on_handshake_done_callback,
            ytls_on_clear_data_callback,
            ytls_on_encrypted_data_callback,
            gobj
        );
        if(!priv->sskt) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "ytls_new_secure_filter() FAILED",
                NULL
            );
            set_disconnect_cause(gobj, "TLS: cannot create the secure filter"); // the backend logged why
            try_to_stop_yevents(gobj);
            return;
        }

        ytls_set_trace(priv->ytls, priv->sskt, (trace_level & TRACE_TLS)?TRUE:FALSE);

        // Hand the peer/sock names to ytls so its default-on TLS logs are
        // self-contained (peername/sockname are already set at accept/connect).
        ytls_set_peer_name(priv->ytls, priv->sskt,
            gobj_read_str_attr(gobj, "peername"),
            gobj_read_str_attr(gobj, "sockname")
        );

        /*
         *  Kick off the handshake explicitly. On the client side this emits
         *  the ClientHello via on_encrypted_data_cb (which is wired to
         *  EV_SEND_ENCRYPTED_DATA; we are already in ST_WAIT_HANDSHAKE so
         *  the event is accepted). On the server side it is a no-op that
         *  returns 0 (WANT_READ) until the peer speaks first.
         */
        int ret = ytls_do_handshake(priv->ytls, priv->sskt);
        if(ret == -2222) {
            /*
             *  The ClientHello's write could not start, and its end
             *  (set_disconnected) freed the session inside the callback:
             *  the connection is over and the gobj may be gone. Up to
             *  7.25.21 ytls touched the freed session on its way out.
             */
            return;
        }
        if(ret < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "ytls_do_handshake() FAILED",
                "error",        "%s", ytls_get_last_error(priv->ytls, priv->sskt),
                NULL
            );
            set_tls_disconnect_cause(gobj, "TLS: the handshake cannot start");
            try_to_stop_yevents(gobj);
            return;
        }

    } else {
        /*---------------------------*
         *      Clear traffic
         **---------------------------*/
        gobj_change_state(gobj, ST_CONNECTED);

        priv->reconnect_backoff = 0;    // established → reset reconnect backoff
        priv->inform_disconnection = TRUE;

        /*
         *  Publish
         */
        json_t *kw_conn = json_pack("{s:s, s:s, s:s}",
            "url",          gobj_read_str_attr(gobj, "url"),
            "peername",     gobj_read_str_attr(gobj, "peername"),
            "sockname",     gobj_read_str_attr(gobj, "sockname")
        );

        alive_t marker;
        alive_push(priv, &marker);
        gobj_publish_event(gobj, EV_CONNECTED, kw_conn);
        if(!marker.alive) {
            return;     // a subscriber ended the connection and its host destroyed the gobj
        }
        alive_pop(priv, &marker);
        if(!gobj_in_this_state(gobj, ST_CONNECTED)) {
            return;     // a subscriber dropped the connection: its end is done or under way
        }

        /*
         *  Inactivity model: flush data queued while disconnected and arm the
         *  inactivity timer (both no-op unless timeout_inactivity > 0).
         */
        if(start_pending_writes(gobj) < 0) {
            return;     // the connection ended, maybe the gobj: not touched
        }
        set_inactivity_timeout(gobj);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void set_secure_connected(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gobj_timer) {
        clear_timeout(priv->gobj_timer);    // the deadline of the handshake (set_connected())
    }

    gobj_write_bool_attr(gobj, "secure_connected", TRUE);
    priv->inform_disconnection = TRUE;
    priv->reconnect_backoff = 0;    // TLS established → reset reconnect backoff

    gobj_change_state(gobj, ST_CONNECTED);

    /*
     *  Publish. With the certificate the peer presented, if it did: who
     *  the peer is, for a server that binds an identity to it (mutual TLS).
     *  Trust it only with its "verified" true.
     */
    json_t *kw_conn = json_pack("{s:s, s:s, s:s}",
        "url",          gobj_read_str_attr(gobj, "url"),
        "peername",     gobj_read_str_attr(gobj, "peername"),
        "sockname",     gobj_read_str_attr(gobj, "sockname")
    );
    json_t *peer_cert = ytls_get_peer_cert_info(priv->ytls, priv->sskt);
    if(peer_cert) {
        json_object_set_new(kw_conn, "peer_cert", peer_cert);
    }

    /*
     *  A subscriber may drop the connection here, and inside the read that
     *  ended the handshake nothing is in flight: the end is synchronous, it
     *  frees the session and its host may destroy the gobj. Up to 7.25.21
     *  the flush below went on with both
     */
    alive_t marker;
    alive_push(priv, &marker);
    gobj_publish_event(gobj, EV_CONNECTED, kw_conn);
    if(!marker.alive) {
        return;     // the gobj is gone
    }
    alive_pop(priv, &marker);
    if(!priv->sskt || !gobj_in_this_state(gobj, ST_CONNECTED)) {
        return;     // a subscriber dropped the connection: its end is done or under way
    }

    int ret = ytls_flush(priv->ytls, priv->sskt);
    if(ret == -2222) {
        /*
         *  Re-entrant free: a subscriber of EV_RX_DATA (published from
         *  flush_clear_data's on_clear_data_cb) dropped or stopped this
         *  connection while we were still inside ytls_flush: its end is
         *  done or under way, and its host may have destroyed the gobj (a
         *  volatile clisrv). Bail before touching it; do NOT call
         *  try_to_stop_yevents().
         */
        return;
    }
    if(ret < -1000) {
        /*
         *  TLS error (-1111): the gobj is still alive, so tear it down here,
         *  matching the decrypt-path discipline at the ytls_decrypt_data
         *  caller (which calls try_to_stop_yevents() on a TLS error).
         *  -1 is a subscriber's own answer to the clear data: the
         *  connection goes on, as on the decrypt path.
         *  Error already logged in flush_clear_data.
         */
        set_tls_disconnect_cause(gobj, "TLS: the flush of clear data failed");
        try_to_stop_yevents(gobj);
        return;
    }

    /*
     *  Inactivity model: flush data queued while disconnected and arm the
     *  inactivity timer (both no-op unless timeout_inactivity > 0).
     */
    if(start_pending_writes(gobj) < 0) {
        return;     // the connection ended, maybe the gobj: not touched
    }
    set_inactivity_timeout(gobj);
}

/***************************************************************************
 *  WARNING set disconnected when all yevents has stopped
 *  It's the signal that gclass can be re-used
 *
 *  The tx queues: the in-flight write (gbuf_txing) is a partial write on a
 *  dead socket, and the encrypted output waiting to be written
 *  (dl_tx_encrypted) belongs to the TLS session that died: both are always
 *  discarded. The pending queue (dl_tx) is discarded when the connection had
 *  been established: its byte stream is broken, and resending would desync
 *  the protocol. When the connection was NEVER established
 *  (inform_disconnection still FALSE: a failed connect or a dropped
 *  handshake), those bytes never reached the wire: in the reconnect-on-tx
 *  model (timeout_inactivity) a running client keeps them, and
 *  start_pending_writes() flushes them once the retry connects.
 *
 *  A host may destroy the gobj when it gets EV_DISCONNECTED (see alive_t):
 *  then nothing of the gobj is touched after the publish.
 ***************************************************************************/
PRIVATE void set_disconnected(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint32_t trace_level = gobj_trace_level(gobj);
    if(trace_level & (TRACE_CONNECT_DISCONNECT)) {
        if(IS_CLI) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "Disconnected",
                "msg2",         "%s", "Disconnected To 🔴",
                "cause",        "%s", gobj_read_str_attr(gobj, "disconnect_cause"),
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                "peername",     "%s", gobj_read_str_attr(gobj, "peername"),
                "sockname",     "%s", gobj_read_str_attr(gobj, "sockname"),
                NULL
            );
        } else {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "Disconnected",
                "msg2",         "%s", "Disconnected From 🔴",
                "cause",        "%s", gobj_read_str_attr(gobj, "disconnect_cause"),
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                "peername",     "%s", gobj_read_str_attr(gobj, "peername"),
                "sockname",     "%s", gobj_read_str_attr(gobj, "sockname"),
                NULL
            );
        }
    }

    if(priv->yev_connect) {
        if(yev_get_fd(priv->yev_connect) > 0) {
            if(trace_level & TRACE_URING) {
                gobj_log_debug(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_YEV_LOOP,
                    "msg",          "%s", "close socket yev_connect",
                    "msg2",         "%s", "💥🟥 close socket yev_connect",
                    "fd",           "%d", yev_get_fd(priv->yev_connect) ,
                    "p",            "%p", priv->yev_connect,
                    NULL
                );
            }

            close(yev_get_fd(priv->yev_connect));
            yev_set_fd(priv->yev_connect, -1);
        }
    } else {
        if(priv->fd_clisrv > 0) {
            if(trace_level & TRACE_URING) {
                gobj_log_debug(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_YEV_LOOP,
                    "msg",          "%s", "close socket fd_clisrv",
                    "msg2",         "%s", "💥🟥 close socket fd_clisrv",
                    "fd",           "%d", priv->fd_clisrv ,
                    NULL
                );
            }

            close(priv->fd_clisrv);
            priv->fd_clisrv = -1;
        }
    }

    if(priv->yev_reading) {
        yev_set_fd(priv->yev_reading, -1);
    }

    if(priv->sskt) {
        ytls_free_secure_filter(priv->ytls, priv->sskt);
        priv->sskt = 0;
    }

    gobj_write_bool_attr(gobj, "connected", FALSE);
    gobj_write_bool_attr(gobj, "secure_connected", FALSE);

    // dl_tx is kept only when its bytes never reached the wire (see the header)
    release_the_message_written(gobj);
    dl_flush(&priv->dl_tx_encrypted, (fnfree)gbuffer_decref);
    BOOL keep_pending_tx =
        priv->timeout_inactivity > 0 &&
        !priv->inform_disconnection &&
        gobj_is_running(gobj);
    if(!keep_pending_tx) {
        flush_the_tx_queue(gobj);
    }

    /*
     *  Info of "disconnected"
     */
    if(priv->inform_disconnection) {
        priv->inform_disconnection = FALSE;

        // its host may destroy it on EV_DISCONNECTED (see the header)
        alive_t marker;
        alive_push(priv, &marker);
        gobj_publish_event(gobj, EV_DISCONNECTED, 0);
        if(!marker.alive) {
            return;     // the gobj is gone
        }
        alive_pop(priv, &marker);
    }

    // WARNING will reset attrs: connected,secure_connected,peername,sockname
    gobj_reset_volatil_attrs(gobj);

    if(IS_CLISRV) {
        priv->txMsgs = 0;
        priv->rxMsgs = 0;
        priv->txBytes = 0;
        priv->rxBytes = 0;

        if(gobj_is_running(gobj)) {
            if(priv->yev_accept) {
                /*
                 *  New method
                 */
                gobj_change_state(gobj, ST_DISCONNECTED);
                yev_start_event(priv->yev_accept);

            } else {
                /*
                 *  Legacy method
                 *  auto stop
                 */
                gobj_stop(gobj);
            }
        }

        if(!gobj_is_running(gobj)) {
            /*
             *  The gobj is in stop. Its host may destroy it here (a
             *  volatile clisrv): nothing of it is touched after this
             */
            gobj_publish_event(gobj, EV_STOPPED, 0);
        }
        return;
    }

    if(IS_CLI) {
        /*
         *  cli
         */
        BOOL idle_closed = priv->idle_closed;
        priv->idle_closed = FALSE;  // consumed; the next disconnect decides afresh

        if(gobj_is_running(gobj)) {
            gobj_change_state(gobj, ST_DISCONNECTED);
            if(priv->timeout_inactivity > 0 && idle_closed) {
                /*
                 *  Inactivity idle-close (connection was up, no pending work):
                 *  do NOT auto-reconnect. Stay disconnected until the next
                 *  on-demand reconnection (EV_TX_DATA / EV_CONNECT in
                 *  ST_DISCONNECTED).
                 */
                clear_timeout(priv->gobj_timer);
            } else {
                /*
                 *  A connect attempt failed or an established link dropped:
                 *  there IS intent to be connected, so retry with the
                 *  timeout_between_connections backoff (EV_TIMEOUT -> ac_connect).
                 *  This covers the classic auto-reconnect model AND a failed
                 *  on-demand reconnect in the inactivity model — without it the
                 *  inactivity client would stall after a single failed connect.
                 */
                json_int_t base = gobj_read_integer_attr(gobj, "timeout_between_connections");
                json_int_t maxbo = gobj_read_integer_attr(gobj, "timeout_between_connections_max");
                if(base == 0) {
                    /*
                     *  -1 is "no timer" on purpose (each connection waits for
                     *  an EV_CONNECT of the owner); 0 says nothing of the kind
                     */
                    if(!priv->reconnect_floor_said) {
                        priv->reconnect_floor_said = TRUE;
                        gobj_log_warning(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_PARAMETER,
                            "msg",          "%s", "timeout_between_connections 0 never reconnects, the floor is used",
                            "floor_ms",     "%d", RECONNECT_FLOOR_MS,
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
                            NULL
                        );
                    }
                    base = RECONNECT_FLOOR_MS;
                }
                json_int_t delay = base;
                if(maxbo > base) {
                    /*
                     *  Exponential backoff: a peer that keeps failing (e.g. an
                     *  IdP whose cert won't verify) must not hammer at the base
                     *  cadence. reconnect_backoff was reset to 0 on the last
                     *  established connection (see the ST_CONNECTED paths), so a
                     *  real reconnect starts again from base.
                     */
                    if(priv->reconnect_backoff < base) {
                        priv->reconnect_backoff = base;
                    }
                    delay = priv->reconnect_backoff;
                    json_int_t next = priv->reconnect_backoff * 2;
                    priv->reconnect_backoff = (next >= maxbo) ? maxbo : next;
                }
                set_timeout(priv->gobj_timer, delay);
            }
        } else {
            /*
             *  The gobj is in stop. Its host may destroy it here: nothing
             *  of it is touched after this
             */
            EXEC_AND_RESET(yev_destroy_event, priv->yev_connect)

            gobj_publish_event(gobj, EV_STOPPED, 0);
        }
    }
}

/***************************************************************************
 *  Start a write of the connection, counted in tx_in_progress only when it
 *  started. A write that does not start (no memory to keep its submission,
 *  an empty gbuffer, a bad fd: logged by yev_start_event) will never
 *  complete, and its bytes are not sent: the stream cannot go on with a
 *  hole in it, so the event is destroyed and the connection is dropped.
 *  Up to 7.25.4 the -1 was ignored: tx_in_progress counted a write that
 *  never completes, the event and its gbuffer leaked, and a stop waited in
 *  ST_WAIT_STOPPED for ever.
 ***************************************************************************/
PRIVATE int start_write_event(hgobj gobj, yev_event_h yev_write_event)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(yev_start_event(yev_write_event) < 0) {
        gbuffer_t *gbuf = yev_get_gbuf(yev_write_event);
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot start a write: the connection is dropped",
            "len",          "%d", gbuf? (int)gbuffer_leftbytes(gbuf): 0,
            NULL
        );
        release_the_write(gobj, yev_write_event);
        set_disconnect_cause(gobj, "Cannot start a write");
        try_to_stop_yevents(gobj);
        return -1;
    }
    if(priv->yev_tx && priv->yev_tx != yev_write_event) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A second tcp write in flight: one at a time is the rule",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            NULL
        );
    }
    priv->yev_tx = yev_write_event;
    priv->tx_in_progress++;
    if(priv->tx_in_progress > priv->max_tx_in_progress) {
        priv->max_tx_in_progress = priv->tx_in_progress;
    }
    return 0;
}

/***************************************************************************
 *  Free a write event of this connection: the one in flight is no longer
 *  kept
 ***************************************************************************/
PRIVATE void release_the_write(hgobj gobj, yev_event_h yev_write_event)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->yev_tx == yev_write_event) {
        priv->yev_tx = 0;
    }
    yev_destroy_event(yev_write_event);
}

/***************************************************************************
 *  `"__secret__": true` in the kw of EV_TX_DATA: the gbuffer holds a
 *  credential. The flag goes on the gbuffer, so it survives the tx queues,
 *  and the traffic dump prints "<N bytes hidden>" for it.
 ***************************************************************************/
PRIVATE void mark_secret_gbuffer(hgobj gobj, json_t *kw, gbuffer_t *gbuf)
{
    if(kw_get_bool(gobj, kw, "__secret__", 0, 0)) {
        gbuffer_set_secret(gbuf, TRUE);
    }
}

/***************************************************************************
 *  The message to write, its bytes kept: its write consumes them, and they
 *  are taken off cur_tx_bytes when it is released
 ***************************************************************************/
PRIVATE void take_the_message_to_write(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gbuf_txing = gbuf;
    priv->txing_bytes = (json_int_t)gbuffer_leftbytes(gbuf);
}

/***************************************************************************
 *  The message being written is released: written, or gone with the
 *  connection
 ***************************************************************************/
PRIVATE void release_the_message_written(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gbuf_txing) {
        priv->cur_tx_bytes -= priv->txing_bytes;
        priv->txing_bytes = 0;
        GBUFFER_DECREF(priv->gbuf_txing)
    }
}

/***************************************************************************
 *  The messages waiting in the tx queue are dropped
 ***************************************************************************/
PRIVATE void flush_the_tx_queue(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    dl_flush(&priv->dl_tx, (fnfree)gbuffer_decref);
    priv->cur_tx_bytes = priv->txing_bytes;    // what is left: the message being written, if any
}

/***************************************************************************
 *  Write the current gbuffer
 ***************************************************************************/
PRIVATE int write_data(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = priv->gbuf_txing;

    /*
     *  A message is counted when it is taken to be written, in clear and
     *  over TLS alike. Up to 7.30.26 only the clear path counted it, and
     *  the txMsgs of a TLS connection stayed 0
     */
    priv->txMsgs++;

    uint32_t trace_level = gobj_trace_level(gobj);
    if(trace_level & TRACE_TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "%s: %s%s%s",
            gobj_short_name(gobj),
            gobj_read_str_attr(gobj, "sockname"),
            " ⏩ ",
            gobj_read_str_attr(gobj, "peername")
        );
    }

    if(priv->sskt) {
        GBUFFER_INCREF(gbuf)
        int ret = ytls_encrypt_data(priv->ytls, priv->sskt, gbuf);
        if(ret == -2222) {
            /*
             *  The TLS session was freed inside: a subscriber of the
             *  EV_RX_DATA the backend published while it waited dropped the
             *  connection. Its end may have released gbuf, and its host
             *  may have destroyed the gobj: not touched
             */
            return -1;
        }
        if(ret < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "ytls_encrypt_data() FAILED",
                "error",        "%s", ytls_get_last_error(priv->ytls, priv->sskt),
                NULL
            );
            set_tls_disconnect_cause(gobj, "TLS: encrypt failed");
            try_to_stop_yevents(gobj);
            return -1;  // the end of the connection released gbuf, and maybe the gobj
        }
        if(gbuffer_leftbytes(gbuf) > 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "NEED a queue, NOT ALL DATA being encrypted",
                NULL
            );
        }
    } else {
        /*
         *  Transmit
         */
        int fd = priv->__clisrv__? priv->fd_clisrv:yev_get_fd(priv->yev_connect);
        yev_event_h yev_write_event = yev_create_write_event(
            yuno_event_loop(),
            yev_callback,
            gobj,
            fd,
            gbuffer_incref(gbuf)
        );
        if(!yev_write_event) {
            GBUFFER_DECREF(gbuf)    // the reference given to the event
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot create a write: the connection is dropped",
                NULL
            );
            set_disconnect_cause(gobj, "Cannot create a write");
            try_to_stop_yevents(gobj);
            return -1;
        }

        if(start_write_event(gobj, yev_write_event) < 0) {
            return -1;  // Error already logged
        }
    }
    return 0;
}

/***************************************************************************
 *  Write a gbuffer of TLS output, the only write in flight (see the header)
 ***************************************************************************/
PRIVATE int write_encrypted_data(hgobj gobj, gbuffer_t *gbuf /* owned */)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int fd = priv->__clisrv__? priv->fd_clisrv:yev_get_fd(priv->yev_connect);
    yev_event_h yev_write_event = yev_create_write_event(
        yuno_event_loop(),
        yev_callback,
        gobj,
        fd,
        gbuf
    );
    if(!yev_write_event) {
        GBUFFER_DECREF(gbuf)    // the reference given to the event
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot create a write: the connection is dropped",
            NULL
        );
        set_disconnect_cause(gobj, "Cannot create a write");
        try_to_stop_yevents(gobj);
        return -1;
    }

    return start_write_event(gobj, yev_write_event);    // on failure: logged, connection dropped
}

/***************************************************************************
 *  Try more writes
 ***************************************************************************/
PRIVATE void try_more_writes(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Clear the current tx msg
     */
    release_the_message_written(gobj);

    /*
     *  Get the next tx msg
     */
    gbuffer_t *gbuf_txing = dl_first(&priv->dl_tx);
    if(!gbuf_txing) {
        /*
         *  If no more publish tx ready
         */
        if(!priv->no_tx_ready_event) {
            json_t *kw_tx_ready = json_object();
            gobj_publish_event(gobj, EV_TX_READY, kw_tx_ready);
        }
    } else {
        dl_delete(&priv->dl_tx, gbuf_txing, 0);
        take_the_message_to_write(gobj, gbuf_txing);
        write_data(gobj);
    }
}

/***************************************************************************
 *  What an EV_TX_DATA must not carry. 0: send it. 1: an empty gbuffer,
 *  nothing to send: ignored, with a warning (up to 7.29.3 it was a write
 *  that cannot start, and the connection was dropped, with two errors); its
 *  EV_TX_READY comes from post_tx_ready(). -1: a gbuffer already queued or
 *  in flight here, whose bytes the first write consumes: refused, with an
 *  error (up to 7.29.3 it was queued again, which failed and lost a
 *  reference, or the second write found it empty and dropped the
 *  connection). A gbuffer in flight in ANOTHER transport is not seen: a
 *  gbuffer handed to a transport is its own until its EV_TX_READY.
 ***************************************************************************/
PRIVATE int check_tx_data(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gbuffer_leftbytes(gbuf) == 0) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tcp tx data empty, ignored",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            "peername",     "%s", gobj_read_str_attr(gobj, "peername"),
            NULL
        );
        return 1;
    }
    if(gbuf->__dl__ || gbuf == priv->gbuf_txing) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "tcp tx data already queued or in flight, refused",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            "peername",     "%s", gobj_read_str_attr(gobj, "peername"),
            "len",          "%d", (int)gbuffer_leftbytes(gbuf),
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Whether a write sent now is taken: connected, or, for a client that
 *  connects on demand (timeout_inactivity), also while disconnected or
 *  connecting (it is queued, and goes once connected). Elsewhere a write
 *  is dropped, and the host was told (EV_DISCONNECTED).
 ***************************************************************************/
PRIVATE BOOL takes_writes(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_in_this_state(gobj, ST_CONNECTED)) {
        return TRUE;
    }
    if(priv->timeout_inactivity > 0 && (
            gobj_in_this_state(gobj, ST_DISCONNECTED) ||
            gobj_in_this_state(gobj, ST_WAIT_CONNECTED) ||
            gobj_in_this_state(gobj, ST_WAIT_HANDSHAKE))) {
        return TRUE;
    }
    return FALSE;
}

/***************************************************************************
 *  Whether an EV_TX_READY is coming anyway: a write of the host is queued
 *  or being written, or, connected, any write is in flight (its end
 *  publishes it, try_more_writes()). A write of the TLS handshake ends
 *  with none: before ST_CONNECTED only the host's writes count.
 ***************************************************************************/
PRIVATE BOOL tx_ready_is_coming(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gbuf_txing || dl_size(&priv->dl_tx) > 0) {
        return TRUE;
    }
    if(gobj_in_this_state(gobj, ST_CONNECTED) &&
            (priv->tx_in_progress > 0 || dl_size(&priv->dl_tx_encrypted) > 0)) {
        return TRUE;
    }
    return FALSE;
}

/***************************************************************************
 *  The EV_TX_READY of an empty write, which sends nothing: a host that
 *  advances only on EV_TX_READY waited for ever (the fixes after 7.29.3).
 *  Wherever a write is taken (takes_writes()): in the fixes after 7.29.3
 *  only in ST_CONNECTED, and a client that connects on demand, sent an
 *  empty write while disconnected, waited for ever too. With a write in
 *  flight or queued, the EV_TX_READY of that one stands for it. It is
 *  posted to itself (ac_tx_ready_posted()), never published here: this
 *  runs inside the sender's gobj_send_event(), and a host that sends its
 *  next write in its EV_TX_READY would recurse.
 ***************************************************************************/
PRIVATE void post_tx_ready(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->no_tx_ready_event || priv->tx_ready_posted) {
        return;
    }
    if(tx_ready_is_coming(gobj)) {
        return;     // the EV_TX_READY of what is in flight comes when it ends
    }
    if(gobj_post_event(gobj, EV_TX_READY, json_object(), gobj) < 0) {
        return;     // Error already logged
    }
    priv->tx_ready_posted = TRUE;
}

/***************************************************************************
 *  Enqueue data
 ***************************************************************************/
PRIVATE int enqueue_write(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  The drops are counted per gobj: up to 7.29.3 one counter of the
     *  process said the drops of every C_TCP
     */
    size_t size = dl_size(&priv->dl_tx);
     if(priv->max_tx_queue > 0 && size >= priv->max_tx_queue) {
         if((priv->tx_queue_dropped % priv->max_tx_queue)==0) {
             gobj_log_error(gobj, 0,
                 "function",     "%s", __FUNCTION__,
                 "msgset",       "%s", MSGSET_INTERNAL,
                 "msg",          "%s", "throw away tcp tx message",
                 "counter",      "%d", (int)priv->tx_queue_dropped,
                 NULL
             );
         }
         priv->tx_queue_dropped++;
         gbuffer_t *gbuf_first = dl_first(&priv->dl_tx);
         dl_delete(&priv->dl_tx, gbuf_first, 0);
         priv->cur_tx_bytes -= (json_int_t)gbuffer_leftbytes(gbuf_first);
         gbuffer_decref(gbuf_first);
     }

    dl_add(&priv->dl_tx, gbuf);
    priv->cur_tx_bytes += (json_int_t)gbuffer_leftbytes(gbuf);

    return 0;
}

/***************************************************************************
 *  Flush data queued while disconnected, right after a (re)connection.
 *  Inactivity model: tx data that arrived while the client was disconnected
 *  was enqueued (ac_tx_data_disconnected/ac_tx_data_queued); start draining
 *  it now. No-op when the queue is empty (the normal, non-inactivity path).
 ***************************************************************************/
PRIVATE int start_pending_writes(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gbuf_txing) {
        return 0;
    }
    gbuffer_t *gbuf_txing = dl_first(&priv->dl_tx);
    if(gbuf_txing) {
        dl_delete(&priv->dl_tx, gbuf_txing, 0);
        take_the_message_to_write(gobj, gbuf_txing);
        return write_data(gobj);    // -1: the connection ended, maybe the gobj: not touched
    }
    return 0;
}


/***************************************************************************
 *  The close of a connection ended: what was sent to it while it closed
 *  (ac_tx_data_closing()) is said in ONE warning, not one per frame. Every
 *  end of a close says it: its end in try_to_stop_yevents(), a destroy
 *  while it still waits (mt_destroy), and, as a guard, the next connection.
 ***************************************************************************/
PRIVATE void log_closing_drops(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->closing_dropped_msgs == 0) {
        return;
    }
    gobj_log_warning(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_CONNECT_DISCONNECT,
        "msg",              "%s", "tcp data sent while the connection closes, dropped",
        "url",              "%s", gobj_read_str_attr(gobj, "url"),
        "peername",         "%s", gobj_read_str_attr(gobj, "peername"),
        "dropped_msgs",     "%ld", (long)priv->closing_dropped_msgs,
        "dropped_bytes",    "%ld", (long)priv->closing_dropped_bytes,
        NULL
    );
    priv->closing_dropped_msgs = 0;
    priv->closing_dropped_bytes = 0;
}

/***************************************************************************
 *  A stop waits for the write in flight, which ends when the peer takes its
 *  data. A peer that never does (it does not read, its window stays at
 *  zero) held the stop for ever: a clisrv in ST_WAIT_STOPPED, and its
 *  C_TCP_S, waiting for it, never listening again (up to 7.25.21). The
 *  socket gets a TCP_USER_TIMEOUT: data not acknowledged for that long
 *  aborts the connection, the write ends with an error (ETIMEDOUT, said
 *  with ONE warning), and the stop ends. A window that stays at zero counts
 *  too: the kernel aborts at its first zero-window probe past the timeout
 *  (the probes back off, so up to some seconds later). A peer that reads,
 *  however slowly, is not affected. Called before the socket of a client
 *  is closed (try_to_stop_yevents()).
 ***************************************************************************/
PRIVATE void bound_the_write_in_flight(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    unsigned int timeout = (unsigned int)gobj_read_integer_attr(gobj, "timeout_stop_tx");
    if(timeout == 0) {
        return;
    }
    int fd = priv->__clisrv__? priv->fd_clisrv : (priv->yev_connect? yev_get_fd(priv->yev_connect) : -1);
    if(fd <= 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "No socket to bound the write in flight: the stop waits for the peer",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            NULL
        );
        return;
    }
    if(setsockopt(fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &timeout, sizeof(timeout)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "setsockopt(TCP_USER_TIMEOUT) FAILED: the stop waits for the peer to take the data in flight",
            "fd",           "%d", fd,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
    }
}

/***************************************************************************
 *  Stop all events, is someone is running go to WAIT_STOPPED else STOPPED
 *  IMPORTANT this is the only place to set ST_WAIT_STOPPED state
 *
 *  The socket of a clisrv is its own (a client's is its connect event's,
 *  closed by the stop of that event, which waits as well for the write in
 *  flight), and it is closed only once all its
 *  io_uring events are stopped (tx_in_progress is then 0). shutdown(SHUT_WR)
 *  goes before close(): Linux sends a RST instead of a FIN when close() is
 *  called with unread data in the receive buffer (the peer's FIN arrived
 *  just after the last data was read), and shutdown(SHUT_WR) moves the
 *  socket to FIN_WAIT_1 first, so the close() follows the graceful
 *  teardown and the peer can end its own. The FIN goes out after what this
 *  side wrote: while that output is still in the send buffer, the close()
 *  of a socket with unread data drops it, and the peer gets a RST and no
 *  FIN. shutdown() is not checked: it can only fail on a connection that is
 *  already gone (ENOTCONN once the peer reset it), and then the close()
 *  that follows is all there is to do. mt_destroy() closes the socket of a
 *  clisrv destroyed before its stop ended the same way.
 ***************************************************************************/
#include <liburing.h> // TODO someday review stopping
PRIVATE void try_to_stop_yevents(hgobj gobj)  // IDEMPOTENT
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    BOOL to_wait_stopped = FALSE;

    if(gobj_current_state(gobj)==ST_STOPPED) {
        return;
    }

    /*
     *  The timer of a connection: the deadline of the TLS handshake, or the
     *  inactivity timer. Either would fire into ST_WAIT_STOPPED, where a
     *  drop waits for its write in flight (up to 7.30.16 the inactivity
     *  one did: "Event NOT DEFINED in state"). A client's reconnect timer
     *  is armed afterwards, by set_disconnected().
     */
    if(priv->gobj_timer && (
            gobj_in_this_state(gobj, ST_WAIT_HANDSHAKE) ||
            gobj_in_this_state(gobj, ST_CONNECTED))) {
        clear_timeout(priv->gobj_timer);
    }

    uint32_t trace_level = gobj_trace_level(gobj);
    if(trace_level & TRACE_URING) {
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "try_to_stop_yevents",
            "msg2",         "%s", "🟥🟥 try_to_stop_yevents",
            "gobj_",        "%s", gobj?gobj_short_name(gobj):"",
            NULL
        );
    }

    /*
     *  Before the stop of the connect event, which closes a client's
     *  socket: up to 7.30.16 a client's write in flight was never bounded
     *  (no socket was found), and its stop waited for ever for a peer that
     *  did not read. Once, as the stop starts to wait.
     */
    if(priv->tx_in_progress > 0 && !gobj_in_this_state(gobj, ST_WAIT_STOPPED)) {
        bound_the_write_in_flight(gobj);
    }

    /*
     *  The stop of the connect event closes a client's socket: not while a
     *  write is in flight. Its completion may be short, and its rest is
     *  written with the number of the socket; up to 7.30.17 that number was
     *  already closed, and the rest went to whatever file or socket had
     *  taken it. The completion of the last write calls here again.
     */
    if(priv->yev_connect && priv->tx_in_progress == 0) {
        if(!yev_event_is_stopped(priv->yev_connect)) {
            yev_stop_event(priv->yev_connect);
            if(!yev_event_is_stopped(priv->yev_connect)) {
                to_wait_stopped = TRUE;
            }
        }
    }

    if(priv->yev_reading) {
        if(!yev_event_is_stopped(priv->yev_reading)) {
            yev_stop_event(priv->yev_reading);
            if(!yev_event_is_stopped(priv->yev_reading)) {
                to_wait_stopped = TRUE;
            }
        }
    }

    if(priv->yev_accept) {
        if(!yev_event_is_stopped(priv->yev_accept)) {
            yev_stop_event(priv->yev_accept);
            if(!yev_event_is_stopped(priv->yev_accept)) {
                to_wait_stopped = TRUE;
            }
        }
    }

    if(priv->tx_in_progress > 0) {
        to_wait_stopped = TRUE;
    }

    // TODO someday review stopping
    // unsigned x = io_uring_cq_ready(yuno_event_loop());
    // unsigned xx = io_uring_sq_ready(yuno_event_loop());
    // printf("💥💥💥 %s state %s, cq_ready %d\n",
    //     gobj_short_name(gobj), gobj_current_state(gobj), x
    // );
    // printf("💥💥💥 %s state %s, sq_ready %d\n",
    //     gobj_short_name(gobj), gobj_current_state(gobj), xx
    // );

    if(to_wait_stopped) {
        gobj_change_state(gobj, ST_WAIT_STOPPED);
    } else {
        log_closing_drops(gobj);

        // All io_uring events are stopped: the socket closes, shutdown() first (see the header)
        if(priv->fd_clisrv > 0) {
            if(trace_level & TRACE_URING) {
                gobj_log_debug(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_YEV_LOOP,
                    "msg",          "%s", "close socket fd_clisrv",
                    "msg2",         "%s", "💥🟥 close socket fd_clisrv",
                    "fd",           "%d", priv->fd_clisrv ,
                    NULL
                );
            }
            shutdown(priv->fd_clisrv, SHUT_WR);
            close(priv->fd_clisrv);
            priv->fd_clisrv = -1;
        }
        if(gobj_current_state(gobj)==ST_DISCONNECTED) {
            /*
             *  set_disconnected() already ran on the disconnect callback: it
             *  put us in ST_DISCONNECTED and, for a RUNNING client, armed the
             *  reconnect EV_TIMEOUT (or left it disconnected for an on-demand
             *  EV_TX_DATA reconnect). Forcing ST_STOPPED here would ORPHAN that
             *  timer — EV_TIMEOUT then fires in ST_STOPPED (st_stopped handles
             *  nothing) and the client never reconnects. This bit the
             *  controlcenter_cli after an EV_DROP: it landed in ST_STOPPED and
             *  the reconnect timeout was silently dropped.
             *
             *  Only finalize to ST_STOPPED when the gobj is actually being
             *  stopped (mt_stop clears the running flag before we get here);
             *  otherwise stay ST_DISCONNECTED so the armed reconnect fires.
             *
             *  The stop of a client that was ALREADY disconnected ends as
             *  every other stop does (set_disconnected): its connect event
             *  freed (mt_start creates a new one) and EV_STOPPED published,
             *  once -- the event a host waits for to destroy a volatile
             *  transport. Up to 7.25.4 this stop published nothing and kept
             *  the connect event, which the next start overwrote (a leak).
             */
            if(!gobj_is_running(gobj)) {
                gobj_change_state(gobj, ST_STOPPED);
                EXEC_AND_RESET(yev_destroy_event, priv->yev_connect)
                gobj_publish_event(gobj, EV_STOPPED, 0);
            }
        } else {
            gobj_change_state(gobj, ST_STOPPED);
            set_disconnected(gobj);
        }
    }
}

/***************************************************************************
 *
 ***************************************************************************/
// PRIVATE int send_clear_data(hytls ytls, hsskt sskt, gbuffer_t *gbuf)
// {
//     if(ytls_encrypt_data(ytls, sskt, gbuf)<0) {
//         gobj_log_error(0, 0,
//             "function",     "%s", __FUNCTION__,
//             "msgset",       "%s", MSGSET_SYSTEM,
//             "msg",          "%s", "ytls_encrypt_data() FAILED",
//             "error",        "%s", ytls_get_last_error(ytls, sskt),
//             NULL
//         );
//         return -1;
//     }
//     if(gbuffer_leftbytes(gbuf) > 0) {
//         gobj_log_error(0, 0,
//             "function",     "%s", __FUNCTION__,
//             "msgset",       "%s", MSGSET_INTERNAL,
//             "msg",          "%s", "NEED a queue, NOT ALL DATA being encrypted",
//             NULL
//         );
//     }
//     return 0;
// }

/***************************************************************************
 *  The reason of the last TLS failure of this connection, "" when its TLS
 *  is already gone (a decrypt error can come after the session was freed)
 ***************************************************************************/
PRIVATE const char *tls_last_error(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->ytls || !priv->sskt) {
        return "";
    }
    return ytls_get_last_error(priv->ytls, priv->sskt);
}

/***************************************************************************
 *  Why the connection, or the attempt to connect, ends: in the attr
 *  `disconnect_cause`, which the owner reads when it sees the state change,
 *  and the Disconnected trace says. The FIRST cause of an end wins: what
 *  ends it ("Local dropping", "Inactivity timeout", an error) comes before
 *  the cancel of its read, whose -ECANCELED must not hide it. Emptied when
 *  a connection begins (EV_CONNECT, set_connected()). Not the process-wide
 *  last message: that is any ERROR of the process, and a TLS backend had
 *  put the specific reason of its failure there.
 ***************************************************************************/
PRIVATE void set_disconnect_cause(hgobj gobj, const char *fmt, ...)
{
    if(!empty_string(gobj_read_str_attr(gobj, "disconnect_cause"))) {
        return; // the first cause of this end is kept
    }
    char cause[512];    // a TLS reason (256, the backends' last_error) and what failed
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(cause, sizeof(cause), fmt, ap);
    va_end(ap);
    if(len < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "cannot format the disconnect cause",
            "fmt",          "%s", fmt,
            NULL
        );
        return;
    }
    if(len >= (int)sizeof(cause)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "disconnect cause truncated",
            "length",       "%d", len,
            "cause",        "%s", cause,
            NULL
        );
    }
    gobj_write_str_attr(gobj, "disconnect_cause", cause);
}

/***************************************************************************
 *  A TLS cause: what failed, and the backend's reason when it has one
 ***************************************************************************/
PRIVATE void set_tls_disconnect_cause(hgobj gobj, const char *what)
{
    const char *reason = tls_last_error(gobj);
    set_disconnect_cause(gobj, "%s%s%s",
        what,
        empty_string(reason)? "" : ": ",
        empty_string(reason)? "" : reason
    );
}

/***************************************************************************
 *  YTLS callbacks, called when handshake is done, or failed (error < 0)
 ***************************************************************************/
PRIVATE int ytls_on_handshake_done_callback(hgobj gobj, int error)
{
    uint32_t trace_level = gobj_trace_level(gobj);
    if((trace_level & TRACE_CONNECT_DISCONNECT) && error >= 0) {
        // The FAILS case is reported default-on (self-contained, with peername)
        // by the ytls backend's do_handshake; don't duplicate it here.
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "TLS Handshake OK",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            "peername",     "%s", gobj_read_str_attr(gobj, "peername"),
            "sockname",     "%s", gobj_read_str_attr(gobj, "sockname"),
            NULL
        );
    }

    if(error < 0) {
        /*
         *  Don't stop here, will be stopped in return of ytls_decrypt_data()
         */
        set_tls_disconnect_cause(gobj, "TLS handshake failed");
    } else {
        set_secure_connected(gobj);
    }

    return 0;
}

/***************************************************************************
 *  YTLS callbacks, called when receiving data has been decrypted
 ***************************************************************************/
PUBLIC int ytls_on_clear_data_callback(hgobj gobj, gbuffer_t *gbuf)
{
    //PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint32_t trace_level = gobj_trace_level(gobj);
    if(trace_level & TRACE_TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "decrypted data");
    }

    json_t *kw = json_object();
    json_object_set_new(kw, "gbuffer", json_integer((json_int_t)(uintptr_t)gbuf));
    return gobj_publish_event(gobj, EV_RX_DATA, kw);
}

/***************************************************************************
 *  YTLS callbacks, called when transmitting data has been encrypted
 ***************************************************************************/
PRIVATE int ytls_on_encrypted_data_callback(hgobj gobj, gbuffer_t *gbuf)
{
    //PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint32_t trace_level = gobj_trace_level(gobj);
    if(trace_level & TRACE_TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "encrypted data");
    }

    json_t *kw = json_object();
    json_object_set_new(kw, "gbuffer", json_integer((json_int_t)(uintptr_t)gbuf));
    return gobj_send_event(gobj, EV_SEND_ENCRYPTED_DATA, kw, gobj);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int yev_callback(yev_event_h yev_event)
{
    if(!yev_event) {
        /*
         *  It's the timeout
         */
        return 0;
    }

    hgobj gobj = yev_get_gobj(yev_event);
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint32_t trace_level = gobj_trace_level(gobj);
    int trace = trace_level & TRACE_URING;
    if(trace) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_get_flag(yev_event));
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev callback",
            "msg2",         "%s", "TCP 🌐🌐💥 yev callback",
            "event type",   "%s", yev_event_type_name(yev_event),
            "state",        "%s", yev_get_state_name(yev_event),
            "result",       "%d", yev_get_result(yev_event),
            "sres",         "%s", (yev_get_result(yev_event)<0)? strerror(-yev_get_result(yev_event)):"",
            "flag",         "%j", jn_flags,
            "p",            "%p", yev_event,
            "fd",           "%d", yev_get_fd(yev_event),
            "gbuffer",      "%p", yev_get_gbuf(yev_event),
            NULL
        );
        json_decref(jn_flags);
    }

    yev_state_t yev_state = yev_get_state(yev_event);

    switch(yev_get_type(yev_event)) {
        case YEV_READ_TYPE:
            {
                /*
                 *  yev_get_gbuf(yev_event) can be null if yev_stop_event() was called
                 */
                gbuffer_t *gbuf = yev_get_gbuf(yev_event);
                if(yev_state == YEV_ST_IDLE) {
                    if(trace_level & TRACE_TRAFFIC) {
                        gobj_trace_dump_gbuf(gobj, gbuf, "%s: %s%s%s",
                            gobj_short_name(gobj),
                            gobj_read_str_attr(gobj, "sockname"),
                            " ⏪ ",
                            gobj_read_str_attr(gobj, "peername")
                        );
                    }

                    priv->rxMsgs++;
                    priv->rxBytes += (json_int_t)gbuffer_leftbytes(gbuf);

                    set_inactivity_timeout(gobj); // rx activity: reset timer

                    int ret = 0;

                    /*
                     *  The clear data is published (EV_RX_DATA): a
                     *  subscriber may end the connection, or its host
                     *  destroy the gobj (see alive_t)
                     */
                    alive_t marker;
                    alive_push(priv, &marker);
                    if(priv->use_ssl) {
                        GBUFFER_INCREF(gbuf)
                        ret = ytls_decrypt_data(priv->ytls, priv->sskt, gbuf);
                        if(!marker.alive) {
                            break;  // the gobj is gone
                        }
                        alive_pop(priv, &marker);
                        if(ret == -2222) {
                            /*
                             *  The TLS session was freed inside the callback:
                             *  a subscriber of EV_RX_DATA dropped or stopped
                             *  the connection, whose end is done or under way,
                             *  and whose host may have destroyed the gobj
                             *  already (a volatile clisrv): not touched
                             */
                            break;
                        }
                        if(ret < 0) {
                            /*
                             *  If return -1 while doing handshake then is good stop here the gobj,
                             *  But if return -1 in response of gobj_send_event,
                             *      then it can be already stopped and destroyed
                             *      Solution: don't return -1 on ytls_on_clear_data_callback
                             */
                            if(ret < -1000) { // Mark as TLS error
                                set_tls_disconnect_cause(gobj, "TLS: decrypt failed");
                                try_to_stop_yevents(gobj);
                                break;
                            }
                            // a subscriber's own error: the reading goes on
                        }

                    } else {
                        GBUFFER_INCREF(gbuf)
                        json_t *kw = json_object();
                        json_object_set_new(kw, "gbuffer", json_integer((json_int_t)(uintptr_t)gbuf));
                        ret = gobj_publish_event(gobj, EV_RX_DATA, kw);
                        if(!marker.alive) {
                            break;  // the gobj is gone
                        }
                        alive_pop(priv, &marker);
                    }

                    /*
                     *  Clear buffer, re-arm read. The gobj lives (the
                     *  marker): if try_to_stop_yevents() was called (mt_stop,
                     *  EV_DROP,...) this event is not idle, and is not
                     *  re-armed. A subscriber that answered an error does not
                     *  stop the reading: up to 7.25.21 it did (ret was asked
                     *  to be 0), and the connection hung in silence, neither
                     *  read nor stopped. A host that paused the reading
                     *  (rx_paused) gets it held until it resumes it.
                     */
                    if(yev_event_is_idle(yev_event)) {
                        gbuffer_clear(gbuf);
                        if(priv->rx_paused && gobj_in_this_state(gobj, ST_CONNECTED)) {
                            priv->rx_held = TRUE;
                        } else {
                            yev_start_event(yev_event);
                        }
                    }

                } else {
                    /*
                     *  Disconnected
                     */
                    set_disconnect_cause(gobj, "%s", strerror(-yev_get_result(yev_event)));

                    if(trace) {
                        gobj_log_debug(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                            "msg",          "%s", "TCP: read FAILED",
                            "msg2",         "%s", "🌐TCP: read FAILED",
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
                            "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                            "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                            "errno",        "%d", -yev_get_result(yev_event),
                            "strerror",     "%s", strerror(-yev_get_result(yev_event)),
                            "p",            "%p", yev_event,
                            "fd",           "%d", yev_get_fd(yev_event),
                            NULL
                        );
                    }

                    try_to_stop_yevents(gobj);
                }
            }
            break;

        case YEV_WRITE_TYPE:
            {
                priv->tx_in_progress--;

                if(yev_state == YEV_ST_IDLE) {
                    if(gobj_is_running(gobj)) {
                        /*
                         *  See if all data was transmitted
                         */
                        priv->txBytes += (json_int_t)yev_get_result(yev_event);

                        set_inactivity_timeout(gobj); // tx activity: reset timer

                        if(gbuffer_leftbytes(yev_get_gbuf(yev_event)) > 0) {
                            if(trace_level & TRACE_MACHINE) {
                                trace_machine("🔄🍄🍄mach(%s%s^%s), st: %s transmit PENDING data %ld",
                                    !gobj_is_running(gobj)?"!!":"",
                                    gobj_gclass_name(gobj), gobj_name(gobj),
                                    gobj_current_state(gobj),
                                    (long)gbuffer_leftbytes(yev_get_gbuf(yev_event))
                                );
                            }

                            start_write_event(gobj, yev_event); // on failure: logged, connection dropped
                            break;
                        }

                        release_the_write(gobj, yev_event);

                        gbuffer_t *gbuf_encrypted = dl_first(&priv->dl_tx_encrypted);
                        if(gbuf_encrypted && (gobj_in_this_state(gobj, ST_CONNECTED) ||
                                gobj_in_this_state(gobj, ST_WAIT_HANDSHAKE))) {
                            /*
                             *  TLS output waiting: it goes before the next
                             *  clear message, which would add more behind it
                             */
                            dl_delete(&priv->dl_tx_encrypted, gbuf_encrypted, 0);
                            write_encrypted_data(gobj, gbuf_encrypted); // on failure: logged, dropped
                        } else if(gobj_in_this_state(gobj, ST_CONNECTED)) {
                            // Avoid while doing handshaking
                            try_more_writes(gobj);
                        } else if(gobj_in_this_state(gobj, ST_WAIT_STOPPED)) {
                            try_to_stop_yevents(gobj);
                        }
                    } else {
                        release_the_write(gobj, yev_event);
                        try_to_stop_yevents(gobj);
                    }

                } else {
                    /*
                     *  Disconnected
                     */
                    set_disconnect_cause(gobj, "%s", strerror(-yev_get_result(yev_event)));

                    if(yev_get_result(yev_event) == -ETIMEDOUT &&
                            gobj_in_this_state(gobj, ST_WAIT_STOPPED)) {
                        // TCP_USER_TIMEOUT of bound_the_write_in_flight()
                        gobj_log_warning(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                            "msg",          "%s", "The peer did not take the data in flight in timeout_stop_tx, connection aborted",
                            "timeout_stop_tx", "%d", (int)gobj_read_integer_attr(gobj, "timeout_stop_tx"),
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
                            "peername",     "%s", gobj_read_str_attr(gobj, "peername"),
                            "sockname",     "%s", gobj_read_str_attr(gobj, "sockname"),
                            NULL
                        );
                    }

                    if(trace) {
                        gobj_log_debug(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                            "msg",          "%s", "TCP: write FAILED",
                            "msg2",         "%s", "🌐TCP: write FAILED",
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
                            "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                            "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                            "errno",        "%d", -yev_get_result(yev_event),
                            "strerror",     "%s", strerror(-yev_get_result(yev_event)),
                            "p",            "%p", yev_event,
                            "fd",           "%d", yev_get_fd(yev_event),
                            NULL
                        );
                    }

                    release_the_write(gobj, yev_event);
                    try_to_stop_yevents(gobj);
                }

            }
            break;

        case YEV_CONNECT_TYPE:
            {
                if(yev_state == YEV_ST_IDLE) {
                    set_connected(gobj, yev_get_fd(yev_event));
                } else {
                    /*
                     *  Error on connection
                     */
                    set_disconnect_cause(gobj, "%s", strerror(-yev_get_result(yev_event)));

                    if(trace) {
                        gobj_log_debug(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_LIBURING,
                            "msg",          "%s", "TCP: connect FAILED",
                            "msg2",         "%s", "🌐TCP: connect FAILED",
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
                            "errno",        "%d", -yev_get_result(yev_event),
                            "strerror",     "%s", strerror(-yev_get_result(yev_event)),
                            "p",            "%p", yev_event,
                            "fd",           "%d", yev_get_fd(yev_event),
                            NULL
                        );
                    }
                    try_to_stop_yevents(gobj);
                }
            }
            break;

        case YEV_ACCEPT_TYPE:
            {
                if(yev_state == YEV_ST_IDLE) {
                    if(priv->fd_clisrv > 0) {
                        gobj_log_debug(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                            "msg",          "%s", "TCP: fd_clisrv > 0",
                            "msg2",         "%s", "🌐TCP: fd_clisrv > 0",
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
                            "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                            "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                            "errno",        "%d", -yev_get_result(yev_event),
                            "strerror",     "%s", strerror(-yev_get_result(yev_event)),
                            "p",            "%p", yev_event,
                            "fd",           "%d", yev_get_fd(yev_event),
                            "fd_clisrv",    "%d", priv->fd_clisrv,
                            "fd_listen",    "%d", priv->fd_listen,
                            NULL
                        );
                        close(priv->fd_clisrv);
                        priv->fd_clisrv = -1;
                    }
                    priv->fd_clisrv = yev_get_result(yev_event);
                    set_connected(gobj, priv->fd_clisrv);
                } else {
                    /*
                     *  Error on connection
                     */
                    set_disconnect_cause(gobj, "%s", strerror(-yev_get_result(yev_event)));

                    if(trace) {
                        gobj_log_debug(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_LIBURING,
                            "msg",          "%s", "TCP: accept FAILED",
                            "msg2",         "%s", "🌐TCP: accept FAILED",
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
                            "errno",        "%d", -yev_get_result(yev_event),
                            "strerror",     "%s", strerror(-yev_get_result(yev_event)),
                            "p",            "%p", yev_event,
                            "fd",           "%d", yev_get_fd(yev_event),
                            NULL
                        );
                    }
                    try_to_stop_yevents(gobj);
                }
            }
            break;

        case YEV_POLL_TYPE:
            {
                /*
                 *  Disconnected
                 */
                // int result = yev_get_result(yev_event);
                { // if(result > 0)
                    set_disconnect_cause(gobj, "POLLHUP Peer shutdown");

                    if(trace) {
                        gobj_log_debug(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                            "msg",          "%s", "TCP: POLL POLLHUP",
                            "msg2",         "%s", "🌐TCP: POLL POLLHUP",
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
                            "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                            "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                            "errno",        "%d", -yev_get_result(yev_event),
                            "strerror",     "%s", strerror(-yev_get_result(yev_event)),
                            "p",            "%p", yev_event,
                            "fd",           "%d", yev_get_fd(yev_event),
                            NULL
                        );
                    }
                    try_to_stop_yevents(gobj);
                }
            }
            break;

        default:
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "TCP: event type NOT IMPLEMENTED",
                "msg2",         "%s", "🌐TCP: event type NOT IMPLEMENTED",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                "event_type",   "%s", yev_event_type_name(yev_event),
                "p",            "%p", yev_event,
                NULL
            );
            break;
    }

    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_connect(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(kw)

    gobj_write_str_attr(gobj, "disconnect_cause", "");

    /*
     *  An EV_CONNECT of the owner (a connection on demand) takes the place
     *  of the reconnection set_disconnected() had armed: left armed, its
     *  EV_TIMEOUT landed in ST_WAIT_CONNECTED ("Event NOT DEFINED") when
     *  the connect took longer than the delay. ac_tx_data_disconnected()
     *  already did this before its own EV_CONNECT.
     */
    if(priv->gobj_timer) {
        clear_timeout(priv->gobj_timer);
    }

    const char *url = gobj_read_str_attr(gobj, "url");
    if(yev_rearm_connect_event(
        priv->yev_connect,
        url,    // client_url
        NULL,   // local bind
        0,  // ai_family AF_UNSPEC
        0   // ai_flags AI_V4MAPPED | AI_ADDRCONFIG
    )<0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Cannot connect tcp gobj",
            NULL
        );
        set_disconnect_cause(gobj, "Cannot set up the connect");
        try_to_stop_yevents(gobj);
        return -1;
    }

    if(yev_get_flag(priv->yev_connect) & YEV_FLAG_USE_TLS) {
        gobj_write_bool_attr(gobj, "use_ssl", TRUE);
    } else {
        gobj_write_bool_attr(gobj, "use_ssl", FALSE);
    }
    if(priv->use_ssl) {
        if(!priv->ytls) {
            json_t *jn_crypto = gobj_read_json_attr(gobj, "crypto");
            /*
             *  A config-supplied ssl_server_name wins (cert pinning where the
             *  pinned cert's name differs from the dialed host); otherwise
             *  derive it from the url host (SNI + hostname verification).
             */
            if(empty_string(kw_get_str(gobj, jn_crypto, "ssl_server_name", "", 0))) {
                char _schema[40]; char _host[120]; char _port[40];
                if(parse_url(gobj, priv->url,
                    _schema, sizeof(_schema),
                    _host, sizeof(_host),
                    _port, sizeof(_port),
                    0, 0,
                    0, 0,
                    FALSE) == 0 && _host[0] != '\0'
                ) {
                    if(json_object_set_new(
                        jn_crypto, "ssl_server_name", json_string(_host)
                    )<0) {
                        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_INTERNAL,
                            "msg",          "%s", "Cannot set 'ssl_server_name', 'crypto' is not a dict",
                            "url",          "%s", priv->url?priv->url:"",
                            NULL
                        );
                    }
                }
            }
            priv->ytls = ytls_init(gobj, jn_crypto, FALSE);
            if(!priv->ytls) {
                // Error already logged (ytls refuses insecure clients by default)
                set_disconnect_cause(gobj, "TLS: the client configuration is refused");
                try_to_stop_yevents(gobj);
                return -1;
            }

            // TODO connection with certificate, review
            //  const char *cert_pem = gobj_read_str_attr(gobj, "cert_pem");
            //if(!empty_string(cert_pem)) {
            //    esp_transport_ssl_set_cert_data(priv->transport, cert_pem, (int)strlen(cert_pem));
            //}
            //if(gobj_read_bool_attr(gobj, "skip_cert_cn")) {
            //    esp_transport_ssl_skip_common_name_check(priv->transport);
        }
    }

    if(yev_start_event(priv->yev_connect)<0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Cannot connect tcp gobj",
            NULL
        );
        set_disconnect_cause(gobj, "Cannot start the connect");
        try_to_stop_yevents(gobj);
        return -1;
    }

    return 0;
}

/***************************************************************************
 *  Sending data, if using TLS will be encrypted, else sent as is
 ***************************************************************************/
PRIVATE int ac_tx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(!gbuf) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "gbuffer NULL",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }
    int checked = check_tx_data(gobj, gbuf);
    if(checked != 0) {
        if(checked > 0) {
            post_tx_ready(gobj);    // an empty write: its EV_TX_READY
        }
        KW_DECREF(kw)
        return checked < 0? -1 : 0;     // Error already logged
    }
    mark_secret_gbuffer(gobj, kw, gbuf);

    if(!priv->gbuf_txing) {
        priv->cur_tx_bytes += (json_int_t)gbuffer_leftbytes(gbuf);
        take_the_message_to_write(gobj, gbuffer_incref(gbuf));
        write_data(gobj);
    } else {
        enqueue_write(gobj, gbuffer_incref(gbuf));
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_send_encrypted_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

    if(priv->tx_in_progress > 0 || dl_size(&priv->dl_tx_encrypted) > 0) {
        dl_add(&priv->dl_tx_encrypted, gbuffer_incref(gbuf));
        KW_DECREF(kw)
        return 0;
    }

    /*
     *  0 even when the write cannot start: that is logged and the connection
     *  dropped already, and the gbuffer went with the kw. The gbuffer is
     *  ours whatever we answer (ytls.h): up to 7.25.21 the mbedTLS backend
     *  freed it again on a negative answer.
     */
    write_encrypted_data(gobj, gbuffer_incref(gbuf));

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The EV_TX_READY posted to itself by post_tx_ready(), a turn later:
 *  published if a write is still taken (takes_writes()) with nothing in
 *  flight nor queued. Otherwise a write sent since publishes it when it
 *  ends, or the connection is gone and the host was told
 *  (EV_DISCONNECTED). Declared in every state, because the state may
 *  change before it comes. Only C_TCP posts it: EV_TX_READY is an output
 *  event, no host sends it.
 ***************************************************************************/
PRIVATE int ac_tx_ready_posted(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src != gobj) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "EV_TX_READY is an output event, it is not sent to a C_TCP",
            "src",          "%s", gobj_short_name(src),
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    priv->tx_ready_posted = FALSE;
    if(takes_writes(gobj) && !priv->no_tx_ready_event && !tx_ready_is_coming(gobj)) {
        return gobj_publish_event(gobj, EV_TX_READY, kw);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    set_disconnect_cause(gobj, "Local dropping");
    try_to_stop_yevents(gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Inactivity timeout expired while connected: close the connection.
 *  The client reconnects on demand when new tx data arrives
 *  (see ac_tx_data_disconnected).
 ***************************************************************************/
PRIVATE int ac_timeout_inactivity(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  This is a deliberate idle-close (no pending work). Mark it so
     *  set_disconnected() stays disconnected and waits for the next on-demand
     *  reconnection, instead of scheduling a timeout_between_connections retry.
     */
    priv->idle_closed = TRUE;
    set_disconnect_cause(gobj, "Inactivity timeout");
    try_to_stop_yevents(gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The TLS handshake did not end in timeout_tls_handshake: the peer said
 *  nothing, or too little. The connection is dropped; a client reconnects
 *  as after any failed connection.
 ***************************************************************************/
PRIVATE int ac_timeout_tls_handshake(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    /*
     *  One warning per PEER_LOG_INTERVAL_MS, saying how many were not
     *  written before it. Up to 7.30.17 each mute connection wrote one: a
     *  peer that kept opening them wrote the log at its will.
     */
    if(handshake_log_t_next && !test_msectimer(handshake_log_t_next)) {
        handshake_log_suppressed++;
    } else {
        /*
         *  The count of the window just closed goes in this warning; that
         *  of an older one in its own (up to 7.30.18 it went in this one
         *  however old it was)
         */
        json_int_t suppressed = 0;
        if(test_msectimer(handshake_log_t_next + PEER_LOG_INTERVAL_MS)) {
            say_suppressed_handshakes(gobj);
        } else {
            suppressed = handshake_log_suppressed;
            handshake_log_suppressed = 0;
        }
        handshake_log_t_next = start_msectimer(PEER_LOG_INTERVAL_MS);
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "TLS handshake not ended in time, connection dropped",
            "timeout_tls_handshake", "%d", (int)gobj_read_integer_attr(gobj, "timeout_tls_handshake"),
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            "peername",     "%s", gobj_read_str_attr(gobj, "peername"),
            "sockname",     "%s", gobj_read_str_attr(gobj, "sockname"),
            "suppressed",   "%ld", (long)suppressed,
            NULL
        );
    }

    set_disconnect_cause(gobj, "TLS: the handshake did not end in time");
    try_to_stop_yevents(gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Tx data while disconnected (inactivity model).
 *  Queue the data and trigger an on-demand reconnection; the queue is
 *  flushed by start_pending_writes() once connected.
 ***************************************************************************/
PRIVATE int ac_tx_data_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(!gbuf) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "gbuffer NULL",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }
    int checked = check_tx_data(gobj, gbuf);
    if(checked != 0) {
        if(checked > 0 && takes_writes(gobj)) {
            post_tx_ready(gobj);    // an empty write: its EV_TX_READY, and no connect
        }
        KW_DECREF(kw)
        return checked < 0? -1 : 0;     // Error already logged
    }
    mark_secret_gbuffer(gobj, kw, gbuf);

    if(priv->timeout_inactivity > 0) {
        enqueue_write(gobj, gbuffer_incref(gbuf));
        clear_timeout(priv->gobj_timer);
        gobj_send_event(gobj, EV_CONNECT, 0, gobj);
    } else {
        /*
         *  No inactivity model: there is no on-demand reconnection, the data
         *  cannot be sent while disconnected. Drop it (logged, not silent).
         */
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "tcp tx data while disconnected, dropped",
            NULL
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Tx data while the connection is going down (ST_WAIT_STOPPED: a drop or
 *  a disconnection is waiting for its last io_uring operation). The layers
 *  above learn it only with EV_DISCONNECTED, so until then they send, and
 *  cannot know better: the event is theirs to send, and this action is not
 *  a swallowed "Event NOT DEFINED". The data goes where set_disconnected()
 *  puts the pending queue of a dead connection: away. A protocol that must
 *  not lose it resends on its own acks (c_qiogate does). What went is
 *  counted, and said ONCE when the close ends (log_closing_drops()): up to
 *  7.25.20 it went with no trace at the default levels.
 ***************************************************************************/
PRIVATE int ac_tx_data_closing(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    priv->closing_dropped_msgs++;
    priv->closing_dropped_bytes += (json_int_t)(gbuf?gbuffer_leftbytes(gbuf):0);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Tx data while a (re)connection is in progress (inactivity model).
 *  Queue it; start_pending_writes() flushes it once connected.
 ***************************************************************************/
PRIVATE int ac_tx_data_queued(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(!gbuf) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "gbuffer NULL",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }
    int checked = check_tx_data(gobj, gbuf);
    if(checked != 0) {
        if(checked > 0 && takes_writes(gobj)) {
            post_tx_ready(gobj);    // an empty write: its EV_TX_READY
        }
        KW_DECREF(kw)
        return checked < 0? -1 : 0;     // Error already logged
    }
    mark_secret_gbuffer(gobj, kw, gbuf);

    if(priv->timeout_inactivity > 0) {
        enqueue_write(gobj, gbuffer_incref(gbuf));
    } else {
        /*
         *  No inactivity model: not expected here, drop it (logged, not silent).
         */
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "tcp tx data while connecting, dropped",
            NULL
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Called when a trace level is enabled at runtime (e.g. via agent command).
 *  Propagate the TLS trace flag to the active secure filter if present.
 ***************************************************************************/
PRIVATE int mt_trace_on(hgobj gobj, const char *level, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->ytls && priv->sskt) {
        uint32_t trace_level = gobj_trace_level(gobj);
        ytls_set_trace(priv->ytls, priv->sskt, (trace_level & TRACE_TLS) ? TRUE : FALSE);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Called when a trace level is disabled at runtime.
 ***************************************************************************/
PRIVATE int mt_trace_off(hgobj gobj, const char *level, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->ytls && priv->sskt) {
        uint32_t trace_level = gobj_trace_level(gobj);
        ytls_set_trace(priv->ytls, priv->sskt, (trace_level & TRACE_TLS) ? TRUE : FALSE);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create      = mt_create,
    .mt_writing     = mt_writing,
    .mt_destroy     = mt_destroy,
    .mt_start       = mt_start,
    .mt_stop        = mt_stop,
    .mt_reading     = mt_reading,
    .mt_trace_on    = mt_trace_on,
    .mt_trace_off   = mt_trace_off,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TCP);

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
    ev_action_t st_disconnected[] = {
        {EV_CONNECT,                ac_connect,                 ST_WAIT_CONNECTED},
        {EV_TIMEOUT,                ac_connect,                 ST_WAIT_CONNECTED},
        {EV_TX_DATA,                ac_tx_data_disconnected,    0},
        {EV_TX_READY,               ac_tx_ready_posted,         0},
        {0,0,0}
    };

    ev_action_t st_stopped[] = {
        {EV_TX_READY,               ac_tx_ready_posted,         0},
        {0,0,0}
    };

    ev_action_t st_wait_stopped[] = {
        {EV_TX_DATA,                ac_tx_data_closing,         0},
        {EV_DROP,                   ac_drop,                    0}, // someone insists
        {EV_TX_READY,               ac_tx_ready_posted,         0},
        {0,0,0}
    };

    ev_action_t st_wait_connected[] = {
        {EV_TX_DATA,                ac_tx_data_queued,          0},
        {EV_DROP,                   ac_drop,                    0},
        {EV_TX_READY,               ac_tx_ready_posted,         0},
        {0,0,0}
    };

    ev_action_t st_wait_handshake[] = {
        {EV_SEND_ENCRYPTED_DATA,    ac_send_encrypted_data,     0},
        {EV_TX_DATA,                ac_tx_data_queued,          0},
        {EV_TIMEOUT,                ac_timeout_tls_handshake,   0},
        {EV_DROP,                   ac_drop,                    0},
        {EV_TX_READY,               ac_tx_ready_posted,         0},
        {0,0,0}
    };

    ev_action_t st_connected[] = {
        {EV_TX_DATA,                ac_tx_data,                 0},
        {EV_SEND_ENCRYPTED_DATA,    ac_send_encrypted_data,     0},
        {EV_TIMEOUT,                ac_timeout_inactivity,      0},
        {EV_DROP,                   ac_drop,                    0},
        {EV_TX_READY,               ac_tx_ready_posted,         0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_DISCONNECTED,       st_disconnected},
        {ST_STOPPED,            st_stopped},
        {ST_WAIT_STOPPED,       st_wait_stopped},
        {ST_WAIT_CONNECTED,     st_wait_connected},

        /* Order is important. Below are the connected states */
        {ST_WAIT_HANDSHAKE,     st_wait_handshake},
        {ST_CONNECTED,          st_connected},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_RX_DATA,            EVF_OUTPUT_EVENT},
        {EV_TX_DATA,            0},
        {EV_SEND_ENCRYPTED_DATA,0},
        {EV_TX_READY,           EVF_OUTPUT_EVENT},
        {EV_CONNECT,            0},
        {EV_CONNECTED,          EVF_OUTPUT_EVENT},
        {EV_DISCONNECTED,       EVF_OUTPUT_EVENT},
        {EV_STOPPED,            EVF_OUTPUT_EVENT},
        {EV_TIMEOUT,            0},
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
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,
        gcflag_manual_start // gclass_flag
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
PUBLIC int register_c_tcp(void)
{
    return create_gclass(C_TCP);
}
