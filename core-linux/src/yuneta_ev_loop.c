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
#define ENTRIES 2024

#define MAX_CONNECTIONS     4096
#define MAX_MESSAGE_LEN     2048
#define BUFFERS_COUNT       MAX_CONNECTIONS


//int group_id = 1337; // TODO ??? what is?
//char bufs[BUFFERS_COUNT][MAX_MESSAGE_LEN]; // TODO ???

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
PUBLIC int yev_loop_create(hgobj yuno, yev_loop_t **yev_loop_)
{
    struct io_uring ring_test;
    int err;

    *yev_loop_ = 0;     // error case

    struct io_uring_params params_test = {0};
    // TODO use IORING_SETUP_SQPOLL when you know how to use
    // Info in https://unixism.net/loti/tutorial/sq_poll.html
    //params_test.flags |= IORING_SETUP_SQPOLL;
    //params_test.flags |= IORING_SETUP_COOP_TASKRUN;      // Available since 5.18
    //params_test.flags |= IORING_SETUP_SINGLE_ISSUER;     // Available since 6.0
retry:
    err = io_uring_queue_init_params(ENTRIES, &ring_test, &params_test);
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
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_SYSTEM_ERROR,
            "msg",                  "%s", "Linux kernel without io_uring, cannot run yunetas",
            "errno",                "%d", -err,
            "serrno",               "%s", strerror(-err),
            NULL
        );
        return -1;
    }

    /* FAST_POOL is required for pre-posting receive buffers */
    if (!(params_test.features & IORING_FEAT_FAST_POLL)) {
        io_uring_queue_exit(&ring_test);
        gobj_log_critical(yuno, LOG_OPT_EXIT_ZERO,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_SYSTEM_ERROR,
            "msg",                  "%s", "Linux kernel without io_uring IORING_FEAT_FAST_POLL, cannot run yunetas",
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
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_SYSTEM_ERROR,
            "msg",                  "%s", "Linux kernel without io_uring IORING_OP_PROVIDE_BUFFERS, cannot run yunetas",
            NULL
        );
        return -1;
    }
    io_uring_free_probe(probe);
    io_uring_queue_exit(&ring_test);

    yev_loop_t *yev_loop = GBMEM_MALLOC(sizeof(yev_loop_t));
    if(!yev_loop) {
        gobj_log_critical(yuno, LOG_OPT_ABORT,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_MEMORY_ERROR,
            "msg",                  "%s", "No memory to yev_loop",
            NULL
        );
        return -1;
    }
    struct io_uring_params params = {
        .flags = params_test.flags
    };

    err = io_uring_queue_init_params(ENTRIES, &yev_loop->ring, &params);
    if (err < 0) {
        gobj_log_critical(yuno, LOG_OPT_EXIT_ZERO,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_SYSTEM_ERROR,
            "msg",                  "%s", "Linux io_uring_queue_init_params() FAILED, cannot run yunetas",
            "errno",                "%d", -err,
            "serrno",               "%s", strerror(-err),
            NULL
        );
        return -1;
    }

    yev_loop->yuno = yuno;

    *yev_loop_ = yev_loop;

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void yev_loop_destroy(yev_loop_t *yev_loop)
{
    // TODO destroy all events

    io_uring_queue_exit(&yev_loop->ring);
    GBMEM_FREE(yev_loop)
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_loop_run(yev_loop_t *yev_loop)
{
    /*------------------------------------------*
     *  Register buffers for buffer selection
     *------------------------------------------*/
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;

//    sqe = io_uring_get_sqe(&yev_loop->ring);
//    io_uring_prep_provide_buffers(sqe, bufs, MAX_MESSAGE_LEN, BUFFERS_COUNT, group_id, 0); // TODO bufs
//
//    io_uring_submit(&yev_loop->ring);
//    io_uring_wait_cqe(&yev_loop->ring, &cqe);
//    if (cqe->res < 0) {
//        gobj_log_critical(gobj, LOG_OPT_EXIT_ZERO,
//            "function",             "%s", __FUNCTION__,
//            "msgset",               "%s", MSGSET_SYSTEM_ERROR,
//            "msg",                  "%s", "Linux io_uring_wait_cqe() FAILED, cannot run yunetas",
//            "res",                  "%d", (int)cqe->res,
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
                "function",             "%s", __FUNCTION__,
                "msgset",               "%s", MSGSET_SYSTEM_ERROR,
                "msg",                  "%s", "io_uring_wait_cqe() FAILED",
                "errno",                "%d", -err,
                "serrno",               "%s", strerror(-err),
                NULL
            );
            break;
        }
        yev_event_t *yev_event = (yev_event_t *)io_uring_cqe_get_data(cqe);
        switch(yev_event->type) {
            case YEV_TIMER_EV:
                {
                    sqe = io_uring_get_sqe(&yev_loop->ring);
                    io_uring_prep_read(sqe, yev_event->fd, &yev_event->buf, sizeof(yev_event->buf), 0);
                    io_uring_sqe_set_data(sqe, yev_event);
                    io_uring_submit(&yev_loop->ring); // TODO repeat only if periodic
                    yev_event->callback(yev_event->gobj, yev_event, 0);
                }
                break;
            default:
                printf("=============> Event %d\n", yev_event->type);
        }

        /* Mark this request as processed */
        io_uring_cqe_seen(&yev_loop->ring, cqe);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void yev_destroy_event(yev_event_t *yev_event)
{
    // TODO remove from ring?
    GBMEM_FREE(yev_event)
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC yev_event_t *yev_create_timer(yev_loop_t *loop, yev_callback_t callback, hgobj gobj)
{
    yev_event_t *yev_timer = GBMEM_MALLOC(sizeof(yev_event_t));
    if(!yev_timer) {
        gobj_log_critical(gobj, LOG_OPT_ABORT,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_SYSTEM_ERROR,
            "msg",                  "%s", "No memory for loop timer",
            NULL
        );
        return NULL;
    }

    yev_timer->yev_loop = loop;
    yev_timer->type = YEV_TIMER_EV;
    yev_timer->gobj = gobj;
    yev_timer->callback = callback;
    yev_timer->fd = timerfd_create(CLOCK_BOOTTIME, TFD_NONBLOCK|TFD_CLOEXEC);
    if(yev_timer->fd < 0) {
        gobj_log_critical(gobj, LOG_OPT_EXIT_ZERO,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_SYSTEM_ERROR,
            "msg",                  "%s", "timerfd_create() FAILED, cannot run yunetas",
            NULL
        );
        return NULL;
    }

    return yev_timer;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void yev_timer_set(
    yev_event_t *yev_event,
    time_t timeout_ms,
    BOOL periodic
) {

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

    // TODO if timeout_ms es 0 para el timer, como se quita del queue ring?

    // prep read into uint64_t of timer
    struct io_uring_sqe *sqe = io_uring_get_sqe(&yev_event->yev_loop->ring);
    io_uring_prep_read(sqe, yev_event->fd, &yev_event->buf, sizeof(yev_event->buf), 0);
    io_uring_sqe_set_data(sqe, (char *)yev_event);
    io_uring_submit(&yev_event->yev_loop->ring);
}
