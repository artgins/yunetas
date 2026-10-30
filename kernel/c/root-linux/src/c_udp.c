/***********************************************************************
 *          C_UDP.C
 *          Udp GClass: a UDP client. Its socket is connected to its url:
 *          it sends there, and reads what comes from there.
 *
 *          Not registered by the SDK: a yuno that uses it includes c_udp.h
 *          and calls register_c_udp().
 *
 *          States: ST_STOPPED, ST_IDLE (started: it reads, and sends),
 *          ST_WAIT_STOPPED (a stop waiting for its read to be canceled and
 *          its send in flight to complete). EV_TX_DATA in ST_IDLE; output
 *          events EV_RX_DATA, EV_TX_READY and EV_STOPPED, to its parent (or
 *          its subscriber).
 *
 *          One datagram is sent at a time, the others wait in a queue, and
 *          EV_TX_READY is published when the queue is done. A datagram that
 *          cannot be sent is dropped and the next one goes, as in C_UDP_S.
 *          The stop contract is C_TCP's: every stop of a running C_UDP ends
 *          in ST_STOPPED and publishes EV_STOPPED once, the last thing it
 *          does with itself; the datagrams waiting in the queue go with the
 *          stop.
 *
 *          Up to 7.29.3 it was a port never finished: register_c_udp()
 *          failed (EV_STOPPED declared as an input that no state takes),
 *          c_udp.h declared another name, the completion of its connect was
 *          "event type NOT IMPLEMENTED", only the first datagram was sent,
 *          a stop published nothing and did not wait for the send in flight,
 *          and its read event leaked.
 *
 *          Copyright (c) 2020 Niyamaka.
 *          Copyright (c) 2025-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <errno.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include <yev_loop.h>
#include <ytls.h>
#include "c_yuno.h"
#include "c_udp.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int yev_callback(yev_event_h yev_event);
PRIVATE void try_to_stop_yevents(hgobj gobj); // IDEMPOTENT

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name------------flag--------default-description---------- */
SDATA (DTP_STRING,      "url",          SDF_RD,     "",     "Url to connect"),
SDATA (DTP_INTEGER,     "connxs",       SDF_STATS,  0,      "Current connections"),
SDATA (DTP_INTEGER,     "rx_buffer_size", SDF_WR|SDF_PERSIST, "4096", "Rx buffer size"),

SDATA (DTP_INTEGER,     "txBytes",      SDF_RSTATS,     "0", "Bytes transmitted"),
SDATA (DTP_INTEGER,     "rxBytes",      SDF_RSTATS,     "0", "Bytes received"),
SDATA (DTP_INTEGER,     "txMsgs",       SDF_RSTATS,     "0", "Datagrams transmitted"),
SDATA (DTP_INTEGER,     "rxMsgs",       SDF_RSTATS,     "0", "Datagrams received"),
SDATA (DTP_STRING,      "peername",     SDF_VOLATIL|SDF_STATS, "",  "Peername"),
SDATA (DTP_STRING,      "sockname",     SDF_VOLATIL|SDF_STATS, "",  "Sockname"),

SDATA (DTP_POINTER,     "user_data",    0,          0,      "user data"),
SDATA (DTP_POINTER,     "user_data2",   0,          0,      "more user data"),
SDATA (DTP_POINTER,     "subscriber",   0,          0,      "subscriber of output-events. If it's null then subscriber is the parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_CONNECT_DISCONNECT    = 0x0001,
    TRACE_TRAFFIC               = 0x0002,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"connections",         "Trace connections and disconnections"},
{"traffic",             "Trace dump traffic"},
{0, 0},
};


/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
/*
 *  A publish of an event hands control to the subscribers, which may stop
 *  the gobj or, through its host, destroy it: the call that published keeps
 *  a marker on its stack, the markers are chained, and mt_destroy() clears
 *  them all. Read after the publish: FALSE, the gobj is gone and nothing of
 *  it may be touched (the same as C_TCP).
 */
typedef struct alive_s {
    BOOL alive;
    struct alive_s *prev;
} alive_t;

typedef struct _PRIVATE_DATA {
    // Data oid
    json_int_t connxs;
    json_int_t txBytes;
    json_int_t rxBytes;
    json_int_t txMsgs;
    json_int_t rxMsgs;

    yev_event_h yev_connect; // Used if not __clisrv__ (pure tcp client)
    yev_event_h yev_reading;
    hytls ytls;
    hsskt sskt;
    BOOL use_ssl;

    dl_list_t dl_tx;
    gbuffer_t *gbuf_txing;

    BOOL no_tx_ready_event;
    BOOL tx_ready_posted;           // an EV_TX_READY posted to itself (post_tx_ready())
    int tx_in_progress;

    alive_t *alive;                 // the markers of the publishes in progress (see alive_t)
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

    dl_init(&priv->dl_tx, gobj);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  A reset of the stats writes the counters, which live in priv
     */
    if(path && strcmp(path, "txBytes")==0) {
        priv->txBytes = 0;
    } else if(path && strcmp(path, "rxBytes")==0) {
        priv->rxBytes = 0;
    } else if(path && strcmp(path, "txMsgs")==0) {
        priv->txMsgs = 0;
    } else if(path && strcmp(path, "rxMsgs")==0) {
        priv->rxMsgs = 0;
    }
}

/***************************************************************************
 *      Framework Method reading: the counters live in priv. Up to 7.29.3
 *      there was none, and txBytes and rxBytes read 0 for ever
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
    }
    return v;
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_in_this_state(gobj, ST_STOPPED) || priv->yev_connect) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Initial wrong udp state: a start before the stop ended",
            "state",        "%s", gobj_current_state(gobj),
            NULL
        );
        return -1;
    }

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
            "msg",          "%s", "Cannot create the udp socket",
            "url",          "%s", url,
            NULL
        );
        return -1;
    }
    if(yev_start_event(priv->yev_connect) < 0) {
        // Error already logged
        EXEC_AND_RESET(yev_destroy_event, priv->yev_connect)
        return -1;
    }

    /*
     *  Info of "connecting..."
     */
    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "UDP Connecting...",
            "url",          "%s", url,
            NULL
        );
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
            yev_get_fd(priv->yev_connect),
            gbuffer_create(rx_buffer_size, rx_buffer_size)
        );
    }

    if(priv->yev_reading) {
        yev_set_fd(priv->yev_reading, yev_get_fd(priv->yev_connect));

        if(!yev_get_gbuf(priv->yev_reading)) {
            json_int_t rx_buffer_size = gobj_read_integer_attr(gobj, "rx_buffer_size");
            yev_set_gbuffer(priv->yev_reading, gbuffer_create(rx_buffer_size, rx_buffer_size));
        } else {
            gbuffer_clear(yev_get_gbuf(priv->yev_reading));
        }
    }
    gobj_change_state(gobj, ST_IDLE);

    if(!priv->yev_reading || yev_start_event(priv->yev_reading) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot start the read of the udp socket: stopped",
            "url",          "%s", url,
            NULL
        );
        try_to_stop_yevents(gobj);
        return -1;
    }

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    gobj_reset_volatil_attrs(gobj);

    try_to_stop_yevents(gobj);  // EV_STOPPED may come inside: the last thing

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

    if(!gobj_in_this_state(gobj, ST_STOPPED)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "GObj NOT STOPPED. UV handler ACTIVE!",
            NULL
        );
    }

    EXEC_AND_RESET(yev_destroy_event, priv->yev_connect)
    EXEC_AND_RESET(yev_destroy_event, priv->yev_reading)

    GBUFFER_DECREF(priv->gbuf_txing);
    dl_flush(&priv->dl_tx, (fnfree)gbuffer_decref);
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  Stop all events, is someone is running go to WAIT_STOPPED else STOPPED
 *  IMPORTANT this is the only place to set ST_WAIT_STOPPED state
 ***************************************************************************/
PRIVATE void try_to_stop_yevents(hgobj gobj)  // IDEMPOTENT
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    BOOL to_wait_stopped = FALSE;

    if(gobj_current_state(gobj)==ST_STOPPED) {
        return;
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

    if(priv->yev_connect) {
        yev_stop_event(priv->yev_connect);
        if(yev_event_is_stopped(priv->yev_connect)) {
            yev_destroy_event(priv->yev_connect);
            priv->yev_connect = 0;
        } else {
            to_wait_stopped = TRUE;
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

    if(priv->tx_in_progress > 0) {
        to_wait_stopped = TRUE;     // its completion comes to this gobj
    }

    /*
     *  What waits to be sent goes with the stop. The datagram in flight
     *  is held by its send event, which completes on its own.
     */
    size_t queued = dl_size(&priv->dl_tx);
    if(queued > 0) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "UDP client stopped: the datagrams waiting to be sent are dropped",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            "dropped",      "%d", (int)queued,
            NULL
        );
    }
    GBUFFER_DECREF(priv->gbuf_txing)
    dl_flush(&priv->dl_tx, (fnfree)gbuffer_decref);

    if(to_wait_stopped) {
        gobj_change_state(gobj, ST_WAIT_STOPPED);
    } else {
        gobj_change_state(gobj, ST_STOPPED);
        gobj_publish_event(gobj, EV_STOPPED, 0);    // the last thing: the host may destroy it
    }
}

/***************************************************************************
 *  What an EV_TX_DATA must not carry. 0: send it. 1: an empty gbuffer,
 *  nothing to send: ignored, with a warning (up to 7.29.3 it was a send
 *  that cannot start: an error, its event lost, and every later datagram
 *  held in the queue for ever); its EV_TX_READY comes from post_tx_ready().
 *  -1: a gbuffer already queued or in flight here, whose bytes the first
 *  send consumes: refused, with an error (up to 7.29.3 it was queued again,
 *  which failed and lost a reference). A gbuffer in flight in ANOTHER
 *  transport is not seen: a gbuffer handed to a transport is its own until
 *  its EV_TX_READY.
 ***************************************************************************/
PRIVATE int check_tx_data(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gbuffer_leftbytes(gbuf) == 0) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "udp tx data empty, ignored",
            NULL
        );
        return 1;
    }
    if(gbuf->__dl__ || gbuf == priv->gbuf_txing) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "udp tx data already queued or in flight, refused",
            "len",          "%d", (int)gbuffer_leftbytes(gbuf),
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Enqueue data
 ***************************************************************************/
PRIVATE int enqueue_write(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    //static int counter = 0;
    //size_t size = dl_size(&priv->dl_tx);
    // if(priv->max_tx_queue && size >= priv->max_tx_queue) {
    //     if((counter % priv->max_tx_queue)==0) {
    //         log_error(0,
    //             "function",     "%s", __FUNCTION__,
    //             "msgset",       "%s", MSGSET_INTERNAL,
    //             "msg",          "%s", "Tiro mensaje tx",
    //             "counter",      "%d", (int)counter,
    //             NULL
    //         );
    //     }
    //     counter++;
    //     GBUFFER *gbuf_first = dl_first(&priv->dl_tx);
    //     gobj_incr_qs(QS_DROP_BY_OVERFLOW, 1);
    //     dl_delete(&priv->dl_tx, gbuf_first, 0);
    //     gobj_decr_qs(QS_OUPUT_QUEUE, 1);
    //     gbuf_decref(gbuf_first);
    // }

    dl_add(&priv->dl_tx, gbuf);

    return 0;
}

/***************************************************************************
 *  Write the current gbuffer
 ***************************************************************************/
PRIVATE int write_data(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = priv->gbuf_txing;

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
        if(ytls_encrypt_data(priv->ytls, priv->sskt, gbuf)<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "ytls_encrypt_data() FAILED",
                "error",        "%s", ytls_get_last_error(priv->ytls, priv->sskt),
                NULL
            );
            try_to_stop_yevents(gobj);
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
        priv->txMsgs++;

        /*
         *  Transmit
         */
        int fd =yev_get_fd(priv->yev_connect);
        yev_event_h yev_write_event = yev_create_write_event(
            yuno_event_loop(),
            yev_callback,
            gobj,
            fd,
            gbuffer_incref(gbuf)
        );
        if(!yev_write_event) {
            GBUFFER_DECREF(gbuf)    // Error already logged: the reference given to the event
            return -1;
        }

        /*
         *  A send that does not start will not complete: the callback never
         *  frees the event nor counts it done. Up to 7.29.3 the event and
         *  its gbuffer leaked, tx_in_progress never went back to 0, and
         *  gbuf_txing held every later datagram in the queue (as C_UDP_S
         *  did up to 7.25.4).
         */
        if(yev_start_event(yev_write_event) < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot send datagram: dropped",
                "len",          "%d", (int)gbuffer_leftbytes(gbuf),
                NULL
            );
            yev_destroy_event(yev_write_event);
            return -1;
        }
        priv->tx_in_progress++;
    }
    return 0;
}

/***************************************************************************
 *  The EV_TX_READY of a send from inside the sender's gobj_send_event(): an
 *  empty one, which sends nothing, or one that did not start (dropped).
 *  With a datagram in flight or queued, the EV_TX_READY of that one stands
 *  for it. It is posted to itself (ac_tx_ready_posted()), never published
 *  here: a host that sends its next datagram in its EV_TX_READY would
 *  recurse, and with every send failing, for as long as it sends. In the
 *  fixes after 7.29.3 a send that did not start published it here, and an
 *  empty one got none: a host that advances only on EV_TX_READY waited for
 *  ever.
 ***************************************************************************/
PRIVATE void post_tx_ready(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->no_tx_ready_event || priv->tx_ready_posted) {
        return;
    }
    if(priv->gbuf_txing || priv->tx_in_progress > 0 || dl_size(&priv->dl_tx) > 0) {
        return;     // the EV_TX_READY of what is in flight comes when it ends
    }
    if(gobj_post_event(gobj, EV_TX_READY, json_object(), gobj) < 0) {
        return;     // Error already logged
    }
    priv->tx_ready_posted = TRUE;
}

/***************************************************************************
 *  Try more writes, at the completion of a send
 ***************************************************************************/
PRIVATE void try_more_writes(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Clear the current tx msg
     */
    GBUFFER_DECREF(priv->gbuf_txing)

    /*
     *  Get the next tx msg: a datagram that cannot be sent is dropped
     *  (logged by write_data) and the next one is tried
     */
    gbuffer_t *gbuf_txing;
    while((gbuf_txing = dl_first(&priv->dl_tx)) != NULL) {
        priv->gbuf_txing = gbuf_txing;
        dl_delete(&priv->dl_tx, gbuf_txing, 0);
        if(write_data(gobj) == 0) {
            return;
        }
        GBUFFER_DECREF(priv->gbuf_txing)    // Error already logged
    }

    /*
     *  If no more publish tx ready
     */
    if(!priv->no_tx_ready_event) {
        json_t *kw_tx_ready = json_object();
        gobj_publish_event(gobj, EV_TX_READY, kw_tx_ready);
    }
}

/***************************************************************************
 *  on read callback
 ***************************************************************************/
#ifdef PEPE
PRIVATE void on_read_cb(
    uv_udp_t* handle,
    ssize_t nread,
    const uv_buf_t* buf,
    const struct sockaddr* addr,
    unsigned flags)
{
    hgobj gobj = handle->data;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_UV) {
        log_debug_printf(0, "<<< on_read_cb(%s:%s)",
            gobj_gclass_name(gobj), gobj_name(gobj)
        );
    }

    if(nread < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING,
            "msg",          "%s", "read FAILED",
            "uv_error",     "%s", uv_err_name(nread),
            NULL
        );
        return;
    }

    if(nread == 0) {
        // Yes, sometimes arrive with nread 0.
        return;
    }
    *priv->prxBytes += nread;

    ip_port ipp;
    ipp.is_ip6 = priv->ipp_sockname.is_ip6;
    memcpy(&ipp.sa.ip, addr, sizeof(ipp.sa.ip));
    char peername[60];
    get_ipp_url(&ipp, peername, sizeof(peername)); // TODO mucho proceso en cada rx

    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        char temp[256];
        snprintf(temp, sizeof(temp), "%s: %s %s %s",
            gobj_full_name(gobj),
            priv->sockname,
            "<-",
            peername
        );

        gobj_trace_dump(gobj,
            0,
            buf->base,
            nread,
            "%s", temp
        );
    }

    if(!empty_string(priv->rx_data_event_name)) {
        gbuffer_t *gbuf = gbuffer_create(nread, nread, 0,0);
        if(!gbuf) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY,
                "msg",          "%s", "no memory for gbuf",
                "size",         "%d", nread,
                NULL);
            return;
        }
        gbuffer_append(gbuf, buf->base, nread);
        gbuf_setlabel(gbuf, peername);
        json_t *kw = json_pack("{s:I}",
            "gbuffer", (json_int_t)(uintptr_t)gbuf
        );
        gobj_publish_event(gobj, priv->rx_data_event_name, kw);
    }
}

/***************************************************************************
 *  on write callback
 ***************************************************************************/
PRIVATE void on_upd_send_cb(uv_udp_send_t* req, int status)
{
    hgobj gobj = req->data;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_UV) {
        log_debug_printf(0, "<<< on_upd_send_cb(%s:%s)",
            gobj_gclass_name(gobj), gobj_name(gobj)
        );
    }

    if (status != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING,
            "msg",          "%s", "upd send FAILED",
            "uv_error",     "%s", uv_err_name(status),
            NULL
        );
        gobj_change_state(gobj, ST_CLOSED);
        return;
    }

    size_t ln = gbuffer_chunk(priv->gbuf_txing);
    if(ln) {
        send_data(gobj, 0);  // continue with gbuf_txing
        return;

    } else {
        // Remove curr txing and get the next
        gbuffer_decref(priv->gbuf_txing);
        priv->gbuf_txing = 0;

        gbuffer_t *gbuf = dl_first(&priv->dl_tx);
        if(gbuf) {
            dl_delete(&priv->dl_tx, gbuf, 0);
            send_data(gobj, gbuf);
            return;
        }
    }

    if(!empty_string(priv->tx_ready_event_name)) {
        gobj_publish_event(gobj, priv->tx_ready_event_name, 0);
    }
}

/***************************************************************************
 *  Send data
 ***************************************************************************/
PRIVATE int send_data(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gbuf) {
        if(priv->gbuf_txing) {
            dl_add(&priv->dl_tx, gbuf);
            return 0;
        } else {
            priv->gbuf_txing = gbuf;
        }
    } else {
        gbuf = priv->gbuf_txing;
    }
    size_t ln = gbuffer_chunk(gbuf);
    if(ln > 1500) {
        // TODO aunque ponga un data_size de 1500 luego crece
        //gobj_log_error(gobj, 0,
        //    "function",     "%s", __FUNCTION__,
        //    "msgset",       "%s", MSGSET_INTERNAL,
        //    "msg",          "%s", "UPD lenght must not be greater than 1500",
        //    "ln",           "%d", ln,
        //    "rHost",        "%s", priv->rHost,
        //    "rPort",        "%s", priv->rPort,
        //    NULL
        //);
        ln = 1500;
    }
    char *bf = gbuffer_get(gbuf, ln);

    uv_buf_t b[] = {
        {.base = bf, .len = ln}
    };

    priv->req_send.data = gobj;

    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        char temp[256];

        snprintf(temp, sizeof(temp), "%s: %s %s %s:%s",
            gobj_full_name(gobj),
            priv->sockname,
            "->",
            priv->rHost,
            priv->rPort
        );

        gobj_trace_dump(gobj,
            0,
            b[0].base,
            b[0].len,
            "%s", temp
        );
    }

    *priv->ptxBytes += ln;

    if(gobj_trace_level(gobj) & TRACE_UV) {
        log_debug_printf(0, ">>>upd_send(%s:%s)",
            gobj_gclass_name(gobj), gobj_name(gobj)
        );
    }
    int ret = uv_udp_send(
        &priv->req_send,
        &priv->uv_udp,
        b,
        1,
        NULL, // For connected UDP handles, addr must be set to NULL, otherwise it will return UV_EISCONN error
        on_upd_send_cb
    );
    if(ret < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING,
            "msg",          "%s", "uv_udp_send() FAILED",
            "uv_error",     "%s", uv_err_name(ret),
            "rHost",        "%s", priv->rHost,
            "rPort",        "%s", priv->rPort,
            NULL
        );
    }

    return 0;
}
#endif

/***************************************************************************
 *  The addresses of the connected socket, as C_TCP says them
 ***************************************************************************/
PRIVATE void get_peer_and_sock_name(hgobj gobj, int fd)
{
    char temp[60];

    get_peername(temp, sizeof(temp), fd);
    gobj_write_str_attr(gobj, "peername", temp);

    get_sockname(temp, sizeof(temp), fd);
    gobj_write_str_attr(gobj, "sockname", temp);
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
 *  The completions of the connect, the read and the sends
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
            "msg2",         "%s", "UDP 🌐🌐💥 yev callback",
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
                if(yev_state == YEV_ST_IDLE) {
                    /*
                     *  yev_get_gbuf(yev_event) can be null if yev_stop_event() was called
                     */
                    if(trace_level & TRACE_TRAFFIC) {
                        gobj_trace_dump_gbuf(gobj, yev_get_gbuf(yev_event), "%s: %s%s%s",
                            gobj_short_name(gobj),
                            gobj_read_str_attr(gobj, "sockname"),
                            " ⏪ ",
                            gobj_read_str_attr(gobj, "peername")
                        );
                    }

                    priv->rxMsgs++;
                    priv->rxBytes += (json_int_t)gbuffer_leftbytes(yev_get_gbuf(yev_event));

                    int ret = 0;

                    /*
                     *  The clear data is published (EV_RX_DATA): a
                     *  subscriber may stop the gobj, or its host destroy it
                     *  (see alive_t)
                     */
                    alive_t marker;
                    alive_push(priv, &marker);
                    if(priv->use_ssl) {
                        GBUFFER_INCREF(yev_get_gbuf(yev_event))
                        ret = ytls_decrypt_data(priv->ytls, priv->sskt, yev_get_gbuf(yev_event));
                        if(!marker.alive) {
                            break;  // the gobj is gone
                        }
                        alive_pop(priv, &marker);
                        if(ret < -1000) { // Mark as TLS error
                            try_to_stop_yevents(gobj);
                            break;
                        }
                        // a subscriber's own error: the reading goes on

                    } else {
                        GBUFFER_INCREF(yev_get_gbuf(yev_event))
                        json_t *kw = json_pack("{s:I}",
                            "gbuffer", (json_int_t)(uintptr_t)yev_get_gbuf(yev_event)
                        );
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
                     *  stop the reading: up to 7.25.22 it did (ret was asked
                     *  to be 0), and the reading ended in silence.
                     */
                    if(yev_event_is_idle(yev_event)) {
                        gbuffer_clear(yev_get_gbuf(yev_event));
                        if(yev_start_event(yev_event) < 0) {
                            gobj_log_error(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_SYSTEM,
                                "msg",          "%s", "Cannot start the read of the udp socket again: stopped",
                                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                                NULL
                            );
                            try_to_stop_yevents(gobj);  // EV_STOPPED may come inside: the last thing
                        }
                    }

                } else {
                    /*
                     *  The read ends: canceled by a stop, or the socket
                     *  failed. A connected udp socket fails its read with
                     *  ECONNREFUSED when nothing listens at the url, and a
                     *  read of an empty datagram is taken as the end
                     *  (EPIPE): either one stops the client, with an
                     *  error, and EV_STOPPED tells its host. Up to 7.29.3
                     *  only a trace said so.
                     */
                    gobj_log_set_last_message("%s", strerror(-yev_get_result(yev_event)));

                    if(gobj_is_running(gobj) && yev_get_result(yev_event) != -ECANCELED) {
                        gobj_log_error(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                            "msg",          "%s", "UDP: read FAILED, the client stops",
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
                            "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                            "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                            "errno",        "%d", -yev_get_result(yev_event),
                            "strerror",     "%s", strerror(-yev_get_result(yev_event)),
                            NULL
                        );
                    } else if(trace) {
                        gobj_log_debug(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                            "msg",          "%s", "UDP: read stopped",
                            "msg2",         "%s", "🌐UDP: read stopped",
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

        case YEV_CONNECT_TYPE:
            {
                if(yev_state == YEV_ST_IDLE) {
                    /*
                     *  The socket has its peer. Up to 7.29.3 this
                     *  completion was "event type NOT IMPLEMENTED"
                     */
                    get_peer_and_sock_name(gobj, yev_get_fd(yev_event));
                    if(trace_level & TRACE_CONNECT_DISCONNECT) {
                        gobj_log_info(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                            "msg",          "%s", "UDP connected",
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
                            "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                            "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                            NULL
                        );
                    }
                    break;
                }

                if(gobj_is_running(gobj) && yev_get_result(yev_event) != -ECANCELED) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                        "msg",          "%s", "UDP: connect FAILED, the client stops",
                        "url",          "%s", gobj_read_str_attr(gobj, "url"),
                        "errno",        "%d", -yev_get_result(yev_event),
                        "strerror",     "%s", strerror(-yev_get_result(yev_event)),
                        NULL
                    );
                }
                try_to_stop_yevents(gobj);
            }
            break;

        case YEV_WRITE_TYPE:
            {
                priv->tx_in_progress--;

                /*
                 *  The completion of the datagram being sent, not of one
                 *  sent before a stop (its gbuffer was dropped by the stop)
                 */
                BOOL is_txing = priv->gbuf_txing &&
                    yev_get_gbuf(yev_event) == priv->gbuf_txing;

                if(!gobj_is_running(gobj)) {
                    /*
                     *  A stop waits for its sends: destroy the write event
                     */
                    yev_destroy_event(yev_event);
                    try_to_stop_yevents(gobj);  // EV_STOPPED may come inside: the last thing
                    break;
                }

                if(yev_state == YEV_ST_IDLE) {
                    /*
                     *  See if all data was transmitted
                     */
                    priv->txBytes += (json_int_t)yev_get_result(yev_event);
                    if(gbuffer_leftbytes(yev_get_gbuf(yev_event)) > 0) {
                        if(trace_level & TRACE_MACHINE) {
                            trace_machine("🔄🍄🍄mach(%s%s^%s), st: %s transmit PENDING data %ld",
                                !gobj_is_running(gobj)?"!!":"",
                                gobj_gclass_name(gobj), gobj_name(gobj),
                                gobj_current_state(gobj),
                                (long)gbuffer_leftbytes(yev_get_gbuf(yev_event))
                            );
                        }

                        if(yev_start_event(yev_event) == 0) {
                            priv->tx_in_progress++;
                            break;
                        }
                        gobj_log_error(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_SYSTEM,
                            "msg",          "%s", "Cannot send the rest of a datagram: dropped",
                            "len",          "%d", (int)gbuffer_leftbytes(yev_get_gbuf(yev_event)),
                            NULL
                        );
                    }

                } else {
                    /*
                     *  The kernel refused THIS datagram (nothing listens at
                     *  the url: ECONNREFUSED; too big: EMSGSIZE). It is
                     *  dropped and the next one is sent, as in C_UDP_S. Up
                     *  to 7.29.3 it was taken as a disconnection: the
                     *  client stopped, and only a trace said so.
                     */
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                        "msg",          "%s", "UDP: datagram refused by the kernel, dropped",
                        "url",          "%s", gobj_read_str_attr(gobj, "url"),
                        "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                        "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                        "errno",        "%d", -yev_get_result(yev_event),
                        "strerror",     "%s", strerror(-yev_get_result(yev_event)),
                        NULL
                    );
                }

                /*
                 *  The next datagram. Up to 7.29.3 this asked for
                 *  ST_CONNECTED, a state C_UDP does not have: the first
                 *  datagram was sent, every later one waited in the queue
                 *  for ever, and EV_TX_READY never came (C_UDP_S had the
                 *  same up to 7.25.4)
                 */
                yev_destroy_event(yev_event);
                if(is_txing && gobj_in_this_state(gobj, ST_IDLE)) {
                    try_more_writes(gobj);
                }
            }
            break;

        default:
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "UDP: event type NOT IMPLEMENTED",
                "msg2",         "%s", "🌐UDP: event type NOT IMPLEMENTED",
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
            post_tx_ready(gobj);    // an empty datagram: its EV_TX_READY
        }
        KW_DECREF(kw)
        return checked < 0? -1 : 0;     // Error already logged
    }

    if(!priv->gbuf_txing) {
        priv->gbuf_txing = gbuffer_incref(gbuf);
        if(write_data(gobj) < 0) {
            GBUFFER_DECREF(priv->gbuf_txing)    // Error already logged: this one is dropped
            post_tx_ready(gobj);
        }
    } else {
        enqueue_write(gobj, gbuffer_incref(gbuf));
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The EV_TX_READY posted to itself by post_tx_ready(), a turn later:
 *  published if nothing is in flight nor queued. Otherwise a datagram sent
 *  since publishes it when it ends, or the C_UDP was stopped. Declared in
 *  every state, because the state may change before it comes. Only C_UDP
 *  posts it: EV_TX_READY is an output event, no host sends it.
 ***************************************************************************/
PRIVATE int ac_tx_ready_posted(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src != gobj) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "EV_TX_READY is an output event, it is not sent to a C_UDP",
            "src",          "%s", gobj_short_name(src),
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    priv->tx_ready_posted = FALSE;
    if(gobj_in_this_state(gobj, ST_IDLE) && !priv->no_tx_ready_event &&
            !priv->gbuf_txing && priv->tx_in_progress == 0 && dl_size(&priv->dl_tx) == 0) {
        return gobj_publish_event(gobj, EV_TX_READY, kw);
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
    .mt_create = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_writing = mt_writing,
    .mt_reading = mt_reading,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_UDP);

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
    ev_action_t st_stopped[] = {
        {EV_TX_READY,           ac_tx_ready_posted, 0},
        {0, 0, 0}
    };
    ev_action_t st_wait_stopped[] = {
        {EV_TX_READY,           ac_tx_ready_posted, 0},
        {0, 0, 0}
    };
    ev_action_t st_idle[] = {
        {EV_TX_DATA,            ac_tx_data,         0},
        {EV_TX_READY,           ac_tx_ready_posted, 0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_STOPPED,            st_stopped},
        {ST_WAIT_STOPPED,       st_wait_stopped},
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TX_DATA,            0},
        {EV_RX_DATA,            EVF_OUTPUT_EVENT},
        {EV_TX_READY,           EVF_OUTPUT_EVENT},
        {EV_STOPPED,            EVF_OUTPUT_EVENT},
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
        0,  // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table
        0,  // command_table
        s_user_trace_level,
        0   // gcflag
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
PUBLIC int register_c_udp(void)
{
    return create_gclass(C_UDP);
}
