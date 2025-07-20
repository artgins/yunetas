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
PRIVATE int uv_udp_set_broadcast(int fd, int on);
PRIVATE int send_data(hgobj gobj, gbuffer_t *gbuf);
PRIVATE int get_sock_name(hgobj gobj);

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
SDATA (DTP_DICT,        "child_tree_filter",    SDF_RD,  0, "tree of children to create on new accept, legacy method"),

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
    BOOL use_ssl;
    // uv_udp_t uv_udp;
    // uv_udp_send_t req_send;
    //
    // ip_port ipp_sockname;

    dl_list_t dl_tx;
    gbuffer_t *gbuf_txing;
    BOOL trace_tls;
    json_t *child_tree_filter;

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
    SET_PRIV(child_tree_filter, gobj_read_json_attr)

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
    priv->yev_server_accept = yev_create_accept_event(
        yuno_event_loop(),
        yev_callback,
        priv->url,    // server_url,
        0,      //backlog,
        0,      // shared
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


    uv_udp_set_broadcast(yev_get_fd(priv->yev_server_accept), 0);
    gobj_write_str_attr(gobj, "lHost", host);
    gobj_write_str_attr(gobj, "lPort", port);

    get_sock_name(gobj);

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

    if(json_object_size(priv->child_tree_filter) > 0) {
        /*--------------------------------*
         *      Legacy method
         *--------------------------------*/
        yev_start_event(priv->yev_server_accept);

    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "NEW method not implemented",
            "url",          "%s", priv->url,
            NULL
        );
    }
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_UV) {
        log_debug_printf(0, ">>> uv_udp_recv_stop p=%p", &priv->uv_udp);
    }
    uv_udp_recv_stop((uv_udp_t *)&priv->uv_udp);

    if(gobj_trace_level(gobj) & TRACE_UV) {
        log_debug_printf(0, ">>> uv_close updS p=%p", &priv->uv_udp);
    }
    gobj_change_state(gobj, ST_WAIT_STOPPED);
    uv_close((uv_handle_t *)&priv->uv_udp, on_close_cb);

    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int uv_udp_set_broadcast(int fd, int on)
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
 *
 ***************************************************************************/
PRIVATE int get_sock_name(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    get_udp_sock_name(&priv->uv_udp, &priv->ipp_sockname);

    char url[60];
    get_ipp_url(&priv->ipp_sockname, url, sizeof(url));
    gobj_write_str_attr(gobj, "sockname", url);

    return 0;
}

/***************************************************************************
 *  on alloc callback
 ***************************************************************************/
PRIVATE void on_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
    hgobj gobj = handle->data;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // TODO: OPTIMIZE to use few memory
    buf->base = priv->bfinput;
    buf->len = sizeof(priv->bfinput);
}

/***************************************************************************
 *  on read callback
 ***************************************************************************/
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




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  udp_channel is "ip:port" and it's in the label of gbuff.
 ***************************************************************************/
PRIVATE int ac_tx_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);

    send_data(gobj, gbuf);

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
GOBJ_DEFINE_GCLASS(GCLASS_UDP_S);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_TX_DATA);

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
        {EV_STOPPED,            0},
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
    return create_gclass(GCLASS_UDP_S);
}
