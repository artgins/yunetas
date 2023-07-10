/****************************************************************************
 *          c_linux_transport.c
 *
 *          GClass Transport: tcp,ssl,...
 *          Low level linux
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <c_timer.h>
#include <c_linux_yuno.h>
#include <parse_url.h>
#include <kwid.h>
#include <gbuffer.h>
#include <yunetas_ev_loop.h>
#include "c_linux_transport.h"

/***************************************************************
 *              Constants
 ***************************************************************/
#define BUFFER_SIZE (4*1024)    // TODO move to configuration

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void set_connected(hgobj gobj, int fd);
PRIVATE void set_disconnected(hgobj gobj, const char *cause);
PRIVATE int yev_client_callback(yev_event_t *event);

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = { // WARNING repeated in c_linux_transport/c_esp_transport
/*-ATTR-type--------name----------------flag------------default-----description---------- */
SDATA (DTP_INTEGER, "connxs",           SDF_STATS,      "0",        "connection counter"),
SDATA (DTP_BOOLEAN, "connected",        SDF_VOLATIL|SDF_STATS, "false", "Connection state. Important filter!"),
SDATA (DTP_STRING,  "url",              SDF_RD,         "",         "Url to connect"),
SDATA (DTP_STRING,  "schema",           SDF_RD,         "",         "schema, decoded from url. Set internally"),
SDATA (DTP_STRING,  "host",             SDF_RD,         "",         "host, decoded from url. Set internally"),
SDATA (DTP_STRING,  "port",             SDF_RD,         "",         "port, decoded from url. Set internally"),
SDATA (DTP_BOOLEAN, "use_ssl",          SDF_RD,         "false",    "True if schema is secure. Set internally"),
SDATA (DTP_STRING,  "cert_pem",         SDF_RD,         "",         "SSL server certification, PEM str format"),
SDATA (DTP_STRING,  "jwt",              SDF_RD,         "",         "TODO. Access with token JWT"),
SDATA (DTP_BOOLEAN, "skip_cert_cn",     SDF_RD,         "true",     "Skip verification of cert common name"),
SDATA (DTP_INTEGER, "keep_alive",       SDF_RD,         "10",       "Set keep-alive if > 0"),
SDATA (DTP_BOOLEAN, "character_device", SDF_RD,         "false",    "Char device (Ex: tty://dev/ttyUSB2)"),
SDATA (DTP_BOOLEAN, "manual",           SDF_RD,         "false",    "Set true if you want connect manually"),

SDATA (DTP_INTEGER, "timeout_waiting_connected", SDF_WR|SDF_PERSIST, "60000", "Timeout waiting connected in miliseconds"),
SDATA (DTP_INTEGER, "timeout_between_connections", SDF_WR|SDF_PERSIST, "2000", "Idle timeout to wait between attempts of connection, in miliseconds"),
SDATA (DTP_INTEGER, "timeout_inactivity", SDF_WR|SDF_PERSIST, "-1", "Inactivity timeout in miliseconds to close the connection. Reconnect when new data arrived. With -1 never close."),

SDATA (DTP_INTEGER, "txBytes",          SDF_VOLATIL|SDF_STATS, "0", "Messages transmitted"),
SDATA (DTP_INTEGER, "rxBytes",          SDF_VOLATIL|SDF_STATS, "0", "Messages received"),
SDATA (DTP_INTEGER, "txMsgs",           SDF_VOLATIL|SDF_STATS, "0", "Messages transmitted"),
SDATA (DTP_INTEGER, "rxMsgs",           SDF_VOLATIL|SDF_STATS, "0", "Messages received"),
SDATA (DTP_INTEGER, "txMsgsec",         SDF_VOLATIL|SDF_STATS, "0", "Messages by second"),
SDATA (DTP_INTEGER, "rxMsgsec",         SDF_VOLATIL|SDF_STATS, "0", "Messages by second"),
SDATA (DTP_INTEGER, "maxtxMsgsec",      SDF_VOLATIL|SDF_RSTATS,"0", "Max Messages by second"),
SDATA (DTP_INTEGER, "maxrxMsgsec",      SDF_VOLATIL|SDF_RSTATS,"0", "Max Messages by second"),
SDATA (DTP_STRING,  "peername",         SDF_VOLATIL|SDF_STATS, "",  "Peername"),
SDATA (DTP_STRING,  "sockname",         SDF_VOLATIL|SDF_STATS, "",  "Sockname"),
SDATA (DTP_INTEGER, "max_tx_queue",     SDF_WR,         "0",        "Maximum messages in tx queue. Default is 0: no limit."),
SDATA (DTP_INTEGER, "tx_queue_size",    SDF_RD|SDF_STATS,"0",       "Current messages in tx queue"),

SDATA (DTP_BOOLEAN, "__clisrv__",       SDF_STATS,      "false",    "Client of tcp server"),
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
    hgobj gobj_timer;
    yev_event_t *yev_client_connect;    // Used in not __clisrv__

    gbuffer_t *gbuf_client_tx;
    yev_event_t *yev_client_tx;

    gbuffer_t *gbuf_client_rx;
    yev_event_t *yev_client_rx;

    int timeout_inactivity;
    BOOL inform_disconnection;
    BOOL use_ssl;
} PRIVATE_DATA;

PRIVATE hgclass gclass = 0;





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

    char schema[16];
    char host[120];
    char port[10];

    parse_url(
        gobj,
        gobj_read_str_attr(gobj, "url"),
        schema, sizeof(schema),
        host, sizeof(host),
        port, sizeof(port),
        0, 0,
        0, 0,
        FALSE
    );
    if(strlen(schema) > 0 && schema[strlen(schema)-1]=='s') {
        priv->use_ssl = TRUE;
        gobj_write_bool_attr(gobj, "use_ssl", TRUE);
    }
    gobj_write_str_attr(gobj, "schema", schema);
    gobj_write_str_attr(gobj, "host", host);
    gobj_write_str_attr(gobj, "port", port);

    if(priv->use_ssl) {
//        priv->transport = esp_transport_ssl_init();

// TODO       const char *cert_pem = gobj_read_str_attr(gobj, "cert_pem");
//        if(!empty_string(cert_pem)) {
//            esp_transport_ssl_set_cert_data(priv->transport, cert_pem, (int)strlen(cert_pem));
//        }
//        if(gobj_read_bool_attr(gobj, "skip_cert_cn")) {
//            esp_transport_ssl_skip_common_name_check(priv->transport);
//        }
    } else {
//        priv->transport = esp_transport_tcp_init();
    }

    if(!gobj_read_bool_attr(gobj, "character_device")) {
        if (!gobj_read_bool_attr(gobj, "__clisrv__")) {
            const char *url = gobj_read_str_attr(gobj, "url");
            priv->yev_client_connect = yev_create_connect_event(
                yuno_event_loop(),
                yev_client_callback,
                gobj
            );
            int fd = yev_setup_connect_event(
                priv->yev_client_connect,
                url,    // client_url
                NULL    // local bind
            );
            if (fd < 0) {
                // TODO exit???
                gobj_trace_msg(gobj, "Error setup connect to %s", gobj_read_str_attr(gobj, "url"));
                exit(0);
            }
        }
    }

    /*
     *  Child, default subscriber, the parent
     */
    hgobj subscriber = (hgobj)(size_t)gobj_read_integer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    SET_PRIV(timeout_inactivity,    (int)gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    IF_EQ_SET_PRIV(timeout_inactivity,  (int) gobj_read_integer_attr)
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
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Initial wrong task state",
            "state",        "%s", gobj_current_state(gobj),
            NULL
        );
    }
    if(state == ST_STOPPED) {
        gobj_change_state(gobj, ST_DISCONNECTED);
    }

    gobj_reset_volatil_attrs(gobj);

    if(!gobj_read_bool_attr(gobj, "character_device")) {
        if(!gobj_read_bool_attr(gobj, "__clisrv__")) {
            /*
             * pure tcp client: try to connect
             */
            // HACK el start de tcp0 lo hace el timer
            if(!gobj_read_bool_attr(gobj, "manual")) {
                set_timeout(priv->gobj_timer, 100);
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

    clear_timeout(priv->gobj_timer);
    gobj_stop(priv->gobj_timer);

    BOOL change_to_wait_stopped = FALSE;

    if(priv->yev_client_rx) {
        if(!(priv->yev_client_rx->flag & (YEV_STOPPING_FLAG|YEV_STOPPED_FLAG))) {
            change_to_wait_stopped = TRUE;
            yev_stop_event(priv->yev_client_rx);
        }
    }
    if(priv->yev_client_tx) {
        if(!(priv->yev_client_tx->flag & (YEV_STOPPING_FLAG|YEV_STOPPED_FLAG))) {
            change_to_wait_stopped = TRUE;
            yev_stop_event(priv->yev_client_tx);
        }
    }
    if(priv->yev_client_connect) {
        if(!(priv->yev_client_connect->flag & (YEV_STOPPING_FLAG|YEV_STOPPED_FLAG))) {
            change_to_wait_stopped = TRUE;
            yev_stop_event(priv->yev_client_connect);
        }
    }

    if(change_to_wait_stopped) {
        gobj_change_state(gobj, ST_WAIT_STOPPED);
    } else {
        gobj_change_state(gobj, ST_STOPPED);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    EXEC_AND_RESET(yev_destroy_event, priv->yev_client_connect);
    EXEC_AND_RESET(yev_destroy_event, priv->yev_client_rx);
    EXEC_AND_RESET(yev_destroy_event, priv->yev_client_tx);

    GBUFFER_DECREF(priv->gbuf_client_rx)
    GBUFFER_DECREF(priv->gbuf_client_tx)
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

    clear_timeout(priv->gobj_timer);

    INCR_ATTR_INTEGER(connxs)

    priv->inform_disconnection = TRUE;
    get_peer_and_sock_name(gobj, fd);

    /*
     *  Info of "connected"
     */
    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECTION,
            "msg",          "%s", "Connected",
            "msg2",         "%s", "Connected🔵",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
            "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
            NULL
        );
    }

    /*
     *  Ready to receive
     */
    if(!priv->gbuf_client_rx) {
        priv->gbuf_client_rx = gbuffer_create(BUFFER_SIZE, BUFFER_SIZE);
        gbuffer_setlabel(priv->gbuf_client_rx, "transport-rx");
    }
    if(!priv->yev_client_rx) {
        priv->yev_client_rx = yev_create_read_event(
            yuno_event_loop(),
            yev_client_callback,
            gobj,
            fd
        );
    }
    yev_start_event(priv->yev_client_rx, priv->gbuf_client_rx);

    gobj_change_state(gobj, ST_CONNECTED);

    // TODO try_write_all(gobj, FALSE);

    /*
     *  Publish
     */
    json_t *kw_conn = json_pack("{s:s, s:s, s:s}",
        "url",          gobj_read_str_attr(gobj, "url"),
        "peername",     gobj_read_str_attr(gobj, "peername"),
        "sockname",     gobj_read_str_attr(gobj, "sockname")
    );
    gobj_publish_event(gobj, EV_CONNECTED, kw_conn);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void set_disconnected(hgobj gobj, const char *cause)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->gobj_timer);

    gobj_change_state(gobj, ST_DISCONNECTED);

    /*
     *  Info of "disconnected"
     */
    if(priv->inform_disconnection) {
        priv->inform_disconnection = FALSE;

        if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_CONNECTION,
                "msg",          "%s", "Disconnected",
                "msg2",         "%s", "Disconnected🔴",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                "peername",     "%s", gobj_read_str_attr(gobj, "peername"),
                "sockname",     "%s", gobj_read_str_attr(gobj, "sockname"),
                NULL
            );
        }

        gobj_publish_event(gobj, EV_DISCONNECTED, 0);
    }

    gobj_write_str_attr(gobj, "peername", "");
    gobj_write_str_attr(gobj, "sockname", "");

    if(gobj_read_bool_attr(gobj, "__clisrv__")) {
        // TODO to stop
    } else {
        if(gobj_is_running(gobj)) {
            set_timeout(
                priv->gobj_timer,
                gobj_read_integer_attr(gobj, "timeout_between_connections")
            );
        }
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int yev_client_callback(yev_event_t *yev_event)
{
    hgobj gobj = yev_event->gobj;
    BOOL stopped = (yev_event->flag & YEV_STOPPED_FLAG)?TRUE:FALSE;

    if(gobj_trace_level(gobj) & TRACE_UV) {
        gobj_trace_msg(
            gobj, "yev client callback %s%s", yev_event_type_name(yev_event), stopped ? ", STOPPED" : ""
        );
    }
    switch(yev_event->type) {
        case YEV_READ_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Disconnected
                     */
                    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
                        gobj_log_info(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_CONNECTION,
                            "msg",          "%s", "read FAILED",
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
                            "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                            "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                            "errno",        "%d", -yev_event->result,
                            "strerror",     "%s", strerror(-yev_event->result),
                            NULL
                        );
                    }
                    gbuffer_clear(yev_event->gbuf); // TODO check if rx gbuf? clear tx gbuf too?
                    set_disconnected(gobj, strerror(-yev_event->result));
                    break;
                }

                if(gobj_trace_level(gobj) & TRACE_DUMP_TRAFFIC) {
                    gobj_trace_dump_gbuf(gobj, yev_event->gbuf, "%s: %s%s%s",
                        gobj_short_name(gobj),
                        gobj_read_str_attr(gobj, "sockname"),
                        " <- ",
                        gobj_read_str_attr(gobj, "peername")
                    );
                }
                GBUFFER_INCREF(yev_event->gbuf)
                json_t *kw = json_pack("{s:I}",
                    "gbuffer", (json_int_t)(size_t)yev_event->gbuf
                );
                gobj_publish_event(gobj, EV_RX_DATA, kw);

                /*
                 *  Clear buffer
                 *  Re-arm read
                 */
                gbuffer_clear(yev_event->gbuf);
                yev_start_event(yev_event, yev_event->gbuf);
            }
            break;

        case YEV_WRITE_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Disconnected
                     */
                    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
                        gobj_log_info(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_CONNECTION,
                            "msg",          "%s", "read FAILED",
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
                            "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                            "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                            "errno",        "%d", -yev_event->result,
                            "strerror",     "%s", strerror(-yev_event->result),
                            NULL
                        );
                    }
                    gbuffer_clear(yev_event->gbuf); // TODO check if rx gbuf? clear tx gbuf too?
                    set_disconnected(gobj, strerror(-yev_event->result));
                    break;
                }

                // Write ended
                // try_write_all(gobj, TRUE); TODO ???
            }
            break;

        case YEV_CONNECT_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Error on connection
                     */
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBUV_ERROR,
                        "msg",          "%s", "connect FAILED",
                        "url",          "%s", gobj_read_str_attr(gobj, "url"),
                        "errno",        "%d", -yev_event->result,
                        "strerror",     "%s", strerror(-yev_event->result),
                        NULL
                    );
                    set_disconnected(gobj, strerror(-yev_event->result));
                    break;
                }

                set_connected(gobj, yev_event->fd);
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
 *  Timeout to start connection
 ***************************************************************************/
PRIVATE int ac_timeout_disconnected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_read_bool_attr(gobj, "manual")) {
        if (priv->timeout_inactivity > 0) {
            // don't connect until arrives data to transmit
            if (gobj_read_integer_attr(gobj, "connxs") > 0) {
            } else {
                // But connect once time at least.
                gobj_send_event(gobj, EV_CONNECT, 0, gobj);
            }
        } else {
            gobj_send_event(gobj, EV_CONNECT, 0, gobj);
        }
    }

    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_connect(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // HACK firstly set timeout, EV_CONNECTED can be received inside gobj_start()
    set_timeout(priv->gobj_timer, gobj_read_integer_attr(gobj, "timeout_waiting_connected"));
    gobj_change_state(gobj, ST_WAIT_CONNECTED);
    yev_start_event(priv->yev_client_connect, NULL);

    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_wait_connected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(
        priv->gobj_timer,
        gobj_read_integer_attr(gobj, "timeout_between_connections")
    );

    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
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

//                /*
//                 *  Transmit
//                 */
//                if(!gbuf_client_tx) {
//                    gbuf_client_tx = gbuffer_create(BUFFER_SIZE, BUFFER_SIZE);
//                    gbuffer_setlabel(gbuf_client_tx, "client-tx");
//                    #ifdef LIKE_LIBUV_PING_PONG
//                    gbuffer_append_string(gbuf_client_tx, PING);
//                    #else
//                    for(int i= 0; i<BUFFER_SIZE; i++) {
//                        gbuffer_append_char(gbuf_client_tx, 'A');
//                    }
//                    #endif
//                }
//
//                if(!yev_client_tx) {
//                    yev_client_tx = yev_create_write_event(
//                        yev_event->yev_loop,
//                        yev_client_callback,
//                        gobj,
//                        yev_event->fd
//                    );
//                }
//
//                /*
//                 *  Transmit
//                 */
//                if(dump) {
//                    gobj_trace_dump_gbuf(gobj, yev_event->gbuf, "Client transmitting");
//                }
//                yev_start_event(yev_client_tx, gbuf_client_tx);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    // TODO

    JSON_DECREF(kw)
    return 0;
}




                    /***************************
                     *          FSM
                     ***************************/




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
GOBJ_DEFINE_GCLASS(C_LINUX_TRANSPORT);

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
    if(gclass) {
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
        {0,0,0}
    };
    ev_action_t st_disconnected[] = {
        {EV_CONNECT,            ac_connect,                 0},
        {EV_TIMEOUT,            ac_timeout_disconnected,    0},  // send EV_CONNECT
        {0,0,0}
    };
    ev_action_t st_wait_connected[] = {
        {EV_TIMEOUT,            ac_timeout_wait_connected,  ST_DISCONNECTED},
        {EV_DROP,               ac_drop,                    0},
        {0,0,0}
    };
    ev_action_t st_connected[] = {
        {EV_TX_DATA,            ac_tx_data,                 0},
        {EV_DROP,               ac_drop,                    0},
        {0,0,0}
    };
    ev_action_t st_wait_disconnected[] = {
        {0,0,0}
    };
    ev_action_t st_wait_stopped[] = {
        {0,0,0}
    };

    states_t states[] = {
        {ST_STOPPED,            st_stopped},
        {ST_DISCONNECTED,       st_disconnected},
        {ST_WAIT_CONNECTED,     st_wait_connected},
        {ST_CONNECTED,          st_connected},
        {ST_WAIT_DISCONNECTED,  st_wait_disconnected},
        {ST_WAIT_STOPPED,       st_wait_stopped},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TX_DATA,        0},
        {EV_RX_DATA,        EVF_OUTPUT_EVENT},
        {EV_DROP,           0},
        {EV_CONNECTED,      EVF_OUTPUT_EVENT},
        {EV_DISCONNECTED,   EVF_OUTPUT_EVENT},
        {EV_TX_READY,       EVF_OUTPUT_EVENT},  // TODO not used by now
        {EV_STOPPED,        EVF_OUTPUT_EVENT},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    gclass = gclass_create(
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
    if(!gclass) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_linux_transport(void)
{
    return create_gclass(C_LINUX_TRANSPORT);
}

/*----------------------------------------------------------------------*
 *      New client of server connected.
 *      Called from TODO on_connection_cb() of c_tcp_s0.c
 *      If return -1 the clisrv gobj is destroyed by c_tcp_s0
 *----------------------------------------------------------------------*/
PUBLIC int accept_connection(
    hgobj clisrv,
    void *uv_server_socket)
{
//    PRIVATE_DATA *priv = gobj_priv_data(clisrv);

    if(!gobj_is_running(clisrv)) {
// TODO       if(gobj_trace_level(clisrv) & TRACE_UV) {
//            trace_msg(">>> tcp not_accept p=%p", &priv->uv_socket);
//        }
//        uv_not_accept((uv_stream_t *)uv_server_socket);
//        log_error(0,
//            "gobj",         "%s", gobj_full_name(clisrv),
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
//            "msg",          "%s", "TCP Gobj must be RUNNING",
//            NULL
//        );
        return -1;
    }

    /*-------------------------------------*
     *      Accept connection
     *-------------------------------------*/
    if(gobj_trace_level(clisrv) & TRACE_UV) {
        // TODO trace_msg(">>> tcp accept p=%p", &priv->uv_socket);
    }
// TODO   int err = uv_accept((uv_stream_t *)uv_server_socket, (uv_stream_t*)&priv->uv_socket);
//    if (err != 0) {
//        log_error(0,
//            "gobj",         "%s", gobj_full_name(clisrv),
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_LIBUV_ERROR,
//            "msg",          "%s", "uv_accept FAILED",
//            "uv_error",     "%s", uv_err_name(err),
//            NULL
//        );
//        return -1;
//    }
//    set_connected(clisrv);

    return 0;
}