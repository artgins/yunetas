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

enum pointer_tags {
     TIMER_EV       = 1,
     LISTEN_EV      = 2,
     READ_EV        = 3,
     READ_DATA_EV   = 4,
     WRITE_DATA_EV  = 5,
     WRITE_EV       = 6,
     TIMER_ONCE_EV  = 7,
     READ_CB_EV     = 8,
     READV_EV       = 9,

//    SOCKET_READ,
//    SOCKET_WRITE,
//    LISTEN_SOCKET_ACCEPT,
//    SOCKET_CONNECT,
    LOOP_TIMER = 10,
};

int group_id = 1337; // TODO ??? what is?
char bufs[BUFFERS_COUNT][MAX_MESSAGE_LEN]; // TODO ???

/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct yev_event_s {
    struct yev_loop_s *yev_loop;
    int type;
    int fd;
    uint64_t buf;
    hgobj gobj;
    gobj_event_t ev;
//    mr_timer_cb *tcb;
//    mr_accept_cb *acb;
//    mr_read_cb *rcb;
//    mr_write_cb *wcb;
//    mr_done_cb *dcb;
//    void *user_data;
//    struct iovec iov;
} yev_event_t;

typedef struct yev_loop_s {
    struct io_uring ring;
    yev_event_t *timer;
    hgobj yuno;
} yev_loop_t;

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/


/***************************************************************************
 *
 ***************************************************************************/
PUBLIC yev_event_t *yev_create_timer(yev_loop_t *loop, hgobj gobj, gobj_event_t ev)
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
    yev_timer->type = TIMER_EV;
    yev_timer->gobj = gobj;
    yev_timer->ev = ev;
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
    yev_event_t *t,
    void (*cb)(yev_event_t *t),
    int timeout_ms,
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
    timerfd_settime(t->fd, 0, &delta, NULL);

    // prep read into uint64_t of timer
    struct io_uring_sqe *sqe = io_uring_get_sqe(&t->yev_loop->ring);
    io_uring_prep_read(sqe, t->fd, &t->buf, sizeof(t->buf), 0);
    io_uring_sqe_set_data(sqe, (char *)t);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_loop_create(hgobj yuno, yev_loop_h *yev_loop_)
{
    struct io_uring ring_test;
    int err;

    *yev_loop_ = 0;     // error case

    struct io_uring_params params_test = {0};
    params_test.flags |= IORING_SETUP_SQPOLL;
    params_test.flags |= IORING_SETUP_COOP_TASKRUN;      // Available since 5.18
    params_test.flags |= IORING_SETUP_SINGLE_ISSUER;     // Available since 6.0
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
            "errno",                "%s", strerror(-err),
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
    io_uring_free_probe(probe); // DIE here
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
            "errno",                "%s", strerror(-err),
            NULL
        );
        return -1;
    }

    yev_loop->yuno = yuno;
    yev_loop->timer = yev_create_timer(yev_loop, yuno, EV_PERIODIC_TIMEOUT);

    *yev_loop_ = yev_loop;

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void yev_loop_destroy(yev_loop_h yev_loop_)
{
    yev_loop_t *yev_loop = yev_loop_;
    io_uring_queue_exit(&yev_loop->ring);
    GBMEM_FREE(yev_loop->timer)
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

    yev_timer_set(
        yev_loop->timer,
        NULL,
        1000,
        FALSE
    );

    /*------------------------------------------*
     *      Infinite loop
     *------------------------------------------*/
    while(gobj_is_running(yev_loop->yuno)) {
        io_uring_submit_and_wait(&yev_loop->ring, 1);
        unsigned head;
        unsigned count = 0;

printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");

        // go through all CQEs
        io_uring_for_each_cqe(&yev_loop->ring, head, cqe) {
            yev_event_t *yev_event = (yev_event_t *)io_uring_cqe_get_data(cqe);
            switch(yev_event->type) {
                case TIMER_EV:
                    {
                        sqe = io_uring_get_sqe(&yev_loop->ring);
                        io_uring_prep_read(sqe, yev_event->fd, &yev_event->buf, sizeof(yev_event->buf), 0);
                        io_uring_sqe_set_data(sqe, yev_event);
                        gobj_send_event(yev_event->gobj, yev_event->ev, 0, yev_loop->yuno);
                    }
                    break;
                default:
                    printf("=============> Event %d\n", yev_event->type);
            }

            ++count;
        }

        io_uring_cq_advance(&yev_loop->ring, count);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_loop_stop(yev_loop_h yev_loop_)
{
    yev_loop_t *yev_loop = yev_loop_;

    gobj_set_yuno_must_die();

    return 0;
}
