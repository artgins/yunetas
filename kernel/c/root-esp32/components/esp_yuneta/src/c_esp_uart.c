/****************************************************************************
 *          c_esp_uart.c
 *
 *          GClass Uart: work with uart
 *          Low level esp-idf
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#ifdef ESP_PLATFORM
  #include <esp_event.h>
  #include <c_esp_yuno.h>
  #include <driver/uart.h>
  #include <driver/gpio.h>
#endif
#include <kwid.h>
#include "c_esp_uart.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
#ifdef ESP_PLATFORM
PRIVATE void rx_task(void *pv);
PRIVATE void uart_tx_ev_loop_callback(
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
PRIVATE const sdata_desc_t tattr_desc[] = {
/*-ATTR-type--------name----------------flag------------default-----description---------- */
SDATA (DTP_INTEGER, "mode",             SDF_PERSIST,    "0",        "0 UART, 1 RS485_HALF_DUPLEX, 2 IRDA, 3 RS485_COLLISION_DETECT, 4 RS485_APP_CTRL"),
SDATA (DTP_INTEGER, "uart_number",      SDF_PERSIST,    "0",        "Uart number"),
SDATA (DTP_INTEGER, "gpio_rx",          SDF_PERSIST,    "-1",       "Gpio rx (3 console uart0"),
SDATA (DTP_INTEGER, "gpio_tx",          SDF_PERSIST,    "-1",       "Gpio Tx (1 console uart0"),
SDATA (DTP_INTEGER, "baudrate",         SDF_PERSIST,    "9600",     "Messages transmitted"),
SDATA (DTP_INTEGER, "bytesize",         SDF_PERSIST,    "3",        "Byte size: 2=7bits, 3=8bits"),
SDATA (DTP_INTEGER, "parity",           SDF_PERSIST,    "0",        "Parity: 0 none, 2 even, 3 odd"),
SDATA (DTP_INTEGER, "stopbits",         SDF_PERSIST,    "1",        "Stop bits: 1=1bit, 2=1.5bits, 3=2bits"),
SDATA (DTP_INTEGER, "flowcontrol",      SDF_PERSIST,    "0",        "0 DISABLE, 1 RTS, 2 CTS, 3 CTS_RTS"),
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
    TRACE_DUMP_TRAFFIC          = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"traffic",             "Trace dump traffic"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
#ifdef ESP_PLATFORM
    QueueHandle_t uart_queue;
    TaskHandle_t  rx_task_h;                // Task to read
    esp_event_loop_handle_t tx_ev_loop_h;   // event loop with task to tx messages through task's callback
#endif
    volatile BOOL task_running;
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
#ifdef ESP_PLATFORM
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*---------------------------------*
     *  Create event loop to transmit
     *---------------------------------*/
    esp_event_loop_args_t loop_handle_args = {
        .queue_size = 8,
        .task_name = "uart-tx-queue", // task will be created
        .task_priority = tskIDLE_PRIORITY,
        .task_stack_size = 2*1024,  // esp32 stack size
        .task_core_id = tskNO_AFFINITY
    };
    ESP_ERROR_CHECK(esp_event_loop_create(&loop_handle_args, &priv->tx_ev_loop_h));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(
        priv->tx_ev_loop_h,         // event loop handle
        ESP_EVENT_ANY_BASE,         // event base
        ESP_EVENT_ANY_ID,           // event id
        uart_tx_ev_loop_callback,        // event handler
        gobj,                       // event_handler_arg
        NULL                        // event handler instance, useful to unregister callback
    ));
#endif

    if(!gobj_is_pure_child(gobj)) {
        /*
         *  Not pure child, explicitly use subscriber
         */
        hgobj subscriber = (hgobj)(size_t)gobj_read_integer_attr(gobj, "subscriber");
        if(subscriber) {
            gobj_subscribe_event(gobj, NULL, NULL, subscriber);
        }
    }
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    //PRIVATE_DATA *priv = gobj_priv_data(gobj);
    //IF_EQ_SET_PRIV(timeout_inactivity,  (int) gobj_read_integer_attr)
    //END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    int uart_number = (int) gobj_read_integer_attr(gobj, "uart_number");

#ifdef ESP_PLATFORM
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = (int)gobj_read_integer_attr(gobj, "baudrate"),
        .data_bits = (int)gobj_read_integer_attr(gobj, "bytesize"),
        .parity    = (int)gobj_read_integer_attr(gobj, "parity"),
        .stop_bits = (int)gobj_read_integer_attr(gobj, "stopbits"),
        .flow_ctrl = (int)gobj_read_integer_attr(gobj, "flowcontrol"),
        .source_clk = UART_SCLK_DEFAULT,
    };

    int intr_alloc_flags = 0; // ESP_INTR_FLAG_IRAM;

    uart_driver_delete(uart_number);

    ESP_ERROR_CHECK(uart_param_config(uart_number, &uart_config));

    esp_err_t err = uart_set_pin(
        uart_number,
        (int)gobj_read_integer_attr(gobj, "gpio_tx"),
        (int)gobj_read_integer_attr(gobj, "gpio_rx"),
        -1, // RTS,
        -1  // CTS
    );
    if(err != ESP_OK) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "uart_set_pin() FAILED",
            "esp_error",    "%s", esp_err_to_name(err),
            "uart_number",  "%d", (int)uart_number,
            "gpio_tx",      "%d", (int)gobj_read_integer_attr(gobj, "gpio_tx"),
            "gpio_rx",      "%d", (int)gobj_read_integer_attr(gobj, "gpio_rx"),
            NULL
        );
    }
    ESP_ERROR_CHECK(uart_driver_install(
        uart_number,
        1*1024,             // rx_buffer_size
        1*1024,             // tx_buffer_size
        20,                 // event_queue_size
        &priv->uart_queue,  // uart_queue handle
        intr_alloc_flags
    ));

    if(!priv->rx_task_h) {
        portBASE_TYPE ret = xTaskCreate(
            rx_task,
            "uart-rx-task",
            2*1024, // esp32 stack size
            gobj,
            tskIDLE_PRIORITY,
            &priv->rx_task_h
        );
        if(ret != pdPASS) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Cannot create Uart Task",
                NULL
            );
        }
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Uart Task Already exists",
            NULL
        );
    }
#endif

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "Start Uart",
        "uart",         "%d", uart_number,
        "gpio_tx",      "%d", (int)gobj_read_integer_attr(gobj, "gpio_tx"),
        "gpio_rx",      "%d", (int)gobj_read_integer_attr(gobj, "gpio_rx"),
        NULL
    );

    if(gobj_is_pure_child(gobj)) {
        gobj_send_event(gobj_parent(gobj), EV_CONNECTED, 0, gobj);
    } else {
        gobj_publish_event(gobj, EV_CONNECTED, 0);
    }

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
        /* A running client cannot be stopped from the MQTT task/event handler */
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

    uart_driver_delete((int) gobj_read_integer_attr(gobj, "uart_number"));
#endif

    if(gobj_is_pure_child(gobj)) {
        gobj_send_event(gobj_parent(gobj), EV_DISCONNECTED, 0, gobj);
    } else {
        gobj_publish_event(gobj, EV_DISCONNECTED, 0);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
#ifdef ESP_PLATFORM
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

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
PRIVATE void rx_task(void *pv)
{
    hgobj gobj = pv;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uart_event_t event;
    int uart_number = (int) gobj_read_integer_attr(gobj, "uart_number");

    priv->task_running = true;
    while(priv->task_running) {
        if(xQueueReceive(priv->uart_queue, (void *)&event, 1/portTICK_PERIOD_MS)) {
            switch(event.type) {
                case UART_DATA:
                    {
                        size_t len = 0;
                        uart_get_buffered_data_len(uart_number, &len);
                        if(len > 0) {
                            gbuffer_t *gbuf = gbuffer_create(len, len);
                            if(gbuf) {
                                char *p = gbuffer_cur_wr_pointer(gbuf);
                                uart_read_bytes(uart_number, p, len, 0);
                                gbuffer_set_wr(gbuf, len);
                                json_t *kw = json_pack("{s:I}",
                                    "gbuffer", (json_int_t)(size_t)gbuf
                                );
                                gobj_post_event(gobj, EV_RX_DATA, kw, gobj);
                            }
                        }
                    }
                    break;
                case UART_FIFO_OVF:
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(uart_number);
                    xQueueReset(priv->uart_queue);
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                        "msg",          "%s", "UART_FIFO_OVF",
                        NULL
                    );
                    break;
                case UART_BUFFER_FULL:
                    // If buffer full happened, you should consider encreasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(uart_number);
                    xQueueReset(priv->uart_queue);
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                        "msg",          "%s", "UART_BUFFER_FULL",
                        NULL
                    );
                    break;
                case UART_BREAK:
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                        "msg",          "%s", "UART_BREAK",
                        NULL
                    );
                    break;
                case UART_PARITY_ERR:
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                        "msg",          "%s", "UART_PARITY_ERR",
                        NULL
                    );
                    break;
                case UART_FRAME_ERR:
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                        "msg",          "%s", "UART_FRAME_ERR",
                        NULL
                    );
                    break;
                case UART_DATA_BREAK:
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                        "msg",          "%s", "UART_DATA_BREAK",
                        NULL
                    );
                    break;
                case UART_PATTERN_DET:
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                        "msg",          "%s", "UART_PATTERN_DET",
                        NULL
                    );
                    break;
                default:
                    break;
            }
        }
    }

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
PRIVATE void uart_tx_ev_loop_callback(
    void *event_handler_arg,
    esp_event_base_t base,
    int32_t id,
    void *event_data
) {
    /*
     *  Manage tx_ev_loop_h
     */
    hgobj gobj = event_handler_arg;

    int uart_number = (int) gobj_read_integer_attr(gobj, "uart_number");
    gbuffer_t *gbuf = *((gbuffer_t **) event_data);

    int trozo = 0;
    int len, wlen;
    char *bf;
    while((len = (int)gbuffer_leftbytes(gbuf))) {
        bf = gbuffer_cur_rd_pointer(gbuf);
        wlen = uart_write_bytes(uart_number, bf, len);
        if (wlen < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "uart_write_bytes() FAILED",
                NULL
            );
            break;

        } else if (wlen == 0) {
            // Timeout
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "uart_write_bytes() return 0",
                NULL
            );
            break;
        }

        if(gobj_trace_level(gobj) & TRACE_DUMP_TRAFFIC) {
            gobj_trace_dump(gobj, bf, wlen, "%s: (trozo %d)",
                gobj_short_name(gobj),
                ++trozo
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
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_trace_level(gobj) & TRACE_DUMP_TRAFFIC) {
        gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
        gobj_trace_dump_gbuf(gobj, gbuf, "%s",
            gobj_short_name(gobj)
        );
    }
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
            "msg",          "%s", "esp_event_post_to(Uart TX) FAILED",
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
GOBJ_DEFINE_GCLASS(C_ESP_UART);

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
    ev_action_t st_idle[] = {
        {EV_RX_DATA,            ac_rx_data,                 0},
        {EV_TX_DATA,            ac_tx_data,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,           st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TX_DATA,        0},
        {EV_RX_DATA,        EVF_OUTPUT_EVENT},
        {EV_CONNECTED,      EVF_OUTPUT_EVENT},
        {EV_DISCONNECTED,   EVF_OUTPUT_EVENT},
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
        0   // gclass_flag
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
PUBLIC int register_c_esp_uart(void)
{
    return create_gclass(C_ESP_UART);
}
