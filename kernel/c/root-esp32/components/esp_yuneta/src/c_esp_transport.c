/****************************************************************************
 *          c_esp_transport.c
 *
 *          GClass Transport: work with esp-idf transport: tcp,ssl,...
 *          Low level esp-idf
 *
 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#ifdef ESP_PLATFORM
  #include <esp_event.h>
  #include <esp_transport.h>
  #include <esp_transport_tcp.h>
  #include <esp_transport_ssl.h>
  #include <c_esp_yuno.h>
#endif

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_states.h>
#include <helpers.h>
#include "c_esp_transport.h"

#ifdef ESP_PLATFORM
extern int esp_transport_get_socket(esp_transport_handle_t t);
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
typedef enum {
    TASK_TRANSPORT_STOPPED = 0,
    TASK_TRANSPORT_DISCONNECTED,
    TASK_TRANSPORT_CONNECTED,
    TASK_TRANSPORT_WAIT_RECONNECT,
} transport_state_t;

/***************************************************************
 *              Prototypes
 ***************************************************************/
#ifdef ESP_PLATFORM
PRIVATE void rx_task(void *pv);
PRIVATE void transport_tx_ev_loop_callback(
    void *event_handler_arg,
    esp_event_base_t base,
    int32_t id,
    void *event_data
);
#endif

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE const sdata_desc_t tattr_desc[] = { // WARNING repeated in c_linux_transport/c_esp_transport
/*-ATTR-type--------name----------------flag------------default-----description---------- */
SDATA (DTP_INTEGER, "connxs",           SDF_STATS,      "0",        "connection counter"),
SDATA (DTP_BOOLEAN, "connected",        SDF_VOLATIL|SDF_STATS, "false", "Connection state. Important filter!"),

SDATA (DTP_STRING,  "ifr_name",         SDF_RD,         "",         "Interface to use"),
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
SDATA (DTP_INTEGER, "timeout_between_connections", SDF_WR|SDF_PERSIST, "6000", "Idle timeout to wait between attempts of connection, in miliseconds"),
SDATA (DTP_INTEGER, "timeout_inactivity", SDF_WR|SDF_PERSIST, "-1", "Inactivity timeout in miliseconds to close the connection. Reconnect when new data arrived. With -1 never close."),

SDATA (DTP_INTEGER, "txBytes",          SDF_VOLATIL|SDF_STATS, "0", "Messages transmitted"),
SDATA (DTP_INTEGER, "rxBytes",          SDF_VOLATIL|SDF_STATS, "0", "Messages received"),
SDATA (DTP_INTEGER, "txMsgs",           SDF_VOLATIL|SDF_STATS, "0", "Messages transmitted"),
SDATA (DTP_INTEGER, "rxMsgs",           SDF_VOLATIL|SDF_STATS, "0", "Messages received"),
SDATA (DTP_STRING,  "peername",         SDF_VOLATIL|SDF_STATS, "",  "Peername"),
SDATA (DTP_STRING,  "sockname",         SDF_VOLATIL|SDF_STATS, "",  "Sockname"),
SDATA (DTP_INTEGER, "max_tx_queue",     SDF_WR,         "0",        "Maximum messages in tx queue. Default is 0: no limit."),
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
#ifdef ESP_PLATFORM
    esp_transport_handle_t transport;
    esp_transport_keep_alive_t keep_alive_cfg;
    transport_state_t transport_state;
    TaskHandle_t  rx_task_h;                // Task to read/connect
    esp_event_loop_handle_t tx_ev_loop_h;   // event loop with task to tx messages through task's callback
    struct ifreq ifr;   /* only has ifr_name as field, of 6 bytes length */
    char buf_rx[1024];
#endif
    volatile BOOL task_running;
    volatile int dynamic_read_timeout;
    BOOL inform_disconnection;
    BOOL use_ssl;
    const char *url;
} PRIVATE_DATA;






                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char schema[16];
    char host[120];
    char port[10];

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
    gobj_write_str_attr(gobj, "host", host);
    gobj_write_str_attr(gobj, "port", port);

#ifdef ESP_PLATFORM
    if(priv->use_ssl) {
        priv->transport = esp_transport_ssl_init();

        const char *cert_pem = gobj_read_str_attr(gobj, "cert_pem");
        if(!empty_string(cert_pem)) {
            esp_transport_ssl_set_cert_data(priv->transport, cert_pem, (int)strlen(cert_pem));
        }
        if(gobj_read_bool_attr(gobj, "skip_cert_cn")) {
            esp_transport_ssl_skip_common_name_check(priv->transport);
        }
    } else {
        priv->transport = esp_transport_tcp_init();
    }

    int keep_alive = (int)gobj_read_integer_attr(gobj, "keep_alive");
    if(keep_alive > 0) {
        memset(&priv->keep_alive_cfg, 0, sizeof(priv->keep_alive_cfg));
        priv->keep_alive_cfg.keep_alive_enable = true;
        priv->keep_alive_cfg.keep_alive_idle = 5;
        priv->keep_alive_cfg.keep_alive_interval = 5;
        priv->keep_alive_cfg.keep_alive_count = 3;
        esp_transport_tcp_set_keep_alive(priv->transport, &priv->keep_alive_cfg);
    }

    /*---------------------------------*
     *  Create event loop to transmit
     *---------------------------------*/
    esp_event_loop_args_t loop_handle_args = {
        .queue_size = 8,
        .task_name = "trans-tx-queue", // task will be created
        .task_priority = tskIDLE_PRIORITY,
        .task_stack_size = 4*1024,  // esp32 stack size
        .task_core_id = tskNO_AFFINITY
    };
    ESP_ERROR_CHECK(esp_event_loop_create(&loop_handle_args, &priv->tx_ev_loop_h));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(
        priv->tx_ev_loop_h,         // event loop handle
        ESP_EVENT_ANY_BASE,         // event base
        ESP_EVENT_ANY_ID,           // event id
        transport_tx_ev_loop_callback,        // event handler
        gobj,                       // event_handler_arg
        NULL                        // event handler instance, useful to unregister callback
    ));
#endif

    gobj_subscribe_event(gobj_yuno(), EV_TIMEOUT_PERIODIC, NULL, gobj);

    /*
     *  CHILD subscription model TODO check
     */
    hgobj subscriber = (hgobj)(size_t)gobj_read_integer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

//    if(!gobj_is_pure_child(gobj)) {
//        /*
//         *  Not pure child, explicitly use subscriber
//         */
//        hgobj subscriber = (hgobj)(size_t)gobj_read_integer_attr(gobj, "subscriber");
//        if(subscriber) {
//            gobj_subscribe_event(gobj, NULL, NULL, subscriber);
//        }
//    }

    //SET_PRIV(timeout_inactivity,    (int)gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(url,     gobj_read_str_attr)
        if (!empty_string(priv->url)) {
            char host[120];
            char port[10];

            if(parse_url(
                gobj,
                priv->url,
                0, 0,
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
            } else {
                gobj_write_str_attr(gobj, "host", host);
                gobj_write_str_attr(gobj, "port", port);
            }
        }
    END_EQ_SET_PRIV()
}

#ifdef HACK_ESP
/*
 *  Add this function to esp_event.c
 *  and the prototype to esp_event.h
 *      size_t esp_event_queue_size(esp_event_loop_handle_t event_loop); // GMS
 */
size_t esp_event_queue_size(esp_event_loop_handle_t event_loop) // GMS
{
    esp_event_loop_instance_t *loop = (esp_event_loop_instance_t *) event_loop;
    if(!loop->queue) {
        return 0;
    }
    return uxQueueMessagesWaiting(loop->queue);
}
#endif

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->task_running) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "gobj asked to start task, but task was already started",
            NULL
        );
        return -1;
    }

    if(empty_string(gobj_read_str_attr(gobj, "host"))) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot start transport, NO host defined",
            NULL
        );
        return -1;
    }

    if(!gobj_read_bool_attr(gobj, "character_device")) {
        if(empty_string(gobj_read_str_attr(gobj, "port"))) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Cannot start transport, NO port defined",
                NULL
            );
        }
    }

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
    priv->dynamic_read_timeout = 100;

#ifdef ESP_PLATFORM

    const char *ifr_name = gobj_read_str_attr(gobj, "ifr_name");
    if(!empty_string(ifr_name)) {
        memmove(priv->ifr.ifr_name, ifr_name, MIN(sizeof(priv->ifr.ifr_name), strlen(ifr_name)));
        esp_transport_tcp_set_interface_name(priv->transport, &priv->ifr);

        if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "Using ifr_name",
                "msg2",         "%s", "Using ifr_name 🌏 ✅",
                "ifr_name",     "%s", ifr_name,
                NULL
            );
        }
    }

    portBASE_TYPE ret = xTaskCreate(
        rx_task,
        "trans-rx-task",
        4*1024, // esp32 stack size
        gobj,
        tskIDLE_PRIORITY,
        &priv->rx_task_h
    );
    if(ret != pdPASS) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot create Transport Task",
            NULL
        );
    }

#endif

//    if(!gobj_read_bool_attr(gobj, "__clisrv__")) {
//        /*
//         * pure tcp client: try to connect
//         */
//        // HACK firstly set timeout, EV_CONNECTED can be received inside gobj_start()
//        set_timeout(priv->gobj_timer, gobj_read_integer_attr(gobj, "timeout_waiting_connected"));
//        gobj_change_state(gobj, "ST_WAIT_CONNECTED");
//    }

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
#ifdef ESP_PLATFORM
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->task_running) {
        /* A running client cannot be stopped from the task/event handler */
        TaskHandle_t running_task = xTaskGetCurrentTaskHandle();
        if(running_task == priv->rx_task_h) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "gobj cannot be stopped from ESP task",
                NULL
            );
            //return -1;
        }
        priv->task_running = FALSE;
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "gobj asked to stop task, but task was not started",
            NULL
        );
        //return -1;
    }
#endif

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
#ifdef ESP_PLATFORM
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    EXEC_AND_RESET(esp_transport_destroy, priv->transport)

    if(priv->tx_ev_loop_h) {
        esp_event_loop_delete(priv->tx_ev_loop_h);
        priv->tx_ev_loop_h = 0;
    }
#endif
}




                    /***************************
                     *      Local methods
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
#ifdef ESP_PLATFORM
//PRIVATE const char *transport_state_name(transport_state_t transport_state)
//{
//    switch(transport_state) {
//        case TASK_TRANSPORT_STOPPED:
//            return "TASK_TRANSPORT_STOPPED";
//        case TASK_TRANSPORT_DISCONNECTED:
//            return "TASK_TRANSPORT_DISCONNECTED";
//        case TASK_TRANSPORT_CONNECTED:
//            return "TASK_TRANSPORT_CONNECTED";
//        case TASK_TRANSPORT_WAIT_RECONNECT:
//            return "TASK_TRANSPORT_WAIT_RECONNECT";
//    }
//    return "???";
//}
#endif

/***************************************************************************
 *
 ***************************************************************************/
#ifdef ESP_PLATFORM
PRIVATE void rx_task(void *pv)
{
    hgobj gobj = pv;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int err;

    priv->transport_state = TASK_TRANSPORT_DISCONNECTED;
    priv->task_running = true;
    while(priv->task_running) {
//        gobj_log_warning(gobj, 0,
//            "msgset",       "%s", MSGSET_INFO,
//            "msg",          "%s", "Transport Task LOOP",
//            "task state",   "%s", transport_state_name(priv->transport_state),
//            "gobj state",   "%s", gobj_current_state(gobj),
//            NULL
//        );
        switch(priv->transport_state) {
            case TASK_TRANSPORT_STOPPED:
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "wrong TRANSPORT_STOPPED state",
                    NULL
                );
                // esp_abort_connection
                esp_transport_close(priv->transport);
                break;

            case TASK_TRANSPORT_DISCONNECTED:
                {
                    const char *host = gobj_read_str_attr(gobj, "host");
                    int port = atoi(gobj_read_str_attr(gobj, "port"));
                    int timeout_ms = (int)gobj_read_integer_attr(gobj, "timeout_waiting_connected");
                    err = esp_transport_connect(
                        priv->transport,
                        host,
                        port,
                        timeout_ms
                    );
                    if(err < 0) {
                        int actual_errno = esp_transport_get_errno(priv->transport);
                        gobj_log_error(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                            "msg",          "%s", "esp_transport_connect() FAILED",
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
                            "schema",       "%s", gobj_read_str_attr(gobj, "schema"),
                            "host",         "%s", host,
                            "port",         "%d", port,
                            "timeout_ms",   "%d", timeout_ms,
                            "err",          "%d", err,
                            "errno",        "%d", actual_errno,
                            "serrno",       "%s", strerror(actual_errno),
                            NULL
                        );
                        // esp_abort_connection
                        esp_transport_close(priv->transport);
                        priv->transport_state = TASK_TRANSPORT_WAIT_RECONNECT;

                        gobj_post_event(gobj, EV_DISCONNECTED, 0, gobj);
                        break;
                    }

                    int s = esp_transport_get_socket(priv->transport);
                    struct sockaddr_in addr;
                    socklen_t addrlen = sizeof(addr);
                    char temp[64];
                    if(getpeername(s, (struct sockaddr *)&addr, &addrlen)==0) {
                        snprintf(temp, sizeof(temp), "%s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
                        gobj_write_str_attr(gobj, "peername", temp);
                    }
                    if(getsockname(s, (struct sockaddr *)&addr, &addrlen)==0) {
                        snprintf(temp, sizeof(temp), "%s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
                        gobj_write_str_attr(gobj, "sockname", temp);
                    }

                    priv->transport_state = TASK_TRANSPORT_CONNECTED;
                    gobj_post_event(gobj, EV_CONNECTED, 0, gobj);
                }
                break;

            case TASK_TRANSPORT_CONNECTED:
                {
                    /*-----------------------------*
                     *      Try to Read
                     *-----------------------------*/
                    int read_len = esp_transport_read(
                        priv->transport,
                        priv->buf_rx,
                        (int)sizeof(priv->buf_rx),
                        priv->dynamic_read_timeout // read_poll_timeout_ms
                    );
                    if(read_len < 0) {
                        int actual_errno = esp_transport_get_errno(priv->transport);
                        gobj_log_error(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                            "msg",          "%s", "esp_transport_read() FAILED",
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
                            "err",          "%d", read_len,
                            "errno",        "%d", actual_errno,
                            "serrno",       "%s", strerror(actual_errno),
                            NULL
                        );
                        priv->dynamic_read_timeout = 20;

                        // esp_abort_connection
                        esp_transport_close(priv->transport);
                        priv->transport_state = TASK_TRANSPORT_WAIT_RECONNECT;

                        gobj_post_event(gobj, EV_DISCONNECTED, 0, gobj);
                        break;
                    } else if(read_len == 0) {
                        priv->dynamic_read_timeout = 20;
                    } else {
                        priv->dynamic_read_timeout = 0;
                        gbuffer_t *gbuf = gbuffer_create(read_len, read_len);
                        gbuffer_append(gbuf, priv->buf_rx, read_len);

                        json_t *kw = json_pack("{s:I}",
                            "gbuffer", (json_int_t)(size_t)gbuf
                        );
                        gobj_post_event(gobj, EV_RX_DATA, kw, gobj);
                    }
                }
                break;

            case TASK_TRANSPORT_WAIT_RECONNECT:
                vTaskDelay(gobj_read_integer_attr(gobj, "timeout_between_connections")/portTICK_PERIOD_MS);
                priv->transport_state = TASK_TRANSPORT_DISCONNECTED;
                break;
        }
    }

    esp_transport_close(priv->transport);
    gobj_post_event(gobj, EV_DISCONNECTED, 0, gobj);

    priv->transport_state = TASK_TRANSPORT_STOPPED;
    gobj_post_event(gobj, EV_STOPPED, 0, gobj);

    priv->rx_task_h = NULL;
    vTaskDelete(NULL);
}

/***************************************************************************
 *  Two types of data can be passed in to the event handler:
 *      - the handler specific data
 *      - the event-specific data.
 *
 *  - The handler specific data (handler_args) is a pointer to the original data,
 *  therefore, the user should ensure that the memory location it points to
 *  is still valid when the handler executes.
 *
 *  - The event-specific data (event_data) is a pointer to a deep copy of the original data,
 *  and is managed automatically.
***************************************************************************/
PRIVATE void transport_tx_ev_loop_callback(
    void *event_handler_arg,
    esp_event_base_t base,
    int32_t id,
    void *event_data
) {
    /*
     *  Manage tx_ev_loop_h
     */
    hgobj gobj = event_handler_arg;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = *((gbuffer_t **) event_data);

    int trozo = 0;
    int len, wlen;
    char *bf;
    while((len = (int)gbuffer_leftbytes(gbuf))) {
        bf = gbuffer_cur_rd_pointer(gbuf);
        wlen = esp_transport_write(priv->transport, bf, len, 2*1000);
        if (wlen < 0) {
            int actual_errno = esp_transport_get_errno(priv->transport);
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "esp_transport_write() FAILED",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                "errno",        "%d", actual_errno,
                "serrno",       "%s", strerror(actual_errno),
                NULL
            );
            esp_transport_close(priv->transport);
            break;

        } else if (wlen == 0) {
            // Timeout
            int actual_errno = esp_transport_get_errno(priv->transport);
            if(actual_errno != 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "esp_transport_write() return 0",
                    "errno",        "%d", actual_errno,
                    "serrno",       "%s", strerror(actual_errno),
                    NULL
                );
                esp_transport_close(priv->transport); // TODO ??? se ha cerrado el socket o tx buffer lleno?
            }
            break;
        }

        if(gobj_trace_level(gobj) & TRACE_DUMP_TRAFFIC) {
            gobj_trace_dump(gobj, bf, wlen, "%s: (trozo %d) %s%s%s",
                gobj_short_name(gobj),
                ++trozo,
                gobj_read_str_attr(gobj, "sockname"),
                " -> ",
                gobj_read_str_attr(gobj, "peername")
            );
        }

        /*
         *  Pop sent data
         */
        gbuffer_get(gbuf, wlen);
    }

    GBUFFER_DECREF(gbuf)
}
#endif




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_bool_attr(gobj, "connected", FALSE);

    if(priv->inform_disconnection) {
        priv->inform_disconnection = FALSE;

        if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "Disconnected",
                "msg2",         "%s", "Disconnected🔴",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                "peername",     "%s", gobj_read_str_attr(gobj, "peername"),
                "sockname",     "%s", gobj_read_str_attr(gobj, "sockname"),
                NULL
            );
        }

        if(gobj_is_pure_child(gobj)) {
            gobj_send_event(gobj_parent(gobj), EV_DISCONNECTED, 0, gobj);
        } else {
            gobj_publish_event(gobj, EV_DISCONNECTED, 0);
        }
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_bool_attr(gobj, "connected", TRUE);

//    INCR_ATTR_INTEGER(connxs)

    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Connected",
            "msg2",         "%s", "Connected🔵",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            "peername",     "%s", gobj_read_str_attr(gobj, "peername"),
            "sockname",     "%s", gobj_read_str_attr(gobj, "sockname"),
            NULL
        );
    }

    priv->inform_disconnection = TRUE;

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

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

    if(gobj_trace_level(gobj) & TRACE_DUMP_TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "%s: %s%s%s",
            gobj_short_name(gobj),
            gobj_read_str_attr(gobj, "sockname"),
            " <- ",
            gobj_read_str_attr(gobj, "peername")
        );
    }

//    INCR_ATTR_INTEGER(rxMsgs)
//    INCR_ATTR_INTEGER2(rxBytes, gbuffer_leftbytes(gbuf))

    if(gobj_is_pure_child(gobj)) {
        gobj_send_event(gobj_parent(gobj), EV_RX_DATA, kw, gobj); // use the same kw
    } else {
        gobj_publish_event(gobj, EV_RX_DATA, kw); // use the same kw
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED|KW_EXTRACT);
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

//    INCR_ATTR_INTEGER(txMsgs)
//    INCR_ATTR_INTEGER2(txBytes, gbuffer_leftbytes(gbuf))

#ifdef ESP_PLATFORM
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    esp_err_t err = esp_event_post_to(
        priv->tx_ev_loop_h,
        event,
        0,
        &gbuf,
        sizeof(gbuffer_t *),
        2
    );
    if(err != ESP_OK) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "esp_event_post_to(Transport TX) FAILED",
            "esp_error",    "%s", esp_err_to_name(err),
            NULL
        );
        GBUFFER_DECREF(gbuf)
        KW_DECREF(kw)
        return -1;
    }
#endif

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{

#ifdef ESP_PLATFORM
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    esp_transport_close(priv->transport);
#endif

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_periodic_off(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_periodic_on(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
//    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_periodic_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_pure_child(gobj)) {
        gobj_send_event(gobj_parent(gobj), EV_STOPPED, 0, gobj);
    } else {
        gobj_publish_event(gobj, EV_STOPPED, 0);
    }

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
GOBJ_DEFINE_GCLASS(C_ESP_TRANSPORT);

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
    ev_action_t st_disconnected[] = {
        {EV_CONNECTED,          ac_connected,               ST_CONNECTED},
        {EV_DISCONNECTED,       ac_disconnected,            0},
        {EV_TIMEOUT_PERIODIC,   ac_timeout_periodic_off,    0},
        {EV_DROP,               ac_drop,                    0},
        {EV_STOPPED,            ac_stopped,                 ST_STOPPED},
        {0,0,0}
    };
    ev_action_t st_connected[] = {
        {EV_RX_DATA,            ac_rx_data,                 0},
        {EV_TX_DATA,            ac_tx_data,                 0},
        {EV_DISCONNECTED,       ac_disconnected,            ST_DISCONNECTED},
        {EV_TIMEOUT_PERIODIC,   ac_timeout_periodic_on,     0},
        {EV_DROP,               ac_drop,                    ST_DISCONNECTED},
        {0,0,0}
    };
    ev_action_t st_stopped[] = {
        {EV_TIMEOUT_PERIODIC,   ac_timeout_periodic_stopped,0},
        {EV_CONNECTED,          ac_connected,               ST_CONNECTED},
        {0,0,0}
    };

    states_t states[] = {
        {ST_DISCONNECTED,       st_disconnected},
        {ST_CONNECTED,          st_connected},
        {ST_STOPPED,            st_stopped},
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
PUBLIC int register_c_esp_transport(void)
{
    return create_gclass(C_ESP_TRANSPORT);
}
