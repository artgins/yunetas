/***********************************************************************
 *          C_SERIAL.C
 *          Serial GClass.
 *
 *          Manage Serial Ports uv-mixin
 *
 *          Partial source code from https://github.com/vsergeev/c-periphery
 *
 *          Copyright (c) 2021 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include <yev_loop.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include "c_yuno.h"
#include "c_serial.h"

#include <termios.h>

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
PRIVATE int configure_tty(hgobj gobj);
PRIVATE int on_read_cb(hgobj gobj, gbuffer_t *gbuf);
PRIVATE int yev_callback(yev_event_h yev_event);
PRIVATE void try_to_stop_yevents(hgobj gobj);  // IDEMPOTENT
PRIVATE void try_more_writes(hgobj gobj);


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag--------default-description---------- */
SDATA (DTP_STRING,      "url",              SDF_RD,     "",     "Device (url by compatibility with C_TCP) to open: i.e. /dev/ttyS0"),
SDATA (DTP_STRING,      "cert_pem",         SDF_RD,     "",     "Not use, by compatibility with C_TCP"),
SDATA (DTP_INTEGER,     "baudrate",         SDF_RD,     "9600", "Baud rate"),
SDATA (DTP_INTEGER,     "bytesize",         SDF_RD,     "8",    "Byte size"),
SDATA (DTP_STRING,      "parity",           SDF_RD,     "none", "Parity"),
SDATA (DTP_INTEGER,     "stopbits",         SDF_RD,     "1",    "Stop bits"),
SDATA (DTP_BOOLEAN,     "xonxoff",          SDF_RD,     0,      "xonxoff"),
SDATA (DTP_BOOLEAN,     "rtscts",           SDF_RD,     0,      "rtscts"),
SDATA (DTP_INTEGER,     "txMsgs",           SDF_RSTATS, 0,      "Messages transmitted"),
SDATA (DTP_INTEGER,     "rxMsgs",           SDF_RSTATS, 0,      "Messages received"),
SDATA (DTP_INTEGER,     "max_tx_queue",     SDF_WR,     "32",   "Maximum messages in tx queue. Default is 0: no limit."),
SDATA (DTP_POINTER,     "user_data",        0,          0,      "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,          0,      "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,          0,      "subscriber of output-events. If it's null then subscriber is the parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_TRAFFIC           = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"traffic",             "Trace dump traffic"},
{0, 0},
};


/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
#define BFINPUT_SIZE (2*1024)

typedef struct _PRIVATE_DATA {
    int tty_fd;
    yev_event_h yev_reading;

    dl_list_t dl_tx;
    gbuffer_t *gbuf_txing;
    json_int_t max_tx_queue;
    int tx_in_progress;
    json_int_t txMsgs;
    json_int_t rxMsgs;

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
    priv->tty_fd = -1;

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(max_tx_queue,  gobj_read_integer_attr)

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBUFFER_DECREF(priv->gbuf_txing)
    dl_flush(&priv->dl_tx, (fnfree)gbuffer_decref);
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
     PRIVATE_DATA *priv = gobj_priv_data(gobj);

     IF_EQ_SET_PRIV(max_tx_queue,   gobj_read_integer_attr)
     END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *device = gobj_read_str_attr(gobj, "url");
    if(empty_string(device)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "what tty device?",
            NULL
        );
        return -1;
    }

    priv->tty_fd = open(device, O_RDWR|O_CLOEXEC|O_NOCTTY, 0);
    if(priv->tty_fd < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "Cannot open serial device",
            "device",       "%s", device,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    set_nonblocking(priv->tty_fd);
    set_cloexec(priv->tty_fd);

    configure_tty(gobj);

    /*-------------------------------*
     *      Setup reading event
     *-------------------------------*/
    json_int_t rx_buffer_size = 1024;
    if(!priv->yev_reading) {
        priv->yev_reading = yev_create_read_event(
            yuno_event_loop(),
            yev_callback,
            gobj,
            priv->tty_fd,
            gbuffer_create(rx_buffer_size, rx_buffer_size)
        );
    }

    if(priv->yev_reading) {
        yev_set_fd(priv->yev_reading, priv->tty_fd);

        if(!yev_get_gbuf(priv->yev_reading)) {
            yev_set_gbuffer(priv->yev_reading, gbuffer_create(rx_buffer_size, rx_buffer_size));
        } else {
            gbuffer_clear(yev_get_gbuf(priv->yev_reading));
        }

        yev_start_event(priv->yev_reading);
    }

    json_t *kw_on_open = json_pack("{s:s}",
        "device", gobj_read_str_attr(gobj, "url")
    );
    gobj_publish_event(gobj, EV_CONNECTED, kw_on_open);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    try_to_stop_yevents(gobj);

    return 0;
}




            /***************************
             *      Local Methods
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
PRIVATE int configure_tty(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    struct termios termios_settings;
    if(tcgetattr(priv->tty_fd, &termios_settings)<0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "tcgetattr() FAILED",
            "device",       "%s", gobj_read_str_attr(gobj, "url"),
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
    int baudrate_ = (int)gobj_read_integer_attr(gobj, "baudrate");
    int baudrate = _serial_baudrate_to_bits(baudrate_);
    if(baudrate == -1) {
        baudrate = B9600;
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "device",       "%s", gobj_read_str_attr(gobj, "url"),
            "fd",           "%d", priv->tty_fd,
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
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Parity UNKNOWN",
                "device",       "%s", gobj_read_str_attr(gobj, "url"),
                "fd",           "%d", priv->tty_fd,
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
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Bad bytesize",
                "device",       "%s", gobj_read_str_attr(gobj, "url"),
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
    int stopbits = (int)gobj_read_integer_attr(gobj, "stopbits");
    switch(stopbits) {
        case 1:
        case 2:
            break;
        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Bad stopbits",
                "device",       "%s", gobj_read_str_attr(gobj, "url"),
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
            "device",       "%s", gobj_read_str_attr(gobj, "url"),
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
PRIVATE int yev_callback(yev_event_h yev_event)
{
    hgobj gobj = yev_get_gobj(yev_event);
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!yev_event) {
        /*
         *  It's the timeout
         */
        return 0;
    }

    uint32_t trace_level = gobj_trace_level(gobj);
    int trace = trace_level & TRACE_URING;
    if(trace) {
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
                    int ret = 0;

                    ret = on_read_cb(gobj, yev_get_gbuf(yev_event));

                    /*
                     *  Clear buffer, re-arm read
                     *  Check ret is 0 because the EV_RX_DATA could provoke
                     *      stop or destroy of gobj
                     *      or order to disconnect (EV_DROP)
                     *  If try_to_stop_yevents() has been called (mt_stop, EV_DROP,...)
                     *      this event will be in stopped state.
                     *  If it's in idle then re-arm
                     */
                    if(ret == 0 && yev_event_is_idle(yev_event)) {
                        gbuffer_clear(yev_get_gbuf(yev_event));
                        yev_start_event(yev_event);
                    }

                } else {
                    if(trace) {
                        gobj_log_debug(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                            "msg",          "%s", "read FAILED",
                            "errno",        "%d", -yev_get_result(yev_event),
                            "strerror",     "%s", strerror(-yev_get_result(yev_event)),
                            "p",            "%p", yev_event,
                            "fd",           "%d", yev_get_fd(yev_event),
                            NULL
                        );
                    }
                    gobj_stop(gobj);
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
                        if(gbuffer_leftbytes(yev_get_gbuf(yev_event)) > 0) {
                            if(trace_level & TRACE_MACHINE) {
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

                        yev_destroy_event(yev_event);
                        try_more_writes(gobj);
                    } else {
                        yev_destroy_event(yev_event);
                        gobj_stop(gobj);
                    }

                } else {
                    /*
                     *  Disconnected
                     */
                    gobj_log_set_last_message("%s", strerror(-yev_get_result(yev_event)));

                    if(trace) {
                        gobj_log_debug(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                            "msg",          "%s", "write FAILED",
                            "errno",        "%d", -yev_get_result(yev_event),
                            "strerror",     "%s", strerror(-yev_get_result(yev_event)),
                            "p",            "%p", yev_event,
                            "fd",           "%d", yev_get_fd(yev_event),
                            NULL
                        );
                    }

                    yev_destroy_event(yev_event);
                    gobj_stop(gobj);
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

/***************************************************************************
 *  Stop all events, is someone is running go to WAIT_STOPPED else STOPPED
 *  IMPORTANT this is the only place to set ST_WAIT_STOPPED state
 ***************************************************************************/
PRIVATE void try_to_stop_yevents(hgobj gobj)  // IDEMPOTENT
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    BOOL to_wait_stopped = FALSE;

    uint32_t trace_level = gobj_trace_level(gobj);
    if(trace_level & TRACE_URING) {
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "try_to_stop_yevents",
            "msg2",         "%s", "🟥🟥 try_to_stop_yevents",
            NULL
        );
    }

    if(priv->tty_fd != -1) {
        close(priv->tty_fd);
        priv->tty_fd = -1;
    }

    if(priv->tx_in_progress > 0) {
        to_wait_stopped = TRUE;
    }

    if(priv->yev_reading) {
        if(!yev_event_is_stopped(priv->yev_reading)) {
            yev_stop_event(priv->yev_reading);
            if(!yev_event_is_stopped(priv->yev_reading)) {
                to_wait_stopped = TRUE;
            }
        }
    }

    if(to_wait_stopped) {
        gobj_change_state(gobj, ST_WAIT_STOPPED);
    } else {
        gobj_change_state(gobj, ST_STOPPED);
        json_t *kw_on_close = json_pack("{s:s}}",
            "device", gobj_read_str_attr(gobj, "url")
        );
        gobj_publish_event(gobj, EV_DISCONNECTED, kw_on_close);

        if(gobj_is_volatil(gobj)) {
            gobj_destroy(gobj);
        } else {
            gobj_publish_event(gobj, EV_STOPPED, 0);
        }
    }
}

/***************************************************************************
 *  on read callback
 ***************************************************************************/
PRIVATE int on_read_cb(hgobj gobj, gbuffer_t *gbuf)
{
    size_t nread = gbuffer_leftbytes(gbuf);
    char *base = gbuffer_cur_rd_pointer(gbuf);

    if(nread == 0) {
        // Yes, sometimes arrive with nread 0.
        return 0;
    }

    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        gobj_trace_dump(gobj,
            base,
            nread,
            "READ from SERIAL %s",
            gobj_short_name(gobj)
        );
    }

    gbuffer_t *gbuf_rx = gbuffer_create(nread, nread);

    if(!gbuf_rx) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "no memory for gbuf",
            "size",         "%d", nread,
            NULL);
        return -1;
    }
    gbuffer_append(gbuf, base, nread);

    json_t *kw = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf_rx
    );
    gobj_publish_event(gobj, EV_RX_DATA, kw);

    return 0;
}

/***************************************************************************
 *  Write data to pseudo terminal
 ***************************************************************************/
PRIVATE int write_data_to_serial(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = priv->gbuf_txing;

    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        size_t ln = gbuffer_chunk(gbuf);
        char *bf = gbuffer_cur_rd_pointer(gbuf);
        gobj_trace_dump(gobj,
            bf,
            ln,
            "WRITE to SERIAL %s",
            gobj_short_name(gobj)
        );
    }

    priv->txMsgs++;

    /*
     *  Transmit
     */
    yev_event_h yev_write_event = yev_create_write_event(
        yuno_event_loop(),
        yev_callback,
        gobj,
        priv->tty_fd,
        gbuffer_incref(gbuf)
    );

    priv->tx_in_progress++;
    yev_start_event(yev_write_event);

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
        //
    } else {
        priv->gbuf_txing = gbuf_txing;
        dl_delete(&priv->dl_tx, gbuf_txing, 0);
        write_data_to_serial(gobj);
    }
}

/***************************************************************************
 *  Enqueue data
 ***************************************************************************/
PRIVATE int enqueue_write(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    static int counter = 0;
    size_t size = dl_size(&priv->dl_tx);

    if(priv->max_tx_queue > 0 && size >= priv->max_tx_queue) {
        if((counter % priv->max_tx_queue)==0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "throw away serial tx message",
                "counter",      "%d", (int)counter,
                NULL
            );
        }
        counter++;
        gbuffer_t *gbuf_first = dl_first(&priv->dl_tx);
        dl_delete(&priv->dl_tx, gbuf_first, 0);
        gbuffer_decref(gbuf_first);
    }

    dl_add(&priv->dl_tx, gbuf);

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

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, TRUE);
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
        write_data_to_serial(gobj);
    } else {
        enqueue_write(gobj, gbuffer_incref(gbuf));
    }

    KW_DECREF(kw);
    return 0;
}

/***********************************************************************
 *                          FSM
 ***********************************************************************/

/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_writing = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_SERIAL);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_idle[] = {
        {EV_TX_DATA,          ac_tx_data,             0},
        {0,0,0}
    };

    ev_action_t st_wait_stopped[] = {
        {0,0,0}
    };

    ev_action_t st_stopped[] = {
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {ST_WAIT_STOPPED,       st_wait_stopped},
        {ST_STOPPED,            st_stopped},
        {0,0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_TX_DATA,            0},
        {EV_RX_DATA,            EVF_OUTPUT_EVENT},
        {EV_CONNECTED,          EVF_OUTPUT_EVENT},
        {EV_DISCONNECTED,       EVF_OUTPUT_EVENT},
        {EV_STOPPED,            EVF_OUTPUT_EVENT},
        {0,0}
    };

    /*----------------------------------------*
     *          Register GClass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,                  /* lmt (no hay métodos locales) */
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,                  /* authz_table */
        0,                  /* command_table */
        s_user_trace_level,
        gcflag_manual_start /* gcflag */
    );
    if(!__gclass__) {
        return -1;
    }
    return 0;
}

/***************************************************************************
 *          Public access
 ***************************************************************************/
PUBLIC int register_c_serial(void)
{
    return create_gclass(C_SERIAL);
}
