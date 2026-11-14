/****************************************************************************
 *          C_TEST1.C
 *
 *          A C_PROT_HTTP_SR server and the peers it refuses.
 *
 *          The server is a C_IOGATE whose channels are C_PROT_HTTP_SR over
 *          C_TCP (max_body_size 1024, max_tx_queue 4, timeout_tx_stall 500,
 *          max_rx_held 65536, timeout_linger 500), step 15 has a second
 *          one (timeout_tx_stall 4000, max_rx_held by default) and step 16 a
 *          third one (max_tx_queue 12000, max_rx_held 8 MB). This gobj is
 *          the application behind them. Steps, each on a connection of its
 *          own:
 *
 *          1. 600 requests pipelined in one write; the application drops
 *             the connection at the 3rd. It gets 3 requests, no more, and
 *             nothing is logged: the rest of the bytes is of nobody.
 *          2. A request whose head is over max_header_size (64 KB): the
 *             peer reads "431 Request Header Fields Too Large", once, and
 *             keeps sending: those bytes are thrown away, they do not
 *             lengthen the wait, and the server drops the connection
 *             timeout_linger after the answer, not before. One warning.
 *          3. A request whose body is over max_body_size: "413 Content
 *             Too Large", the same way. One warning.
 *          4. A peer that sends 600 requests and never reads: the answers
 *             (100 KB each) pile up to max_tx_queue, the rest of the
 *             requests is held and not parsed, and after timeout_tx_stall
 *             with no write ended the connection is dropped. One warning.
 *          5. A peer that READS: 500 requests in one write, one more with
 *             each of their answers (they find the ones before them held,
 *             and the queue at any length), and 1000 more in one write when
 *             it has all those answers. It gets 2000 answers, in the order of
 *             the requests, and nothing is logged.
 *          6. The same with 100 answers of 100 KB each.
 *          7. As step 1, but the application answers the first two: a write
 *             is in flight, so the disconnection comes later, out of the
 *             drop: it still gets 3 requests, no more.
 *          8. Three requests and, behind them in the same write, a head over
 *             the limit, with an application that answers them later, all
 *             in one turn: the peer reads the three 200 and then the 431, and
 *             the connection is dropped after timeout_linger.
 *          9. A peer that sends one request at a time and never reads: the
 *             application never sees more than max_tx_queue answers in the
 *             transport. One warning.
 *          10. A peer that does not read and keeps sending: past max_rx_held
 *             bytes held the connection is dropped. One warning.
 *          11. A peer that reads slowly: its 30 answers of 1 MB take longer
 *             than timeout_tx_stall, and it is not dropped, because writes
 *             keep ending. Nothing is logged.
 *          12. A peer fills the queue, reads it all and goes silent: the timer
 *             that watched the queue goes back to timeout_inactivity, and
 *             the peer is dropped.
 *          13. A peer that never reads, and an application that goes on
 *             answering later, after the drop that waits for the write in
 *             flight: ONE warning for the peer, and ONE for those answers.
 *          14. An application that answers later, and a peer that reads:
 *             10000 requests, 1000 in each write, the next write when it has
 *             all the answers of the one before. What is due and what waits
 *             in the transport never passes max_tx_queue by more than one
 *             read; halfway the application goes silent for longer than
 *             timeout_tx_stall, and the peer is not dropped for that. It gets
 *             every answer, in order, and nothing is logged.
 *          15. The same application, and a peer that sends 10000 requests in
 *             one pipeline and never reads: the same bound, and it is dropped
 *             timeout_tx_stall after the last write that moved, and at most
 *             two seconds later. One warning.
 *          16. A queue longer than the events that can be posted in one turn:
 *             30000 requests in one write to an application that answers each
 *             one as it comes. Once the queue is empty one turn parses and
 *             answers 12000 of those held, and nothing is logged: what resumes
 *             the parsing is the EV_TX_READY of the transport, and an answer
 *             posts nothing.
 *          17. A peer that sends max_tx_queue + 1 requests in one write and
 *             never reads, with the socket buffers of the connection small at
 *             both ends: the first answer (1 MB) never leaves, whatever the
 *             kernel's defaults, and the last one brings the transport to
 *             max_tx_queue waiting behind it. Nothing else comes. The answer itself starts the wait: the peer
 *             is dropped at timeout_tx_stall. One warning.
 *          18. A peer fills the answers owed (max_tx_queue + 1 requests the
 *             application answers later, at once), then sends a body over the
 *             limit, which is held. When the answers leave, what is held is
 *             parsed: ONE 413, after the five 200, and the rest of what was
 *             held is not parsed. One warning.
 *          19. As 18, with two more requests before the body: the refusal waits
 *             for their answers, which the application sends seconds later,
 *             after the timer that watched the full queue would have fired.
 *             The peer reads seven 200 and the 413. One warning.
 *          20. A peer that sends a request every second, under a
 *             timeout_inactivity of 3 seconds: what it sends keeps the
 *             connection, and it gets every answer. Nothing is logged.
 *          21. An application that answers a request with a body that cannot
 *             be dumped (invalid UTF-8): the peer reads a 500 in its place,
 *             and the answer of the next request after it, in order. One
 *             error.
 *          22. The same with an answer too big for a gbuffer (its headers
 *             fill the max block): a 500 in its place, then the next answer.
 *             The error of the memory and one of the answer.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "c_test1.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define SERVER_URL          "tcp://127.0.0.1:7766"
#define SERVER_PORT         7766
#define SERVER_PORT2        7765        // see main_test1.c
#define WINDOW_URL          "tcp://127.0.0.1:7769"  // see main_test1.c
#define PIPELINED           600
#define DROP_AT             3
#define BIG_HEADER          (70*1000)
#define BIG_BODY            5000
#define ANSWER_SIZE         (100*1024)
#define ANSWER_SIZE_BIG     (1024*1024)
#define READS_FIRST         500         // in one write
#define READS_ONE_BY_ONE    500         // one with each answer of those
#define READS_LAST          1000        // in one write, with all those answered
#define READS_TOTAL         (READS_FIRST + READS_ONE_BY_ONE + READS_LAST)
#define READS_BIG           100
#define MAX_TX_QUEUE        4           // see main_test1.c
#define MAX_RX_HELD         65536       // see main_test1.c
#define SLOW_ANSWERS        30
#define SLOW_PIECE          (64*1024)
#define ORDER_REQUESTS      3           // under MAX_TX_QUEUE: the head behind them is parsed, not held
#define IDLE_REQUESTS       10
#define LATE_REQUESTS       30
#define IDLE_INACTIVITY     3           // seconds, in step 12
#define IDLE_NOT_AFTER      8000        // milliseconds: timeout_tx_stall + that, in seconds of accuracy
#define TIMEOUT_LINGER      500         // see main_test1.c
#define LINGER_MARGIN       100         // the peer reads the answer after it was sent
#define ASYNC_TOTAL         10000
#define ASYNC_WINDOW        1000        // requests of one write, under max_rx_held
#define ASYNC_ONE_READ      120         // requests of the shortest kind in the 4096 bytes of a read
#define ASYNC_BOUND         (MAX_TX_QUEUE + ASYNC_ONE_READ)
#define ASYNC_SILENCE       1500        // milliseconds, over timeout_tx_stall and the timer's accuracy
#define ASYNC_SILENCE_AT    5500        // in the middle of a window: nothing the peer sends comes in the silence
#define ASYNC_REQUEST_ROOM  48
#define STALL2              4000        // timeout_tx_stall of the second server, see main_test1.c
#define STALL2_NOT_AFTER    (STALL2 + 2000)
#define STALL2_MARGIN       1500        // the test looks every 5 ms, on a loaded machine; the drop of a wait measured from the look, 2 x STALL2, stays out
#define STALL2_BEFORE       100         // the server may see a write before the test does
#define STEP_LONG_WATCH     20000       // steps 14, 15, milliseconds: 10000 answers on a loaded board (14 needed 10.5 s at -j8 on riscv64); 15 measures its drop, not its watchdog
#define WINDOW_REQUESTS     30000       // step 16: over the max_tx_queue of its server, and that over MAX_POSTED_EVENTS
#define FILL_REQUESTS       (MAX_TX_QUEUE + 1)  // answered in one turn: one in flight, max_tx_queue waiting
#define SMALL_SOCKET_BUFFER 4096        // step 17: the buffers of its connection, both ends
#define HELD_BODY           12000       // steps 18, 19: over max_body_size, in three reads, under max_rx_held
#define HELD_WAIT           300         // steps 18, 19: milliseconds, the body is held before the answers
#define PENDING_WAIT        3000        // step 19: milliseconds, over the look of a full queue (timeout_tx_stall and two ticks)
#define ACTIVE_INACTIVITY   3           // step 20: seconds
#define ACTIVE_REQUESTS     6           // step 20: one each ACTIVE_EVERY, longer than the inactivity
#define ACTIVE_EVERY        1000        // step 20: milliseconds
#define FILLING_END         "cccccccccccccccc\"}"   // the end of the body of an answer of 16 bytes

enum {
    STEP_DROP = 1,
    STEP_HEAD,
    STEP_BODY,
    STEP_MUTE,
    STEP_READS,
    STEP_READS_BIG,
    STEP_DROP_IN_FLIGHT,
    STEP_ORDER,
    STEP_QUEUE,
    STEP_HELD,
    STEP_SLOW,
    STEP_IDLE,
    STEP_LATE_ANSWERS,
    STEP_ASYNC,
    STEP_ASYNC_MUTE,
    STEP_WINDOW,
    STEP_ARM_AFTER,
    STEP_HELD_REFUSED,
    STEP_REFUSAL_PENDING,
    STEP_ACTIVE,
    STEP_BAD_BODY,
    STEP_ANSWER_NO_MEMORY,
    STEP_END
};

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void start_step(hgobj gobj);
PRIVATE void send_raw(hgobj gobj, const char *data, size_t len);
PRIVATE void answer_request(hgobj gobj, hgobj channel, int n, size_t size);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
GOBJ_DEFINE_EVENT(EV_NEXT_STEP);
GOBJ_DEFINE_EVENT(EV_PEER_ANSWER);

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
    hgobj timer_tick;           // what a peer or the application does later

    hgobj gobj_input_side;
    hgobj gobj_client;          // the peer that reads
    GHTTP_PARSER *peer_parser;  // what that peer reads, in steps of many answers
    int fd_mute;                // the peer that never reads
    int step;
    int requests;               // requests the application got in this step
    int answers;                // answers the peer read in this step
    int peer_sent;              // requests the peer that reads sent in this step
    gbuffer_t *answer;          // what the peer read in this step
    BOOL step_done;
    BOOL peer_connected;
    uint64_t linger_timer;      // the answer was read: the drop not before this
    hgobj held_channel;         // the channel of the requests the application answers later
    int mute_sent;              // requests (or bytes) a peer that never reads sent so far
    size_t slow_read;           // step 11: the bytes the slow peer read
    uint64_t idle_timer;        // step 12: the drop before this
    int max_queue_seen;         // step 9: the most answers the transport held at a request
    int next_answer;            // steps 14, 15: the first request not answered yet
    int max_owed_seen;          // steps 14, 15: the most answers due or waiting in the transport
    uint64_t silence_timer;     // step 14: the application says nothing until this
    gbuffer_t *pipeline;        // step 15: what the peer still has to send
    json_int_t last_written;    // step 15: txBytes of the transport
    uint64_t last_write_moved;  // step 15: when it changed
    json_int_t cut_after;       // step 15: milliseconds from then to the drop, -1: no drop yet
    uint64_t pending_timer;     // step 19: the application answers the last two after this
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
    priv->timer_tick = gobj_create_pure_child("tick", C_TIMER0, 0, gobj);
    priv->answer = gbuffer_create(4*1024, 64*1024);
    priv->fd_mute = -1;

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
    gobj_start(priv->timer_tick);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->timer);
    gobj_stop(priv->timer_tick);
    if(priv->gobj_client && gobj_is_running(priv->gobj_client)) {
        gobj_stop(priv->gobj_client);
    }
    if(priv->fd_mute >= 0) {
        close(priv->fd_mute);
        priv->fd_mute = -1;
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

    hgobj gobj_input_side2 = gobj_find_service("__input_side2__", TRUE);
    gobj_subscribe_event(gobj_input_side2, NULL, 0, gobj);
    gobj_start_tree(gobj_input_side2);

    hgobj gobj_input_side3 = gobj_find_service("__input_side3__", TRUE);
    gobj_subscribe_event(gobj_input_side3, NULL, 0, gobj);
    gobj_start_tree(gobj_input_side3);

    set_timeout(priv->timer, priv->timeout); // then the first step

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    clear_timeout0(priv->timer_tick);

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBUFFER_DECREF(priv->answer)
    GBUFFER_DECREF(priv->pipeline)
    if(priv->peer_parser) {
        ghttp_parser_destroy(priv->peer_parser);
        priv->peer_parser = 0;
    }
}



                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Bytes to the server, through the peer of steps 1 to 3
 ***************************************************************************/
PRIVATE void send_raw(hgobj gobj, const char *data, size_t len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(len, len);
    if(!gbuf) {
        return; // Error already logged
    }
    gbuffer_append(gbuf, (void *)data, len);
    gobj_send_event(priv->gobj_client, EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
        gobj
    );
}

/***************************************************************************
 *  A socket that connects to the server and never reads
 ***************************************************************************/
PRIVATE int connect_mute_peer(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->fd_mute = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((priv->step == STEP_ASYNC_MUTE)? SERVER_PORT2 : SERVER_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if(priv->fd_mute >= 0 && priv->step == STEP_ARM_AFTER) {
        // A small window: the kernel takes nothing like the answers, not one write ends
        int rcvbuf = SMALL_SOCKET_BUFFER;
        setsockopt(priv->fd_mute, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    }
    if(priv->fd_mute < 0 || connect(priv->fd_mute, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "The peer that never reads cannot connect",
            "errno",        "%s", strerror(errno),
            NULL
        );
        set_yuno_must_die();
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  A peer for the step: a C_TCP that does not reconnect, or a socket that
 *  never reads
 ***************************************************************************/
PRIVATE void start_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->step++;
    priv->requests = 0;
    priv->answers = 0;
    priv->peer_sent = 0;
    priv->step_done = FALSE;
    priv->linger_timer = 0;
    priv->idle_timer = 0;
    priv->mute_sent = 0;
    priv->slow_read = 0;
    priv->max_queue_seen = 0;
    priv->held_channel = 0;
    priv->next_answer = 0;
    priv->max_owed_seen = 0;
    priv->silence_timer = 0;
    priv->last_written = 0;
    priv->last_write_moved = 0;
    priv->cut_after = -1;
    priv->pending_timer = 0;
    gbuffer_reset_wr(priv->answer);
    gbuffer_reset_rd(priv->answer);

    if(priv->step >= STEP_END) {
        set_yuno_must_die();

    } else if(priv->step == STEP_MUTE) {
        if(connect_mute_peer(gobj) < 0) {
            return; // Error already logged
        }
        char req[64];
        for(int i = 0; i < PIPELINED; i++) {
            int n = snprintf(req, sizeof(req), "GET /mute/%d HTTP/1.1\r\nHost: x\r\n\r\n", i);
            if(write(priv->fd_mute, req, (size_t)n) != n) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "The peer that never reads cannot write",
                    "errno",        "%s", strerror(errno),
                    NULL
                );
                break;
            }
        }

    } else if(priv->step == STEP_QUEUE || priv->step == STEP_HELD) {
        if(connect_mute_peer(gobj) < 0) {
            return; // Error already logged
        }
        set_timeout_periodic0(priv->timer_tick, 5);

    } else if(priv->step == STEP_ARM_AFTER) {
        if(connect_mute_peer(gobj) < 0) {
            return; // Error already logged
        }
        char req[FILL_REQUESTS*64];
        int n = 0;
        for(int i = 0; i < FILL_REQUESTS; i++) {
            n += snprintf(req + n, sizeof(req) - (size_t)n, "GET /arm/%d HTTP/1.1\r\nHost: x\r\n\r\n", i);
        }
        if(write(priv->fd_mute, req, (size_t)n) != n) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "The peer that never reads cannot write",
                "errno",        "%s", strerror(errno),
                NULL
            );
        }

    } else if(priv->step == STEP_SLOW || priv->step == STEP_LATE_ANSWERS) {
        if(connect_mute_peer(gobj) < 0) {
            return; // Error already logged
        }
        for(int i = 0; i < ((priv->step == STEP_SLOW)? SLOW_ANSWERS : LATE_REQUESTS); i++) {
            char req[64];
            int n = snprintf(req, sizeof(req), "GET /slow/%d HTTP/1.1\r\nHost: x\r\n\r\n", i);
            if(write(priv->fd_mute, req, (size_t)n) != n) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "The peer that reads slowly cannot write",
                    "errno",        "%s", strerror(errno),
                    NULL
                );
                break;
            }
        }
        set_timeout_periodic0(priv->timer_tick, (priv->step == STEP_SLOW)? 10 : 100);

    } else if(priv->step == STEP_ASYNC_MUTE) {
        if(connect_mute_peer(gobj) < 0) {
            return; // Error already logged
        }
        size_t size = ASYNC_TOTAL * ASYNC_REQUEST_ROOM;
        priv->pipeline = gbuffer_create(size, size);
        for(int i = 0; i < ASYNC_TOTAL; i++) {
            gbuffer_printf(priv->pipeline, "GET /async/%d HTTP/1.1\r\nHost: x\r\n\r\n", i);
        }
        set_timeout(priv->timer, STEP_LONG_WATCH);
        set_timeout_periodic0(priv->timer_tick, 5);

    } else {
        char name[32];
        snprintf(name, sizeof(name), "client-%d", priv->step);
        priv->gobj_client = gobj_create_pure_child(
            name,
            C_TCP,
            json_pack("{s:s, s:i}",
                "url", (priv->step == STEP_WINDOW)? WINDOW_URL : SERVER_URL,
                "timeout_between_connections", 600000
            ),
            gobj
        );
        if(priv->step == STEP_ASYNC) {
            set_timeout(priv->timer, STEP_LONG_WATCH);
        }
        gobj_start(priv->gobj_client);
    }
}

/***************************************************************************
 *  `n` requests in one write, of the peer that reads
 ***************************************************************************/
PRIVATE void send_pipelined_requests(hgobj gobj, int n)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create((size_t)n * 64, (size_t)n * 64);
    if(!gbuf) {
        return; // Error already logged
    }
    for(int i = 0; i < n; i++) {
        gbuffer_printf(gbuf, "GET /pipelined/%d HTTP/1.1\r\nHost: x\r\n\r\n", priv->peer_sent);
        priv->peer_sent++;
    }
    gobj_send_event(priv->gobj_client, EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
        gobj
    );
}

/***************************************************************************
 *  The application that answers later answers what it has, or `most`
 ***************************************************************************/
PRIVATE void answer_late_requests(hgobj gobj, size_t size, int most)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj bottom = gobj_last_bottom_gobj(priv->held_channel);
    int owed = (priv->requests - priv->next_answer) + (int)gobj_read_integer_attr(bottom, "cur_tx_queue");
    if(owed > priv->max_owed_seen) {
        priv->max_owed_seen = owed;
    }
    if(owed > ASYNC_BOUND) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The answers of an application that answers later were not bounded",
            "owed",         "%d", owed,
            "bound",        "%d", ASYNC_BOUND,
            NULL
        );
        clear_timeout0(priv->timer_tick);
        return;
    }
    for(int i = 0; i < most && priv->next_answer < priv->requests; i++) {
        answer_request(gobj, priv->held_channel, priv->next_answer, size);
        priv->next_answer++;
    }
}

/***************************************************************************
 *  What the peer that never reads still has to send, as it fits
 ***************************************************************************/
PRIVATE void send_pipeline(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t left = priv->pipeline? gbuffer_leftbytes(priv->pipeline) : 0;
    if(left > 0) {
        ssize_t sent = send(priv->fd_mute, gbuffer_cur_rd_pointer(priv->pipeline), left, MSG_NOSIGNAL|MSG_DONTWAIT);
        if(sent > 0) {
            gbuffer_get(priv->pipeline, (size_t)sent);
        }
    }
}

/***************************************************************************
 *  Step 14: the application answers later, and once, halfway, it has answered
 *  one with the queue empty and goes silent.
 ***************************************************************************/
PRIVATE void do_async_tick(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->held_channel || (priv->silence_timer && !test_msectimer(priv->silence_timer))) {
        return;
    }
    if(!priv->silence_timer && priv->next_answer >= ASYNC_SILENCE_AT) {
        hgobj bottom = gobj_last_bottom_gobj(priv->held_channel);
        if(priv->next_answer < priv->requests && gobj_read_integer_attr(bottom, "cur_tx_queue") == 0) {
            answer_late_requests(gobj, 16, 1);
            priv->silence_timer = start_msectimer(ASYNC_SILENCE);
        }
        return;
    }
    answer_late_requests(gobj, 16, ASYNC_TOTAL);
}

/***************************************************************************
 *  Step 15: the peer sends, the application answers, and the test sees the
 *  last write that moved and when the server dropped the connection
 ***************************************************************************/
PRIVATE void do_async_mute_tick(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->fd_mute >= 0) {
        send_pipeline(gobj);
    }
    if(!priv->held_channel || priv->step_done) {
        return;
    }
    hgobj bottom = gobj_last_bottom_gobj(priv->held_channel);
    if(!bottom) {
        return;
    }
    if(gobj_current_state(bottom) != ST_CONNECTED) {
        if(priv->cut_after < 0) {
            priv->cut_after = (json_int_t)(time_in_milliseconds_monotonic() - priv->last_write_moved);
        }
        return;
    }
    answer_late_requests(gobj, ANSWER_SIZE, ASYNC_TOTAL);

    json_int_t written = gobj_read_integer_attr(bottom, "txBytes");
    if(written != priv->last_written || !priv->last_write_moved) {
        priv->last_written = written;
        priv->last_write_moved = time_in_milliseconds_monotonic();
    }
}

/***************************************************************************
 *  The peer that reads, or the application, or the peer that never reads
 *  acts again
 ***************************************************************************/
PRIVATE void do_tick(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->step == STEP_HEAD) {
        if(priv->peer_connected) {
            char bytes[1024];
            memset(bytes, 'z', sizeof(bytes));
            send_raw(gobj, bytes, sizeof(bytes));
        }

    } else if(priv->step == STEP_ASYNC) {
        do_async_tick(gobj);

    } else if(priv->step == STEP_ASYNC_MUTE) {
        do_async_mute_tick(gobj);

    } else if(priv->step == STEP_ORDER) {
        clear_timeout0(priv->timer_tick);
        for(int i = 0; i < ORDER_REQUESTS; i++) {
            answer_request(gobj, priv->held_channel, i, 16);
        }

    } else if((priv->step == STEP_HELD_REFUSED || priv->step == STEP_REFUSAL_PENDING) && priv->held_channel) {
        /*
         *  The answers owed, at once: the last one fills the transport
         */
        if(priv->next_answer < FILL_REQUESTS && test_msectimer(priv->silence_timer)) {
            for(int i = 0; i < FILL_REQUESTS; i++) {
                answer_request(gobj, priv->held_channel, i, 16);
            }
            priv->next_answer = FILL_REQUESTS;
        } else if(priv->pending_timer && test_msectimer(priv->pending_timer)) {
            priv->pending_timer = 0;
            clear_timeout0(priv->timer_tick);
            for(int i = FILL_REQUESTS; i < FILL_REQUESTS + 2; i++) {
                answer_request(gobj, priv->held_channel, i, 16);
            }
        }

    } else if(priv->step == STEP_ACTIVE) {
        if(priv->peer_sent < ACTIVE_REQUESTS) {
            send_pipelined_requests(gobj, 1);
        }

    } else if(priv->step == STEP_LATE_ANSWERS && priv->held_channel && !priv->step_done) {
        answer_request(gobj, priv->held_channel, -1, 16);

    } else if(priv->step == STEP_QUEUE && priv->fd_mute >= 0 && priv->requests == priv->mute_sent) {
        /*
         *  Each one alone in its read, and only when the one before reached
         *  the application: the one held is the last one.
         */
        char req[64];
        int n = snprintf(req, sizeof(req), "GET /queue/%d HTTP/1.1\r\nHost: x\r\n\r\n", priv->mute_sent);
        if(send(priv->fd_mute, req, (size_t)n, MSG_NOSIGNAL) == n) {
            priv->mute_sent++;
        }

    } else if(priv->step == STEP_HELD && priv->fd_mute >= 0) {
        char bytes[8*1024];
        int left = 2*MAX_RX_HELD - priv->mute_sent;
        if(left > 0) {
            int n = 0;
            while((size_t)(n + 64) < sizeof(bytes)) {
                n += snprintf(bytes + n, sizeof(bytes) - (size_t)n, "GET /held HTTP/1.1\r\nHost: x\r\n\r\n");
            }
            ssize_t sent = send(priv->fd_mute, bytes, (size_t)n, MSG_NOSIGNAL|MSG_DONTWAIT);
            if(sent > 0) {
                priv->mute_sent += (int)sent;
            }
        }

    } else if(priv->step == STEP_SLOW && priv->fd_mute >= 0) {
        char *bytes = gbmem_malloc(SLOW_PIECE);
        if(bytes) {
            ssize_t n = recv(priv->fd_mute, bytes, SLOW_PIECE, MSG_DONTWAIT);
            if(n > 0) {
                priv->slow_read += (size_t)n;
            }
            if(n == 0 || priv->slow_read >= SLOW_ANSWERS*ANSWER_SIZE_BIG) {
                clear_timeout0(priv->timer_tick);
                close(priv->fd_mute);   // it read them all, or it was dropped
                priv->fd_mute = -1;
            }
            GBMEM_FREE(bytes)
        }
    }
}

/***************************************************************************
 *  The application answers request `n`, with a dict that says which one
 *  and has `size` bytes of filling
 ***************************************************************************/
PRIVATE void answer_request(hgobj gobj, hgobj channel, int n, size_t size)
{
    char *filling = gbmem_malloc(size + 1);
    if(!filling) {
        return; // Error already logged
    }
    memset(filling, 'c', size);
    filling[size] = 0;
    gobj_send_event(channel, EV_SEND_MESSAGE,
        json_pack("{s:s, s:s, s:{s:i, s:s}}",
            "code", "200 OK",
            "headers", "",
            "body",
                "n", n,
                "filling", filling
        ),
        gobj
    );
    GBMEM_FREE(filling)
}

/***************************************************************************
 *  The answers of the steps where the peer counts them
 ***************************************************************************/
PRIVATE int answers_of_step(int step)
{
    switch(step) {
        case STEP_READS:
            return READS_TOTAL;
        case STEP_ASYNC:
            return ASYNC_TOTAL;
        case STEP_WINDOW:
            return WINDOW_REQUESTS;
        default:
            return READS_BIG;
    }
}

/***************************************************************************
 *  Step 17: the server's end of the silent peer's connection gets a small
 *  send buffer, so no answer of 1 MB ends whatever the kernel's tcp_wmem.
 *  The socket is found among the fds of this process: the one on
 *  SERVER_PORT whose peer is the silent peer.
 ***************************************************************************/
PRIVATE void shrink_server_sndbuf(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    struct sockaddr_in mute = {0};
    socklen_t len = sizeof(mute);
    if(getsockname(priv->fd_mute, (struct sockaddr *)&mute, &len) == 0) {
        for(int fd = 0; fd < 4096; fd++) {
            struct sockaddr_in local = {0}, peer = {0};
            socklen_t local_len = sizeof(local), peer_len = sizeof(peer);
            if(fd == priv->fd_mute ||
                    getsockname(fd, (struct sockaddr *)&local, &local_len) < 0 ||
                    local.sin_family != AF_INET || ntohs(local.sin_port) != SERVER_PORT ||
                    getpeername(fd, (struct sockaddr *)&peer, &peer_len) < 0 ||
                    peer.sin_port != mute.sin_port) {
                continue;
            }
            int sndbuf = SMALL_SOCKET_BUFFER;
            if(setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) == 0) {
                return;
            }
            break;
        }
    }
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_SYSTEM,
        "msg",          "%s", "The send buffer of the silent peer's connection could not be made small",
        "errno",        "%s", strerror(errno),
        NULL
    );
}

/***************************************************************************
 *  Steps 18 and 19: what the peer sends behind the answers owed, held: a
 *  body over max_body_size, after two requests in step 19
 ***************************************************************************/
PRIVATE void send_held_refused(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(HELD_BODY + 256, HELD_BODY + 256);
    if(!gbuf) {
        return; // Error already logged
    }
    if(priv->step == STEP_REFUSAL_PENDING) {
        gbuffer_printf(gbuf,
            "GET /held/%d HTTP/1.1\r\nHost: x\r\n\r\nGET /held/%d HTTP/1.1\r\nHost: x\r\n\r\n",
            FILL_REQUESTS, FILL_REQUESTS + 1
        );
    }
    gbuffer_printf(gbuf,
        "POST /big HTTP/1.1\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n", HELD_BODY);
    for(int i = 0; i < HELD_BODY; i++) {
        gbuffer_append_char(gbuf, 'b');
    }
    gobj_send_event(priv->gobj_client, EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
        gobj
    );
}

/***************************************************************************
 *  The status lines `status` the peer read
 ***************************************************************************/
PRIVATE int count_status(hgobj gobj, const char *status)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int count = 0;
    const char *text = gbuffer_cur_rd_pointer(priv->answer);
    while((text = strstr(text, status))) {
        count++;
        text += strlen(status);
    }
    return count;
}

/***************************************************************************
 *  What the peer read: first the status line it is due
 ***************************************************************************/
PRIVATE void check_answer(hgobj gobj, const char *status_line)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *answer = gbuffer_cur_rd_pointer(priv->answer);
    if(strncmp(answer, status_line, strlen(status_line)) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The refused request was not told why",
            "step",         "%d", priv->step,
            "answer",       "%.80s", answer,
            "expected",     "%s", status_line,
            NULL
        );
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  First timeout: the server listens, the first step begins.
 *  Any other: a step never ended.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->timer_tick) {
        do_tick(gobj);
    } else if(priv->step == 0) {
        start_step(gobj);
        set_timeout(priv->timer, 10*priv->timeout);
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A step did not end",
            "step",         "%d", priv->step,
            "requests",     "%d", priv->requests,
            NULL
        );
        set_yuno_must_die();
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The next step, out of the stack of the one that ended
 ***************************************************************************/
PRIVATE int ac_next_step(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout0(priv->timer_tick);

    if((priv->step == STEP_DROP || priv->step == STEP_DROP_IN_FLIGHT) && priv->requests != DROP_AT) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A dropped connection delivered requests after its drop",
            "step",         "%d", priv->step,
            "requests",     "%d", priv->requests,
            "expected",     "%d", DROP_AT,
            NULL
        );
    } else if(priv->step == STEP_HEAD) {
        check_answer(gobj, "HTTP/1.1 431 Request Header Fields Too Large\r\n");
        if(strstr((char *)gbuffer_cur_rd_pointer(priv->answer) + 8, "HTTP/1.1")) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The bytes sent after the refusal were answered",
                NULL
            );
        }
    } else if(priv->step == STEP_BODY) {
        check_answer(gobj, "HTTP/1.1 413 Content Too Large\r\n");
    } else if(priv->step == STEP_READS || priv->step == STEP_READS_BIG || priv->step == STEP_ASYNC ||
            priv->step == STEP_WINDOW) {
        int expected = answers_of_step(priv->step);
        if(priv->answers != expected || priv->requests != expected) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A peer that reads did not get all its answers",
                "step",         "%d", priv->step,
                "requests",     "%d", priv->requests,
                "answers",      "%d", priv->answers,
                "expected",     "%d", expected,
                NULL
            );
        }
    } else if(priv->step == STEP_ORDER) {
        check_answer(gobj, "HTTP/1.1 200 OK\r\n");
        const char *text = gbuffer_cur_rd_pointer(priv->answer);
        int oks = 0;
        const char *found;
        while((found = strstr(text, "HTTP/1.1 "))) {
            if(strncmp(found, "HTTP/1.1 431 ", 13) == 0) {
                break;
            }
            oks++;
            text = found + 9;
        }
        if(oks != ORDER_REQUESTS || !found) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The refusal did not follow the answers before it",
                "answers",      "%d", oks,
                "answer",       "%.200s", (char *)gbuffer_cur_rd_pointer(priv->answer),
                NULL
            );
        }
    } else if(priv->step == STEP_HELD_REFUSED || priv->step == STEP_REFUSAL_PENDING) {
        int oks = (priv->step == STEP_HELD_REFUSED)? FILL_REQUESTS : FILL_REQUESTS + 2;
        const char *last = strstr(gbuffer_cur_rd_pointer(priv->answer), "HTTP/1.1 413 ");
        if(count_status(gobj, "HTTP/1.1 200 OK\r\n") != oks || count_status(gobj, "HTTP/1.1 413 ") != 1 ||
                count_status(gobj, "HTTP/1.1 ") != oks + 1 || !last || strstr(last + 1, "HTTP/1.1 ")) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A refused request held behind the answers owed was not answered once, after them",
                "step",         "%d", priv->step,
                "oks",          "%d", count_status(gobj, "HTTP/1.1 200 OK\r\n"),
                "refusals",     "%d", count_status(gobj, "HTTP/1.1 413 "),
                "expected_oks", "%d", oks,
                NULL
            );
        }
    } else if(priv->step == STEP_ACTIVE) {
        if(count_status(gobj, "HTTP/1.1 200 OK\r\n") != ACTIVE_REQUESTS) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "An active peer was dropped at timeout_inactivity",
                "answers",      "%d", count_status(gobj, "HTTP/1.1 200 OK\r\n"),
                "expected",     "%d", ACTIVE_REQUESTS,
                NULL
            );
        }
    } else if(priv->step == STEP_BAD_BODY || priv->step == STEP_ANSWER_NO_MEMORY) {
        const char *text = gbuffer_cur_rd_pointer(priv->answer);
        const char *ok = strstr(text, "HTTP/1.1 200 OK\r\n");
        if(strncmp(text, "HTTP/1.1 500 ", 13) != 0 || !ok || !strstr(ok, "{\"n\":1,")) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "An answer that cannot be built did not leave a 500 in its place",
                "step",         "%d", priv->step,
                "answer",       "%.120s", text,
                NULL
            );
        }
    } else if(priv->step == STEP_SLOW) {
        if(priv->slow_read < SLOW_ANSWERS*ANSWER_SIZE_BIG) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A peer that reads slowly was dropped",
                "slow_read",    "%lu", (unsigned long)priv->slow_read,
                NULL
            );
        }
        if(priv->fd_mute >= 0) {
            close(priv->fd_mute);
            priv->fd_mute = -1;
        }
    } else if(priv->step == STEP_ASYNC_MUTE) {
        if(priv->fd_mute >= 0) {
            close(priv->fd_mute);
            priv->fd_mute = -1;
        }
        GBUFFER_DECREF(priv->pipeline)
        if(priv->cut_after < STALL2 - STALL2_BEFORE || priv->cut_after > STALL2_NOT_AFTER + STALL2_MARGIN) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A peer that does not read was not dropped at timeout_tx_stall",
                "cut_after",    "%d", (int)priv->cut_after,
                "not_before",   "%d", STALL2,
                "not_after",    "%d", STALL2_NOT_AFTER,
                NULL
            );
        }
    } else if(priv->step == STEP_MUTE || priv->step == STEP_QUEUE || priv->step == STEP_HELD ||
            priv->step == STEP_LATE_ANSWERS || priv->step == STEP_ARM_AFTER) {
        if(priv->fd_mute >= 0) {
            close(priv->fd_mute);
            priv->fd_mute = -1;
        }
        if(priv->step == STEP_QUEUE && priv->max_queue_seen != MAX_TX_QUEUE - 1) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A request was not held at max_tx_queue answers",
                "max_queue_seen", "%d", priv->max_queue_seen,
                "expected",     "%d", MAX_TX_QUEUE - 1,
                NULL
            );
        }
        if(priv->step != STEP_QUEUE && priv->requests >= PIPELINED/2) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The requests of a peer that does not read were not held",
                "step",         "%d", priv->step,
                "requests",     "%d", priv->requests,
                NULL
            );
        }
    }

    if(priv->gobj_client) {
        /*
         *  Forgotten before it is stopped: its disconnection comes in the
         *  next step, and must not be taken for that step's own peer
         */
        hgobj client = priv->gobj_client;
        priv->gobj_client = 0;
        gobj_stop(client);
    }
    set_timeout(priv->timer, 10*priv->timeout);
    start_step(gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The peer is connected: what it sends in its step
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->peer_connected = TRUE;

    if(priv->step == STEP_DROP || priv->step == STEP_DROP_IN_FLIGHT) {
        send_pipelined_requests(gobj, PIPELINED);

    } else if(priv->step == STEP_READS || priv->step == STEP_READS_BIG || priv->step == STEP_IDLE ||
            priv->step == STEP_ASYNC || priv->step == STEP_WINDOW) {
        if(priv->peer_parser) {
            ghttp_parser_destroy(priv->peer_parser);
        }
        priv->peer_parser = ghttp_parser_create(gobj, HTTP_RESPONSE, 0, 0, EV_PEER_ANSWER, TRUE);
        if(priv->step == STEP_IDLE) {
            send_pipelined_requests(gobj, IDLE_REQUESTS);
        } else if(priv->step == STEP_ASYNC) {
            send_pipelined_requests(gobj, ASYNC_WINDOW);
            set_timeout_periodic0(priv->timer_tick, 5);
        } else if(priv->step == STEP_WINDOW) {
            send_pipelined_requests(gobj, WINDOW_REQUESTS);
        } else {
            send_pipelined_requests(gobj, (priv->step == STEP_READS)? READS_FIRST : READS_BIG);
        }

    } else if(priv->step == STEP_HEAD || priv->step == STEP_ORDER) {
        const char *first = (priv->step == STEP_ORDER)?
            "GET /1 HTTP/1.1\r\nHost: x\r\n\r\nGET /2 HTTP/1.1\r\nHost: x\r\n\r\n"
            "GET /3 HTTP/1.1\r\nHost: x\r\n\r\n" : "";
        const char *head = "GET / HTTP/1.1\r\nX-Big: ";
        size_t first_len = strlen(first);
        size_t head_len = strlen(head);
        size_t len = first_len + head_len + BIG_HEADER + 4;
        char *req = gbmem_malloc(len + 1);
        if(req) {
            memcpy(req, first, first_len);
            memcpy(req + first_len, head, head_len);
            memset(req + first_len + head_len, 'a', BIG_HEADER);
            memcpy(req + first_len + head_len + BIG_HEADER, "\r\n\r\n", 5);
            send_raw(gobj, req, len);
            GBMEM_FREE(req)
        }

    } else if(priv->step == STEP_HELD_REFUSED || priv->step == STEP_REFUSAL_PENDING) {
        send_pipelined_requests(gobj, FILL_REQUESTS);

    } else if(priv->step == STEP_ACTIVE) {
        send_pipelined_requests(gobj, 1);
        set_timeout_periodic0(priv->timer_tick, ACTIVE_EVERY);

    } else if(priv->step == STEP_BAD_BODY || priv->step == STEP_ANSWER_NO_MEMORY) {
        send_pipelined_requests(gobj, 2);

    } else if(priv->step == STEP_BODY) {
        char head[128];
        int n = snprintf(head, sizeof(head),
            "POST /big HTTP/1.1\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n", BIG_BODY);
        char *req = gbmem_malloc((size_t)n + BIG_BODY);
        if(req) {
            memcpy(req, head, (size_t)n);
            memset(req + n, 'b', BIG_BODY);
            send_raw(gobj, req, (size_t)n + BIG_BODY);
            GBMEM_FREE(req)
        }
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the peer reads
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED);
    if(gbuf && priv->peer_parser && (priv->step == STEP_READS || priv->step == STEP_READS_BIG ||
            priv->step == STEP_IDLE || priv->step == STEP_ASYNC || priv->step == STEP_WINDOW)) {
        ghttp_parser_received(priv->peer_parser, gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf));

    } else if(gbuf) {
        gbuffer_append_gbuf(priv->answer, gbuf);
        if(priv->step == STEP_HEAD && !priv->linger_timer) {
            priv->linger_timer = start_msectimer(TIMEOUT_LINGER - LINGER_MARGIN);
            set_timeout_periodic0(priv->timer_tick, 50);
        }
        BOOL all_read = FALSE;
        if(priv->step == STEP_ACTIVE) {
            all_read = count_status(gobj, "HTTP/1.1 200 OK\r\n") == ACTIVE_REQUESTS;
        } else if(priv->step == STEP_BAD_BODY || priv->step == STEP_ANSWER_NO_MEMORY) {
            all_read = strstr(gbuffer_cur_rd_pointer(priv->answer), FILLING_END) != NULL;
        }
        if(all_read && !priv->step_done) {
            priv->step_done = TRUE;
            gobj_post_event(gobj, EV_NEXT_STEP, json_object(), gobj);
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  An answer read by the peer of the steps of many answers: they must come
 *  in the order of the requests. With the last one the peer goes.
 ***************************************************************************/
PRIVATE int ac_peer_answer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int n = (int)kw_get_int(gobj, kw, "body`n", -1, 0);
    if(n != priv->answers) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The answers did not come in the order of the requests",
            "answer",       "%d", n,
            "expected",     "%d", priv->answers,
            NULL
        );
    }
    priv->answers++;

    if(priv->step == STEP_IDLE) {
        if(priv->answers == IDLE_REQUESTS) {
            priv->idle_timer = start_msectimer(IDLE_NOT_AFTER);
        }
        KW_DECREF(kw)
        return 0;
    }
    if(priv->step == STEP_READS) {
        if(priv->answers <= READS_ONE_BY_ONE) {
            send_pipelined_requests(gobj, 1);
        } else if(priv->answers == READS_FIRST + READS_ONE_BY_ONE) {
            send_pipelined_requests(gobj, READS_LAST);
        }
    }
    if(priv->step == STEP_ASYNC && priv->answers % ASYNC_WINDOW == 0 && priv->answers < ASYNC_TOTAL) {
        send_pipelined_requests(gobj, ASYNC_WINDOW);
    }
    if(priv->answers == answers_of_step(priv->step) && !priv->step_done) {
        priv->step_done = TRUE;
        gobj_post_event(gobj, EV_NEXT_STEP, json_object(), gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The peer of steps 1 to 3 lost its connection: the step ends. The peer of
 *  a step the test ends itself is forgotten (gobj_client 0) before it stops,
 *  so its disconnection, which comes in the next step, ends nothing.
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->peer_connected = FALSE;

    if(src == priv->gobj_client && !priv->step_done && priv->step < STEP_END) {
        priv->step_done = TRUE;
        if(priv->step == STEP_IDLE && (!priv->idle_timer || test_msectimer(priv->idle_timer))) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A silent peer was not dropped at timeout_inactivity",
                "answers",      "%d", priv->answers,
                NULL
            );
        }
        if(priv->linger_timer && !test_msectimer(priv->linger_timer)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The connection of a refused request was dropped before timeout_linger",
                NULL
            );
        }
        gobj_post_event(gobj, EV_NEXT_STEP, json_object(), gobj);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  At the server, a request: what the application does with it in each step
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj channel = (hgobj)(uintptr_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    const char *number = strrchr(kw_get_str(gobj, kw, "url", "", KW_REQUIRED), '/');
    int n = number? atoi(number + 1) : -1;  // the request says which one it is
    priv->requests++;

    if(priv->step == STEP_DROP) {
        if(priv->requests == DROP_AT) {
            gobj_send_event(channel, EV_DROP, 0, gobj);
        }

    } else if(priv->step == STEP_DROP_IN_FLIGHT) {
        if(priv->requests == DROP_AT) {
            gobj_send_event(channel, EV_DROP, 0, gobj);
        } else {
            answer_request(gobj, channel, n, 16);
        }

    } else if(priv->step == STEP_ASYNC || priv->step == STEP_ASYNC_MUTE) {
        priv->held_channel = channel;

    } else if(priv->step == STEP_ORDER) {
        priv->held_channel = channel;
        if(priv->requests == ORDER_REQUESTS) {
            set_timeout_periodic0(priv->timer_tick, 100);
        }

    } else if(priv->step == STEP_LATE_ANSWERS) {
        priv->held_channel = channel;
        answer_request(gobj, channel, n, ANSWER_SIZE_BIG);

    } else if(priv->step == STEP_READS || priv->step == STEP_WINDOW || priv->step == STEP_ACTIVE) {
        answer_request(gobj, channel, n, 16);

    } else if(priv->step == STEP_ARM_AFTER) {
        /*
         *  The first answer never leaves; the others wait behind it, and the
         *  last one makes them max_tx_queue
         */
        if(!priv->held_channel) {
            shrink_server_sndbuf(gobj);
        }
        priv->held_channel = channel;
        answer_request(gobj, channel, n, (n == 0)? ANSWER_SIZE_BIG : 16);

    } else if(priv->step == STEP_HELD_REFUSED || priv->step == STEP_REFUSAL_PENDING) {
        priv->held_channel = channel;
        if(priv->requests == FILL_REQUESTS) {
            send_held_refused(gobj);
            priv->silence_timer = start_msectimer(HELD_WAIT);
            set_timeout_periodic0(priv->timer_tick, 50);
        } else if(priv->requests == FILL_REQUESTS + 2) {
            priv->pending_timer = start_msectimer(PENDING_WAIT);
        }

    } else if(priv->step == STEP_BAD_BODY || priv->step == STEP_ANSWER_NO_MEMORY) {
        if(n == 0) {
            json_t *kw_answer;
            if(priv->step == STEP_BAD_BODY) {
                kw_answer = json_pack("{s:s, s:s, s:o}",
                    "code", "200 OK",
                    "headers", "",
                    "body", json_stringn_nocheck("\xff", 1)
                );
            } else {
                size_t size = gbmem_get_maximum_block() - 200;     // a string fits, the answer does not
                char *headers = gbmem_malloc(size + 1);
                if(!headers) {
                    KW_DECREF(kw)
                    return 0;   // Error already logged
                }
                memset(headers, 'h', size);
                headers[size] = 0;
                kw_answer = json_pack("{s:s, s:s, s:{}}",
                    "code", "200 OK",
                    "headers", headers,
                    "body"
                );
                GBMEM_FREE(headers)
            }
            int ret = gobj_send_event(channel, EV_SEND_MESSAGE, kw_answer, gobj);
            if(ret != -1) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "An answer that cannot be built was not refused",
                    NULL
                );
            }
        } else {
            answer_request(gobj, channel, n, 16);
        }

    } else if(priv->step == STEP_IDLE) {
        gobj_write_integer_attr(gobj_bottom_gobj(channel), "timeout_inactivity", IDLE_INACTIVITY);
        answer_request(gobj, channel, n, 16);

    } else if(priv->step == STEP_READS_BIG || priv->step == STEP_MUTE || priv->step == STEP_HELD) {
        answer_request(gobj, channel, n, ANSWER_SIZE);

    } else if(priv->step == STEP_SLOW) {
        answer_request(gobj, channel, n, ANSWER_SIZE_BIG);

    } else if(priv->step == STEP_QUEUE) {
        hgobj bottom = gobj_last_bottom_gobj(channel);
        int queued = (int)gobj_read_integer_attr(bottom, "cur_tx_queue");
        if(queued > priv->max_queue_seen) {
            priv->max_queue_seen = queued;
        }
        answer_request(gobj, channel, n, ANSWER_SIZE_BIG);

    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A refused request reached the application",
            "step",         "%d", priv->step,
            NULL
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  At the server, a channel opens or closes; the close ends the steps of a
 *  peer that never reads
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj channel = (hgobj)(uintptr_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, 0);
    if(channel && priv->step == STEP_ACTIVE) {
        // Before the connection arms its timer: the inactivity of the step from the start
        gobj_write_integer_attr(gobj_bottom_gobj(channel), "timeout_inactivity", ACTIVE_INACTIVITY);
    }

    JSON_DECREF(kw)
    return 0;
}

PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj channel = (hgobj)(uintptr_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, 0);
    if(channel && (priv->step == STEP_IDLE || priv->step == STEP_ACTIVE)) {
        gobj_write_integer_attr(gobj_bottom_gobj(channel), "timeout_inactivity", 300);
    }

    /*
     *  What closes in steps 15 and 17 before their requests is the peer of
     *  the step before
     */
    BOOL of_this_step = ((priv->step != STEP_ASYNC_MUTE && priv->step != STEP_ARM_AFTER) ||
        (channel && channel == priv->held_channel));
    if(of_this_step && (priv->step == STEP_MUTE || priv->step == STEP_QUEUE || priv->step == STEP_HELD ||
            priv->step == STEP_SLOW || priv->step == STEP_LATE_ANSWERS ||
            priv->step == STEP_ASYNC_MUTE || priv->step == STEP_ARM_AFTER) && !priv->step_done) {
        priv->step_done = TRUE;
        gobj_post_event(gobj, EV_NEXT_STEP, json_object(), gobj);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tx_ready(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(kw)

    if(src == priv->gobj_client) {
        priv->gobj_client = 0;
    }
    if(gobj_parent(src) == gobj && gobj_gclass_name(src) == C_TCP) {
        gobj_destroy(src);  // a peer of a step that ended: its last act
    } else if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }

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
GOBJ_DEFINE_GCLASS(C_TEST1);

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
        {EV_ON_OPEN,                ac_on_open,                 0},
        {EV_ON_MESSAGE,             ac_on_message,              0},
        {EV_ON_CLOSE,               ac_on_close,                0},
        {EV_CONNECTED,              ac_connected,               0},
        {EV_RX_DATA,                ac_rx_data,                 0},
        {EV_TX_READY,               ac_tx_ready,                0},
        {EV_DISCONNECTED,           ac_disconnected,            0},
        {EV_NEXT_STEP,              ac_next_step,               0},
        {EV_TIMEOUT_PERIODIC,       ac_timeout,                 0},
        {EV_PEER_ANSWER,            ac_peer_answer,             0},
        {EV_TIMEOUT,                ac_timeout,                 0},
        {EV_STOPPED,                ac_stopped,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_OPEN,                0},
        {EV_ON_MESSAGE,             0},
        {EV_ON_CLOSE,               0},
        {EV_CONNECTED,              0},
        {EV_RX_DATA,                0},
        {EV_TX_READY,               0},
        {EV_DISCONNECTED,           0},
        {EV_NEXT_STEP,              0},
        {EV_TIMEOUT_PERIODIC,       0},
        {EV_PEER_ANSWER,            0},
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
PUBLIC int register_c_test1(void)
{
    return create_gclass(C_TEST1);
}
