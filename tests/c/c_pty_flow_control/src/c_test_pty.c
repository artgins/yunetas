/***********************************************************************
 *          C_TEST_PTY.C
 *
 *          Drives a C_PTY through its flow control (rx_paused).
 *
 *          A bash in the C_PTY prints `seq 1 200000` (1.3 MB):
 *
 *          1. Paused INSIDE its EV_TTY_DATA (the read just ended, not
 *             re-armed): no EV_TTY_DATA comes in 500 ms.
 *          2. Resumed: the output comes again. Paused OUTSIDE (a read in
 *             flight), resumed while that read is still in flight (it
 *             must start no second read) and paused again: at most the
 *             read in flight comes in 500 ms.
 *          3. Resumed: every line comes, in order, none lost.
 *          4. Paused inside its EV_TTY_DATA again, and stopped while
 *             paused: its EV_TTY_CLOSE and EV_STOPPED come at once.
 *          5. A C_PTY created paused reads nothing (not even the prompt)
 *             until it is resumed.
 *
 *          A failure is an ERROR "TEST FAIL", which the strict FIFO of
 *          expected logs (main.c) does not hold.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test_pty.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define SEQ_LAST    200000

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int fail(hgobj gobj, const char *what, json_int_t value);
PRIVATE void write_line(hgobj pty, const char *line);
PRIVATE void parse_output(hgobj gobj, const char *data, size_t len);
GOBJ_DECLARE_EVENT(EV_PAUSE_OUTSIDE);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
PRIVATE const trace_level_t s_user_trace_level[16] = {
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef enum {
    PHASE_START = 0,
    PHASE_PAUSED_INSIDE,        // 1: paused in its EV_TTY_DATA
    PHASE_RESUMED,              // 2: resumed, waiting to pause outside
    PHASE_PAUSED_OUTSIDE,       // 2: paused with a read in flight
    PHASE_TO_THE_END,           // 3: resumed, until the last line
    PHASE_STOP_WHILE_PAUSED,    // 4
    PHASE_STOPPING,             // 4: stopped, waiting its close
    PHASE_CREATED_PAUSED,       // 5
    PHASE_CREATED_RESUMED,      // 5
    PHASE_DONE,
} phase_t;

typedef struct _PRIVATE_DATA {
    hgobj timer;
    hgobj pty;
    phase_t phase;
    int data_in_window;         // EV_TTY_DATA since the last pause
    json_int_t next_line;       // the next number of `seq` expected
    gbuffer_t *line;            // the line being received
    BOOL cr;                    // a '\r' not yet followed by '\n'
    BOOL seq_sent;
    BOOL ended;                 // END_SEQ seen
    BOOL tty_closed;
    BOOL pty_stopped;
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
    priv->line = gbuffer_create(256, 64*1024);
    priv->next_line = 1;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBUFFER_DECREF(priv->line)
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

    gobj_stop(priv->timer);
    if(priv->pty && gobj_is_running(priv->pty)) {
        gobj_stop(priv->pty);
    }
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->pty = gobj_create("pty", C_PTY,
        json_pack("{s:s, s:s}", "process", "bash", "cwd", "/tmp"),
        gobj
    );
    gobj_start(priv->pty);
    priv->phase = PHASE_START;
    set_timeout(priv->timer, 500);  // the prompt
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




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  A failed check: logged (the FIFO of expected logs fails), -1
 ***************************************************************************/
PRIVATE int fail(hgobj gobj, const char *what, json_int_t value)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "TEST FAIL",
        "what",         "%s", what,
        "value",        "%ld", (long)value,
        NULL
    );
    return -1;
}

/***************************************************************************
 *  Type `line` in the terminal
 ***************************************************************************/
PRIVATE void write_line(hgobj pty, const char *line)
{
    gbuffer_t *gbuf = gbuffer_create(strlen(line), strlen(line));
    gbuffer_append_string(gbuf, line);
    json_t *kw = json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf);
    gobj_send_event(pty, EV_WRITE_TTY, kw, pty);
    // the kw took the gbuffer: kw_decref() of the kw releases it
}

/***************************************************************************
 *  The lines of the output: a line of digits only is a number of `seq`,
 *  which must be the next one. What the terminal printed before it and
 *  overwrote (a '\r', an escape of bash) does not count.
 ***************************************************************************/
PRIVATE void parse_output(hgobj gobj, const char *data, size_t len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(size_t i=0; i<len; i++) {
        char c = data[i];
        if(c == '\r') {
            priv->cr = TRUE;
            continue;
        }
        if(c != '\n') {
            if(priv->cr) {
                gbuffer_clear(priv->line);  // overwritten by what follows the '\r'
                priv->cr = FALSE;
            }
            gbuffer_append_char(priv->line, c);
            continue;
        }
        priv->cr = FALSE;
        char *line = gbuffer_cur_rd_pointer(priv->line);
        size_t n = gbuffer_leftbytes(priv->line);
        BOOL digits = n > 0;
        for(size_t j=0; j<n; j++) {
            if(line[j] < '0' || line[j] > '9') {
                digits = FALSE;
                break;
            }
        }
        if(digits) {
            json_int_t number = 0;
            for(size_t j=0; j<n; j++) {
                number = number*10 + (line[j] - '0');
            }
            if(number != priv->next_line) {
                fail(gobj, "a line of the output lost or out of order, expected", priv->next_line);
                priv->next_line = number;
            }
            priv->next_line++;
        } else if(n == 7 && memcmp(line, "END_SEQ", 7)==0) {
            priv->ended = TRUE;
        }
        gbuffer_clear(priv->line);
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tty_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tty_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *content64 = kw_get_str(gobj, kw, "content64", "", 0);
    gbuffer_t *gbuf = gbuffer_base64_to_binary(content64, strlen(content64));
    if(gbuf) {
        parse_output(gobj, gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf));
        GBUFFER_DECREF(gbuf)
    }
    priv->data_in_window++;

    switch(priv->phase) {
        case PHASE_START:
            if(priv->next_line > 1000) {
                /*
                 *  1. Paused inside its publication: the read that just
                 *  ended is not re-armed
                 */
                gobj_write_bool_attr(src, "rx_paused", TRUE);
                priv->data_in_window = 0;
                priv->phase = PHASE_PAUSED_INSIDE;
                set_timeout(priv->timer, 500);
            }
            break;
        case PHASE_RESUMED:
            if(priv->data_in_window == 50) {
                /*
                 *  2. Outside the publication, on the next turn of the
                 *  loop: the read is in flight again by then
                 */
                gobj_post_event(gobj, EV_PAUSE_OUTSIDE, json_object(), gobj);
            }
            break;
        case PHASE_TO_THE_END:
            if(priv->ended) {
                /*
                 *  4. Something more to print, to pause inside again
                 */
                priv->phase = PHASE_STOP_WHILE_PAUSED;
                write_line(src, "echo AFTER\n");
            }
            break;
        case PHASE_STOP_WHILE_PAUSED:
            gobj_write_bool_attr(src, "rx_paused", TRUE);
            priv->phase = PHASE_STOPPING;
            set_timeout(priv->timer, 100);
            break;
        case PHASE_CREATED_PAUSED:
            fail(gobj, "a C_PTY created paused read its terminal", priv->data_in_window);
            break;
        default:
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tty_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->tty_closed = TRUE;
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->pty) {
        priv->pty_stopped = TRUE;
    }
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  2. Paused with a read in flight, resumed while it is still in flight
 *  (nothing to start), paused again
 ***************************************************************************/
PRIVATE int ac_pause_outside(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_bool_attr(priv->pty, "rx_paused", TRUE);
    gobj_write_bool_attr(priv->pty, "rx_paused", FALSE);
    gobj_write_bool_attr(priv->pty, "rx_paused", TRUE);
    priv->data_in_window = 0;
    priv->phase = PHASE_PAUSED_OUTSIDE;
    set_timeout(priv->timer, 500);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->phase) {
        case PHASE_START:
            if(priv->seq_sent) {
                fail(gobj, "the first lines did not come", priv->next_line - 1);
                set_yuno_must_die();
                break;
            }
            write_line(priv->pty, "seq 1 200000; echo END_\"\"SEQ\n");
            priv->seq_sent = TRUE;
            set_timeout(priv->timer, 10000);    // a failure if it fires
            break;

        case PHASE_PAUSED_INSIDE:
            if(priv->data_in_window != 0) {
                fail(gobj, "EV_TTY_DATA after a pause inside the publication", priv->data_in_window);
            }
            priv->data_in_window = 0;
            priv->phase = PHASE_RESUMED;
            gobj_write_bool_attr(priv->pty, "rx_paused", FALSE);
            set_timeout(priv->timer, 10000);    // a failure if it fires
            break;

        case PHASE_RESUMED:
            fail(gobj, "no output after the resume", priv->data_in_window);
            set_yuno_must_die();
            break;

        case PHASE_PAUSED_OUTSIDE:
            if(priv->data_in_window > 1) {
                fail(gobj, "EV_TTY_DATA after a pause outside the publication (one in flight at most)",
                    priv->data_in_window);
            }
            if(priv->ended) {
                fail(gobj, "the output ended before the pauses were checked", priv->next_line);
            }
            gobj_write_bool_attr(priv->pty, "rx_paused", FALSE);
            priv->phase = PHASE_TO_THE_END;
            set_timeout(priv->timer, 30000);    // a failure if it fires
            break;

        case PHASE_TO_THE_END:
        case PHASE_STOP_WHILE_PAUSED:
            fail(gobj, "the output did not end, lines", priv->next_line - 1);
            set_yuno_must_die();
            break;

        case PHASE_STOPPING:
            /*
             *  3. Every line, in order
             */
            if(priv->next_line != SEQ_LAST + 1) {
                fail(gobj, "lines of seq received", priv->next_line - 1);
            }
            /*
             *  4. Stopped while paused
             */
            gobj_stop(priv->pty);
            if(!priv->tty_closed || !priv->pty_stopped) {
                fail(gobj, "a C_PTY stopped while paused did not end at once (closed, stopped)",
                    priv->tty_closed*10 + priv->pty_stopped);
            }
            gobj_destroy(priv->pty);
            /*
             *  5. Created paused
             */
            priv->pty = gobj_create("pty2", C_PTY,
                json_pack("{s:s, s:s, s:b}", "process", "bash", "cwd", "/tmp", "rx_paused", 1),
                gobj
            );
            gobj_start(priv->pty);
            write_line(priv->pty, "echo HELLO\n");    // echoed and printed: output to read
            priv->data_in_window = 0;
            priv->phase = PHASE_CREATED_PAUSED;
            set_timeout(priv->timer, 500);
            break;

        case PHASE_CREATED_PAUSED:
            priv->phase = PHASE_CREATED_RESUMED;
            gobj_write_bool_attr(priv->pty, "rx_paused", FALSE);
            set_timeout(priv->timer, 2000);
            break;

        case PHASE_CREATED_RESUMED:
            if(priv->data_in_window == 0) {
                fail(gobj, "a C_PTY created paused read nothing once resumed", 0);
            }
            priv->phase = PHASE_DONE;
            set_yuno_must_die();
            break;

        default:
            break;
    }

    KW_DECREF(kw)
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
GOBJ_DEFINE_GCLASS(C_TEST_PTY);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_PAUSE_OUTSIDE);

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
        {EV_TTY_OPEN,               ac_tty_open,                0},
        {EV_TTY_DATA,               ac_tty_data,                0},
        {EV_TTY_CLOSE,              ac_tty_close,               0},
        {EV_TIMEOUT,                ac_timeout,                 0},
        {EV_STOPPED,                ac_stopped,                 0},
        {EV_PAUSE_OUTSIDE,          ac_pause_outside,           0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TTY_OPEN,               0},
        {EV_TTY_DATA,               0},
        {EV_TTY_CLOSE,              0},
        {EV_TIMEOUT,                0},
        {EV_STOPPED,                0},
        {EV_PAUSE_OUTSIDE,          0},
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
PUBLIC int register_c_test_pty(void)
{
    return create_gclass(C_TEST_PTY);
}
