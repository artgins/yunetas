/****************************************************************************
 *          C_TEST1.C
 *
 *          A C_PROT_HTTP_CL client and a GET with parameters.
 *
 *          A GET carries its parameters in the query of the resource, and
 *          C_PROT_HTTP_CL builds it from `data` (a dict) or from `query` (a
 *          text) when the Content-Type is application/x-www-form-urlencoded.
 *          Up to 7.26.9 the buffer of the new resource had no room for the
 *          '?', and the query lost its last character with no log.
 *
 *          The server is raw (C_PROT_RAW): it collects the bytes of the
 *          requests and checks their request lines.
 *          - `data` {"a": "1", "b": "xyz"} -> GET /path?a=1&b=xyz
 *          - `query` "q=hello"             -> GET /search?q=hello
 *          - neither                       -> GET /plain (no '?')
 *          - `data` {"user": "user+1@x.com", "pass": "p a%41&b=c~-._*"}
 *            -> GET /form?user=user%2B1%40x.com&pass=p+a%2541%26b%3Dc~-._%2A
 *            each key and value form-urlencoded, so a "+" is not a space
 *            for the server and a "&" does not begin another field.
 *          And a POST whose `data` is a json string, "hello": its body is
 *          "\"hello\"". Up to 7.26.9 json_dumps() refused a json that is
 *          not a dict or a list, and the client crashed on its NULL.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test1.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define CLIENT_URL      "http://127.0.0.1:7744"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE const char *expected_lines[] = {
    "GET /path?a=1&b=xyz HTTP/1.1",
    "GET /search?q=hello HTTP/1.1",
    "GET /plain HTTP/1.1",
    "GET /form?user=user%2B1%40x.com&pass=p+a%2541%26b%3Dc~-._%2A HTTP/1.1",
    "POST /echo HTTP/1.1",
    NULL
};
#define EXPECTED_REQUESTS   5
#define EXPECTED_BODY       "\"hello\""   // the last request's: a json string

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
    gbuffer_t *received;        // the bytes the server got, all the requests
    BOOL checked;
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
    priv->received = gbuffer_create(4*1024, 64*1024);

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

    set_timeout(priv->timer, priv->timeout); // then the client connects

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
}



                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  One GET to the server, form-urlencoded
 ***************************************************************************/
PRIVATE void send_get(hgobj gobj, const char *resource, const char *query, json_t *data)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = json_pack("{s:s, s:s, s:s, s:{s:s}}",
        "method", "GET",
        "resource", resource,
        "query", query,
        "headers",
            "Content-Type", "application/x-www-form-urlencoded"
    );
    if(data) {
        json_object_set_new(kw, "data", data);
    }
    gobj_send_event(priv->gobj_client, EV_SEND_MESSAGE, kw, gobj);
}

/***************************************************************************
 *  The request lines the server got, once all of them arrived
 ***************************************************************************/
PRIVATE void check_request_lines(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t len = gbuffer_leftbytes(priv->received);
    char *text = gbmem_strndup(gbuffer_cur_rd_pointer(priv->received), len);
    if(!text) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "gbmem_strndup() FAILED",
            NULL
        );
        return;
    }

    int ends = 0;
    char *last_end = NULL;
    for(char *e = strstr(text, "\r\n\r\n"); e; e = strstr(e + 4, "\r\n\r\n")) {
        ends++;
        last_end = e;
    }
    if(ends < EXPECTED_REQUESTS || strlen(last_end + 4) < strlen(EXPECTED_BODY)) {
        GBMEM_FREE(text)
        return;     // the rest is on its way
    }
    if(strcmp(last_end + 4, EXPECTED_BODY)!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The POST did not carry its json string body",
            "body",         "%s", last_end + 4,
            "expected",     "%s", EXPECTED_BODY,
            NULL
        );
    }

    int found = 0;
    char *p = text;
    while(p && *p) {
        char *eol = strstr(p, "\r\n");
        if(!eol) {
            break;
        }
        *eol = 0;
        if(strncmp(p, "GET ", 4)==0 || strncmp(p, "POST ", 5)==0) {
            const char *expected = found < EXPECTED_REQUESTS? expected_lines[found] : NULL;
            if(!expected || strcmp(p, expected)!=0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "The request did not carry its query whole",
                    "request_line", "%s", p,
                    "expected",     "%s", expected? expected : "no more requests",
                    NULL
                );
            }
            found++;
        }
        p = eol + 2;
    }
    GBMEM_FREE(text)

    if(found != EXPECTED_REQUESTS) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The server did not get the requests sent",
            "found",        "%d", found,
            NULL
        );
    }
    priv->checked = TRUE;
    set_yuno_must_die();
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  First timeout: the server listens, the client connects.
 *  Any other: the requests never arrived whole.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->gobj_client) {
        priv->gobj_client = gobj_create(
            "client",
            C_PROT_HTTP_CL,
            json_pack("{s:s}", "url", CLIENT_URL),
            gobj
        );
        gobj_start(priv->gobj_client);
        set_timeout(priv->timer, 5*priv->timeout);

    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The requests did not reach the server",
            "received",     "%lu", (unsigned long)gbuffer_leftbytes(priv->received),
            NULL
        );
        set_yuno_must_die();
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The client is connected: the four GETs, and the POST
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_client) {
        send_get(gobj, "/path", "", json_pack("{s:s, s:s}", "a", "1", "b", "xyz"));
        send_get(gobj, "/search", "q=hello", NULL);
        send_get(gobj, "/plain", "", NULL);
        send_get(gobj, "/form", "",
            json_pack("{s:s, s:s}", "user", "user+1@x.com", "pass", "p a%41&b=c~-._*"));
        gobj_send_event(priv->gobj_client, EV_SEND_MESSAGE,
            json_pack("{s:s, s:s, s:s}", "method", "POST", "resource", "/echo", "data", "hello"),
            gobj
        );
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  At the server, the bytes of the requests
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side && !priv->checked) {
        gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED);
        if(gbuf) {
            gbuffer_append_gbuf(priv->received, gbuf);
        }
        check_request_lines(gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)

    if(gobj_is_volatil(src)) {
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
