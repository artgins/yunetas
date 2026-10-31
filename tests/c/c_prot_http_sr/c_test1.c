/****************************************************************************
 *          C_TEST1.C
 *
 *          A C_PROT_HTTP_SR server and the peers it refuses.
 *
 *          The server is a C_IOGATE whose channels are C_PROT_HTTP_SR over
 *          C_TCP (max_body_size 1024, max_tx_queue 4, timeout_tx_stall 500,
 *          max_rx_held 65536, timeout_linger 500). This gobj is the
 *          application behind it. Steps, each on a connection of its own:
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
 *          8. Six requests and, behind them in the same write, a head over
 *             the limit, with an application that answers them later, all
 *             in one turn: the peer reads the six 200 and then the 431, and
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
#define ORDER_REQUESTS      6
#define IDLE_REQUESTS       10
#define LATE_REQUESTS       30
#define IDLE_INACTIVITY     3           // seconds, in step 12
#define IDLE_NOT_AFTER      8000        // milliseconds: timeout_tx_stall + that, in seconds of accuracy
#define TIMEOUT_LINGER      500         // see main_test1.c
#define LINGER_MARGIN       100         // the peer reads the answer after it was sent

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
    addr.sin_port = htons(SERVER_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
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

    } else {
        char name[32];
        snprintf(name, sizeof(name), "client-%d", priv->step);
        priv->gobj_client = gobj_create_pure_child(
            name,
            C_TCP,
            json_pack("{s:s, s:i}",
                "url", SERVER_URL,
                "timeout_between_connections", 600000
            ),
            gobj
        );
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

    } else if(priv->step == STEP_ORDER) {
        clear_timeout0(priv->timer_tick);
        for(int i = 0; i < ORDER_REQUESTS; i++) {
            answer_request(gobj, priv->held_channel, i, 16);
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
    } else if(priv->step == STEP_READS || priv->step == STEP_READS_BIG) {
        int expected = (priv->step == STEP_READS)? READS_TOTAL : READS_BIG;
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
    } else if(priv->step == STEP_MUTE || priv->step == STEP_QUEUE || priv->step == STEP_HELD ||
            priv->step == STEP_LATE_ANSWERS) {
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
        gobj_stop(priv->gobj_client);
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

    } else if(priv->step == STEP_READS || priv->step == STEP_READS_BIG || priv->step == STEP_IDLE) {
        if(priv->peer_parser) {
            ghttp_parser_destroy(priv->peer_parser);
        }
        priv->peer_parser = ghttp_parser_create(gobj, HTTP_RESPONSE, 0, 0, EV_PEER_ANSWER, TRUE);
        if(priv->step == STEP_IDLE) {
            send_pipelined_requests(gobj, IDLE_REQUESTS);
        } else {
            send_pipelined_requests(gobj, (priv->step == STEP_READS)? READS_FIRST : READS_BIG);
        }

    } else if(priv->step == STEP_HEAD || priv->step == STEP_ORDER) {
        const char *first = (priv->step == STEP_ORDER)?
            "GET /1 HTTP/1.1\r\nHost: x\r\n\r\nGET /2 HTTP/1.1\r\nHost: x\r\n\r\n"
            "GET /3 HTTP/1.1\r\nHost: x\r\n\r\nGET /4 HTTP/1.1\r\nHost: x\r\n\r\n"
            "GET /5 HTTP/1.1\r\nHost: x\r\n\r\nGET /6 HTTP/1.1\r\nHost: x\r\n\r\n" : "";
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
            priv->step == STEP_IDLE)) {
        ghttp_parser_received(priv->peer_parser, gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf));

    } else if(gbuf) {
        gbuffer_append_gbuf(priv->answer, gbuf);
        if(priv->step == STEP_HEAD && !priv->linger_timer) {
            priv->linger_timer = start_msectimer(TIMEOUT_LINGER - LINGER_MARGIN);
            set_timeout_periodic0(priv->timer_tick, 50);
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
    int expected = (priv->step == STEP_READS)? READS_TOTAL : READS_BIG;
    if(priv->answers == expected && !priv->step_done) {
        priv->step_done = TRUE;
        gobj_post_event(gobj, EV_NEXT_STEP, json_object(), gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The peer of steps 1 to 3 lost its connection: the step ends
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->peer_connected = FALSE;

    if(src == priv->gobj_client && !priv->step_done) {
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

    } else if(priv->step == STEP_ORDER) {
        priv->held_channel = channel;
        if(priv->requests == ORDER_REQUESTS) {
            set_timeout_periodic0(priv->timer_tick, 100);
        }

    } else if(priv->step == STEP_LATE_ANSWERS) {
        priv->held_channel = channel;
        answer_request(gobj, channel, n, ANSWER_SIZE_BIG);

    } else if(priv->step == STEP_READS) {
        answer_request(gobj, channel, n, 16);

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
    JSON_DECREF(kw)
    return 0;
}

PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj channel = (hgobj)(uintptr_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, 0);
    if(channel && priv->step == STEP_IDLE) {
        gobj_write_integer_attr(gobj_bottom_gobj(channel), "timeout_inactivity", 300);
    }

    if((priv->step == STEP_MUTE || priv->step == STEP_QUEUE || priv->step == STEP_HELD ||
            priv->step == STEP_SLOW || priv->step == STEP_LATE_ANSWERS) && !priv->step_done) {
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
