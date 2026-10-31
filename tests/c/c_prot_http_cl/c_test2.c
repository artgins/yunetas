/****************************************************************************
 *          C_TEST2.C
 *
 *          A C_PROT_HTTP_CL client: its forms, and the responses it
 *          refuses.
 *
 *          The server is raw (C_PROT_RAW): it collects the bytes of each
 *          request and answers with bytes written here. Steps, each with
 *          a client of its own:
 *
 *          1. Forms. A POST whose `data` holds an empty value, a number,
 *             a real, a boolean, a value with a NUL and a value of 6 MB
 *             goes as `empty=&n=5&r=1.5&t=true&nul=a%00b&big=xx...`, its
 *             Content-Length exact. A POST with `data` {} goes with
 *             Content-Length: 0 and no error. A form whose value is a
 *             null is not sent: ONE error.
 *          2. A response whose head passes max_header_size (64 KB):
 *             EV_ON_MESSAGE with `headers_too_large` and no body, ONE
 *             warning, the connection dropped.
 *          3. A response with a CR alone in a header: EV_ON_MESSAGE with
 *             `protocol_error`, ONE warning, the connection dropped; and
 *             the request is not sent again (the server gets it once).
 *          4. A server that never reads (a socket that listens and never
 *             accepts), max_tx_queue 3 and timeout_tx_stall 500: a request
 *             too long to leave stays in flight. The requests that find
 *             less than max_tx_queue waiting go; the first that finds them
 *             starts the wait; the one that finds that nothing was written
 *             in timeout_tx_stall is refused, ONE warning, and the
 *             connection dropped.
 *          5. A server that reads: 100 requests in one turn all go, and all
 *             are answered, with nothing logged.
 *          6. A whole response and then garbage: ONE EV_ON_MESSAGE, the
 *             good one, ONE warning, the connection dropped.
 *          7. Garbage alone: no EV_ON_MESSAGE, no response began. ONE
 *             warning, the connection dropped.
 *          8. A chunked response whose trailers repeat its headers: a
 *             string in each header, and the body read as json.
 *          9. raw_body_data: two responses on one connection, each with
 *             the headers of its own.
 *         10. raw_body_data and a head over max_header_size: nothing is
 *             published (a last EV_ON_MESSAGE would say the body is whole),
 *             ONE warning, the connection dropped.
 *         11. Three responses in one read, the client drops at the first with
 *             a write in flight, so the disconnection comes later, out of
 *             the drop: the other two are not published.
 *         12. A server that reads slowly, max_tx_queue 1 and timeout_tx_stall
 *             500: the requests take longer than that to leave, and none is
 *             refused, because writes keep ending. Nothing is logged.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "c_test2.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define CLIENT_URL      "http://127.0.0.1:7767"
#define MUTE_URL        "http://127.0.0.1:7768"
#define MUTE_PORT       7768
#define BIG_VALUE       (6*1000*1000)
#define BIG_HEADER      (70*1000)
#define STALLED_BODY    (8*1000*1000)   // more than the socket buffers take
#define MUTE_QUEUE      3
#define REFUSED_AT      6
#define TIMEOUT_STALL   500
#define READS           100
#define SLOW_POSTS      4
#define SLOW_BODY       (4*1000*1000)
#define SLOW_PIECE      (64*1024)
#define SLOW_QUEUE      1

enum {
    STEP_FORMS = 1,
    STEP_HEAD,
    STEP_PROTOCOL,
    STEP_MUTE,
    STEP_READS,
    STEP_GOOD_THEN_GARBAGE,
    STEP_GARBAGE,
    STEP_TRAILERS,
    STEP_RAW,
    STEP_RAW_REFUSED,
    STEP_DROP,
    STEP_SLOW,
    STEP_END
};

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void start_step(hgobj gobj);

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
    hgobj timer_tick;           // steps 4 and 12: the next request
    hgobj timer_read;           // step 12: the slow server reads

    hgobj gobj_input_side;
    hgobj gobj_client;
    int fd_listen_mute;         // step 4: listens, never accepts
    int fd_slow;                // step 12: the server that reads slowly
    size_t slow_read;           // step 12: the bytes it read
    int sent;                   // step 4: requests sent
    int refused_at;             // step 4: the request refused
    int step;
    int requests;               // requests the server got in this step
    int responses;              // EV_ON_MESSAGE the client got in this step
    int heads;                  // EV_ON_HEADER the client got in this step
    int messages_ended;         // the last EV_ON_MESSAGE of a raw response
    gbuffer_t *received;        // the bytes the server got in this step
    BOOL step_done;
    char *expected_form;
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
    priv->timer_tick = gobj_create_pure_child("tick", C_TIMER, 0, gobj);
    priv->timer_read = gobj_create_pure_child("read", C_TIMER0, 0, gobj);
    priv->received = gbuffer_create(64*1024, 8*1024*1024);
    priv->fd_listen_mute = -1;
    priv->fd_slow = -1;

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
    gobj_start(priv->timer_read);

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
    gobj_stop(priv->timer_read);
    if(priv->gobj_client && gobj_is_running(priv->gobj_client)) {
        gobj_stop(priv->gobj_client);
    }
    if(priv->fd_listen_mute >= 0) {
        close(priv->fd_listen_mute);
        priv->fd_listen_mute = -1;
    }
    if(priv->fd_slow >= 0) {
        close(priv->fd_slow);
        priv->fd_slow = -1;
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
    clear_timeout(priv->timer_tick);
    clear_timeout0(priv->timer_read);

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBUFFER_DECREF(priv->received)
    GBMEM_FREE(priv->expected_form)
}



                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  A socket that listens and never accepts: a server that never reads
 ***************************************************************************/
PRIVATE int listen_mute(hgobj gobj)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int on = 1;
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(MUTE_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if(fd < 0 ||
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0 ||
            bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
            listen(fd, 4) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "The server that never reads cannot listen",
            "errno",        "%s", strerror(errno),
            NULL
        );
        if(fd >= 0) {
            close(fd);
        }
        return -1;
    }
    return fd;
}

/***************************************************************************
 *  A client for the step
 ***************************************************************************/
PRIVATE void start_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->step++;
    priv->requests = 0;
    priv->responses = 0;
    priv->heads = 0;
    priv->messages_ended = 0;
    priv->sent = 0;
    priv->refused_at = 0;
    priv->step_done = FALSE;
    gbuffer_reset_wr(priv->received);
    gbuffer_reset_rd(priv->received);

    if(priv->step >= STEP_END) {
        set_yuno_must_die();
        return;
    }

    const char *url = CLIENT_URL;
    json_t *kw_client = json_pack("{s:I}", "max_tx_queue", (json_int_t)64);
    if(priv->step == STEP_MUTE || priv->step == STEP_SLOW) {
        priv->fd_listen_mute = listen_mute(gobj);
        if(priv->fd_listen_mute < 0) {
            JSON_DECREF(kw_client)
            set_yuno_must_die();
            return;
        }
        url = MUTE_URL;
        json_object_set_new(kw_client, "max_tx_queue",
            json_integer((priv->step == STEP_MUTE)? MUTE_QUEUE : SLOW_QUEUE)
        );
        json_object_set_new(kw_client, "timeout_tx_stall", json_integer(TIMEOUT_STALL));
    } else if(priv->step == STEP_RAW || priv->step == STEP_RAW_REFUSED) {
        json_object_set_new(kw_client, "raw_body_data", json_true());
    }
    json_object_set_new(kw_client, "url", json_string(url));

    char name[32];
    snprintf(name, sizeof(name), "client-%d", priv->step);
    priv->gobj_client = gobj_create(name, C_PROT_HTTP_CL, kw_client, gobj);
    gobj_start(priv->gobj_client);
}

/***************************************************************************
 *  A POST with a short body
 ***************************************************************************/
PRIVATE int send_post(hgobj gobj, const char *resource, const char *body)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return gobj_send_event(priv->gobj_client, EV_SEND_MESSAGE,
        json_pack("{s:s, s:s, s:s}", "method", "POST", "resource", resource, "data", body),
        gobj
    );
}

/***************************************************************************
 *  A POST of `size` bytes
 ***************************************************************************/
PRIVATE void send_big_post(hgobj gobj, size_t size)
{
    char *body = gbmem_malloc(size + 1);
    if(!body) {
        return; // Error already logged
    }
    memset(body, 'd', size);
    body[size] = 0;
    send_post(gobj, "/big", body);
    GBMEM_FREE(body)
}

/***************************************************************************
 *  Step 12: the server reads one piece; with all the bytes the step ends
 ***************************************************************************/
PRIVATE void read_slowly(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char *bytes = gbmem_malloc(SLOW_PIECE);
    if(!bytes) {
        return; // Error already logged
    }
    ssize_t n = recv(priv->fd_slow, bytes, SLOW_PIECE, MSG_DONTWAIT);
    GBMEM_FREE(bytes)
    if(n > 0) {
        priv->slow_read += (size_t)n;
    }
    if(n == 0 || priv->slow_read >= SLOW_POSTS*SLOW_BODY) {
        clear_timeout0(priv->timer_read);
        clear_timeout(priv->timer_tick);
        if(!priv->step_done) {
            priv->step_done = TRUE;
            gobj_post_event(gobj, EV_NEXT_STEP, json_object(), gobj);
        }
    }
}

/***************************************************************************
 *  Step 4: one more request to the server that never reads
 ***************************************************************************/
PRIVATE void send_next_to_mute(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->sent++;
    if(send_post(gobj, "/next", "x") < 0) {
        priv->refused_at = priv->sent;
        clear_timeout(priv->timer_tick);
        if(priv->refused_at != REFUSED_AT) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The server that never reads was not cut when it is due",
                "refused_at",   "%d", priv->refused_at,
                "expected",     "%d", REFUSED_AT,
                NULL
            );
        }
        // A request after the drop is no second warning of the server
        send_post(gobj, "/next", "x");
        // The listener goes, its queued connection is reset: the drop ends now
        close(priv->fd_listen_mute);
        priv->fd_listen_mute = -1;
    }
}

/***************************************************************************
 *  Step 1: the forms
 ***************************************************************************/
PRIVATE void send_forms(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char *big = gbmem_malloc(BIG_VALUE + 1);
    if(!big) {
        return; // Error already logged
    }
    memset(big, 'x', BIG_VALUE);
    big[BIG_VALUE] = 0;

    json_t *data = json_object();
    json_object_set_new(data, "empty", json_string(""));
    json_object_set_new(data, "n", json_integer(5));
    json_object_set_new(data, "r", json_real(1.5));
    json_object_set_new(data, "t", json_true());
    json_object_set_new(data, "nul", json_stringn("a\0b", 3));
    json_object_set_new(data, "big", json_string(big));

    size_t prefix_len = strlen("empty=&n=5&r=1.5&t=true&nul=a%00b&big=");
    priv->expected_form = gbmem_malloc(prefix_len + BIG_VALUE + 1);
    if(priv->expected_form) {
        memcpy(priv->expected_form, "empty=&n=5&r=1.5&t=true&nul=a%00b&big=", prefix_len);
        memcpy(priv->expected_form + prefix_len, big, BIG_VALUE + 1);
    }
    GBMEM_FREE(big)

    gobj_send_event(priv->gobj_client, EV_SEND_MESSAGE,
        json_pack("{s:s, s:s, s:{s:s}, s:o}",
            "method", "POST",
            "resource", "/form",
            "headers", "Content-Type", "application/x-www-form-urlencoded",
            "data", data
        ),
        gobj
    );

    gobj_send_event(priv->gobj_client, EV_SEND_MESSAGE,
        json_pack("{s:s, s:s, s:{s:s}, s:{}}",
            "method", "POST",
            "resource", "/empty",
            "headers", "Content-Type", "application/x-www-form-urlencoded",
            "data"
        ),
        gobj
    );

    int ret = gobj_send_event(priv->gobj_client, EV_SEND_MESSAGE,
        json_pack("{s:s, s:s, s:{s:s}, s:{s:n}}",
            "method", "POST",
            "resource", "/null",
            "headers", "Content-Type", "application/x-www-form-urlencoded",
            "data", "bad"
        ),
        gobj
    );
    if(ret != -1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A form with a null value was not refused",
            NULL
        );
    }
}

/***************************************************************************
 *  Step 1, at the server: the two requests whole
 ***************************************************************************/
PRIVATE BOOL check_forms(hgobj gobj, hgobj channel)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char *text = gbuffer_cur_rd_pointer(priv->received);
    size_t form_len = priv->expected_form? strlen(priv->expected_form) : 0;

    if(gbuffer_leftbytes(priv->received) < form_len) {
        return FALSE;   // the rest is on its way
    }
    char *second = strstr(text, "POST /empty HTTP/1.1\r\n");
    if(!second || !strstr(second, "\r\n\r\n")) {
        return FALSE;   // the rest is on its way
    }

    char cl[64];
    snprintf(cl, sizeof(cl), "Content-Length: %lu\r\n", (unsigned long)form_len);
    char *body = strstr(text, "\r\n\r\n");
    if(strncmp(text, "POST /form HTTP/1.1\r\n", 21) != 0 || !strstr(text, cl) || !body ||
            (size_t)(second - (body + 4)) != form_len ||
            memcmp(body + 4, priv->expected_form, form_len) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The form did not reach the server as it is due",
            "head",         "%.200s", text,
            "body",         "%.80s", body? body + 4 : "",
            "expected",     "%.80s", priv->expected_form? priv->expected_form : "",
            NULL
        );
    }
    if(!strstr(second, "Content-Length: 0\r\n")) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The empty form did not go with Content-Length: 0",
            "request",      "%.200s", second,
            NULL
        );
    }
    if(strstr(text, "/null")) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The form with a null value was sent",
            NULL
        );
    }

    gbuffer_t *gbuf = str2gbuf("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");
    gobj_send_event(channel, EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
        gobj
    );
    return TRUE;
}

/***************************************************************************
 *  At the server, the response the step is about
 ***************************************************************************/
PRIVATE void answer_bad_response(hgobj gobj, hgobj channel)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf;
    if(priv->step == STEP_HEAD || priv->step == STEP_RAW_REFUSED) {
        gbuf = gbuffer_create(BIG_HEADER + 256, BIG_HEADER + 256);
        gbuffer_printf(gbuf, "HTTP/1.1 200 OK\r\nX-Two: a\r\nX-Two: b\r\nX-Big: ");
        for(int i = 0; i < BIG_HEADER; i++) {
            gbuffer_append(gbuf, "a", 1);
        }
        gbuffer_printf(gbuf, "\r\nContent-Length: 2\r\n\r\nok");
    } else if(priv->step == STEP_PROTOCOL) {
        gbuf = str2gbuf("HTTP/1.1 200 OK\r\nX-Two: a\r\nX-Two: b\r\nX-A: b\rc\r\nContent-Length: 2\r\n\r\nok");
    } else if(priv->step == STEP_GOOD_THEN_GARBAGE) {
        gbuf = str2gbuf("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nokGARBAGE\r\n");
    } else if(priv->step == STEP_GARBAGE) {
        gbuf = str2gbuf("GARBAGE\r\n");
    } else if(priv->step == STEP_TRAILERS) {
        gbuf = str2gbuf(
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nX-A: one\r\n"
            "Transfer-Encoding: chunked\r\n\r\n"
            "2\r\n{}\r\n0\r\nContent-Type: text/plain\r\nX-A: two\r\nX-A: three\r\n\r\n"
        );
    } else if(priv->step == STEP_RAW) {
        gbuf = str2gbuf(
            "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok"
            "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nokok"
        );
    } else {
        gbuf = str2gbuf(
            "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok"
            "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok"
            "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok"
        );
    }
    gobj_send_event(channel, EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
        gobj
    );
}

/***************************************************************************
 *  Step 5, at the server: each request whole, answered
 ***************************************************************************/
PRIVATE void answer_requests(hgobj gobj, hgobj channel)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char *text = gbuffer_cur_rd_pointer(priv->received);
    char *end;
    while((end = strstr(text, "\r\n\r\n"))) {
        gbuffer_get(priv->received, (size_t)(end + 4 - text));
        text = gbuffer_cur_rd_pointer(priv->received);
        priv->requests++;
        gbuffer_t *gbuf = str2gbuf("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");
        gobj_send_event(channel, EV_SEND_MESSAGE,
            json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
            gobj
        );
    }
}

/***************************************************************************
 *  A header of a kw, that must be a string
 ***************************************************************************/
PRIVATE void check_header(hgobj gobj, json_t *kw, const char *name, const char *expected)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_value = json_object_get(json_object_get(kw, "headers"), name);
    if(!json_is_string(jn_value) || strcmp(json_string_value(jn_value), expected) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A header is not the string it is due",
            "step",         "%d", priv->step,
            "header",       "%s", name,
            "expected",     "%s", expected,
            NULL
        );
        gobj_trace_json(gobj, kw, "A header is not the string it is due");
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

    if(src == priv->timer_read) {
        read_slowly(gobj);
    } else if(src == priv->timer_tick) {
        if(priv->step == STEP_SLOW) {
            if(send_post(gobj, "/next", "x") < 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "A request to a server that reads slowly was refused",
                    NULL
                );
                clear_timeout(priv->timer_tick);
            }
        } else {
            send_next_to_mute(gobj);
        }
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
            "responses",    "%d", priv->responses,
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

    clear_timeout(priv->timer_tick);
    clear_timeout0(priv->timer_read);

    if(priv->step == STEP_PROTOCOL && priv->requests != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A request answered with a broken response was sent again",
            "requests",     "%d", priv->requests,
            NULL
        );
    }
    if(priv->step == STEP_READS && (priv->requests != READS || priv->responses != READS)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A server that reads did not get all the requests",
            "requests",     "%d", priv->requests,
            "responses",    "%d", priv->responses,
            "expected",     "%d", READS,
            NULL
        );
    }
    if(priv->step == STEP_SLOW) {
        if(priv->slow_read < SLOW_POSTS*SLOW_BODY) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A server that reads slowly was dropped",
                "slow_read",    "%lu", (unsigned long)priv->slow_read,
                NULL
            );
        }
        if(priv->fd_slow >= 0) {
            close(priv->fd_slow);
            priv->fd_slow = -1;
        }
    }
    if((priv->step == STEP_MUTE || priv->step == STEP_SLOW) && priv->fd_listen_mute >= 0) {
        close(priv->fd_listen_mute);
        priv->fd_listen_mute = -1;
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
 *  The client is connected: what it sends in its step
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_client) {
        if(priv->step == STEP_FORMS) {
            send_forms(gobj);

        } else if(priv->step == STEP_MUTE) {
            /*
             *  The first goes in flight and never ends. In one turn nothing
             *  is ever written, so none of these can be refused.
             */
            send_big_post(gobj, STALLED_BODY);
            send_post(gobj, "/next", "x");
            send_post(gobj, "/next", "x");
            priv->sent = 3;
            set_timeout_periodic(priv->timer_tick, TIMEOUT_STALL + 100);

        } else if(priv->step == STEP_READS) {
            for(int i = 0; i < READS; i++) {
                if(gobj_send_event(priv->gobj_client, EV_SEND_MESSAGE,
                        json_pack("{s:s, s:s}", "method", "GET", "resource", "/reads"),
                        gobj) < 0) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "A request to a server that reads was refused",
                        "request",      "%d", i + 1,
                        NULL
                    );
                    break;
                }
            }

        } else if(priv->step == STEP_DROP) {
            send_big_post(gobj, STALLED_BODY);

        } else if(priv->step == STEP_SLOW) {
            for(int i = 0; i < SLOW_POSTS; i++) {
                send_big_post(gobj, SLOW_BODY);
            }
            priv->fd_slow = accept(priv->fd_listen_mute, NULL, NULL);
            if(priv->fd_slow < 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "The server that reads slowly cannot accept",
                    "errno",        "%s", strerror(errno),
                    NULL
                );
                set_yuno_must_die();
            } else {
                set_timeout_periodic0(priv->timer_read, 10);
                set_timeout_periodic(priv->timer_tick, TIMEOUT_STALL + 100);
            }

        } else {
            send_post(gobj, "/token", "password");
        }
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  At the client, a response; at the server, the bytes of the requests
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_client) {
        priv->responses++;
        int status = (int)kw_get_int(gobj, kw, "response_status_code", 0, 0);
        BOOL bad = FALSE;
        if(priv->step == STEP_FORMS) {
            bad = (status != 200);
            if(!priv->step_done) {
                priv->step_done = TRUE;
                gobj_post_event(gobj, EV_NEXT_STEP, json_object(), gobj);
            }
        } else if(priv->step == STEP_HEAD) {
            bad = kw_get_int(gobj, kw, "headers_too_large", 0, 0) != 65536 ||
                kw_has_key(kw, "body") || kw_has_key(kw, "gbuffer");
            check_header(gobj, kw, "X-TWO", "a, b");
        } else if(priv->step == STEP_PROTOCOL) {
            bad = empty_string(kw_get_str(gobj, kw, "protocol_error", "", 0)) ||
                kw_has_key(kw, "body") || kw_has_key(kw, "gbuffer");
            check_header(gobj, kw, "X-TWO", "a, b");
        } else if(priv->step == STEP_READS) {
            bad = (status != 200);
            if(priv->responses == READS && !priv->step_done) {
                priv->step_done = TRUE;
                gobj_post_event(gobj, EV_NEXT_STEP, json_object(), gobj);
            }
        } else if(priv->step == STEP_GOOD_THEN_GARBAGE) {
            bad = (status != 200) || kw_has_key(kw, "protocol_error");
        } else if(priv->step == STEP_GARBAGE || priv->step == STEP_RAW_REFUSED) {
            bad = TRUE;
        } else if(priv->step == STEP_TRAILERS) {
            bad = !json_is_object(json_object_get(kw, "body"));
            check_header(gobj, kw, "X-A", "one, two, three");
            check_header(gobj, kw, "CONTENT-TYPE", "application/json, text/plain");
            if(!priv->step_done) {
                priv->step_done = TRUE;
                gobj_post_event(gobj, EV_NEXT_STEP, json_object(), gobj);
            }
        } else if(priv->step == STEP_RAW) {
            if(!kw_has_key(kw, "__pbf__")) {
                priv->messages_ended++;
                if(priv->messages_ended == 2 && !priv->step_done) {
                    priv->step_done = TRUE;
                    gobj_post_event(gobj, EV_NEXT_STEP, json_object(), gobj);
                }
            }
        } else if(priv->step == STEP_DROP) {
            if(priv->responses == 1) {
                gobj_send_event(priv->gobj_client, EV_DROP, 0, gobj);
            } else {
                bad = TRUE;
            }
        }
        if(bad) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The client got a response it is not due",
                "step",         "%d", priv->step,
                NULL
            );
            gobj_trace_json(gobj, kw, "The client got a response it is not due");
        }

    } else if(src == priv->gobj_input_side) {
        hgobj channel = (hgobj)(uintptr_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
        gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED);
        if(gbuf) {
            if(priv->step == STEP_DROP && priv->requests > 0) {
                gbuffer_clear(gbuf);    // the rest of a long body
            }
            gbuffer_append_gbuf(priv->received, gbuf);
        }
        if(priv->step == STEP_FORMS) {
            if(priv->requests == 0 && check_forms(gobj, channel)) {
                priv->requests++;
            }
        } else if(priv->step == STEP_READS) {
            answer_requests(gobj, channel);
        } else if(strstr(gbuffer_cur_rd_pointer(priv->received), "\r\n\r\n")) {
            priv->requests++;
            gbuffer_reset_wr(priv->received);
            gbuffer_reset_rd(priv->received);
            if(priv->requests == 1) {
                answer_bad_response(gobj, channel);
            }
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  raw_body_data: the head of a response, with the headers of its own
 ***************************************************************************/
PRIVATE int ac_on_header(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->heads++;
    if(priv->step == STEP_RAW) {
        check_header(gobj, kw, "CONTENT-LENGTH", (priv->heads == 1)? "2" : "4");
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The client lost its connection: the steps of a refused response end
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_client && priv->step != STEP_FORMS && priv->step < STEP_SLOW &&
            !priv->step_done) {
        priv->step_done = TRUE;
        int expected = 1;
        if(priv->step == STEP_MUTE || priv->step == STEP_GARBAGE || priv->step == STEP_RAW_REFUSED) {
            expected = 0;
        }
        if(priv->responses != expected) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Not the EV_ON_MESSAGE that are due before the close",
                "step",         "%d", priv->step,
                "responses",    "%d", priv->responses,
                "expected",     "%d", expected,
                NULL
            );
        }
        if(priv->step == STEP_MUTE && priv->refused_at != REFUSED_AT) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The connection to a server that never reads ended by itself",
                NULL
            );
        }
        gobj_post_event(gobj, EV_NEXT_STEP, json_object(), gobj);
    }

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
    if(gobj_parent(src) == gobj && gobj_gclass_name(src) == C_PROT_HTTP_CL) {
        gobj_destroy(src);  // the client of a step that ended
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
GOBJ_DEFINE_GCLASS(C_TEST2);

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
        {EV_ON_HEADER,              ac_on_header,               0},
        {EV_ON_CLOSE,               ac_on_close,                0},
        {EV_NEXT_STEP,              ac_next_step,               0},
        {EV_TIMEOUT_PERIODIC,       ac_timeout,                 0},
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
        {EV_ON_HEADER,              0},
        {EV_ON_CLOSE,               0},
        {EV_NEXT_STEP,              0},
        {EV_TIMEOUT_PERIODIC,       0},
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
PUBLIC int register_c_test2(void)
{
    return create_gclass(C_TEST2);
}
