/****************************************************************************
 *          c_tcp.c
 *
 *          TCP Transport: tcp,ssl,...
 *          Low level linux with io_uring
 *
 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <parse_url.h>
#include <kwid.h>
#include <ytls.h>
#include <yunetas_ev_loop.h>
#include "c_timer.h"
#include "c_yuno.h"
#include "c_tcp.h"

/*
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
                                            │
                                ┌───────────────────────────┐
                                │       WAIT_TXED           │
                                └───────────────────────────┘
 */

/***************************************************************
 *              Constants
 ***************************************************************/
#define IS_CLI      (!priv->__clisrv__)
#define IS_CLISRV   (priv->__clisrv__)

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE BOOL try_to_stop_yevents(hgobj gobj);
PRIVATE void set_connected(hgobj gobj, int fd);
PRIVATE int yev_callback(yev_event_t *event);

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE const sdata_desc_t tattr_desc[] = { // WARNING repeated in c_tcp/c_esp_transport
/*-ATTR-type--------name----------------flag------------default-----description---------- */
SDATA (DTP_BOOLEAN, "__clisrv__",       SDF_STATS,      "false",    "Client of tcp server"),
SDATA (DTP_JSON,    "crypto",           SDF_RD,         0,          "Crypto config"),
SDATA (DTP_POINTER, "ytls",             0,              0,          "TLS handler"),
SDATA (DTP_POINTER, "fd_clisrv",        0,              0,          "socket fd of clisrv"),

SDATA (DTP_INTEGER, "connxs",           SDF_STATS,      "0",        "connection counter"),
SDATA (DTP_BOOLEAN, "connected",        SDF_VOLATIL|SDF_STATS, "false", "Connection state. Important filter!"),
SDATA (DTP_STRING,  "url",              SDF_RD,         "",         "Url to connect"),
SDATA (DTP_STRING,  "schema",           SDF_RD,         "",         "schema, decoded from url. Set internally"),
SDATA (DTP_BOOLEAN, "use_ssl",          SDF_RD,         "false",    "True if schema is secure. Set internally if client, externally is clisrv"),
SDATA (DTP_STRING,  "cert_pem",         SDF_RD,         "",         "SSL server certification, PEM str format"),
SDATA (DTP_STRING,  "jwt",              SDF_RD,         "",         "TODO. Access with token JWT"),
SDATA (DTP_BOOLEAN, "skip_cert_cn",     SDF_RD,         "true",     "Skip verification of cert common name"),
SDATA (DTP_INTEGER, "keep_alive",       SDF_RD,         "10",       "Set keep-alive if > 0"),
SDATA (DTP_BOOLEAN, "manual",           SDF_RD,         "false",    "Set true if you want connect manually"),

SDATA (DTP_STRING,  "tx_ready_event_name",SDF_RD,       "",         "Legacy attr. Set no-empty if you want EV_TX_READY event"),

SDATA (DTP_INTEGER, "rx_buffer_size",   SDF_WR|SDF_PERSIST, "4096", "Rx buffer size"),
SDATA (DTP_INTEGER, "timeout_between_connections", SDF_WR|SDF_PERSIST, "2000", "Idle timeout to wait between attempts of connection, in miliseconds"),
SDATA (DTP_INTEGER, "timeout_inactivity", SDF_WR|SDF_PERSIST, "-1", "Inactivity timeout in miliseconds to close the connection. Reconnect when new data arrived. With -1 never close."),

SDATA (DTP_INTEGER, "txBytes",          SDF_VOLATIL|SDF_STATS, "0", "Messages transmitted"),
SDATA (DTP_INTEGER, "rxBytes",          SDF_VOLATIL|SDF_STATS, "0", "Messages received"),
SDATA (DTP_INTEGER, "txMsgs",           SDF_VOLATIL|SDF_STATS, "0", "Messages transmitted"),
SDATA (DTP_INTEGER, "rxMsgs",           SDF_VOLATIL|SDF_STATS, "0", "Messages received"),
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
    hgobj gobj_timer;
    BOOL __clisrv__;
    yev_event_t *yev_client_connect;    // Used if not __clisrv__ (pure tcp client)
    yev_event_t *yev_client_rx;
    yev_event_t *yev_client_tx;
    int fd_clisrv;
    int timeout_inactivity;
    char inform_disconnection;

    json_int_t *pconnxs;
    json_int_t *ptxMsgs;
    json_int_t *prxMsgs;
    json_int_t *ptxBytes;
    json_int_t *prxBytes;

    BOOL use_ssl;
    hytls ytls;
    hsskt sskt;

    const char *tx_ready_event_name;
} PRIVATE_DATA;

PRIVATE hgclass __gclass__ = 0;





                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gobj_timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    priv->ptxMsgs = gobj_danger_attr_ptr(gobj, "txMsgs");
    priv->prxMsgs = gobj_danger_attr_ptr(gobj, "rxMsgs");
    priv->ptxBytes = gobj_danger_attr_ptr(gobj, "txBytes");
    priv->prxBytes = gobj_danger_attr_ptr(gobj, "rxBytes");
    priv->pconnxs = gobj_danger_attr_ptr(gobj, "connxs");

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)(size_t)gobj_read_integer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    SET_PRIV(__clisrv__,            gobj_read_bool_attr)
    SET_PRIV(tx_ready_event_name,   gobj_read_str_attr)
    SET_PRIV(timeout_inactivity,    (int)gobj_read_integer_attr)
    SET_PRIV(fd_clisrv,             (int)gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout_inactivity,      (int) gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(tx_ready_event_name,   gobj_read_str_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->gobj_timer);

    gobj_state_t state = gobj_current_state(gobj);
    if(!(state == ST_STOPPED || state == ST_DISCONNECTED)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Initial wrong tcp state",
            "state",        "%s", gobj_current_state(gobj),
            NULL
        );
        return -1;
    }

    if(priv->yev_client_connect) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "yev_client_connect ALREADY exists",
            "state",        "%s", gobj_current_state(gobj),
            NULL
        );
        return -1;
    }

    gobj_reset_volatil_attrs(gobj);

    if(priv->__clisrv__) {
        /*
         *  clisrv
         *  It's already connected when is created
         */
        set_connected(gobj, priv->fd_clisrv);

    } else {
        /*
         *  cli
         *  Decode url to connect, create yev_create_connect_event
         */
        if(state == ST_STOPPED) {
            gobj_change_state(gobj, ST_DISCONNECTED);
        }

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
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Parsing url failed",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                NULL
            );
        }
        if(strlen(schema) > 0 && schema[strlen(schema)-1]=='s') {
            priv->use_ssl = TRUE;
            gobj_write_bool_attr(gobj, "use_ssl", TRUE);
        }
        gobj_write_str_attr(gobj, "schema", schema);

        priv->yev_client_connect = yev_create_connect_event(
            yuno_event_loop(),
            yev_callback,
            gobj
        );
    }

    if(priv->use_ssl) {
        json_t *jn_crypto = gobj_read_json_attr(gobj, "crypto");
        priv->ytls = ytls_init(gobj, jn_crypto, FALSE);

        // TODO connection with certificate
        //  const char *cert_pem = gobj_read_str_attr(gobj, "cert_pem");
        //if(!empty_string(cert_pem)) {
        //    esp_transport_ssl_set_cert_data(priv->transport, cert_pem, (int)strlen(cert_pem));
        //}
        //if(gobj_read_bool_attr(gobj, "skip_cert_cn")) {
        //    esp_transport_ssl_skip_common_name_check(priv->transport);
    }

    if(IS_CLI) {
        /*
         *  cli
         *  try to connect
         */
        if (!gobj_read_bool_attr(gobj, "manual")) {
            if (priv->timeout_inactivity > 0) {
                // don't connect until arrives data to transmit
                if((*priv->pconnxs) > 0) {
                    // Some connection was happen
                } else {
                    // But connect once time at least.
                    gobj_send_event(gobj, EV_CONNECT, 0, gobj);
                }
            } else {
                gobj_send_event(gobj, EV_CONNECT, 0, gobj);
            }
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

    if(gobj_is_running(priv->gobj_timer)) {
        gobj_stop(priv->gobj_timer);
    }

    if(!gobj_in_this_state(gobj, ST_STOPPED)) {
        try_to_stop_yevents(gobj);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    EXEC_AND_RESET(yev_destroy_event, priv->yev_client_connect)
    EXEC_AND_RESET(yev_destroy_event, priv->yev_client_rx)
    EXEC_AND_RESET(yev_destroy_event, priv->yev_client_tx)
    EXEC_AND_RESET(ytls_cleanup, priv->ytls)
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
 *
 ***************************************************************************/
PRIVATE void set_connected(hgobj gobj, int fd)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_bool_attr(gobj, "connected", TRUE);
    get_peer_and_sock_name(gobj, fd);

    /*
     *  Info of "connected"
     */
    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
        if(IS_CLI) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "Connected To",
                "msg2",         "%s", "Connected To 🔵",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                NULL
            );
        } else {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "Connected From",
                "msg2",         "%s", "Connected From 🔵",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                NULL
            );
        }
    }

    clear_timeout(priv->gobj_timer);
    gobj_change_state(gobj, ST_CONNECTED);

    (*priv->pconnxs)++;

    /*
     *  Ready to receive
     */
    if(!priv->yev_client_rx) {
        json_int_t rx_buffer_size = gobj_read_integer_attr(gobj, "rx_buffer_size");
        priv->yev_client_rx = yev_create_read_event(
            yuno_event_loop(),
            yev_callback,
            gobj,
            fd,
            gbuffer_create(rx_buffer_size, rx_buffer_size)
        );
    }

    if(priv->yev_client_rx) {
        yev_set_fd(priv->yev_client_rx, fd);
    }
    if(!priv->yev_client_rx->gbuf) {
        json_int_t rx_buffer_size = gobj_read_integer_attr(gobj, "rx_buffer_size");
        yev_set_gbuffer(priv->yev_client_rx, gbuffer_create(rx_buffer_size, rx_buffer_size));
    } else {
        gbuffer_clear(priv->yev_client_rx->gbuf);
    }

    yev_start_event(priv->yev_client_rx);

    priv->inform_disconnection = TRUE;

    /*
     *  Publish
     */
    json_t *kw_conn = json_pack("{s:s, s:s, s:s}",
        "url",          gobj_read_str_attr(gobj, "url"),
        "peername",     gobj_read_str_attr(gobj, "peername"),
        "sockname",     gobj_read_str_attr(gobj, "sockname")
    );
    if(gobj_is_pure_child(gobj)) {
        gobj_send_event(gobj_parent(gobj), EV_CONNECTED, kw_conn, gobj);
    } else {
        gobj_publish_event(gobj, EV_CONNECTED, kw_conn);
    }
}

/***************************************************************************
 *  WARNING set disconnected when all yevents has stopped
 *  It's the signal that gclass can be re-used
 ***************************************************************************/
PRIVATE void set_disconnected(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_bool_attr(gobj, "connected", FALSE);

    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
        if(IS_CLI) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "Disconnected To",
                "msg2",         "%s", "Disconnected To 🔴",
                "cause",        "%s", gobj_log_last_message(),
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                "peername",     "%s", gobj_read_str_attr(gobj, "peername"),
                "sockname",     "%s", gobj_read_str_attr(gobj, "sockname"),
                NULL
            );
        } else {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "Disconnected From",
                "msg2",         "%s", "Disconnected From 🔴",
                "cause",        "%s", gobj_log_last_message(),
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                "peername",     "%s", gobj_read_str_attr(gobj, "peername"),
                "sockname",     "%s", gobj_read_str_attr(gobj, "sockname"),
                NULL
            );
        }
    }

    if(priv->yev_client_connect) {
        if(priv->yev_client_connect->fd > 0) {
            if(gobj_trace_level(gobj) & TRACE_UV) {
                gobj_log_debug(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_YEV_LOOP,
                    "msg",          "%s", "close socket",
                    "msg2",         "%s", "💥🟥 close yev_client_connect socket",
                    "fd",           "%d", priv->yev_client_connect->fd ,
                    "p",            "%p", priv->yev_client_connect,
                    NULL
                );
            }

            close(priv->yev_client_connect->fd);
            priv->yev_client_connect->fd = -1;
        }
        yev_set_flag(priv->yev_client_connect, YEV_FLAG_CONNECTED, FALSE);
    }

    /*
     *  Info of "disconnected"
     */
    if(priv->inform_disconnection) {
        priv->inform_disconnection = FALSE;
        if(gobj_is_pure_child(gobj)) {
            gobj_send_event(gobj_parent(gobj), EV_DISCONNECTED, 0, gobj);
        } else {
            gobj_publish_event(gobj, EV_DISCONNECTED, 0);
        }
    }

    gobj_write_str_attr(gobj, "peername", "");
    gobj_write_str_attr(gobj, "sockname", "");

    if(IS_CLISRV) {
        // auto stop
        if(gobj_is_running(gobj)) {
            gobj_stop(gobj);
        }
        if(gobj_is_pure_child(gobj)) {
            gobj_send_event(gobj_parent(gobj), EV_STOPPED, 0, gobj);
        } else {
            gobj_publish_event(gobj, EV_STOPPED, 0);
        }
    }

    if(IS_CLI) {
        /*
         *  cli
         */
        if(gobj_is_running(gobj)) {
            gobj_change_state(gobj, ST_DISCONNECTED);
            set_timeout(
                priv->gobj_timer,
                gobj_read_integer_attr(gobj, "timeout_between_connections")
            );
        } else {
            if(gobj_is_pure_child(gobj)) {
                gobj_send_event(gobj_parent(gobj), EV_STOPPED, 0, gobj);
            } else {
                gobj_publish_event(gobj, EV_STOPPED, 0);
            }
        }
    }
}

/***************************************************************************
 *  Return TRUE if all yevents stopped and destroyed
 *  if TRUE change to STOPPED state, else wait in WAIT_STOPPED
 ***************************************************************************/
PRIVATE BOOL try_to_stop_yevents(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    BOOL to_wait_stopped = FALSE;

    gobj_change_state(gobj, ST_WAIT_STOPPED);

    if(priv->yev_client_connect) {
        if(yev_event_is_stoppable(priv->yev_client_connect)) {
            to_wait_stopped = TRUE;
            yev_stop_event(priv->yev_client_connect);
        }
    }

    if(priv->yev_client_rx) {
        if(yev_event_is_stoppable(priv->yev_client_rx)) {
            to_wait_stopped = TRUE;
            yev_stop_event(priv->yev_client_rx);
        }
    }

    if(priv->yev_client_tx) {
        if(yev_event_is_stoppable(priv->yev_client_tx)) {
            to_wait_stopped = TRUE;
            yev_stop_event(priv->yev_client_tx);
        }
    }

    if(to_wait_stopped) {
        return FALSE;
    } else {
        gobj_change_state(gobj, ST_STOPPED);
        set_disconnected(gobj);
        return TRUE;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int yev_callback(yev_event_t *yev_event)
{
    hgobj gobj = yev_event->gobj;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_UV) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_event->flag);
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev callback",
            "msg2",         "%s", "💥 yev callback",
            "event type",   "%s", yev_event_type_name(yev_event),
            "result",       "%d", yev_event->result,
            "sres",         "%s", (yev_event->result<0)? strerror(-yev_event->result):"",
            "flag",         "%j", jn_flags,
            "p",            "%p", yev_event,
            NULL
        );
        json_decref(jn_flags);
    }

//    if(yev_event_is_stopped(yev_event)) {
//        gobj_send_event(gobj, EV_STOPPED, 0, gobj);
//        return 0;
//    }

    switch(yev_event->type) {
        case YEV_READ_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Disconnected
                     */
                    gobj_log_set_last_message("%s", strerror(-yev_event->result));

                    if(gobj_trace_level(gobj) & TRACE_UV) {
                        if(yev_event->result != -ECANCELED) {
                            gobj_log_debug(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                                "msg",          "%s", "read FAILED",
                                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                                "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                                "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                                "errno",        "%d", -yev_event->result,
                                "strerror",     "%s", strerror(-yev_event->result),
                                "p",            "%p", yev_event,
                                NULL
                            );
                        }
                    }
                    try_to_stop_yevents(gobj);

                } else {
                    /*
                     *  yev_event->gbuf can be null if yev_stop_event() was called
                     */
                    if(yev_event->gbuf) {
                        if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
                            gobj_trace_dump_gbuf(gobj, yev_event->gbuf, "%s: %s%s%s",
                                gobj_short_name(gobj),
                                gobj_read_str_attr(gobj, "sockname"),
                                " <- ",
                                gobj_read_str_attr(gobj, "peername")
                            );
                        }

                        (*priv->prxMsgs)++;
                        (*priv->prxBytes) += (json_int_t)gbuffer_leftbytes(yev_event->gbuf);

                        GBUFFER_INCREF(yev_event->gbuf)
                        json_t *kw = json_pack("{s:I}",
                            "gbuffer", (json_int_t)(size_t)yev_event->gbuf
                        );
                        if(gobj_is_pure_child(gobj)) {
                            gobj_send_event(gobj_parent(gobj), EV_RX_DATA, kw, gobj);
                        } else {
                            gobj_publish_event(gobj, EV_RX_DATA, kw);
                        }
                    }

                    /*
                     *  Clear buffer
                     *  Re-arm read
                     */
                    if(yev_event->gbuf) {
                        gbuffer_clear(yev_event->gbuf);
                        yev_start_event(yev_event);
                    }
                }
            }
            break;

        case YEV_WRITE_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Disconnected
                     */
                    gobj_log_set_last_message("%s", strerror(-yev_event->result));

                    if(gobj_trace_level(gobj) & TRACE_UV) {
                        if(yev_event->result != -ECANCELED) {
                            gobj_log_debug(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                                "msg",          "%s", "write FAILED",
                                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                                "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                                "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                                "errno",        "%d", -yev_event->result,
                                "strerror",     "%s", strerror(-yev_event->result),
                                "p",            "%p", yev_event,
                                NULL
                            );
                        }
                    }
                    try_to_stop_yevents(gobj);

                } else {
                    json_int_t mark = (json_int_t)gbuffer_getmark(yev_event->gbuf);
                    if(yev_event->flag & YEV_FLAG_WANT_TX_READY) {
                        if(!empty_string(priv->tx_ready_event_name)) {
                            json_t *kw_tx_ready = json_object();
                            json_object_set_new(kw_tx_ready, "gbuffer_mark", json_integer(mark));
                            if(gobj_is_pure_child(gobj)) {
                                gobj_send_event(gobj_parent(gobj), EV_TX_READY, kw_tx_ready, gobj);
                            } else {
                                gobj_publish_event(gobj, EV_TX_READY, kw_tx_ready);
                            }
                        }
                    }
                }

                yev_destroy_event(yev_event);
            }
            break;

        case YEV_CONNECT_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Error on connection
                     */
                    gobj_log_set_last_message("%s", strerror(-yev_event->result));

                    if(gobj_trace_level(gobj) & TRACE_UV) {
                        if(yev_event->result != -ECANCELED) {
                            gobj_log_debug(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_LIBUV_ERROR,
                                "msg",          "%s", "connect FAILED",
                                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                                "errno",        "%d", -yev_event->result,
                                "strerror",     "%s", strerror(-yev_event->result),
                                "p",            "%p", yev_event,
                                NULL
                            );
                        }
                    }
                    try_to_stop_yevents(gobj);

                } else {
                    set_connected(gobj, yev_event->fd);
                }
            }
            break;
        default:
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "event type NOT IMPLEMENTED",
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

    const char *url = gobj_read_str_attr(gobj, "url");
    if(yev_setup_connect_event(
        priv->yev_client_connect,
        url,    // client_url
        NULL    // local bind
    )<0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Cannot connect tcp gobj",
            NULL
        );
        try_to_stop_yevents(gobj);
        return -1;
    }

    if(yev_start_event(priv->yev_client_connect)<0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Cannot connect tcp gobj",
            NULL
        );
        try_to_stop_yevents(gobj);
        return -1;
    }

    return 0;
}

/***************************************************************************
 *  Sending data not encrypted
 ***************************************************************************/
PRIVATE int ac_tx_clear_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL want_tx_ready = kw_get_bool(gobj, kw, "want_tx_ready", 0, 0); // TODO get from attr?

    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED|KW_EXTRACT);
    if(!gbuf) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "gbuffer NULL",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
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
        (*priv->ptxMsgs)++;
        (*priv->ptxBytes) += (json_int_t)gbuffer_leftbytes(gbuf);

        /*
         *  Transmit
         */
        int fd = priv->__clisrv__? priv->fd_clisrv:priv->yev_client_connect->fd;
        if(!priv->yev_client_tx) {
            priv->yev_client_tx = yev_create_write_event(
                yuno_event_loop(),
                yev_callback,
                gobj,
                fd,
                gbuf
            );

        }
        yev_set_flag(priv->yev_client_tx, YEV_FLAG_WANT_TX_READY, want_tx_ready);
        yev_start_event(priv->yev_client_tx);
        gobj_change_state(gobj, ST_WAIT_TXED);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_send_encrypted_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
//    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

    gbuffer_incref(gbuf); // Quédate una copia

//   TODO if(priv->output_priority) {
//        /*
//         *  Salida prioritaria.
//         */
//        if(priv->gbuf_txing) {
//            log_error(LOG_OPT_TRACE_STACK,
//                "gobj",         "%s", gobj_full_name(gobj),
//                "function",     "%s", __FUNCTION__,
//                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//                "msg",          "%s", "gbuf_txing NOT NULL",
//                NULL
//            );
//            GBUF_DECREF(priv->gbuf_txing)
//        }
//        priv->gbuf_txing = gbuf;
//        try_write_all(gobj, TRUE);
//    } else {
//        do_write(gobj, gbuf);
//    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_enqueue_encrypted_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

    gbuffer_incref(gbuf); // Quédate una copia
    // TODO enqueue_write(gobj, gbuf);
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    try_to_stop_yevents(gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  This action must be only in ST_WAIT_STOPPED
 *  When all yevents are stopped then change to STOPPED and set_disconnect
 ***************************************************************************/
PRIVATE int ac_wait_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL change_to_stopped = TRUE;

    if(priv->yev_client_connect) {
        if(!yev_event_is_stopped(priv->yev_client_connect)) {
            change_to_stopped = FALSE;
        }
    }
    if(priv->yev_client_rx) {
        if(!yev_event_is_stopped(priv->yev_client_rx)) {
            change_to_stopped = FALSE;
        }
    }
    if(priv->yev_client_tx) {
        if(!yev_event_is_stopped(priv->yev_client_tx)) {
            change_to_stopped = FALSE;
        }
    }

    if(change_to_stopped) {
        gobj_change_state(gobj, ST_STOPPED);
        set_disconnected(gobj);
    }

    JSON_DECREF(kw)
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
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
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
GOBJ_DEFINE_EVENT(EV_SEND_ENCRYPTED_DATA);

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
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
    ev_action_t st_disconnected[] = {
        {EV_CONNECT,                ac_connect,                 ST_WAIT_CONNECTED},
        {EV_TIMEOUT,                ac_connect,                 ST_WAIT_CONNECTED},
        {0,0,0}
    };

    ev_action_t st_stopped[] = {
        {0,0,0}
    };

    ev_action_t st_wait_stopped[] = {
        {EV_STOPPED,                ac_wait_stopped,            0},
        {0,0,0}
    };

    ev_action_t st_wait_connected[] = {
        {EV_DROP,                   ac_drop,                    0},
        {0,0,0}
    };

    ev_action_t st_wait_handshake[] = {
        {EV_SEND_ENCRYPTED_DATA,    ac_send_encrypted_data,     0},
        {EV_STOPPED,                ac_drop,                    0},
        {EV_DROP,                   ac_drop,                    0},
        {0,0,0}
    };

    ev_action_t st_connected[] = {
        {EV_TX_DATA,                ac_tx_clear_data,           0},
        {EV_SEND_ENCRYPTED_DATA,    ac_send_encrypted_data,     ST_WAIT_TXED},
        {EV_STOPPED,                ac_drop,                    0},
        {EV_DROP,                   ac_drop,                    0},
        {0,0,0}
    };

    ev_action_t st_wait_txed[] = {
        {EV_TX_DATA,                ac_tx_clear_data,           0},
        {EV_SEND_ENCRYPTED_DATA,    ac_enqueue_encrypted_data,  0},
        {EV_STOPPED,                ac_drop,                    0},
        {EV_DROP,                   ac_drop,                    0},
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
        {ST_WAIT_TXED,          st_wait_txed},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_RX_DATA,        EVF_OUTPUT_EVENT},
        {EV_TX_DATA,        0},
        {EV_TX_READY,       EVF_OUTPUT_EVENT},
        {EV_DROP,           0},
        {EV_CONNECTED,      EVF_OUTPUT_EVENT},
        {EV_DISCONNECTED,   EVF_OUTPUT_EVENT},
        {EV_STOPPED,        EVF_OUTPUT_EVENT},
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
        tattr_desc,
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
