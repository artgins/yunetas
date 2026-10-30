/****************************************************************************
 *          c_uart.c
 *
 *          GClass Uart: work with tty ports
 *          Low level linux
 *
 *          The stop contract, the one of C_TCP: every stop of a running
 *          C_UART ends in ST_STOPPED, with its tty closed, EV_DISCONNECTED
 *          published if it was connected, and then EV_STOPPED, ONCE --
 *          synchronously, inside gobj_stop(), when its read was not in
 *          flight (a stop inside its own EV_RX_DATA, or while
 *          disconnected), or later, from ST_WAIT_STOPPED, when the cancel
 *          of its read completes. EV_STOPPED is the last thing it does with
 *          itself; so is EV_DISCONNECTED when a host destroys it there.
 *          A host declares EV_DISCONNECTED and EV_STOPPED in its FSM.
 *          A stop, a drop (EV_DROP) or a disconnection does not wait for
 *          the writes: the write in flight is canceled, the output it left
 *          in the tty is discarded, and the writes queued behind it go.
 *          A start in ST_WAIT_STOPPED is refused (-1): the stop goes on.
 *
 *          The writes go one at a time, in order: a short write (the line
 *          took part of it) sends its rest before the next, and EV_TX_READY
 *          is published when the last write queued is done. The queue is
 *          bounded by max_tx_queue (0, the default: no bound, as C_TCP);
 *          full, it drops its oldest write, with an error.
 *
 *          Up to 7.29.3 a stop stopped the read and nothing else: inside its
 *          EV_RX_DATA the tty stayed open (overwritten, so leaked, by the
 *          next start, whose EV_CONNECTED reached a host still connected),
 *          with the read in flight it stayed in ST_WAIT_STOPPED for ever,
 *          EV_STOPPED was never published, and the callback of a stopped
 *          C_UART answered -1 to the event loop, which ENDS the loop: the
 *          yuno went down. A destroy with a write in flight was a use after
 *          free when the write completed, a destroy in ST_WAIT_STOPPED left
 *          the tty open, a short write lost its rest (and published
 *          EV_TX_READY), and EV_DROP was not an event of it.
 *
 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <fcntl.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include <yev_loop.h>
#include "c_timer.h"
#include "c_yuno.h"
#include "c_uart.h"

/***************************************************************
 *              Constants
 ***************************************************************/
#define RECONNECT_FLOOR_MS  1000    // the least time between connections (0 is raised to it)

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
PRIVATE int set_disconnected(hgobj gobj, const char *cause);
PRIVATE void try_to_stop_yevents(hgobj gobj); // IDEMPOTENT
PRIVATE int write_next(hgobj gobj);
PRIVATE void cut_the_writes(hgobj gobj);
PRIVATE int yev_callback(yev_event_h yev_event);
PRIVATE json_int_t time_to_reconnect(hgobj gobj);

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE const sdata_desc_t attrs_table[] = {
/*-ATTR-type--------name----------------flag------------default-----description---------- */
SDATA (DTP_INTEGER, "connxs",           SDF_STATS,      "0",        "connection counter"),
SDATA (DTP_BOOLEAN, "connected",        SDF_VOLATIL|SDF_STATS,      "false", "Connection state. Important filter!"),
SDATA (DTP_STRING,  "url",              SDF_RD,         "",         "Device to open (url by compatibility with C_TCP) to open: i.e. /dev/ttyUSB0"),
SDATA (DTP_STRING,  "cert_pem",         SDF_RD,         "",         "Not used, by compatibility with C_TCP"),

SDATA (DTP_INTEGER, "rx_buffer_size",   SDF_WR|SDF_PERSIST, "4096", "Rx buffer size"),
SDATA (DTP_INTEGER, "timeout_between_connections", SDF_WR|SDF_PERSIST, "2000", "Idle timeout to wait between attempts of connection, in milliseconds"),
SDATA (DTP_INTEGER, "max_tx_queue",     SDF_WR,         "0",        "Maximum writes waiting behind the one in flight; when full the oldest is dropped, with an error. 0: no limit (as C_TCP)"),

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
/*
 *  A publish of an event hands control to the subscribers, which may stop
 *  the C_UART or, through its host, destroy the gobj: the call that
 *  published keeps a marker on its stack, the markers are chained, and
 *  mt_destroy() clears them all. Read after the publish: FALSE, the gobj is
 *  gone and nothing of it may be touched. As in C_TCP.
 */
typedef struct alive_s {
    BOOL alive;
    struct alive_s *prev;
} alive_t;

typedef struct _PRIVATE_DATA {
    hgobj gobj_timer;
    int tty_fd;
    yev_event_h yev_client_rx;
    yev_event_h yev_client_tx;  // the write in flight: one at a time
    dl_list_t dl_tx;            // the gbuffers waiting for it, in order
    json_int_t max_tx_queue;
    json_int_t tx_queue_dropped;    // writes dropped by a full queue, said 1 of max_tx_queue
    char inform_disconnection;
    BOOL reconnect_floor_said;  // the warning of a 0 between connections, once per gobj
    alive_t *alive;             // the markers of the publishes in progress (see alive_t)
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

    priv->gobj_timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    priv->tty_fd = -1;
    dl_init(&priv->dl_tx, gobj);

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)(uintptr_t)gobj_read_integer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(max_tx_queue,          gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(max_tx_queue,        gobj_read_integer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  A start while a stop waits for the cancel of its read
     *  (ST_WAIT_STOPPED) is refused, as in C_TCP: the stop goes on, and ends
     *  in ST_STOPPED and EV_STOPPED. Up to 7.29.3 the start went on: its
     *  timer fired in ST_WAIT_STOPPED ("Event NOT DEFINED") and the stop
     *  was lost.
     */
    gobj_state_t state = gobj_current_state(gobj);
    if(!(state == ST_STOPPED || state == ST_DISCONNECTED)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Initial wrong task state",
            "state",        "%s", gobj_current_state(gobj),
            NULL
        );
        return -1;
    }

    gobj_start(priv->gobj_timer);

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

    clear_timeout(priv->gobj_timer);

    try_to_stop_yevents(gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(alive_t *marker = priv->alive; marker; marker = marker->prev) {
        marker->alive = FALSE;  // destroyed inside a publish: its caller must not go on
    }

    /*
     *  Destroyed with a stop not ended (ST_WAIT_STOPPED), or a write in
     *  flight: its events are canceled and freed by the loop when their
     *  completions come, with no callback; the tty is closed here. Up to
     *  7.29.3 the write events were not kept, and their completion ran on
     *  the freed gobj; and the tty of ST_WAIT_STOPPED was never closed.
     */
    cut_the_writes(gobj);
    if(priv->yev_client_rx) {
        if(!yev_event_is_stopped(priv->yev_client_rx)) {
            yev_stop_event(priv->yev_client_rx);
        }
        EXEC_AND_RESET(yev_destroy_event, priv->yev_client_rx)
    }
    if(priv->tty_fd >= 0) {
        close(priv->tty_fd);
        priv->tty_fd = -1;
    }
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
// PRIVATE int configure_tty(hgobj gobj, int fd)
// {
//     struct termios termios_settings;
//     if(tcgetattr(fd, &termios_settings)<0) {
//         gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//             "function",     "%s", __FUNCTION__,
//             "msgset",       "%s", MSGSET_PROTOCOL,
//             "msg",          "%s", "tcgetattr() FAILED",
//             "url",          "%s", gobj_read_str_attr(gobj, "url"),
//             "fd",           "%d", fd,
//             "errno",        "%d", errno,
//             "strerror",     "%s", strerror(errno),
//             NULL
//         );
//         return -1;
//     }
//
//     /*-----------------------------*
//      *      Baud rate
//      *-----------------------------*/
//     int baudrate_ = (int)gobj_read_integer_attr(gobj, "baudrate");
//     int baudrate = _serial_baudrate_to_bits(baudrate_);
//     if(baudrate == -1) {
//         baudrate = B9600;
//         gobj_log_error(gobj, 0,
//             "function",     "%s", __FUNCTION__,
//             "msgset",       "%s", MSGSET_PARAMETER,
//             "url",          "%s", gobj_read_str_attr(gobj, "url"),
//             "fd",           "%d", fd,
//             "msg",          "%s", "Bad baudrate",
//             "baudrate",     "%d", baudrate_,
//             NULL
//         );
//     }
//
//     /*-----------------------------*
//      *      Parity
//      *-----------------------------*/
//     serial_parity_t parity = PARITY_NONE;
//     const char *sparity = gobj_read_str_attr(gobj, "parity");
//     SWITCHS(sparity) {
//         CASES("odd")
//             parity = PARITY_ODD;
//             break;
//         CASES("even")
//             parity = PARITY_EVEN;
//             break;
//         CASES("none")
//             parity = PARITY_NONE;
//             break;
//         DEFAULTS
//             parity = PARITY_NONE;
//             gobj_log_error(gobj, 0,
//                 "function",     "%s", __FUNCTION__,
//                 "msgset",       "%s", MSGSET_PARAMETER,
//                 "msg",          "%s", "Parity UNKNOWN",
//                 "url",          "%s", gobj_read_str_attr(gobj, "url"),
//                 "fd",           "%d", fd,
//                 "parity",       "%s", sparity,
//                 NULL
//             );
//             break;
//     } SWITCHS_END;
//
//     /*-----------------------------*
//      *      Byte size
//      *-----------------------------*/
//     int bytesize = (int)gobj_read_integer_attr(gobj, "bytesize");
//     switch(bytesize) {
//         case 5:
//         case 6:
//         case 7:
//         case 8:
//             break;
//         default:
//             gobj_log_error(gobj, 0,
//                 "function",     "%s", __FUNCTION__,
//                 "msgset",       "%s", MSGSET_PARAMETER,
//                 "msg",          "%s", "Bad bytesize",
//                 "url",          "%s", gobj_read_str_attr(gobj, "url"),
//                 "fd",           "%d", fd,
//                 "bytesize",     "%d", bytesize,
//                 NULL
//             );
//             bytesize = 8;
//             break;
//     }
//
//     /*-----------------------------*
//      *      Stop bits
//      *-----------------------------*/
//     int stopbits = (int)gobj_read_integer_attr(gobj, "stopbits");
//     switch(stopbits) {
//         case 1:
//         case 2:
//             break;
//         default:
//             gobj_log_error(gobj, 0,
//                 "function",     "%s", __FUNCTION__,
//                 "msgset",       "%s", MSGSET_PARAMETER,
//                 "msg",          "%s", "Bad stopbits",
//                 "url",          "%s", gobj_read_str_attr(gobj, "url"),
//                 "fd",           "%d", fd,
//                 "stopbits",     "%d", bytesize,
//                 NULL
//             );
//             stopbits = 1;
//             break;
//     }
//
//     /*-----------------------------*
//      *      Control
//      *-----------------------------*/
//     int xonxoff = gobj_read_bool_attr(gobj, "xonxoff");
//     int rtscts = gobj_read_bool_attr(gobj, "rtscts");
//
//     /* c_iflag */
//
//     /* Ignore break characters */
//     termios_settings.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
//
//     if (parity != PARITY_NONE)
//         termios_settings.c_iflag |= INPCK;
//     /* Only use ISTRIP when less than 8 bits as it strips the 8th bit */
//     if (parity != PARITY_NONE && bytesize != 8)
//         termios_settings.c_iflag |= ISTRIP;
//     if (xonxoff)
//         termios_settings.c_iflag |= (IXON | IXOFF);
//
//     /* c_oflag */
//     termios_settings.c_oflag &= ~OPOST;
//
//     /* c_lflag */
//     termios_settings.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN); // No echo
//
//     /* c_cflag */
//     /* Enable receiver, ignore modem control lines */
//     termios_settings.c_cflag = CREAD | CLOCAL;
//
//     /* Databits */
//     if (bytesize == 5)
//         termios_settings.c_cflag |= CS5;
//     else if (bytesize == 6)
//         termios_settings.c_cflag |= CS6;
//     else if (bytesize == 7)
//         termios_settings.c_cflag |= CS7;
//     else if (bytesize == 8)
//         termios_settings.c_cflag |= CS8;
//
//     /* Parity */
//     if (parity == PARITY_EVEN)
//         termios_settings.c_cflag |= PARENB;
//     else if (parity == PARITY_ODD)
//         termios_settings.c_cflag |= (PARENB | PARODD);
//
//     /* Stopbits */
//     if (stopbits == 2)
//         termios_settings.c_cflag |= CSTOPB;
//
//     /* RTS/CTS */
//     if (rtscts) {
//         termios_settings.c_cflag |= CRTSCTS;
//     }
//
//     /* Baudrate */
//     cfsetispeed(&termios_settings, baudrate);
//     cfsetospeed(&termios_settings, baudrate);
//
//     if(tcsetattr(fd, TCSANOW, &termios_settings)<0) {
//         gobj_log_error(gobj, 0,
//             "function",     "%s", __FUNCTION__,
//             "msgset",       "%s", MSGSET_PROTOCOL,
//             "msg",          "%s", "tcsetattr() FAILED",
//             "url",          "%s", gobj_read_str_attr(gobj, "url"),
//             "fd",           "%d", fd,
//             "errno",        "%d", errno,
//             "strerror",     "%s", strerror(errno),
//             NULL
//         );
//         return -1;
//     }
//
//     return 0;
// }

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int configure_tty(hgobj gobj, int fd)
{
    struct termios termios_settings;
    if(tcgetattr(fd, &termios_settings)<0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "tcgetattr() FAILED",
            "device",       "%s", gobj_read_str_attr(gobj, "url"),
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
    /*
     *  A speed the tty does not know runs the line at 9600, and the attr
     *  says so: who counts the time of a frame on the line (the Modbus
     *  master) reads it here. Up to 7.29.3 it kept the speed asked for.
     */
    int baudrate_ = (int)gobj_read_integer_attr(gobj, "baudrate");
    int baudrate = _serial_baudrate_to_bits(baudrate_);
    if(baudrate == -1) {
        baudrate = B9600;
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "device",       "%s", gobj_read_str_attr(gobj, "url"),
            "fd",           "%d", fd,
            "msg",          "%s", "Bad baudrate: the line runs at 9600",
            "baudrate",     "%d", baudrate_,
            NULL
        );
        gobj_write_integer_attr(gobj, "baudrate", 9600);
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
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Parity UNKNOWN: the line runs with none",
                "device",       "%s", gobj_read_str_attr(gobj, "url"),
                "fd",           "%d", fd,
                "parity",       "%s", sparity,
                NULL
            );
            gobj_write_str_attr(gobj, "parity", "none");
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
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Bad bytesize: the line runs with 8",
                "device",       "%s", gobj_read_str_attr(gobj, "url"),
                "fd",           "%d", fd,
                "bytesize",     "%d", bytesize,
                NULL
            );
            bytesize = 8;
            gobj_write_integer_attr(gobj, "bytesize", bytesize);
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
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Bad stopbits: the line runs with 1",
                "device",       "%s", gobj_read_str_attr(gobj, "url"),
                "fd",           "%d", fd,
                "stopbits",     "%d", stopbits,
                NULL
            );
            stopbits = 1;
            gobj_write_integer_attr(gobj, "stopbits", stopbits);
            break;
    }

    /*-----------------------------*
     *      Control
     *-----------------------------*/
    int xonxoff = gobj_read_bool_attr(gobj, "xonxoff");
    int rtscts = gobj_read_bool_attr(gobj, "rtscts");

    /*-------------------------------*
     *      Set termios settings
     *-------------------------------*/
    memset(&termios_settings, 0, sizeof(termios_settings));

    // How libuv does:
    // case UV_TTY_MODE_RAW:
    // termios_settings.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    // termios_settings.c_oflag |= (ONLCR);
    // termios_settings.c_cflag |= (CS8);
    // termios_settings.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    termios_settings.c_cc[VMIN] = 1;
    termios_settings.c_cc[VTIME] = 0;

    /* c_iflag */

    /* Ignore break characters */
    termios_settings.c_iflag = IGNBRK;

    if (parity != PARITY_NONE) {
        termios_settings.c_iflag |= INPCK;
    }
    /* Only use ISTRIP when less than 8 bits as it strips the 8th bit */
    if (parity != PARITY_NONE && bytesize != 8) {
        termios_settings.c_iflag |= ISTRIP;
    }
    if (xonxoff) {
        termios_settings.c_iflag |= (IXON | IXOFF);
    }

    /* c_oflag */
    termios_settings.c_oflag |= (ONLCR);

    /* c_lflag */
    termios_settings.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG ); // No echo

    /* c_cflag */
    /* Enable receiver, ignore modem control lines */
    termios_settings.c_cflag = CREAD | CLOCAL;

    /* Databits */
    if (bytesize == 5) {
        termios_settings.c_cflag |= CS5;
    } else if (bytesize == 6) {
        termios_settings.c_cflag |= CS6;
    } else if (bytesize == 7) {
        termios_settings.c_cflag |= CS7;
    } else if (bytesize == 8) {
        termios_settings.c_cflag |= CS8;
    }

    /* Parity */
    if (parity == PARITY_EVEN) {
        termios_settings.c_cflag |= PARENB;
    } else if (parity == PARITY_ODD) {
        termios_settings.c_cflag |= (PARENB | PARODD);
    }

    /* Stopbits */
    if (stopbits == 2) {
        termios_settings.c_cflag |= CSTOPB;
    }

    /* RTS/CTS */
    if (rtscts) {
        termios_settings.c_cflag |= CRTSCTS;
    }

    /* Baudrate */
    cfsetispeed(&termios_settings, baudrate);
    cfsetospeed(&termios_settings, baudrate);

    if(tcsetattr(fd, TCSANOW, &termios_settings)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "tcsetattr() FAILED",
            "device",       "%s", gobj_read_str_attr(gobj, "url"),
            "fd",           "%d", fd,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    /* Clear both RX and TX queues just in case */
    tcflush(fd, TCIOFLUSH);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int open_tty(hgobj gobj)
{
    const char *path = gobj_read_str_attr(gobj, "url");
    if(empty_string(path)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "path EMPTY",
            NULL
        );
        return -1;
    }

    int fd = open(path, O_RDWR|O_CLOEXEC|O_NOCTTY, 0);
    if(fd < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "Cannot open tty device",
            "url",          "%s", path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    set_nonblocking(fd);

    if(configure_tty(gobj, fd)<0) {
        // Error already logged
        close(fd);
        return -1;
    } else {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "tty opened",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
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

    gobj_write_bool_attr(gobj, "connected", TRUE);

    /*
     *  Info of "connected"
     */
    uint32_t trace_level = gobj_trace_level(gobj);
    if(trace_level & TRACE_CONNECT_DISCONNECT) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Connected",
            "msg2",         "%s", "Connected🔵",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            NULL
        );
    }

    clear_timeout(priv->gobj_timer);
    gobj_change_state(gobj, ST_CONNECTED);

//    INCR_ATTR_INTEGER(connxs)

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
        if(!priv->yev_client_rx) {
            /*
             *  A line that cannot be read is no line: closed, and opened
             *  again after timeout_between_connections. Up to 7.29.3 the
             *  NULL event was used.
             */
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot create the read of the tty: the line is dropped",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                NULL
            );
            set_disconnected(gobj, "Cannot create the read");
            return;
        }
    }

    yev_set_fd(priv->yev_client_rx, fd);
    if(!yev_get_gbuf(priv->yev_client_rx)) {
        json_int_t rx_buffer_size = gobj_read_integer_attr(gobj, "rx_buffer_size");
        yev_set_gbuffer(priv->yev_client_rx, gbuffer_create(rx_buffer_size, rx_buffer_size));
    } else {
        gbuffer_clear(yev_get_gbuf(priv->yev_client_rx));
    }

    yev_start_event(priv->yev_client_rx);

    priv->inform_disconnection = TRUE;

    /*
     *  Publish
     */
    json_t *kw_conn = json_pack("{s:s}",
        "url",      gobj_read_str_attr(gobj, "url")
    );

    gobj_publish_event(gobj, EV_CONNECTED, kw_conn);
}

/***************************************************************************
 *  The time to wait before connecting again. 0 is raised to a floor, said
 *  once: a timer of 0 is no timer (C_TIMER stops), so 0 never reconnected,
 *  and said nothing. -1 keeps meaning no timer.
 ***************************************************************************/
PRIVATE json_int_t time_to_reconnect(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t msec = gobj_read_integer_attr(gobj, "timeout_between_connections");
    if(msec == 0) {
        if(!priv->reconnect_floor_said) {
            priv->reconnect_floor_said = TRUE;
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "timeout_between_connections 0 never reconnects, the floor is used",
                "floor_ms",     "%d", RECONNECT_FLOOR_MS,
                NULL
            );
        }
        msec = RECONNECT_FLOOR_MS;
    }
    return msec;
}

/***************************************************************************
 *  The marker of a publish in progress (see alive_t)
 ***************************************************************************/
PRIVATE void alive_push(PRIVATE_DATA *priv, alive_t *marker)
{
    marker->alive = TRUE;
    marker->prev = priv->alive;
    priv->alive = marker;
}

PRIVATE void alive_pop(PRIVATE_DATA *priv, alive_t *marker)
{
    priv->alive = marker->prev;     // only while the gobj lives
}

/***************************************************************************
 *  The line goes: its writes are cut, its read is stopped and freed (a new
 *  one comes with the next open), its tty closed. Running (a disconnection,
 *  a drop) it waits in ST_DISCONNECTED to open again; at the end of a stop
 *  (ST_STOPPED) it stays there.
 *  EV_DISCONNECTED, once per connection, is the last thing done: its host
 *  may destroy the gobj there. -1 when it did: nothing of it may be touched.
 ***************************************************************************/
PRIVATE int set_disconnected(hgobj gobj, const char *cause)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    BOOL reconnect = gobj_is_running(gobj) && !gobj_in_this_state(gobj, ST_STOPPED);

    gobj_write_bool_attr(gobj, "connected", FALSE);

    if(gobj_current_state(gobj)==ST_DISCONNECTED) {
        if(reconnect) {
            set_timeout(priv->gobj_timer, time_to_reconnect(gobj));
        }
        return 0;
    }
    uint32_t trace_level = gobj_trace_level(gobj);
    if(trace_level & TRACE_CONNECT_DISCONNECT) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Disconnected",
            "msg2",         "%s", "Disconnected🔴",
            "cause",        "%s", cause?cause:"",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            NULL
        );
    }

    if(reconnect) {
        gobj_change_state(gobj, ST_DISCONNECTED);
    }

    cut_the_writes(gobj);   // before the close: what a write left in the tty is discarded

    /*
     *  The read of a connection that went is freed with it, in flight or
     *  not: its completion comes to nobody, and cannot reach the next
     *  connection
     */
    if(priv->yev_client_rx) {
        if(!yev_event_is_stopped(priv->yev_client_rx)) {
            yev_stop_event(priv->yev_client_rx);
        }
        EXEC_AND_RESET(yev_destroy_event, priv->yev_client_rx)
    }

    if(priv->tty_fd >= 0) {
        if(trace_level & TRACE_URING) {
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

    if(reconnect) {
        set_timeout(priv->gobj_timer, time_to_reconnect(gobj));
    }

    /*
     *  Info of "disconnected". A host may destroy the gobj here (see
     *  alive_t)
     */
    if(priv->inform_disconnection) {
        priv->inform_disconnection = FALSE;

        alive_t marker;
        alive_push(priv, &marker);
        gobj_publish_event(gobj, EV_DISCONNECTED, 0);
        if(!marker.alive) {
            return -1;  // the gobj is gone
        }
        alive_pop(priv, &marker);
    }
    return 0;
}

/***************************************************************************
 *  The writes go with the line, and nothing waits for them: the one in
 *  flight is canceled (its event is freed by the loop when its completion
 *  comes, with no callback), the output it left in the tty is discarded
 *  (output held by flow control would hold the close), and the queued ones
 *  are dropped. So a stop never waits for the other side to read.
 ***************************************************************************/
PRIVATE void cut_the_writes(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->yev_client_tx) {
        if(!yev_event_is_stopped(priv->yev_client_tx)) {
            yev_stop_event(priv->yev_client_tx);
        }
        EXEC_AND_RESET(yev_destroy_event, priv->yev_client_tx)

        if(priv->tty_fd >= 0 && tcflush(priv->tty_fd, TCOFLUSH) < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "tcflush() FAILED: the output of the write canceled may still go",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
        }
    }
    dl_flush(&priv->dl_tx, (fnfree)gbuffer_decref);
}

/***************************************************************************
 *  Start the write in flight (new, or the rest of a short one). One that
 *  does not start will never complete, and its bytes are not sent: the line
 *  cannot go on with a hole in it, so it is dropped (-1: the gobj may be
 *  gone, nothing of it is touched).
 ***************************************************************************/
PRIVATE int start_the_write(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(yev_start_event(priv->yev_client_tx) < 0) {
        gbuffer_t *gbuf = yev_get_gbuf(priv->yev_client_tx);
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot start a write: the line is dropped",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            "len",          "%d", gbuf? (int)gbuffer_leftbytes(gbuf): 0,
            NULL
        );
        EXEC_AND_RESET(yev_destroy_event, priv->yev_client_tx)
        set_disconnected(gobj, "Cannot start a write");
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Start the next write queued, when none is in flight: the writes go one
 *  at a time, so the rest of a short write goes before the next.
 *  -1: the line was dropped, and the gobj may be gone.
 ***************************************************************************/
PRIVATE int write_next(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->yev_client_tx) {
        return 0;   // its completion starts the next
    }
    gbuffer_t *gbuf = dl_first(&priv->dl_tx);
    if(!gbuf) {
        return 0;
    }
    dl_delete(&priv->dl_tx, gbuf, 0);   // its reference goes to the event

    priv->yev_client_tx = yev_create_write_event(
        yuno_event_loop(),
        yev_callback,
        gobj,
        priv->tty_fd,
        gbuf
    );
    if(!priv->yev_client_tx) {
        // Error already logged
        gbuffer_decref(gbuf);   // the reference given to the event not created
        set_disconnected(gobj, "Cannot create a write");
        return -1;
    }
    return start_the_write(gobj);
}

/***************************************************************************
 *  Cut the writes and stop the read; if the read is still in flight, wait
 *  for its cancel in ST_WAIT_STOPPED (its callback comes back here), else
 *  end the stop: ST_STOPPED, the tty closed, EV_DISCONNECTED if it was
 *  connected, and EV_STOPPED, the last thing done with the gobj (its host
 *  may destroy it).
 *  IMPORTANT this is the only place to set ST_WAIT_STOPPED and ST_STOPPED
 ***************************************************************************/
PRIVATE void try_to_stop_yevents(hgobj gobj)  // IDEMPOTENT
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    BOOL to_wait_stopped = FALSE;

    if(gobj_current_state(gobj)==ST_STOPPED) {
        return;
    }

    cut_the_writes(gobj);

    if(priv->yev_client_rx) {
        if(!yev_event_is_stopped(priv->yev_client_rx)) {
            yev_stop_event(priv->yev_client_rx);
            if(!yev_event_is_stopped(priv->yev_client_rx)) {
                to_wait_stopped = TRUE;
            }
        }
    }

    if(to_wait_stopped) {
        gobj_change_state(gobj, ST_WAIT_STOPPED);
        return;
    }

    gobj_change_state(gobj, ST_STOPPED);
    if(set_disconnected(gobj, "Local stop") < 0) {
        return;     // its host destroyed it on EV_DISCONNECTED
    }

    /*
     *  The gobj is in stop. Its host may destroy it here: nothing of it is
     *  touched after this
     */
    gobj_publish_event(gobj, EV_STOPPED, 0);
}

/***************************************************************************
 *  Nothing of the gobj is touched after a publication that may have stopped
 *  it: its host may have destroyed it on EV_STOPPED. And the answer is
 *  always 0: a -1 ends the event loop, and the yuno with it.
 *  An event freed with its line (set_disconnected(), cut_the_writes()) is
 *  never called back.
 ***************************************************************************/
PRIVATE int yev_callback(yev_event_h yev_event)
{
    hgobj gobj = yev_get_gobj(yev_event);
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint32_t trace_level = gobj_trace_level(gobj);
    if(trace_level & TRACE_URING) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_get_flag(yev_event));
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev callback",
            "msg2",         "%s", "💥 yev callback",
            "event type",   "%s", yev_event_type_name(yev_event),
            "result",       "%d", yev_get_result(yev_event),
            "sres",         "%s", (yev_get_result(yev_event)<0)? strerror(-yev_get_result(yev_event)):"",
            "flag",         "%j", jn_flags,
            "p",            "%p", yev_event,
            "fd",           "%d", yev_get_fd(yev_event),
            NULL
        );
        json_decref(jn_flags);
    }
    switch(yev_get_type(yev_event)) {
        case YEV_READ_TYPE:
            {
                if(yev_get_result(yev_event) == -EINTR &&
                        gobj_in_this_state(gobj, ST_CONNECTED)) {
                    /*
                     *  Nothing read: the read of a tty runs blocking in a
                     *  worker of io_uring, and a notification of the ring
                     *  interrupts it. It is read again: up to 7.29.3 the
                     *  line was dropped.
                     */
                    gbuffer_clear(yev_get_gbuf(yev_event));
                    if(yev_start_event(yev_event) < 0) {
                        // Error already logged
                        set_disconnected(gobj, "Cannot read again");
                    }
                    break;
                }
                if(yev_get_result(yev_event) < 0) {
                    /*
                     *  Disconnected
                     */
                    if(trace_level & TRACE_URING) {
                        if(yev_get_result(yev_event) != -ECANCELED) {
                            gobj_log_info(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                                "msg",          "%s", "read FAILED",
                                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                                "errno",        "%d", -yev_get_result(yev_event),
                                "strerror",     "%s", strerror(-yev_get_result(yev_event)),
                                "p",            "%p", yev_event,
                                NULL
                            );
                        }
                    }
                    if(gobj_in_this_state(gobj, ST_WAIT_STOPPED)) {
                        try_to_stop_yevents(gobj);  // the end of a stop that waited for it
                    } else {
                        set_disconnected(gobj, strerror(-yev_get_result(yev_event)));
                    }

                } else {
                    if(trace_level & TRACE_TRAFFIC) {
                        gobj_trace_dump_gbuf(gobj, yev_get_gbuf(yev_event), "%s: %s%s",
                            gobj_short_name(gobj),
                            " ⏪ ",
                            gobj_read_str_attr(gobj, "url")
                        );
                    }
                    GBUFFER_INCREF(yev_get_gbuf(yev_event))
                    json_t *kw = json_pack("{s:I}",
                        "gbuffer", (json_int_t)(uintptr_t)yev_get_gbuf(yev_event)
                    );

                    gobj_publish_event(gobj, EV_RX_DATA, kw);

                    /*
                     *  Clear buffer, re-arm read. A subscriber may have
                     *  stopped or dropped the line (this event, idle, was
                     *  stopped with it), or its host destroyed the gobj:
                     *  only the event is looked at. A subscriber that
                     *  answered an error does not stop the reading.
                     */
                    if(yev_event_is_idle(yev_event)) {
                        gbuffer_clear(yev_get_gbuf(yev_event));
                        yev_start_event(yev_event);
                    }
                }
            }
            break;

        case YEV_WRITE_TYPE:
            {
                int result = yev_get_result(yev_event);
                if(yev_event != priv->yev_client_tx) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "The completion of a write that is not the one in flight",
                        "url",          "%s", gobj_read_str_attr(gobj, "url"),
                        "result",       "%d", result,
                        "p",            "%p", yev_event,
                        NULL
                    );
                    yev_destroy_event(yev_event);
                    break;
                }

                if(result == -EINTR) {
                    /*
                     *  Nothing written: the write of a tty runs blocking in
                     *  a worker of io_uring, and a notification of the ring
                     *  interrupts it. It goes again: up to 7.29.3 the line
                     *  was dropped.
                     */
                    start_the_write(gobj);  // on failure: logged, the line dropped
                    break;
                }
                if(result < 0) {
                    /*
                     *  Disconnected
                     */
                    if(trace_level & TRACE_URING) {
                        gobj_log_info(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                            "msg",          "%s", "write FAILED",
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
                            "errno",        "%d", -result,
                            "strerror",     "%s", strerror(-result),
                            NULL
                        );
                    }
                    EXEC_AND_RESET(yev_destroy_event, priv->yev_client_tx)
                    set_disconnected(gobj, strerror(-result));
                    break;
                }

                gbuffer_t *gbuf = yev_get_gbuf(yev_event);
                if(gbuf && gbuffer_leftbytes(gbuf) > 0) {
                    /*
                     *  A short write: the line took part of it. The rest
                     *  goes now, before anything queued. Up to 7.29.3 it was
                     *  lost, and EV_TX_READY published.
                     */
                    start_the_write(gobj);  // on failure: logged, the line dropped
                    break;
                }

                EXEC_AND_RESET(yev_destroy_event, priv->yev_client_tx)
                if(dl_size(&priv->dl_tx) > 0) {
                    write_next(gobj);       // on failure: logged, the line dropped
                    break;
                }

                json_t *kw_tx_ready = json_object();
                gobj_publish_event(gobj, EV_TX_READY, kw_tx_ready);
            }
            break;

        default:
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "event type NOT IMPLEMENTED",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
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
 *  Timeout to start connection
 ***************************************************************************/
PRIVATE int ac_timeout_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_send_event(gobj, EV_CONNECT, 0, gobj);

    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_connect(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(!gbuf) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "gbuffer NULL",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    /*
     *  Transmit: queued behind the write in flight, if there is one. A full
     *  queue drops its oldest write, as C_TCP does
     */
    if(priv->max_tx_queue > 0 && (json_int_t)dl_size(&priv->dl_tx) >= priv->max_tx_queue) {
        if((priv->tx_queue_dropped % priv->max_tx_queue) == 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "uart tx queue full, the oldest write dropped",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                "max_tx_queue", "%d", (int)priv->max_tx_queue,
                "dropped",      "%d", (int)priv->tx_queue_dropped,
                NULL
            );
        }
        priv->tx_queue_dropped++;
        gbuffer_t *gbuf_oldest = dl_first(&priv->dl_tx);
        dl_delete(&priv->dl_tx, gbuf_oldest, 0);
        gbuffer_decref(gbuf_oldest);
    }
    dl_add(&priv->dl_tx, gbuffer_incref(gbuf));
    write_next(gobj);   // on failure: logged, the line dropped

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Drop the line: the tty closed (what is in flight cut, as in a stop),
 *  EV_DISCONNECTED, and opened again after timeout_between_connections.
 *  The Modbus master sends it down when it cannot frame what it reads.
 *  Up to 7.29.3 C_UART had no EV_DROP: an error was logged and nothing
 *  was dropped.
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    set_disconnected(gobj, "Local dropping");

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
GOBJ_DEFINE_GCLASS(C_UART);

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
            "msgset",       "%s", MSGSET_INTERNAL,
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
        {EV_DROP,               ac_drop,                    0},
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
        {EV_TX_READY,       EVF_OUTPUT_EVENT},
        {EV_CONNECTED,      EVF_OUTPUT_EVENT},
        {EV_DISCONNECTED,   EVF_OUTPUT_EVENT},
        {EV_STOPPED,        EVF_OUTPUT_EVENT},

        {EV_TX_DATA,        0},
        {EV_DROP,           0},
        {EV_CONNECT,        0},
        {EV_TIMEOUT,        0},
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
PUBLIC int register_c_uart(void)
{
    return create_gclass(C_UART);
}
