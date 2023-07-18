/****************************************************************************
 *          c_linux_uart.c
 *
 *          GClass Uart: work with tty ports
 *          Low level linux
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <gbuffer.h>
#include <c_timer.h>
#include <c_linux_yuno.h>
#include <kwid.h>
#include "yunetas_ev_loop.h"
#include "c_linux_uart.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/
typedef enum serial_parity {
    PARITY_NONE,
    PARITY_ODD,
    PARITY_EVEN,
} serial_parity_t;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void set_connected(hgobj gobj, int fd);
PRIVATE void set_disconnected(hgobj gobj, const char *cause);
PRIVATE int yev_tty_callback(yev_event_t *event);

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type--------name----------------flag------------default-----description---------- */
SDATA (DTP_INTEGER, "connxs",           SDF_STATS,      "0",        "connection counter"),
SDATA (DTP_BOOLEAN, "connected",        SDF_VOLATIL|SDF_STATS, "false", "Connection state. Important filter!"),
SDATA (DTP_STRING,  "path",             SDF_RD,         "/dev/ttyUSB0", "Device to open"),

SDATA (DTP_INTEGER, "rx_buffer_size",   SDF_WR|SDF_PERSIST, "4096", "Rx buffer size"),
SDATA (DTP_INTEGER, "timeout_between_connections", SDF_WR|SDF_PERSIST, "2000", "Idle timeout to wait between attempts of connection, in miliseconds"),

SDATA (DTP_INTEGER, "txBytes",          SDF_VOLATIL|SDF_STATS, "0", "Messages transmitted"),
SDATA (DTP_INTEGER, "rxBytes",          SDF_VOLATIL|SDF_STATS, "0", "Messages received"),
SDATA (DTP_INTEGER, "txMsgs",           SDF_VOLATIL|SDF_STATS, "0", "Messages transmitted"),
SDATA (DTP_INTEGER, "rxMsgs",           SDF_VOLATIL|SDF_STATS, "0", "Messages received"),
SDATA (DTP_INTEGER, "txMsgsec",         SDF_VOLATIL|SDF_STATS, "0", "Messages by second"),
SDATA (DTP_INTEGER, "rxMsgsec",         SDF_VOLATIL|SDF_STATS, "0", "Messages by second"),
SDATA (DTP_INTEGER, "maxtxMsgsec",      SDF_VOLATIL|SDF_RSTATS,"0", "Max Messages by second"),
SDATA (DTP_INTEGER, "maxrxMsgsec",      SDF_VOLATIL|SDF_RSTATS,"0", "Max Messages by second"),

SDATA (DTP_INTEGER, "baudrate",         SDF_RD,         "9600",     "Baud rate"),
SDATA (DTP_INTEGER, "bytesize",         SDF_RD,         "8",        "Byte size"),
SDATA (DTP_STRING,  "parity",           SDF_RD,         "none",     "Parity"),
SDATA (DTP_INTEGER, "stopbits",         SDF_RD,         "1",        "Stop bits"),
SDATA (DTP_BOOLEAN, "xonxoff",          SDF_RD,         "0",        "xonxoff"),
SDATA (DTP_BOOLEAN, "rtscts",           SDF_RD,         "0",        "rtscts"),

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
    int tty_fd;
    yev_event_t *yev_client_rx;
    char inform_disconnection;
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

    /*
     *  Child, default subscriber, the parent
     */
    hgobj subscriber = (hgobj)(size_t)gobj_read_integer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);
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

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "Start Uart",
//        "uart",         "%d", uart_number,
//        "gpio_tx",      "%d", (int)gobj_read_integer_attr(gobj, "gpio_tx"),
//        "gpio_rx",      "%d", (int)gobj_read_integer_attr(gobj, "gpio_rx"),
        NULL
    );

    // HACK el start de tty lo hace el timer
    set_timeout(priv->gobj_timer, 100);

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->gobj_timer);

    BOOL change_to_wait_stopped = FALSE;

    if(priv->yev_client_rx) {
        if(yev_event_in_ring(priv->yev_client_rx)) {
            change_to_wait_stopped = TRUE;
        }
        yev_stop_event(priv->yev_client_rx);
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

    EXEC_AND_RESET(yev_destroy_event, priv->yev_client_rx);
}




                    /***************************
                     *      Local methods
                     ***************************/




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
PRIVATE int configure_tty(hgobj gobj, int fd)
{
    struct termios termios_settings;
    if(tcgetattr(fd, &termios_settings)<0) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "tcgetattr() FAILED",
            "path",         "%s", gobj_read_str_attr(gobj, "path"),
            "fd",           "%d", fd,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    /*-----------------------------*
     *      Baud rate
     *-----------------------------*/
    int baudrate_ = (int)gobj_read_integer_attr(gobj, "baudrate");
    int baudrate = _serial_baudrate_to_bits(baudrate_);
    if(baudrate == -1) {
        baudrate = B9600;
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "path",         "%s", gobj_read_str_attr(gobj, "path"),
            "fd",           "%d", fd,
            "msg",          "%s", "Bad baudrate",
            "baudrate",     "%d", baudrate_,
            NULL
        );
    }

    /*-----------------------------*
     *      Parity
     *-----------------------------*/
    serial_parity_t parity = PARITY_NONE;
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
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Parity UNKNOWN",
                "path",         "%s", gobj_read_str_attr(gobj, "path"),
                "fd",           "%d", fd,
                "parity",       "%s", sparity,
                NULL
            );
            break;
    } SWITCHS_END;

    /*-----------------------------*
     *      Byte size
     *-----------------------------*/
    int bytesize = (int)gobj_read_integer_attr(gobj, "bytesize");
    switch(bytesize) {
        case 5:
        case 6:
        case 7:
        case 8:
            break;
        default:
            gobj_log_error(gobj, 0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Bad bytesize",
                "path",         "%s", gobj_read_str_attr(gobj, "path"),
                "fd",           "%d", fd,
                "bytesize",     "%d", bytesize,
                NULL
            );
            bytesize = 8;
            break;
    }

    /*-----------------------------*
     *      Stop bits
     *-----------------------------*/
    int stopbits = (int)gobj_read_integer_attr(gobj, "stopbits");
    switch(stopbits) {
        case 1:
        case 2:
            break;
        default:
            gobj_log_error(gobj, 0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Bad stopbits",
                "path",         "%s", gobj_read_str_attr(gobj, "path"),
                "fd",           "%d", fd,
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

    if(tcsetattr(fd, TCSANOW, &termios_settings)<0) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "tcsetattr() FAILED",
            "path",         "%s", gobj_read_str_attr(gobj, "path"),
            "fd",           "%d", fd,
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
    const char *path = gobj_read_str_attr(gobj, "path");
    if(empty_string(path)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "path EMPTY",
            NULL
        );
        return -1;
    }

    int fd = open(path, O_RDWR, 0);
    if(fd < 0) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "Cannot open path",
            "path",         "%s", path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    if(configure_tty(gobj, fd)<0) {
        // Error already logged
        close(fd);
        return -1;
    } else {
        gobj_log_info(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "tty opened",
            "path",         "%s", gobj_read_str_attr(gobj, "path"),
            "fd",           "%d", fd,
            NULL
        );
    }

    return fd;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void set_connected(hgobj gobj, int fd)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->tty_fd = fd;

    /*
     *  Info of "connected"
     */
    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Connected",
            "msg2",         "%s", "Connected🔵",
            "path",         "%s", gobj_read_str_attr(gobj, "path"),
            NULL
        );
    }

    clear_timeout(priv->gobj_timer);
    gobj_change_state(gobj, ST_CONNECTED);

    INCR_ATTR_INTEGER(connxs)

    /*
     *  Ready to receive
     */
    if(!priv->yev_client_rx) {
        priv->yev_client_rx = yev_create_read_event(
            yuno_event_loop(),
            yev_tty_callback,
            gobj,
            fd
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
    json_t *kw_conn = json_pack("{s:s}",
        "path",     gobj_read_str_attr(gobj, "path")
    );
    gobj_publish_event(gobj, EV_CONNECTED, kw_conn);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void set_disconnected(hgobj gobj, const char *cause)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_current_state(gobj)==ST_DISCONNECTED) {
        if(gobj_is_running(gobj)) {
            set_timeout(
                priv->gobj_timer,
                gobj_read_integer_attr(gobj, "timeout_between_connections")
            );
        }
        return;
    }
    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Disconnected",
            "msg2",         "%s", "Disconnected🔴",
            "cause",        "%s", cause?cause:"",
            "path",         "%s", gobj_read_str_attr(gobj, "path"),
            NULL
        );
    }

    if(gobj_is_running(gobj)) {
        gobj_change_state(gobj, ST_DISCONNECTED);
    }

    if(priv->tty_fd > 0) {
        if(gobj_trace_level(gobj) & TRACE_UV) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_YEV_LOOP,
                "msg",          "%s", "close socket",
                "msg2",         "%s", "💥🟥 close socket",
                "fd",           "%d", priv->tty_fd ,
                NULL
            );
        }

        close(priv->tty_fd);
        priv->tty_fd = -1;
    }

    if(priv->yev_client_rx) {
        yev_set_fd(priv->yev_client_rx, -1);
        yev_stop_event(priv->yev_client_rx);
    }

    if(gobj_is_running(gobj)) {
        set_timeout(
            priv->gobj_timer,
            gobj_read_integer_attr(gobj, "timeout_between_connections")
        );
    }

    /*
     *  Info of "disconnected"
     */
    if(priv->inform_disconnection) {
        priv->inform_disconnection = FALSE;
        gobj_publish_event(gobj, EV_DISCONNECTED, 0);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int yev_tty_callback(yev_event_t *yev_event)
{
    hgobj gobj = yev_event->gobj;

    if(gobj_trace_level(gobj) & TRACE_UV) {
        json_t *jn_flags = bits2str(yev_flag_strings(), yev_event->flag);
        gobj_log_info(gobj, 0,
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
    switch(yev_event->type) {
        case YEV_READ_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Disconnected
                     */
                    if(gobj_trace_level(gobj) & TRACE_UV) {
                        if(yev_event->result != -ECANCELED) {
                            gobj_log_info(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                                "msg",          "%s", "read FAILED",
                                "path",         "%s", gobj_read_str_attr(gobj, "path"),
                                "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                                "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                                "errno",        "%d", -yev_event->result,
                                "strerror",     "%s", strerror(-yev_event->result),
                                "p",            "%p", yev_event,
                                NULL
                            );
                        }
                    }
                    set_disconnected(gobj, strerror(-yev_event->result));

                } else {
                    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
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
                    yev_start_event(yev_event);
                }
            }
            break;

        case YEV_WRITE_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Disconnected
                     */
                    if(gobj_trace_level(gobj) & TRACE_UV) {
                        if(yev_event->result != -ECANCELED) {
                            gobj_log_info(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                                "msg",          "%s", "write FAILED",
                                "path",         "%s", gobj_read_str_attr(gobj, "path"),
                                "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                                "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                                "errno",        "%d", -yev_event->result,
                                "strerror",     "%s", strerror(-yev_event->result),
                                "p",            "%p", yev_event,
                                NULL
                            );
                        }
                    }
                    set_disconnected(gobj, strerror(-yev_event->result));

                } else {
                    json_int_t mark = (json_int_t)gbuffer_getmark(yev_event->gbuf);
                    if(yev_event->flag & YEV_FLAG_WANT_TX_READY) {
                        json_t *kw_tx_ready = json_object();
                        json_object_set_new(kw_tx_ready, "gbuffer_mark", json_integer(mark));
                        gobj_publish_event(gobj, EV_TX_READY, kw_tx_ready);
                    }
                }

                yev_destroy_event(yev_event);
            }
            break;

        default:
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "event type NOT IMPLEMENTED",
                "path",         "%s", gobj_read_str_attr(gobj, "path"),
                "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                "event_type",   "%s", yev_event_type_name(yev_event),
                "p",            "%p", yev_event,
                NULL
            );
            break;
    }

    return gobj_is_running(gobj)?0:-1;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Timeout to start connection
 ***************************************************************************/
PRIVATE int ac_timeout_disconnected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_send_event(gobj, EV_CONNECT, 0, gobj);

    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_connect(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    int fd = open_tty(gobj);
    if(fd < 0) {
        set_disconnected(gobj, strerror(errno));
    } else {
        set_connected(gobj, fd);
    }

    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL want_tx_ready = kw_get_bool(gobj, kw, "want_tx_ready", 0, 0);
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

    /*
     *  Transmit
     */
    yev_event_t *yev_client_tx = yev_create_write_event(
        yuno_event_loop(),
        yev_tty_callback,
        gobj,
        priv->tty_fd
    );
    yev_set_gbuffer(
        yev_client_tx,
        gbuf
    );
    yev_set_flag(yev_client_tx, YEV_FLAG_WANT_TX_READY, want_tx_ready);
    yev_start_event(yev_client_tx);

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
GOBJ_DEFINE_GCLASS(C_LINUX_UART);

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
    ev_action_t st_connected[] = {
        {EV_TX_DATA,            ac_tx_data,                 0},
        {0,0,0}
    };
    ev_action_t st_wait_stopped[] = {
        {0,0,0}
    };

    states_t states[] = {
        {ST_STOPPED,            st_stopped},
        {ST_DISCONNECTED,       st_disconnected},
        {ST_CONNECTED,          st_connected},
        {ST_WAIT_STOPPED,       st_wait_stopped},
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
        gcflag_manual_start // gclass_flag TODO is needed?
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
PUBLIC int register_c_linux_uart(void)
{
    return create_gclass(C_LINUX_UART);
}
