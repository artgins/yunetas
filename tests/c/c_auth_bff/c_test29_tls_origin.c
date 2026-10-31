/****************************************************************************
 *          C_TEST29_TLS_ORIGIN.C
 *
 *          Driver for test29_tls_origin: a BFF that ends TLS itself, with no
 *          allowed_origin, and with an end_session_endpoint that is no URL.
 *          One C_PROT_HTTP_CL over TLS to it, one request at a time:
 *            1. a login from "http://" and the Host: 403 origin_not_allowed
 *               (the connection is TLS, so that Origin is another origin);
 *            2. a login from "https://" and the Host: 200;
 *            3. a logout from "https://" and the Host: 500 auth_config_error,
 *               the cookies cleared (the HTTP client keeps one Set-Cookie,
 *               the last, the refresh_token's): the job has no URL to call,
 *               and the browser is answered all the same.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#include <yunetas.h>

#include "c_test29_tls_origin.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TEST_NAME       "test29_tls_origin"
#define NSTEPS          3

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type----------name-------------flag----default-description*/
SDATA (DTP_STRING,    "bff_host",      SDF_RD, "",     "host:port of the BFF, as the HTTP client says it in its Host"),
SDATA (DTP_POINTER,   "user_data",     0,      0,      "user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    {"messages",    "Trace test driver flow"},
    {0, 0}
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj   timer;
    hgobj   gobj_http_cl;   /* HTTP client over TLS to the BFF */
    BOOL    dying;          /* TRUE once a death-timer is armed */
    int     step;           /* step whose answer is awaited */
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

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER0, 0, gobj);  // C_TIMER counts in seconds
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

    if(priv->gobj_http_cl && gobj_is_running(priv->gobj_http_cl)) {
        gobj_stop_tree(priv->gobj_http_cl);
    }
    gobj_stop(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout0(priv->timer, 500);
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout0(priv->timer);
    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  The request of the current step
 ***************************************************************************/
PRIVATE void post_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char origin[128];
    snprintf(origin, sizeof(origin), "%s://%s", priv->step == 0? "http" : "https",
        gobj_read_str_attr(gobj, "bff_host"));
    json_t *jn_headers = json_pack("{s:s, s:s}",
        "Origin",       origin,
        "Content-Type", "application/json"
    );
    json_t *jn_data;
    if(priv->step == 2) {
        json_object_set_new(jn_headers, "Cookie", json_string("refresh_token=RT1"));
        jn_data = json_object();
    } else {
        jn_data = json_pack("{s:s, s:s}", "username", "testuser", "password", "testpass");
    }
    json_t *query = json_pack("{s:s, s:s, s:o, s:o}",
        "method",   "POST",
        "resource", priv->step == 2? "/auth/logout" : "/auth/login",
        "headers",  jn_headers,
        "data",     jn_data
    );
    gobj_send_event(priv->gobj_http_cl, EV_SEND_MESSAGE, query, gobj);
}

/***************************************************************************
 *  An answer that is not the one expected
 ***************************************************************************/
PRIVATE void step_failed(hgobj gobj, const char *what, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char *dump = json2uglystr(kw);
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_APP,
        "msg",          "%s", what,
        "step",         "%d", priv->step + 1,
        "status",       "%d", (int)kw_get_int(gobj, kw, "response_status_code", -1, 0),
        "answer",       "%.800s", dump? dump : "",
        NULL
    );
    GBMEM_FREE(dump)
}

/***************************************************************************
 *  Stop the sides, and die on the next turn of the timer
 ***************************************************************************/
PRIVATE void end_test(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop_tree(priv->gobj_http_cl);
    hgobj bff_side = gobj_find_service("__bff_side__", FALSE);
    if(bff_side) {
        gobj_stop_tree(bff_side);
    }
    hgobj idp_side = gobj_find_service("__idp_side__", FALSE);
    if(idp_side) {
        gobj_stop_tree(idp_side);
    }
    priv->dying = TRUE;
    set_timeout0(priv->timer, 200);
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  Timer fired: open the client, or die
 ***************************************************************************/
PRIVATE int ac_timer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->dying) {
        JSON_DECREF(kw)
        set_yuno_must_die();
        return 0;
    }

    char url[128];
    snprintf(url, sizeof(url), "tcps://%s", gobj_read_str_attr(gobj, "bff_host"));
    priv->gobj_http_cl = gobj_create(gobj_name(gobj), C_PROT_HTTP_CL,
        json_pack("{s:s}", "url", url), gobj);
    gobj_set_bottom_gobj(priv->gobj_http_cl,
        gobj_create_pure_child(gobj_name(gobj), C_TCP,
            json_pack("{s:s, s:{s:b}}",
                "url",      url,
                "crypto",
                    "ssl_allow_insecure_client", 1
            ),
            priv->gobj_http_cl
        )
    );
    gobj_start_tree(priv->gobj_http_cl);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Connected: the first request
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    post_step(gobj);
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The answer of a step
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int status = (int)kw_get_int(gobj, kw, "response_status_code", -1, 0);
    json_t *jn_body = kw_get_dict(gobj, kw, "body", NULL, 0);
    const char *error_code = json_string_value(json_object_get(jn_body, "error_code"));
    char *headers = json2uglystr(json_object_get(kw, "headers"));

    switch(priv->step) {
        case 0:
            if(status != 403 || !error_code || strcmp(error_code, "origin_not_allowed") != 0) {
                step_failed(gobj, TEST_NAME ": an Origin http:// on a TLS connection is served", kw);
            }
            break;
        case 1:
            if(status != 200) {
                step_failed(gobj, TEST_NAME ": the Origin https:// of the Host is not served", kw);
            }
            break;
        default:
            if(status != 500 || !error_code || strcmp(error_code, "auth_config_error") != 0 ||
                    !headers || !strstr(headers, "refresh_token=; Max-Age=0")) {
                step_failed(gobj, TEST_NAME ": a logout with no URL to call keeps the session", kw);
            }
            break;
    }
    GBMEM_FREE(headers)

    priv->step++;
    if(priv->step < NSTEPS) {
        post_step(gobj);
    } else {
        end_test(gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The connection to the BFF closed
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Volatil child stopped: destroy it. The C_TIMER0 publishes one at each
 *  cancel, and is not volatil.
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
    JSON_DECREF(kw)
    return 0;
}




                    /***************************
                     *      FSM
                     ***************************/




PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_play    = mt_play,
    .mt_pause   = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST29_TLS_ORIGIN);

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    ev_action_t st_idle[] = {
        {EV_ON_OPEN,        ac_on_open,     0},
        {EV_ON_MESSAGE,     ac_on_message,  0},
        {EV_ON_CLOSE,       ac_on_close,    0},
        {EV_TIMEOUT,        ac_timer,       0},
        {EV_STOPPED,        ac_stopped,     0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_OPEN,        0},
        {EV_ON_MESSAGE,     0},
        {EV_ON_CLOSE,       0},
        {EV_TIMEOUT,        0},
        {EV_STOPPED,        0},
        {0, 0}
    };

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,              // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,              // authz_table
        0,              // command_table
        s_user_trace_level,
        0               // gcflag_t
    );
    if(!__gclass__) {
        return -1;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_test29_tls_origin(void)
{
    return create_gclass(C_TEST29_TLS_ORIGIN);
}
