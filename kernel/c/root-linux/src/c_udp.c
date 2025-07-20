/***********************************************************************
 *          C_UDP0.C
 *          Udp0 GClass.
 *
 *          GClass of UDP level 0 uv-mixin
 *
 *          Copyright (c) 2020 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
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
PRIVATE int send_data(hgobj gobj, gbuffer_t *gbuf);
PRIVATE int get_sock_name(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name--------------------flag--------default-description---------- */
SDATA (DTP_INTEGER,     "connxs",               SDF_RD,     0,      "Current connections"),
SDATA (DTP_INTEGER,     "txBytes",              SDF_RD,     0,      "Bytes transmitted by this socket"),
SDATA (DTP_INTEGER,     "rxBytes",              SDF_RD,     0,      "Bytes received by this socket"),
SDATA (DTP_STRING,      "lHost",                SDF_RD,     0,      "local ip"),
SDATA (DTP_STRING,      "lPort",                SDF_RD,     0,      "local port"),
SDATA (DTP_STRING,      "rHost",                SDF_RD,     0,      "remote ip"),
SDATA (DTP_STRING,      "rPort",                SDF_RD,     0,      "remote port"),
SDATA (DTP_STRING,      "peername",             SDF_RD,     0,      "Peername"),
SDATA (DTP_STRING,      "sockname",             SDF_RD,     0,      "Sockname"),
SDATA (DTP_POINTER,     "user_data",            0,          0,      "user data"),
SDATA (DTP_POINTER,     "user_data2",           0,          0,      "more user data"),
SDATA (DTP_POINTER,     "subscriber",           0,          0,      "subscriber of output-events. If it's null then subscriber is the parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_CONNECT_DISCONNECT    = 0x0001,
    TRACE_DUMP_TRAFFIC          = 0x0002,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"connections",         "Trace connections and disconnections"},
{"traffic",             "Trace dump traffic"},
{0, 0},
};


/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
#define BFINPUT_SIZE (2*1024)

typedef struct _PRIVATE_DATA {
    // Data oid
    uint64_t *ptxBytes;
    uint64_t *prxBytes;

    // uv_udp_t uv_udp;
    // uv_udp_send_t req_send;

    const char *lHost;
    const char *lPort;
    const char *rHost;
    const char *rPort;

    const char *peername;
    const char *sockname;
    // ip_port ipp_sockname;
    // ip_port ipp_peername;
    //
    // struct sockaddr raddr;

    dl_list_t dl_tx;
    gbuffer_t *gbuf_txing;

    char bfinput[BFINPUT_SIZE];

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
    SET_PRIV(peername,              gobj_read_str_attr)
    SET_PRIV(sockname,              gobj_read_str_attr)
    SET_PRIV(lHost,                 gobj_read_str_attr)
    SET_PRIV(lPort,                 gobj_read_str_attr)
    SET_PRIV(rHost,                 gobj_read_str_attr)
    SET_PRIV(rPort,                 gobj_read_str_attr)

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

    IF_EQ_SET_PRIV(sockname,                      gobj_read_str_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uv_loop_t *loop = yuno_uv_event_loop();
    struct addrinfo hints;
    struct addrinfo *res;
    int r;

    uv_udp_init(loop, &priv->uv_udp);
    priv->uv_udp.data = gobj;
    uv_udp_set_broadcast(&priv->uv_udp, 0);

    /*
     *  Bind if local ip
     */
    if(!empty_string(priv->lHost)) {
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;  /* Allow IPv4 or IPv6 */
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
        hints.ai_flags = 0;

        r = getaddrinfo(
            priv->lHost,
            priv->lPort,
            &hints,
            &res
        );
        if(r!=0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "getaddrinfo() FAILED",
                "lHost",        "%s", priv->lHost,
                "lPort",        "%s", priv->lPort,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            return -1;
        }

        r = uv_udp_bind(&priv->uv_udp, res->ai_addr, 0);
        freeaddrinfo(res);
        if(r<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "uv_udp_bind() FAILED",
                "lHost",        "%s", priv->lHost,
                "lPort",        "%s", priv->lPort,
                "uv_error",     "%s", uv_err_name(r),
                NULL
            );
            return -1;
        }
    }

    /*
     *  Set remote addr for quick transmit
     */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;  /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = 0;

    r = getaddrinfo(
        priv->rHost,
        priv->rPort,
        &hints,
        &res
    );
    if(r!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "getaddrinfo() FAILED",
            "rHost",        "%s", priv->rHost,
            "rPort",        "%s", priv->rPort,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    memcpy(&priv->raddr, res->ai_addr, sizeof(priv->raddr));
    freeaddrinfo(res);

    r = uv_udp_connect((uv_udp_t *)&priv->uv_udp, &priv->raddr);
    if(r!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "uv_udp_connect() FAILED",
            "rHost",        "%s", priv->rHost,
            "rPort",        "%s", priv->rPort,
            "uv_error",     "%s", uv_err_name(r),
            NULL
        );
        return -1;
    }
    get_sock_name(gobj);

    /*
     *  Info of "connecting..."
     */
    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "UDP Connecting...",
            "lHost",        "%s", priv->lHost,
            "lPort",        "%s", priv->lPort,
            "rHost",        "%s", priv->rHost,
            "rPort",        "%s", priv->rPort,
            NULL
        );
    }

    if(gobj_trace_level(gobj) & TRACE_UV) {
        log_debug_printf(0, ">>>start_read(%s:%s)",
            gobj_gclass_name(gobj), gobj_name(gobj)
        );
    }
    r = uv_udp_recv_start(&priv->uv_udp, on_alloc_cb, on_read_cb);
    if(r!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "uv_udp_recv_start() FAILED",
            "lHost",        "%s", priv->lHost,
            "lPort",        "%s", priv->lPort,
            "rHost",        "%s", priv->rHost,
            "rPort",        "%s", priv->rPort,
            "uv_error",     "%s", uv_err_name(r),
            NULL
        );
    }

    gobj_change_state(gobj, ST_IDLE);

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
    uv_udp_connect((uv_udp_t *)&priv->uv_udp, NULL); // null to disconnect

    if(gobj_trace_level(gobj) & TRACE_UV) {
        log_debug_printf(0, ">>> uv_close updS p=%p", &priv->uv_udp);
    }
    gobj_change_state(gobj, ST_WAIT_STOPPED);
    uv_close((uv_handle_t *)&priv->uv_udp, on_close_cb);

    return 0;
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
            "msgset",       "%s", MSGSET_LIBUV_ERROR,
            "msg",          "%s", "GObj NOT STOPPED. UV handler ACTIVE!",
            NULL
        );
    }

    GBUFFER_DECREF(priv->gbuf_txing);
    dl_flush(&priv->dl_tx, (fnfree)gbuffer_decref);
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
    gobj_change_state(gobj, ST_STOPPED);

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
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBUV_ERROR,
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

    if(gobj_trace_level(gobj) & TRACE_DUMP_TRAFFIC) {
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
                "msgset",       "%s", MSGSET_MEMORY_ERROR,
                "msg",          "%s", "no memory for gbuf",
                "size",         "%d", nread,
                NULL);
            return;
        }
        gbuffer_append(gbuf, buf->base, nread);
        gbuf_setlabel(gbuf, peername);
        json_t *kw = json_pack("{s:I}",
            "gbuffer", (json_int_t)(size_t)gbuf
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
            "msgset",       "%s", MSGSET_LIBUV_ERROR,
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
        //    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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

    if(gobj_trace_level(gobj) & TRACE_DUMP_TRAFFIC) {
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
            "msgset",       "%s", MSGSET_LIBUV_ERROR,
            "msg",          "%s", "uv_udp_send() FAILED",
            "uv_error",     "%s", uv_err_name(ret),
            "rHost",        "%s", priv->rHost,
            "rPort",        "%s", priv->rPort,
            NULL
        );
    }

    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
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
GOBJ_DEFINE_GCLASS(GCLASS_UDP);

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
PUBLIC int register_c_udp(void)
{
    return create_gclass(GCLASS_UDP);
}
