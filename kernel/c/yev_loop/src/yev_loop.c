/****************************************************************************
 *          yev_loop.c
 *
 *          Yunetas Event Loop

1. Two-phase completion in zerocopy
    When you use io_uring_prep_sendmsg_zc(), the kernel may send data without copying it from your buffer.
    But your buffer cannot be freed immediately:
        The kernel keeps referencing your buffer until it’s transmitted.
        The zerocopy mechanism has to notify you when the buffer is finally safe to reuse.

2. Completion events
    First CQE → The send request completed (data queued for sending).
        cqe->res > 0 = number of bytes accepted for transmission.
    Second CQE → The zerocopy buffer release notification:
        cqe->res = 0 (no new data sent, just a notification).
        This is delivered after the NIC has finished using your buffer.
        It comes with a special flag:
        cqe->flags & IORING_CQE_F_NOTIF
        or sometimes IORING_CQE_F_MORE.

 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024-2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <liburing.h>
#include <unistd.h>
#include <errno.h>
#include <sys/timerfd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include <testing.h>
#include <helpers.h>
#include "yev_loop.h"


/***************************************************************
 *              Constants
 ***************************************************************/
int multishot_available = 0; // Available since kernel 5.19 NOT TESTED!! DONT'USE

/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct yev_loop_s yev_loop_t;

struct yev_loop_s {
    struct io_uring ring;
    unsigned entries;
    hgobj yuno;
    int keep_alive;
    volatile int running;
    volatile int stopping;
    yev_callback_t callback; // if return -1 the loop in yev_loop_run will break;
};

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE yev_state_t yev_set_state(yev_event_t *yev_event, yev_state_t new_state);
PRIVATE int print_addrinfo(hgobj gobj, char *bf, size_t bfsize, struct addrinfo *ai, int port);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE const char *yev_flag_s[] = {
    "YEV_FLAG_TIMER_PERIODIC",
    "YEV_FLAG_USE_TLS",
    "YEV_FLAG_CONNECTED",
    "YEV_FLAG_ACCEPT_DUP",
    "YEV_FLAG_ACCEPT_DUP2",
    0
};

PRIVATE volatile char __inside_loop__ = false;

PRIVATE int _yev_protocol_fill_hints( // fill hints according the schema
    const char *schema,
    struct addrinfo *hints,
    int *secure // fill true if needs TLS
);
PRIVATE yev_protocol_fill_hints_fn_t yev_protocol_fill_hints_fn = _yev_protocol_fill_hints;

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_loop_create(
    hgobj yuno,
    unsigned entries,
    int keep_alive,
    yev_callback_t callback,
    yev_loop_h *yev_loop_
)
{
    struct io_uring ring_test;
    int err;

    *yev_loop_ = 0;     // error case

    if(entries <= 0) {
        entries = DEFAULT_ENTRIES;
    }

    struct io_uring_params params_test = {0};
    //params_test.flags |= IORING_SETUP_COOP_TASKRUN; // Available since 5.18
    //params_test.flags |= IORING_SETUP_SINGLE_ISSUER; // Available since 6.0
retry:
    err = io_uring_queue_init_params(10, &ring_test, &params_test);
    if(err) {
        if (err == -EINVAL && params_test.flags & IORING_SETUP_SINGLE_ISSUER) {
            params_test.flags &= ~IORING_SETUP_SINGLE_ISSUER;
            goto retry;
        }
        if (err == -EINVAL && params_test.flags & IORING_SETUP_COOP_TASKRUN) {
            params_test.flags &= ~IORING_SETUP_COOP_TASKRUN;
            goto retry;
        }

        gobj_log_critical(yuno, LOG_OPT_ABORT,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "Linux kernel without io_uring, cannot run yunetas",
            "errno",        "%d", -err,
            "serrno",       "%s", strerror(-err),
            NULL
        );
        return -1;
    }
    io_uring_queue_exit(&ring_test);

    yev_loop_t *yev_loop = GBMEM_MALLOC(sizeof(yev_loop_t));
    if(!yev_loop) {
        gobj_log_critical(yuno, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "No memory to yev_loop",
            NULL
        );
        return -1;
    }
    struct io_uring_params params = {
        .flags = params_test.flags | IORING_SETUP_CLAMP
    };
    err = io_uring_queue_init_params(entries, &yev_loop->ring, &params);
    if (err < 0) {
        GBMEM_FREE(yev_loop)
        gobj_log_critical(yuno, LOG_OPT_ABORT,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "Linux io_uring_queue_init_params() FAILED, cannot run yunetas",
            "errno",        "%d", -err,
            "serrno",       "%s", strerror(-err),
            NULL
        );
        return -1;
    }

    yev_loop->yuno = yuno;
    yev_loop->entries = entries;
    yev_loop->keep_alive = keep_alive?keep_alive:60;
    yev_loop->callback = callback;

    *yev_loop_ = yev_loop;

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void yev_loop_destroy(yev_loop_h yev_loop_)
{
    yev_loop_t *yev_loop = (yev_loop_t *)yev_loop_;
    io_uring_queue_exit(&yev_loop->ring);
    GBMEM_FREE(yev_loop)
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int callback_cqe(yev_loop_t *yev_loop, struct io_uring_cqe *cqe)
{
    if(!cqe) {
        /*
         *  It's the timeout, call the yev_loop callback, if return -1 the loop will break;
         */
        if(yev_loop->callback) {
            return yev_loop->callback(0);
        }
        return 0;
    }

    yev_event_t *yev_event = (yev_event_t *)(uintptr_t)cqe->user_data;
    if(!yev_event) {
        // HACK CQE event without data is loop ending
        return -1; /* Break the loop */
    }

    hgobj gobj = yev_loop->running? (yev_loop->yuno?yev_event->gobj:0) : 0;
    int cqe_res = cqe->res;

    /*------------------------*
     *      Trace
     *------------------------*/
    uint32_t trace_level = gobj_global_trace_level();
    if(trace_level) {
        if(((trace_level & TRACE_URING) && yev_event->type != YEV_TIMER_TYPE) ||
            ((trace_level & TRACE_URING_TIME) && yev_event->type == YEV_TIMER_TYPE)
        ) {
            json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
            gobj_log_debug(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_YEV_LOOP,
                "msg",          "%s", "callback_cqe",
                "msg2",         "%s", "💥💥💥💥⏪ callback_cqe",
                "type",         "%s", yev_event_type_name(yev_event),
                "yev_state",    "%s", yev_get_state_name(yev_event),
                "loop_running", "%d", yev_loop->running?1:0,
                "p",            "%p", yev_event,
                "fd",           "%d", yev_get_fd(yev_event),
                "flag",         "%j", jn_flags,
                "cqe->res",     "%d", (int)cqe_res,
                "sres",         "%s", (cqe_res<0)? strerror(-cqe_res):"",
                NULL
            );
            json_decref(jn_flags);
        }
    }

    /*------------------------*
     *      Set state
     *------------------------*/
    yev_state_t cur_state = yev_get_state(yev_event);
    switch(cur_state) {
        case YEV_ST_RUNNING:  // cqe ready
            if(cqe_res > 0) {
                yev_set_state(yev_event, YEV_ST_IDLE);
            } else if(cqe_res < 0) {
                yev_set_state(yev_event, YEV_ST_STOPPED);
            } else { // cqe_res == 0
                // In READ events when the peer has closed the socket the reads return 0
                if(yev_event->type == YEV_READ_TYPE) {
                    cqe_res = -EPIPE; // Broken pipe (remote side closed connection)
                    yev_set_state(yev_event, YEV_ST_STOPPED);
                } else {
                    yev_set_state(yev_event, YEV_ST_IDLE);
                }
            }
            break;
        case YEV_ST_CANCELING: // cqe ready
            /*
             *  In the case of YEV_TIMER_TYPE and others, when canceling
             *      first receives cqe_res 0
             *      after receives ECANCELED
             */
            if(cqe_res < 0) {
                /*
                 *  HACK this error is information of disconnection cause.
                 */
                yev_set_state(yev_event, YEV_ST_STOPPED);

            } else if(cqe_res >= 0) {
                /*
                 *  When canceling it could arrive events type with res >= 0
                 *  Wait to one negative
                 */
                /*
                 *  Mark this request as processed
                 */
                return 0;
            }
            break;

        case YEV_ST_STOPPED: // cqe ready
            /*
             *  When not running there is a IORING_ASYNC_CANCEL_ANY submit
             *  and it can receive cqe_res = -2 (No such file or directory)
             *
             *  Cases seen:
             *      - Cancel the read event (previously the socket was closed for testing)
             *      - receive a read  "errno": -104, "strerror": "Connection reset by peer"
             *          causing to set the read event in STOPPED state.
             *      - receive another read event with "errno": -2, "No such file or directory"
             *          this cause a log_warning "receive event in stopped state"
             *          the event is ignored
             */
            if(cqe_res != -2) {
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_LIBURING_ERROR,
                    "msg",          "%s", "receive event in stopped state",
                    "event_type",   "%s", yev_event_type_name(yev_event),
                    "yev_state",    "%s", yev_get_state_name(yev_event),
                    "p",            "%p", yev_event,
                    "cqe_res",     "%d", (int)cqe_res,
                    "sres",         "%s", (cqe_res<0)? strerror(-cqe_res):"",
                    NULL
                );
            }
            /*
             *  Don't call callback again
             *  if the state is STOPPED the callback was already done,
             *  It'd normally receive first cqe_res = 0 and later cqe_res = -ECANCELED
             */

            /* Mark this request as processed */
            return 0;

        case YEV_ST_IDLE: // cqe ready
            if(yev_event->type == YEV_SENDMSG_TYPE) {
                break;
            }
            /* fall through */
        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING_ERROR,
                "msg",          "%s", "Wrong STATE receiving cqe",
                "event_type",   "%s", yev_event_type_name(yev_event),
                "yev_state",    "%s", yev_get_state_name(yev_event),
                "p",            "%p", yev_event,
                "cqe_res",      "%d", (int)cqe_res,
                "sres",         "%s", (cqe_res<0)? strerror(-cqe_res):"",
                NULL
            );
            /* Mark this request as processed */
            return 0;
    }

    if(cur_state != yev_get_state(yev_event)) {
        // State has changed
        if(trace_level) {
            if(((trace_level & TRACE_URING) && yev_event->type != YEV_TIMER_TYPE) ||
               ((trace_level & TRACE_URING_TIME) && yev_event->type == YEV_TIMER_TYPE)
                ) {
                json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
                gobj_log_debug(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_YEV_LOOP,
                    "msg",          "%s", "callback_cqe NEW STATE",
                    "msg2",         "%s", "💥💥💥💥⏪ callback_cqe NEW STATE",
                    "type",         "%s", yev_event_type_name(yev_event),
                    "yev_state",    "%s", yev_get_state_name(yev_event),
                    "loop_running", "%d", yev_loop->running?1:0,
                    "p",            "%p", yev_event,
                    "fd",           "%d", yev_get_fd(yev_event),
                    "flag",         "%j", jn_flags,
                    "cqe_res",     "%d", (int)cqe_res,
                    "sres",         "%s", (cqe_res<0)? strerror(-cqe_res):"",
                    NULL
                );
                json_decref(jn_flags);
            }
        }
        cur_state = yev_get_state(yev_event);
    }

    /*-------------------------------*
     *      cqe ready
     *-------------------------------*/
    int ret = 0;
    switch((yev_type_t)yev_event->type) {
        case YEV_CONNECT_TYPE: // cqe ready
            {
                if(cur_state == YEV_ST_IDLE) {
                    // HACK res == 0 when connected
                    yev_set_flag(yev_event, YEV_FLAG_CONNECTED, TRUE);
                } else {
                    yev_set_flag(yev_event, YEV_FLAG_CONNECTED, FALSE);
                    if(yev_event->fd > 0) {
                        if(gobj_trace_level(0) & (TRACE_URING)) {
                            gobj_log_debug(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_YEV_LOOP,
                                "msg",          "%s", "close socket",
                                "msg2",         "%s", "💥🟥 close socket",
                                "fd",           "%d", yev_event->fd ,
                                "p",            "%p", yev_event,
                                NULL
                            );
                        }
                        close(yev_event->fd);
                        yev_event->fd = -1;
                    }
                }

                /*
                 *  Call callback
                 */
                yev_event->result = cqe_res;
                if(yev_event->callback) {
                    ret = yev_event->callback(
                        yev_event
                    );
                }
            }
            break;

        case YEV_ACCEPT_TYPE: // cqe ready
            {
                /*
                 *  Call callback
                 */
                yev_event->result = cqe_res; // HACK: is the cli_srv socket

                if(yev_event->result > 0) {
                    // set_nonblocking(yev_event->result); // Already set in io_uring_prep_accept
                    // set_cloexec(yev_event->result);

                    if (is_tcp_socket(yev_event->result)) {
                        set_tcp_socket_options(yev_event->result, yev_loop->keep_alive);
                    }
                }

                if(yev_event->callback) {
                    ret = yev_event->callback(
                        yev_event
                    );
                }
                if(ret == 0 && yev_loop->running &&
                        yev_event->state == YEV_ST_IDLE &&
                        !(yev_event->flag & YEV_FLAG_ACCEPT_DUP2)) {
                    if(!gobj || (gobj && gobj_is_running(gobj))) {
                        /*
                         *  Rearm accept event
                         */
                        struct io_uring_sqe *sqe = io_uring_get_sqe(&yev_loop->ring);
                        io_uring_sqe_set_data(sqe, yev_event);
                        io_uring_prep_accept(
                            sqe,
                            yev_event->fd,
                            &yev_event->sock_info->addr,
                            &yev_event->sock_info->addrlen,
                            SOCK_CLOEXEC | SOCK_NONBLOCK
                        );
                        io_uring_submit(&yev_loop->ring);
                        yev_set_state(yev_event, YEV_ST_RUNNING); // re-arming

                        if(trace_level & TRACE_URING) {
                            json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
                            gobj_log_debug(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_YEV_LOOP,
                                "msg",          "%s", "re-arming accept event",
                                "msg2",         "%s", "💥💥⏩ re-arming accept event",
                                "type",         "%s", yev_event_type_name(yev_event),
                                "yev_state",    "%s", yev_get_state_name(yev_event),
                                "fd",           "%d", yev_get_fd(yev_event),
                                "p",            "%p", yev_event,
                                "gbuffer",      "%p", yev_event->gbuf,
                                "flag",         "%j", jn_flags,
                                NULL
                            );
                            json_decref(jn_flags);
                        }
                    }
                }
            }
            break;

        case YEV_WRITE_TYPE: // cqe ready
        case YEV_SENDMSG_TYPE: // SEE doc of zerocopy in header
            {
                if(cqe_res > 0 && yev_event->gbuf) {
                    // Pop the read bytes used to write fd
                    gbuffer_get(yev_event->gbuf, cqe_res);
                }

                /*
                 *  Call callback
                 */
                yev_event->result = cqe_res;
                if(yev_event->callback) {
                    ret = yev_event->callback(
                        yev_event
                    );
                }
            }
            break;

        case YEV_READ_TYPE: // cqe ready
        case YEV_RECVMSG_TYPE:
            {
                if(cqe_res > 0 && yev_event->gbuf) {
                    // Mark the written bytes of reading fd
                    gbuffer_set_wr(yev_event->gbuf, cqe_res);
                }

                /*
                 *  Call callback
                 */
                yev_event->result = cqe_res;
                if (yev_event->callback) {
                    ret = yev_event->callback(
                        yev_event
                    );
                }
            }
            break;

        case YEV_POLL_TYPE: // cqe ready
        {
            /*
             *  Call callback
             */
            yev_event->result = cqe_res;
            if (yev_event->callback) {
                ret = yev_event->callback(
                    yev_event
                );
            }
        }
        break;

        case YEV_TIMER_TYPE: // cqe ready
            {
                /*
                 *  Call callback
                 */
                yev_event->result = cqe_res;
                if(yev_event->callback) {
                    ret = yev_event->callback(
                        yev_event
                    );
                }

                if(ret == 0 && yev_loop->running && yev_event->state == YEV_ST_IDLE &&
                        (yev_event->flag & YEV_FLAG_TIMER_PERIODIC)) {
                    if(!gobj || (gobj && gobj_is_running(gobj))) {
                        /*
                         *  Rearm periodic timer event
                         */
                        struct io_uring_sqe *sqe = io_uring_get_sqe(&yev_loop->ring);
                        io_uring_sqe_set_data(sqe, yev_event);
                        io_uring_prep_read(
                            sqe,
                            yev_event->fd,
                            &yev_event->timer_bf,
                            sizeof(yev_event->timer_bf),
                            0
                        );
                        io_uring_submit(&yev_loop->ring);
                        yev_set_state(yev_event, YEV_ST_RUNNING); // re-arming

                        if(trace_level & TRACE_URING_TIME) {
                            json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
                            gobj_log_debug(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_YEV_LOOP,
                                "msg",          "%s", "re-arming timer event",
                                "msg2",         "%s", "💥💥⏩ re-arming timer event",
                                "type",         "%s", yev_event_type_name(yev_event),
                                "yev_state",    "%s", yev_get_state_name(yev_event),
                                "fd",           "%d", yev_get_fd(yev_event),
                                "p",            "%p", yev_event,
                                "gbuffer",      "%p", yev_event->gbuf,
                                "flag",         "%j", jn_flags,
                                NULL
                            );
                            json_decref(jn_flags);
                        }
                    }
                }
            }
            break;
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_loop_run(yev_loop_h yev_loop_, int timeout_in_seconds)
{
#ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
    char print_temp[120]={0};
    int measuring_times = get_measure_times();
#endif
    yev_loop_t *yev_loop = (yev_loop_t *)yev_loop_;

    if(__inside_loop__) {
        gobj_log_error(yev_loop->yuno, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING_ERROR,
            "msg",          "%s", "ALREADY running a event loop",
            NULL
        );
        return -1;
    }
    __inside_loop__ = TRUE;

    /*------------------------------------------*
     *      Infinite loop
     *------------------------------------------*/
    uint32_t level = gobj_global_trace_level();
    if(level & (TRACE_MACHINE|TRACE_START_STOP|TRACE_URING)) {
        gobj_log_debug(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev loop running",
            "msg2",         "%s", "💥🟩 yev loop running",
            "timeout",      "%d", timeout_in_seconds,
            NULL
        );
    }

    struct io_uring_cqe *cqe;
    yev_loop->running = TRUE;
    while(yev_loop->running) {
        int err;
        if(timeout_in_seconds > 0) {
            struct __kernel_timespec timeout = { .tv_sec = timeout_in_seconds, .tv_nsec = 0 };
            err = io_uring_wait_cqe_timeout(&yev_loop->ring, &cqe, &timeout);
        } else {
            err = io_uring_wait_cqe(&yev_loop->ring, &cqe);
        }

        /*
         *  To measure the time of executing of the event
         */
        #ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
        MT_START_TIME2(yev_time_measure, 1)
        #endif
        if (err < 0) {
            if(err == -EINTR) {
                // Ctrl+C cause this

                /* Mark this request as processed */
                io_uring_cqe_seen(&yev_loop->ring, cqe);
                continue;
            }
            if(err == -ETIME) {
                // Timeout
                #ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
                if(measuring_times & YEV_TIMER_TYPE) {
                    MT_PRINT_TIME(yev_time_measure, "BEFORE callback_cqe TIMEOUT");
                }
                #endif

                if(callback_cqe(yev_loop, NULL)<0) {
                    yev_loop->running = false;
                }

                #ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
                if(measuring_times & YEV_TIMER_TYPE) {
                    MT_PRINT_TIME(yev_time_measure, "AFTER callback_cqe TIMEOUT");
                }
                #endif

                /* Mark this request as processed */
                io_uring_cqe_seen(&yev_loop->ring, cqe);
                continue;
            }
            gobj_log_error(yev_loop->yuno, LOG_OPT_TRACE_STACK|LOG_OPT_ABORT,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING_ERROR,
                "msg",          "%s", "io_uring_wait_cqe() FAILED",
                "err",          "%d", -err,
                "serr",         "%s", strerror(-err),
                NULL
            );
            /*
             * Mark this request as processed
             */
            io_uring_cqe_seen(&yev_loop->ring, cqe);
            break;
        }

        #ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
        yev_event_t *yev_event = (yev_event_t *)(uintptr_t)cqe->user_data;
        int yev_event_type = yev_event? yev_event->type:0;
        measuring_cur_type = measuring_times & yev_event_type;
        if(measuring_cur_type) {
            snprintf(print_temp, sizeof(print_temp), "BEFORE callback_cqe(%s), res %d",
                yev_event_type_name(yev_event),
                cqe->res
            );
            MT_PRINT_TIME(yev_time_measure, print_temp);
        }
        #endif

        if(callback_cqe(yev_loop, cqe)<0) {
            yev_loop->running = false;
        }

        #ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
        if(measuring_cur_type) {
            snprintf(print_temp, sizeof(print_temp), "AFTER callback_cqe(%s): res %d\n",
                yev_event?yev_event_type_name(yev_event):"",
                cqe->res
            );
            MT_PRINT_TIME(yev_time_measure, print_temp);
        }
        #endif

        /*
         * Mark this request as processed
         */
        io_uring_cqe_seen(&yev_loop->ring, cqe);
    }

    if(level & (TRACE_MACHINE|TRACE_START_STOP|TRACE_URING)) {
        gobj_log_debug(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev loop exited",
            "msg2",         "%s", "💥🟩 yev loop exited",
            "timeout",      "%d", timeout_in_seconds,
            NULL
        );
    }

    __inside_loop__ = false;

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_loop_run_once(yev_loop_h yev_loop_)
{
    yev_loop_t *yev_loop = (yev_loop_t *)yev_loop_;
    struct io_uring_cqe *cqe;

    if(__inside_loop__) {
        gobj_log_error(yev_loop->yuno, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING_ERROR,
            "msg",          "%s", "ALREADY running a event loop",
            NULL
        );
        return -1;
    }
    __inside_loop__ = TRUE;

    if(gobj_is_level_tracing(0, TRACE_MACHINE|TRACE_START_STOP|TRACE_URING)) {
        gobj_log_debug(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev loop ONCE running",
            "msg2",         "%s", "💥🟩 yev loop ONCE running",
            NULL
        );
    }

    #ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
    int measuring_times = get_measure_times();
    MT_START_TIME(yev_time_measure)
    MT_SET_COUNT(yev_time_measure, 1)
    #endif

    cqe = 0;
    while(io_uring_peek_cqe(&yev_loop->ring, &cqe)==0) {
        #ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
        yev_event_t *yev_event = (yev_event_t *)(uintptr_t)cqe->user_data;
        int yev_event_type = yev_event? yev_event->type:0;
        measuring_cur_type = measuring_times & yev_event_type;
        if(measuring_cur_type) {
            char temp[80];
            snprintf(temp, sizeof(temp), "run1 BEFORE callback_cqe(%s), res %d",
                yev_event_type_name(yev_event),
                cqe->res
            );
            MT_PRINT_TIME(yev_time_measure, temp);
        }
        #endif

        if(callback_cqe(yev_loop, cqe)<0) {
            if(yev_loop->stopping) {
                break;
            }
        }
        /* Mark this request as processed */
        io_uring_cqe_seen(&yev_loop->ring, cqe);

        #ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
        if(measuring_cur_type) {
            MT_PRINT_TIME(yev_time_measure, "run1 AFTER io_uring_cqe_seen()\n");
        }
        #endif
    }

    if(gobj_is_level_tracing(0, TRACE_MACHINE|TRACE_START_STOP|TRACE_URING)) {
        gobj_log_debug(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev loop ONCE exited",
            "msg2",         "%s", "💥🟩 yev loop ONCE exited",
            NULL
        );
    }

    __inside_loop__ = false;

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_loop_stop(yev_loop_h yev_loop_)
{
    yev_loop_t *yev_loop = (yev_loop_t *)yev_loop_;

    if(!yev_loop->stopping) {
        yev_loop->stopping = TRUE;
        if(gobj_trace_level(0) & TRACE_URING) {
            gobj_log_debug(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_YEV_LOOP,
                "msg",          "%s", "yev_loop_stop",
                "msg2",         "%s", "💥🟥🟥🟥🟥 yev_loop_stop",
                NULL
            );
        }

        struct io_uring_sqe *sqe;
        sqe = io_uring_get_sqe(&yev_loop->ring);
        io_uring_sqe_set_data(sqe, NULL);  // HACK CQE event without data is loop ending
        io_uring_prep_cancel(sqe, 0, IORING_ASYNC_CANCEL_ANY);
        io_uring_submit(&yev_loop->ring);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void yev_loop_reset_running(yev_loop_h yev_loop)
{
    ((yev_loop_t *)yev_loop)->running = 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_protocol_set_protocol_fill_hints_fn(
    yev_protocol_fill_hints_fn_t yev_protocol_fill_hints
)
{
    yev_protocol_fill_hints_fn = yev_protocol_fill_hints;
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _yev_protocol_fill_hints( // fill hints according the schema
    const char *schema,
    struct addrinfo *hints,
    int *secure // fill true if needs TLS
)
{
    SWITCHS(schema) { // WARNING Repeated
        ICASES("tcp")
        ICASES("tcp4h")
        ICASES("http")
        ICASES("mqtt")
        ICASES("ws")
            hints->ai_socktype = SOCK_STREAM; /* TCP socket */
            hints->ai_protocol = IPPROTO_TCP;
            *secure = false;
            break;

        ICASES("tcps")
        ICASES("tcp4hs")
        ICASES("https")
        ICASES("mqtts")
        ICASES("wss")
            hints->ai_socktype = SOCK_STREAM; /* TCP socket */
            hints->ai_protocol = IPPROTO_TCP;
            *secure = TRUE;
            break;

        ICASES("udps")
            hints->ai_socktype = SOCK_DGRAM; /* UDP socket */
            hints->ai_protocol = IPPROTO_UDP;
            *secure = TRUE;
            break;
        ICASES("udp")
            hints->ai_socktype = SOCK_DGRAM; /* UDP socket */
            hints->ai_protocol = IPPROTO_UDP;
            *secure = false;
            break;

        DEFAULTS
            gobj_log_warning(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING_ERROR,
                "msg",          "%s", "schema NOT supported, using tcp",
                "schema",       "%s", schema,
                NULL
            );
            hints->ai_socktype = SOCK_STREAM; /* TCP socket */
            hints->ai_protocol = IPPROTO_TCP;
            *secure = false;
            return -1;
    } SWITCHS_END

    return 0;
}

/***************************************************************************
 *  Return previous state
 ***************************************************************************/
PRIVATE yev_state_t yev_set_state(yev_event_t *yev_event, yev_state_t new_state)
{
    yev_state_t prev_state = yev_event->state;
    yev_event->state = new_state;
    return prev_state;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC const char * yev_get_state_name(yev_event_h yev_event)
{
    if (yev_event->state == YEV_ST_IDLE) {
        return "ST_IDLE";

    } else if (yev_event->state == YEV_ST_RUNNING) {
        return "ST_RUNNING";

    } else if (yev_event->state == YEV_ST_CANCELING) {
        return "ST_CANCELING";

    } else if(yev_event->state == YEV_ST_STOPPED) {
        return "ST_STOPPED";

    } else {
        return "???";
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_set_gbuffer( // only for yev_create_read_event() and yev_create_write_event()
    yev_event_h yev_event,
    gbuffer_t *gbuf // WARNING if there is previous gbuffer it will be free
                    // if NULL reset the current gbuf
) {
    if(gbuf && gbuf == yev_event->gbuf) {
        return 0;
    }
    if(!gbuf) {
        GBUFFER_DECREF(yev_event->gbuf)
    }
    if(yev_event->gbuf) {
        gobj_log_warning(yev_event->gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING_ERROR,
            "msg",          "%s", "Re-writing gbuffer",
            "gbuffer",      "%p", yev_event->gbuf,
            NULL
        );
        GBUFFER_DECREF(yev_event->gbuf)
    }
    yev_event->gbuf = gbuf;
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgobj yev_get_yuno(yev_loop_h yev_loop)
{
    return ((yev_loop_t *)yev_loop)->yuno;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_start_event(
    yev_event_h yev_event_
) {
    yev_event_t *yev_event = (yev_event_t *)yev_event_;

    /*------------------------*
     *  Check parameters
     *------------------------*/
    if(!yev_event) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING_ERROR,
            "msg",          "%s", "yev_event NULL",
            NULL
        );
        return -1;
    }

    yev_loop_t *yev_loop = yev_event->yev_loop;
    hgobj gobj = yev_loop->yuno?yev_event->gobj:0;

    /*------------------------*
     *      Trace
     *------------------------*/
    uint32_t trace_level = gobj_global_trace_level();
    if(trace_level & TRACE_URING) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev_start_event",
            "msg2",         "%s", "💥💥⏩ yev_start_event",
            "type",         "%s", yev_event_type_name(yev_event),
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "fd",           "%d", yev_get_fd(yev_event),
            "p",            "%p", yev_event,
            "gbuffer",      "%p", yev_event->gbuf,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);
    }

    /*---------------------------*
     *      Check state
     *---------------------------*/
    yev_state_t cur_state = yev_get_state(yev_event);
    if(!(cur_state == YEV_ST_IDLE || cur_state == YEV_ST_STOPPED)) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
        const char *msg = (cur_state==YEV_ST_RUNNING)?
            "cannot start event: is RUNNING":
            "cannot start event: is CANCELING";
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING_ERROR,
            "msg",          "%s", msg,
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "type",         "%s", yev_event_type_name(yev_event),
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "fd",           "%d", yev_get_fd(yev_event),
            "p",            "%p", yev_event,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);
        return -1;
    }

    /*-------------------------------*
     *      Summit sqe
     *-------------------------------*/
    switch((yev_type_t)yev_event->type) {
        case YEV_CONNECT_TYPE: // Summit sqe
            {
                if(!yev_event->sock_info || yev_event->sock_info->addrlen <= 0) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING_ERROR,
                        "msg",          "%s", "Cannot start event: connect addr NULL",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }

                struct io_uring_sqe *sqe = io_uring_get_sqe(&yev_loop->ring);
                io_uring_sqe_set_data(sqe, yev_event);
                /*
                 *  Use the file descriptor fd to start connecting to the destination
                 *  described by the socket address at addr and of structure length addrlen.
                 */
                io_uring_prep_connect(
                    sqe,
                    yev_event->fd,
                    &yev_event->sock_info->addr,
                    yev_event->sock_info->addrlen
                );
                io_uring_submit(&yev_loop->ring);
                yev_set_state(yev_event, YEV_ST_RUNNING);
                yev_set_flag(yev_event, YEV_FLAG_CONNECTED, false);
            }
            break;

        case YEV_ACCEPT_TYPE: // Summit sqe
            {
                if(!yev_event->sock_info || yev_event->sock_info->addrlen <= 0) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING_ERROR,
                        "msg",          "%s", "Cannot start event: accept addr NULL",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        "sock_info",    "%d", yev_event->sock_info?1:0,
                        "addrlen",      "%d", (int)yev_event->sock_info->addrlen,
                        NULL
                    );
                    return -1;
                }

                if(is_tcp_socket(yev_event->fd)) {
                    /*
                     *  Use the file descriptor fd to start accepting a connection request
                     *  described by the socket address at addr and of structure length addrlen
                     */
                    struct io_uring_sqe *sqe = io_uring_get_sqe(&yev_loop->ring);
                    io_uring_sqe_set_data(sqe, yev_event);

                    if(multishot_available) {
                        io_uring_prep_multishot_accept(
                            sqe,
                            yev_event->fd,
                            NULL,
                            NULL,
                            SOCK_CLOEXEC | SOCK_NONBLOCK
                        );

                    } else {
                        io_uring_prep_accept(
                            sqe,
                            yev_event->fd,
                            &yev_event->sock_info->addr,
                            &yev_event->sock_info->addrlen,
                            SOCK_CLOEXEC | SOCK_NONBLOCK
                        );
                    }
                    io_uring_submit(&yev_loop->ring);
                    yev_set_state(yev_event, YEV_ST_RUNNING);

                } else if(is_udp_socket(yev_event->fd)) {

                }
            }
            break;

        case YEV_WRITE_TYPE: // Summit sqe
            {
                if(yev_event->fd <= 0) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING_ERROR,
                        "msg",          "%s", "Cannot start event: fd negative",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }
                if(!yev_event->gbuf) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING_ERROR,
                        "msg",          "%s", "Cannot start event: gbuffer NULL",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }
                if(gbuffer_leftbytes(yev_event->gbuf)==0) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING_ERROR,
                        "msg",          "%s", "Cannot start event: gbuffer WITHOUT data to write",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        "gbuf_label",   "%s", gbuffer_getlabel(yev_event->gbuf),
                        NULL
                    );
                    return -1;
                }

                struct io_uring_sqe *sqe = io_uring_get_sqe(&yev_loop->ring);
                if(sqe) {
                    io_uring_sqe_set_data(sqe, yev_event);
                    io_uring_prep_write(
                        sqe,
                        yev_event->fd,
                        gbuffer_cur_rd_pointer(yev_event->gbuf),
                        gbuffer_leftbytes(yev_event->gbuf),
                        0
                    );
                    io_uring_submit(&yev_loop->ring);
                    yev_set_state(yev_event, YEV_ST_RUNNING);
                } else {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK|LOG_OPT_ABORT,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING_ERROR,
                        "msg",          "%s", "io_uring_get_sqe() FAILED",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }
            }
            break;

        case YEV_READ_TYPE: // Summit sqe
            {
                if(yev_event->fd <= 0) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING_ERROR,
                        "msg",          "%s", "Cannot start event: fd negative",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }
                if(!yev_event->gbuf) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING_ERROR,
                        "msg",          "%s", "Cannot start event: gbuffer NULL",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }
                if(gbuffer_freebytes(yev_event->gbuf)==0) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING_ERROR,
                        "msg",          "%s", "Cannot start event: gbuffer WITHOUT space to read",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        "gbuf_label",   "%s", gbuffer_getlabel(yev_event->gbuf),
                        NULL
                    );
                    return -1;
                }

                struct io_uring_sqe *sqe = io_uring_get_sqe(&yev_loop->ring);
                io_uring_sqe_set_data(sqe, yev_event);
                io_uring_prep_read(
                    sqe,
                    yev_event->fd,
                    gbuffer_cur_wr_pointer(yev_event->gbuf),
                    gbuffer_freebytes(yev_event->gbuf),
                    0
                );
                io_uring_submit(&yev_loop->ring);
                yev_set_state(yev_event, YEV_ST_RUNNING);
            }
            break;

        case YEV_SENDMSG_TYPE: // Summit sqe, SEE doc of zerocopy in header
            {
                if(yev_event->fd <= 0) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING_ERROR,
                        "msg",          "%s", "Cannot start event: fd negative",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }
                if(!yev_event->gbuf) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING_ERROR,
                        "msg",          "%s", "Cannot start event: gbuffer NULL",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }
                if(gbuffer_leftbytes(yev_event->gbuf)==0) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING_ERROR,
                        "msg",          "%s", "Cannot start event: gbuffer WITHOUT data to write",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        "gbuf_label",   "%s", gbuffer_getlabel(yev_event->gbuf),
                        NULL
                    );
                    return -1;
                }

                if(!yev_event->msghdr->msg_name || yev_event->msghdr->msg_namelen <= 0) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING_ERROR,
                        "msg",          "%s", "Cannot start event: sendmsg addr NULL",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }

                struct io_uring_sqe *sqe = io_uring_get_sqe(&yev_loop->ring);
                if(sqe) {
                    io_uring_sqe_set_data(sqe, yev_event);

                    yev_event->iov.iov_base = gbuffer_cur_rd_pointer(yev_event->gbuf);
                    yev_event->iov.iov_len = gbuffer_leftbytes(yev_event->gbuf);

                    io_uring_prep_sendmsg_zc(
                        sqe,
                        yev_event->fd,
                        yev_event->msghdr,
                        0
                    );
                    io_uring_submit(&yev_loop->ring);
                    yev_set_state(yev_event, YEV_ST_RUNNING);
                } else {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK|LOG_OPT_ABORT,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING_ERROR,
                        "msg",          "%s", "io_uring_get_sqe() FAILED",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }
            }
            break;

        case YEV_RECVMSG_TYPE: // Summit sqe
            {
                if(yev_event->fd <= 0) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING_ERROR,
                        "msg",          "%s", "Cannot start event: fd negative",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }
                if(!yev_event->gbuf) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING_ERROR,
                        "msg",          "%s", "Cannot start event: gbuffer NULL",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }
                if(gbuffer_freebytes(yev_event->gbuf)==0) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING_ERROR,
                        "msg",          "%s", "Cannot start event: gbuffer WITHOUT space to read",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        "gbuf_label",   "%s", gbuffer_getlabel(yev_event->gbuf),
                        NULL
                    );
                    return -1;
                }

                struct io_uring_sqe *sqe = io_uring_get_sqe(&yev_loop->ring);
                io_uring_sqe_set_data(sqe, yev_event);

                yev_event->iov.iov_base = gbuffer_cur_wr_pointer(yev_event->gbuf);
                yev_event->iov.iov_len = gbuffer_freebytes(yev_event->gbuf);

                io_uring_prep_recvmsg(
                    sqe,
                    yev_event->fd,
                    yev_event->msghdr,
                    0
                );
                io_uring_submit(&yev_loop->ring);
                yev_set_state(yev_event, YEV_ST_RUNNING);
            }
            break;

        case YEV_POLL_TYPE: // Summit sqe
            {
                if(yev_event->fd <= 0) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING_ERROR,
                        "msg",          "%s", "Cannot start event: fd negative",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }

                struct io_uring_sqe *sqe = io_uring_get_sqe(&yev_loop->ring);
                io_uring_sqe_set_data(sqe, yev_event);
                io_uring_prep_poll_add(
                    sqe,
                    yev_event->fd,
                    yev_event->poll_mask
                );
                io_uring_submit(&yev_loop->ring);
                yev_set_state(yev_event, YEV_ST_RUNNING);
            }
            break;

        case YEV_TIMER_TYPE: // Summit sqe
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING_ERROR,
                "msg",          "%s", "Cannot start event: use yev_start_timer_event() to start timer event",
                "event_type",   "%s", yev_event_type_name(yev_event),
                "yev_state",    "%s", yev_get_state_name(yev_event),
                "p",            "%p", yev_event,
                NULL
            );
            return -1;
    }

    if(cur_state != yev_get_state(yev_event)) {
        // State has changed
        if(trace_level & TRACE_URING) {
            json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
            gobj_log_debug(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_YEV_LOOP,
                "msg",          "%s", "yev_start_event NEW STATE",
                "msg2",         "%s", "💥💥⏩ yev_start_event NEW STATE",
                "type",         "%s", yev_event_type_name(yev_event),
                "yev_state",    "%s", yev_get_state_name(yev_event),
                "loop_running", "%d", yev_loop->running?1:0,
                "p",            "%p", yev_event,
                "fd",           "%d", yev_get_fd(yev_event),
                "flag",         "%j", jn_flags,
                NULL
            );
            json_decref(jn_flags);
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_start_timer_event(
    yev_event_h yev_event_,
    time_t timeout_ms,
    BOOL periodic
) {
    yev_event_t *yev_event = (yev_event_t *)yev_event_;
    /*------------------------*
     *  Check parameters
     *------------------------*/
    if(!yev_event) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING_ERROR,
            "msg",          "%s", "yev_event NULL",
            NULL
        );
        return -1;
    }
    yev_loop_t *yev_loop = yev_event->yev_loop;

    hgobj gobj = yev_loop->yuno?yev_event->gobj:0;

    if(yev_event->fd < 0) {
        yev_event->fd = timerfd_create(CLOCK_BOOTTIME, TFD_NONBLOCK|TFD_CLOEXEC);
        if(yev_event->fd < 0) {
            gobj_log_critical(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "timerfd_create() FAILED, cannot run yunetas",
                NULL
            );
            return -1;
        }
    }

    if(periodic) {
        yev_event->flag |= YEV_FLAG_TIMER_PERIODIC;
    } else {
        yev_event->flag &= ~YEV_FLAG_TIMER_PERIODIC;
    }

    if(timeout_ms <= 0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING_ERROR,
            "msg",          "%s", "Cannot start timer event: time negative",
            "event_type",   "%s", yev_event_type_name(yev_event),
            "p",            "%p", yev_event,
            NULL
        );
        return -1;
    }

    struct timeval timeout = {
        .tv_sec  = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000,
    };
    struct itimerspec delta = {
        .it_interval.tv_sec = periodic? timeout.tv_sec : 0,
        .it_interval.tv_nsec = periodic? timeout.tv_usec*1000 : 0,
        .it_value.tv_sec = timeout.tv_sec,
        .it_value.tv_nsec = timeout.tv_usec * 1000,

    };

    /*------------------------*
     *      Trace
     *------------------------*/
    uint32_t trace_level = gobj_global_trace_level();
    if(trace_level & TRACE_URING) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev_start_timer_event",
            "msg2",         "%s", "💥💥⏩ ⏰⏰ yev_start_timer_event",
            "type",         "%s", yev_event_type_name(yev_event),
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "timeout_ms",   "%d", (int)timeout_ms,
            "fd",           "%d", yev_get_fd(yev_event),
            "p",            "%p", yev_event,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);
    }

    /*---------------------------*
     *      Check state
     *---------------------------*/
    yev_state_t cur_state = yev_get_state(yev_event);
    switch (cur_state) {
        case YEV_ST_IDLE:
        case YEV_ST_STOPPED:
            break;

        case YEV_ST_RUNNING:
            timerfd_settime(yev_event->fd, 0, &delta, NULL);
            return 0;

        case YEV_ST_CANCELING:
            {
                json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_LIBURING_ERROR,
                    "msg",          "%s", "cannot start timer: is CANCELING",
                    "type",         "%s", yev_event_type_name(yev_event),
                    "yev_state",    "%s", yev_get_state_name(yev_event),
                    "timeout_ms",   "%d", (int)timeout_ms,
                    "fd",           "%d", yev_get_fd(yev_event),
                    "p",            "%p", yev_event,
                    "flag",         "%j", jn_flags,
                    NULL
                );
                json_decref(jn_flags);
            }
            return -1;
    }

    /*-------------------------------*
     *      Summit sqe
     *-------------------------------*/
    struct io_uring_sqe *sqe;
    timerfd_settime(yev_event->fd, 0, &delta, NULL);
    sqe = io_uring_get_sqe(&yev_loop->ring);
    io_uring_sqe_set_data(sqe, (char *)yev_event);
    io_uring_prep_read(sqe, yev_event->fd, &yev_event->timer_bf, sizeof(yev_event->timer_bf), 0);
    io_uring_submit(&yev_loop->ring);
    yev_set_state(yev_event, YEV_ST_RUNNING);

    if(cur_state != yev_get_state(yev_event)) {
        // State has changed
        if(trace_level & TRACE_URING) {
            json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
            gobj_log_debug(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_YEV_LOOP,
                "msg",          "%s", "yev_start_event NEW STATE",
                "msg2",         "%s", "💥💥⏩ yev_start_event NEW STATE",
                "type",         "%s", yev_event_type_name(yev_event),
                "yev_state",    "%s", yev_get_state_name(yev_event),
                "timeout_ms",   "%d", (int)timeout_ms,
                "p",            "%p", yev_event,
                "fd",           "%d", yev_get_fd(yev_event),
                "flag",         "%j", jn_flags,
                NULL
            );
            json_decref(jn_flags);
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_stop_event(yev_event_h yev_event_) // IDEMPOTENT close fd (timer,accept,connect)
{
    yev_event_t *yev_event = (yev_event_t *)yev_event_;

    /*------------------------*
     *  Check parameters
     *------------------------*/
    if(!yev_event) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING_ERROR,
            "msg",          "%s", "yev_event NULL",
            NULL
        );
        return -1;
    }

    yev_loop_t *yev_loop = yev_event->yev_loop;
    struct io_uring_sqe *sqe;
    hgobj gobj = yev_loop->yuno?yev_event->gobj:0;
    uint32_t trace_level = gobj_global_trace_level();

    /*------------------------*
     *      Trace
     *------------------------*/
    if(trace_level & TRACE_URING) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev_stop_event",
            "msg2",         "%s", (yev_type_t)yev_event->type == YEV_TIMER_TYPE?
                                    "💥🟥⏰⏰ yev_stop_event":
                                    "💥🟥 yev_stop_event",
            "type",         "%s", yev_event_type_name(yev_event),
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "fd",           "%d", yev_get_fd(yev_event),
            "p",            "%p", yev_event,
            "gbuffer",      "%p", yev_event->gbuf,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);
    }

    /*---------------------------*
     *      Free
     *---------------------------*/
    GBUFFER_DECREF(yev_event->gbuf)

    /*-------------------------------*
     *      stopping
     *-------------------------------*/
    switch((yev_type_t)yev_event->type) {
        case YEV_READ_TYPE:
        case YEV_WRITE_TYPE:
        case YEV_RECVMSG_TYPE:
        case YEV_SENDMSG_TYPE:
        case YEV_ACCEPT_TYPE:
        case YEV_POLL_TYPE:
            break;
        case YEV_CONNECT_TYPE:
        case YEV_TIMER_TYPE:
            // Each connection needs a new socket fd, i.e., after each disconnection.
            // The timer (once) if it's in idle can be reused, if stopped you must create one new.
            if(yev_event->fd > 0) {
                if(trace_level & (TRACE_URING)) {
                    gobj_log_debug(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_YEV_LOOP,
                        "msg",          "%s", "close socket",
                        "msg2",         "%s", "💥🟥 close socket",
                        "fd",           "%d", yev_event->fd ,
                        "p",            "%p", yev_event,
                        NULL
                    );
                }
                close(yev_event->fd);
                yev_event->fd = -1;
            }
            break;
    }

    /*-------------------------------*
     *      Checking state
     *-------------------------------*/
    yev_state_t cur_state = yev_get_state(yev_event);
    switch(cur_state) {
        case YEV_ST_RUNNING:
            sqe = io_uring_get_sqe(&yev_loop->ring);
            io_uring_sqe_set_data(sqe, yev_event);
            io_uring_prep_cancel(sqe, yev_event, 0);
            io_uring_submit(&yev_loop->ring);
            yev_set_state(yev_event, YEV_ST_CANCELING);
            break;

        case YEV_ST_IDLE:
            yev_set_state(yev_event, YEV_ST_STOPPED);
            if(yev_event->type == YEV_CONNECT_TYPE) {
                yev_set_flag(yev_event, YEV_FLAG_CONNECTED, false);
            }

            if(yev_event->type == YEV_TIMER_TYPE) {
                yev_event->result = -ECANCELED; // For timer In idle state simulate canceled error
                if (yev_event->callback) {
                    int ret = yev_event->callback(
                        yev_event
                    );
                    if(ret < 0) {
                        yev_loop->running = 0;
                    }
                }
            }
            break;

        case YEV_ST_CANCELING:
        case YEV_ST_STOPPED:
            // "yev_event already stopped" Silence please
            return -1;
    }

    if(cur_state != yev_get_state(yev_event)) {
        // State has changed
        if(trace_level & TRACE_URING) {
            json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
            gobj_log_debug(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_YEV_LOOP,
                "msg",          "%s", "yev_stop_event NEW STATE",
                "msg2",         "%s", "💥🟥 yev_stop_event NEW STATE",
                "type",         "%s", yev_event_type_name(yev_event),
                "yev_state",    "%s", yev_get_state_name(yev_event),
                "loop_running", "%d", yev_loop->running?1:0,
                "p",            "%p", yev_event,
                "fd",           "%d", yev_get_fd(yev_event),
                "flag",         "%j", jn_flags,
                NULL
            );
            json_decref(jn_flags);
        }
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void yev_destroy_event(yev_event_h yev_event_)
{
    yev_event_t *yev_event = (yev_event_t *)yev_event_;

    /*------------------------*
     *  Check parameters
     *------------------------*/
    if(!yev_event) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING_ERROR,
            "msg",          "%s", "yev_event NULL",
            NULL
        );
        return;
    }
    yev_loop_t *yev_loop = yev_event->yev_loop;
    hgobj gobj = yev_loop->yuno?yev_event->gobj:0;

    /*------------------------*
     *      Trace
     *------------------------*/
    if(gobj_trace_level(0) & TRACE_URING) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev_destroy_event",
            "msg2",         "%s", "💥🟥🟥 yev_destroy_event",
            "type",         "%s", yev_event_type_name(yev_event),
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "fd",           "%d", yev_get_fd(yev_event),
            "p",            "%p", yev_event,
            "gbuffer",      "%p", yev_event->gbuf,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);
    }

    /*---------------------------*
     *  Check if loop exiting
     *      Check state
     *---------------------------*/
    yev_state_t yev_state = yev_get_state(yev_event);
    if(yev_state == YEV_ST_RUNNING) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING_ERROR,
            "msg",          "%s", "Destroying a running event, stop it",
            "type",         "%s", yev_event_type_name(yev_event),
            "fd",           "%d", yev_get_fd(yev_event),
            "p",            "%p", yev_event,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);

        if(yev_loop->stopping) {
            // Don't call callback if stopping loop
            yev_event->callback = NULL;
        }
        yev_stop_event(yev_event);
    }

    /*---------------------------*
     *      Free
     *---------------------------*/
    GBUFFER_DECREF(yev_event->gbuf)
    GBMEM_FREE(yev_event->sock_info)
    GBMEM_FREE(yev_event->msghdr)

    /*-------------------------------*
     *      destroying
     *-------------------------------*/
    switch((yev_type_t)yev_event->type) {
        case YEV_READ_TYPE:
        case YEV_WRITE_TYPE:
        case YEV_RECVMSG_TYPE:
        case YEV_SENDMSG_TYPE:
        case YEV_POLL_TYPE:
            break;
        case YEV_CONNECT_TYPE:  // it must not happen
        case YEV_ACCEPT_TYPE:   // it must not happen
        case YEV_TIMER_TYPE:
            if(yev_event->fd > 0) {
                if(gobj_trace_level(0) & (TRACE_URING)) {
                    gobj_log_debug(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_YEV_LOOP,
                        "msg",          "%s", "close socket",
                        "msg2",         "%s", "💥🟥 close socket",
                        "fd",           "%d", yev_event->fd ,
                        "p",            "%p", yev_event,
                        NULL
                    );
                }
                close(yev_event->fd);
                yev_event->fd = -1;
            }
            break;
    }

    GBMEM_FREE(yev_event)
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE yev_event_t *create_event(
    yev_loop_t *yev_loop,
    yev_callback_t callback,
    hgobj gobj,
    int fd
) {
    yev_event_t *yev_event = GBMEM_MALLOC(sizeof(yev_event_t));
    if(!yev_event) {
        gobj_log_critical(yev_loop->yuno?gobj:0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "No memory for yev event",    // use the same string
            NULL
        );
        return NULL;
    }

    yev_event->yev_loop = yev_loop;
    yev_event->gobj = gobj;
    yev_event->callback = callback? callback:yev_loop->callback;
    yev_event->fd = fd;

    return yev_event;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC yev_event_h yev_create_timer_event(
    yev_loop_h yev_loop_,
    yev_callback_t callback,
    hgobj gobj
) {
    yev_loop_t *yev_loop = (yev_loop_t *)yev_loop_;

    yev_event_t *yev_event = create_event(yev_loop, callback, gobj, -1);
    if(!yev_event) {
        // Error already logged
        return NULL;
    }

    yev_event->type = YEV_TIMER_TYPE;

    if(gobj_trace_level(yev_loop->yuno?gobj:0) & (TRACE_URING)) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
        gobj_log_debug(yev_loop->yuno?gobj:0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev_create_timer_event",
            "msg2",         "%s", "💥🟦 ⏰⏰ yev_create_timer_event",
            "type",         "%s", yev_event_type_name(yev_event),
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "fd",           "%d", yev_get_fd(yev_event),
            "p",            "%p", yev_event,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);
    }

    return yev_event;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC yev_event_h yev_create_connect_event( // create the socket to connect in yev_event->fd
    yev_loop_h yev_loop_,
    yev_callback_t callback, // if return -1 the loop in yev_loop_run will break;
    const char *dst_url,
    const char *src_url,    /* local bind, only host:port */
    int ai_family,          /* default: AF_UNSPEC, Allow IPv4 or IPv6  (AF_INET AF_INET6) */
    int ai_flags,           /* default: AI_V4MAPPED | AI_ADDRCONFIG */
    hgobj gobj
) {
    yev_loop_t *yev_loop = (yev_loop_t *)yev_loop_;
    uint32_t trace_level = gobj_trace_level(yev_loop->yuno?gobj:0);

    yev_event_t *yev_event = create_event(yev_loop, callback, gobj, -1);
    if(!yev_event) {
        // Error already logged
        return NULL;
    }

    yev_event->type = YEV_CONNECT_TYPE;
    yev_event->sock_info = GBMEM_MALLOC(sizeof(sock_info_t ));

    if(trace_level & (TRACE_URING)) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev_create_connect_event",
            "msg2",         "%s", "💥🟦 yev_create_connect_event",
            "type",         "%s", yev_event_type_name(yev_event),
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "fd",           "%d", yev_get_fd(yev_event),
            "p",            "%p", yev_event,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);
    }

    yev_rearm_connect_event(yev_event,
        dst_url,
        src_url,
        ai_family,
        ai_flags
    );

    return yev_event;
}

/***************************************************************************
 *  create the socket to connect in yev_event->fd
 *  If fd already set, let it and return
 *  To recreate fd previously close it and set -1
 ***************************************************************************/
PUBLIC int yev_rearm_connect_event( // create the socket to connect in yev_event->fd
                                    // If fd already set, let it and return
                                    // To recreate fd, previously close it and set -1
    yev_event_h yev_event_,
    const char *dst_url,
    const char *src_url,    /* local bind, only host:port */
    int ai_family,          /* default: AF_UNSPEC, Allow IPv4 or IPv6  (AF_INET AF_INET6) */
    int ai_flags            /* default: AI_V4MAPPED | AI_ADDRCONFIG */
) {
    yev_event_t *yev_event = (yev_event_t *)yev_event_;
    if(!yev_event) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING_ERROR,
            "msg",          "%s", "yev_event NULL",
            NULL
        );
        return -1;
    }

    if(yev_event->fd > 0) {
        // Already set
        return yev_event->fd;
    }

    if(!ai_family) {
        ai_family = AF_UNSPEC;
    }
    if(!ai_flags) {
        ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;
    }

    yev_loop_t *yev_loop = yev_event->yev_loop;
    hgobj gobj = yev_loop->yuno?yev_event->gobj:0;
    uint32_t trace_level = gobj_global_trace_level();

    char schema[16];
    char dst_host[120];
    char dst_port[10];
    char saddr[80];

    int ret = parse_url(
        gobj,
        dst_url,
        schema, sizeof(schema),
        dst_host, sizeof(dst_host),
        dst_port, sizeof(dst_port),
        0, 0,
        0, 0,
        false
    );
    if(ret < 0) {
        // Error already logged
        return -1;
    }

    struct addrinfo hints = {
        .ai_family = ai_family,
        .ai_flags = ai_flags,
    };

    int secure;
    yev_protocol_fill_hints_fn(
        schema,
        &hints,
        &secure
    );
    if(secure) {
        yev_event->flag |= YEV_FLAG_USE_TLS;
    } else {
        yev_event->flag &= ~YEV_FLAG_USE_TLS;
    }

    struct addrinfo *results;
    struct addrinfo *rp;
    ret = getaddrinfo(
        dst_host,
        dst_port,
        &hints,
        &results
    );
    if(ret != 0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING_ERROR,
            "msg",          "%s", "getaddrinfo() FAILED",
            "url",          "%s", dst_url,
            "host",         "%s", dst_host,
            "port",         "%s", dst_port,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    int fd = -1;
    for (rp = results; rp; rp = rp->ai_next) {
        print_addrinfo(gobj, saddr, sizeof(saddr), rp, atoi(dst_port));
        if(gobj_is_level_tracing(0, TRACE_URING)) {
            gobj_log_debug(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_YEV_LOOP,
                "msg",          "%s", "addrinfo found, to connect",
                "addrinfo",     "%s", saddr,
                "ai_flags",     "%d", rp->ai_flags,
                "ai_family",    "%d", rp->ai_family,
                "ai_socktype",  "%d", rp->ai_socktype,
                "ai_protocol",  "%d", rp->ai_protocol,
                "ai_canonname", "%s", rp->ai_canonname,
                NULL
            );
        }
        fd = socket(
            rp->ai_family,
            rp->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC,
            rp->ai_protocol
        );

        if (fd == -1) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING_ERROR,
                "msg",          "%s", "socket() FAILED",
                "url",          "%s", dst_url,
                "addrinfo",     "%s", saddr,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            continue;
        }

        /*--------------------------------------*
         *  Option to bind to local host/port
         *--------------------------------------*/
        if(!empty_string(src_url)) {
            char src_host[120] = {0};
            char src_port[10] = {0};
            if(!empty_string(src_host)) {
                ret = parse_url(
                    gobj,
                    src_url,
                    0, 0,
                    src_host, sizeof(src_host),
                    src_port, sizeof(src_port),
                    0, 0,
                    0, 0,
                    true
                );
                if(ret < 0) {
                    close(fd);
                    // Error already logged
                    return -1;
                }
            }

            struct addrinfo *res;
            ret = getaddrinfo(
                src_host,
                src_port,
                &hints,
                &res
            );
            if(ret != 0) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_LIBURING_ERROR,
                    "msg",          "%s", "getaddrinfo() src_url FAILED",
                    "url",          "%s", src_url,
                    "host",         "%s", src_host,
                    "port",         "%s", src_port,
                    "errno",        "%d", errno,
                    "strerror",     "%s", strerror(errno),
                    NULL
                );
                close(fd);
                return -1;
            }

            ret = bind(fd, res->ai_addr, (socklen_t) res->ai_addrlen);
            if (ret == -1) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_LIBURING_ERROR,
                    "msg",          "%s", "bind() FAILED",
                    "url",          "%s", src_url,
                    "addrinfo",     "%s", saddr,
                    "errno",        "%d", errno,
                    "strerror",     "%s", strerror(errno),
                    NULL
                );
            }
            freeaddrinfo(res);
            if(ret == -1) {
                close(fd);
                return -1;
            }
        }

        if(trace_level & TRACE_URING) {
            gobj_log_debug(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "addrinfo to connect",
                "url",          "%s", dst_url,
                "addrinfo",     "%s", saddr,
                "fd",           "%d", fd,
                "p",            "%p", yev_event,
                NULL
            );
        }

        ret = 0;    // Got a addr
        break;
    }

    if (!rp || fd == -1) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING_ERROR,
            "msg",          "%s", "Cannot get addr to connect",
            "url",          "%s", dst_url,
            "host",         "%s", dst_host,
            "port",         "%s", dst_port,
            NULL
        );
        ret = -1;
    }

    if(ret == 0) {
        if(rp && rp->ai_addrlen <= sizeof(yev_event->sock_info->addr)) {
            memcpy(&yev_event->sock_info->addr, rp->ai_addr, rp->ai_addrlen);
            yev_event->sock_info->addrlen = (socklen_t) rp->ai_addrlen;
        } else {
            close(fd);
            fd = -1;
            ret = -1;
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING_ERROR,
                "msg",          "%s", "What merde?",
                "url",          "%s", dst_url,
                "host",         "%s", dst_host,
                "port",         "%s", dst_port,
                NULL
            );
        }

    }

    freeaddrinfo(results);

    if(ret == -1) {
        return ret;
    }

    // set_nonblocking(fd); // Already set in socket()
    // set_cloexec(fd);

    if (is_tcp_socket(fd)) {
        set_tcp_socket_options(fd, yev_loop->keep_alive);
    }

    yev_event->fd = fd;

    if(trace_level & (TRACE_URING)) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev_rearm_connect_event",
            "msg2",         "%s", "💥🟦🟦 yev_rearm_connect_event",
            "type",         "%s", yev_event_type_name(yev_event),
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "fd",           "%d", fd,
            "p",            "%p", yev_event,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);
    }

    return fd;
}

/***************************************************************************
 *  Read the value of net.core.somaxconn
 *  Returns: the value on success, or -1 on error.
 ***************************************************************************/
#ifdef __linux__
PRIVATE int get_net_core_somaxconn(void)
{
    const char *path = "/proc/sys/net/core/somaxconn";
    FILE *fp = fopen(path, "r");
    if(!fp) {
        return -1;
    }

    int value;
    if(fscanf(fp, "%d", &value) != 1) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return value;
}
#endif

/***************************************************************************
 *  backlog /proc/sys/net/core/somaxconn, since Linux 5.4 is 4096
 ***************************************************************************/
PUBLIC yev_event_h yev_create_accept_event( // create the socket listening in yev_event->fd
    yev_loop_h yev_loop_,
    yev_callback_t callback, // if return -1 the loop in yev_loop_run will break;
    const char *listen_url,
    int backlog,            /* queue of pending connections for socket listening */
    BOOL shared,            /* open socket as shared */
    int ai_family,          /* default: AF_UNSPEC, Allow IPv4 or IPv6  (AF_INET AF_INET6) */
    int ai_flags,           /* default: AI_V4MAPPED | AI_ADDRCONFIG */
    hgobj gobj
) {
    yev_loop_t *yev_loop = (yev_loop_t *)yev_loop_;
    uint32_t trace_level = gobj_trace_level(yev_loop->yuno?gobj:0);

    if(!ai_family) {
        ai_family = AF_UNSPEC;
    }
    if(!ai_flags) {
        ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;
    }

    char schema[40];
    char host[120];
    char port[40];
    char saddr[80];

    int ret = parse_url(
        gobj,
        listen_url,
        schema, sizeof(schema),
        host, sizeof(host),
        port, sizeof(port),
        0, 0,
        0, 0,
        false
    );
    if(ret < 0) {
        // Error already logged
        return NULL;
    }

#ifdef __linux__
    int somaxconn = get_net_core_somaxconn();
    if(somaxconn < backlog) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "net.core.somaxconn TOO SMALL, increase it in the s.o.Change dynamically with 'sysctl -w net.core.somaxconn=?'. Change persistent with a file in /etc/sysctl.d/. Consult with 'cat /proc/sys/net/core/somaxconn'",
            "somaxconn",    "%d", somaxconn,
            "backlog",      "%d", backlog,
            NULL
        );
    }
#endif

    struct addrinfo hints = {
        .ai_family = ai_family,
        .ai_flags = ai_flags,
    };

    int secure;
    yev_protocol_fill_hints_fn(
        schema,
        &hints,
        &secure
    );

    struct addrinfo *results;
    struct addrinfo *rp;
    ret = getaddrinfo(
        host,
        port,
        &hints,
        &results
    );
    if(ret != 0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING_ERROR,
            "msg",          "%s", "getaddrinfo() FAILED",
            "url",          "%s", listen_url,
            "host",         "%s", host,
            "port",         "%s", port,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return NULL;
    }

    int fd = -1;
    for (rp = results; rp; rp = rp->ai_next) {
        print_addrinfo(gobj, saddr, sizeof(saddr), rp, atoi(port));
        if(trace_level & (TRACE_URING)) {
            gobj_log_debug(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_YEV_LOOP,
                "msg",          "%s", "addrinfo found, to listen",
                "addrinfo",     "%s", saddr,
                "ai_flags",     "%d", rp->ai_flags,
                "ai_family",    "%d", rp->ai_family,
                "ai_socktype",  "%d", rp->ai_socktype,
                "ai_protocol",  "%d", rp->ai_protocol,
                "ai_canonname", "%s", rp->ai_canonname,
                "ai_addrlen",   "%d", (int)rp->ai_addrlen,
                NULL
            );
        }

        fd = socket(
            rp->ai_family,
            rp->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC,
            rp->ai_protocol
        );

        if (fd == -1) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING_ERROR,
                "msg",          "%s", "socket() FAILED",
                "url",          "%s", listen_url,
                "addrinfo",     "%s", saddr,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            continue;
        }

        if(hints.ai_protocol == IPPROTO_TCP || hints.ai_protocol == IPPROTO_UDP) {
            // TODO review for UDP
            int on = 1;
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
            if(shared) {
                setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
            }
        }

        // set_nonblocking(fd); // Already set in socket()
        // set_cloexec(fd);

        ret = bind(fd, rp->ai_addr, (socklen_t) rp->ai_addrlen);
        if (ret == -1) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING_ERROR,
                "msg",          "%s", "bind() FAILED",
                "url",          "%s", listen_url,
                "addrinfo",     "%s", saddr,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            close(fd);
            break;
        }

        if(hints.ai_protocol == IPPROTO_TCP) {
            ret = listen(fd, backlog);
            if(ret == -1) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_LIBURING_ERROR,
                    "msg",          "%s", "listen() FAILED",
                    "url",          "%s", listen_url,
                    "addrinfo",     "%s", saddr,
                    "errno",        "%d", errno,
                    "strerror",     "%s", strerror(errno),
                    NULL
                );
                close(fd);
                break;
            }
        }

        if(trace_level & (TRACE_URING)) {
            gobj_log_debug(gobj, 0,
               "function",     "%s", __FUNCTION__,
               "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
               "msg",          "%s", "addrinfo on listen",
               "msg2",          "%s", "addrinfo on listen 🟦🟦🦻🦻🦻",
               "url",          "%s", listen_url,
               "addrinfo",     "%s", saddr,
               "fd",           "%d", fd,
               NULL
           );
        }

        ret = 0;    // Got a addr
        break;
    }

    if (!rp || fd == -1) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING_ERROR,
            "msg",          "%s", "Cannot get addr to listen",
            "url",          "%s", listen_url,
            "host",         "%s", host,
            "port",         "%s", port,
            NULL
        );
        close(fd);
        fd = -1;
        ret = -1;
    }

    if(ret == -1) {
        freeaddrinfo(results);
        return NULL;
    }

    // set_nonblocking(fd); // Already set in socket()
    // set_cloexec(fd);

    yev_event_t *yev_event = create_event(yev_loop, callback, gobj, -1);
    if(!yev_event) {
        // Error already logged
        freeaddrinfo(results);
        return NULL;
    }

    yev_event->type = YEV_ACCEPT_TYPE;
    yev_event->sock_info = GBMEM_MALLOC(sizeof(sock_info_t ));
    yev_event->fd = fd;

    if(rp && rp->ai_addrlen <= sizeof(yev_event->sock_info->addr)) {
        memcpy(&yev_event->sock_info->addr, rp->ai_addr, rp->ai_addrlen);
        yev_event->sock_info->addrlen = (socklen_t) rp->ai_addrlen;
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING_ERROR,
            "msg",          "%s", "Cannot copy ai_addr to sock_info",
            "url",          "%s", listen_url,
            "host",         "%s", host,
            "port",         "%s", port,
            "rp found",     "%d", rp?1:0,
            "ai_addrlen",   "%d", (int)rp->ai_addrlen,
            "addr size",    "%d", (int)sizeof(yev_event->sock_info->addr),
            NULL
        );
    }

    if(secure) {
        yev_event->flag |= YEV_FLAG_USE_TLS;
    } else {
        yev_event->flag &= ~YEV_FLAG_USE_TLS;
    }

    if(trace_level & (TRACE_URING)) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
        gobj_log_debug(yev_loop->yuno?gobj:0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev_create_accept_event",
            "msg2",         "%s", "💥🟦 yev_create_accept_event",
            "type",         "%s", yev_event_type_name(yev_event),
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "fd",           "%d", yev_get_fd(yev_event),
            "p",            "%p", yev_event,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);
    }
    freeaddrinfo(results);

    return yev_event;
}

/***************************************************************************
 *  Create a duplicate of accept events using the socket of
 *  yev_server_accept (created with yev_create_accept_event()).
 *  It's managed in callback of the same yev_create_accept_event().
 *  It needs 'child_tree_filter'.
 ***************************************************************************/
PUBLIC yev_event_h yev_dup_accept_event(
    yev_event_h yev_server_accept,
    int dup_idx,
    hgobj gobj
) {
    yev_event_t *yev_event_accept = (yev_event_t *)yev_server_accept;
    yev_loop_t *yev_loop = yev_event_accept->yev_loop;

    uint32_t trace_level = gobj_trace_level(yev_loop->yuno?gobj:0);

    yev_event_t *yev_event = create_event(
        yev_loop,
        yev_event_accept->callback,
        gobj,
        yev_event_accept->fd
    );
    if(!yev_event) {
        // Error already logged
        return NULL;
    }

    yev_event->type = YEV_ACCEPT_TYPE;
    yev_event->flag = YEV_FLAG_ACCEPT_DUP;
    yev_event->sock_info = GBMEM_MALLOC(sizeof(sock_info_t ));
    memcpy(
        &yev_event->sock_info->addr,
        &yev_event_accept->sock_info->addr,
        yev_event_accept->sock_info->addrlen
    );
    yev_event->sock_info->addrlen = yev_event_accept->sock_info->addrlen;
    yev_event->dup_idx = dup_idx;

    if(trace_level & (TRACE_URING)) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
        gobj_log_debug(yev_loop->yuno?gobj:0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev_dup_accept_event",
            "msg2",         "%s", "💥🟦 yev_dup_accept_event",
            "type",         "%s", yev_event_type_name(yev_event),
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "fd",           "%d", yev_get_fd(yev_event),
            "p",            "%p", yev_event,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);
    }

    return yev_event;
}

/***************************************************************************
 *  Create a duplicate of accept events using the listen socket
 *  (created with yev_create_accept_event()),
 *  but managed in another callback of another child (usually C_TCP) gobj
 ***************************************************************************/
PUBLIC yev_event_h yev_dup2_accept_event(
    yev_loop_h yev_loop_,
    yev_callback_t callback, // if return -1 the loop in yev_loop_run will break;
    int fd_listen,
    hgobj gobj
) {
    yev_loop_t *yev_loop = (yev_loop_t *)yev_loop_;

    uint32_t trace_level = gobj_trace_level(yev_loop->yuno?gobj:0);

    yev_event_t *yev_event = create_event(
        yev_loop,
        callback,
        gobj,
        fd_listen
    );
    if(!yev_event) {
        // Error already logged
        return NULL;
    }

    yev_event->type = YEV_ACCEPT_TYPE;
    yev_event->flag = YEV_FLAG_ACCEPT_DUP2;
    yev_event->sock_info = GBMEM_MALLOC(sizeof(sock_info_t ));
    memset(
        &yev_event->sock_info->addr,
        0,
        sizeof(yev_event->sock_info->addr)
    );
    yev_event->sock_info->addrlen = sizeof(yev_event->sock_info->addr);

    if(trace_level & (TRACE_URING)) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
        gobj_log_debug(yev_loop->yuno?gobj:0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev_dup2_accept_event",
            "msg2",         "%s", "💥🟦 yev_dup2_accept_event",
            "type",         "%s", yev_event_type_name(yev_event),
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "fd",           "%d", yev_get_fd(yev_event),
            "p",            "%p", yev_event,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);
    }

    return yev_event;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC yev_event_h yev_create_poll_event( // create a poll event of yev_event->fd
    yev_loop_h yev_loop_,
    yev_callback_t callback, // if return -1 the loop in yev_loop_run will break;
    hgobj gobj,
    int fd,
    unsigned poll_mask
) {

    yev_loop_t *yev_loop = (yev_loop_t *)yev_loop_;
    yev_event_t *yev_event = create_event(yev_loop, callback, gobj, fd);
    if(!yev_event) {
        // Error already logged
        return NULL;
    }

    yev_event->type = YEV_POLL_TYPE;
    yev_event->poll_mask = poll_mask;

    if(gobj_trace_level(yev_loop->yuno?gobj:0) & (TRACE_URING)) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
        gobj_log_debug(yev_loop->yuno?gobj:0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev_create_poll_event",
            "msg2",         "%s", "💥🟦 yev_create_poll_event",
            "type",         "%s", yev_event_type_name(yev_event),
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "fd",           "%d", fd,
            "poll_mask",    "%d", poll_mask,
            "p",            "%p", yev_event,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);
    }

    return yev_event;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC yev_event_h yev_create_read_event(
    yev_loop_h yev_loop_,
    yev_callback_t callback,
    hgobj gobj,
    int fd,
    gbuffer_t *gbuf
) {
    yev_loop_t *yev_loop = (yev_loop_t *)yev_loop_;
    yev_event_t *yev_event = create_event(yev_loop, callback, gobj, fd);
    if(!yev_event) {
        // Error already logged
        return NULL;
    }

    yev_event->type = YEV_READ_TYPE;
    yev_event->gbuf = gbuf;

    if(gobj_trace_level(yev_loop->yuno?gobj:0) & (TRACE_URING)) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
        gobj_log_debug(yev_loop->yuno?gobj:0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev_create_read_event",
            "msg2",         "%s", "💥🟦 yev_create_read_event",
            "type",         "%s", yev_event_type_name(yev_event),
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "fd",           "%d", fd,
            "p",            "%p", yev_event,
            "gbuffer",      "%p", gbuf,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);
    }

    return yev_event;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC yev_event_h yev_create_write_event(
    yev_loop_h yev_loop_,
    yev_callback_t callback,
    hgobj gobj,
    int fd,
    gbuffer_t *gbuf
) {
    yev_loop_t *yev_loop = (yev_loop_t *)yev_loop_;
    yev_event_t *yev_event = create_event(yev_loop, callback, gobj, fd);
    if(!yev_event) {
        // Error already logged
        return NULL;
    }

    yev_event->type = YEV_WRITE_TYPE;
    yev_event->gbuf = gbuf;

    if(gobj_trace_level(yev_loop->yuno?gobj:0) & (TRACE_URING)) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
        gobj_log_debug(yev_loop->yuno?gobj:0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev_create_write_event",
            "msg2",         "%s", "💥🟦 yev_create_write_event",
            "type",         "%s", yev_event_type_name(yev_event),
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "fd",           "%d", fd,
            "p",            "%p", yev_event,
            "gbuffer",      "%p", gbuf,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);
    }

    return yev_event;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC yev_event_h yev_create_recvmsg_event(
    yev_loop_h yev_loop_,
    yev_callback_t callback,
    hgobj gobj,
    int fd,
    gbuffer_t *gbuf
) {
    yev_loop_t *yev_loop = (yev_loop_t *)yev_loop_;
    yev_event_t *yev_event = create_event(yev_loop, callback, gobj, fd);
    if(!yev_event) {
        // Error already logged
        return NULL;
    }

    yev_event->type = YEV_RECVMSG_TYPE;
    yev_event->msghdr = GBMEM_MALLOC(sizeof(struct msghdr));
    yev_event->sock_info = GBMEM_MALLOC(sizeof(sock_info_t));

    yev_event->msghdr->msg_iov = &yev_event->iov;
    yev_event->msghdr->msg_iovlen = 1;
    yev_event->gbuf = gbuf;

    yev_event->msghdr->msg_name = &yev_event->sock_info->addr;
    yev_event->msghdr->msg_namelen = sizeof(yev_event->sock_info->addr);

    if(gobj_trace_level(yev_loop->yuno?gobj:0) & (TRACE_URING)) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
        gobj_log_debug(yev_loop->yuno?gobj:0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev_create_recvmsg_event",
            "msg2",         "%s", "💥🟦 yev_create_recvmsg_event",
            "type",         "%s", yev_event_type_name(yev_event),
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "fd",           "%d", fd,
            "p",            "%p", yev_event,
            "gbuffer",      "%p", gbuf,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);
    }

    return yev_event;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC yev_event_h yev_create_sendmsg_event(
    yev_loop_h yev_loop_,
    yev_callback_t callback,
    hgobj gobj,
    int fd,
    gbuffer_t *gbuf,
    struct sockaddr *dst_addr
) {
    yev_loop_t *yev_loop = (yev_loop_t *)yev_loop_;
    yev_event_t *yev_event = create_event(yev_loop, callback, gobj, fd);
    if(!yev_event) {
        // Error already logged
        return NULL;
    }

    yev_event->type = YEV_SENDMSG_TYPE;

    yev_event->msghdr = GBMEM_MALLOC(sizeof(struct msghdr));

    yev_event->msghdr->msg_iov = &yev_event->iov;
    yev_event->msghdr->msg_iovlen = 1;
    yev_event->gbuf = gbuf;

    yev_event->msghdr->msg_name = dst_addr;
    yev_event->msghdr->msg_namelen = sizeof(struct sockaddr);

    if(gobj_trace_level(yev_loop->yuno?gobj:0) & (TRACE_URING)) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
        gobj_log_debug(yev_loop->yuno?gobj:0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev_create_sendmsg_event",
            "msg2",         "%s", "💥🟦 yev_create_sendmsg_event",
            "type",         "%s", yev_event_type_name(yev_event),
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "fd",           "%d", fd,
            "p",            "%p", yev_event,
            "gbuffer",      "%p", gbuf,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);
    }

    return yev_event;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int print_addrinfo(hgobj gobj, char *bf, size_t bfsize, struct addrinfo *ai, int port)
{
    void *addr;

    if(bfsize < INET6_ADDRSTRLEN + 20) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING_ERROR,
            "msg",          "%s", "buffer to small",
            NULL
        );
        return -1;
    }
    if (ai->ai_family == AF_INET6) {
        addr = &((struct sockaddr_in6 *) ai->ai_addr)->sin6_addr;
    } else {
        addr = &((struct sockaddr_in *) ai->ai_addr)->sin_addr;
    }

    inet_ntop(ai->ai_family, addr, bf, bfsize);
    size_t pos = strlen(bf);
    if(pos < bfsize) {
        snprintf(bf + pos, bfsize - pos, ":%d", port);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC const char *yev_event_type_name(yev_event_h yev_event)
{
    switch(yev_event->type) {
        case YEV_READ_TYPE:
            return "YEV_READ_TYPE";
        case YEV_WRITE_TYPE:
            return "YEV_WRITE_TYPE";
        case YEV_CONNECT_TYPE:
            return "YEV_CONNECT_TYPE";
        case YEV_ACCEPT_TYPE:
            return "YEV_ACCEPT_TYPE";
        case YEV_TIMER_TYPE:
            return "YEV_TIMER_TYPE";
        case YEV_POLL_TYPE:
            return "YEV_POLL_TYPE";
        case YEV_SENDMSG_TYPE:
            return "YEV_SENDMSG_TYPE";
        case YEV_RECVMSG_TYPE:
            return "YEV_RECVMSG_TYPE";
    }
    return "YEV_???_TYPE";
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC const char **yev_flag_strings(void)
{
    return yev_flag_s;
}
