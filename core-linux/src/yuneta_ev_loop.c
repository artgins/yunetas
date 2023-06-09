/****************************************************************************
 *          yev_loop.c
 *
 *          Yuneta Event Loop
 *
 *          Copyright (c) 2024 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <liburing.h>
#include "yuneta_ev_loop.h"

/***************************************************************
 *              Constants
 ***************************************************************/
#define ENTRIES 2024

/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct yev_loop_s {
    struct io_uring ring;
    hgobj gobj;
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
PUBLIC int yev_loop_create(hgobj gobj, yev_loop_h *yev_loop_)
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

        gobj_log_critical(gobj, LOG_OPT_EXIT_ZERO,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_SYSTEM_ERROR,
            "msg",                  "%s", "Linux kernel without io_uring, cannot run yunetas",
            "errno",                "%s", strerror(-err),
            NULL
        );
        return -1;
    }
    io_uring_queue_exit(&ring_test);

    /* FAST_POOL is required for pre-posting receive buffers */
    if (!(params_test.features & IORING_FEAT_FAST_POLL)) {
        gobj_log_critical(gobj, LOG_OPT_EXIT_ZERO,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_SYSTEM_ERROR,
            "msg",                  "%s", "Linux kernel without io_uring IORING_FEAT_FAST_POLL, cannot run yunetas",
            NULL
        );
        return -1;
    }

    yev_loop_t *yev_loop = GBMEM_MALLOC(sizeof(yev_loop_t));
    if(!yev_loop) {
        gobj_log_critical(gobj, LOG_OPT_ABORT,
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
        gobj_log_critical(gobj, LOG_OPT_EXIT_ZERO,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_SYSTEM_ERROR,
            "msg",                  "%s", "Linux io_uring_queue_init_params() FAILED, cannot run yunetas",
            "errno",                "%s", strerror(-err),
            NULL
        );
        return -1;
    }

    yev_loop->gobj = gobj;
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
    GBMEM_FREE(yev_loop)
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_loop_run(yev_loop_h yev_loop_)
{
    yev_loop_t *yev_loop = yev_loop_;

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_loop_stop(yev_loop_h yev_loop_)
{
    yev_loop_t *yev_loop = yev_loop_;

    return 0;
}
