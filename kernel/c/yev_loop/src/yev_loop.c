/****************************************************************************
 *          yev_loop.c
 *
 *          Yunetas Event Loop

1. Two-phase completion in zerocopy
    A YEV_SENDMSG_TYPE event is sent with io_uring_prep_sendmsg_zc() when the
    kernel has it (probed in yev_loop_create()), else with a plain
    io_uring_prep_sendmsg(). A zero-copy send may not copy the data: the
    kernel reads the buffer until the datagram is transmitted.

2. Completion events
    First CQE → the result of the send.
        cqe->res >= 0 = bytes sent, < 0 = the error (an error too can come
        with IORING_CQE_F_MORE).
        cqe->flags & IORING_CQE_F_MORE: a second CQE follows.
    Second CQE → the notification that the kernel released the buffer.
        cqe->res = 0, cqe->flags & IORING_CQE_F_NOTIF.
    Without IORING_CQE_F_MORE in the first CQE there is no second one.

3. What the loop does with them
    - A CQE with IORING_CQE_F_MORE does not end the operation: in_flight
      is not decremented, so the event is not freed (a destroy is deferred).
    - The callback is called once, at the first CQE.
    - The notification is not delivered to the callback. It only ends the
      operation: in_flight is decremented, and an event destroyed before it
      is freed there, with its gbuffer.
    - It can arrive in any state: the event may be sent again, or stopped,
      before the notification of the previous send.
    In 7.25.4 and earlier each submission counted one CQE: an event
    destroyed at the first CQE (C_UDP_S when all the data is sent) was
    freed, and the notification read the freed event.

# io_uring Cancel CQEs

You **always** receive two CQEs (at minimum) when a cancellation succeeds:

1. **Cancel operation CQE** — result of the cancel request itself
2. **Cancelled operation CQE** — the original operation completes with `-ECANCELED`

---

## Race Condition

When the read fails on its own before the cancel takes effect:

| Order | CQE              | res  | errno        | Meaning |
|-------|------------------|------|--------------|---------|
| 1st   | Read operation   | -104 | `ECONNRESET` | Connection was reset by peer |
| 2nd   | Cancel operation | -2   | `ENOENT`     | Nothing to cancel (already completed) |

**Explanation:**

1. The **read** failed because the remote peer closed/reset the connection (`ECONNRESET`)
2. The **cancel** arrived too late — the read had already completed (with an error), so there was nothing to cancel → `ENOENT`

This is **not** a cancelled operation. A truly cancelled read would show:

```
Read CQE:   res = -125 (ECANCELED)
Cancel CQE: res = 0    (success)
```


 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <liburing.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/timerfd.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include <testing.h>
#include <helpers.h>
#include <gobj.h>
#include "yev_loop.h"

#include "static_resolv.h"

/***************************************************************
 *              Constants
 ***************************************************************/
int multishot_available = 0; // Available since kernel 5.19 NOT TESTED!! DONT'USE

/*
 *  Cycles of the loop with submissions the kernel does not take, after
 *  which the loop says so (see submit_pending)
 */
#define YEV_PENDING_CYCLES_ALARM    100

/*
 *  How long yev_loop_destroy() waits for the completions of the destroyed
 *  events before it frees them anyway
 */
#define YEV_DYING_WAIT_MS           1000

/*
 *  How long it waits, in all, when what is left are zero-copy sends whose
 *  notification has not come. A notification cannot be canceled: it comes
 *  when the kernel frees the packet, and a packet waiting for an ARP
 *  resolution that fails is held about 3 s (3 probes, 1 s apart).
 */
#define YEV_ZC_NOTIF_WAIT_MS        5000

/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct yev_loop_s yev_loop_t;

/*
 *  A completion made by the loop itself, delivered at the next cycle as if
 *  the kernel had posted it (see take_back_submissions)
 */
typedef struct {
    uint64_t user_data;
    int res;
} kept_cqe_t;

struct yev_loop_s {
    struct io_uring ring;   // HACK first member: the tests reach it
    unsigned entries;
    hgobj yuno;
    int keep_alive;
    volatile int running;
    volatile int stopping;
    yev_callback_t callback; // if return -1 the loop in yev_loop_run will break;

    /*
     *  Submissions the kernel did not take (see get_sqe), in order, and
     *  the completions of those that were stopped before it took them
     */
    struct io_uring_sqe *kept_sqes;
    unsigned kept_sqes_size;
    unsigned kept_sqes_max;
    kept_cqe_t *kept_cqes;
    unsigned kept_cqes_size;
    unsigned kept_cqes_max;

    /*
     *  Cycles in a row with submissions the kernel did not take, and
     *  whether that was said (see submit_pending)
     */
    unsigned pending_cycles;
    BOOL pending_told;

    /*
     *  The kernel has a zero-copy sendmsg (probed at create): if not, a
     *  YEV_SENDMSG_TYPE event is sent with a plain sendmsg
     */
    BOOL sendmsg_zc;

    /*
     *  Events destroyed with completions still to come (see
     *  yev_destroy_event): freed at their last completion, or by
     *  yev_loop_destroy() when the loop ends first
     */
    yev_event_t *dying;
    unsigned dying_size;
};

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE yev_state_t yev_set_state(yev_event_t *yev_event, yev_state_t new_state);
PRIVATE int print_addrinfo(hgobj gobj, char *bf, size_t bfsize, struct addrinfo *ai, int port);
PRIVATE void forget_kept(yev_loop_t *yev_loop, yev_event_t *yev_event);
PRIVATE void take_back_submissions_on_fd(yev_loop_t *yev_loop, int fd);
PRIVATE void host_without_brackets(char *host);
PRIVATE int bind_src_url(
    hgobj gobj,
    int fd,
    const char *src_url,
    int ai_family,
    int ai_socktype,
    int ai_protocol
);
PRIVATE void free_dying_events(yev_loop_t *yev_loop);
PRIVATE unsigned queue_entries_of(yev_loop_t *yev_loop, yev_event_t *yev_event, BOOL take);

/***************************************************************
 *              Data
 ***************************************************************/
/*
 *  The event of an entry taken back from the submission queue (see
 *  take_back_from_queue): its completion is not delivered
 */
PRIVATE yev_event_t taken_back_event;

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
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "No memory to yev_loop",
            NULL
        );
        return -1;
    }
    struct io_uring_params params = {
        .flags = params_test.flags | IORING_SETUP_CLAMP
    };
    /*
     *  ENOMEM here is nearly always RLIMIT_MEMLOCK, not a shortage of RAM:
     *  the rings are pinned pages and the budget is charged per USER, shared
     *  by every yuno running as the same account. A yuno asking for 32768
     *  entries pins ~3.1 MB, so the common 8 MB default admits only two of
     *  them and the rest abort at startup on a node with gigabytes free.
     *  Report the limit and the ring footprint below: without them the log
     *  reads as a memory leak and sends the reader hunting in the wrong place.
     */
    struct rlimit rl_memlock;
    char memlock_limit[32];
    if(getrlimit(RLIMIT_MEMLOCK, &rl_memlock) < 0) {
        snprintf(memlock_limit, sizeof(memlock_limit), "unknown");
    } else if(rl_memlock.rlim_cur == RLIM_INFINITY) {
        snprintf(memlock_limit, sizeof(memlock_limit), "unlimited");
    } else {
        snprintf(memlock_limit, sizeof(memlock_limit), "%lu bytes",
            (unsigned long)rl_memlock.rlim_cur
        );
    }
    unsigned long ring_bytes =
        (unsigned long)entries * sizeof(struct io_uring_sqe) +
        (unsigned long)entries * 2 * sizeof(struct io_uring_cqe) +
        (unsigned long)entries * sizeof(uint32_t);

    /*
     *  Retry on transient ENOMEM/EAGAIN before aborting. io_uring rings
     *  consume pinned kernel memory (RLIMIT_MEMLOCK / vm.max_user_locks);
     *  a synchronised restart of many yunos (e.g. an agent deactivate-snap
     *  on a node with 10+ yunos) momentarily saturates it even though
     *  the kernel releases the previous rings' pages within milliseconds.
     *  Without this retry every mass-restart used to dump N cores in
     *  /var/crash for no operational reason: the ydaemon watcher relaunches,
     *  the 2nd or 3rd attempt succeeds, but every failed attempt aborts
     *  with SIGABRT. Exponential backoff caps the wait at ~3 s total.
     *
     *  This absorbs a BURST, never a ceiling: if memlock is simply too low
     *  for the configured io_uring_entries, all five attempts fail and the
     *  abort below fires with the diagnostics gathered above.
     *
     *  Non-transient errors (EINVAL, ENOSYS, EPERM, …) skip the retry and
     *  fall through to the original abort path — those are real config /
     *  kernel-support failures, not pressure.
     */
    int retry_delay_ms = 100;
    for(int retry = 0; retry < 5; retry++) {
        err = io_uring_queue_init_params(entries, &yev_loop->ring, &params);
        if(err >= 0) {
            break;
        }
        if(err != -ENOMEM && err != -EAGAIN) {
            break;
        }
        gobj_log_warning(yuno, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "io_uring_queue_init_params() pinned-memory pressure, retrying",
            "attempt",      "%d", retry + 1,
            "delay_ms",     "%d", retry_delay_ms,
            "entries",      "%d", entries,
            "ring_bytes",   "%lu", ring_bytes,
            "memlock",      "%s", memlock_limit,
            "errno",        "%d", -err,
            "serrno",       "%s", strerror(-err),
            NULL
        );
        struct timespec ts = {
            .tv_sec  = retry_delay_ms / 1000,
            .tv_nsec = (retry_delay_ms % 1000) * 1000000L,
        };
        nanosleep(&ts, NULL);
        retry_delay_ms *= 2;
    }
    if (err < 0) {
        GBMEM_FREE(yev_loop)
        const char *hint = "";
        if(err == -ENOMEM) {
            hint = "RLIMIT_MEMLOCK exhausted (check 'ulimit -l'): it is charged "
                "per user and shared by every yuno. Raise memlock in "
                "/etc/security/limits.d/99-yuneta-core.conf, or lower the yuno's "
                "'io_uring_entries' attr";
        }
        gobj_log_critical(yuno, LOG_OPT_ABORT,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "Linux io_uring_queue_init_params() FAILED, cannot run yunetas",
            "entries",      "%d", entries,
            "ring_bytes",   "%lu", ring_bytes,
            "memlock",      "%s", memlock_limit,
            "hint",         "%s", hint,
            "errno",        "%d", -err,
            "serrno",       "%s", strerror(-err),
            NULL
        );
        return -1;
    }

    yev_loop->yuno = yuno;
    yev_loop->entries = entries;

    struct io_uring_probe *probe = io_uring_get_probe_ring(&yev_loop->ring);
    if(probe) {
        yev_loop->sendmsg_zc = io_uring_opcode_supported(probe, IORING_OP_SENDMSG_ZC)? TRUE:FALSE;
        io_uring_free_probe(probe);
    }
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
    if(yev_loop->dying) {
        free_dying_events(yev_loop);
    }
    io_uring_queue_exit(&yev_loop->ring);
    GBMEM_FREE(yev_loop->kept_sqes)
    GBMEM_FREE(yev_loop->kept_cqes)
    GBMEM_FREE(yev_loop)
}

/***************************************************************************
 *
 ***************************************************************************/
/***************************************************************************
 *  Free the event for real. Factored out of yev_destroy_event so the free
 *  can be deferred to callback_cqe when the event still has CQEs in flight.
 ***************************************************************************/
PRIVATE void really_free_yev_event(yev_event_t *yev_event)
{
    yev_loop_t *yev_loop = yev_event->yev_loop;
    hgobj gobj = yev_loop->yuno?yev_event->gobj:0;

    if(yev_event->destroy_requested) {
        if(yev_event->dying_prev) {
            yev_event->dying_prev->dying_next = yev_event->dying_next;
        } else {
            yev_loop->dying = yev_event->dying_next;
        }
        if(yev_event->dying_next) {
            yev_event->dying_next->dying_prev = yev_event->dying_prev;
        }
        yev_loop->dying_size--;
    }

    forget_kept(yev_loop, yev_event);

    GBUFFER_DECREF(yev_event->gbuf)
    GBMEM_FREE(yev_event->sock_info)
    GBMEM_FREE(yev_event->msghdr)

    switch((yev_type_t)yev_event->type) {
        case YEV_READ_TYPE:
        case YEV_WRITE_TYPE:
        case YEV_RECVMSG_TYPE:
        case YEV_SENDMSG_TYPE:
        case YEV_POLL_TYPE:
            break;
        case YEV_CONNECT_TYPE:
        case YEV_ACCEPT_TYPE:
        case YEV_TIMER_TYPE:
            if(yev_event->fd > 0) {
                take_back_submissions_on_fd(yev_loop, yev_event->fd);
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
 *  An event destroyed with completions still to come: it is not freed now,
 *  the kernel may still use it (its buffers, its msghdr, its gbuffer). Its
 *  callback is not called again, and the loop frees it at its last
 *  completion (callback_cqe). The loop keeps the list of them, so that
 *  yev_loop_destroy() frees those whose completions never came.
 ***************************************************************************/
PRIVATE void defer_free(yev_loop_t *yev_loop, yev_event_t *yev_event)
{
    if(yev_event->destroy_requested) {
        return;
    }
    yev_event->destroy_requested = TRUE;
    yev_event->gobj = 0;    // it may be destroyed before the event: no log names it
    yev_event->dying_prev = NULL;
    yev_event->dying_next = yev_loop->dying;
    if(yev_loop->dying) {
        yev_loop->dying->dying_prev = yev_event;
    }
    yev_loop->dying = yev_event;
    yev_loop->dying_size++;
}

/***************************************************************************
 *  A completion reaped by yev_loop_destroy(): no callback is called, the
 *  loop is ending. It only counts the operation done, and frees what can be
 *  freed.
 ***************************************************************************/
PRIVATE void reap_at_end(yev_loop_t *yev_loop, uint64_t user_data, uint32_t flags)
{
    yev_event_t *yev_event = (yev_event_t *)(uintptr_t)user_data;
    if(!yev_event || yev_event == &taken_back_event) {
        return;
    }
    if(flags & IORING_CQE_F_MORE) {
        yev_event->zc_notif_pending = TRUE;
    }
    if(flags & IORING_CQE_F_NOTIF) {
        yev_event->zc_notif_pending = FALSE;
    }
    if(yev_event->in_flight > 0 && !(flags & IORING_CQE_F_MORE)) {
        yev_event->in_flight--;
    }
    if(yev_event->in_flight <= 0 && yev_event->gbuf_release_pending) {
        yev_event->gbuf_release_pending = FALSE;
        GBUFFER_DECREF(yev_event->gbuf)
    }
    if(yev_event->destroy_requested && yev_event->in_flight <= 0) {
        really_free_yev_event(yev_event);
    }
}

/***************************************************************************
 *  The submissions of an event that the kernel never took (kept by the
 *  loop, or still in the submission queue), and the completions the loop
 *  made for it and did not deliver: none of them will complete
 ***************************************************************************/
PRIVATE unsigned untaken_of(yev_loop_t *yev_loop, yev_event_t *yev_event)
{
    uint64_t user_data = (uint64_t)(uintptr_t)yev_event;
    unsigned n = 0;
    for(unsigned i = 0; i < yev_loop->kept_sqes_size; i++) {
        if(yev_loop->kept_sqes[i].user_data == user_data) {
            n++;
        }
    }
    for(unsigned i = 0; i < yev_loop->kept_cqes_size; i++) {
        if(yev_loop->kept_cqes[i].user_data == user_data) {
            n++;
        }
    }
    if(io_uring_sq_ready(&yev_loop->ring) > 0) {
        n += queue_entries_of(yev_loop, yev_event, FALSE);
    }
    return n;
}

/***************************************************************************
 *  TRUE when a destroyed event waits for the notification of a zero-copy
 *  send: it comes when the kernel frees the packet, and cannot be canceled
 ***************************************************************************/
PRIVATE BOOL zc_notifications_pending(yev_loop_t *yev_loop)
{
    for(yev_event_t *yev_event = yev_loop->dying; yev_event; yev_event = yev_event->dying_next) {
        if(yev_event->zc_notif_pending) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  The loop ends with events destroyed whose completions have not come
 *  (a callback broke the loop, or the loop was stopped, before them). No
 *  one reaps them after this: without it they leak, with their gbuffers.
 *
 *  What the kernel never took is dropped. What it has is canceled, and the
 *  completions are reaped for YEV_DYING_WAIT_MS at most -- or for
 *  YEV_ZC_NOTIF_WAIT_MS while a zero-copy send waits for its notification,
 *  which a cancel does not reach. What is left then:
 *    - a zero-copy send still waiting for its notification is NOT freed,
 *      with a warning: the kernel may still read its gbuffer;
 *    - the rest is freed anyway, with an error: a completion that does not
 *      come after a cancel is a fault of the accounting (in_flight), not a
 *      slow kernel.
 ***************************************************************************/
PRIVATE void free_dying_events(yev_loop_t *yev_loop)
{
    struct io_uring *ring = &yev_loop->ring;
    yev_event_t *next;

    for(yev_event_t *yev_event = yev_loop->dying; yev_event; yev_event = next) {
        next = yev_event->dying_next;
        unsigned untaken = untaken_of(yev_loop, yev_event);
        if(untaken > 0) {
            forget_kept(yev_loop, yev_event);
            yev_event->in_flight -= (int)untaken;
        }
        if(yev_event->in_flight <= 0) {
            really_free_yev_event(yev_event);
        }
    }
    if(!yev_loop->dying) {
        return;
    }

    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if(!sqe) {
        io_uring_submit(ring);
        sqe = io_uring_get_sqe(ring);
    }
    if(sqe) {
        io_uring_prep_cancel(sqe, 0, IORING_ASYNC_CANCEL_ALL|IORING_ASYNC_CANCEL_ANY);
        io_uring_sqe_set_data(sqe, &taken_back_event);
    } else {
        /*
         *  Said here: the error at the end would otherwise blame the
         *  accounting of the completions, for a cancel never sent.
         */
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING,
            "msg",          "%s", "Submission queue full: the cancel of the events left is NOT submitted, their completions may not come",
            "events",       "%d", (int)yev_loop->dying_size,
            NULL
        );
    }

    uint64_t wait = start_msectimer(YEV_DYING_WAIT_MS);
    uint64_t zc_wait = start_msectimer(YEV_ZC_NOTIF_WAIT_MS);
    while(yev_loop->dying) {
        if(test_msectimer(wait) && (!zc_notifications_pending(yev_loop) || test_msectimer(zc_wait))) {
            break;
        }
        struct __kernel_timespec ts = { .tv_sec = 0, .tv_nsec = 10*1000*1000 };
        struct io_uring_cqe *cqe;
        int err = io_uring_submit_and_wait_timeout(ring, &cqe, 1, &ts, NULL);
        if(err < 0 && err != -ETIME && err != -EINTR) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING,
                "msg",          "%s", "io_uring_submit_and_wait_timeout() FAILED",
                "errno",        "%d", -err,
                "serrno",       "%s", strerror(-err),
                NULL
            );
            break;
        }
        unsigned head;
        unsigned reaped = 0;
        io_uring_for_each_cqe(ring, head, cqe) {
            reap_at_end(yev_loop, cqe->user_data, cqe->flags);
            reaped++;
        }
        io_uring_cq_advance(ring, reaped);
    }

    /*
     *  A zero-copy send whose notification has not come is NOT freed: the
     *  kernel may still read its gbuffer (the notification says it is
     *  done), and a gbuffer freed and reused would be sent with whatever
     *  was written into it. It is left to the end of the process, said.
     *  Up to this fix it was freed after YEV_DYING_WAIT_MS, as a fault of
     *  the accounting, which it is not.
     */
    unsigned zc_left = 0;
    for(yev_event_t *yev_event = yev_loop->dying; yev_event; yev_event = next) {
        next = yev_event->dying_next;
        if(!yev_event->zc_notif_pending) {
            continue;
        }
        if(yev_event->dying_prev) {
            yev_event->dying_prev->dying_next = yev_event->dying_next;
        } else {
            yev_loop->dying = yev_event->dying_next;
        }
        if(yev_event->dying_next) {
            yev_event->dying_next->dying_prev = yev_event->dying_prev;
        }
        yev_event->dying_prev = NULL;
        yev_event->dying_next = NULL;
        yev_loop->dying_size--;
        zc_left++;
    }
    if(zc_left > 0) {
        gobj_log_warning(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING,
            "msg",          "%s", "Loop destroyed with zero-copy sends whose notification did not come: NOT freed, the kernel may still read their gbuffer",
            "events",       "%d", (int)zc_left,
            "wait_ms",      "%d", YEV_ZC_NOTIF_WAIT_MS,
            NULL
        );
    }

    if(yev_loop->dying) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING,
            "msg",          "%s", "Loop destroyed with events whose completions did not come: freed",
            "events",       "%d", (int)yev_loop->dying_size,
            "cancel_submitted", "%d", sqe? 1: 0,
            "type",         "%s", yev_event_type_name(yev_loop->dying),
            "in_flight",    "%d", yev_loop->dying->in_flight,
            "wait_ms",      "%d", YEV_DYING_WAIT_MS,
            NULL
        );
        while(yev_loop->dying) {
            yev_event_t *yev_event = yev_loop->dying;
            yev_event->in_flight = 0;
            really_free_yev_event(yev_event);
        }
    }
}

/***************************************************************************
 *  Attach an event to an SQE and account for the CQE it will produce.
 *  Every event-carrying submit must go through here so in_flight stays
 *  balanced against the decrement in callback_cqe. Multishot is disabled,
 *  so each submitted SQE yields one CQE that ends it; a zero-copy send
 *  yields one more (IORING_CQE_F_MORE, then IORING_CQE_F_NOTIF), and
 *  callback_cqe does not count the first one (see the file header).
 ***************************************************************************/
PRIVATE void track_submit(yev_event_t *yev_event, struct io_uring_sqe *sqe)
{
    io_uring_sqe_set_data(sqe, yev_event);
    yev_event->in_flight++;
}

/***************************************************************************
 *  Hand the kept submissions to the kernel, in their order, as far as the
 *  queue takes them. The ones it does not take stay kept. Return what the
 *  last io_uring_submit() answered (0 when none was made).
 ***************************************************************************/
PRIVATE int submit_kept(yev_loop_t *yev_loop)
{
    int ret = 0;
    unsigned done = 0;
    while(done < yev_loop->kept_sqes_size) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(&yev_loop->ring);
        if(!sqe) {
            ret = io_uring_submit(&yev_loop->ring);
            sqe = io_uring_get_sqe(&yev_loop->ring);
            if(!sqe) {
                break;
            }
        }
        *sqe = yev_loop->kept_sqes[done];
        done++;
    }
    if(done == 0) {
        return ret;
    }
    yev_loop->kept_sqes_size -= done;
    if(yev_loop->kept_sqes_size > 0) {
        memmove(
            yev_loop->kept_sqes,
            yev_loop->kept_sqes + done,
            yev_loop->kept_sqes_size * sizeof(struct io_uring_sqe)
        );
    }
    return io_uring_submit(&yev_loop->ring);
}

/***************************************************************************
 *  TRUE when there are submissions the kernel has not taken: kept by the
 *  loop (get_sqe), or left in the queue by an io_uring_submit() that
 *  failed.
 ***************************************************************************/
static inline BOOL has_pending_submissions(yev_loop_t *yev_loop)
{
    return yev_loop->kept_sqes_size > 0 || io_uring_sq_ready(&yev_loop->ring) > 0;
}

/***************************************************************************
 *  Hand the kernel, at each cycle of the loop, what it did not take.
 *
 *  A submission that the kernel did not take is not only a kept one
 *  (get_sqe): an io_uring_submit() that fails while the queue has room
 *  leaves its entry in the queue, and so does the submit that hands the
 *  kept entries over (submit_kept). Nothing else would submit them again,
 *  and the loop would wait for a completion that cannot come.
 *
 *  Said once, when it starts (a WARNING), again when it lasts
 *  YEV_PENDING_CYCLES_ALARM cycles (an ERROR), and when it ends after
 *  that (an INFO).
 ***************************************************************************/
PRIVATE void submit_pending(yev_loop_t *yev_loop)
{
    int ret = 0;
    if(has_pending_submissions(yev_loop)) {
        if(!yev_loop->pending_told) {
            yev_loop->pending_told = TRUE;
            gobj_log_warning(yev_loop->yuno, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING,
                "msg",          "%s", "Submissions the kernel did not take: submitted again at each cycle",
                "kept",         "%d", (int)yev_loop->kept_sqes_size,
                "in_queue",     "%d", (int)io_uring_sq_ready(&yev_loop->ring),
                NULL
            );
        }
        if(yev_loop->kept_sqes_size > 0) {
            ret = submit_kept(yev_loop);
        } else {
            ret = io_uring_submit(&yev_loop->ring);
        }
    }

    if(!has_pending_submissions(yev_loop)) {
        if(yev_loop->pending_cycles >= YEV_PENDING_CYCLES_ALARM) {
            gobj_log_info(yev_loop->yuno, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING,
                "msg",          "%s", "Submissions taken by the kernel again",
                "cycles",       "%d", (int)yev_loop->pending_cycles,
                NULL
            );
        }
        yev_loop->pending_cycles = 0;
        yev_loop->pending_told = FALSE;
        return;
    }

    yev_loop->pending_cycles++;
    if(yev_loop->pending_cycles == YEV_PENDING_CYCLES_ALARM) {
        gobj_log_error(yev_loop->yuno, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING,
            "msg",          "%s", "Submissions not taken by the kernel for many cycles of the loop: their operations wait",
            "cycles",       "%d", (int)yev_loop->pending_cycles,
            "kept",         "%d", (int)yev_loop->kept_sqes_size,
            "in_queue",     "%d", (int)io_uring_sq_ready(&yev_loop->ring),
            "ret",          "%d", ret,
            "sret",         "%s", (ret<0)? strerror(-ret):"",
            NULL
        );
    }
}

/***************************************************************************
 *  A free submission queue entry. NULL only without memory (logged).
 *
 *  Every entry is submitted right after it is prepared, so a full queue
 *  holds entries that a submit did not hand to the kernel: they are
 *  flushed (io_uring_submit) and the entry is asked for once more. When
 *  the kernel still takes nothing -- io_uring_enter() failed: a CQ
 *  overflow answers EBUSY on older kernels until the completions are
 *  reaped, and they are reaped only when the callback that is submitting
 *  returns -- the entry is one KEPT by the loop: the caller prepares it
 *  as any other, and the loop hands it to the kernel at its next cycle
 *  (submit_pending), when the completions have made room. Waiting here for
 *  room would wait for completions that only this loop reaps.
 *
 *  While anything is kept, a new entry is kept after it: the kernel
 *  receives the submissions in the order they were made (two writes of
 *  one socket).
 ***************************************************************************/
PRIVATE struct io_uring_sqe *get_sqe(yev_loop_t *yev_loop)
{
    struct io_uring_sqe *sqe;
    int ret;
    if(yev_loop->kept_sqes_size == 0) {
        sqe = io_uring_get_sqe(&yev_loop->ring);
        if(sqe) {
            return sqe;
        }
        ret = io_uring_submit(&yev_loop->ring);
    } else {
        ret = submit_kept(yev_loop);
    }
    if(yev_loop->kept_sqes_size == 0) {
        sqe = io_uring_get_sqe(&yev_loop->ring);
        if(sqe) {
            return sqe;
        }
        /*
         *  Said once, when the loop starts keeping: not per submission
         */
        if(!yev_loop->pending_told) {
            yev_loop->pending_told = TRUE;
            gobj_log_warning(yev_loop->yuno, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING,
                "msg",          "%s", "Submission queue full and the kernel takes nothing: kept for the next cycle",
                "entries",      "%d", (int)yev_loop->entries,
                "ret",          "%d", ret,
                "sret",         "%s", (ret<0)? strerror(-ret):"",
                NULL
            );
        }
    }

    if(yev_loop->kept_sqes_size >= yev_loop->kept_sqes_max) {
        unsigned new_max = yev_loop->kept_sqes_max? yev_loop->kept_sqes_max*2 : 16;
        struct io_uring_sqe *new_sqes = gbmem_realloc(
            yev_loop->kept_sqes, new_max * sizeof(struct io_uring_sqe)
        );
        if(!new_sqes) {
            gobj_log_critical(yev_loop->yuno, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY,
                "msg",          "%s", "No memory to keep a submission",
                "kept",         "%d", (int)yev_loop->kept_sqes_size,
                NULL
            );
            return NULL;
        }
        yev_loop->kept_sqes = new_sqes;
        yev_loop->kept_sqes_max = new_max;
    }
    sqe = &yev_loop->kept_sqes[yev_loop->kept_sqes_size++];
    memset(sqe, 0, sizeof(*sqe));
    return sqe;
}

/***************************************************************************
 *  Walk the entries of the submission queue that the kernel has not taken
 *  (from its head to our tail) and count those of an event; with `take`,
 *  also turn each one into a NOP whose completion goes to taken_back_event
 *  and is not delivered. An entry already published to the kernel cannot
 *  be removed from the queue, but the kernel reads it only in the next
 *  io_uring_enter(), made by this loop (no SQPOLL), so rewriting it here
 *  is safe.
 *
 *  The entries are found by their EVENT (user_data), never by an fd: the
 *  fd of a stopped event is closed and its number may already belong to
 *  another one.
 ***************************************************************************/
PRIVATE unsigned queue_entries_of(yev_loop_t *yev_loop, yev_event_t *yev_event, BOOL take)
{
    struct io_uring *ring = &yev_loop->ring;
    struct io_uring_sq *sq = &ring->sq;
    unsigned shift = io_uring_sqe_shift(ring);
    uint64_t user_data = (uint64_t)(uintptr_t)yev_event;
    unsigned n = 0;
    for(unsigned i = io_uring_load_sq_head(ring); i != sq->sqe_tail; i++) {
        struct io_uring_sqe *sqe = &sq->sqes[(i & sq->ring_mask) << shift];
        if(sqe->user_data != user_data) {
            continue;
        }
        n++;
        if(take) {
            io_uring_initialize_sqe(sqe);
            io_uring_prep_nop(sqe);
            io_uring_sqe_set_data(sqe, &taken_back_event);
        }
    }
    return n;
}

/***************************************************************************
 *  A stop of an event whose submission the kernel has not taken yet: kept
 *  by the loop, or still in the submission queue. The kernel never saw it,
 *  so there is nothing to cancel there -- and handed over later it would
 *  run on an fd the stop has closed, maybe reused by then. It is taken
 *  back, and its completion is made here instead, as a cancel makes it
 *  (-ECANCELED), delivered at the next cycle of the loop: the event
 *  reaches its callback STOPPED, as after any cancel. One completion for
 *  the event, as a cancel gives. TRUE when there was one.
 *
 *  Without memory for that completion nothing is taken back (logged): the
 *  caller cancels the event in the kernel as usual.
 ***************************************************************************/
PRIVATE BOOL take_back_submissions(yev_loop_t *yev_loop, yev_event_t *yev_event)
{
    uint64_t user_data = (uint64_t)(uintptr_t)yev_event;
    unsigned in_kept = 0;
    for(unsigned i = 0; i < yev_loop->kept_sqes_size; i++) {
        if(yev_loop->kept_sqes[i].user_data == user_data) {
            in_kept++;
        }
    }
    unsigned in_queue = 0;
    if(io_uring_sq_ready(&yev_loop->ring) > 0) {
        in_queue = queue_entries_of(yev_loop, yev_event, FALSE);
    }
    if(in_kept + in_queue == 0) {
        return FALSE;
    }

    if(yev_loop->kept_cqes_size >= yev_loop->kept_cqes_max) {
        unsigned new_max = yev_loop->kept_cqes_max? yev_loop->kept_cqes_max*2 : 16;
        kept_cqe_t *new_cqes = gbmem_realloc(
            yev_loop->kept_cqes, new_max * sizeof(kept_cqe_t)
        );
        if(!new_cqes) {
            gobj_log_critical(yev_loop->yuno, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY,
                "msg",          "%s", "No memory to keep a completion: submission handed over as it is",
                NULL
            );
            return FALSE;
        }
        yev_loop->kept_cqes = new_cqes;
        yev_loop->kept_cqes_max = new_max;
    }

    if(in_queue > 0) {
        queue_entries_of(yev_loop, yev_event, TRUE);
    }
    if(in_kept > 0) {
        unsigned j = 0;
        for(unsigned i = 0; i < yev_loop->kept_sqes_size; i++) {
            if(yev_loop->kept_sqes[i].user_data != user_data) {
                yev_loop->kept_sqes[j++] = yev_loop->kept_sqes[i];
            }
        }
        yev_loop->kept_sqes_size = j;
    }

    /*
     *  Each entry was counted in flight (track_submit), and only one
     *  completion comes back
     */
    yev_event->in_flight -= (int)(in_kept + in_queue - 1);
    yev_loop->kept_cqes[yev_loop->kept_cqes_size].user_data = user_data;
    yev_loop->kept_cqes[yev_loop->kept_cqes_size].res = -ECANCELED;
    yev_loop->kept_cqes_size++;
    return TRUE;
}

/***************************************************************************
 *  The event of the first submission on `fd` that the kernel has not taken
 *  (kept by the loop, or in the submission queue), NULL if none
 ***************************************************************************/
PRIVATE yev_event_t *untaken_owner_of_fd(yev_loop_t *yev_loop, int fd)
{
    for(unsigned i = 0; i < yev_loop->kept_sqes_size; i++) {
        struct io_uring_sqe *sqe = &yev_loop->kept_sqes[i];
        if(sqe->fd == fd && sqe->user_data && sqe->user_data != (uint64_t)(uintptr_t)&taken_back_event) {
            return (yev_event_t *)(uintptr_t)sqe->user_data;
        }
    }
    if(io_uring_sq_ready(&yev_loop->ring) > 0) {
        struct io_uring *ring = &yev_loop->ring;
        struct io_uring_sq *sq = &ring->sq;
        unsigned shift = io_uring_sqe_shift(ring);
        for(unsigned i = io_uring_load_sq_head(ring); i != sq->sqe_tail; i++) {
            struct io_uring_sqe *sqe = &sq->sqes[(i & sq->ring_mask) << shift];
            if(sqe->fd == fd && sqe->user_data && sqe->user_data != (uint64_t)(uintptr_t)&taken_back_event) {
                return (yev_event_t *)(uintptr_t)sqe->user_data;
            }
        }
    }
    return NULL;
}

/***************************************************************************
 *  The loop is about to close the fd of an event (a connect, an accept, a
 *  timer: the event owns it). Other events may have submissions on the
 *  same fd that the kernel has not taken yet -- a write of C_TCP on the
 *  socket of its connect event, kept while io_uring_enter() refuses
 *  submissions. Handed over after the close, they would run on whatever
 *  file takes the number next: the bytes of an old connection sent to a
 *  new peer, and the write told it was sent. Each one is taken back before
 *  the close, and its event completes as canceled (STOPPED, -ECANCELED) at
 *  the next cycle of the loop, as a stop makes it. What the kernel HAS is
 *  not touched: the kernel holds its own reference to the file.
 *
 *  Without memory for the completion (take_back_submissions logs it) the
 *  submissions are dropped all the same: the event then waits for a
 *  completion that does not come, which is said, instead of sending its
 *  data to another file.
 ***************************************************************************/
PRIVATE void take_back_submissions_on_fd(yev_loop_t *yev_loop, int fd)
{
    if(fd < 0 || !has_pending_submissions(yev_loop)) {
        return;
    }

    yev_event_t *owner;
    while((owner = untaken_owner_of_fd(yev_loop, fd)) != NULL) {
        hgobj gobj = yev_loop->yuno? owner->gobj:0;
        if(take_back_submissions(yev_loop, owner)) {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING,
                "msg",          "%s", "An fd is closed with submissions of other events on it that the kernel did not take: taken back, completed as canceled",
                "fd",           "%d", fd,
                "type",         "%s", yev_event_type_name(owner),
                "p",            "%p", owner,
                NULL
            );
        } else {
            forget_kept(yev_loop, owner);
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING,
                "msg",          "%s", "An fd is closed with submissions of other events on it that the kernel did not take: dropped, the event will not complete",
                "fd",           "%d", fd,
                "type",         "%s", yev_event_type_name(owner),
                "p",            "%p", owner,
                NULL
            );
        }
    }
}

/***************************************************************************
 *  An event freed for real takes with it what the loop keeps of it, and
 *  what is left of it in the submission queue
 ***************************************************************************/
PRIVATE void forget_kept(yev_loop_t *yev_loop, yev_event_t *yev_event)
{
    uint64_t user_data = (uint64_t)(uintptr_t)yev_event;
    unsigned j = 0;
    for(unsigned i = 0; i < yev_loop->kept_sqes_size; i++) {
        if(yev_loop->kept_sqes[i].user_data != user_data) {
            yev_loop->kept_sqes[j++] = yev_loop->kept_sqes[i];
        }
    }
    yev_loop->kept_sqes_size = j;
    j = 0;
    for(unsigned i = 0; i < yev_loop->kept_cqes_size; i++) {
        if(yev_loop->kept_cqes[i].user_data != user_data) {
            yev_loop->kept_cqes[j++] = yev_loop->kept_cqes[i];
        }
    }
    yev_loop->kept_cqes_size = j;
    if(io_uring_sq_ready(&yev_loop->ring) > 0) {
        queue_entries_of(yev_loop, yev_event, TRUE);
    }
}

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
    if(yev_event == &taken_back_event) {
        // An entry taken back from the queue (take_back_submissions)
        return 0;
    }

    /*------------------------------------------------------------------*
     *  One CQE reaped for this event. If the event was destroyed while
     *  it still had ops in flight (destroy_requested), do NOT dispatch
     *  its callback or touch its state — just drain, and free it once
     *  the last outstanding CQE has been reaped. This is what makes a
     *  destroy-while-in-flight safe: the struct stays alive until its
     *  CQEs drain, so this very handler never lands on freed memory.
     *------------------------------------------------------------------*/
    /*
     *  A CQE with IORING_CQE_F_MORE is not the last of its operation: a
     *  zero-copy send posts its notification (IORING_CQE_F_NOTIF) later,
     *  and the kernel reads the buffer until then
     */
    if(cqe->flags & IORING_CQE_F_MORE) {
        yev_event->zc_notif_pending = TRUE;
    }
    if(cqe->flags & IORING_CQE_F_NOTIF) {
        yev_event->zc_notif_pending = FALSE;
    }
    if(yev_event->in_flight > 0 && !(cqe->flags & IORING_CQE_F_MORE)) {
        yev_event->in_flight--;
    }
    if(yev_event->in_flight <= 0 && yev_event->gbuf_release_pending) {
        /*
         *  The kernel is done with the gbuffer of a stopped event (see
         *  yev_stop_event): released before the callback, which sees the
         *  event without gbuffer, as when the stop released it
         */
        yev_event->gbuf_release_pending = FALSE;
        GBUFFER_DECREF(yev_event->gbuf)
    }
    if(yev_event->destroy_requested) {
        if(yev_event->in_flight <= 0) {
            really_free_yev_event(yev_event);
        }
        return 0;
    }

    if(cqe->flags & IORING_CQE_F_NOTIF) {
        /*
         *  The buffer of a zero-copy send is released. The callback was told
         *  the result at the first CQE: it is not called again. The event may
         *  already run another send, or be stopped: its state is not touched.
         */
        if(gobj_global_trace_level() & TRACE_URING) {
            gobj_log_debug(yev_loop->running && yev_loop->yuno? yev_event->gobj:0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_YEV_LOOP,
                "msg",          "%s", "callback_cqe zero-copy notification",
                "msg2",         "%s", "💥💥💥💥⏪ callback_cqe zero-copy notification",
                "type",         "%s", yev_event_type_name(yev_event),
                "yev_state",    "%s", yev_get_state_name(yev_event),
                "p",            "%p", yev_event,
                "in_flight",    "%d", yev_event->in_flight,
                "cqe->res",     "%d", (int)cqe->res,
                NULL
            );
        }
        return 0;
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
                "type_",        "%d", (int)yev_event->type,
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
            if(cqe_res >= 0) {
                /*
                 *  When canceling it could arrive events type with res >= 0
                 *  -2 ENOENT is because the cancelling has failed
                 *  Wait to one negative
                 */
                /*
                 *  Mark this request as processed
                 */
                return 0;
            }
            if(cqe_res < 0 ) {
                /*
                 *  HACK this error is information of disconnection cause.
                 */
                yev_set_state(yev_event, YEV_ST_STOPPED);
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
                    "msgset",       "%s", MSGSET_LIBURING,
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
        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING,
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
    /*
     *  Mark the event "in dispatch" so a callback that calls yev_destroy_event()
     *  on its own event defers the free (sets destroy_requested) instead of
     *  freeing synchronously. Otherwise the post-callback re-arm below would
     *  dereference freed memory (use-after-free) when this was the last in-flight CQE.
     */
    yev_event->in_dispatch = TRUE;
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
                if(ret == 0 && !yev_event->destroy_requested && yev_loop->running &&
                        yev_event->state == YEV_ST_IDLE &&
                        !(yev_event->flag & YEV_FLAG_ACCEPT_DUP2)) {
                    if(!gobj || (gobj && gobj_is_running(gobj))) {
                        /*
                         *  Rearm accept event
                         */
                        struct io_uring_sqe *sqe = get_sqe(yev_loop);
                        if(!sqe) {
                            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_LIBURING,
                                "msg",          "%s", "No memory to keep a submission: accept event NOT re-armed",
                                "type",         "%s", yev_event_type_name(yev_event),
                                "yev_state",    "%s", yev_get_state_name(yev_event),
                                "fd",           "%d", yev_get_fd(yev_event),
                                "p",            "%p", yev_event,
                                NULL
                            );
                            break;
                        }
                        track_submit(yev_event, sqe);
                        io_uring_prep_accept(
                            sqe,
                            yev_event->fd,
                            (struct sockaddr *)&yev_event->sock_info->addr,
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
                if(cqe_res >= 0 && yev_event->type == YEV_RECVMSG_TYPE) {
                    // The length of the peer address the kernel wrote in sock_info->addr
                    yev_event->sock_info->addrlen = yev_event->msghdr->msg_namelen;
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

                if(ret == 0 && !yev_event->destroy_requested && yev_loop->running &&
                        yev_event->state == YEV_ST_IDLE &&
                        (yev_event->flag & YEV_FLAG_TIMER_PERIODIC)) {
                    if(!gobj || (gobj && gobj_is_running(gobj))) {
                        /*
                         *  Rearm periodic timer event
                         */
                        struct io_uring_sqe *sqe = get_sqe(yev_loop);
                        if(!sqe) {
                            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_LIBURING,
                                "msg",          "%s", "No memory to keep a submission: periodic timer NOT re-armed",
                                "type",         "%s", yev_event_type_name(yev_event),
                                "yev_state",    "%s", yev_get_state_name(yev_event),
                                "fd",           "%d", yev_get_fd(yev_event),
                                "p",            "%p", yev_event,
                                NULL
                            );
                            break;
                        }
                        track_submit(yev_event, sqe);
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

    /*
     *  Dispatch (callback + re-arm) finished: it is now safe to touch / free
     *  the event. If the callback requested destruction while in dispatch,
     *  yev_destroy_event deferred the free to here. If no op was re-armed
     *  (in_flight drained to 0) free now; otherwise the deferred free happens
     *  when the last outstanding CQE drains (see top of callback_cqe).
     */
    yev_event->in_dispatch = FALSE;
    if(yev_event->destroy_requested && yev_event->in_flight <= 0) {
        really_free_yev_event(yev_event);
        return ret;
    }

    return ret;
}

/***************************************************************************
 *  Deliver the completions the loop made itself (take_back_submissions),
 *  oldest first, as the kernel's are: through callback_cqe(). -1 when a
 *  callback asks to break the loop: the rest wait for the next cycle.
 ***************************************************************************/
PRIVATE int deliver_kept_cqes(yev_loop_t *yev_loop)
{
    while(yev_loop->kept_cqes_size > 0) {
        kept_cqe_t kept = yev_loop->kept_cqes[0];
        yev_loop->kept_cqes_size--;
        if(yev_loop->kept_cqes_size > 0) {
            memmove(
                yev_loop->kept_cqes,
                yev_loop->kept_cqes + 1,
                yev_loop->kept_cqes_size * sizeof(kept_cqe_t)
            );
        }
        struct io_uring_cqe cqe;
        memset(&cqe, 0, sizeof(cqe));
        cqe.user_data = kept.user_data;
        cqe.res = kept.res;
        if(callback_cqe(yev_loop, &cqe) < 0) {
            return -1;
        }
    }
    return 0;
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
            "msgset",       "%s", MSGSET_LIBURING,
            "msg",          "%s", "ALREADY running the main event loop",
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

        /*
         *  What the kernel did not take when it was submitted (kept by
         *  get_sqe, or left in the queue by a failed submit), now that the
         *  completions of the last cycle made room, and the completions of
         *  what was stopped before it took them
         */
        if(yev_loop->pending_told || has_pending_submissions(yev_loop)) {
            submit_pending(yev_loop);
        }
        if(yev_loop->kept_cqes_size > 0) {
            if(deliver_kept_cqes(yev_loop) < 0) {
                yev_loop->running = false;
                break;
            }
        }

        /*
         *  Deliver what was posted with gobj_post_event().
         *
         *  Here, at the top of the cycle, and not after the completions: a
         *  event posted before the loop even started -- in mt_play(), say --
         *  would otherwise wait for the first completion to arrive, which in
         *  a yuno with nothing else armed is never.
         *
         *  It delivers a snapshot, so an action that posts the next message
         *  leaves it for the following cycle. That is the whole reason the
         *  peek below exists.
         */
        gobj_deliver_posted_events();
        if(!yev_loop->running) {
            break;
        }

        if(gobj_posted_events_size() > 0 || yev_loop->kept_cqes_size > 0) {
            /*
             *  There is work waiting: do not block on the ring, take a
             *  completion if one is ready and go back to the queue if not.
             *  Blocking here is what would turn a posted event into one
             *  delivered hours later, when some timer happened to fire.
             *
             *  So is a completion the loop made itself: a posted action
             *  that stopped an event whose submission the kernel had not
             *  taken (take_back_submissions). Up to 7.25.4 its STOPPED
             *  waited here for an unrelated completion.
             */
            err = io_uring_peek_cqe(&yev_loop->ring, &cqe);
            if(err == -EAGAIN) {
                continue;
            }
        } else if(has_pending_submissions(yev_loop)) {
            /*
             *  Submissions are still pending: the kernel takes them once
             *  completions are reaped, and if none comes, a short wait
             *  tries again. Not the timeout of the loop: its callback is
             *  not called.
             */
            struct __kernel_timespec retry = { .tv_sec = 0, .tv_nsec = 10*1000*1000 };
            err = io_uring_wait_cqe_timeout(&yev_loop->ring, &cqe, &retry);
            if(err == -ETIME) {
                continue;
            }
        } else if(timeout_in_seconds > 0) {
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
                "msgset",       "%s", MSGSET_LIBURING,
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
            "msgset",       "%s", MSGSET_LIBURING,
            "msg",          "%s", "ALREADY running the once event loop",
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

    /*
     *  One turn of the loop also means one delivery of what was posted:
     *  the callers of this function (service management, shutdown) use it
     *  to let pending work settle, and a posted event IS pending work.
     *  So is a submission the kernel did not take (get_sqe), and the
     *  completion of one stopped before it took it.
     */
    gobj_deliver_posted_events();

    BOOL broken = FALSE;
    if(yev_loop->pending_told || has_pending_submissions(yev_loop)) {
        submit_pending(yev_loop);
    }
    if(yev_loop->kept_cqes_size > 0) {
        if(deliver_kept_cqes(yev_loop) < 0 && yev_loop->stopping) {
            broken = TRUE;
        }
    }

    cqe = 0;
    while(!broken && io_uring_peek_cqe(&yev_loop->ring, &cqe)==0) {
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
        sqe = get_sqe(yev_loop);
        if(!sqe) {
            /*
             *  Not stopping: a later call tries again
             */
            yev_loop->stopping = FALSE;
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING,
                "msg",          "%s", "No memory to keep a submission: loop NOT stopped",
                NULL
            );
            return -1;
        }
        io_uring_sqe_set_data(sqe, NULL);  // HACK CQE event without data is loop ending
        io_uring_prep_cancel(sqe, 0, IORING_ASYNC_CANCEL_ALL|IORING_ASYNC_CANCEL_ANY);
        io_uring_submit(&yev_loop->ring);
    }

    return 0;
}

/***************************************************************************
 *  Exit of the main event loop
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
/***************************************************************************
 *  Return the well-known default port for a URL schema.
 *  Returns NULL when no default applies (caller must have an explicit port).
 ***************************************************************************/
PRIVATE const char *default_port_for_schema(const char *schema)
{
    SWITCHS(schema) {
        ICASES("http")
        ICASES("ws")
            return "80";
        ICASES("https")
        ICASES("wss")
            return "443";
        ICASES("mqtt")
            return "1883";
        ICASES("mqtts")
            return "8883";
        ICASES("smtps")
            return "465";
        DEFAULTS
            return NULL;
    } SWITCHS_END;
    return NULL;
}

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
        ICASES("smtps")
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
                "msgset",       "%s", MSGSET_LIBURING,
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
        yev_event->gbuf_release_pending = FALSE;    // in use again
        return 0;
    }
    if(yev_event->gbuf && yev_event->in_flight > 0) {
        /*
         *  The kernel may still use the current gbuffer (see
         *  yev_stop_event): it is released at the last completion
         */
        if(!gbuf) {
            yev_event->gbuf_release_pending = TRUE;
            return 0;
        }
        yev_loop_t *yev_loop = yev_event->yev_loop;
        gobj_log_error(yev_loop->yuno? yev_event->gobj:0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING,
            "msg",          "%s", "Cannot replace the gbuffer of an event with an operation in the kernel",
            "type",         "%s", yev_event_type_name(yev_event),
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "gbuffer",      "%p", yev_event->gbuf,
            NULL
        );
        GBUFFER_DECREF(gbuf)    // owned
        return -1;
    }
    if(!gbuf) {
        GBUFFER_DECREF(yev_event->gbuf)
    }
    if(yev_event->gbuf) {
        gobj_log_warning(yev_event->gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING,
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
            "msgset",       "%s", MSGSET_LIBURING,
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
            "msgset",       "%s", MSGSET_LIBURING,
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

    /*
     *  Started again before the last completion of its stop: the gbuffer
     *  is in use again, it is not released
     */
    yev_event->gbuf_release_pending = FALSE;

    /*-------------------------------*
     *      Summit sqe
     *-------------------------------*/
    switch((yev_type_t)yev_event->type) {
        case YEV_CONNECT_TYPE: // Summit sqe
            {
                if(!yev_event->sock_info || yev_event->sock_info->addrlen <= 0) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING,
                        "msg",          "%s", "Cannot start event: connect addr NULL",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }

                struct io_uring_sqe *sqe = get_sqe(yev_loop);
                if(!sqe) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING,
                        "msg",          "%s", "No memory to keep a submission: event NOT started",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }
                track_submit(yev_event, sqe);
                /*
                 *  Use the file descriptor fd to start connecting to the destination
                 *  described by the socket address at addr and of structure length addrlen.
                 */
                io_uring_prep_connect(
                    sqe,
                    yev_event->fd,
                    (struct sockaddr *)&yev_event->sock_info->addr,
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
                        "msgset",       "%s", MSGSET_LIBURING,
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
                    struct io_uring_sqe *sqe = get_sqe(yev_loop);
                    if(!sqe) {
                        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_LIBURING,
                            "msg",          "%s", "No memory to keep a submission: event NOT started",
                            "event_type",   "%s", yev_event_type_name(yev_event),
                            "yev_state",    "%s", yev_get_state_name(yev_event),
                            "p",            "%p", yev_event,
                            NULL
                        );
                        return -1;
                    }
                    track_submit(yev_event, sqe);

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
                            (struct sockaddr *)&yev_event->sock_info->addr,
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
                        "msgset",       "%s", MSGSET_LIBURING,
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
                        "msgset",       "%s", MSGSET_LIBURING,
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
                        "msgset",       "%s", MSGSET_LIBURING,
                        "msg",          "%s", "Cannot start event: gbuffer WITHOUT data to write",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        "gbuf_label",   "%s", gbuffer_getlabel(yev_event->gbuf),
                        NULL
                    );
                    return -1;
                }

                struct io_uring_sqe *sqe = get_sqe(yev_loop);
                if(sqe) {
                    track_submit(yev_event, sqe);
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
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING,
                        "msg",          "%s", "No memory to keep a submission: event NOT started",
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
                        "msgset",       "%s", MSGSET_LIBURING,
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
                        "msgset",       "%s", MSGSET_LIBURING,
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
                        "msgset",       "%s", MSGSET_LIBURING,
                        "msg",          "%s", "Cannot start event: gbuffer WITHOUT space to read",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        "gbuf_label",   "%s", gbuffer_getlabel(yev_event->gbuf),
                        NULL
                    );
                    return -1;
                }

                struct io_uring_sqe *sqe = get_sqe(yev_loop);
                if(!sqe) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING,
                        "msg",          "%s", "No memory to keep a submission: event NOT started",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }
                track_submit(yev_event, sqe);
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
                        "msgset",       "%s", MSGSET_LIBURING,
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
                        "msgset",       "%s", MSGSET_LIBURING,
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
                        "msgset",       "%s", MSGSET_LIBURING,
                        "msg",          "%s", "Cannot start event: gbuffer WITHOUT data to write",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        "gbuf_label",   "%s", gbuffer_getlabel(yev_event->gbuf),
                        NULL
                    );
                    return -1;
                }

                if(!yev_event->msghdr->msg_name || yev_event->msghdr->msg_namelen <= 0 ||
                        yev_event->msghdr->msg_namelen > sizeof(struct sockaddr_storage)) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING,
                        "msg",          "%s", "Cannot start event: sendmsg addr NULL or bad addr length",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        "addrlen",      "%d", (int)yev_event->msghdr->msg_namelen,
                        NULL
                    );
                    return -1;
                }

                struct io_uring_sqe *sqe = get_sqe(yev_loop);
                if(sqe) {
                    track_submit(yev_event, sqe);

                    yev_event->iov.iov_base = gbuffer_cur_rd_pointer(yev_event->gbuf);
                    yev_event->iov.iov_len = gbuffer_leftbytes(yev_event->gbuf);

                    if(yev_loop->sendmsg_zc) {
                        io_uring_prep_sendmsg_zc(
                            sqe,
                            yev_event->fd,
                            yev_event->msghdr,
                            0
                        );
                    } else {
                        io_uring_prep_sendmsg(
                            sqe,
                            yev_event->fd,
                            yev_event->msghdr,
                            0
                        );
                    }
                    io_uring_submit(&yev_loop->ring);
                    yev_set_state(yev_event, YEV_ST_RUNNING);
                } else {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING,
                        "msg",          "%s", "No memory to keep a submission: event NOT started",
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
                        "msgset",       "%s", MSGSET_LIBURING,
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
                        "msgset",       "%s", MSGSET_LIBURING,
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
                        "msgset",       "%s", MSGSET_LIBURING,
                        "msg",          "%s", "Cannot start event: gbuffer WITHOUT space to read",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        "gbuf_label",   "%s", gbuffer_getlabel(yev_event->gbuf),
                        NULL
                    );
                    return -1;
                }

                struct io_uring_sqe *sqe = get_sqe(yev_loop);
                if(!sqe) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING,
                        "msg",          "%s", "No memory to keep a submission: event NOT started",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }
                track_submit(yev_event, sqe);

                yev_event->iov.iov_base = gbuffer_cur_wr_pointer(yev_event->gbuf);
                yev_event->iov.iov_len = gbuffer_freebytes(yev_event->gbuf);
                /*
                 *  The kernel sets msg_namelen to the length of the peer
                 *  address it writes: give it all the room at each receive
                 */
                yev_event->msghdr->msg_namelen = sizeof(yev_event->sock_info->addr);

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
                        "msgset",       "%s", MSGSET_LIBURING,
                        "msg",          "%s", "Cannot start event: fd negative",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }

                struct io_uring_sqe *sqe = get_sqe(yev_loop);
                if(!sqe) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING,
                        "msg",          "%s", "No memory to keep a submission: event NOT started",
                        "event_type",   "%s", yev_event_type_name(yev_event),
                        "yev_state",    "%s", yev_get_state_name(yev_event),
                        "p",            "%p", yev_event,
                        NULL
                    );
                    return -1;
                }
                track_submit(yev_event, sqe);
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
                "msgset",       "%s", MSGSET_LIBURING,
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
            "msgset",       "%s", MSGSET_LIBURING,
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
                "msgset",       "%s", MSGSET_SYSTEM,
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
            "msgset",       "%s", MSGSET_LIBURING,
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
                    "msgset",       "%s", MSGSET_LIBURING,
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
    struct io_uring_sqe *sqe = get_sqe(yev_loop);
    if(!sqe) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING,
            "msg",          "%s", "No memory to keep a submission: timer NOT started",
            "type",         "%s", yev_event_type_name(yev_event),
            "yev_state",    "%s", yev_get_state_name(yev_event),
            "timeout_ms",   "%d", (int)timeout_ms,
            "fd",           "%d", yev_get_fd(yev_event),
            "p",            "%p", yev_event,
            NULL
        );
        return -1;
    }
    timerfd_settime(yev_event->fd, 0, &delta, NULL);
    track_submit(yev_event, sqe);
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
 *  What a stop gives up, once the stop is going to happen: the gbuffer and
 *  the fd of a connect or a timer.
 *
 *  An operation still in the kernel (a read, write, recvmsg or send that
 *  runs, or a zero-copy send whose notification has not arrived) may still
 *  write into the gbuffer or read from it until its completion: a cancel
 *  is not done when it is submitted. The event keeps the gbuffer, and
 *  callback_cqe releases it at the last completion of the event.
 *
 *  Called only when the stop goes on: a stop refused for lack of memory
 *  (get_sqe) leaves the event RUNNING, and it must keep both.
 ***************************************************************************/
PRIVATE void release_on_stop(yev_event_t *yev_event, hgobj gobj, uint32_t trace_level)
{
    if(yev_event->gbuf && yev_event->in_flight > 0) {
        yev_event->gbuf_release_pending = TRUE;
    } else {
        GBUFFER_DECREF(yev_event->gbuf)
    }

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
                take_back_submissions_on_fd(yev_event->yev_loop, yev_event->fd);
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
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int yev_stop_event(yev_event_h yev_event_) // IDEMPOTENT close fd (timer, connect)
{
    yev_event_t *yev_event = (yev_event_t *)yev_event_;

    /*------------------------*
     *  Check parameters
     *------------------------*/
    if(!yev_event) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING,
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

    /*-------------------------------*
     *      Checking state
     *-------------------------------*/
    yev_state_t cur_state = yev_get_state(yev_event);
    switch(cur_state) {
        case YEV_ST_RUNNING:
            /*
             *  Its submission may not be taken by the kernel yet (kept by
             *  get_sqe, or still in the queue): the kernel never saw it, so
             *  it is not canceled there, it is taken back, and the loop
             *  completes it as a cancel
             */
            if(has_pending_submissions(yev_loop) && take_back_submissions(yev_loop, yev_event)) {
                release_on_stop(yev_event, gobj, trace_level);
                yev_set_state(yev_event, YEV_ST_CANCELING);
                break;
            }
            sqe = get_sqe(yev_loop);
            if(!sqe) {
                /*
                 *  Still RUNNING: its operation is in the kernel,
                 *  uncanceled, with its gbuffer and its fd. Up to 7.25.4
                 *  both were given up before this point: the operation
                 *  then completed normally, with its gbuffer released
                 *  and its fd closed.
                 */
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_LIBURING,
                    "msg",          "%s", "No memory to keep a submission: event NOT canceled",
                    "type",         "%s", yev_event_type_name(yev_event),
                    "yev_state",    "%s", yev_get_state_name(yev_event),
                    "fd",           "%d", yev_get_fd(yev_event),
                    "p",            "%p", yev_event,
                    NULL
                );
                return -1;
            }
            release_on_stop(yev_event, gobj, trace_level);
            track_submit(yev_event, sqe);
            io_uring_prep_cancel(sqe, yev_event, 0);
            io_uring_submit(&yev_loop->ring);
            yev_set_state(yev_event, YEV_ST_CANCELING);
            break;

        case YEV_ST_IDLE:
            release_on_stop(yev_event, gobj, trace_level);
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
            release_on_stop(yev_event, gobj, trace_level);   // IDEMPOTENT
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
            "msgset",       "%s", MSGSET_LIBURING,
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

    if(yev_event->destroy_requested) {
        // Already being destroyed: its deferred free is pending in callback_cqe.
        return;
    }

    if(yev_state == YEV_ST_RUNNING) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_s, yev_event->flag);
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING,
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
        yev_stop_event(yev_event);  // submits a cancel -> CANCELING, bumps in_flight
    }

    /*-----------------------------------------------------------------*
     *      Free
     *  An event with a CQE outstanding (it was RUNNING/CANCELING, a cancel
     *  was just submitted, or a zero-copy notification is still to come) is
     *  not freed here: a later completion would re-enter callback_cqe on
     *  freed memory, and the kernel may still read or write its buffers.
     *  It is marked dying, and freed at its last completion.
     *
     *  Also when the loop is not running (teardown, after yev_loop_run
     *  returned): the kernel still has the operation. Up to 7.25.4 the
     *  event was freed at once in that case, to not leak it; now
     *  yev_loop_destroy() frees the dying events whose completions did not
     *  come (free_dying_events).
     *-----------------------------------------------------------------*/
    /*
     *  If we are inside callback_cqe's dispatch of this very event (a callback
     *  destroying its own event), never free synchronously: the caller is
     *  callback_cqe and it will still dereference the event in the re-arm block
     *  and the dispatch tail. Defer; callback_cqe frees it once dispatch ends
     *  and any re-armed op has drained.
     */
    if(yev_event->in_dispatch || yev_event->in_flight > 0) {
        defer_free(yev_loop, yev_event);
        return;
    }

    really_free_yev_event(yev_event);
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
            "msgset",       "%s", MSGSET_SYSTEM,
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
 *  getaddrinfo() is synchronous, and the loop calls it while arming a
 *  connect or a listen. So a slow resolver does not merely delay this one
 *  socket: it stops the whole process — every gobj, every timer, every
 *  pending completion — for as long as the name takes to resolve.
 *
 *  That failure is invisible by construction. Resolution still succeeds,
 *  the yuno still comes up, and all that shows downstream is that
 *  everything was late. Naming it here, with the number, is what turns
 *  "the yuno is slow" into "resolving X blocked us for N ms".
 *
 *  Real symptom (7.8.x): a node whose /etc/resolv.conf listed a
 *  black-holed nameserver first paid ~6 s per lookup, and a yuno building
 *  25 channels spent minutes in start up without ever saying why.
 ***************************************************************************/
#define SLOW_RESOLUTION_MSEC    1000

PRIVATE void warn_if_slow_resolution(
    hgobj gobj,
    const char *funcname,
    const char *host,
    uint64_t t0
)
{
    uint64_t elapsed = time_in_milliseconds_monotonic() - t0;
    if(elapsed < SLOW_RESOLUTION_MSEC) {
        return;
    }
    gobj_log_warning(gobj, 0,
        "function",     "%s", funcname,
        "msgset",       "%s", MSGSET_SYSTEM,
        "msg",          "%s", "getaddrinfo() BLOCKED the event loop",
        "host",         "%s", host?host:"",
        "msec",         "%lu", (unsigned long)elapsed,
        NULL
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC yev_event_h yev_create_connect_event( // create the socket to connect in yev_event->fd
    yev_loop_h yev_loop_,
    yev_callback_t callback, // if return -1 the loop in yev_loop_run will break;
    const char *dst_url,
    const char *src_url,    /* local bind: "host:port", "[ipv6]:port" or "schema://host:port" */
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
    const char *src_url,    /* local bind: "host:port", "[ipv6]:port" or "schema://host:port" */
    int ai_family,          /* default: AF_UNSPEC, Allow IPv4 or IPv6  (AF_INET AF_INET6) */
    int ai_flags            /* default: AI_V4MAPPED | AI_ADDRCONFIG */
) {
    yev_event_t *yev_event = (yev_event_t *)yev_event_;
    if(!yev_event) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING,
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
    host_without_brackets(dst_host);

    /*
     *  If no explicit port in the URL, use the well-known default for the schema
     *  (e.g. 443 for https, 80 for http).
     */
    if(empty_string(dst_port)) {
        const char *def = default_port_for_schema(schema);
        if(def) {
            snprintf(dst_port, sizeof(dst_port), "%s", def);
        }
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
    uint64_t t_resolv = time_in_milliseconds_monotonic();
    ret = getaddrinfo(
        dst_host,
        dst_port,
        &hints,
        &results
    );
    warn_if_slow_resolution(gobj, __FUNCTION__, dst_host, t_resolv);
    if(ret != 0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING,
            "msg",          "%s", "getaddrinfo() FAILED",
            "url",          "%s", dst_url,
            "host",         "%s", dst_host,
            "port",         "%s", dst_port,
            "gai_ret",      "%d", ret,
            "gai_strerror", "%s", gai_strerror(ret),
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
                "msgset",       "%s", MSGSET_LIBURING,
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
            int ret_bind = bind_src_url(gobj, fd, src_url, rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if(ret_bind == -2) {
                /*
                 *  The src_url has no address in the family of this
                 *  destination address (localhost is ::1 first, the src
                 *  127.0.0.1): the next address is tried. Up to this fix
                 *  the connect ended here, and never tried the address of
                 *  the other family.
                 */
                close(fd);
                fd = -1;
                continue;
            }
            if(ret_bind < 0) {
                // Error already logged: a bad src_url is bad for every address
                close(fd);
                freeaddrinfo(results);
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
            "msgset",       "%s", MSGSET_LIBURING,
            "msg",          "%s", "Cannot get addr to connect",
            "url",          "%s", dst_url,
            "host",         "%s", dst_host,
            "port",         "%s", dst_port,
            "src_url",      "%s", src_url? src_url: "",
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
                "msgset",       "%s", MSGSET_LIBURING,
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
    host_without_brackets(host);

    /*
     *  If no explicit port in the URL, use the well-known default for the schema
     *  (e.g. 443 for https, 80 for http).
     */
    if(empty_string(port)) {
        const char *def = default_port_for_schema(schema);
        if(def) {
            snprintf(port, sizeof(port), "%s", def);
        }
    }

#ifdef __linux__
    int somaxconn = get_net_core_somaxconn();
    if(somaxconn < backlog) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
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
    uint64_t t_resolv = time_in_milliseconds_monotonic();
    ret = getaddrinfo(
        host,
        port,
        &hints,
        &results
    );
    warn_if_slow_resolution(gobj, __FUNCTION__, host, t_resolv);
    if(ret != 0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING,
            "msg",          "%s", "getaddrinfo() FAILED",
            "url",          "%s", listen_url,
            "host",         "%s", host,
            "port",         "%s", port,
            "gai_ret",      "%d", ret,
            "gai_strerror", "%s", gai_strerror(ret),
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
                "msgset",       "%s", MSGSET_LIBURING,
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
                "msgset",       "%s", MSGSET_LIBURING,
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
                    "msgset",       "%s", MSGSET_LIBURING,
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
            "msgset",       "%s", MSGSET_LIBURING,
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
            "msgset",       "%s", MSGSET_LIBURING,
            "msg",          "%s", "Cannot copy ai_addr to sock_info",
            "url",          "%s", listen_url,
            "host",         "%s", host,
            "port",         "%s", port,
            "rp found",     "%d", rp?1:0,
            "ai_addrlen",   "%d", (int)rp->ai_addrlen,
            "addr size",    "%d", (int)sizeof(yev_event->sock_info->addr),
            NULL
        );
        freeaddrinfo(results);
        yev_destroy_event(yev_event);
        return NULL;
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
    const struct sockaddr *dst_addr,
    socklen_t dst_addrlen
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

    yev_event->msghdr->msg_name = (void *)dst_addr;
    yev_event->msghdr->msg_namelen = dst_addrlen;

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
 *  Bind the socket of a connect to its local address, src_url:
 *  "host:port", "[ipv6]:port" or "schema://host:port". The host is
 *  resolved in the family of the socket. An empty host binds any address
 *  of the family, a port 0 (or none) any port.
 *  Up to 7.25.4 the src_url was never parsed, and the socket was bound to
 *  a port of the kernel's choice: the src_url was ignored without a word.
 *  Return 0 bound, -2 (silent) the src has no address in this family (the
 *  caller tries its next destination address), -1 (logged) a bad src_url
 *  or a bind() that fails.
 ***************************************************************************/
PRIVATE int bind_src_url(
    hgobj gobj,
    int fd,
    const char *src_url,
    int ai_family,
    int ai_socktype,
    int ai_protocol
)
{
    char url[PATH_MAX];
    char src_host[120];
    char src_port[10];

    /*
     *  parse_url() splits "host:port" at the first colon, so an IPv6
     *  literal is parsed as the authority of a url
     */
    int n;
    if(strstr(src_url, "://")) {
        n = snprintf(url, sizeof(url), "%s", src_url);
    } else {
        n = snprintf(url, sizeof(url), "src://%s", src_url);
    }
    if(n < 0 || (size_t)n >= sizeof(url)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Bad src_url: cannot bind the connect",
            "src_url",      "%s", src_url,
            "reason",       "%s", "too long",
            NULL
        );
        return -1;
    }
    int ret = parse_url(
        gobj,
        url,
        0, 0,
        src_host, sizeof(src_host),
        src_port, sizeof(src_port),
        0, 0,
        0, 0,
        false
    );
    size_t len = strlen(src_host);
    if(ret < 0 || (len > 0 && src_host[0] == '[' && src_host[len-1] != ']')) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Bad src_url: cannot bind the connect",
            "src_url",      "%s", src_url,
            NULL
        );
        return -1;
    }
    host_without_brackets(src_host);

    struct addrinfo hints = {
        .ai_family = ai_family,
        .ai_socktype = ai_socktype,
        .ai_protocol = ai_protocol,
        .ai_flags = AI_PASSIVE,
    };
    struct addrinfo *res;
    uint64_t t_resolv_src = time_in_milliseconds_monotonic();
    ret = getaddrinfo(
        empty_string(src_host)? NULL:src_host,
        empty_string(src_port)? "0":src_port,
        &hints,
        &res
    );
    warn_if_slow_resolution(gobj, __FUNCTION__, src_host, t_resolv_src);
    BOOL not_in_family = (ret == EAI_NONAME || ret == EAI_FAMILY);
#ifdef EAI_ADDRFAMILY
    not_in_family = not_in_family || ret == EAI_ADDRFAMILY;
#endif
    if(not_in_family) {
        /*
         *  No address of the src in THIS family: the caller tries the next
         *  destination address, and logs the connect that finds none
         */
        if(gobj_global_trace_level() & TRACE_URING) {
            gobj_log_debug(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_YEV_LOOP,
                "msg",          "%s", "src_url has no address in this family",
                "src_url",      "%s", src_url,
                "ai_family",    "%d", ai_family,
                "gai_strerror", "%s", gai_strerror(ret),
                NULL
            );
        }
        return -2;
    }
    if(ret != 0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING,
            "msg",          "%s", "getaddrinfo() src_url FAILED",
            "src_url",      "%s", src_url,
            "host",         "%s", src_host,
            "port",         "%s", src_port,
            "ai_family",    "%d", ai_family,
            "gai_ret",      "%d", ret,
            "gai_strerror", "%s", gai_strerror(ret),
            NULL
        );
        return -1;
    }

    ret = bind(fd, res->ai_addr, (socklen_t) res->ai_addrlen);
    if(ret == -1) {
        char saddr[80];
        print_addrinfo(gobj, saddr, sizeof(saddr), res, atoi(src_port));
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING,
            "msg",          "%s", "bind() src_url FAILED",
            "src_url",      "%s", src_url,
            "addrinfo",     "%s", saddr,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
    }
    freeaddrinfo(res);
    return ret;
}

/***************************************************************************
 *  An IPv6 literal of a url comes in brackets, as parse_url() gives it
 *  ("[::1]"): the resolver takes it without them
 ***************************************************************************/
PRIVATE void host_without_brackets(char *host)
{
    size_t len = strlen(host);
    if(len >= 2 && host[0] == '[' && host[len-1] == ']') {
        memmove(host, host + 1, len - 2);
        host[len-2] = 0;
    }
}

/***************************************************************************
 *  TODO why don't use print_socket_address()?
 ***************************************************************************/
PRIVATE int print_addrinfo(hgobj gobj, char *bf, size_t bfsize, struct addrinfo *ai, int port)
{
    void *addr;

    if(bfsize < INET6_ADDRSTRLEN + 20) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_LIBURING,
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
