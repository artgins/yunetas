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
#include <unistd.h>
#include <helpers.h>
#include <kwid.h>
#include <ytls.h>
#include <yev_loop.h>
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
 */

/***************************************************************
 *              Constants
 ***************************************************************/
#define IS_CLI      (!priv->__clisrv__)
#define IS_CLISRV   (priv->__clisrv__)

GOBJ_DECLARE_EVENT(EV_SEND_ENCRYPTED_DATA);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void try_to_stop_yevents(hgobj gobj); // IDEMPOTENT
PRIVATE void set_connected(hgobj gobj, int fd);
PRIVATE int yev_callback(yev_event_h yev_event);
PRIVATE int ytls_on_handshake_done_callback(hgobj gobj, int error);
PUBLIC int ytls_on_clear_data_callback(hgobj gobj, gbuffer_t *gbuf);
PRIVATE int ytls_on_encrypted_data_callback(hgobj gobj, gbuffer_t *gbuf);

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE const sdata_desc_t attrs_table[] = { // WARNING repeated in c_tcp/c_esp_transport
/*-ATTR-type--------name----------------flag------------default-----description---------- */
SDATA (DTP_BOOLEAN, "__clisrv__",       SDF_STATS,      "false",    "Client of tcp server"),
SDATA (DTP_JSON,    "crypto",           SDF_RD,         0,          "Crypto config"),
SDATA (DTP_BOOLEAN, "use_ssl",          SDF_RD,         "false",    "True if schema is secure. Set internally if client, externally is clisrv"),
SDATA (DTP_POINTER, "ytls",             0,              0,          "TLS handler"),
SDATA (DTP_POINTER, "fd_clisrv",        0,              0,          "socket fd of clisrv"),

SDATA (DTP_INTEGER, "connxs",           SDF_STATS,      "0",        "connection counter"),
SDATA (DTP_BOOLEAN, "connected",        SDF_VOLATIL|SDF_STATS, "false", "Connection state. Important filter!"),
SDATA (DTP_BOOLEAN, "secure_connected", SDF_VOLATIL|SDF_STATS, "false", "Connection state"),
SDATA (DTP_STRING,  "url",              SDF_RD,         "",         "Url to connect"),
SDATA (DTP_STRING,  "schema",           SDF_RD,         "",         "schema, decoded from url. Set internally"),
SDATA (DTP_STRING,  "jwt",              SDF_RD,         "",         "TODO. Access with token JWT"),
SDATA (DTP_BOOLEAN, "skip_cert_cn",     SDF_RD,         "true",     "Skip verification of cert common name"),
SDATA (DTP_INTEGER, "keep_alive",       SDF_RD,         "10",       "Set keep-alive if > 0"),
SDATA (DTP_BOOLEAN, "manual",           SDF_RD,         "false",    "Set true if you want connect manually"),

SDATA (DTP_BOOLEAN,  "no_tx_ready_event",SDF_RD,        0,          "Set true if you don't want EV_TX_READY event"),

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
    TRACE_TLS                   = 0x0004,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"connections",         "Trace connections and disconnections"},
{"traffic",             "Trace dump traffic"},
{"tls",                 "Trace tls"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj gobj_timer;
    BOOL __clisrv__;
    yev_event_h yev_client_connect;    // Used if not __clisrv__ (pure tcp client)
    yev_event_h yev_client_rx;
    int fd_clisrv;
    int timeout_inactivity;
    BOOL inform_disconnection;

    json_int_t connxs;
    json_int_t txMsgs;
    json_int_t rxMsgs;
    json_int_t txBytes;
    json_int_t rxBytes;

    BOOL use_ssl;
    hytls ytls;
    hsskt sskt;

    dl_list_t dl_tx;
    gbuffer_t *gbuf_txing;

    BOOL no_tx_ready_event;
    int tx_in_progress;
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

    dl_init(&priv->dl_tx, gobj);

    priv->gobj_timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)(size_t)gobj_read_integer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    SET_PRIV(__clisrv__,            gobj_read_bool_attr)
    SET_PRIV(use_ssl,               gobj_read_bool_attr)
    SET_PRIV(no_tx_ready_event,     gobj_read_bool_attr)
    SET_PRIV(timeout_inactivity,    (int)gobj_read_integer_attr)
    SET_PRIV(fd_clisrv,             (int)gobj_read_integer_attr)
    SET_PRIV(ytls,                  (hytls)(size_t)gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout_inactivity,      (int) gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(no_tx_ready_event,     gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(use_ssl,               gobj_read_bool_attr)
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

        /*
         *  Parse url to save schema
         *  It's parsing again in yev_setup_connect_event()
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
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Parsing url failed",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                NULL
            );
        }
        gobj_write_str_attr(gobj, "schema", schema);

        priv->yev_client_connect = yev_create_connect_event(
            yuno_event_loop(),
            yev_callback,
            gobj
        );
    }

    if(IS_CLI) {
        /*
         *  cli
         *  try to connect
         */
        if (!gobj_read_bool_attr(gobj, "manual")) {
            if (priv->timeout_inactivity > 0) {
                // don't connect until arrives data to transmit
                if(priv->connxs > 0) {
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

    try_to_stop_yevents(gobj);

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
    if(priv->sskt) {
        ytls_free_secure_filter(priv->ytls, priv->sskt);
        priv->sskt = 0;
    }
    if(IS_CLI) {
        EXEC_AND_RESET(ytls_cleanup, priv->ytls)
    }

    GBUFFER_DECREF(priv->gbuf_txing)
    dl_flush(&priv->dl_tx, (fnfree)gbuffer_decref);
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
        v.v.i = (json_int_t)dl_size(&priv->dl_tx);
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
    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT || 1) {
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

    priv->connxs++;

    clear_timeout(priv->gobj_timer);

    /*-------------------------------*
     *      Setup reading event
     *-------------------------------*/
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
    if(!yev_get_gbuf(priv->yev_client_rx)) {
        json_int_t rx_buffer_size = gobj_read_integer_attr(gobj, "rx_buffer_size");
        yev_set_gbuffer(priv->yev_client_rx, gbuffer_create(rx_buffer_size, rx_buffer_size));
    } else {
        gbuffer_clear(yev_get_gbuf(priv->yev_client_rx));
    }

    yev_start_event(priv->yev_client_rx);

    if(priv->use_ssl) {
        /*---------------------------*
         *      Secure traffic
         **---------------------------*/
        gobj_change_state(gobj, ST_WAIT_HANDSHAKE);

        priv->inform_disconnection = FALSE;

        priv->sskt = ytls_new_secure_filter(
            priv->ytls,
            ytls_on_handshake_done_callback,
            ytls_on_clear_data_callback,
            ytls_on_encrypted_data_callback,
            gobj
        );
        if(!priv->sskt) {
            if(gobj_is_running(gobj)) {
                gobj_stop(gobj); // auto-stop
            }
            return;
        }

        ytls_set_trace(priv->ytls, priv->sskt, (gobj_trace_level(gobj) & TRACE_TLS)?TRUE:FALSE);

    } else {
        /*---------------------------*
         *      Clear traffic
         **---------------------------*/
        gobj_change_state(gobj, ST_CONNECTED);

        priv->inform_disconnection = TRUE;

        /*
         *  Publish
         */
        json_t *kw_conn = json_pack("{s:s, s:s, s:s}",
            "url",          gobj_read_str_attr(gobj, "url"),
            "peername",     gobj_read_str_attr(gobj, "peername"),
            "sockname",     gobj_read_str_attr(gobj, "sockname")
        );

        /*
         *  CHILD subscription model
         */
        if(gobj_is_service(gobj)) {
            gobj_publish_event(gobj, EV_CONNECTED, kw_conn);
        } else {
            gobj_send_event(gobj_parent(gobj), EV_CONNECTED, kw_conn, gobj);
        }
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void set_secure_connected(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_bool_attr(gobj, "secure_connected", TRUE);
    priv->inform_disconnection = TRUE;

    gobj_change_state(gobj, ST_CONNECTED);

    /*
     *  Publish
     */
    json_t *kw_conn = json_pack("{s:s, s:s, s:s}",
        "url",          gobj_read_str_attr(gobj, "url"),
        "peername",     gobj_read_str_attr(gobj, "peername"),
        "sockname",     gobj_read_str_attr(gobj, "sockname")
    );

    /*
     *  CHILD subscription model
     */
    if(gobj_is_service(gobj)) {
        gobj_publish_event(gobj, EV_CONNECTED, kw_conn);
    } else {
        gobj_send_event(gobj_parent(gobj), EV_CONNECTED, kw_conn, gobj);
    }

    ytls_flush(priv->ytls, priv->sskt);
}

/***************************************************************************
 *  WARNING set disconnected when all yevents has stopped
 *  It's the signal that gclass can be re-used
 ***************************************************************************/
PRIVATE void set_disconnected(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_bool_attr(gobj, "connected", FALSE);
    gobj_write_bool_attr(gobj, "secure_connected", FALSE);

    if(gobj_trace_level(gobj) & (TRACE_CONNECT_DISCONNECT) || 1) {
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
        if(yev_get_fd(priv->yev_client_connect) > 0) {
            if(gobj_trace_level(gobj) & TRACE_URING) {
                gobj_log_debug(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_YEV_LOOP,
                    "msg",          "%s", "close socket yev_client_connect",
                    "msg2",         "%s", "💥🟥 close socket yev_client_connect",
                    "fd",           "%d", yev_get_fd(priv->yev_client_connect) ,
                    "p",            "%p", priv->yev_client_connect,
                    NULL
                );
            }

            close(yev_get_fd(priv->yev_client_connect));
            yev_set_fd(priv->yev_client_connect, -1);
        }
    } else {
        if(priv->fd_clisrv > 0) {
            if(gobj_trace_level(gobj) & TRACE_URING) {
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

    if(priv->sskt) {
        ytls_free_secure_filter(priv->ytls, priv->sskt);
        priv->sskt = 0;
    }

    /*
     *  Info of "disconnected"
     */
    if(priv->inform_disconnection) {
        priv->inform_disconnection = FALSE;
        /*
         *  CHILD subscription model
         */
        if(gobj_is_service(gobj)) {
            gobj_publish_event(gobj, EV_DISCONNECTED, 0);
        } else {
            gobj_send_event(gobj_parent(gobj), EV_DISCONNECTED, 0, gobj);
        }
    }

    gobj_write_str_attr(gobj, "peername", "");
    gobj_write_str_attr(gobj, "sockname", "");

    if(IS_CLISRV) {
        // auto stop
        if(gobj_is_running(gobj)) {
            gobj_stop(gobj);
        }

        /*
         *  CHILD subscription model
         */
        if(gobj_is_service(gobj)) {
            gobj_publish_event(gobj, EV_STOPPED, 0);
        } else {
            gobj_send_event(gobj_parent(gobj), EV_STOPPED, 0, gobj);
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
            /*
             *  CHILD subscription model
             */
            if(gobj_is_service(gobj)) {
                gobj_publish_event(gobj, EV_STOPPED, 0);
            } else {
                gobj_send_event(gobj_parent(gobj), EV_STOPPED, 0, gobj);
            }
        }
    }
}

/***************************************************************************
 *  Write the current gbuffer
 ***************************************************************************/
PRIVATE int write_data(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = priv->gbuf_txing;

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
        priv->txMsgs++;

        /*
         *  Transmit
         */
        int fd = priv->__clisrv__? priv->fd_clisrv:yev_get_fd(priv->yev_client_connect);
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
    if(gobj_current_state(gobj)==ST_DISCONNECTED) {
        gobj_change_state(gobj, ST_STOPPED);
        return;
    }

    gobj_change_state(gobj, ST_WAIT_STOPPED);

    if(gobj_trace_level(gobj) & TRACE_URING) {
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "try_to_stop_yevents",
            "msg2",         "%s", "🟥🟥 try_to_stop_yevents",
            NULL
        );
    }

    if(priv->fd_clisrv > 0) {
        if(gobj_trace_level(gobj) & TRACE_URING) {
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

    if(priv->yev_client_connect) {
        if(!yev_event_is_stopped(priv->yev_client_connect)) {
            yev_stop_event(priv->yev_client_connect);
            if(!yev_event_is_stopped(priv->yev_client_connect)) {
                to_wait_stopped = TRUE;
            }
        }
    }

    if(priv->yev_client_rx) {
        if(!yev_event_is_stopped(priv->yev_client_rx)) {
            yev_stop_event(priv->yev_client_rx);
            if(!yev_event_is_stopped(priv->yev_client_rx)) {
                to_wait_stopped = TRUE;
            }
        }
    }

    if(priv->tx_in_progress > 0) {
        to_wait_stopped = TRUE;
    }

    if(!to_wait_stopped) {
        gobj_change_state(gobj, ST_STOPPED);
        set_disconnected(gobj);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
int send_clear_data(hytls ytls, hsskt sskt, gbuffer_t *gbuf)
{
    if(ytls_encrypt_data(ytls, sskt, gbuf)<0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "ytls_encrypt_data() FAILED",
            "error",        "%s", ytls_get_last_error(ytls, sskt),
            NULL
        );
        return -1;
    }
    if(gbuffer_leftbytes(gbuf) > 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "NEED a queue, NOT ALL DATA being encrypted",
            NULL
        );
    }
    return 0;
}

/***************************************************************************
 *  YTLS callbacks, called when handshake is done
 ***************************************************************************/
PRIVATE int ytls_on_handshake_done_callback(hgobj gobj, int error)
{
    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", error<0?"TLS handshake FAILS":"TLS Handshake OK",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            "cause",        "%s", gobj_log_last_message(),
            "peername",     "%s", gobj_read_str_attr(gobj, "peername"),
            "sockname",     "%s", gobj_read_str_attr(gobj, "sockname"),
            NULL
        );
    }

    if(error < 0) {
        /*
         *  Don't stop here, will be stopped in return of ytls_decrypt_data()
         */
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

    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "decrypted data");
    }

    json_t *kw = json_pack("{s:I}",
        "gbuffer", (json_int_t)(size_t)gbuf
    );
    /*
     *  CHILD subscription model
     */
    if(gobj_is_service(gobj)) {
        return gobj_publish_event(gobj, EV_RX_DATA, kw);
    } else {
        return gobj_send_event(gobj_parent(gobj), EV_RX_DATA, kw, gobj);
    }
}

/***************************************************************************
 *  YTLS callbacks, called when transmitting data has been encrypted
 ***************************************************************************/
PRIVATE int ytls_on_encrypted_data_callback(hgobj gobj, gbuffer_t *gbuf)
{
    //PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "encrypted data");
    }

    json_t *kw = json_pack("{s:I}",
        "gbuffer", (json_int_t)(size_t)gbuf
    );
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

    if(gobj_trace_level(gobj) & TRACE_URING) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_get_flag(yev_event));
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev callback",
            "msg2",         "%s", "🌐🌐💥 yev callback",
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
                    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
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
                     *  Clear buffer
                     *  Re-arm read
                     *  Check ret is 0 because the EV_RX_DATA could provoke the destroying of gobj
                     */
                    if(ret == 0 && gobj_is_running(gobj)) {
                        gbuffer_clear(yev_get_gbuf(yev_event));
                        yev_start_event(yev_event);
                    }

                } else {
                    /*
                     *  Disconnected
                     */
                    gobj_log_set_last_message("%s", strerror(-yev_get_result(yev_event)));

                    if(gobj_trace_level(gobj) & TRACE_URING || 1) {
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
                            if(gobj_trace_level(gobj) & TRACE_MACHINE) {
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

                    if(gobj_trace_level(gobj) & TRACE_URING || 1) {
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
                            NULL
                        );
                    }

                    yev_destroy_event(yev_event);

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
                    gobj_log_set_last_message("%s", strerror(-yev_get_result(yev_event)));

                    if(gobj_trace_level(gobj) & TRACE_URING || 1) {
                        gobj_log_debug(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_LIBURING_ERROR,
                            "msg",          "%s", "TCP: connect FAILED",
                            "msg2",         "%s", "🌐TCP: connect FAILED",
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
                            "errno",        "%d", -yev_get_result(yev_event),
                            "strerror",     "%s", strerror(-yev_get_result(yev_event)),
                            "p",            "%p", yev_event,
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
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
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

    const char *url = gobj_read_str_attr(gobj, "url");
    if(yev_setup_connect_event(
        priv->yev_client_connect,
        url,    // client_url
        NULL,   // local bind
        0,  // ai_family AF_UNSPEC
        0   // ai_flags AI_V4MAPPED | AI_ADDRCONFIG
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

    if(yev_get_flag(priv->yev_client_connect) & YEV_FLAG_USE_TLS) {
        gobj_write_bool_attr(gobj, "use_ssl", TRUE);
    } else {
        gobj_write_bool_attr(gobj, "use_ssl", FALSE);
    }
    if(priv->use_ssl) {
        if(!priv->ytls) {
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
 *  Sending data, if using TLS will be encrypted, else sent as is
 ***************************************************************************/
PRIVATE int ac_tx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
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

    if(!priv->gbuf_txing) {
        priv->gbuf_txing = gbuffer_incref(gbuf);
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
    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

    /*
     *  Transmit
     */
    int fd = priv->__clisrv__? priv->fd_clisrv:yev_get_fd(priv->yev_client_connect);
    yev_event_h yev_write_event = yev_create_write_event(
        yuno_event_loop(),
        yev_callback,
        gobj,
        fd,
        gbuffer_incref(gbuf)
    );

    priv->tx_in_progress++;
    yev_start_event(yev_write_event);

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
    .mt_reading = mt_reading,
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
        {0,0,0}
    };

    ev_action_t st_wait_connected[] = {
        {EV_DROP,                   ac_drop,                    0},
        {0,0,0}
    };

    ev_action_t st_wait_handshake[] = {
        {EV_SEND_ENCRYPTED_DATA,    ac_send_encrypted_data,     0},
        {EV_DROP,                   ac_drop,                    0},
        {0,0,0}
    };

    ev_action_t st_connected[] = {
        {EV_TX_DATA,                ac_tx_data,                 0},
        {EV_SEND_ENCRYPTED_DATA,    ac_send_encrypted_data,     0},
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
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_RX_DATA,            EVF_OUTPUT_EVENT},
        {EV_TX_READY,           EVF_OUTPUT_EVENT},
        {EV_CONNECTED,          EVF_OUTPUT_EVENT},
        {EV_DISCONNECTED,       EVF_OUTPUT_EVENT},
        {EV_STOPPED,            EVF_OUTPUT_EVENT},
        {EV_CONNECT,            0},
        {EV_TIMEOUT,            0},
        {EV_SEND_ENCRYPTED_DATA,0},
        {EV_TX_DATA,            0},
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
