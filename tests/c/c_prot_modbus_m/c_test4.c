/****************************************************************************
 *          C_TEST4.C
 *
 *          A C_PROT_MODBUS_M master over its own C_UART, on a pty:
 *          the stop contract of C_UART.
 *
 *          The test opens a pty: the master is given the path of its slave
 *          side as its url, so it creates a C_UART on it ("RTU"), and the
 *          test plays the Modbus slave on the master side of the pty,
 *          answering RTU frames by hand, CRC included. The test keeps a fd
 *          of its own on the slave side, so the pty never hangs up, and
 *          counts the fds of this process open on that path: one is its
 *          own, any other is the C_UART's.
 *
 *          - The master polls one block. On the message of its first cycle,
 *            published inside the read of C_UART (its EV_RX_DATA), the test
 *            stops the master, there. The C_UART must close its tty and end
 *            stopped (ST_STOPPED) at once: its read was not in flight. Up to
 *            the fix its stop stopped the read and nothing else: the tty
 *            stayed open, and the read callback, finding the gobj stopped,
 *            answered -1 to the event loop, which ENDS the loop, and with it
 *            the yuno.
 *          - Started again, the master must connect (one fd on the tty, not
 *            two: up to the fix the next open overwrote the old fd, and
 *            EV_CONNECTED came to a master still in ST_CONNECTED, "Event
 *            NOT DEFINED in state") and poll again.
 *          - Then the master is stopped from a timeout, with the read of
 *            C_UART in flight: the stop ends at once, in ST_STOPPED with the
 *            tty closed. Up to 7.29.3 it waited in ST_WAIT_STOPPED, stayed
 *            there for ever, the next start logged "Initial wrong task
 *            state" and never connected; and the cancel's callback answered
 *            -1, ending the yuno.
 *          - Started again, it polls a third time.
 *          - Then it is stopped and started in the same turn, with the read
 *            in flight, as a host does to change the url of the line: it
 *            must poll a fourth time, and the test ends. In the fixes after
 *            7.29.3 the C_UART refused that start (it was waiting in
 *            ST_WAIT_STOPPED), stayed stopped, marked as running, and the
 *            master never connected again.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <termios.h>

#include <c_prot_modbus_m.h>
#include "c_test4.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TIMEOUT_POLLING     1000    // ms between two cycles of the master
#define TIMEOUT_RESPONSE    2       // s
#define POLL_MAX_MS         6000    // a cycle not published by then: the master does not poll
#define RESTART_MS          1500    // a stopped master is started again after it
#define STOP_AFTER_MS       500     // the stop with the read in flight, after a cycle
#define END_AFTER_MS        500
#define BLOCK_ADDRESS       10
#define ANSWER_VALUE        0x0A0B
#define RX_SIZE             256

typedef enum {
    PHASE_FIRST_POLL = 0,       // waiting the first cycle: the stop inside the read
    PHASE_RESTART,              // stopped: started again at the timeout
    PHASE_SECOND_POLL,          // waiting a cycle of the restarted master
    PHASE_STOP_OUTSIDE,         // the stop at the timeout, with the read in flight
    PHASE_RESTART2,             // stopped: checked and started again at the timeout
    PHASE_THIRD_POLL,           // waiting a cycle of the restarted master
    PHASE_STOP_START,           // stopped and started in one turn, with the read in flight
    PHASE_FOURTH_POLL,          // waiting a cycle: the end
    PHASE_END,
} phase_t;

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int pty_callback(yev_event_h yev_event);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE char slaves_config[] = "\
[                                                                   \n\
    {                                                               \n\
        'id': 1,                                                    \n\
        'mapping': [                                                \n\
            {'type': 'holding_register', 'address': 10, 'size': 1}  \n\
        ],                                                          \n\
        'conversion': [                                             \n\
            {'id': 'v', 'type': 'holding_register', 'address': 10, 'format': 'uint16'} \n\
        ]                                                           \n\
    }                                                               \n\
]                                                                   \n\
";

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_INTEGER,     "timeout",          SDF_RD,             "1000",     "Timeout"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES  = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    json_int_t timeout;
    hgobj timer;

    hgobj gobj_master;

    int pty_master_fd;          // the slave of Modbus plays here
    int pty_slave_fd;           // the test's own fd on the tty: the pty never hangs up
    char pty_path[PATH_MAX];    // the url of the master
    yev_event_h yev_pty_rx;

    uint8_t rx[RX_SIZE];        // what the slave received and did not answer yet
    size_t rx_len;

    phase_t phase;
    int cycles;
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

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    priv->pty_master_fd = -1;
    priv->pty_slave_fd = -1;

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,               gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gobj_master && gobj_is_running(priv->gobj_master)) {
        gobj_stop(priv->gobj_master);
    }
    if(priv->yev_pty_rx) {
        yev_stop_event(priv->yev_pty_rx);
    }
    gobj_stop(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, priv->timeout); // then the pty and the master

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    EXEC_AND_RESET(yev_destroy_event, priv->yev_pty_rx)
    if(priv->pty_slave_fd >= 0) {
        close(priv->pty_slave_fd);
        priv->pty_slave_fd = -1;
    }
    if(priv->pty_master_fd >= 0) {
        close(priv->pty_master_fd);
        priv->pty_master_fd = -1;
    }
}




                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  The CRC of Modbus RTU (CRC-16, polynomial 0xA001 reflected, from 0xFFFF),
 *  computed bit by bit: not the master's tables
 ***************************************************************************/
PRIVATE uint16_t rtu_crc(const uint8_t *bf, size_t len)
{
    uint16_t crc = 0xFFFF;
    for(size_t i=0; i<len; i++) {
        crc ^= bf[i];
        for(int bit=0; bit<8; bit++) {
            if(crc & 1) {
                crc = (uint16_t)((crc >> 1) ^ 0xA001);
            } else {
                crc = (uint16_t)(crc >> 1);
            }
        }
    }
    return crc;
}

/***************************************************************************
 *  How many fds of this process are open on the tty of the master: the
 *  test's own, and the C_UART's when it is connected
 ***************************************************************************/
PRIVATE int fds_on_tty(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    DIR *dir = opendir("/proc/self/fd");
    if(!dir) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot open /proc/self/fd",
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    int count = 0;
    struct dirent *de;
    while((de = readdir(dir))) {
        if(de->d_name[0] == '.') {
            continue;
        }
        char link[PATH_MAX];
        if(!build_path(link, sizeof(link), "/proc/self/fd", de->d_name, NULL)) {
            // Error already logged
            continue;
        }
        char target[PATH_MAX];
        ssize_t n = readlink(link, target, sizeof(target)-1);
        if(n < 0) {
            continue;   // the fd of the listing itself, gone when it is read
        }
        target[n] = 0;
        if(strcmp(target, priv->pty_path)==0) {
            count++;
        }
    }
    closedir(dir);
    return count;
}

/***************************************************************************
 *  Check the fds on the tty and the state of the C_UART of the master
 ***************************************************************************/
PRIVATE void check_uart(hgobj gobj, int fds_expected, gobj_state_t state_expected, const char *when)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int fds = fds_on_tty(gobj);
    if(fds != fds_expected) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The fds open on the tty are not the expected",
            "when",         "%s", when,
            "fds",          "%d", fds,
            "expected",     "%d", fds_expected,
            NULL
        );
    }
    if(state_expected) {
        hgobj uart = gobj_bottom_gobj(priv->gobj_master);
        if(!uart || gobj_current_state(uart) != state_expected) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The C_UART is not in the expected state",
                "when",         "%s", when,
                "state",        "%s", uart? gobj_current_state(uart) : "",
                "expected",     "%s", state_expected,
                NULL
            );
        }
    }
}

/***************************************************************************
 *  Open the pty: its slave side is the url of the master
 ***************************************************************************/
PRIVATE int open_pty(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->pty_master_fd = posix_openpt(O_RDWR|O_NOCTTY|O_CLOEXEC);
    if(priv->pty_master_fd < 0 ||
            grantpt(priv->pty_master_fd) < 0 ||
            unlockpt(priv->pty_master_fd) < 0 ||
            ptsname_r(priv->pty_master_fd, priv->pty_path, sizeof(priv->pty_path)) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot open a pty",
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    set_nonblocking(priv->pty_master_fd);

    priv->pty_slave_fd = open(priv->pty_path, O_RDWR|O_NOCTTY|O_CLOEXEC);
    if(priv->pty_slave_fd < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot open the slave of the pty",
            "path",         "%s", priv->pty_path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    struct termios tios;
    if(tcgetattr(priv->pty_slave_fd, &tios) == 0) {
        cfmakeraw(&tios);   // no echo of the answers before the C_UART sets its own
        tcsetattr(priv->pty_slave_fd, TCSANOW, &tios);
    }

    priv->yev_pty_rx = yev_create_read_event(
        yuno_event_loop(),
        pty_callback,
        gobj,
        priv->pty_master_fd,
        gbuffer_create(RX_SIZE, RX_SIZE)
    );
    yev_start_event(priv->yev_pty_rx);
    return 0;
}

/***************************************************************************
 *  One whole request at the slave, its CRC already checked: the read of
 *  the block is answered with ANSWER_VALUE
 ***************************************************************************/
PRIVATE void slave_request(hgobj gobj, const uint8_t *req)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int unit = req[0];
    int function = req[1];
    int address = (req[2] << 8) | req[3];
    int quantity = (req[4] << 8) | req[5];

    if(priv->phase == PHASE_RESTART) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A request reached the slave while the master was stopped",
            NULL
        );
        return;
    }
    if(unit != 1 || function != 3 || address != BLOCK_ADDRESS || quantity != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Unexpected request at the slave",
            "unit",         "%d", unit,
            "modbus_function", "%d", function,
            "address",      "%d", address,
            "quantity",     "%d", quantity,
            NULL
        );
        return;
    }

    uint8_t rsp[5 + 2] = {req[0], req[1], 2, (uint8_t)(ANSWER_VALUE >> 8), (uint8_t)(ANSWER_VALUE & 0xFF)};
    uint16_t crc = rtu_crc(rsp, 5);
    rsp[5] = (uint8_t)(crc & 0xFF);
    rsp[6] = (uint8_t)(crc >> 8);

    ssize_t n = write(priv->pty_master_fd, rsp, sizeof(rsp));
    if(n != (ssize_t)sizeof(rsp)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "The slave could not write its answer",
            "written",      "%d", (int)n,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
    }
}

/***************************************************************************
 *  Bytes at the slave, from the master side of the pty: answer every whole
 *  request (8 bytes: only reads of function 03 are expected)
 ***************************************************************************/
PRIVATE int pty_callback(yev_event_h yev_event)
{
    hgobj gobj = yev_get_gobj(yev_event);
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int result = yev_get_result(yev_event);
    if(result < 0) {
        /*
         *  The stop of the test cancels the read: ECANCELED, or ENOENT when
         *  the cancel came too late to find it
         */
        if(gobj_is_running(gobj) && result != -ECANCELED) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "The read of the pty failed",
                "errno",        "%d", -result,
                "strerror",     "%s", strerror(-result),
                NULL
            );
        }
        return 0;
    }

    gbuffer_t *gbuf = yev_get_gbuf(yev_event);
    size_t len = gbuffer_leftbytes(gbuf);
    if(priv->rx_len + len > sizeof(priv->rx)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The slave got more than it can hold",
            "len",          "%lu", (unsigned long)len,
            NULL
        );
        priv->rx_len = 0;
    } else {
        memcpy(priv->rx + priv->rx_len, gbuffer_cur_rd_pointer(gbuf), len);
        priv->rx_len += len;
    }

    while(priv->rx_len >= 8) {
        uint16_t crc = rtu_crc(priv->rx, 6);
        if(priv->rx[6] != (crc & 0xFF) || priv->rx[7] != (crc >> 8)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A request with a bad CRC at the slave",
                "modbus_function", "%d", priv->rx[1],
                NULL
            );
        } else {
            slave_request(gobj, priv->rx);
        }
        memmove(priv->rx, priv->rx + 8, priv->rx_len - 8);
        priv->rx_len -= 8;
    }

    gbuffer_clear(gbuf);
    yev_start_event(yev_event);
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The steps that are not answers of the master, and the watchdog of the
 *  ones that are
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->gobj_master) {
        if(open_pty(gobj) < 0) {
            // Error already logged
            set_yuno_must_die();
            JSON_DECREF(kw)
            return 0;
        }
        helper_quote2doublequote(slaves_config);
        json_t *jn_slaves = string2json(slaves_config, TRUE);
        priv->gobj_master = gobj_create(
            "modbus",
            C_PROT_MODBUS_M,
            json_pack("{s:s, s:s, s:i, s:i, s:o}",
                "modbus_protocol", "RTU",
                "url", priv->pty_path,
                "timeout_polling", TIMEOUT_POLLING,
                "timeout_response", TIMEOUT_RESPONSE,
                "slaves", jn_slaves
            ),
            gobj
        );
        gobj_start(priv->gobj_master);
        priv->phase = PHASE_FIRST_POLL;
        set_timeout(priv->timer, POLL_MAX_MS);
        JSON_DECREF(kw)
        return 0;
    }

    switch(priv->phase) {
        case PHASE_RESTART:
            check_uart(gobj, 1, ST_STOPPED, "before the first restart");
            gobj_start(priv->gobj_master);
            priv->phase = PHASE_SECOND_POLL;
            set_timeout(priv->timer, POLL_MAX_MS);
            break;

        case PHASE_STOP_OUTSIDE:
            gobj_stop(priv->gobj_master);   // the read of the C_UART in flight: the stop ends at once
            check_uart(gobj, 1, ST_STOPPED, "stopped with the read in flight");
            priv->phase = PHASE_RESTART2;
            set_timeout(priv->timer, RESTART_MS);
            break;

        case PHASE_RESTART2:
            check_uart(gobj, 1, ST_STOPPED, "after a stop with the read in flight");
            gobj_start(priv->gobj_master);
            priv->phase = PHASE_THIRD_POLL;
            set_timeout(priv->timer, POLL_MAX_MS);
            break;

        case PHASE_STOP_START:
            gobj_stop(priv->gobj_master);   // the read of the C_UART in flight
            check_uart(gobj, 1, ST_STOPPED, "stopped with the read in flight, to start it again");
            gobj_start(priv->gobj_master);  // in the same turn
            check_uart(gobj, 1, ST_DISCONNECTED, "started in the turn of its stop");
            priv->phase = PHASE_FOURTH_POLL;
            set_timeout(priv->timer, POLL_MAX_MS);
            break;

        case PHASE_END:
            set_yuno_must_die();
            break;

        case PHASE_FIRST_POLL:
        case PHASE_SECOND_POLL:
        case PHASE_THIRD_POLL:
        case PHASE_FOURTH_POLL:
        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The master did not poll",
                "phase",        "%d", (int)priv->phase,
                "cycles",       "%d", priv->cycles,
                NULL
            );
            set_yuno_must_die();
            break;
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A cycle of the master, published inside the read of its C_UART
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->cycles++;
    json_int_t v = kw_get_int(gobj, kw, "v", -1, KW_REQUIRED);
    if(v != ANSWER_VALUE) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The cycle did not publish the value answered",
            "published",    "%j", kw,
            NULL
        );
    }

    switch(priv->phase) {
        case PHASE_FIRST_POLL:
            /*
             *  On purpose: the master, and its C_UART, stopped inside the
             *  read of the C_UART. Its read is not in flight: the stop ends
             *  here, with the tty closed.
             */
            gobj_stop(priv->gobj_master);
            check_uart(gobj, 1, ST_STOPPED, "stopped inside the read of the C_UART");
            priv->phase = PHASE_RESTART;
            set_timeout(priv->timer, RESTART_MS);
            break;

        case PHASE_SECOND_POLL:
            priv->phase = PHASE_STOP_OUTSIDE;
            set_timeout(priv->timer, STOP_AFTER_MS);
            break;

        case PHASE_THIRD_POLL:
            priv->phase = PHASE_STOP_START;
            set_timeout(priv->timer, STOP_AFTER_MS);
            break;

        case PHASE_FOURTH_POLL:
            priv->phase = PHASE_END;
            set_timeout(priv->timer, END_AFTER_MS);
            break;

        default:
            break;  // a cycle while the next step waits for its timeout
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The master connected: its C_UART holds one fd on the tty, not two
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    check_uart(gobj, 2, ST_CONNECTED, "connected");

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A close is expected only on a stop
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_is_running(priv->gobj_master) && !gobj_is_shutdowning()) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The connection closed with the master running",
            "phase",        "%d", (int)priv->phase,
            NULL
        );
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)

    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }

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
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST4);

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
    ev_action_t st_idle[] = {
        {EV_ON_OPEN,                ac_on_open,                 0},
        {EV_ON_MESSAGE,             ac_on_message,              0},
        {EV_ON_CLOSE,               ac_on_close,                0},
        {EV_TIMEOUT,                ac_timeout,                 0},
        {EV_STOPPED,                ac_stopped,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_OPEN,                0},
        {EV_ON_MESSAGE,             0},
        {EV_ON_CLOSE,               0},
        {EV_TIMEOUT,                0},
        {EV_STOPPED,                0},
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
        s_user_trace_level,  // s_user_trace_level,
        0   // gcflag_t
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
PUBLIC int register_c_test4(void)
{
    return create_gclass(C_TEST4);
}
