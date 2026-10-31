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
 *             accepts): with max_tx_queue 2, the 4th request of a burst is
 *             refused, ONE warning, and the connection dropped.
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
#define BURST           8
#define BURST_BODY      (1024*1024)

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

    hgobj gobj_input_side;
    hgobj gobj_client;
    int fd_listen_mute;         // step 4: listens, never accepts
    int step;
    int requests;               // requests the server got in this step
    int responses;              // EV_ON_MESSAGE the client got in this step
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
    priv->received = gbuffer_create(64*1024, 8*1024*1024);
    priv->fd_listen_mute = -1;

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
    if(priv->fd_listen_mute >= 0) {
        close(priv->fd_listen_mute);
        priv->fd_listen_mute = -1;
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
    priv->step_done = FALSE;
    gbuffer_reset_wr(priv->received);
    gbuffer_reset_rd(priv->received);

    if(priv->step > 4) {
        set_yuno_must_die();
        return;
    }

    const char *url = CLIENT_URL;
    json_int_t max_tx_queue = 64;
    if(priv->step == 4) {
        priv->fd_listen_mute = listen_mute(gobj);
        if(priv->fd_listen_mute < 0) {
            set_yuno_must_die();
            return;
        }
        url = MUTE_URL;
        max_tx_queue = 2;
    }

    char name[32];
    snprintf(name, sizeof(name), "client-%d", priv->step);
    priv->gobj_client = gobj_create(
        name,
        C_PROT_HTTP_CL,
        json_pack("{s:s, s:I}", "url", url, "max_tx_queue", max_tx_queue),
        gobj
    );
    gobj_start(priv->gobj_client);
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
 *  Steps 2 and 3, at the server: the response that breaks the client
 ***************************************************************************/
PRIVATE void answer_bad_response(hgobj gobj, hgobj channel)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf;
    if(priv->step == 2) {
        gbuf = gbuffer_create(BIG_HEADER + 256, BIG_HEADER + 256);
        gbuffer_printf(gbuf, "HTTP/1.1 200 OK\r\nX-Big: ");
        for(int i = 0; i < BIG_HEADER; i++) {
            gbuffer_append(gbuf, "a", 1);
        }
        gbuffer_printf(gbuf, "\r\nContent-Length: 2\r\n\r\nok");
    } else {
        gbuf = str2gbuf("HTTP/1.1 200 OK\r\nX-A: b\rc\r\nContent-Length: 2\r\n\r\nok");
    }
    gobj_send_event(channel, EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
        gobj
    );
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

    if(priv->step == 3 && priv->requests != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A request answered with a broken response was sent again",
            "requests",     "%d", priv->requests,
            NULL
        );
    }
    if(priv->step == 4 && priv->fd_listen_mute >= 0) {
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
        if(priv->step == 1) {
            send_forms(gobj);

        } else if(priv->step == 2 || priv->step == 3) {
            gobj_send_event(priv->gobj_client, EV_SEND_MESSAGE,
                json_pack("{s:s, s:s, s:{s:s}}",
                    "method", "POST",
                    "resource", "/token",
                    "data", "grant_type", "password"
                ),
                gobj
            );

        } else if(priv->step == 4) {
            char *body = gbmem_malloc(BURST_BODY + 1);
            if(body) {
                memset(body, 'd', BURST_BODY);
                body[BURST_BODY] = 0;
                int refused = 0;
                for(int i = 0; i < BURST && !refused; i++) {
                    if(gobj_send_event(priv->gobj_client, EV_SEND_MESSAGE,
                            json_pack("{s:s, s:s, s:s}",
                                "method", "POST",
                                "resource", "/burst",
                                "data", body
                            ),
                            gobj) < 0) {
                        refused = i + 1;
                    }
                }
                GBMEM_FREE(body)
                if(refused != 4) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "The burst to a server that never reads was not cut at max_tx_queue",
                        "refused_at",   "%d", refused,
                        NULL
                    );
                }
            }
            // The listener goes, its queued connection is reset: the drop ends now
            close(priv->fd_listen_mute);
            priv->fd_listen_mute = -1;
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
        if(priv->step == 1) {
            bad = (status != 200);
            if(!priv->step_done) {
                priv->step_done = TRUE;
                gobj_post_event(gobj, EV_NEXT_STEP, json_object(), gobj);
            }
        } else if(priv->step == 2) {
            bad = kw_get_int(gobj, kw, "headers_too_large", 0, 0) != 65536 ||
                kw_has_key(kw, "body") || kw_has_key(kw, "gbuffer");
        } else if(priv->step == 3) {
            bad = empty_string(kw_get_str(gobj, kw, "protocol_error", "", 0)) ||
                kw_has_key(kw, "body") || kw_has_key(kw, "gbuffer");
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
            gbuffer_append_gbuf(priv->received, gbuf);
        }
        if(priv->step == 1) {
            if(priv->requests == 0 && check_forms(gobj, channel)) {
                priv->requests++;
            }
        } else if(priv->step == 2 || priv->step == 3) {
            if(strstr(gbuffer_cur_rd_pointer(priv->received), "\r\n\r\n")) {
                priv->requests++;
                gbuffer_reset_wr(priv->received);
                gbuffer_reset_rd(priv->received);
                answer_bad_response(gobj, channel);
            }
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The client lost its connection: steps 2 to 4 end
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_client && priv->step >= 2 && !priv->step_done) {
        priv->step_done = TRUE;
        if(priv->step <= 3 && priv->responses != 1) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The request waiting for a refused response was not answered once",
                "step",         "%d", priv->step,
                "responses",    "%d", priv->responses,
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
        {EV_ON_CLOSE,               ac_on_close,                0},
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
PUBLIC int register_c_test2(void)
{
    return create_gclass(C_TEST2);
}
