/***********************************************************************
 *          C_SERIAL.C
 *          Serial GClass.
 *
 *          Manage Serial Ports uv-mixin
 *
 *          Partial source code from https://github.com/vsergeev/c-periphery
 *
 *          Copyright (c) 2021 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include "c_serial.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef enum serial_parity {
    PARITY_NONE,
    PARITY_ODD,
    PARITY_EVEN,
} serial_parity_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int open_tty(hgobj gobj);
PRIVATE void on_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
PRIVATE void on_read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
PRIVATE void on_close_cb(uv_handle_t* handle);
PRIVATE void do_close(hgobj gobj);
PRIVATE int do_write(hgobj gobj, gbuffer_t *gbuf);


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag------------default----------description---------- */
SDATA (DTP_INTEGER,     "txMsgs",           SDF_RD|SDF_RSTATS, 0,           "Messages transmitted"),
SDATA (DTP_INTEGER,     "rxMsgs",           SDF_RD|SDF_RSTATS, 0,           "Messages receiveds"),

SDATA (DTP_STRING,      "lHost",            SDF_RD,         "",             "Connex compatible, no use"),
SDATA (DTP_STRING,      "lPort",            SDF_RD,         "",             "Connex compatible, no use"),
SDATA (DTP_STRING,      "rHost",            SDF_RD,         "",             "Connex compatible, TTY Port"),
SDATA (DTP_STRING,      "rPort",            SDF_RD,         "",             "Connex compatible, no use"),

SDATA (DTP_INTEGER,     "baudrate",         SDF_RD,         9600,           "Baud rate"),
SDATA (DTP_INTEGER,     "bytesize",         SDF_RD,         8,              "Byte size"),
SDATA (DTP_STRING,      "parity",           SDF_RD,         "none",         "Parity"),
SDATA (DTP_INTEGER,     "stopbits",         SDF_RD,         1,              "Stop bits"),
SDATA (DTP_BOOLEAN,     "xonxoff",          SDF_RD,         0,              "xonxoff"),
SDATA (DTP_BOOLEAN,     "rtscts",           SDF_RD,         0,              "rtscts"),

SDATA (DTP_STRING,      "connected_event_name",SDF_RD,      EV_CONNECTED, "Must be empty if you don't want receive this event"),
SDATA (DTP_STRING,      "disconnected_event_name",SDF_RD,   EV_DISCONNECTED, "Must be empty if you don't want receive this event"),
SDATA (DTP_STRING,      "tx_ready_event_name",SDF_RD,       EV_TX_READY,  "Must be empty if you don't want receive this event"),
SDATA (DTP_STRING,      "rx_data_event_name",SDF_RD,        EV_RX_DATA,   "Must be empty if you don't want receive this event"),
SDATA (DTP_STRING,      "stopped_event_name",SDF_RD,        EV_STOPPED,   "Stopped event name"),

SDATA (DTP_POINTER,     "user_data",        0,              0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,              0,              "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,              0,              "subscriber of output-events. If it's null then subscriber is the parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendent value!
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
    // Conf
    char port[64];
    const char *connected_event_name;
    const char *tx_ready_event_name;
    const char *rx_data_event_name;
    const char *disconnected_event_name;
    const char *stopped_event_name;
    BOOL inform_disconnection;

    uint64_t txMsgs;
    uint64_t rxMsgs;

    uv_tty_t uv_tty;
    char uv_handler_active;
    char uv_read_active;
    /* Write request type. Careful attention must be paid when reusing objects of this type.
     * When a stream is in non-blocking mode, write requests sent with uv_write will be queued.
     * Reusing objects at this point is undefined behaviour.
     * It is safe to reuse the uv_write_t object only after the callback passed to uv_write is fired.
     */
    uv_write_t uv_req_write;
    char uv_req_write_active;
    int tty_fd;
    grow_buffer_t bfinput;
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


    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(connected_event_name,          gobj_read_str_attr)
    SET_PRIV(tx_ready_event_name,           gobj_read_str_attr)
    SET_PRIV(rx_data_event_name,            gobj_read_str_attr)
    SET_PRIV(disconnected_event_name,       gobj_read_str_attr)
    SET_PRIV(stopped_event_name,            gobj_read_str_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
//     PRIVATE_DATA *priv = gobj_priv_data(gobj);
//
//     IF_EQ_SET_PRIV(tx_ready_event_name,   gobj_read_str_attr)
//     END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->uv_handler_active) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "GObj NOT STOPPED. UV handler ACTIVE!",
            NULL
        );
    }
    if(priv->uv_read_active) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "UV req_read ACTIVE",
            NULL
        );
    }
    if(priv->uv_req_write_active) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "UV req_write ACTIVE",
            NULL
        );
    }

    growbf_reset(&priv->bfinput);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    if(open_tty(gobj) < 0) {
        do_close(gobj);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    do_close(gobj);
    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void set_connected(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->inform_disconnection = TRUE;
    gobj_change_state(gobj, ST_CONNECTED);

    if(gobj_trace_level(gobj) & TRACE_UV) {
        gobj_trace_msg(gobj, ">>> uv_read_start tcp p=%p", &priv->uv_tty);
    }

    priv->uv_read_active = 1;
    uv_read_start((uv_stream_t*)&priv->uv_tty, on_alloc_cb, on_read_cb);

    /*
     *  Info of "connected"
     */
    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Connected",
            "msg2",         "%s", "Connected🔵",
            "port",         "%s", priv->port,
            NULL
        );
    }

    json_t *kw_ev = json_pack("{s:s}",
        "port", priv->port
    );
    // TODO error de diseño, si se cambia connected_event_name la publicación fallará
    // porque el evento no estará en output_events list.
    gobj_publish_event(gobj, priv->connected_event_name, kw_ev);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void set_disconnected(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Info of "disconnected"
     */
    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Disconnected",
            "msg2",         "%s", "Disconnected🔴",
            "port",         "%s", priv->port,
            NULL
        );
    }

    gobj_change_state(gobj, ST_STOPPED);

    if(priv->inform_disconnection) {
        priv->inform_disconnection = FALSE;
        gobj_publish_event(gobj, priv->disconnected_event_name, 0);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _serial_baudrate_to_bits(int baudrate)
{
    switch (baudrate) {
        case 50: return B50;
        case 75: return B75;
        case 110: return B110;
        case 134: return B134;
        case 150: return B150;
        case 200: return B200;
        case 300: return B300;
        case 600: return B600;
        case 1200: return B1200;
        case 1800: return B1800;
        case 2400: return B2400;
        case 4800: return B4800;
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 500000: return B500000;
        case 576000: return B576000;
        case 921600: return B921600;
        case 1000000: return B1000000;
        case 1152000: return B1152000;
        case 1500000: return B1500000;
        case 2000000: return B2000000;
#ifdef B2500000
        case 2500000: return B2500000;
#endif
#ifdef B3000000
        case 3000000: return B3000000;
#endif
#ifdef B3500000
        case 3500000: return B3500000;
#endif
#ifdef B4000000
        case 4000000: return B4000000;
#endif
        default: return -1;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int configure_tty(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    struct termios termios_settings;
    if(tcgetattr(priv->tty_fd, &termios_settings)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "tcgetattr() FAILED",
            "tty",          "%s", priv->port,
            "fd",           "%d", priv->tty_fd,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    /*-----------------------------*
     *      Baud rate
     *-----------------------------*/
    int baudrate_ = gobj_read_integer_attr(gobj, "baudrate");
    int baudrate = _serial_baudrate_to_bits(baudrate_);
    if(baudrate == -1) {
        baudrate = B9600;
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "tty",          "%s", priv->port,
            "fd",           "%d", priv->tty_fd,
            "msg",          "%s", "Bad baudrate",
            "baudrate",     "%d", baudrate_,
            NULL
        );
    }

    /*-----------------------------*
     *      Parity
     *-----------------------------*/
    serial_parity_t parity;
    const char *sparity = gobj_read_str_attr(gobj, "parity");
    SWITCHS(sparity) {
        CASES("odd")
            parity = PARITY_ODD;
            break;
        CASES("even")
            parity = PARITY_EVEN;
            break;
        CASES("none")
            parity = PARITY_NONE;
            break;
        DEFAULTS
            parity = PARITY_NONE;
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Parity UNKNOWN",
                "tty",          "%s", priv->port,
                "fd",           "%d", priv->tty_fd,
                "parity",       "%s", sparity,
                NULL
            );
            break;
    } SWITCHS_END;

    /*-----------------------------*
     *      Byte size
     *-----------------------------*/
    int bytesize = gobj_read_integer_attr(gobj, "bytesize");
    switch(bytesize) {
        case 5:
        case 6:
        case 7:
        case 8:
            break;
        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Bad bytesize",
                "tty",          "%s", priv->port,
                "fd",           "%d", priv->tty_fd,
                "bytesize",     "%d", bytesize,
                NULL
            );
            bytesize = 8;
            break;
    }

    /*-----------------------------*
     *      Stop bits
     *-----------------------------*/
    int stopbits = gobj_read_integer_attr(gobj, "stopbits");
    switch(stopbits) {
        case 1:
        case 2:
            break;
        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Bad stopbits",
                "tty",          "%s", priv->port,
                "fd",           "%d", priv->tty_fd,
                "stopbits",     "%d", bytesize,
                NULL
            );
            stopbits = 1;
            break;
    }

    /*-----------------------------*
     *      Control
     *-----------------------------*/
    int xonxoff = gobj_read_bool_attr(gobj, "xonxoff");
    int rtscts = gobj_read_bool_attr(gobj, "rtscts");

    /* c_iflag */

    /* Ignore break characters */
    termios_settings.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);

    if (parity != PARITY_NONE)
        termios_settings.c_iflag |= INPCK;
    /* Only use ISTRIP when less than 8 bits as it strips the 8th bit */
    if (parity != PARITY_NONE && bytesize != 8)
        termios_settings.c_iflag |= ISTRIP;
    if (xonxoff)
        termios_settings.c_iflag |= (IXON | IXOFF);

    /* c_oflag */
    termios_settings.c_oflag &= ~OPOST;

    /* c_lflag */
    termios_settings.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN); // No echo

    /* c_cflag */
    /* Enable receiver, ignore modem control lines */
    termios_settings.c_cflag = CREAD | CLOCAL;

    /* Databits */
    if (bytesize == 5)
        termios_settings.c_cflag |= CS5;
    else if (bytesize == 6)
        termios_settings.c_cflag |= CS6;
    else if (bytesize == 7)
        termios_settings.c_cflag |= CS7;
    else if (bytesize == 8)
        termios_settings.c_cflag |= CS8;

    /* Parity */
    if (parity == PARITY_EVEN)
        termios_settings.c_cflag |= PARENB;
    else if (parity == PARITY_ODD)
        termios_settings.c_cflag |= (PARENB | PARODD);

    /* Stopbits */
    if (stopbits == 2)
        termios_settings.c_cflag |= CSTOPB;

    /* RTS/CTS */
    if (rtscts) {
        termios_settings.c_cflag |= CRTSCTS;
    }

    /* Baudrate */
    cfsetispeed(&termios_settings, baudrate);
    cfsetospeed(&termios_settings, baudrate);

    if(tcsetattr(priv->tty_fd, TCSANOW, &termios_settings)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "tcsetattr() FAILED",
            "tty",          "%s", priv->port,
            "fd",           "%d", priv->tty_fd,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int open_tty(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *port = gobj_read_str_attr(gobj, "rHost");
    if(empty_string(port)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "tty port (rHost attr) EMPTY",
            NULL
        );
        return -1;
    }

    if(count_char(port, '/')==0) {
        snprintf(priv->port, sizeof(priv->port), "/dev/%s", port);
    } else {
        snprintf(priv->port, sizeof(priv->port), "%s", port);
    }

    do {
        priv->tty_fd = open(priv->port, O_RDWR, 0);
        if(priv->tty_fd < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
                "msg",          "%s", "Cannot open serial device",
                "tty",          "%s", priv->port,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            return -1;
        }

        if(gobj_trace_level(gobj) & TRACE_UV) {
            gobj_trace_msg(gobj, ">>> uv_init TTY p=%p", &priv->uv_tty);
        }
        int e;
        if((e=uv_tty_init(yuno_uv_event_loop(), &priv->uv_tty, priv->tty_fd, 0))<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
                "msg",          "%s", "uv_tty_init() FAILED",
                "tty",          "%s", priv->port,
                "error",        "%d", e,
                "uv_strerror",  "%s", uv_strerror(e),
                NULL
            );
            return -1;
        }
        priv->uv_tty.data = gobj;
        priv->uv_handler_active = 1;

        if((e=uv_tty_set_mode(&priv->uv_tty, UV_TTY_MODE_RAW))<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
                "msg",          "%s", "uv_tty_set_mode() FAILED",
                "tty",          "%s", priv->port,
                "error",        "%d", e,
                "uv_strerror",  "%s", uv_strerror(e),
                NULL
            );
            return -1;
        }

        if(configure_tty(gobj)<0) {
            // Error already logged
            return -1;
        } else {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "tty opened",
                "tty",          "%s", priv->port,
                "fd",           "%d", priv->tty_fd,
                NULL
            );
            set_connected(gobj);
        }

    } while(0);

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
        gobj_trace_msg(gobj, "<<< on_read_cb %d TTY p=%p",
            (int)nread,
            &priv->uv_tty
        );
    }

    if(nread < 0) {
        if(nread == UV_ECONNRESET) {
            gobj_log_info(gobj, 0,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "Connection Reset",
                "msg2",         "%s", "Connection Reset🔴",
                NULL
            );
        } else if(nread == UV_EOF) {
            gobj_log_info(gobj, 0,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "EOF",
                "msg2",         "%s", "EOF🔴",
                NULL
            );
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBUV_ERROR,
                "msg",          "%s", "read FAILED",
                "msg2",         "%s", "read FAILED🔴",
                "uv_error",     "%s", uv_err_name(nread),
                NULL
            );
        }

        do_close(gobj); // hay que reintentar el open: pueden meter/sacar un usb
        return;
    }

    if(nread == 0) {
        // Yes, sometimes arrive with nread 0.
        return;
    }

    priv->rxMsgs++;

    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        gobj_trace_dump(gobj, 
            LOG_DUMP_INPUT,
            buf->base,
            nread,
            "TTY %s",
            priv->port
        );
    }

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

    json_t *kw = json_pack("{s:I}",
        "gbuffer", (json_int_t)(size_t)gbuf
    );
    gobj_publish_event(gobj, priv->rx_data_event_name, kw);
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
        gobj_trace_msg(gobj, "<<< on_close_cb TTY p=%p", &priv->uv_tty);
    }

    priv->uv_handler_active = 0;

    close(priv->tty_fd);
    priv->tty_fd = -1;

    set_disconnected(gobj);

    if(gobj_is_volatil(gobj)) {
        gobj_destroy(gobj);
    } else {
        gobj_publish_event(gobj, priv->stopped_event_name, 0);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void do_close(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->uv_handler_active) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "UV handler NOT ACTIVE!",
            NULL
        );
        set_disconnected(gobj);
        return;
    }
    if(priv->uv_read_active) {
        uv_read_stop((uv_stream_t *)&priv->uv_tty);
        priv->uv_read_active = 0;
    }

    uv_tty_set_mode(&priv->uv_tty, UV_TTY_MODE_NORMAL);
    uv_tty_reset_mode();

    if(gobj_trace_level(gobj) & TRACE_UV) {
        gobj_trace_msg(gobj, ">>> uv_close TTY p=%p", &priv->uv_tty);
    }
    gobj_change_state(gobj, ST_WAIT_STOPPED);
    uv_close((uv_handle_t *)&priv->uv_tty, on_close_cb);
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
        gobj_trace_msg(gobj, ">>> on_write_cb TTY p=%p",
            &priv->uv_tty
        );
    }

    if(status != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBUV_ERROR,
            "msg",          "%s", "on_write_cb FAILED",
            "status",       "%d", status,
            NULL
        );
    }

    if(!empty_string(priv->tx_ready_event_name)) {
        gobj_publish_event(gobj, priv->tx_ready_event_name, 0);
    }
}

/***************************************************************************
 *  Send data
 ***************************************************************************/
PRIVATE int do_write(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->uv_req_write_active) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "uv_req_write ALREADY ACTIVE",
            NULL
        );
    }

    priv->uv_req_write_active = 1;
    priv->uv_req_write.data = gobj;

    size_t ln = gbuffer_chunk(gbuf); // y si ln es 0??????????

    char *bf = gbuffer_get(gbuf, ln);
    uv_buf_t b[] = {
        {.base = bf, .len = ln}
    };
    uint32_t trace = gobj_trace_level(gobj);
    if((trace & TRACE_UV)) {
        gobj_trace_msg(gobj, ">>> uv_write TTY p=%p, send %d\n", &priv->uv_tty, (int)ln);
    }
    int ret = uv_write(
        &priv->uv_req_write,
        (uv_stream_t*)&priv->uv_tty,
        b,
        1,
        on_write_cb
    );
    if(ret < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBUV_ERROR,
            "msg",          "%s", "uv_write FAILED",
            "uv_error",     "%s", uv_err_name(ret),
            "ln",           "%d", ln,
            NULL
        );
        do_close(gobj);
        return -1;
    }

    if((trace & TRACE_TRAFFIC)) {
        gobj_trace_dump(gobj, 
            LOG_DUMP_OUTPUT,
            bf,
            ln,
            "TTY %s",
            priv->port
        );
    }

    priv->txMsgs++;

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
    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

    int ret = do_write(gobj, gbuf);

    KW_DECREF(kw);
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    do_close(gobj);
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = {
    {EV_TX_DATA,          0},
    {EV_DROP,             0},
    {NULL, 0}
};
PRIVATE const EVENT output_events[] = {
    {EV_RX_DATA,          0},
    {EV_TX_READY,         0},
    {EV_CONNECTED,        0},
    {EV_DISCONNECTED,     0},
    {EV_STOPPED,          0},
    {NULL, 0}
};
PRIVATE const char *state_names[] = {
    ST_STOPPED,
    ST_WAIT_STOPPED,
    ST_WAIT_CONNECTED,
    ST_WAIT_DISCONNECTED, /* order is important. Below the connected states */
    ST_CONNECTED,
    NULL
};

PRIVATE EV_ACTION ST_STOPPED[] = {
    {0,0,0}
};
PRIVATE EV_ACTION ST_WAIT_STOPPED[] = {
    {0,0,0}
};
PRIVATE EV_ACTION ST_WAIT_CONNECTED[] = {
    {EV_DROP,         ac_drop,                0},
    {0,0,0}
};
PRIVATE EV_ACTION ST_WAIT_DISCONNECTED[] = {
    {0,0,0}
};
PRIVATE EV_ACTION ST_CONNECTED[] = {
    {EV_TX_DATA,      ac_tx_data,             0},
    {EV_DROP,         ac_drop,                0},
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_STOPPED,
    ST_WAIT_STOPPED,
    ST_WAIT_CONNECTED,
    ST_WAIT_DISCONNECTED,
    ST_CONNECTED,
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
    GCLASS_SERIAL_NAME,
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
        0, //mt_command_parser,
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
        0, //mt_authzs,
        0, //mt_create_node,
        0, //mt_update_node,
        0, //mt_delete_node,
        0, //mt_link_nodes,
        0, //mt_link_nodes2,
        0, //mt_unlink_nodes,
        0, //mt_unlink_nodes2,
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
        0, //mt_node_instances,
        0, //mt_save_node,
        0, //mt_topic_size,
        0, //mt_future62,
        0, //mt_future63,
        0, //mt_future64
    },
    lmt,
    tattr_desc,
    sizeof(PRIVATE_DATA),
    0,  // s_user_authz_level
    s_user_trace_level,
    0,  // cmds
    gcflag_manual_start, // gcflag
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC GCLASS *gclass_serial(void)
{
    return &_gclass;
}
