/****************************************************************************
 *          yev_loop.c
 *
 *          Yuneta Event Loop
 *
 *          Copyright (c) 2024 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <liburing.h>
#include <sys/timerfd.h>
#include "yuneta_ev_loop.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_loop_create(hgobj yuno, unsigned entries, yev_loop_t **yev_loop_)
{
    struct io_uring ring_test;
    int err;

    *yev_loop_ = 0;     // error case

    if(entries == 0) {
        entries = DEFAULT_ENTRIES;
    }

    struct io_uring_params params_test = {0};
    // TODO use IORING_SETUP_SQPOLL when you know how to use
    // Info in https://unixism.net/loti/tutorial/sq_poll.html
    //params_test.flags |= IORING_SETUP_SQPOLL;
    //params_test.flags |= IORING_SETUP_COOP_TASKRUN;      // Available since 5.18
    //params_test.flags |= IORING_SETUP_SINGLE_ISSUER;     // Available since 6.0
retry:
    err = io_uring_queue_init_params(entries, &ring_test, &params_test);
    if(err) {
        if (err == -EINVAL && params_test.flags & IORING_SETUP_SINGLE_ISSUER) {
            params_test.flags &= ~IORING_SETUP_SINGLE_ISSUER;
            goto retry;
        }
        if (err == -EINVAL && params_test.flags & IORING_SETUP_COOP_TASKRUN) {
            params_test.flags &= ~IORING_SETUP_COOP_TASKRUN;
            goto retry;
        }

        gobj_log_critical(yuno, LOG_OPT_EXIT_ZERO,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Linux kernel without io_uring, cannot run yunetas",
            "errno",        "%d", -err,
            "serrno",       "%s", strerror(-err),
            NULL
        );
        return -1;
    }

    /* FAST_POOL is required for pre-posting receive buffers */
    if (!(params_test.features & IORING_FEAT_FAST_POLL)) {
        io_uring_queue_exit(&ring_test);
        gobj_log_critical(yuno, LOG_OPT_EXIT_ZERO,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Linux kernel without io_uring IORING_FEAT_FAST_POLL, cannot run yunetas",
            NULL
        );
        return -1;
    }

    // check if buffer selection is supported
    struct io_uring_probe *probe;
    probe = io_uring_get_probe_ring(&ring_test);
    if (!probe || !io_uring_opcode_supported(probe, IORING_OP_PROVIDE_BUFFERS)) {
        io_uring_free_probe(probe); // DIE here
        io_uring_queue_exit(&ring_test);
        gobj_log_critical(yuno, LOG_OPT_EXIT_ZERO,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Linux kernel without io_uring IORING_OP_PROVIDE_BUFFERS, cannot run yunetas",
            NULL
        );
        return -1;
    }
    io_uring_free_probe(probe);
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
        .flags = params_test.flags
    };

    err = io_uring_queue_init_params(entries, &yev_loop->ring, &params);
    if (err < 0) {
        GBMEM_FREE(yev_loop)
        gobj_log_critical(yuno, LOG_OPT_EXIT_ZERO,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Linux io_uring_queue_init_params() FAILED, cannot run yunetas",
            "errno",        "%d", -err,
            "serrno",       "%s", strerror(-err),
            NULL
        );
        return -1;
    }

    yev_loop->yuno = yuno;
    yev_loop->entries = entries;

    *yev_loop_ = yev_loop;

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void yev_loop_destroy(yev_loop_t *yev_loop)
{
    if(1) {
        struct io_uring_sqe *sqe;
        sqe = io_uring_get_sqe(&yev_loop->ring);
        io_uring_prep_cancel(sqe, 0, IORING_ASYNC_CANCEL_ANY);
        io_uring_submit(&yev_loop->ring);
    }

    io_uring_queue_exit(&yev_loop->ring);
    GBMEM_FREE(yev_loop)
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_loop_run(yev_loop_t *yev_loop)
{
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;

    /*------------------------------------------*
     *      Infinite loop
     *------------------------------------------*/
    while(gobj_is_running(yev_loop->yuno)) {
        int err = io_uring_wait_cqe(&yev_loop->ring, &cqe);
        if (err < 0) {
            if(err == -EINTR) {
                // Ctrl+C cause this
                continue;
            }
            gobj_log_error(yev_loop->yuno, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "io_uring_wait_cqe() FAILED",
                "errno",        "%d", -err,
                "serrno",       "%s", strerror(-err),
                NULL
            );
            break;
        }

        yev_event_t *yev_event = (yev_event_t *)io_uring_cqe_get_data(cqe);
        if(yev_event->flag & YEV_STOPPING_FLAG) {
            yev_event->flag &= ~YEV_STOPPING_FLAG;
            yev_event->flag |= YEV_STOPPED_FLAG;
        }

        switch((yev_type_t)yev_event->type) {
            case YEV_READ_TYPE:
                {
                    if(cqe->res < 0) {
                        gobj_log_error(yev_loop->yuno, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                            "msg",          "%s", "YEV_READ_TYPE failed",
                            "errno",        "%d", cqe->res,
                            "serrno",       "%s", strerror(cqe->res),
                            NULL
                        );
                    }
                    /*
                     *  Call Callback
                     */
                    gbuffer *gbuf = yev_event->bf.gbuf;
                    yev_event->bf.gbuf = 0;

                    gbuffer_set_wr(gbuf, cqe->res);     // Mark the written bytes of reading fd

                    if(yev_event->callback) {
                        yev_event->callback(
                            yev_event->gobj,
                            yev_event,
                            gbuf, // give out, callback responsible to free or reuse
                            (yev_event->flag & YEV_STOPPED_FLAG)?TRUE:FALSE
                        );
                    }
                }
                break;
            case YEV_WRITE_TYPE:
                {
                    if(cqe->res < 0) {
                        gobj_log_error(yev_loop->yuno, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                            "msg",          "%s", "YEV_WRITE_TYPE failed",
                            "errno",        "%d", cqe->res,
                            "serrno",       "%s", strerror(cqe->res),
                            NULL
                        );
                    }
                    /*
                     *  Call Callback
                     */
                    gbuffer *gbuf = yev_event->bf.gbuf;
                    yev_event->bf.gbuf = 0;

                    gbuffer_get(gbuf, cqe->res);    // Pop the read bytes used to write fd

                    if(yev_event->callback) {
                        yev_event->callback(
                            yev_event->gobj,
                            yev_event,
                            gbuf, // give out, callback responsible to free or reuse
                            (yev_event->flag & YEV_STOPPED_FLAG)?TRUE:FALSE
                        );
                    }
                }
                break;
            case YEV_CONNECT_TYPE:
                {
                    if(cqe->res < 0) {
                        gobj_log_error(yev_loop->yuno, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                            "msg",          "%s", "YEV_CONNECT_TYPE failed",
                            "errno",        "%d", cqe->res,
                            "serrno",       "%s", strerror(cqe->res),
                            NULL
                        );
                    }
                    if(yev_event->callback) {
                        yev_event->callback(
                            yev_event->gobj,
                            yev_event,
                            0,
                            (yev_event->flag & YEV_STOPPED_FLAG)?TRUE:FALSE
                        );
                    }
                }
                break;
            case YEV_ACCEPT_TYPE:
                {
                    if(cqe->res < 0) {
                        gobj_log_error(yev_loop->yuno, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                            "msg",          "%s", "YEV_ACCEPT_TYPE failed",
                            "errno",        "%d", cqe->res,
                            "serrno",       "%s", strerror(cqe->res),
                            NULL
                        );
                    }
                    int sock_conn_fd = cqe->res;
                    if(yev_event->callback) {
                        yev_event->callback(
                            yev_event->gobj,
                            yev_event,
                            &sock_conn_fd,
                            (yev_event->flag & YEV_STOPPED_FLAG)?TRUE:FALSE
                        );
                    }

                    /*
                     *  Rearm accept if no stopped
                     */
                    if(yev_event->flag & YEV_TIMER_PERIODIC_FLAG &&
                            !(yev_event->flag & (YEV_STOPPED_FLAG|YEV_STOPPING_FLAG))) {
                        sqe = io_uring_get_sqe(&yev_loop->ring);
                        yev_event->src_addrlen = sizeof(*yev_event->src_addr);
                        io_uring_prep_accept(
                            sqe,
                            yev_event->fd,
                            yev_event->src_addr,
                            &yev_event->src_addrlen,
                            0
                        );
                        io_uring_sqe_set_data(sqe, yev_event);
                        io_uring_submit(&yev_loop->ring);
                    }
                }
                break;
            case YEV_TIMER_TYPE:
                {
                    /*
                     *  Call Callback
                     */
                    if(yev_event->callback) {
                        yev_event->callback(
                            yev_event->gobj,
                            yev_event,
                            0,
                            (yev_event->flag & YEV_STOPPED_FLAG)?TRUE:FALSE
                        );
                    }

                    /*
                     *  Rearm periodic timer if no stopped
                     */
                    if(yev_event->flag & YEV_TIMER_PERIODIC_FLAG &&
                            !(yev_event->flag & (YEV_STOPPED_FLAG|YEV_STOPPING_FLAG))) {
                        sqe = io_uring_get_sqe(&yev_loop->ring);
                        io_uring_prep_read(
                            sqe,
                            yev_event->fd,
                            &yev_event->bf.timer_bf,
                            sizeof(yev_event->bf.timer_bf),
                            0
                        );
                        io_uring_sqe_set_data(sqe, yev_event);
                        io_uring_submit(&yev_loop->ring);
                    }
                }
                break;
        }

        /* Mark this request as processed */
        io_uring_cqe_seen(&yev_loop->ring, cqe);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_start_event(
    yev_event_t *yev_event_,
    gbuffer *gbuf_  // Used with yev_create_read_event(), yev_create_write_event()
) {
    yev_event_t *yev_event = yev_event_;
    hgobj gobj = yev_event->gobj;
    yev_loop_t *yev_loop = yev_event->yev_loop;

    if(yev_event->flag & YEV_STOPPING_FLAG) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "yev_event STOPPING",
            NULL
        );
        return -1;
    }

    /*-------------------------------*
     *      Re-start if stopped
     *-------------------------------*/
    if(yev_event->flag & YEV_STOPPED_FLAG) {
        yev_event->flag &= ~YEV_STOPPED_FLAG;
    }

    /*-------------------------------*
     *      Prepare data
     *-------------------------------*/
    switch((yev_type_t)yev_event->type) {
        case YEV_READ_TYPE:
        case YEV_WRITE_TYPE:
            {
                if(yev_event->bf.gbuf) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "yev_event ALREADY using gbuffer",
                        NULL
                    );
                    GBUFFER_DECREF(yev_event->bf.gbuf)
                }
                yev_event->bf.gbuf = gbuf_;
            }
            break;

        case YEV_CONNECT_TYPE:
            {
                if(!yev_event->dst_addr || yev_event->dst_addrlen <= 0) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "yev_event addr NULL",
                        NULL
                    );
                    return -1;
                }
            }
            break;
        case YEV_ACCEPT_TYPE:
            {
                if(!yev_event->src_addr || yev_event->src_addrlen <= 0) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "yev_event addr NULL",
                        NULL
                    );
                    return -1;
                }
            }
            break;

        case YEV_TIMER_TYPE:
            break;
    }

    /*-------------------------------*
     *      Summit sqe
     *-------------------------------*/
    switch((yev_type_t)yev_event->type) {
        case YEV_READ_TYPE:
            {
                if(!yev_event->bf.gbuf) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "gbuffer NULL",
                        NULL
                    );
                    break;
                };
                if(gbuffer_freebytes(yev_event->bf.gbuf)==0) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "gbuffer WITHOUT space to read",
                        NULL
                    );
                    break;
                }
                struct io_uring_sqe *sqe = io_uring_get_sqe(&yev_loop->ring);
                io_uring_prep_read(
                    sqe,
                    yev_event->fd,
                    gbuffer_cur_wr_pointer(yev_event->bf.gbuf),
                    gbuffer_freebytes(yev_event->bf.gbuf),
                    0
                );
                io_uring_sqe_set_data(sqe, yev_event);
                io_uring_submit(&yev_loop->ring);
            }
            break;
        case YEV_WRITE_TYPE:
            {
                if(!yev_event->bf.gbuf) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "gbuffer NULL",
                        NULL
                    );
                    break;
                };
                if(gbuffer_leftbytes(yev_event->bf.gbuf)==0) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "gbuffer WITHOUT data to write",
                        NULL
                    );
                    break;
                }

                struct io_uring_sqe *sqe = io_uring_get_sqe(&yev_loop->ring);
                io_uring_prep_write(
                    sqe,
                    yev_event->fd,
                    gbuffer_cur_rd_pointer(yev_event->bf.gbuf),
                    gbuffer_leftbytes(yev_event->bf.gbuf),
                    0
                );
                io_uring_sqe_set_data(sqe, yev_event);
                io_uring_submit(&yev_loop->ring);
            }
            break;
        case YEV_CONNECT_TYPE:
            {
                struct io_uring_sqe *sqe = io_uring_get_sqe(&yev_loop->ring);
                /*
                 *  Use the file descriptor fd to start connecting to the destination
                 *  described by the socket address at addr and of structure length addrlen.
                 */
                io_uring_prep_connect(
                    sqe,
                    yev_event->fd,
                    yev_event->dst_addr,
                    yev_event->dst_addrlen
                );
                io_uring_sqe_set_data(sqe, yev_event);
                io_uring_submit(&yev_loop->ring);
            }
            break;
        case YEV_ACCEPT_TYPE:
            {
                struct io_uring_sqe *sqe = io_uring_get_sqe(&yev_loop->ring);
                /*
                 *  Use the file descriptor fd to start accepting a connection request
                 *  described by the socket address at addr and of structure length addrlen
                 */
                io_uring_prep_accept(
                    sqe,
                    yev_event->fd,
                    yev_event->src_addr,
                    &yev_event->src_addrlen,
                    0
                );
                io_uring_sqe_set_data(sqe, yev_event);
                io_uring_submit(&yev_loop->ring);
            }
            break;
        case YEV_TIMER_TYPE:
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "use yev_timer_set() to start timer event",
                NULL
            );
            return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_stop_event(yev_event_t *yev_event)
{
    yev_loop_t *yev_loop = yev_event->yev_loop;
    struct io_uring_sqe *sqe;

    switch((yev_type_t)yev_event->type) {
        case YEV_READ_TYPE:
        case YEV_WRITE_TYPE:
            GBUFFER_DECREF(yev_event->bf.gbuf)
            break;
        case YEV_CONNECT_TYPE:
            GBMEM_FREE(yev_event->dst_addr)
            yev_event->dst_addrlen = 0;
            break;
        case YEV_ACCEPT_TYPE:
            GBMEM_FREE(yev_event->src_addr)
            yev_event->src_addrlen = 0;
            break;
        case YEV_TIMER_TYPE:
            break;
    }

    yev_event->flag |= YEV_STOPPING_FLAG;
    sqe = io_uring_get_sqe(&yev_loop->ring);
    io_uring_prep_cancel(sqe, yev_event, 0);
    io_uring_submit(&yev_event->yev_loop->ring);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void yev_destroy_event(yev_event_t *yev_event)
{
    hgobj gobj = yev_event->gobj;

    if(!(yev_event->flag & YEV_STOPPED_FLAG)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "event not stopped",
            NULL
        );
        yev_event->callback = NULL;
        yev_stop_event(yev_event);
    }

    if(yev_event->bf.gbuf) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "yev_event gbuffer NOT free",
            NULL
        );
        GBUFFER_DECREF(yev_event->bf.gbuf)
    }

    if(yev_event->src_addr) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "yev_event src_addr NOT free",
            NULL
        );
        GBMEM_FREE(yev_event->src_addr)
    }
    if(yev_event->dst_addr) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "yev_event dst_addr NOT free",
            NULL
        );
        GBMEM_FREE(yev_event->dst_addr)
    }

    switch((yev_type_t)yev_event->type) {
        case YEV_READ_TYPE:
        case YEV_WRITE_TYPE:
            break;
        case YEV_CONNECT_TYPE:
        case YEV_ACCEPT_TYPE:
        case YEV_TIMER_TYPE:
            if(yev_event->fd > 0) {
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
PUBLIC yev_event_t *yev_create_timer_event(yev_loop_t *loop_, yev_callback_t callback, hgobj gobj)
{
    yev_loop_t *yev_loop = loop_;

    yev_event_t *yev_event = GBMEM_MALLOC(sizeof(yev_event_t));
    if(!yev_event) {
        gobj_log_critical(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "No memory for yev event",    // use the same string
            NULL
        );
        return NULL;
    }

    yev_event->yev_loop = yev_loop;
    yev_event->type = YEV_TIMER_TYPE;
    yev_event->gobj = gobj;
    yev_event->callback = callback;
    yev_event->fd = timerfd_create(CLOCK_BOOTTIME, TFD_NONBLOCK|TFD_CLOEXEC);
    if(yev_event->fd < 0) {
        gobj_log_critical(gobj, LOG_OPT_EXIT_ZERO,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "timerfd_create() FAILED, cannot run yunetas",
            NULL
        );
        return NULL;
    }

    return yev_event;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void yev_timer_set(
    yev_event_t *yev_event_,
    time_t timeout_ms,
    BOOL periodic
) {
    yev_event_t *yev_event = yev_event_;
    struct io_uring_sqe *sqe;

    if(timeout_ms <= 0) {
        sqe = io_uring_get_sqe(&yev_event->yev_loop->ring);
        io_uring_prep_cancel(sqe, yev_event, 0);
        io_uring_submit(&yev_event->yev_loop->ring);
        return;
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
    timerfd_settime(yev_event->fd, 0, &delta, NULL);

    if(periodic) {
        yev_event->flag |= YEV_TIMER_PERIODIC_FLAG;
    }

    sqe = io_uring_get_sqe(&yev_event->yev_loop->ring);
    io_uring_prep_read(sqe, yev_event->fd, &yev_event->bf.timer_bf, sizeof(yev_event->bf.timer_bf), 0);
    io_uring_sqe_set_data(sqe, (char *)yev_event);
    io_uring_submit(&yev_event->yev_loop->ring);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE yev_event_t *yev_create_event(
    yev_loop_t *yev_loop,
    yev_callback_t callback,
    hgobj gobj,
    int fd
) {
    yev_event_t *yev_event = GBMEM_MALLOC(sizeof(yev_event_t));
    if(!yev_event) {
        gobj_log_critical(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "No memory for yev event",    // use the same string
            NULL
        );
        return NULL;
    }

    yev_event->yev_loop = yev_loop;
    yev_event->gobj = gobj;
    yev_event->callback = callback;
    yev_event->fd = fd;

    return yev_event;
}

/***************************************************************************
 *
 *********** ****************************************************************/
PUBLIC yev_event_t *yev_create_read_event(
    yev_loop_t *loop_,
    yev_callback_t callback,
    hgobj gobj,
    int fd
) {
    yev_event_t *yev_event = yev_create_event(loop_, callback, gobj, fd);
    if(!yev_event) {
        // Error already logged
        return NULL;
    }

    yev_event->type = YEV_READ_TYPE;

    return yev_event;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC yev_event_t *yev_create_write_event(
    yev_loop_t *loop_,
    yev_callback_t callback,
    hgobj gobj,
    int fd
) {
    yev_event_t *yev_event = yev_create_event(loop_, callback, gobj, fd);
    if(!yev_event) {
        // Error already logged
        return NULL;
    }

    yev_event->type = YEV_WRITE_TYPE;

    return yev_event;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC yev_event_t *yev_create_connect_event(
    yev_loop_t *loop_,
    yev_callback_t callback,
    hgobj gobj
) {
    yev_event_t *yev_event = yev_create_event(loop_, callback, gobj, -1);
    if(!yev_event) {
        // Error already logged
        return NULL;
    }

    yev_event->type = YEV_CONNECT_TYPE;

    return yev_event;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC yev_event_t *yev_create_accept_event(
    yev_loop_t *loop_,
    yev_callback_t callback,
    hgobj gobj
) {
    yev_event_t *yev_event = yev_create_event(loop_, callback, gobj, -1);
    if(!yev_event) {
        // Error already logged
        return NULL;
    }

    yev_event->type = YEV_ACCEPT_TYPE;

    return yev_event;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_setup_connect_event(
    yev_event_t *yev_event,
    const char *dst_url,
    const char *src_url
) {
    yev_event->dst_addrlen = sizeof(*yev_event->dst_addr);
    // TODO
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_setup_accept_event(
    yev_event_t *yev_event,
    const char *listen_url
) {
    // TODO
    yev_event->src_addrlen = sizeof(*yev_event->src_addr);
    return 0;
}

#ifdef PEPE
struct us_socket_t *us_socket_context_connect(int ssl, struct us_socket_context_t *context, const char *host, int port, const char *source_host, int options, int socket_ext_size) {


    struct us_socket_t *s = malloc(sizeof(struct us_socket_t) + socket_ext_size);
    s->context = context;

    s->timeout = 255;
    s->long_timeout = 255;

    us_internal_socket_context_link_socket(context, s);


    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_string[16];
    snprintf(port_string, 16, "%d", port);

    if (getaddrinfo(host, port_string, &hints, &result) != 0) {
        return NULL;
    }

    LIBUS_SOCKET_DESCRIPTOR fd = bsd_create_socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    bsd_socket_nodelay(fd, 1);
    if (fd == LIBUS_SOCKET_ERROR) {
        freeaddrinfo(result);
        return NULL;
    }

    if (source_host) {
        struct addrinfo *interface_result;
        if (!getaddrinfo(source_host, NULL, NULL, &interface_result)) {
            int ret = bind(fd, interface_result->ai_addr, (socklen_t) interface_result->ai_addrlen);
            freeaddrinfo(interface_result);
            if (ret == LIBUS_SOCKET_ERROR) {
                bsd_close_socket(fd);
                freeaddrinfo(result);
                return NULL;
            }
        }
    }

    struct io_uring_sqe *sqe = io_uring_get_sqe(&context->loop->ring);
    io_uring_prep_connect(sqe, fd, result->ai_addr, (socklen_t) result->ai_addrlen);

        static int num_sockets;

    // register this file add
    io_uring_register_files_update(&context->loop->ring, num_sockets, &fd, 1);



    s->dd = num_sockets++;


    struct iovec iovecs = {s->sendBuf, 16 * 1024};
    //printf("register: %d\n", io_uring_register_buffers_update_tag(&context->loop->ring, s->dd, &iovecs, 0, 1));


    io_uring_sqe_set_data(sqe, (char *)s + SOCKET_CONNECT);


    freeaddrinfo(result);



    return s;
}


static int pp_getaddrinfo(char *name, uint16_t port, struct addrinfo **results)
{
	int ret;
	const char *err_msg;
	char port_s[6];

	struct addrinfo hints = {
	    .ai_family = pp_ipv6 ? AF_INET6 : AF_INET,
	    .ai_socktype = SOCK_STREAM, /* TCP socket */
	    .ai_protocol = IPPROTO_TCP, /* Any protocol */
	    .ai_flags = AI_NUMERICSERV /* numeric port is used */
	};

	snprintf(port_s, 6, "%" PRIu16, port);

	ret = getaddrinfo(name, port_s, &hints, results);
	if (ret != 0) {
		err_msg = (const char *) gai_strerror(ret);
		PP_ERR("getaddrinfo : %s", err_msg);
		ret = -EXIT_FAILURE;
		goto out;
	}
	ret = EXIT_SUCCESS;

out:
	return ret;
}

static void pp_print_addrinfo(struct addrinfo *ai, char *msg)
{
	char s[80] = {0};
	void *addr;

	if (ai->ai_family == AF_INET6)
		addr = &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
	else
		addr = &((struct sockaddr_in *)ai->ai_addr)->sin_addr;

	inet_ntop(ai->ai_family, addr, s, 80);
	PP_DEBUG("%s %s\n", msg, s);
}

static int pp_ctrl_init_client(struct ct_pingpong *ct)
{
	struct addrinfo *results;
	struct addrinfo *rp;
	int errno_save = 0;
	int ret;

	ret = pp_getaddrinfo(ct->opts.dst_addr, ct->opts.dst_port, &results);
	if (ret)
		return ret;

	if (!results) {
		PP_ERR("getaddrinfo returned NULL list");
		return -EXIT_FAILURE;
	}

	for (rp = results; rp; rp = rp->ai_next) {
		ct->ctrl_connfd = ofi_socket(rp->ai_family, rp->ai_socktype,
					     rp->ai_protocol);
		if (ct->ctrl_connfd == INVALID_SOCKET) {
			errno_save = ofi_sockerr();
			continue;
		}

		if (ct->opts.src_port != 0) {
			if (pp_ipv6) {
				struct sockaddr_in6 in6_addr = {0};

				in6_addr.sin6_family = AF_INET6;
				in6_addr.sin6_port = htons(ct->opts.src_port);
				in6_addr.sin6_addr = in6addr_any;

				ret =
				    bind(ct->ctrl_connfd, (struct sockaddr *)&in6_addr,
					 sizeof(in6_addr));
			} else {
				struct sockaddr_in in_addr = {0};

				in_addr.sin_family = AF_INET;
				in_addr.sin_port = htons(ct->opts.src_port);
				in_addr.sin_addr.s_addr = htonl(INADDR_ANY);

				ret =
				    bind(ct->ctrl_connfd, (struct sockaddr *)&in_addr,
					 sizeof(in_addr));
			}
			if (ret == -1) {
				errno_save = ofi_sockerr();
				ofi_close_socket(ct->ctrl_connfd);
				continue;
			}
		}

		pp_print_addrinfo(rp, "CLIENT: connecting to");

		ret = connect(ct->ctrl_connfd, rp->ai_addr, (socklen_t) rp->ai_addrlen);
		if (ret != -1)
			break;

		errno_save = ofi_sockerr();
		ofi_close_socket(ct->ctrl_connfd);
	}

	if (!rp || ret == -1) {
		ret = -errno_save;
		ct->ctrl_connfd = -1;
		PP_ERR("failed to connect: %s", strerror(errno_save));
	} else {
		PP_DEBUG("CLIENT: connected\n");
	}

	freeaddrinfo(results);

	return ret;
}

static int pp_ctrl_init_server(struct ct_pingpong *ct)
{
	int optval = 1;
	SOCKET listenfd;
	int ret;

	listenfd = ofi_socket(pp_ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0);
	if (listenfd == INVALID_SOCKET) {
		ret = -ofi_sockerr();
		PP_PRINTERR("socket", ret);
		return ret;
	}

	ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
			 (const char *)&optval, sizeof(optval));
	if (ret == -1) {
		ret = -ofi_sockerr();
		PP_PRINTERR("setsockopt(SO_REUSEADDR)", ret);
		goto fail_close_socket;
	}

	if (pp_ipv6) {
		struct sockaddr_in6 ctrl6_addr = {0};

		ctrl6_addr.sin6_family = AF_INET6;
		ctrl6_addr.sin6_port = htons(ct->opts.src_port);
		ctrl6_addr.sin6_addr = in6addr_any;

		ret = bind(listenfd, (struct sockaddr *)&ctrl6_addr,
			   sizeof(ctrl6_addr));
	} else {
		struct sockaddr_in ctrl_addr = {0};

		ctrl_addr.sin_family = AF_INET;
		ctrl_addr.sin_port = htons(ct->opts.src_port);
		ctrl_addr.sin_addr.s_addr = htonl(INADDR_ANY);

		ret = bind(listenfd, (struct sockaddr *)&ctrl_addr,
			   sizeof(ctrl_addr));
	}
	if (ret == -1) {
		ret = -ofi_sockerr();
		PP_PRINTERR("bind", ret);
		goto fail_close_socket;
	}

	ret = listen(listenfd, 10);
	if (ret == -1) {
		ret = -ofi_sockerr();
		PP_PRINTERR("listen", ret);
		goto fail_close_socket;
	}

	PP_DEBUG("SERVER: waiting for connection\n");

	ct->ctrl_connfd = accept(listenfd, NULL, NULL);
	if (ct->ctrl_connfd == -1) {
		ret = -ofi_sockerr();
		PP_PRINTERR("accept", ret);
		goto fail_close_socket;
	}

	ofi_close_socket(listenfd);

	PP_DEBUG("SERVER: connected\n");

	return ret;

fail_close_socket:
	if (ct->ctrl_connfd != -1) {
		ofi_close_socket(ct->ctrl_connfd);
		ct->ctrl_connfd = -1;
	}

	if (listenfd != -1)
		ofi_close_socket(listenfd);

	return ret;
}





static int fio_init_server_ip(void)
{
	struct sockaddr *addr;
	socklen_t socklen;
	char buf[80];
	const char *str;
	int sk, opt;

	if (use_ipv6)
		sk = socket(AF_INET6, SOCK_STREAM, 0);
	else
		sk = socket(AF_INET, SOCK_STREAM, 0);

	if (sk < 0) {
		log_err("fio: socket: %s\n", strerror(errno));
		return -1;
	}

	opt = 1;
	if (setsockopt(sk, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof(opt)) < 0) {
		log_err("fio: setsockopt(REUSEADDR): %s\n", strerror(errno));
		close(sk);
		return -1;
	}
#ifdef SO_REUSEPORT
	/*
	 * Not fatal if fails, so just ignore it if that happens
	 */
	if (setsockopt(sk, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) {
	}
#endif

	if (use_ipv6) {
		void *src = &saddr_in6.sin6_addr;

		addr = (struct sockaddr *) &saddr_in6;
		socklen = sizeof(saddr_in6);
		saddr_in6.sin6_family = AF_INET6;
		str = inet_ntop(AF_INET6, src, buf, sizeof(buf));
	} else {
		void *src = &saddr_in.sin_addr;

		addr = (struct sockaddr *) &saddr_in;
		socklen = sizeof(saddr_in);
		saddr_in.sin_family = AF_INET;
		str = inet_ntop(AF_INET, src, buf, sizeof(buf));
	}

	if (bind(sk, addr, socklen) < 0) {
		log_err("fio: bind: %s\n", strerror(errno));
		log_info("fio: failed with IPv%c %s\n", use_ipv6 ? '6' : '4', str);
		close(sk);
		return -1;
	}

	return sk;
}

static int fio_init_server_sock(void)
{
	struct sockaddr_un addr;
	socklen_t len;
	mode_t mode;
	int sk;

	sk = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sk < 0) {
		log_err("fio: socket: %s\n", strerror(errno));
		return -1;
	}

	mode = umask(000);

	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", bind_sock);

	len = sizeof(addr.sun_family) + strlen(bind_sock) + 1;

	if (bind(sk, (struct sockaddr *) &addr, len) < 0) {
		log_err("fio: bind: %s\n", strerror(errno));
		close(sk);
		return -1;
	}

	umask(mode);
	return sk;
}

static int fio_init_server_connection(void)
{
	char bind_str[128];
	int sk;

	dprint(FD_NET, "starting server\n");

	if (!bind_sock)
		sk = fio_init_server_ip();
	else
		sk = fio_init_server_sock();

	if (sk < 0)
		return sk;

	memset(bind_str, 0, sizeof(bind_str));

	if (!bind_sock) {
		char *p, port[16];
		void *src;
		int af;

		if (use_ipv6) {
			af = AF_INET6;
			src = &saddr_in6.sin6_addr;
		} else {
			af = AF_INET;
			src = &saddr_in.sin_addr;
		}

		p = (char *) inet_ntop(af, src, bind_str, sizeof(bind_str));

		sprintf(port, ",%u", fio_net_port);
		if (p)
			strcat(p, port);
		else
			snprintf(bind_str, sizeof(bind_str), "%s", port);
	} else
		snprintf(bind_str, sizeof(bind_str), "%s", bind_sock);

	log_info("fio: server listening on %s\n", bind_str);

	if (listen(sk, 4) < 0) {
		log_err("fio: listen: %s\n", strerror(errno));
		close(sk);
		return -1;
	}

	return sk;
}

int fio_server_parse_host(const char *host, int ipv6, struct in_addr *inp,
			  struct in6_addr *inp6)

{
	int ret = 0;

	if (ipv6)
		ret = inet_pton(AF_INET6, host, inp6);
	else
		ret = inet_pton(AF_INET, host, inp);

	if (ret != 1) {
		struct addrinfo *res, hints = {
			.ai_family = ipv6 ? AF_INET6 : AF_INET,
			.ai_socktype = SOCK_STREAM,
		};

		ret = getaddrinfo(host, NULL, &hints, &res);
		if (ret) {
			log_err("fio: failed to resolve <%s> (%s)\n", host,
					gai_strerror(ret));
			return 1;
		}

		if (ipv6)
			memcpy(inp6, &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr, sizeof(*inp6));
		else
			memcpy(inp, &((struct sockaddr_in *) res->ai_addr)->sin_addr, sizeof(*inp));

		ret = 1;
		freeaddrinfo(res);
	}

	return !(ret == 1);
}

/*
 * Parse a host/ip/port string. Reads from 'str'.
 *
 * Outputs:
 *
 * For IPv4:
 *	*ptr is the host, *port is the port, inp is the destination.
 * For IPv6:
 *	*ptr is the host, *port is the port, inp6 is the dest, and *ipv6 is 1.
 * For local domain sockets:
 *	*ptr is the filename, *is_sock is 1.
 */
int fio_server_parse_string(const char *str, char **ptr, bool *is_sock,
			    int *port, struct in_addr *inp,
			    struct in6_addr *inp6, int *ipv6)
{
	const char *host = str;
	char *portp;
	int lport = 0;

	*ptr = NULL;
	*is_sock = false;
	*port = fio_net_port;
	*ipv6 = 0;

	if (!strncmp(str, "sock:", 5)) {
		*ptr = strdup(str + 5);
		*is_sock = true;

		return 0;
	}

	/*
	 * Is it ip:<ip or host>:port
	 */
	if (!strncmp(host, "ip:", 3))
		host += 3;
	else if (!strncmp(host, "ip4:", 4))
		host += 4;
	else if (!strncmp(host, "ip6:", 4)) {
		host += 4;
		*ipv6 = 1;
	} else if (host[0] == ':') {
		/* String is :port */
		host++;
		lport = atoi(host);
		if (!lport || lport > 65535) {
			log_err("fio: bad server port %u\n", lport);
			return 1;
		}
		/* no hostname given, we are done */
		*port = lport;
		return 0;
	}

	/*
	 * If no port seen yet, check if there's a last ',' at the end
	 */
	if (!lport) {
		portp = strchr(host, ',');
		if (portp) {
			*portp = '\0';
			portp++;
			lport = atoi(portp);
			if (!lport || lport > 65535) {
				log_err("fio: bad server port %u\n", lport);
				return 1;
			}
		}
	}

	if (lport)
		*port = lport;

	if (!strlen(host))
		return 0;

	*ptr = strdup(host);

	if (fio_server_parse_host(*ptr, *ipv6, inp, inp6)) {
		free(*ptr);
		*ptr = NULL;
		return 1;
	}

	if (*port == 0)
		*port = fio_net_port;

	return 0;
}



#endif
