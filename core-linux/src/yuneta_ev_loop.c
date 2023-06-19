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
#define DEFAULT_ENTRIES 2024

//#define MAX_CONNECTIONS     4096
#define MAX_MESSAGE_LEN     2048
#define BUFFERS_COUNT       MAX_CONNECTIONS


typedef enum  {
    YEV_TIMER_TYPE        = 1,
    YEV_ACCEPT_TYPE,
    YEV_READ_TYPE,
    YEV_WRITE_TYPE,
//    YEV_PROV_BUF_EV,

//    YEV_LISTEN_EV       = 2,
//    YEV_READ_TYPE         = 3,
//    YEV_READ_DATA_EV    = 4,
//    YEV_WRITE_DATA_EV   = 5,
//    YEV_WRITE_TYPE        = 6,
//    YEV_TIMER_ONCE_EV   = 7,
//    YEV_READ_CB_EV      = 8,
//    YEV_READV_EV        = 9,
//
//    SOCKET_READ,
//    SOCKET_WRITE,
//    LISTEN_SOCKET_ACCEPT,
//    SOCKET_CONNECT,
} yev_type_t;

typedef enum  {
    YEV_STOPPING_FLAG           = 0x01,
    YEV_STOPPED_FLAG            = 0x02,
    YEV_TIMER_PERIODIC_FLAG     = 0x04,
} yev_flag_t;

/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct yev_event_s yev_event_t;
typedef struct yev_loop_s yev_loop_t;

struct yev_event_s {
    yev_loop_t *yev_loop;
    uint8_t type;               // yev_type_t
    uint8_t flag;               // yev_flag_t
    int fd;
    union {
        uint64_t timer_bf;
    } bf;
    hgobj gobj;
    yev_callback_t callback;
//    struct iovec iov;
};

struct yev_loop_s {
    struct io_uring ring;
    unsigned entries;
    hgobj yuno;
};

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_loop_create(hgobj yuno, unsigned entries, yev_loop_h *yev_loop_)
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
PUBLIC void yev_loop_destroy(yev_loop_h yev_loop_)
{
    yev_loop_t *yev_loop = yev_loop_;

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
PUBLIC int yev_loop_run(yev_loop_h yev_loop_)
{
    yev_loop_t *yev_loop = yev_loop_;

    /*------------------------------------------*
     *  Register buffers for buffer selection
     *------------------------------------------*/
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
// TODO
//    sqe = io_uring_get_sqe(&yev_loop->ring);
//    io_uring_prep_provide_buffers(sqe, bufs, MAX_MESSAGE_LEN, BUFFERS_COUNT, group_id, 0); // TODO bufs
//
//    io_uring_submit(&yev_loop->ring);
//    io_uring_wait_cqe(&yev_loop->ring, &cqe);
//    if (cqe->res < 0) {
//        gobj_log_critical(gobj, LOG_OPT_EXIT_ZERO,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
//            "msg",          "%s", "Linux io_uring_wait_cqe() FAILED, cannot run yunetas",
//            "res",          "%d", (int)cqe->res,
//            NULL
//        );
//        return -1;
//    }
//    io_uring_cqe_seen(&yev_loop->ring, cqe);

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

        /* Mark this request as processed */
        io_uring_cqe_seen(&yev_loop->ring, cqe);

        switch((yev_type_t)yev_event->type) {
            case YEV_READ_TYPE:
                break;
            case YEV_ACCEPT_TYPE:
                break;
            case YEV_WRITE_TYPE:
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
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_start_event(yev_event_h yev_event_)
{
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
    if(yev_event->flag & YEV_STOPPED_FLAG) {
        yev_event->flag &= ~YEV_STOPPED_FLAG;
    }

    switch((yev_type_t)yev_event->type) {
        case YEV_READ_TYPE:
            {
                struct io_uring_sqe *sqe = io_uring_get_sqe(&yev_loop->ring);
                //    req->iov[0].iov_base = malloc(READ_SZ);
                //    req->iov[0].iov_len = READ_SZ;
                //    memset(req->iov[0].iov_base, 0, READ_SZ);
                // TODO    io_uring_prep_readv(sqe, client_socket, &req->iov[0], 1, 0);
                io_uring_sqe_set_data(sqe, yev_event);
                io_uring_submit(&yev_loop->ring);

            }
            break;
        case YEV_ACCEPT_TYPE:
            {
                struct io_uring_sqe *sqe = io_uring_get_sqe(&yev_loop->ring);
                // TODO io_uring_prep_accept(sqe, server_socket, (struct sockaddr *) client_addr, client_addr_len, 0);
                io_uring_sqe_set_data(sqe, yev_event);
                io_uring_submit(&yev_loop->ring);
            }
            break;
        case YEV_WRITE_TYPE:
            {
                struct io_uring_sqe *sqe = io_uring_get_sqe(&yev_loop->ring);
                // TODO    io_uring_prep_writev(sqe, yev_event->fd, req->iov, req->iovec_count, 0);
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
PUBLIC int yev_stop_event(yev_event_h yev_event_)
{
    yev_event_t *yev_event = yev_event_;
    yev_loop_t *yev_loop = yev_event->yev_loop;
    struct io_uring_sqe *sqe;

    switch((yev_type_t)yev_event->type) {
        case YEV_READ_TYPE:
        case YEV_ACCEPT_TYPE:
        case YEV_WRITE_TYPE:
        case YEV_TIMER_TYPE:
            yev_event->flag |= YEV_STOPPING_FLAG;
            sqe = io_uring_get_sqe(&yev_loop->ring);
            io_uring_prep_cancel(sqe, yev_event, 0);
            io_uring_submit(&yev_event->yev_loop->ring);
            break;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void yev_destroy_event(yev_event_h yev_event_)
{
    yev_event_t *yev_event = yev_event_;
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

    switch((yev_type_t)yev_event->type) {
        case YEV_READ_TYPE:
            break;
        case YEV_ACCEPT_TYPE:
            break;
        case YEV_WRITE_TYPE:
            break;
        case YEV_TIMER_TYPE:
            if(yev_event->fd > 0) {
                close(yev_event->fd);
            }
            break;
    }

    GBMEM_FREE(yev_event)
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC yev_event_h yev_create_timer_event(yev_loop_h loop_, yev_callback_t callback, hgobj gobj)
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
    yev_event_h yev_event_,
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
    if(fd < 0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "fd negative",
            "fd",           "%d", fd,
            NULL
        );
        return NULL;
    }

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
 ***************************************************************************/
PUBLIC yev_event_h yev_create_accept_event(
    yev_loop_h loop_,
    yev_callback_t callback,
    hgobj gobj,
    int fd
) {
    yev_event_t *yev_event = yev_create_event(loop_, callback, gobj, fd);
    if(!yev_event) {
        // Error already logged
        return NULL;
    }

    yev_event->type = YEV_ACCEPT_TYPE;

    return yev_event;
}

/***************************************************************************
 *
 *********** ****************************************************************/
PUBLIC yev_event_h yev_create_read_event(
    yev_loop_h loop_,
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
PUBLIC yev_event_h yev_create_write_event(
    yev_loop_h loop_,
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
