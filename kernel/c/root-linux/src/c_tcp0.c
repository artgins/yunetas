/***********************************************************************
 *          C_TCP0.C
 *          GClass of TCP level 0 uv-mixin.
 *
 *          Copyright (c) 2013 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "c_tcp0.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name------------------------flag------------default---------description---------- */
SDATA (ASN_INTEGER,     "kpAlive",                  SDF_RD,         60,             "keep-alive in seconds. 0 disable."),
SDATA (ASN_COUNTER64,   "txBytes",                  SDF_RD,         0,              "Bytes transmitted by this socket"),
SDATA (ASN_COUNTER64,   "rxBytes",                  SDF_RD,         0,              "Bytes received by this socket"),
SDATA (ASN_COUNTER64,   "txMsgs",                   SDF_RD,         0,              "Messages transmitted by this socket"),
SDATA (ASN_COUNTER64,   "rxMsgs",                   SDF_RD,         0,              "Messages received by this socket"),
SDATA (ASN_OCTET_STR,   "lHost",                    SDF_RD,         0,              "local ip"),
SDATA (ASN_OCTET_STR,   "lPort",                    SDF_RD,         0,              "local port"),
SDATA (ASN_OCTET_STR,   "rHost",                    SDF_RD,         0,              "remote ip"),
SDATA (ASN_OCTET_STR,   "rPort",                    SDF_RD,         0,              "remote port"),
SDATA (ASN_OCTET_STR,   "peername",                 SDF_RD,         0,              "Peername"),
SDATA (ASN_OCTET_STR,   "sockname",                 SDF_RD,         0,              "Sockname"),
SDATA (ASN_OCTET_STR,   "connected_event_name",     SDF_RD,         "EV_CONNECTED", "Must be empty if you don't want receive this event"),
SDATA (ASN_OCTET_STR,   "disconnected_event_name",  SDF_RD,         "EV_DISCONNECTED", "Must be empty if you don't want receive this event"),
SDATA (ASN_OCTET_STR,   "tx_ready_event_name",      SDF_RD,         "EV_TX_READY",  "Must be empty if you don't want receive this event"),
SDATA (ASN_OCTET_STR,   "rx_data_event_name",       SDF_RD,         "EV_RX_DATA",   "Must be empty if you don't want receive this event"),
SDATA (ASN_OCTET_STR,   "stopped_event_name",       SDF_RD,         "EV_STOPPED",   "Stopped event name"),
SDATA (ASN_UNSIGNED,    "max_tx_queue",             SDF_WR,         0,              "Maximum messages in tx queue. Default is 0: no limit."),
SDATA (ASN_COUNTER64,   "cur_tx_queue",             SDF_RD,         0,              "Current messages in tx queue"),
SDATA (ASN_BOOLEAN,     "__clisrv__",               SDF_RD,         0,              "Client of tcp server"),
SDATA (ASN_BOOLEAN,     "output_priority",          SDF_RD|SDF_WR,  0,              "Make output priority"),
SDATA (ASN_POINTER,     "user_data",                0,              0,              "user data"),
SDATA (ASN_POINTER,     "user_data2",               0,              0,              "more user data"),
SDATA (ASN_POINTER,     "subscriber",               0,              0,              "subscriber of output-events. Default if null is parent."),
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
typedef struct _PRIVATE_DATA {
    uv_tcp_t uv_socket;
    uv_connect_t uv_req_connect;
    uv_write_t uv_req_write;
    /* Write request type. Careful attention must be paid when reusing objects of this type.
     * When a stream is in non-blocking mode, write requests sent with uv_write will be queued.
     * Reusing objects at this point is undefined behaviour.
     * It is safe to reuse the uv_write_t object only after the callback passed to uv_write is fired.
     */
    uv_shutdown_t uv_req_shutdown;
    char uv_handler_active;
    char uv_read_active;
    char uv_req_connect_active;
    char uv_req_write_active;
    char uv_req_shutdown_active;
    BOOL inform_disconnection;

    // Conf
    BOOL output_priority;
    const char *connected_event_name;
    const char *tx_ready_event_name;
    const char *rx_data_event_name;
    const char *disconnected_event_name;
    const char *stopped_event_name;
    int kpAlive;
    uint32_t max_tx_queue;
    uint64_t *ptxBytes;
    uint64_t *prxBytes;
    uint64_t *ptxMsgs;
    uint64_t *prxMsgs;

    const char *lHost;
    const char *lPort;
    const char *rHost;
    const char *rPort;

    const char *peername;
    const char *sockname;
    ip_port ipp_sockname;
    ip_port ipp_peername;

    dl_list_t dl_tx;
    GBUFFER *gbuf_txing;

    grow_buffer_t bfinput;
} PRIVATE_DATA;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void do_close(hgobj gobj);
PRIVATE void set_connected(hgobj gobj);
PRIVATE void set_disconnected(hgobj gobj, const char *cause);
PRIVATE int get_peer_and_sock_name(hgobj gobj);
PRIVATE void on_connect_cb(uv_connect_t* req, int status);
PRIVATE int do_connect(hgobj gobj);
PRIVATE void on_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
PRIVATE void on_read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
PRIVATE void on_shutdown_cb(uv_shutdown_t* req, int status);
PRIVATE void do_shutdown(hgobj gobj);
PRIVATE int do_write(hgobj gobj, GBUFFER *gbuf);
PRIVATE int try_write_all(hgobj gobj, BOOL inform_tx_ready);




            /***************************
             *      Framework Methods
             ***************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    dl_init(&priv->dl_tx);

    priv->ptxBytes = gobj_danger_attr_ptr(gobj, "txBytes");
    priv->prxBytes = gobj_danger_attr_ptr(gobj, "rxBytes");
    priv->ptxMsgs = gobj_danger_attr_ptr(gobj, "txMsgs");
    priv->prxMsgs = gobj_danger_attr_ptr(gobj, "rxMsgs");

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(connected_event_name,          gobj_read_str_attr)
    SET_PRIV(tx_ready_event_name,           gobj_read_str_attr)
    SET_PRIV(rx_data_event_name,            gobj_read_str_attr)
    SET_PRIV(disconnected_event_name,       gobj_read_str_attr)
    SET_PRIV(stopped_event_name,            gobj_read_str_attr)
    SET_PRIV(kpAlive,                       gobj_read_int32_attr)
    SET_PRIV(peername,                      gobj_read_str_attr)
    SET_PRIV(sockname,                      gobj_read_str_attr)
    SET_PRIV(lHost,                         gobj_read_str_attr)
    SET_PRIV(lPort,                         gobj_read_str_attr)
    SET_PRIV(rHost,                         gobj_read_str_attr)
    SET_PRIV(rPort,                         gobj_read_str_attr)
    SET_PRIV(max_tx_queue,                  gobj_read_uint32_attr)
    SET_PRIV(output_priority,               gobj_read_bool_attr)

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

    IF_EQ_SET_PRIV(connected_event_name,            gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(tx_ready_event_name,           gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(rx_data_event_name,            gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(disconnected_event_name,       gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(stopped_event_name,            gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(kpAlive,                       gobj_read_int32_attr)
    ELIF_EQ_SET_PRIV(peername,                      gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(sockname,                      gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(lHost,                         gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(lPort,                         gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(rHost,                         gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(rPort,                         gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(max_tx_queue,                  gobj_read_uint32_attr)
    ELIF_EQ_SET_PRIV(output_priority,               gobj_read_bool_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method reading
 ***************************************************************************/
PRIVATE SData_Value_t mt_reading(hgobj gobj, const char *name, int type, SData_Value_t data)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(strcmp(name, "cur_tx_queue")==0) {
        data.u64 = dl_size(&priv->dl_tx);
    }
    return data;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->uv_handler_active) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "destroying: UV handler ACTIVE!",
            NULL
        );
    }
    if(priv->uv_read_active) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "destroying: UV req_read ACTIVE",
            NULL
        );
    }
    if(priv->uv_req_connect_active) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "destroying: UV req_connect ACTIVE",
            NULL
        );
    }
    if(priv->uv_req_write_active) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "destroying: UV req_write ACTIVE",
            NULL
        );
    }
    if(priv->uv_req_shutdown_active) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "destroying: UV req_shutdown ACTIVE",
            NULL
        );
    }

    /*
     *  Free data
     */
    if(priv->gbuf_txing) {
        gbuf_decref(priv->gbuf_txing);
        priv->gbuf_txing = 0;
    }

    size_t size = dl_size(&priv->dl_tx);
    if(size) {
        gobj_incr_qs(QS_DROP_BY_DOWN, size);
        gobj_decr_qs(QS_OUPUT_QUEUE, size);
        dl_flush(&priv->dl_tx, (fnfree)gbuf_decref);
    }

    growbf_reset(&priv->bfinput);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->uv_handler_active) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "UV handler ALREADY ACTIVE!",
            NULL
        );
        do_close(gobj);
        return -1;
    }

    if(gobj_trace_level(gobj) & TRACE_UV) {
        trace_msg(">>> uv_init tcp p=%p", &priv->uv_socket);
    }
    uv_tcp_init(yuno_uv_event_loop(), &priv->uv_socket);
    priv->uv_socket.data = gobj;
    priv->uv_handler_active = 1;
    uv_tcp_nodelay(&priv->uv_socket, 1);
    uv_tcp_keepalive(&priv->uv_socket, priv->kpAlive?1:0, priv->kpAlive);

    if(!gobj_read_bool_attr(gobj, "__clisrv__")) {
        /*
         * pure tcp client: try to connect
         */
        if(do_connect(gobj)!=0) {
            gobj_stop(gobj);
        }
    }

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    if(gobj_cmp_current_state(gobj, "ST_CONNECTED")>=0) {
        do_shutdown(gobj);
        return 0;
    }
    do_close(gobj);
    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void set_disconnected(hgobj gobj, const char *cause)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Info of "disconnected"
     */
    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
        log_info(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Disconnected",
            "msg2",         "%s", "Disconnected🔴",
            "remote-addr",  "%s", priv->peername?priv->peername:"",
            "local-addr",   "%s", priv->sockname?priv->sockname:"",
            "cause",        "%s", cause,
            NULL
        );
    }

    gobj_change_state(gobj, "ST_STOPPED");

    if(priv->inform_disconnection) {
        priv->inform_disconnection = FALSE;
        gobj_publish_event(gobj, priv->disconnected_event_name, 0);
    }

    if(gobj_read_bool_attr(gobj, "__clisrv__")) {
        gobj_write_str_attr(gobj, "peername", "");
    } else {
        gobj_write_str_attr(gobj, "sockname", "");
    }

    if(gobj_is_volatil(gobj)) {
        gobj_destroy(gobj);
    } else {
        gobj_publish_event(gobj, priv->stopped_event_name, 0);
    }
}

/***************************************************************************
 *  Only NOW you can destroy this gobj,
 *  when uv has released the handler.
 ***************************************************************************/
PRIVATE void on_close_cb(uv_handle_t* handle)
{
    hgobj gobj = handle->data;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_UV) {
        trace_msg("<<< on_close_cb tcp0 p=%p", &priv->uv_socket);
    }
    priv->uv_handler_active = 0;
    priv->uv_req_connect_active = 0;

    set_disconnected(gobj, "on_close_cb");
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void do_close(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->uv_read_active) {
        uv_read_stop((uv_stream_t *)&priv->uv_socket);
        priv->uv_read_active = 0;
    }

    if(!priv->uv_handler_active) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "UV handler NOT ACTIVE 1",
            NULL
        );
        set_disconnected(gobj, "UV handler NOT ACTIVE 1");
        return;
    }

    if(!uv_is_active((uv_handle_t *)&priv->uv_socket)) {
        if(!uv_is_closing((uv_handle_t *)&priv->uv_socket)) {
            if(gobj_trace_level(gobj) & TRACE_UV) {
                trace_msg(">>> uv_close1 tcp p=%p", &priv->uv_socket);
            }
            gobj_change_state(gobj, "ST_WAIT_STOPPED");
            uv_close((uv_handle_t *)&priv->uv_socket, on_close_cb);
        } else {
            priv->uv_handler_active = 0;
            priv->uv_req_connect_active = 0;
            set_disconnected(gobj, "UV handler NOT ACTIVE 2");
        }
        return;
    }

    if(gobj_trace_level(gobj) & TRACE_UV) {
        trace_msg(">>> uv_close2 tcp p=%p", &priv->uv_socket);
    }
    gobj_change_state(gobj, "ST_WAIT_STOPPED");
    uv_close((uv_handle_t *)&priv->uv_socket, on_close_cb);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void set_connected(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->inform_disconnection = TRUE;
    gobj_change_state(gobj, "ST_CONNECTED");

    get_peer_and_sock_name(gobj);

    if(gobj_trace_level(gobj) & TRACE_UV) {
        trace_msg(">>> uv_read_start tcp p=%p", &priv->uv_socket);
    }
    priv->uv_read_active = 1;
    uv_read_start((uv_stream_t*)&priv->uv_socket, on_alloc_cb, on_read_cb);

    /*
     *  Info of "connected"
     */
    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
        log_info(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Connected",
            "msg2",         "%s", "Connected🔵",
            "rHost",        "%s", priv->rHost,
            "rPort",        "%s", priv->rPort,
            "remote-addr",  "%s", priv->peername,
            "local-addr",   "%s", priv->sockname,
            NULL
        );
    }

    try_write_all(gobj, FALSE);

    json_t *kw_ev = json_pack("{s:s, s:s}",
        "peername", priv->peername,
        "sockname", priv->sockname
    );
    // TODO error de diseño, si se cambia connected_event_name la publicación fallará
    // porque el evento no estará en output_events list.
    gobj_publish_event(gobj, priv->connected_event_name, kw_ev);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int get_peer_and_sock_name(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    get_tcp_sock_name(&priv->uv_socket, &priv->ipp_sockname);
    get_tcp_peer_name(&priv->uv_socket, &priv->ipp_peername);

    char url[60];
    get_ipp_url(&priv->ipp_sockname, url, sizeof(url));
    gobj_write_str_attr(gobj, "sockname", url);

    get_ipp_url(&priv->ipp_peername, url, sizeof(url));
    gobj_write_str_attr(gobj, "peername", url);

    return 0;
}

/***************************************************************************
 *  on connect callback
 ***************************************************************************/
PRIVATE void on_connect_cb(uv_connect_t* req, int status)
{
    hgobj gobj = req->data;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->uv_req_connect_active = 0;

    if(gobj_trace_level(gobj) & TRACE_UV) {
        trace_msg("<<< on_connect_cb %d tcp p=%p",
            status,
            &priv->uv_socket
        );
    }

    if (status != 0) {
        if(status == UV_ECONNREFUSED) {
            if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
                log_info(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                    "msg",          "%s", "Connection Refused",
                    "msg2",         "%s", "Connection Refused🔴",
                    "rHost",        "%s", priv->rHost,
                    "rPort",        "%s", priv->rPort,
                    "remote-addr",  "%s", priv->peername,
                    "local-addr",   "%s", priv->sockname,
                    NULL
                );
            }
        } else if(status == UV_ETIMEDOUT) {
            if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
                log_info(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                    "msg",          "%s", "Connection Timeout",
                    "msg2",         "%s", "Connection Timeout🔴",
                    "rHost",        "%s", priv->rHost,
                    "rPort",        "%s", priv->rPort,
                    "remote-addr",  "%s", priv->peername,
                    "local-addr",   "%s", priv->sockname,
                    NULL
                );
            }
        } else if(status == UV_EHOSTUNREACH) {
            if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
                log_info(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                    "msg",          "%s", "Host unreachable",
                    "msg2",         "%s", "Host unreachable🔴",
                    "rHost",        "%s", priv->rHost,
                    "rPort",        "%s", priv->rPort,
                    "remote-addr",  "%s", priv->peername,
                    "local-addr",   "%s", priv->sockname,
                    NULL
                );
            }
        } else if(status == UV_ECANCELED) {
            if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
                log_info(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                    "msg",          "%s", "Canceled",
                    "msg2",         "%s", "Canceled🔴",
                    "rHost",        "%s", priv->rHost,
                    "rPort",        "%s", priv->rPort,
                    "remote-addr",  "%s", priv->peername,
                    "local-addr",   "%s", priv->sockname,
                    NULL
                );
            }
        } else {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBUV_ERROR,
                "msg",          "%s", "connect FAILED",
                "uv_error",     "%s", uv_err_name(status),
                "rHost",        "%s", priv->rHost,
                "rPort",        "%s", priv->rPort,
                NULL
            );
        }
        if(gobj_is_running(gobj)) {
            gobj_stop(gobj); // auto-stop
        }
        return;
    }

    set_connected(gobj);
}

/***************************************************************************
 *  Connect to destination rHost/rPort.
 *  Bind to local ip if lHost is not empty.
 *  rHost/lPort/lHost/lPort must be passed by writing directly in attributes.
 *  WARNING called from mt_start()
 ***************************************************************************/
PRIVATE int do_connect(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    struct addrinfo hints;

    if(priv->uv_req_connect_active) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "uv_req_connect ALREADY ACTIVE",
            NULL
        );
        return -1;
    }

    /*
     *  Info of "connecting..."
     */
    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
        log_info(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "TCP Connecting...",
            "msg2",         "%s", "TCP Connecting...🔜",
            "rHost",        "%s", priv->rHost,
            "rPort",        "%s", priv->rPort,
            "lHost",        "%s", priv->lHost,
            "lPort",        "%s", priv->lPort,
            NULL
        );
    }

    /*
     *  Bind if local ip
     */
    if(!empty_string(priv->lHost)) {
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = 0;
        struct addrinfo *res;

        int r = getaddrinfo(
            priv->lHost,
            priv->lPort,
            &hints,
            &res
        );
        if(r!=0) {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "local getaddrinfo() FAILED",
                "lHost",        "%s", priv->lHost,
                "lPort",        "%s", priv->lPort,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            return -1;
        }

        int ret = uv_tcp_bind(&priv->uv_socket, res->ai_addr, 0);
        if(ret != 0) {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBUV_ERROR,
                "msg",          "%s", "uv_tcp_bind() FAILED",
                "uv_error",     "%s", uv_err_name(ret),
                "lHost",        "%s", priv->lHost,
                "lPort",        "%s", priv->lPort,
                NULL
            );
            // Let continue although bind failed.
        }
        freeaddrinfo(res);
    }

    /*
     *  Resolv remote addr
     */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;
    struct addrinfo *res;

    int r = getaddrinfo(
        priv->rHost,
        priv->rPort,
        &hints,
        &res
    );
    if(r!=0) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "remote getaddrinfo() FAILED",
            "rHost",        "%s", priv->rHost,
            "rPort",        "%s", priv->rPort,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    if(gobj_trace_level(gobj) & TRACE_UV) {
        trace_msg(">>> uv_tcp_connect tcp p=%p", &priv->uv_socket);
    }
    gobj_change_state(gobj, "ST_WAIT_CONNECTED");
    priv->uv_req_connect_active = 1;
    priv->uv_req_connect.data = gobj;
    uv_tcp_connect(
        &priv->uv_req_connect,
        &priv->uv_socket,
        res->ai_addr,
        on_connect_cb
    );
    freeaddrinfo(res);

    return 0;
}

/***************************************************************************
 *  on alloc callback
 ***************************************************************************/
PRIVATE void on_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
    hgobj gobj = handle->data;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    growbf_ensure_size(&priv->bfinput, suggested_size);
    buf->base = priv->bfinput.bf;
    buf->len = priv->bfinput.allocated;
}

/***************************************************************************
 *  on read callback
 ***************************************************************************/
PRIVATE void on_read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
    hgobj gobj = stream->data;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_UV) {
        trace_msg("<<< on_read_cb %d tcp p=%p",
            (int)nread,
            &priv->uv_socket
        );
    }

    if(nread < 0) {
        if(nread == UV_ECONNRESET) {
            if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
                log_info(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                    "msg",          "%s", "Connection Reset",
                    NULL
                );
            }
        } else if(nread == UV_EOF) {
            if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
                log_info(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                    "msg",          "%s", "EOF",
                    NULL
                );
            }
        } else if(nread == UV_ETIMEDOUT) {
            if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
                log_info(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                    "msg",          "%s", "Timeout keep-alive",
                    NULL
                );
            }
        } else {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBUV_ERROR,
                "msg",          "%s", "read FAILED",
                "uv_error",     "%s", uv_err_name(nread),
                NULL
            );
        }
        gobj_change_state(gobj, "ST_WAIT_DISCONNECTED"); // seems like already disconnected
        if(gobj_is_running(gobj)) {
            gobj_stop(gobj); // auto-stop
        }
        return;
    }

    if(nread == 0) {
        // Yes, sometimes arrive with nread 0.
        return;
    }

    (*priv->prxBytes) += nread;
    gobj_incr_qs(QS_RXBYTES, nread);
    (*priv->prxMsgs)++;

    if(gobj_trace_level(gobj) & TRACE_DUMP_TRAFFIC) {
        log_debug_dump(
            0,
            buf->base,
            nread,
            "%s: %s%s%s",
            gobj_short_name(gobj),
            priv->sockname,
            " <- ",
            priv->peername
        );
    }

    // TODO: check is nread is greater than maximum block, and create a overflowable buf
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

    json_t *kw = json_pack("{s:I}",
        "gbuffer", (json_int_t)(size_t)gbuf
    );
    gobj_publish_event(gobj, priv->rx_data_event_name, kw);
}

/***************************************************************************
 *  on shutdown callback
 ***************************************************************************/
PRIVATE void on_shutdown_cb(uv_shutdown_t* req, int status)
{
    hgobj gobj = req->data;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_UV) {
        trace_msg("<<< on_shutdown_cb %d tcp p=%p",
            status,
            &priv->uv_socket
        );
    }

    if(priv->uv_req_write_active) {
        priv->uv_req_write_active = 0;
    }

    priv->uv_req_shutdown_active = 0;
    do_close(gobj);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void do_shutdown(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->uv_req_shutdown_active) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "uv_req_shutdown ALREADY ACTIVE",
            NULL
        );
    }

    /*
     *  Info of "disconnecting..."
     */
    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
        log_info(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "TCP Disconnecting...",
            "rHost",        "%s", priv->rHost,
            "rPort",        "%s", priv->rPort,
            NULL);
    }

    if(priv->uv_read_active) {
        uv_read_stop((uv_stream_t *)&priv->uv_socket);
        priv->uv_read_active = 0;
    }

    gobj_change_state(gobj, "ST_WAIT_DISCONNECTED");
    if(gobj_trace_level(gobj) & TRACE_UV) {
        trace_msg(">>> uv_shutdown tcp p=%p", &priv->uv_socket);
    }
    priv->uv_req_shutdown_active = 1;
    priv->uv_req_shutdown.data = gobj;
    uv_shutdown(
        &priv->uv_req_shutdown,
        (uv_stream_t*)&priv->uv_socket,
        on_shutdown_cb
    );
}

/***************************************************************************
 *  on write callback
 ***************************************************************************/
PRIVATE void on_write_cb(uv_write_t* req, int status)
{
    hgobj gobj = req->data;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->uv_req_write_active = 0;

    if(gobj_trace_level(gobj) & TRACE_UV) {
        trace_msg(">>> on_write_cb status %d, tcp p=%p",
            status,
            &priv->uv_socket
        );
    }

    if(status != 0) {
        if(status == UV_EPIPE) {
            if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
                log_info(0,
                    "gobj",     "%s", gobj_full_name(gobj),
                    "msgset",   "%s", MSGSET_CONNECT_DISCONNECT,
                    "msg",      "%s", "Broken pipe",
                    NULL
                );
            }
        } else if(status == UV_ECONNRESET) {
            if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
                log_info(0,
                    "gobj",     "%s", gobj_full_name(gobj),
                    "msgset",   "%s", MSGSET_CONNECT_DISCONNECT,
                    "msg",      "%s", "Forcibly closed by peer",
                    NULL
                );
            }
        } else {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBUV_ERROR,
                "msg",          "%s", "write: on_write_cb FAILED",
                "uv_error",     "%s", uv_err_name(status),
                NULL
            );
        }
        if(gobj_is_running(gobj)) {
            gobj_change_state(gobj, "ST_WAIT_DISCONNECTED"); // seems like already disconnected
            gobj_stop(gobj); // auto-stop
        }
        return;
    }

    try_write_all(gobj, TRUE);
}

/***************************************************************************
 *  Send data
 ***************************************************************************/
PRIVATE int do_write(hgobj gobj, GBUFFER *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->uv_req_write_active) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "uv_req_write ALREADY ACTIVE",
            NULL
        );
    }
    if(priv->gbuf_txing) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "gbuf_txing NOT NULL",
            NULL
        );
        GBUF_DECREF(priv->gbuf_txing)
    }

    priv->uv_req_write_active = 1;
    priv->uv_req_write.data = gobj;
    priv->gbuf_txing = gbuf;

    size_t ln = gbuf_chunk(gbuf); // TODO y si ln es 0??????????

    char *bf = gbuf_get(gbuf, ln);
    uv_buf_t b[] = {
        {.base = bf, .len = ln}
    };
    uint32_t trace = gobj_trace_level(gobj);
    if((trace & TRACE_UV)) {
        trace_msg(">>> uv_write tcp p=%p, send %d\n", &priv->uv_socket, (int)ln);
    }
    int ret = uv_write(
        &priv->uv_req_write,
        (uv_stream_t*)&priv->uv_socket,
        b,
        1,
        on_write_cb
    );
    if(ret < 0) {
        if(ret == UV_EPIPE) {
            if (gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
                log_info(0,
                    "gobj",     "%s", gobj_full_name(gobj),
                    "msgset",   "%s", MSGSET_CONNECT_DISCONNECT,
                    "msg",      "%s", "Broken pipe",
                    NULL
                );
            }
        } else {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBUV_ERROR,
                "msg",          "%s", "write: uv_write FAILED",
                "uv_error",     "%s", uv_err_name(ret),
                "ln",           "%d", (int)ln,
                NULL
            );
        }
        if(gobj_is_running(gobj)) {
            priv->uv_req_write_active = 0;
            gobj_change_state(gobj, "ST_WAIT_DISCONNECTED"); // seems like already disconnected
            gobj_stop(gobj); // auto-stop
        }
        return -1;
    }
    if((trace & TRACE_DUMP_TRAFFIC)) {
        log_debug_dump(
            LOG_DUMP_OUTPUT,
            bf,
            ln,
            "%s: %s%s%s",
            gobj_short_name(gobj),
            priv->sockname,
            " -> ",
            priv->peername
        );
    }

    (*priv->ptxBytes) += ln;
    gobj_incr_qs(QS_TXBYTES, ln);
    (*priv->ptxMsgs)++;

    gobj_change_state(gobj, "ST_WAIT_TXED");

    return 0;
}

/***************************************************************************
 *  Enqueue data
 ***************************************************************************/
PRIVATE int enqueue_write(hgobj gobj, GBUFFER *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    static int counter = 0;
    size_t size = dl_size(&priv->dl_tx);
    if(priv->max_tx_queue && size >= priv->max_tx_queue) {
        if((counter % priv->max_tx_queue)==0) {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "Tiro mensaje tx",
                "counter",      "%d", (int)counter,
                NULL
            );
        }
        counter++;
        GBUFFER *gbuf_first = dl_first(&priv->dl_tx);
        gobj_incr_qs(QS_DROP_BY_OVERFLOW, 1);
        dl_delete(&priv->dl_tx, gbuf_first, 0);
        gobj_decr_qs(QS_OUPUT_QUEUE, 1);
        gbuf_decref(gbuf_first);
    }

    dl_add(&priv->dl_tx, gbuf);
    gobj_incr_qs(QS_OUPUT_QUEUE, 1);

    return 0;
}

/***************************************************************************
 *  Try write all queue
 ***************************************************************************/
PRIVATE int try_write_all(hgobj gobj, BOOL inform_tx_ready)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_cmp_current_state(gobj, "ST_CONNECTED")<0) {
        /*
         *  Parece que han desconectado
         */
        return 0;
    }
    gobj_change_state(gobj, "ST_WAIT_TXED");

    uint32_t trace = gobj_trace_level(gobj);

    while(1) {
        if(!priv->gbuf_txing) {
            priv->gbuf_txing = dl_first(&priv->dl_tx);
            if(priv->gbuf_txing) {
                dl_delete(&priv->dl_tx, priv->gbuf_txing, 0);
                gobj_decr_qs(QS_OUPUT_QUEUE, 1);
            } else {
                // No more
                break;
            }
        }

        size_t ln = gbuf_chunk(priv->gbuf_txing);
        if(!ln) {
            /*
             *  No more to transmit
             */
            gbuf_decref(priv->gbuf_txing);
            priv->gbuf_txing = 0;
            continue;
        }

        char *bf = gbuf_cur_rd_pointer(priv->gbuf_txing);

        uv_buf_t b[] = {
            {.base = bf, .len = ln}
        };

        int sent = uv_try_write((uv_stream_t*)&priv->uv_socket, b, 1);
        if((trace & TRACE_UV)) {
            trace_msg(">>> uv_try_write tcp p=%p, sent %d\n", &priv->uv_socket, sent);
        }

        if(sent > 0) {
            /*
             *  number of bytes written
             *  (can be less than the supplied buffer size).
             */
            if((trace & TRACE_DUMP_TRAFFIC)) {
                log_debug_dump(
                    0,
                    bf,
                    ln,
                    "%s: %s%s%s",
                    gobj_short_name(gobj),
                    priv->sockname,
                    " -> ",
                    priv->peername
                );
            }

            (*priv->ptxBytes) += sent;
            gobj_incr_qs(QS_TXBYTES, sent);
            (*priv->ptxMsgs)++;

            gbuf_get(priv->gbuf_txing, sent); // removed sent data
            continue;

        } else {
            if(sent == UV_EAGAIN) {
                /*
                 *   Only UV_EAGAIN is permitted
                 *   Change to asynchronous
                 */
                GBUFFER *tx = priv->gbuf_txing;
                priv->gbuf_txing = 0; // Avoid error in do_write()
                return do_write(gobj, tx);  // continue with gbuf_txing
            }
            if(sent == UV_EPIPE) {
                if (gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
                    log_info(0,
                        "gobj",     "%s", gobj_full_name(gobj),
                        "msgset",   "%s", MSGSET_CONNECT_DISCONNECT,
                        "msg",      "%s", "Broken pipe",
                        NULL
                    );
                }
            } else {
                log_error(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_LIBUV_ERROR,
                    "msg",          "%s", "write: uv_try_write FAILED",
                    "uv_error",     "%s", uv_err_name(sent),
                    NULL
                );
            }
            if(gobj_is_running(gobj)) {
                gobj_change_state(gobj, "ST_WAIT_DISCONNECTED"); // seems like already disconnected
                gobj_stop(gobj); // auto-stop
            }
            return -1;
        }
    }

    gobj_change_state(gobj, "ST_CONNECTED");

    if(inform_tx_ready) {
        if(!empty_string(priv->tx_ready_event_name)) {
            gobj_publish_event(gobj, priv->tx_ready_event_name, 0);
        }
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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    GBUFFER *gbuf = (GBUFFER *)(size_t)kw_get_int(kw, "gbuffer", 0, 0);

    gbuf_incref(gbuf); // Quédate una copia

    if(priv->output_priority) {
        /*
         *  Salida prioritaria.
         */
        if(priv->gbuf_txing) {
            log_error(LOG_OPT_TRACE_STACK,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "gbuf_txing NOT NULL",
                NULL
            );
            GBUF_DECREF(priv->gbuf_txing)
        }
        priv->gbuf_txing = gbuf;
        try_write_all(gobj, TRUE);
    } else {
        do_write(gobj, gbuf);
    }


    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_enqueue_tx_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    GBUFFER *gbuf = (GBUFFER *)(size_t)kw_get_int(kw, "gbuffer", 0, 0);

    gbuf_incref(gbuf); // Quédate una copia
    enqueue_write(gobj, gbuf);
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    if(gobj_is_running(gobj)) {
        gobj_stop(gobj);
    }
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_force_drop(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    if(gobj_is_running(gobj)) {
        gobj_stop(gobj);
    }
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = {
    // top input
    {"EV_TX_DATA",              0,  0,  ""},
    {"EV_DROP",                 0,  0,  ""},
    // bottom input
    {"EV_STOPPED",              0,  0,  ""},
    {NULL, 0}
};
PRIVATE const EVENT output_events[] = {
    {"EV_CONNECTED",        0},
    {"EV_TX_READY",         0},
    {"EV_RX_DATA",          0},
    {"EV_DISCONNECTED",     0},
    {"EV_STOPPED",          0},
    {NULL, 0}
};
PRIVATE const char *state_names[] = {
    "ST_STOPPED",
    "ST_WAIT_STOPPED",
    "ST_WAIT_CONNECTED",
    "ST_WAIT_DISCONNECTED", /* order is important. Below the connected states */
    "ST_CONNECTED",
    "ST_WAIT_TXED",
    NULL
};

PRIVATE EV_ACTION ST_STOPPED[] = {
    {"EV_STOPPED",              0,                          0},
    {0,0,0}
};
PRIVATE EV_ACTION ST_WAIT_STOPPED[] = {
    {"EV_STOPPED",              0,                          0},
    {0,0,0}
};
PRIVATE EV_ACTION ST_WAIT_CONNECTED[] = {
    {"EV_DROP",                 ac_drop,                    0},
    {0,0,0}
};
PRIVATE EV_ACTION ST_WAIT_DISCONNECTED[] = {
    {"EV_DROP",                 ac_force_drop,              0}, // HACK no tenemos timeout
                                                                // Father insists
    {"EV_STOPPED",              0,                          0},
    {0,0,0}
};
PRIVATE EV_ACTION ST_CONNECTED[] = {
    {"EV_TX_DATA",      ac_tx_data,             "ST_WAIT_TXED"},
    {"EV_DROP",         ac_drop,                0},
    {0,0,0}
};
PRIVATE EV_ACTION ST_WAIT_TXED[] = {
    {"EV_TX_DATA",      ac_enqueue_tx_data,     "ST_WAIT_TXED"},
    {"EV_DROP",         ac_drop,                0},
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_STOPPED,
    ST_WAIT_STOPPED,
    ST_WAIT_CONNECTED,
    ST_WAIT_DISCONNECTED,
    ST_CONNECTED,
    ST_WAIT_TXED,
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
{0, 0}
};

/*---------------------------------------------*
 *              GClass
 *---------------------------------------------*/
PRIVATE GCLASS _gclass = {
    0,  // base
    GCLASS_TCP0_NAME,
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
        mt_reading,
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
    0,  // cmds
    gcflag_manual_start, // gcflag
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC GCLASS *gclass_tcp0(void)
{
    return &_gclass;
}

/*----------------------------------------------------------------------*
 *      New client of server connected.
 *      Called from on_connection_cb() of c_tcp_s0.c
 *      If return -1 the clisrv gobj is destroyed by c_tcp_s0
 *----------------------------------------------------------------------*/
PUBLIC int accept_connection0(
    hgobj clisrv,
    void *uv_server_socket)
{
    PRIVATE_DATA *priv = gobj_priv_data(clisrv);

    if(!gobj_is_running(clisrv)) {
        if(gobj_trace_level(clisrv) & TRACE_UV) {
            trace_msg(">>> tcp not_accept p=%p", &priv->uv_socket);
        }
        uv_not_accept((uv_stream_t *)uv_server_socket);
        log_error(0,
            "gobj",         "%s", gobj_full_name(clisrv),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "TCP Gobj must be RUNNING",
            NULL
        );
        return -1;
    }

    /*-------------------------------------*
     *      Accept connection
     *-------------------------------------*/
    if(gobj_trace_level(clisrv) & TRACE_UV) {
        trace_msg(">>> tcp accept p=%p", &priv->uv_socket);
    }
    int err = uv_accept((uv_stream_t *)uv_server_socket, (uv_stream_t*)&priv->uv_socket);
    if (err != 0) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(clisrv),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBUV_ERROR,
            "msg",          "%s", "uv_accept FAILED",
            "uv_error",     "%s", uv_err_name(err),
            NULL
        );
        return -1;
    }
    set_connected(clisrv);

    return 0;
}
