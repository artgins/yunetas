/****************************************************************************
 *          C_TEST5.C
 *
 *          A C_UART on a pty, hosted by the test (its parent: it hears
 *          EV_CONNECTED, EV_DISCONNECTED, EV_RX_DATA, EV_TX_READY and
 *          EV_STOPPED). The test reads the master side of the pty, or stops
 *          reading it to fill the line, and counts the fds of this process
 *          open on the tty: one is its own (the pty never hangs up), any
 *          other is a C_UART's.
 *
 *          - A write of 64 KB, more than the line takes at once: the write
 *            is short, and the C_UART must send the rest, in order, and
 *            publish EV_TX_READY once, when all of it went. Up to 7.29.3 the
 *            rest was lost: about 12 KB went, then EV_TX_READY.
 *          - EV_DROP: the C_UART closes its tty, publishes EV_DISCONNECTED
 *            and opens it again after timeout_between_connections. Up to
 *            7.29.3 C_UART had no EV_DROP ("Event NOT DEFINED"), and the
 *            Modbus master sends it down when it cannot frame what it reads.
 *          - The line full (the test does not read it), a write held, and a
 *            stop: the stop must end, with the tty closed and EV_STOPPED,
 *            without waiting for the other side to read. Up to 7.29.3 it
 *            waited in ST_WAIT_STOPPED until the other side read.
 *          - The same, with one write more than its max_tx_queue (1): the
 *            oldest queued is dropped, with an error. And a destroy right
 *            after the stop; then the test reads the line again. Up to 7.29.3 the held write completed on
 *            the destroyed gobj: a use after free.
 *          - A stop with the read in flight (ST_WAIT_STOPPED) and a destroy
 *            there: the tty must be closed. Up to 7.29.3 its fd was lost.
 *          - A stop with the read in flight and a start in ST_WAIT_STOPPED:
 *            the start is refused (-1, "Initial wrong task state"), and the
 *            stop ends as it would. Up to 7.29.3 the start armed its timer
 *            and the stop was lost.
 *          - A host that destroys the C_UART in the EV_DISCONNECTED of a
 *            stop (wrong: the kernel says so, "Destroying a gobj while its
 *            tree is publishing"): the C_UART touches nothing of itself
 *            after it. Up to 7.29.3 it went on to publish EV_STOPPED from
 *            the freed gobj.
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

#include "c_test5.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TX_SIZE             65536   // more than a pty takes at once
#define HELD_TAIL           8       // a write behind the big one
#define RX_SIZE             4096
#define PHASE_MAX_MS        5000    // a phase not ended by then: failed
#define SETTLE_MS           300     // for a held write to be held, or a stop to end
#define RECONNECT_MS        500     // timeout_between_connections of the C_UARTs

typedef enum {
    PHASE_CONNECT1 = 0,         // the first open
    PHASE_SHORT_WRITE,          // 64 KB must arrive whole, and EV_TX_READY once
    PHASE_RECONNECT,            // dropped: it must open again
    PHASE_HELD_STOP,            // a write held: the stop at the timeout
    PHASE_HELD_STOPPED,         // the stop must have ended
    PHASE_RESTART,              // drained: started again at the timeout
    PHASE_CONNECT3,             // the third open
    PHASE_HELD_DESTROY,         // a write held: stop and destroy at the timeout
    PHASE_AFTER_DESTROY,        // the line read again: nothing may happen
    PHASE_CONNECT_WAIT_DESTROY, // a new C_UART: destroyed in ST_WAIT_STOPPED
    PHASE_WAIT_DESTROY,
    PHASE_CONNECT_WAIT_START,   // a new C_UART: started in ST_WAIT_STOPPED
    PHASE_WAIT_START,
    PHASE_WAIT_START_STOPPED,   // its stop must have ended
    PHASE_CONNECT_ALIVE,        // a new C_UART: destroyed in its EV_DISCONNECTED
    PHASE_ALIVE,
    PHASE_END,
} phase_t;

typedef enum {
    SLAVE_COUNT = 0,            // check what arrives against the pattern
    SLAVE_HOLD,                 // read no more: the line fills
    SLAVE_DISCARD,              // read and throw away
} slave_mode_t;

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

    hgobj gobj_uart;            // the C_UART under test
    int uarts;                  // C_UARTs created, to name them

    int pty_master_fd;          // the other side of the line: the test reads here
    int pty_slave_fd;           // the test's own fd on the tty: the pty never hangs up
    char pty_path[PATH_MAX];    // the url of the C_UARTs
    yev_event_h yev_pty_rx;
    BOOL pty_rx_armed;

    slave_mode_t slave_mode;
    size_t rx_count;            // bytes of the pattern received
    BOOL rx_bad;                // a byte out of the pattern, said once

    phase_t phase;
    int connections;
    int disconnections;
    int tx_ready;
    int stopped;
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

    if(priv->gobj_uart && gobj_is_running(priv->gobj_uart)) {
        gobj_stop(priv->gobj_uart);
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

    set_timeout(priv->timer, priv->timeout); // then the pty and the C_UART

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
 *  How many fds of this process are open on the tty: the test's own, and
 *  the C_UART's when it holds the tty
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
 *  Check the fds on the tty and the state of the C_UART
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
        if(!priv->gobj_uart || gobj_current_state(priv->gobj_uart) != state_expected) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The C_UART is not in the expected state",
                "when",         "%s", when,
                "state",        "%s", priv->gobj_uart? gobj_current_state(priv->gobj_uart) : "",
                "expected",     "%s", state_expected,
                NULL
            );
        }
    }
}

/***************************************************************************
 *  Check a counter of the events heard
 ***************************************************************************/
PRIVATE void check_count(hgobj gobj, const char *what, int count, int expected, const char *when)
{
    if(count != expected) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The events heard are not the expected",
            "event",        "%s", what,
            "when",         "%s", when,
            "count",        "%d", count,
            "expected",     "%d", expected,
            NULL
        );
    }
}

/***************************************************************************
 *  Open the pty: its slave side is the url of the C_UARTs
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
        cfmakeraw(&tios);   // no echo, no output processing before the C_UART sets its own
        tcsetattr(priv->pty_slave_fd, TCSANOW, &tios);
    }

    priv->yev_pty_rx = yev_create_read_event(
        yuno_event_loop(),
        pty_callback,
        gobj,
        priv->pty_master_fd,
        gbuffer_create(RX_SIZE, RX_SIZE)
    );
    return 0;
}

/***************************************************************************
 *  Read the master side of the pty in a mode
 ***************************************************************************/
PRIVATE void slave_read(hgobj gobj, slave_mode_t mode)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->slave_mode = mode;
    if(mode != SLAVE_HOLD && !priv->pty_rx_armed) {
        gbuffer_clear(yev_get_gbuf(priv->yev_pty_rx));
        if(yev_start_event(priv->yev_pty_rx) == 0) {
            priv->pty_rx_armed = TRUE;
        }
    }
}

/***************************************************************************
 *  Create and start a new C_UART on the tty
 ***************************************************************************/
PRIVATE void new_uart(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char name[NAME_MAX];
    snprintf(name, sizeof(name), "uart%d", ++priv->uarts);
    priv->gobj_uart = gobj_create(
        name,
        C_UART,
        json_pack("{s:s, s:i, s:i, s:i}",
            "url", priv->pty_path,
            "baudrate", 9600,
            "timeout_between_connections", RECONNECT_MS,
            "max_tx_queue", 1
        ),
        gobj
    );
    gobj_start(priv->gobj_uart);
}

/***************************************************************************
 *  Send a gbuffer of len bytes of the pattern from offset
 ***************************************************************************/
PRIVATE void send_pattern(hgobj gobj, size_t offset, size_t len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(len, len);
    for(size_t i=0; i<len; i++) {
        uint8_t c = (uint8_t)((offset + i) % 251);
        gbuffer_append(gbuf, &c, 1);
    }
    json_t *kw = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    gobj_send_event(priv->gobj_uart, EV_TX_DATA, kw, gobj);
}

/***************************************************************************
 *  Destroy the C_UART under test
 ***************************************************************************/
PRIVATE void destroy_uart(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj uart = priv->gobj_uart;
    priv->gobj_uart = 0;
    gobj_destroy(uart);
}

/***************************************************************************
 *  The 64 KB arrived whole, and EV_TX_READY once: drop the line
 ***************************************************************************/
PRIVATE void check_short_write_done(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase != PHASE_SHORT_WRITE || priv->rx_count < TX_SIZE || priv->tx_ready < 1) {
        return;
    }
    check_count(gobj, "bytes", (int)priv->rx_count, TX_SIZE, "a write of 64 KB");
    check_count(gobj, "EV_TX_READY", priv->tx_ready, 1, "a write of 64 KB");

    priv->phase = PHASE_RECONNECT;
    slave_read(gobj, SLAVE_DISCARD);
    gobj_send_event(priv->gobj_uart, EV_DROP, 0, gobj);
    check_count(gobj, "EV_DISCONNECTED", priv->disconnections, 1, "dropped");
    check_uart(gobj, 1, ST_DISCONNECTED, "dropped");
    set_timeout(priv->timer, PHASE_MAX_MS);
}

/***************************************************************************
 *  Bytes at the other side of the line
 ***************************************************************************/
PRIVATE int pty_callback(yev_event_h yev_event)
{
    hgobj gobj = yev_get_gobj(yev_event);
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->pty_rx_armed = FALSE;

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
    if(priv->slave_mode == SLAVE_COUNT) {
        size_t len = gbuffer_leftbytes(gbuf);
        const uint8_t *p = gbuffer_cur_rd_pointer(gbuf);
        for(size_t i=0; i<len; i++) {
            if(p[i] != (uint8_t)((priv->rx_count + i) % 251) && !priv->rx_bad) {
                priv->rx_bad = TRUE;
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "A byte out of order at the other side of the line",
                    "offset",       "%lu", (unsigned long)(priv->rx_count + i),
                    NULL
                );
            }
        }
        priv->rx_count += len;
    }

    if(priv->slave_mode != SLAVE_HOLD) {
        gbuffer_clear(gbuf);
        if(yev_start_event(yev_event) == 0) {
            priv->pty_rx_armed = TRUE;
        }
    }

    check_short_write_done(gobj);
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The steps, and the watchdog of the phases that wait for an event
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->pty_master_fd < 0) {
        if(open_pty(gobj) < 0) {
            // Error already logged
            set_yuno_must_die();
            JSON_DECREF(kw)
            return 0;
        }
        priv->phase = PHASE_CONNECT1;
        new_uart(gobj);
        set_timeout(priv->timer, PHASE_MAX_MS);
        JSON_DECREF(kw)
        return 0;
    }

    switch(priv->phase) {
        case PHASE_HELD_STOP:
            /*
             *  A write held by the line (no EV_TX_READY since the first
             *  phase), and the read in flight: the stop cuts the write, and
             *  waits only for the cancel of the read
             */
            check_count(gobj, "EV_TX_READY", priv->tx_ready, 1, "a write the line does not take");
            gobj_stop(priv->gobj_uart);
            priv->phase = PHASE_HELD_STOPPED;
            set_timeout(priv->timer, SETTLE_MS);
            break;

        case PHASE_HELD_STOPPED:
            check_uart(gobj, 1, ST_STOPPED, "stopped with a write the line does not take");
            check_count(gobj, "EV_STOPPED", priv->stopped, 1, "stopped with a write the line does not take");
            check_count(gobj, "EV_DISCONNECTED", priv->disconnections, 2, "stopped with a write the line does not take");
            slave_read(gobj, SLAVE_DISCARD);
            priv->phase = PHASE_RESTART;
            set_timeout(priv->timer, SETTLE_MS);
            break;

        case PHASE_RESTART:
            gobj_start(priv->gobj_uart);
            priv->phase = PHASE_CONNECT3;
            set_timeout(priv->timer, PHASE_MAX_MS);
            break;

        case PHASE_HELD_DESTROY:
            /*
             *  The same, and destroyed right after the stop: then the line
             *  is read again, and the write held must not come back to it
             */
            check_count(gobj, "EV_TX_READY", priv->tx_ready, 1, "a write the line does not take");
            gobj_stop(priv->gobj_uart);
            destroy_uart(gobj);
            check_uart(gobj, 1, 0, "destroyed after a stop with a write the line does not take");
            slave_read(gobj, SLAVE_DISCARD);
            priv->phase = PHASE_AFTER_DESTROY;
            set_timeout(priv->timer, SETTLE_MS);
            break;

        case PHASE_AFTER_DESTROY:
            priv->phase = PHASE_CONNECT_WAIT_DESTROY;
            new_uart(gobj);
            set_timeout(priv->timer, PHASE_MAX_MS);
            break;

        case PHASE_WAIT_DESTROY:
            gobj_stop(priv->gobj_uart);     // its read in flight
            check_uart(gobj, 2, ST_WAIT_STOPPED, "stopped with the read in flight");
            destroy_uart(gobj);
            check_uart(gobj, 1, 0, "destroyed in ST_WAIT_STOPPED");
            priv->phase = PHASE_CONNECT_WAIT_START;
            new_uart(gobj);
            set_timeout(priv->timer, PHASE_MAX_MS);
            break;

        case PHASE_WAIT_START:
            gobj_stop(priv->gobj_uart);     // its read in flight
            check_uart(gobj, 2, ST_WAIT_STOPPED, "stopped with the read in flight");
            if(gobj_start(priv->gobj_uart) != -1) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "A start in ST_WAIT_STOPPED was not refused",
                    NULL
                );
            }
            priv->phase = PHASE_WAIT_START_STOPPED;
            set_timeout(priv->timer, SETTLE_MS);
            break;

        case PHASE_WAIT_START_STOPPED:
            check_uart(gobj, 1, ST_STOPPED, "a start refused in ST_WAIT_STOPPED");
            check_count(gobj, "EV_STOPPED", priv->stopped, 2, "a start refused in ST_WAIT_STOPPED");
            gobj_stop(priv->gobj_uart);     // the refused start left it running
            destroy_uart(gobj);
            priv->phase = PHASE_CONNECT_ALIVE;
            new_uart(gobj);
            set_timeout(priv->timer, PHASE_MAX_MS);
            break;

        case PHASE_ALIVE:
            check_uart(gobj, 1, 0, "destroyed in the EV_DISCONNECTED of a stop");
            check_count(gobj, "EV_STOPPED", priv->stopped, 2, "destroyed in the EV_DISCONNECTED of a stop");
            priv->phase = PHASE_END;
            set_timeout(priv->timer, SETTLE_MS);
            break;

        case PHASE_END:
            set_yuno_must_die();
            break;

        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A phase did not end",
                "phase",        "%d", (int)priv->phase,
                "rx_count",     "%lu", (unsigned long)priv->rx_count,
                "tx_ready",     "%d", priv->tx_ready,
                "connections",  "%d", priv->connections,
                "uart_state",   "%s", priv->gobj_uart? gobj_current_state(priv->gobj_uart) : "",
                NULL
            );
            set_yuno_must_die();
            break;
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

    priv->connections++;
    check_uart(gobj, 2, ST_CONNECTED, "connected");

    switch(priv->phase) {
        case PHASE_CONNECT1:
            /*
             *  The write of 64 KB: more than the line takes at once
             */
            priv->phase = PHASE_SHORT_WRITE;
            slave_read(gobj, SLAVE_COUNT);
            send_pattern(gobj, 0, TX_SIZE);
            set_timeout(priv->timer, PHASE_MAX_MS);
            break;

        case PHASE_RECONNECT:
        case PHASE_CONNECT3:
            /*
             *  The line not read: the big write is held, and the small one
             *  behind it
             */
            priv->phase = (priv->phase == PHASE_RECONNECT)? PHASE_HELD_STOP : PHASE_HELD_DESTROY;
            slave_read(gobj, SLAVE_HOLD);
            send_pattern(gobj, 0, TX_SIZE);
            send_pattern(gobj, 0, HELD_TAIL);
            if(priv->phase == PHASE_HELD_DESTROY) {
                send_pattern(gobj, 0, HELD_TAIL);   // max_tx_queue 1: the one queued is dropped
            }
            set_timeout(priv->timer, SETTLE_MS);
            break;

        case PHASE_CONNECT_WAIT_DESTROY:
            priv->phase = PHASE_WAIT_DESTROY;
            set_timeout(priv->timer, SETTLE_MS);
            break;

        case PHASE_CONNECT_WAIT_START:
            priv->phase = PHASE_WAIT_START;
            set_timeout(priv->timer, SETTLE_MS);
            break;

        case PHASE_CONNECT_ALIVE:
            {
                /*
                 *  A byte to it: the host stops it in its EV_RX_DATA
                 */
                priv->phase = PHASE_ALIVE;
                uint8_t c = 0x55;
                if(write(priv->pty_master_fd, &c, 1) != 1) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SYSTEM,
                        "msg",          "%s", "Cannot write to the line",
                        "errno",        "%d", errno,
                        "strerror",     "%s", strerror(errno),
                        NULL
                    );
                }
                set_timeout(priv->timer, SETTLE_MS);
            }
            break;

        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "EV_CONNECTED not expected",
                "phase",        "%d", (int)priv->phase,
                NULL
            );
            break;
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The host of the PHASE_ALIVE destroys the C_UART here, on purpose (the
 *  kernel says it is wrong): the C_UART must touch nothing of itself after
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->disconnections++;
    if(priv->phase == PHASE_ALIVE && src == priv->gobj_uart) {
        destroy_uart(gobj);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase == PHASE_ALIVE && src == priv->gobj_uart) {
        gobj_stop(src);     // nothing in flight: EV_DISCONNECTED comes inside
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "EV_RX_DATA not expected",
            "phase",        "%d", (int)priv->phase,
            NULL
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tx_ready(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->tx_ready++;
    check_short_write_done(gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src != priv->timer) {
        priv->stopped++;
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
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST5);

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
        {EV_CONNECTED,              ac_connected,               0},
        {EV_DISCONNECTED,           ac_disconnected,            0},
        {EV_RX_DATA,                ac_rx_data,                 0},
        {EV_TX_READY,               ac_tx_ready,                0},
        {EV_TIMEOUT,                ac_timeout,                 0},
        {EV_STOPPED,                ac_stopped,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_CONNECTED,              0},
        {EV_DISCONNECTED,           0},
        {EV_RX_DATA,                0},
        {EV_TX_READY,               0},
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
PUBLIC int register_c_test5(void)
{
    return create_gclass(C_TEST5);
}
