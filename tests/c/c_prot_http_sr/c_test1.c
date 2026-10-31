/****************************************************************************
 *          C_TEST1.C
 *
 *          A C_PROT_HTTP_SR server and the peers it refuses.
 *
 *          The server is a C_IOGATE whose channels are C_PROT_HTTP_SR over
 *          C_TCP (max_body_size 1024, max_tx_queue 4, timeout_linger 500).
 *          This gobj is the application behind it. Four steps, each on a
 *          connection of its own:
 *
 *          1. 600 requests pipelined in one write; the application drops
 *             the connection at the 3rd. It gets 3 requests, no more, and
 *             nothing is logged: the rest of the bytes is of nobody.
 *          2. A request whose head is over max_header_size (64 KB): the
 *             peer reads "431 Request Header Fields Too Large", and the
 *             server drops the connection after timeout_linger. One
 *             warning.
 *          3. A request whose body is over max_body_size: "413 Content
 *             Too Large", the same way. One warning.
 *          4. A peer that sends 600 requests and never reads: the answers
 *             (100 KB each) pile up in the transport; at max_tx_queue the
 *             connection is dropped. One warning.
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

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void start_step(hgobj gobj);
PRIVATE void send_raw(hgobj gobj, const char *data, size_t len);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
GOBJ_DEFINE_EVENT(EV_NEXT_STEP);

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

    hgobj gobj_input_side;
    hgobj gobj_client;          // the peer of steps 1 to 3
    int fd_mute;                // the peer of step 4, that never reads
    int step;
    int requests;               // requests the application got in this step
    gbuffer_t *answer;          // what the peer read in this step
    BOOL step_done;
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

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->timer);
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

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBUFFER_DECREF(priv->answer)
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
 *  A peer for the step: a C_TCP that does not reconnect, or (step 4) a
 *  socket that never reads
 ***************************************************************************/
PRIVATE void start_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->step++;
    priv->requests = 0;
    priv->step_done = FALSE;
    gbuffer_reset_wr(priv->answer);
    gbuffer_reset_rd(priv->answer);

    if(priv->step <= 3) {
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

    } else if(priv->step == 4) {
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
            return;
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

    } else {
        set_yuno_must_die();
    }
}

/***************************************************************************
 *  What the peer read in steps 2 and 3: the status line it is due
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

    if(priv->step == 0) {
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

    if(priv->step == 1 && priv->requests != DROP_AT) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A dropped connection delivered requests after its drop",
            "requests",     "%d", priv->requests,
            "expected",     "%d", DROP_AT,
            NULL
        );
    } else if(priv->step == 2) {
        check_answer(gobj, "HTTP/1.1 431 Request Header Fields Too Large\r\n");
    } else if(priv->step == 3) {
        check_answer(gobj, "HTTP/1.1 413 Content Too Large\r\n");
    } else if(priv->step == 4) {
        if(priv->fd_mute >= 0) {
            close(priv->fd_mute);
            priv->fd_mute = -1;
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

    if(priv->step == 1) {
        const char *req = "GET /pipelined HTTP/1.1\r\nHost: x\r\n\r\n";
        size_t req_len = strlen(req);
        gbuffer_t *gbuf = gbuffer_create(req_len * PIPELINED, req_len * PIPELINED);
        for(int i = 0; gbuf && i < PIPELINED; i++) {
            gbuffer_append(gbuf, (void *)req, req_len);
        }
        if(gbuf) {
            gobj_send_event(priv->gobj_client, EV_TX_DATA,
                json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
                gobj
            );
        }

    } else if(priv->step == 2) {
        size_t len;
        const char *head = "GET / HTTP/1.1\r\nX-Big: ";
        len = strlen(head) + BIG_HEADER + 4;
        char *req = gbmem_malloc(len + 1);
        if(req) {
            memcpy(req, head, strlen(head));
            memset(req + strlen(head), 'a', BIG_HEADER);
            memcpy(req + strlen(head) + BIG_HEADER, "\r\n\r\n", 5);
            send_raw(gobj, req, len);
            GBMEM_FREE(req)
        }

    } else if(priv->step == 3) {
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
    if(gbuf) {
        gbuffer_append_gbuf(priv->answer, gbuf);
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

    if(src == priv->gobj_client && !priv->step_done) {
        priv->step_done = TRUE;
        gobj_post_event(gobj, EV_NEXT_STEP, json_object(), gobj);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  At the server, a request: dropped at the 3rd in step 1, answered with
 *  100 KB in step 4
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj channel = (hgobj)(uintptr_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    priv->requests++;

    if(priv->step == 1) {
        if(priv->requests == DROP_AT) {
            gobj_send_event(channel, EV_DROP, 0, gobj);
        }
    } else if(priv->step == 4) {
        char *body = gbmem_malloc(ANSWER_SIZE + 1);
        if(body) {
            memset(body, 'c', ANSWER_SIZE);
            body[ANSWER_SIZE] = 0;
            gobj_send_event(channel, EV_SEND_MESSAGE,
                json_pack("{s:s, s:s, s:s}", "code", "200 OK", "headers", "", "body", body),
                gobj
            );
            GBMEM_FREE(body)
        }
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
 *  At the server, a channel opens or closes; the close ends step 4
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->step == 4 && !priv->step_done) {
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
