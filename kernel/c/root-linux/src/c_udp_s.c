/***********************************************************************
 *          C_UDP_S0.C
 *          GClass of UDP server level 0 uv-mixin.
 *
 *          Mixin uv-gobj
 *
 *          Copyright (c) 2014-2021 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <regex.h>
#include "c_udp_s0.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void on_close_cb(uv_handle_t* handle);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
SDATA (ASN_OCTET_STR,   "url",                  SDF_RD,  0, "url of udp server"),
SDATA (ASN_OCTET_STR,   "lHost",                SDF_RD,  0, "Local ip, got from url"),
SDATA (ASN_OCTET_STR,   "lPort",                SDF_RD,  0, "Local port, got from url."),
SDATA (ASN_OCTET_STR,   "sockname",             SDF_RD,  0, "Sockname"),
SDATA (ASN_COUNTER64,   "txBytes",              SDF_RD,  0, "Bytes transmitted by this socket"),
SDATA (ASN_COUNTER64,   "rxBytes",              SDF_RD,  0, "Bytes received by this socket"),
SDATA (ASN_OCTET_STR,   "stopped_event_name",   SDF_RD,  "EV_STOPPED", "Stopped event name"),
SDATA (ASN_OCTET_STR,   "tx_ready_event_name",  SDF_RD,  "EV_TX_READY", "Must be empty if you don't want receive this event"),
SDATA (ASN_OCTET_STR,   "rx_data_event_name",   SDF_RD,  "EV_RX_DATA", "Must be empty if you don't want receive this event"),
SDATA (ASN_BOOLEAN,     "exitOnError",          SDF_RD,  1, "Exit if Listen failed"),
SDATA (ASN_POINTER,     "user_data",            0,  0, "user data"),
SDATA (ASN_POINTER,     "user_data2",           0,  0, "more user data"),
SDATA (ASN_POINTER,     "subscriber",           0,  0, "subscriber of output-events. Default if null is parent."),
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
    const char *tx_ready_event_name;
    const char *rx_data_event_name;
    BOOL exitOnError;

    // Data oid
    uint64_t *ptxBytes;
    uint64_t *prxBytes;

    uv_udp_t uv_udp;
    uv_udp_send_t req_send;

    ip_port ipp_sockname;
    const char *sockname;

    dl_list_t dl_tx;
    GBUFFER *gbuf_txing;

    char bfinput[BFINPUT_SIZE];

} PRIVATE_DATA;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void on_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
PRIVATE void on_read_cb(
    uv_udp_t* handle,
    ssize_t nread,
    const uv_buf_t* buf,
    const struct sockaddr* addr,
    unsigned flags
);
PRIVATE int send_data(hgobj gobj, GBUFFER *gbuf);
PRIVATE int get_sock_name(hgobj gobj);




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
    SET_PRIV(url,                       gobj_read_str_attr)
    SET_PRIV(exitOnError,               gobj_read_bool_attr)
    SET_PRIV(tx_ready_event_name, gobj_read_str_attr)
    SET_PRIV(rx_data_event_name,        gobj_read_str_attr)
    SET_PRIV(sockname,                  gobj_read_str_attr)

    priv->ptxBytes = gobj_danger_attr_ptr(gobj, "txBytes");
    priv->prxBytes = gobj_danger_attr_ptr(gobj, "rxBytes");

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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(sockname,                  gobj_read_str_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_in_this_state(gobj, "ST_STOPPED")) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBUV_ERROR,
            "msg",          "%s", "GObj NOT STOPPED. UV handler ACTIVE!",
            NULL
        );
    }

    GBUF_DECREF(priv->gbuf_txing);
    dl_flush(&priv->dl_tx, (fnfree)gbuf_decref);
}

/***************************************************************************
 *      Framework Method start - return nonstart flag
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uv_loop_t *loop = yuno_uv_event_loop();
    struct addrinfo hints;
    int r;

    if(!priv->url) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
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
    r = parse_http_url(
        priv->url,
        schema,
        sizeof(schema),
        host,
        sizeof(host),
        port,
        sizeof(port),
        FALSE
    );
    if(r<0) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "parse_http_url() FAILED",
            "url",          "%s", priv->url,
            NULL
        );
        if(priv->exitOnError) {
            exit(0); //WARNING exit with 0 to stop daemon watcher!
        } else {
            return -1;
        }
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;  /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = 0;
    struct addrinfo *res;

    r = getaddrinfo(
        host,
        port,
        &hints,
        &res
    );
    if(r!=0) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "getaddrinfo() FAILED",
            "lHost",        "%s", host,
            "lPort",        "%s", port,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        if(priv->exitOnError) {
            exit(0); //WARNING exit with 0 to stop daemon watcher!
        } else {
            return -1;
        }
    }

    if(gobj_trace_level(gobj) & TRACE_UV) {
        log_debug_printf(0, ">>> uv_init udpS p=%p", &priv->uv_udp);
    }
    uv_udp_init(loop, &priv->uv_udp);
    priv->uv_udp.data = gobj;
    uv_udp_set_broadcast(&priv->uv_udp, 0);

    r = uv_udp_bind(&priv->uv_udp, res->ai_addr, 0);
    freeaddrinfo(res);
    if(r<0) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "uv_udp_bind() FAILED",
            "url",          "%s", priv->url,
            "uv_error",     "%s", uv_err_name(r),
            NULL
        );
        if(priv->exitOnError) {
            exit(0); // WARNING exit with 0 to stop daemon watcher!
        } else {
            return -1;
        }
    }
    gobj_write_str_attr(gobj, "lHost", host);
    gobj_write_str_attr(gobj, "lPort", port);

    get_sock_name(gobj);

    /*
     *  Info of "listening..."
     */
    log_info(0,
        "gobj",         "%s", gobj_full_name(gobj),
        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
        "msg",          "%s", "UDP listening ...",
        "url",          "%s", priv->url,
        "lHost",        "%s", host,
        "lPort",        "%s", port,
        "sockname",     "%s", priv->sockname,
        NULL
    );

    if(gobj_trace_level(gobj) & TRACE_UV) {
        log_debug_printf(0, ">>> start_read udp p=%p", &priv->uv_udp);
    }
    uv_udp_recv_start(&priv->uv_udp, on_alloc_cb, on_read_cb);
    gobj_change_state(gobj, "ST_IDLE");
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
    gobj_change_state(gobj, "ST_WAIT_STOPPED");
    uv_close((uv_handle_t *)&priv->uv_udp, on_close_cb);

    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void on_close_cb(uv_handle_t* handle)
{
    hgobj gobj = handle->data;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_UV) {
        log_debug_printf(0, "<<< on_close_cb udp_s0 p=%p",
            &priv->uv_udp
        );
    }
    gobj_change_state(gobj, "ST_STOPPED");

    /*
     *  Only NOW you can destroy this gobj,
     *  when uv has released the handler.
     */
    const char *stopped_event_name = gobj_read_str_attr(
        gobj,
        "stopped_event_name"
    );
    if(!empty_string(stopped_event_name)) {
        gobj_send_event(
            gobj_parent(gobj),
            stopped_event_name ,
            0,
            gobj
        );
    }
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
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
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
        log_debug_dump(
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
            GBUFFER *gbuf = gbuf_create(nread, nread, 0,0);
            if(!gbuf) {
                log_error(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MEMORY_ERROR,
                    "msg",          "%s", "no memory for gbuf",
                    "size",         "%d", nread,
                    NULL);
                return;
            }
            gbuf_append(gbuf, buf->base, nread);
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
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBUV_ERROR,
            "msg",          "%s", "upd send FAILED",
            "uv_error",     "%s", uv_err_name(status),
            NULL);
    }

    size_t ln = gbuf_chunk(priv->gbuf_txing);
    if(ln) {
        send_data(gobj, 0);  // continue with gbuf_txing
        return;

    } else {
        // Remove curr txing and get the next
        gbuf_decref(priv->gbuf_txing);
        priv->gbuf_txing = 0;

        GBUFFER *gbuf = dl_first(&priv->dl_tx);
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
PRIVATE int send_data(hgobj gobj, GBUFFER *gbuf)
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
    const char *udp_channel = gbuf_getlabel(gbuf);
    if(!udp_channel) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "udp_channel NULL",
            NULL
        );
        GBUF_DECREF(priv->gbuf_txing);
        return -1;
    }

    size_t ln = gbuf_chunk(gbuf);
    if(ln > 1500) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "UPD lenght must not be greater than 1500",
            "ln",           "%d", ln,
            NULL
        );
        ln = 1500;
    }
    char *bf = gbuf_get(gbuf, ln);

    uv_buf_t b[] = {
        {.base = bf, .len = ln}
    };

    priv->req_send.data = gobj;

    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        log_debug_dump(
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
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
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
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
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
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
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
    GBUFFER *gbuf = (GBUFFER *)(size_t)kw_get_int(kw, "gbuffer", 0, FALSE);

    send_data(gobj, gbuf);

    JSON_DECREF(kw);
    return 1;
}


/***************************************************************************
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = {
    {"EV_TX_DATA",      0},
    {NULL, 0}
};
PRIVATE const EVENT output_events[] = {
    {"EV_STOPPED",      0},
    {"EV_RX_DATA",      0},
    {"EV_TX_READY",     0},
    {NULL, 0}
};
PRIVATE const char *state_names[] = {
    "ST_STOPPED",
    "ST_WAIT_STOPPED",
    "ST_IDLE",          /* H2UV handler for UV */
    NULL
};

PRIVATE EV_ACTION ST_STOPPED[] = {
    {0,0,0}
};

PRIVATE EV_ACTION ST_WAIT_STOPPED[] = {
    {0,0,0}
};

PRIVATE EV_ACTION ST_IDLE[] = {
    {"EV_TX_DATA",        ac_tx_data,       0},
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_STOPPED,
    ST_WAIT_STOPPED,
    ST_IDLE,
    NULL
};

PRIVATE FSM fsm = {
    input_events,
    output_events,
    state_names,
    states,
};

/***************************************************************************
 *              GClass
 ***************************************************************************/
/*---------------------------------------------*
 *              Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {0, 0, 0}
};

/*---------------------------------------------*
 *              GClass
 *---------------------------------------------*/
PRIVATE GCLASS _gclass = {
    0,  // base
    GCLASS_UDP_S0_NAME,     // CHANGE WITH each gclass
    &fsm,
    {
        mt_create,
        0, //mt_create2,
        mt_destroy,
        mt_start,
        mt_stop,
        0, //mt_play,
        0, //mt_pause,
        mt_writing,
        0, //mt_reading,
        0, //mt_subscription_added,
        0, //mt_subscription_deleted,
        0, //mt_child_added,
        0, //mt_child_removed,
        0, //mt_stats,
        0, //mt_command,
        0, //mt_inject_event,
        0, //mt_create_resource,
        0, //mt_list_resource,
        0, //mt_save_resource,
        0, //mt_delete_resource,
        0, //mt_future21
        0, //mt_future22
        0, //mt_get_resource
        0, //mt_state_changed,
        0, //mt_authenticate,
        0, //mt_list_childs,
        0, //mt_stats_updated,
        0, //mt_disable,
        0, //mt_enable,
        0, //mt_trace_on,
        0, //mt_trace_off,
        0, //mt_gobj_created,
        0, //mt_future33,
        0, //mt_future34,
        0, //mt_publish_event,
        0, //mt_publication_pre_filter,
        0, //mt_publication_filter,
        0, //mt_authz_checker,
        0, //mt_future39,
        0, //mt_create_node,
        0, //mt_update_node,
        0, //mt_delete_node,
        0, //mt_link_nodes,
        0, //mt_future44,
        0, //mt_unlink_nodes,
        0, //mt_topic_jtree,
        0, //mt_get_node,
        0, //mt_list_nodes,
        0, //mt_shoot_snap,
        0, //mt_activate_snap,
        0, //mt_list_snaps,
        0, //mt_treedbs,
        0, //mt_treedb_topics,
        0, //mt_topic_desc,
        0, //mt_topic_links,
        0, //mt_topic_hooks,
        0, //mt_node_parents,
        0, //mt_node_childs,
        0, //mt_list_instances,
        0, //mt_node_tree,
        0, //mt_topic_size,
        0, //mt_future62,
        0, //mt_future63,
        0, //mt_future64
    },
    lmt,
    tattr_desc,
    sizeof(PRIVATE_DATA),
    0,  // acl
    s_user_trace_level,
    0, // cmds
    0, // gcflag
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC GCLASS *gclass_udp_s0(void)
{
    return &_gclass;
}
