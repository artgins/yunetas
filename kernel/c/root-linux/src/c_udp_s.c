/***********************************************************************
 *          C_UDP_S0.C
 *          GClass of UDP server level 0 uv-mixin.
 *
 *          Mixin uv-gobj
 *
 *          Copyright (c) 2014-2021 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <errno.h>
#include <regex.h>
#include <sys/socket.h>

#include <yev_loop.h>
#include <ytls.h>
#include "c_yuno.h"
#include "c_udp_s.h"


/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int yev_callback(yev_event_h yev_event);
PRIVATE int udp_set_broadcast(int fd, int on);
PRIVATE int send_data(hgobj gobj, gbuffer_t *gbuf);
PRIVATE void try_to_stop_yevents(hgobj gobj);  // IDEMPOTENT

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
SDATA (DTP_STRING,      "url",                  SDF_RD,  0, "url of udp server"),
SDATA (DTP_STRING,      "lHost",                SDF_RD,  0, "Local ip, got from url"),
SDATA (DTP_STRING,      "lPort",                SDF_RD,  0, "Local port, got from url."),
SDATA (DTP_STRING,      "url",                  SDF_RD,  0, "Url"),
SDATA (DTP_JSON,        "crypto",               SDF_WR|SDF_PERSIST, 0, "Crypto config"),
SDATA (DTP_BOOLEAN,     "only_allowed_ips",     SDF_WR|SDF_PERSIST, 0, "Only allowed ips"),
SDATA (DTP_BOOLEAN,     "trace_tls",            SDF_WR|SDF_PERSIST, 0, "Trace TLS"),
SDATA (DTP_BOOLEAN,     "use_ssl",              SDF_RD,  "FALSE", "True if schema is secure. Set internally"),
SDATA (DTP_BOOLEAN,     "exitOnError",          SDF_RD,  "1", "Exit if Listen failed"),
SDATA (DTP_BOOLEAN,     "set_broadcast",        SDF_WR|SDF_PERSIST, 0, "Set udp broadcast"),
SDATA (DTP_BOOLEAN,     "shared",               SDF_WR|SDF_PERSIST, 0, "Share the port"),
SDATA (DTP_STRING,      "sockname",             SDF_VOLATIL|SDF_STATS, "",  "Sockname"),
SDATA (DTP_POINTER,     "user_data",            0,  0, "user data"),
SDATA (DTP_POINTER,     "user_data2",           0,  0, "more user data"),
SDATA (DTP_POINTER,     "subscriber",           0,  0, "subscriber of output-events. Default if null is parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_TRAFFIC           = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"traffic",             "Trace dump traffic"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
#define BFINPUT_SIZE (2*1024)

typedef struct _PRIVATE_DATA {
    // Conf
    const char *url;
    BOOL exitOnError;

    yev_event_h yev_server_accept;
    hytls ytls;
    hsskt sskt;
    BOOL use_ssl;

    json_int_t connxs;
    json_int_t txMsgs;
    json_int_t rxMsgs;
    json_int_t txBytes;
    json_int_t rxBytes;

    dl_list_t dl_tx;
    gbuffer_t *gbuf_txing;

    BOOL no_tx_ready_event;
    int tx_in_progress;

    BOOL trace_tls;

    char bfinput[BFINPUT_SIZE];

} PRIVATE_DATA;




            /***************************
             *      Framework Methods
             ***************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(url,               gobj_read_str_attr)
    SET_PRIV(exitOnError,       gobj_read_bool_attr)
    SET_PRIV(trace_tls,         gobj_read_bool_attr)

    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber)
        subscriber = gobj_parent(gobj);
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_in_this_state(gobj, ST_STOPPED)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "GObj NOT STOPPED. UV handler ACTIVE!",
            NULL
        );
    }

    GBUFFER_DECREF(priv->gbuf_txing);
    dl_flush(&priv->dl_tx, (fnfree)gbuffer_decref);
}

/***************************************************************************
 *      Framework Method start - return nonstart flag
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(empty_string(priv->url)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "URL NULL",
            NULL
        );
        if(priv->exitOnError) {
            exit(0); //WARNING exit with 0 to stop daemon watcher!
        } else {
            return -1;
        }
    }

    char schema[20], host[120], port[40];
    if(parse_url(
        gobj,
        priv->url,
        schema, sizeof(schema),
        host, sizeof(host),
        port, sizeof(port),
        0, 0,
        0, 0,
        FALSE
    )<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Parsing url failed",
            "url",          "%s", priv->url,
            NULL
        );
        if(priv->exitOnError) {
            exit(0); //WARNING exit with 0 to stop daemon watcher!
        } else {
            return -1;
        }
    }

    if(atoi(port) == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Cannot Listen on port 0",
            "url",          "%s", priv->url,
            NULL
        );
        if(priv->exitOnError) {
            exit(0); //WARNING exit with 0 to stop daemon watcher!
        } else {
            return -1;
        }
    }

    /*--------------------------------*
     *      Setup server
     *--------------------------------*/
    BOOL shared = gobj_read_bool_attr(gobj, "shared");
    priv->yev_server_accept = yev_create_accept_event(
        yuno_event_loop(),
        yev_callback,
        priv->url,    // server_url,
        0,      // backlog,
        shared, // shared
        0,      // ai_family AF_UNSPEC
        0,      // ai_flags AI_V4MAPPED | AI_ADDRCONFIG
        gobj
    );
    if(!priv->yev_server_accept) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "yev_create_accept_event() FAILED",
            "url",          "%s", priv->url,
            NULL
        );
        if(priv->exitOnError) {
            exit(0); //WARNING exit with 0 to stop daemon watcher!
        } else {
            if(priv->yev_server_accept) {
                yev_destroy_event(priv->yev_server_accept);
                priv->yev_server_accept = 0;
            }
            return -1;
        }
    }

    if(yev_get_flag(priv->yev_server_accept) & YEV_FLAG_USE_TLS) {
        priv->use_ssl = TRUE;
        gobj_write_bool_attr(gobj, "use_ssl", TRUE);

        json_t *jn_crypto = gobj_read_json_attr(gobj, "crypto");
        json_object_set_new(jn_crypto, "trace", json_boolean(priv->trace_tls));

        EXEC_AND_RESET(ytls_cleanup, priv->ytls)
        priv->ytls = ytls_init(gobj, jn_crypto, TRUE);
    }

    udp_set_broadcast(
        yev_get_fd(priv->yev_server_accept),
        gobj_read_bool_attr(gobj, "set_broadcast")
    );

    gobj_write_str_attr(gobj, "lHost", host);
    gobj_write_str_attr(gobj, "lPort", port);

    char temp[60];
    get_sockname(temp, sizeof(temp), yev_get_fd(priv->yev_server_accept));
    gobj_write_str_attr(gobj, "sockname", temp);

    /*
     *  Info of "listening..."
     */
    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
        "msg",          "%s", "UDP listening ...",
        "msg2",         "%s", "UDP Listening...🔷",
        "url",          "%s", priv->url,
        "lHost",        "%s", host,
        "lPort",        "%s", port,
        NULL
    );

    gobj_change_state(gobj, ST_IDLE);

    yev_start_event(priv->yev_server_accept);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    // PRIVATE_DATA *priv = gobj_priv_data(gobj);

    try_to_stop_yevents(gobj);

    gobj_reset_volatil_attrs(gobj);

    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int udp_set_broadcast(int fd, int on)
{
    if(setsockopt(fd,
        SOL_SOCKET,
        SO_BROADCAST,
        &on,
        sizeof(on))
    ) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "setsockopt() FAILED",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

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
            " -> ",
            gobj_read_str_attr(gobj, "peername")
        );
    }

    if(priv->sskt) {
        GBUFFER_INCREF(gbuf)
        if(ytls_encrypt_data(priv->ytls, priv->sskt, gbuf)<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "ytls_encrypt_data() FAILED",
                "error",        "%s", ytls_get_last_error(priv->ytls, priv->sskt),
                NULL
            );
            try_to_stop_yevents(gobj);
        }
        if(gbuffer_leftbytes(gbuf) > 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "NEED a queue, NOT ALL DATA being encrypted",
                NULL
            );
        }
    } else {
        priv->txMsgs++;

        /*
         *  Transmit
         */
        int fd =yev_get_fd(priv->yev_server_accept);
        yev_event_h yev_write_event = yev_create_write_event(
            yuno_event_loop(),
            yev_callback,
            gobj,
            fd,
            gbuffer_incref(gbuf)
        );

        priv->tx_in_progress++;
        yev_start_event(yev_write_event);
    }
    return 0;
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
    GBUFFER_DECREF(priv->gbuf_txing)

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
            /*
             *  CHILD subscription model
             */
            if(gobj_is_service(gobj)) {
                gobj_publish_event(gobj, EV_TX_READY, kw_tx_ready);
            } else {
                gobj_send_event(gobj_parent(gobj), EV_TX_READY, kw_tx_ready, gobj);
            }
        }
    } else {
        priv->gbuf_txing = gbuf_txing;
        dl_delete(&priv->dl_tx, gbuf_txing, 0);
        write_data(gobj);
    }
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
    //             "gobj",         "%s", gobj_full_name(gobj),
    //             "function",     "%s", __FUNCTION__,
    //             "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
            NULL
        );
    }

    if(priv->yev_server_accept) {
        yev_stop_event(priv->yev_server_accept);
        if(yev_event_is_stopped(priv->yev_server_accept)) {
            yev_destroy_event(priv->yev_server_accept);
            priv->yev_server_accept = 0;
        } else {
            to_wait_stopped = TRUE;
        }
    }

    // if(priv->yev_reading) {
    //     if(!yev_event_is_stopped(priv->yev_reading)) {
    //         yev_stop_event(priv->yev_reading);
    //         if(!yev_event_is_stopped(priv->yev_reading)) {
    //             to_wait_stopped = TRUE;
    //         }
    //     }
    // }
    //
    // if(priv->tx_in_progress > 0) {
    //     to_wait_stopped = TRUE;
    // }

    if(to_wait_stopped) {
        gobj_change_state(gobj, ST_WAIT_STOPPED);
    } else {
        if(gobj_current_state(gobj)==ST_DISCONNECTED) {
            gobj_change_state(gobj, ST_STOPPED);
        } else {
            gobj_change_state(gobj, ST_STOPPED);
            // set_disconnected(gobj);
        }
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
            "msgset",       "%s", MSGSET_LIBUV_ERROR,
            "msg",          "%s", "read FAILED",
            "uv_error",     "%s", uv_err_name(nread),
            NULL
        );
        return;
    }

    if(nread == 0 && !addr) {
        // Yes, sometimes arrive with nread 0.
        return;
    }
    *priv->prxBytes += nread;

    ip_port ipp;
    ipp.is_ip6 = priv->ipp_sockname.is_ip6;
    memcpy(&ipp.sa.ip, addr, sizeof(ipp.sa.ip));
    char peername[60];
    get_ipp_url(&ipp, peername, sizeof(peername));

    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        gobj_trace_dump(gobj,
            0,
            buf->base,
            nread,
            "%s: %s %s %s",
                gobj_full_name(gobj),
                priv->sockname,
                "<-",
                peername
        );
    }

    if(!empty_string(priv->rx_data_event_name)) {
        if(nread) {
            gbuffer_t *gbuf = gbuffer_create(nread, nread, 0,0);
            if(!gbuf) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MEMORY_ERROR,
                    "msg",          "%s", "no memory for gbuf",
                    "size",         "%d", nread,
                    NULL);
                return;
            }
            gbuffer_append(gbuf, buf->base, nread);
            gbuf_setlabel(gbuf, peername);
            json_t *kw_ev = json_pack("{s:I}",
                "gbuffer", (json_int_t)(size_t)gbuf
            );
            gobj_publish_event(gobj, priv->rx_data_event_name, kw_ev);
        }
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
            "msgset",       "%s", MSGSET_LIBUV_ERROR,
            "msg",          "%s", "upd send FAILED",
            "uv_error",     "%s", uv_err_name(status),
            NULL);
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
 *  udp_channel is "ip:port" and it's in the label of gbuff.
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
    const char *udp_channel = gbuffer_getlabel(gbuf);
    if(!udp_channel) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "udp_channel NULL",
            NULL
        );
        GBUFFER_DECREF(priv->gbuf_txing);
        return -1;
    }

    size_t ln = gbuffer_chunk(gbuf);
    if(ln > 1500) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "UPD lenght must not be greater than 1500",
            "ln",           "%d", ln,
            NULL
        );
        ln = 1500;
    }
    char *bf = gbuffer_get(gbuf, ln);

    uv_buf_t b[] = {
        {.base = bf, .len = ln}
    };

    priv->req_send.data = gobj;

    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        gobj_trace_dump(gobj,
            0,
            b[0].base,
            b[0].len,
            "%s: %s %s %s",
                gobj_full_name(gobj),
                priv->sockname,
                "->",
                udp_channel
        );
    }

    *priv->ptxBytes += ln;

    char ip[60];
    int port=0;
    {
        strncpy(ip, udp_channel, sizeof(ip));
        char *p = strrchr(ip, ':');
        if(p) {
            *p = 0;
            p++;    // point to port
            port = atoi(p);
        }
    }
    if(!port) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "UDP port is 0",
            "udp_channel", "%s", udp_channel,
            NULL
        );
    }

    struct sockaddr send_addr;
    int ret;
    if(priv->ipp_sockname.is_ip6) {
        // TODO V641 The size of the '& send_addr' buffer is not a multiple of the element size of the type 'struct sockaddr_in6'
        ret = uv_ip6_addr(ip, port, (struct sockaddr_in6 *)&send_addr);
    } else {
        ret = uv_ip4_addr(ip, port, (struct sockaddr_in *)&send_addr);
    }
    if(ret < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBUV_ERROR,
            "msg",          "%s", "uv_ip?_addr() FAILED",
            "uv_error",     "%s", uv_err_name(ret),
            "udp_channel", "%s", udp_channel,
            NULL
        );
    }

    if(gobj_trace_level(gobj) & TRACE_UV) {
        log_debug_printf(0, ">>>upd_send(%s:%s)",
            gobj_gclass_name(gobj), gobj_name(gobj)
        );
    }
    ret = uv_udp_send(
        &priv->req_send,
        &priv->uv_udp,
        b,
        1,
        &send_addr,
        on_upd_send_cb
    );
    if(ret < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBUV_ERROR,
            "msg",          "%s", "uv_udp_send() FAILED",
            "uv_error",     "%s", uv_err_name(ret),
            "udp_channel", "%s", udp_channel,
            NULL
        );
    }

    return 0;
}
#endif

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
                            " <- ",
                            gobj_read_str_attr(gobj, "peername")
                        );
                    }

                    priv->rxMsgs++;
                    priv->rxBytes += (json_int_t)gbuffer_leftbytes(yev_get_gbuf(yev_event));

                    int ret = 0;

                    if(priv->use_ssl) {
                        GBUFFER_INCREF(yev_get_gbuf(yev_event))
                        ret = ytls_decrypt_data(priv->ytls, priv->sskt, yev_get_gbuf(yev_event));
                        if(ret < 0) {
                            /*
                             *  If return -1 while doing handshake then is good stop here the gobj,
                             *  But if return -1 in response of gobj_send_event,
                             *      then it can be already stopped and destroyed
                             *      Solution: don't return -1 on ytls_on_clear_data_callback
                             */
                            if(ret < -1000) { // Mark as TLS error
                                if(gobj_is_running(gobj)) {
                                    gobj_stop(gobj); // auto-stop
                                    // WARNING if IS_CLISRV the gobj will be destroyed here
                                }
                            }
                            break;
                        }

                    } else {
                        GBUFFER_INCREF(yev_get_gbuf(yev_event))
                        json_t *kw = json_pack("{s:I}",
                            "gbuffer", (json_int_t)(size_t)yev_get_gbuf(yev_event)
                        );
                        /*
                         *  CHILD subscription model
                         */
                        if(gobj_is_service(gobj)) {
                            ret = gobj_publish_event(gobj, EV_RX_DATA, kw);
                        } else {
                            ret = gobj_send_event(gobj_parent(gobj), EV_RX_DATA, kw, gobj);
                        }
                    }

                    /*
                     *  Clear buffer, re-arm read
                     *  Check ret is 0 because the EV_RX_DATA could provoke
                     *      stop or destroy of gobj
                     *      or order to disconnect (EV_DROP)
                     *  If try_to_stop_yevents() has been called (mt_stop, EV_DROP,...)
                     *      this event will be in stopped state.
                     *  If it's in idle then re-arm
                     */
                    if(ret == 0 && yev_event_is_idle(yev_event)) {
                        gbuffer_clear(yev_get_gbuf(yev_event));
                        yev_start_event(yev_event);
                    }

                } else {
                    /*
                     *  Disconnected
                     */
                    gobj_log_set_last_message("%s", strerror(-yev_get_result(yev_event)));

                    if(trace) {
                        gobj_log_debug(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                            "msg",          "%s", "UDP: read FAILED",
                            "msg2",         "%s", "🌐UDP: read FAILED",
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
                        if(gbuffer_leftbytes(yev_get_gbuf(yev_event)) > 0) {
                            if(trace_level & TRACE_MACHINE) {
                                trace_machine("🔄🍄🍄mach(%s%s^%s), st: %s transmit PENDING data %ld",
                                    !gobj_is_running(gobj)?"!!":"",
                                    gobj_gclass_name(gobj), gobj_name(gobj),
                                    gobj_current_state(gobj),
                                    (long)gbuffer_leftbytes(yev_get_gbuf(yev_event))
                                );
                            }

                            priv->tx_in_progress++;
                            yev_start_event(yev_event);
                            break;
                        }

                        if(gobj_in_this_state(gobj, ST_CONNECTED)) { // Avoid while doing handshaking
                            try_more_writes(gobj);
                        }
                        yev_destroy_event(yev_event);
                    } else {
                        yev_destroy_event(yev_event);
                        try_to_stop_yevents(gobj);
                    }

                } else {
                    /*
                     *  Disconnected
                     */
                    gobj_log_set_last_message("%s", strerror(-yev_get_result(yev_event)));

                    if(trace) {
                        gobj_log_debug(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                            "msg",          "%s", "UDP: write FAILED",
                            "msg2",         "%s", "🌐UDP: write FAILED",
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

                    yev_destroy_event(yev_event);

                    try_to_stop_yevents(gobj);
                }

            }
            break;

        default:
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
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
 *  udp_channel is "ip:port" and it's in the label of gbuff.
 ***************************************************************************/
PRIVATE int ac_tx_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);

    // send_data(gobj, gbuf);

    JSON_DECREF(kw);
    return 1;
}

/***********************************************************************
 *          FSM
 ***********************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_writing = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_UDP_S);

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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
        {0, 0, 0}
    };
    ev_action_t st_wait_stopped[] = {
        {0, 0, 0}
    };
    ev_action_t st_idle[] = {
        {EV_TX_DATA,            ac_tx_data,         0},
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
        tattr_desc,
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
PUBLIC int register_c_udp_s(void)
{
    return create_gclass(C_UDP_S);
}
