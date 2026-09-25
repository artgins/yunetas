/***********************************************************************
 *          C_UDP_S0.C
 *          GClass of UDP server level 0 uv-mixin.
 *
 *          Mixin uv-gobj
 *
 *          Copyright (c) 2014-2021 Niyamaka.
 *          Copyright (c) 2025-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include <yev_loop.h>
#include <ytls.h>
#include "c_yuno.h"
#include "c_udp_s.h"
#include "msg_ievent.h"


/***************************************************************************
 *              Constants
 ***************************************************************************/
/*
 *  A refused datagram is said with a WARNING on the transition: the first
 *  one of a cause, then at most one each REFUSAL_WARN_MSEC, with the count
 *  of the ones dropped in between. The source of a datagram can be forged:
 *  a warning per datagram was a flood of the log. Timed on the monotonic
 *  clock (msectimer): a clock set back must not silence the warnings.
 */
#define REFUSAL_WARN_MSEC       60000

typedef enum {
    REFUSAL_DENIED = 0,     // the peer is in denied_ips
    REFUSAL_NOT_ALLOWED,    // only_allowed_ips, and the peer is not in allowed_ips
    REFUSAL_CAUSES
} refusal_cause_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int yev_callback(yev_event_h yev_event);
PRIVATE int udp_set_broadcast(int fd, int on);
// PRIVATE int send_data(hgobj gobj, gbuffer_t *gbuf);
PRIVATE void try_to_stop_yevents(hgobj gobj);  // IDEMPOTENT
PRIVATE int reload_ytls_from_attrs(hgobj gobj);
PRIVATE json_t *cmd_reload_certs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_cert(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Commands
 *---------------------------------------------*/
PRIVATE sdata_desc_t pm_reload_certs[] = {
SDATA_END()
};
PRIVATE sdata_desc_t pm_view_cert[] = {
SDATA_END()
};

PRIVATE sdata_desc_t command_table[] = {
SDATACM(DTP_SCHEMA, "reload-certs", 0, pm_reload_certs, cmd_reload_certs, "Reload TLS certificates from the 'crypto' attribute without dropping active connections"),
SDATACM(DTP_SCHEMA, "view-cert",    0, pm_view_cert,    cmd_view_cert,    "Show metadata of the currently loaded TLS server certificate"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
SDATA (DTP_STRING,      "url",              SDF_RD,  0, "url of udp server"),
SDATA (DTP_STRING,      "lHost",            SDF_RD,  0, "Listening ip, got internally from url"),
SDATA (DTP_STRING,      "lPort",            SDF_RD,  0, "Listening port, got internally from url"),
SDATA (DTP_DICT,        "crypto",           SDF_WR|SDF_PERSIST, "{}", "Crypto config"),
SDATA (DTP_BOOLEAN,     "only_allowed_ips", SDF_WR|SDF_PERSIST, 0, "Only allowed ips"),
SDATA (DTP_BOOLEAN,     "trace_tls",        SDF_WR|SDF_PERSIST, 0, "Trace TLS"),
SDATA (DTP_BOOLEAN,     "use_ssl",          SDF_RD,  "FALSE", "True if schema is secure. Set internally"),
SDATA (DTP_BOOLEAN,     "exitOnError",      SDF_RD,  "1", "Exit if Listen failed"),
SDATA (DTP_BOOLEAN,     "set_broadcast",    SDF_WR|SDF_PERSIST, 0, "Set udp broadcast"),
SDATA (DTP_BOOLEAN,     "shared",           SDF_WR|SDF_PERSIST, 0, "Share the port"),
SDATA (DTP_INTEGER,     "rx_buffer_size",   SDF_WR|SDF_PERSIST, "4096", "Rx buffer size"),

SDATA (DTP_INTEGER,     "txBytes",          SDF_RSTATS,     "0", "Messages transmitted"),
SDATA (DTP_INTEGER,     "rxBytes",          SDF_RSTATS,     "0", "Messages received"),
SDATA (DTP_INTEGER,     "txMsgs",           SDF_RSTATS,     "0", "Messages transmitted"),
SDATA (DTP_INTEGER,     "rxMsgs",           SDF_RSTATS,     "0", "Messages received"),
SDATA (DTP_INTEGER,     "rxRefusedMsgs",    SDF_RSTATS,     "0", "Datagrams dropped: their peer is in denied_ips, or not in allowed_ips with only_allowed_ips"),
SDATA (DTP_STRING,      "sockname",         SDF_VOLATIL|SDF_STATS, "",  "Sockname"),
SDATA (DTP_POINTER,     "user_data",        0,  0, "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,  0, "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,  0, "subscriber of output-events. Default if null is parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_TRAFFIC   = 0x0001,
    TRACE_TLS       = 0x0002,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"traffic",         "Trace dump traffic"},
{"tls",             "Trace tls"},
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

    yev_event_h yev_server_udp;
    yev_event_h yev_reading;
    hytls ytls;
    hsskt sskt;
    BOOL use_ssl;

    json_int_t connxs;
    json_int_t txMsgs;
    json_int_t rxMsgs;
    json_int_t txBytes;
    json_int_t rxBytes;
    json_int_t rxRefusedMsgs;

    uint64_t t_refusal_warn[REFUSAL_CAUSES];        // next warning of a cause (msectimer)
    json_int_t refused_since_warn[REFUSAL_CAUSES];  // dropped since the last warning

    dl_list_t dl_tx;
    gbuffer_t *gbuf_txing;

    int tx_in_progress;

    BOOL trace_tls;
    BOOL only_allowed_ips;

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
    SET_PRIV(only_allowed_ips,  gobj_read_bool_attr)

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

    IF_EQ_SET_PRIV(trace_tls,           gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(only_allowed_ips,  gobj_read_bool_attr)
    END_EQ_SET_PRIV()

    /*
     * If the 'crypto' attribute is written while the listener is running with
     * TLS enabled, hot-reload the certificates. Active TLS sessions keep
     * working with the old state until they close; new sessions use the new.
     */
    if(path && strcmp(path, "crypto") == 0) {
        if(priv->use_ssl && priv->ytls && gobj_is_running(gobj)) {
            reload_ytls_from_attrs(gobj);
        }
    }
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
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "GObj NOT STOPPED. UV handler ACTIVE!",
            NULL
        );
    }

    EXEC_AND_RESET(yev_destroy_event, priv->yev_server_udp)
    EXEC_AND_RESET(yev_destroy_event, priv->yev_reading)

    GBUFFER_DECREF(priv->gbuf_txing);
    dl_flush(&priv->dl_tx, (fnfree)gbuffer_decref);
}

/***************************************************************************
 *      Framework Method start - return nonstart flag
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->yev_server_udp) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "yev_server_udp ALREADY exists",
            "state",        "%s", gobj_current_state(gobj),
            NULL
        );
        return -1;
    }

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
            "msgset",       "%s", MSGSET_PARAMETER,
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
    priv->yev_server_udp = yev_create_accept_event(
        yuno_event_loop(),
        yev_callback,
        priv->url,    // server_url,
        0,      // backlog,
        shared, // shared
        0,      // ai_family AF_UNSPEC
        0,      // ai_flags AI_V4MAPPED | AI_ADDRCONFIG
        gobj
    );
    if(!priv->yev_server_udp) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "yev_create_accept_event() FAILED",
            "url",          "%s", priv->url,
            NULL
        );
        if(priv->exitOnError) {
            exit(0); //WARNING exit with 0 to stop daemon watcher!
        } else {
            if(priv->yev_server_udp) {
                yev_destroy_event(priv->yev_server_udp);
                priv->yev_server_udp = 0;
            }
            return -1;
        }
    }

    if(yev_get_flag(priv->yev_server_udp) & YEV_FLAG_USE_TLS) {
        priv->use_ssl = TRUE;
        gobj_write_bool_attr(gobj, "use_ssl", TRUE);

        json_t *jn_crypto = gobj_read_json_attr(gobj, "crypto");
        uint32_t trace_level = gobj_trace_level(gobj);
        if(json_object_set_new(
            jn_crypto,
            "trace_tls",
            json_boolean(priv->trace_tls || (trace_level & TRACE_TLS))
        )<0) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Cannot set 'trace_tls', 'crypto' is not a dict",
                NULL
            );
        }

        EXEC_AND_RESET(ytls_cleanup, priv->ytls)
        priv->ytls = ytls_init(gobj, jn_crypto, TRUE);
    }

    udp_set_broadcast(
        yev_get_fd(priv->yev_server_udp),
        gobj_read_bool_attr(gobj, "set_broadcast")
    );

    gobj_write_str_attr(gobj, "lHost", host);
    gobj_write_str_attr(gobj, "lPort", port);

    char temp[60];
    get_sockname(temp, sizeof(temp), yev_get_fd(priv->yev_server_udp));
    gobj_write_str_attr(gobj, "sockname", temp);

    /*
     *  Info of "listening..."
     */
    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
        "msg",          "%s", "UDP listening ...",
        "msg2",         "%s", "UDP Listening...🔷",
        "url",          "%s", priv->url,
        "sockname",     "%s", gobj_read_str_attr(gobj, "sockname"),
        NULL
    );

    /*-------------------------------*
     *      Setup reading event
     *-------------------------------*/
    /*
     *  A new read, on the NEW socket. The read of a previous start keeps
     *  the number of the socket its stop closed: up to 7.25.4 it was
     *  started again on that number, which by then could be any other
     *  file of the process (logcenter opens its log file before it
     *  starts its listener), and the server stopped reading with nothing
     *  logged. A read still canceling (a start in the same turn as the
     *  stop) is destroyed too: the loop frees it at the end of its cancel,
     *  without calling back.
     */
    EXEC_AND_RESET(yev_destroy_event, priv->yev_reading)

    json_int_t rx_buffer_size = gobj_read_integer_attr(gobj, "rx_buffer_size");
    priv->yev_reading = yev_create_recvmsg_event(
        yuno_event_loop(),
        yev_callback,
        gobj,
        yev_get_fd(priv->yev_server_udp),
        gbuffer_create(rx_buffer_size, rx_buffer_size)
    );
    if(!priv->yev_reading || yev_start_event(priv->yev_reading) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot start the read of the UDP server",
            "url",          "%s", priv->url,
            NULL
        );
        EXEC_AND_RESET(yev_destroy_event, priv->yev_reading)
        EXEC_AND_RESET(yev_destroy_event, priv->yev_server_udp)
        if(priv->exitOnError) {
            exit(0); //WARNING exit with 0 to stop daemon watcher!
        }
        return -1;
    }

    gobj_change_state(gobj, ST_IDLE);

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
    } else if(strcmp(name, "rxRefusedMsgs")==0) {
        v.found = 1;
        v.v.i = priv->rxRefusedMsgs;
    } else if(strcmp(name, "cur_tx_queue")==0) {
        v.v.i = (json_int_t)dl_size(&priv->dl_tx);
    }

    return v;
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
            "msgset",       "%s", MSGSET_SYSTEM,
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
 *  Enqueue data
 ***************************************************************************/
PRIVATE int enqueue_write(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

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
        struct sockaddr *addr = gbuffer_getaddr(gbuf);
        char peername[80];
        print_socket_address(peername, sizeof(peername), addr);
        gobj_trace_dump_gbuf(gobj, gbuf, "%s: %s%s%s",
            gobj_short_name(gobj),
            gobj_read_str_attr(gobj, "sockname"),
            " ⏩ ",
            peername
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
        int fd =yev_get_fd(priv->yev_server_udp);
        yev_event_h yev_write_event = yev_create_sendmsg_event(
            yuno_event_loop(),
            yev_callback,
            gobj,
            fd,
            gbuffer_incref(gbuf),
            gbuffer_getaddr(gbuf),      // it lives in the gbuffer, held by the event
            gbuffer_getaddrlen(gbuf)
        );
        if(!yev_write_event) {
            GBUFFER_DECREF(gbuf)    // Error already logged: the reference given to the event
            return -1;
        }

        /*
         *  A send that does not start (a gbuffer with no peer address, or
         *  no memory to keep the submission: logged by yev_start_event)
         *  will not complete: the callback never frees the event nor
         *  counts it done. Up to 7.25.4 the event and its gbuffer leaked,
         *  tx_in_progress never went back to 0 (the stop waited for ever)
         *  and gbuf_txing held every later datagram in the queue.
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
     *  Get the next tx msg: a datagram that cannot be sent is dropped
     *  (logged by write_data) and the next one is tried
     */
    gbuffer_t *gbuf_txing;
    while((gbuf_txing = dl_first(&priv->dl_tx)) != NULL) {
        priv->gbuf_txing = gbuf_txing;
        dl_delete(&priv->dl_tx, gbuf_txing, 0);
        if(write_data(gobj) == 0) {
            break;
        }
        GBUFFER_DECREF(priv->gbuf_txing)    // Error already logged
    }
}

/***************************************************************************
 *  Send data
 *  udp_channel is "ip:port" and it's in the label of gbuf.
 ***************************************************************************/
#ifdef PEPE
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
            "msgset",       "%s", MSGSET_INTERNAL,
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
            "msgset",       "%s", MSGSET_INTERNAL,
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
            "msgset",       "%s", MSGSET_INTERNAL,
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
            "msgset",       "%s", MSGSET_LIBURING,
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
            "msgset",       "%s", MSGSET_LIBURING,
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

    if(priv->yev_server_udp) {
        yev_stop_event(priv->yev_server_udp);
        if(yev_event_is_stopped(priv->yev_server_udp)) {
            yev_destroy_event(priv->yev_server_udp);
            priv->yev_server_udp = 0;
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
        to_wait_stopped = TRUE;
    }

    /*
     *  What waits to be sent goes with the stop. The datagram in flight
     *  is held by its send event, which completes on its own. Up to 7.25.4
     *  gbuf_txing was kept: after a start every datagram waited behind it
     *  in the queue, and nothing was sent again.
     */
    size_t queued = dl_size(&priv->dl_tx);
    if(queued > 0) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "UDP server stopped: the datagrams waiting to be sent are dropped",
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
        gobj_publish_event(gobj, EV_STOPPED, 0);
    }
}

/***************************************************************************
 *  The peer of a recvmsg or sendmsg event, "ip:port" or "[ipv6]:port"
 ***************************************************************************/
PRIVATE void get_peer_of_event(char *peername, size_t size, yev_event_h yev_event)
{
    peername[0] = 0;
    if(yev_event->msghdr && yev_event->msghdr->msg_name &&
            yev_event->msghdr->msg_namelen > 0) {
        if(print_socket_address(peername, size, yev_event->msghdr->msg_name) < 0) {
            peername[0] = 0;
        }
    }
}

/***************************************************************************
 *  A peer on this host: 127.0.0.x, ::1, or 127.0.0.x seen by a dual-stack
 *  socket. The same exemption as in C_TCP_S.
 ***************************************************************************/
PRIVATE BOOL is_loopback_peer(const char *peername)
{
    const char *loopbacks[] = {"127.0.0.", "[::1]", "[::ffff:127.0.0.", 0};
    for(int i=0; loopbacks[i]; i++) {
        if(strncmp(peername, loopbacks[i], strlen(loopbacks[i]))==0) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  Is the datagram of this peer refused? The yuno's ip lists, as C_TCP_S
 *  asks them at accept. A loopback peer is exempt from both; a peer in
 *  `denied_ips` is refused always, and wins; with `only_allowed_ips`, a
 *  peer not in `allowed_ips` is refused.
 ***************************************************************************/
PRIVATE BOOL peer_is_refused(hgobj gobj, const char *peername, refusal_cause_t *cause)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(is_loopback_peer(peername)) {
        return FALSE;
    }
    if(is_ip_denied(peername)) {
        *cause = REFUSAL_DENIED;
        return TRUE;
    }
    if(priv->only_allowed_ips && !is_ip_allowed(peername)) {
        *cause = REFUSAL_NOT_ALLOWED;
        return TRUE;
    }
    return FALSE;
}

/***************************************************************************
 *  A refused datagram: counted always (rxRefusedMsgs), said on the
 *  transition. The first one of a cause is a WARNING, then at most one each
 *  REFUSAL_WARN_MSEC, with the datagrams of that cause dropped since the
 *  last one (`dropped`, this one included). Said on the transition: a
 *  flood of a forged source must not be a flood of the log.
 ***************************************************************************/
PRIVATE void note_refused_datagram(
    hgobj gobj,
    refusal_cause_t cause,
    const char *peername,
    size_t len
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->rxRefusedMsgs++;
    priv->refused_since_warn[cause]++;

    if(priv->t_refusal_warn[cause] != 0 && !test_msectimer(priv->t_refusal_warn[cause])) {
        return; // counted, said at the next warning of this cause
    }

    const char *refusal = (cause == REFUSAL_DENIED)?
        "UDP_S: Ip denied, datagram dropped":
        "UDP_S: Ip not allowed, datagram dropped";
    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
        "msg",          "%s", refusal,
        "msg2",         "%s", refusal,
        "url",          "%s", priv->url,
        "peername",     "%s", peername,
        "len",          "%d", (int)len,
        "dropped",      "%ld", (long)priv->refused_since_warn[cause],
        "rxRefusedMsgs", "%ld", (long)priv->rxRefusedMsgs,
        "next_warning_in_ms", "%d", REFUSAL_WARN_MSEC,
        NULL
    );
    priv->refused_since_warn[cause] = 0;
    priv->t_refusal_warn[cause] = start_msectimer(REFUSAL_WARN_MSEC);
}

/***************************************************************************
 *  Read the next datagram. The gbuffer of the one just published is used
 *  again when nobody kept it, the common case. When the host kept it --
 *  it answered IN it (EV_TX_DATA with the kw of the EV_RX_DATA: the peer
 *  address is there already), and the answer waits in the queue or is in
 *  flight -- the next datagram goes to a NEW gbuffer. Up to 7.25.4 it was
 *  cleared and read into again: the answer was sent empty (dropped, "Cannot
 *  send datagram"), or with the bytes and the peer of the next datagram,
 *  and a zero-copy send could read memory the next read was writing.
 ***************************************************************************/
PRIVATE void rearm_read(hgobj gobj, yev_event_h yev_event)
{
    gbuffer_t *gbuf = yev_get_gbuf(yev_event);

    if(gbuf && gbuf->refcount > 1) {
        json_int_t rx_buffer_size = gobj_read_integer_attr(gobj, "rx_buffer_size");
        gbuffer_t *gbuf_new = gbuffer_create((size_t)rx_buffer_size, (size_t)rx_buffer_size);
        if(!gbuf_new) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY,
                "msg",          "%s", "UDP: no memory for the next read, the server stops listening",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                "rx_buffer_size", "%ld", (long)rx_buffer_size,
                NULL
            );
            try_to_stop_yevents(gobj);
            return;
        }
        yev_set_gbuffer(yev_event, NULL);       // the host keeps its reference
        yev_set_gbuffer(yev_event, gbuf_new);   // owned by the event
    } else {
        gbuffer_clear(gbuf);
    }
    yev_start_event(yev_event); // a failure is logged by yev_start_event()
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
    int trace = (int)trace_level & TRACE_URING;
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

    /*
     *  The peer, always: it is the label of the EV_RX_DATA gbuffer, and
     *  C_GSS_UDP_S keys its channels by it. Up to 7.25.4 it was written
     *  only when tracing: every peer shared the channel "", and the pieces
     *  of the long messages of two peers were joined in one frame.
     */
    char peername[80];
    get_peer_of_event(peername, sizeof(peername), yev_event);

    yev_state_t yev_state = yev_get_state(yev_event);

    switch(yev_get_type(yev_event)) {
        case YEV_RECVMSG_TYPE:
            {
                /*
                 *  yev_get_gbuf(yev_event) can be null if yev_stop_event() was called
                 */
                gbuffer_t *gbuf = yev_get_gbuf(yev_event);

                if(yev_state == YEV_ST_IDLE) {
                    /*
                     *  The peer, with the length the kernel gave: an answer
                     *  written in this gbuffer goes back to it (write_data;
                     *  the next read takes a new gbuffer, see rearm_read())
                     */
                    gbuffer_setaddr(
                        gbuf,
                        yev_event->msghdr->msg_name,
                        yev_event->msghdr->msg_namelen
                    );

                    if(trace_level & TRACE_TRAFFIC) {
                        gobj_trace_dump_gbuf(gobj, gbuf, "%s: %s%s%s",
                            gobj_short_name(gobj),
                            gobj_read_str_attr(gobj, "sockname"),
                            " ⏪ ",
                            peername
                        );
                    }

                    refusal_cause_t cause;
                    if(peer_is_refused(gobj, peername, &cause)) {
                        /*
                         *  Up to 7.25.4 `only_allowed_ips` was documented
                         *  and never read, and the deny-list not asked:
                         *  every peer was heard
                         */
                        note_refused_datagram(gobj, cause, peername, gbuffer_leftbytes(gbuf));
                        rearm_read(gobj, yev_event);
                        break;
                    }

                    priv->rxMsgs++;
                    priv->rxBytes += (json_int_t)gbuffer_leftbytes(gbuf);

                    int ret = 0;

                    if(priv->use_ssl) {
                        GBUFFER_INCREF(gbuf)
                        ret = ytls_decrypt_data(priv->ytls, priv->sskt, gbuf);
                        if(ret < 0) {
                            /*
                             *  If return -1 while doing handshake then is good stop here the gobj,
                             *  But if return -1 in response of gobj_send_event,
                             *      then it can be already stopped and destroyed
                             *      Solution: don't return -1 on ytls_on_clear_data_callback
                             */
                            if(ret < -1000) { // Mark as TLS error
                                try_to_stop_yevents(gobj);
                            }
                            break;
                        }

                    } else {
                        GBUFFER_INCREF(gbuf)
                        json_t *kw = json_pack("{s:I}",
                            "gbuffer", (json_int_t)(uintptr_t)gbuf
                        );
                        gbuffer_setlabel(gbuf, peername);
                        ret = gobj_publish_event(gobj, EV_RX_DATA, kw);
                    }

                    /*
                     *  Re-arm read (a new gbuffer if the host kept this one)
                     *  Check ret is 0 because the EV_RX_DATA could provoke
                     *      stop or destroy of gobj
                     *      or order to disconnect (EV_DROP)
                     *  If try_to_stop_yevents() has been called (mt_stop, EV_DROP,...)
                     *      this event will be in stopped state.
                     *  If it's in idle then re-arm
                     */
                    if(ret == 0 && yev_event_is_idle(yev_event)) {
                        rearm_read(gobj, yev_event);
                    }

                } else {
                    /*
                     *  The read ends: canceled by a stop, or the socket failed
                     */
                    gobj_log_set_last_message("%s", strerror(-yev_get_result(yev_event)));

                    if(gobj_is_running(gobj) && yev_get_result(yev_event) != -ECANCELED) {
                        gobj_log_error(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                            "msg",          "%s", "UDP: read FAILED, the server stops listening",
                            "msg2",         "%s", "🌐UDP: read FAILED, the server stops listening",
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
                            "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                            "errno",        "%d", -yev_get_result(yev_event),
                            "strerror",     "%s", strerror(-yev_get_result(yev_event)),
                            "fd",           "%d", yev_get_fd(yev_event),
                            NULL
                        );
                    } else if(trace) {
                        gobj_log_debug(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                            "msg",          "%s", "UDP: read stopped",
                            "msg2",         "%s", "🌐UDP: read stopped",
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
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

        case YEV_SENDMSG_TYPE:
            {
                priv->tx_in_progress--;

                /*
                 *  The completion of the datagram being sent, not of one
                 *  sent before a stop and a start (its gbuffer was dropped
                 *  by the stop)
                 */
                BOOL is_txing = priv->gbuf_txing &&
                    yev_get_gbuf(yev_event) == priv->gbuf_txing;

                if(!gobj_is_running(gobj)) {
                    /*
                     *  A stop waits for its sends: destroy the write event
                     */
                    yev_destroy_event(yev_event);
                    try_to_stop_yevents(gobj);
                    break;
                }

                if(yev_state == YEV_ST_IDLE) {
                    int sent = yev_get_result(yev_event);
                    if(sent > 0) {
                        priv->txBytes += (json_int_t)sent;
                    }

                    /*
                     *  See if all data was transmitted
                     */
                    gbuffer_t *gbuf = yev_get_gbuf(yev_event);
                    if(sent > 0 && gbuf && gbuffer_leftbytes(gbuf) > 0) {
                        if(trace_level & TRACE_MACHINE) {
                            trace_machine("🔄🍄🍄mach(%s%s^%s), st: %s transmit PENDING data %ld",
                                !gobj_is_running(gobj)?"!!":"",
                                gobj_gclass_name(gobj), gobj_name(gobj),
                                gobj_current_state(gobj),
                                (long)gbuffer_leftbytes(gbuf)
                            );
                        }

                        priv->tx_in_progress++;
                        yev_start_event(yev_event);
                        break;
                    }

                } else {
                    /*
                     *  The kernel refused THIS datagram (a peer port 0 or an
                     *  address of another family: EINVAL, EAFNOSUPPORT; a
                     *  broadcast without set_broadcast: EACCES; too big:
                     *  EMSGSIZE). It is dropped and the next one is sent.
                     *  Up to 7.25.4 it was taken as a disconnection: the
                     *  whole server stopped, and only a trace said so.
                     */
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                        "msg",          "%s", "UDP: datagram refused by the kernel, dropped",
                        "msg2",         "%s", "🌐UDP: datagram refused by the kernel, dropped",
                        "url",          "%s", gobj_read_str_attr(gobj, "url"),
                        "remote-addr",  "%s", peername,
                        "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                        "errno",        "%d", -yev_get_result(yev_event),
                        "strerror",     "%s", strerror(-yev_get_result(yev_event)),
                        NULL
                    );
                }

                /*
                 *  The next datagram. Up to 7.25.4 this asked for
                 *  ST_CONNECTED, a state C_UDP_S does not have: the
                 *  first datagram was sent, and every later one
                 *  waited in the queue for ever.
                 */
                if(is_txing && gobj_in_this_state(gobj, ST_IDLE)) {
                    try_more_writes(gobj);
                }
                yev_destroy_event(yev_event);
            }
            break;

        default:
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "UDP: event type NOT IMPLEMENTED",
                "msg2",         "%s", "🌐UDP: event type NOT IMPLEMENTED",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                "remote-addr",  "%s", peername,
                "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                "event_type",   "%s", yev_event_type_name(yev_event),
                "p",            "%p", yev_event,
                NULL
            );
            break;
    }

    return 0;
}

/***************************************************************************
 *  Read the current 'crypto' attribute and invoke ytls_reload_certificates().
 *  Returns 0 on success, -1 on failure (old ytls kept intact).
 ***************************************************************************/
PRIVATE int reload_ytls_from_attrs(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->use_ssl || !priv->ytls) {
        return -1;
    }

    /*
     *  `crypto` is DTP_JSON with a null default, so an unset one arrives as
     *  json_null(), a valid pointer: `if(!jn_crypto)` was dead code and the
     *  reload went on to hand a json null to ytls.
     */
    json_t *jn_crypto = gobj_read_json_attr(gobj, "crypto");
    if(empty_json(jn_crypto)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "reload-certs: 'crypto' attribute is empty",
            NULL
        );
        return -1;
    }

    uint32_t trace_level = gobj_trace_level(gobj);
    json_object_set_new(
        jn_crypto,
        "trace_tls",
        json_boolean(priv->trace_tls || (trace_level & TRACE_TLS))
    );

    int ret = ytls_reload_certificates(priv->ytls, jn_crypto);
    if(ret == 0) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "TLS certificates reloaded successfully",
            "url",          "%s", priv->url ? priv->url : "",
            NULL
        );
    }
    return ret;
}




                    /***************************
                     *      Commands
                     ***************************/




/***************************************************************************
 *  Command: reload-certs
 ***************************************************************************/
PRIVATE json_t *cmd_reload_certs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->use_ssl || !priv->ytls) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("Listener is not TLS-enabled or not running"),
            0,
            0,
            kw
        );
    }

    int ret = reload_ytls_from_attrs(gobj);
    if(ret < 0) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("reload-certs FAILED (check logs)"),
            0,
            0,
            kw
        );
    }

    return msg_iev_build_response(gobj,
        0,
        json_sprintf("TLS certificates reloaded"),
        0,
        0,
        kw
    );
}

/***************************************************************************
 *  Command: view-cert
 ***************************************************************************/
PRIVATE json_t *cmd_view_cert(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->use_ssl || !priv->ytls) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("Listener is not TLS-enabled or not running"),
            0,
            0,
            kw
        );
    }

    json_t *info = ytls_get_cert_info(priv->ytls);
    if(!info) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("No certificate info available"),
            0,
            0,
            kw
        );
    }

    json_int_t not_after = kw_get_int(gobj, info, "not_after", 0, 0);
    if(not_after > 0) {
        time_t now = time(NULL);
        json_int_t days = (not_after - (json_int_t)now) / 86400;
        json_object_set_new(info, "days_remaining", json_integer(days));
    }
    json_object_set_new(info, "url", json_string(priv->url ? priv->url : ""));

    return msg_iev_build_response(gobj, 0, 0, 0, info, kw);
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  udp_channel is "ip:port" and it's in the label of gbuff.
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

    if(!priv->gbuf_txing) {
        priv->gbuf_txing = gbuffer_incref(gbuf);
        if(write_data(gobj) < 0) {
            try_more_writes(gobj);  // Error already logged: this one is dropped
        }
    } else {
        enqueue_write(gobj, gbuffer_incref(gbuf));
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
        command_table,
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
