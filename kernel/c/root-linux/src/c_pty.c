/***********************************************************************
 *          C_PTY.C
 *          Pty GClass.
 *
 *          Pseudoterminal uv-mixin.
 *
 *          Code inspired in the project: https://github.com/tsl0922/ttyd
 *          Copyright (c) 2016 Shuanglei Tao <tsl0922@gmail.com>
 *
 *          Copyright (c) 2021 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <pty.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include <yev_loop.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include "c_yuno.h"
#include "c_pty.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int fd_duplicate(int fd);
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
/*-ATTR-type------------name--------------------flag--------default-description---------- */
SDATA (DTP_STRING,      "process",              SDF_RD,     "bash", "Process to execute in pseudo terminal"),
SDATA (DTP_BOOLEAN,     "no_output",            0,          0,      "Mirror, only input"),
SDATA (DTP_INTEGER,     "rows",                 SDF_RD,     "24",   "Rows"),
SDATA (DTP_INTEGER,     "cols",                 SDF_RD,     "80",   "Columns"),
SDATA (DTP_STRING,      "cwd",                  SDF_RD,     "",     "Current work directory"),
SDATA (DTP_INTEGER,     "max_tx_queue",         SDF_WR,     "32",   "Maximum messages in tx queue. Default is 0: no limit."),
SDATA (DTP_POINTER,     "user_data",            0,          0,      "user data"),
SDATA (DTP_POINTER,     "user_data2",           0,          0,      "more user data"),
SDATA (DTP_POINTER,     "subscriber",           0,          0,      "subscriber of output-events. If it's null then subscriber is the parent."),
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
    int rows;
    int cols;
    char *argv[2]; // HACK Command or process (by the moment) without arguments

    // char uv_read_active;
    // char uv_req_write_active;
    // // uv_write_t uv_req_write;
    // /* Write request type. Careful attention must be paid when reusing objects of this type.
    //  * When a stream is in non-blocking mode, write requests sent with uv_write will be queued.
    //  * Reusing objects at this point is undefined behaviour.
    //  * It is safe to reuse the uv_write_t object only after the callback passed to uv_write is fired.
    //  */

    int uv_in;   // Duplicated fd of pty, use for put data into terminal
    int uv_out;  // Duplicated fd of pty, use for get the output of the terminal

    yev_event_h yev_reading;

    pid_t pty;      // file descriptor of pseudoterminal
    int pid;        // child pid

    dl_list_t dl_tx;
    gbuffer_t *gbuf_txing;
    json_int_t max_tx_queue;
    int tx_in_progress;
    json_int_t txMsgs;

    char slave_name[NAME_MAX+1];
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

    const char *process = gobj_read_str_attr(gobj, "process");
    priv->argv[0] = (char *)gbmem_strdup(process);
    priv->argv[1] = 0;

    dl_init(&priv->dl_tx, gobj);
    priv->pty = -1;
    priv->uv_in = -1;
    priv->uv_out = -1;

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(rows,          gobj_read_integer_attr)
    SET_PRIV(cols,          gobj_read_integer_attr)
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

    GBMEM_FREE(priv->argv[0]);
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
    int master, pid;

    if(priv->pty != -1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "pty terminal ALREADY open",
            NULL
        );
        return -1;
    }

    // like uv_disable_stdio_inheritance();
    set_cloexec(0);
    set_cloexec(1);
    set_cloexec(2);

    BOOL tty_empty = (empty_string(priv->argv[0]))?TRUE:FALSE;
    BOOL no_output = gobj_read_bool_attr(gobj, "no_output");

    struct winsize size = {
        priv->rows,
        priv->cols,
        0,
        0
    };
    pid = forkpty(&master, priv->slave_name, NULL, &size);
    if (pid < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "forkpty() FAILED",
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;

    } else if (pid == 0) {
        // Child
        setsid();

        putenv("TERM=linux");

        const char *cwd = gobj_read_str_attr(gobj, "cwd");
        if(!empty_string(cwd)) {
            int x = chdir(cwd);
            if(x) {} // avoid warning
        }

        if(!tty_empty) {
            int ret = execvp(priv->argv[0], priv->argv);
            if(ret < 0) {
                print_error(0, "forkpty() FAILED: %s", strerror(errno));
            }
            exit(0); // Child will die after exit of execute command
        } else {
            print_error(0, "🙋 exit pty, tty empty");
            exit(0); // Child will die after receive signal
        }
    }

    set_nonblocking(master);
    set_cloexec(master);

    if(1) {
        priv->uv_in = fd_duplicate(master);
        if(priv->uv_in < 0) {
           gobj_log_error(gobj, 0,
               "function",     "%s", __FUNCTION__,
               "msgset",       "%s", MSGSET_SYSTEM_ERROR,
               "msg",          "%s", "fd_duplicate() FAILED",
               "errno",        "%d", errno,
               "strerror",     "%s", strerror(errno),
               NULL
           );
           close(master);
           kill(pid, SIGKILL);
           waitpid(pid, NULL, 0);
           return -1;
       }
    }

    if(!no_output) {
        priv->uv_out = fd_duplicate(master);
        if(priv->uv_out < 0) {
           gobj_log_error(gobj, 0,
               "function",     "%s", __FUNCTION__,
               "msgset",       "%s", MSGSET_SYSTEM_ERROR,
               "msg",          "%s", "fd_duplicate() FAILED",
               "errno",        "%d", errno,
               "strerror",     "%s", strerror(errno),
               NULL
           );
           close(master);
           kill(pid, SIGKILL);
           waitpid(pid, NULL, 0);
           return -1;
       }
    }

    priv->pty = master;     // file descriptor of pseudoterminal
    priv->pid = pid;        // child pid

    if(priv->uv_out != -1) {
        /*-------------------------------*
         *      Setup reading event
         *-------------------------------*/
        json_int_t rx_buffer_size = 1024;
        if(!priv->yev_reading) {
            priv->yev_reading = yev_create_read_event(
                yuno_event_loop(),
                yev_callback,
                gobj,
                priv->uv_out,
                gbuffer_create(rx_buffer_size, rx_buffer_size)
            );
        }

        if(priv->yev_reading) {
            yev_set_fd(priv->yev_reading, priv->uv_out);

            if(!yev_get_gbuf(priv->yev_reading)) {
                yev_set_gbuffer(priv->yev_reading, gbuffer_create(rx_buffer_size, rx_buffer_size));
            } else {
                gbuffer_clear(yev_get_gbuf(priv->yev_reading));
            }

            yev_start_event(priv->yev_reading);
        }
    }

    json_t *kw_on_open = json_pack("{s:s, s:s, s:s, s:s, s:s, s:i, s:i, s:i}",
        "name", gobj_name(gobj),
        "process", priv->argv[0],
        "uuid", node_uuid(),
        "slave_name", priv->slave_name,
        "cwd", gobj_read_str_attr(gobj, "cwd"),
        "fd", master,
        "rows", (int)priv->rows,
        "cols", (int)priv->cols
    );
    gobj_publish_event(gobj, EV_TTY_OPEN, kw_on_open);

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

    if(priv->pty != -1) {
        close(priv->pty);
        priv->pty = -1;
    }
    if(priv->uv_in != -1) {
        close(priv->uv_in);
        priv->uv_in = -1;
    }
    if(priv->uv_out != -1) {
        close(priv->uv_out);
        priv->uv_out = -1;
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
        json_t *kw_on_close = json_pack("{s:s, s:s, s:s, s:s}}",
            "name", gobj_name(gobj),
            "process", priv->argv[0],
            "uuid", node_uuid(),
            "slave_name", priv->slave_name
        );
        gobj_publish_event(gobj, EV_TTY_CLOSE, kw_on_close);

        if(priv->pid > 0) {
            if(kill(priv->pid, 0) == 0) {
                kill(priv->pid, SIGKILL);
                waitpid(priv->pid, NULL, 0);
            }
            priv->pid = -1;
        }

        if(gobj_is_volatil(gobj)) {
            gobj_destroy(gobj);
        } else {
            gobj_publish_event(gobj, EV_STOPPED, 0);
        }
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int fd_duplicate(int fd)
{
    int fd_dup = dup(fd);
    if (fd_dup < 0) {
        return -1;
    }

    set_cloexec(fd_dup);

    return fd_dup;
}

/***************************************************************************
 *  on read callback
 ***************************************************************************/
PRIVATE int on_read_cb(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

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
            "READ from PTY %s",
            gobj_short_name(gobj)
        );
    }

    gbuffer_t *gbuf_base64 = gbuffer_string_to_base64(base, nread);
    if(!gbuf_base64) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "gbuffer_string_to_base64() FAILED",
            NULL
        );
        return -1;
    }

    json_t *kw = json_pack("{s:s, s:s, s:s, s:s}",
        "name", gobj_name(gobj),
        "process", priv->argv[0],
        "slave_name", priv->slave_name,
        "content64", gbuffer_cur_rd_pointer(gbuf_base64)
    );
    gobj_publish_event(gobj, EV_TTY_DATA, kw);
    gbuffer_decref(gbuf_base64);

    return 0;
}

/***************************************************************************
 *  Write data to pseudo terminal
 ***************************************************************************/
PRIVATE int write_data_to_pty(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = priv->gbuf_txing;

    // const char *bracket_paste_mode = "\e[200~";
    // size_t xl = strlen(bracket_paste_mode);
    // if(ln >= xl) {
    //     if(memcmp(bf, bracket_paste_mode, xl)==0) {
    //         bf += xl;
    //         ln -= xl;
    //     }
    // }

    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        size_t ln = gbuffer_chunk(gbuf);
        char *bf = gbuffer_cur_rd_pointer(gbuf);
        gobj_trace_dump(gobj,
            bf,
            ln,
            "WRITE to PTY %s",
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
        priv->uv_in,
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
        write_data_to_pty(gobj);
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
                "msg",          "%s", "Tiro mensaje tx",
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
PRIVATE int ac_write_tty(hgobj gobj, const char *event, json_t *kw, hgobj src)
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
        write_data_to_pty(gobj);
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
GOBJ_DEFINE_GCLASS(C_PTY);

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
        {EV_WRITE_TTY,  ac_write_tty,   0},
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
        /* Salidas públicas primero (para búsqueda rápida) */
        {EV_TTY_DATA,   EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT},
        {EV_TTY_OPEN,   EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT},
        {EV_TTY_CLOSE,  EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT},

        /* Entradas */
        {EV_WRITE_TTY,  0},

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
        0                   /* gcflag */
    );
    if(!__gclass__) {
        return -1;
    }
    return 0;
}

/***************************************************************************
 *          Public access
 ***************************************************************************/
PUBLIC int register_c_pty(void)
{
    return create_gclass(C_PTY);
}
