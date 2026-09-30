/***********************************************************************
 *          c_fake_smtp.c
 *
 *          A fake SMTP server, for the tests of emailsender.
 *
 *          It sits where a protocol sits in a server channel:
 *              C_IOGATE -> C_TCP_S + C_CHANNEL -> C_FAKE_SMTP -> C_TCP
 *          and speaks plain SMTP (no TLS) over its C_TCP:
 *              220 at connect, 250 to EHLO/HELO, MAIL FROM, RCPT TO and the
 *              end of DATA, 354 to DATA, 221 to QUIT.
 *
 *          AUTH is answered with the lines of `auth_replies`, one per AUTH
 *          received, the last one again when they run out. Each answer and
 *          each message delivered is logged (INFO), so a test says in its
 *          list of expected logs what the server saw, and in which order.
 *
 *          With `die_on_delivery` the yuno ends a second after the first
 *          message delivered: time for the client to read the 250 and say
 *          so.
 *
 *          With `auth_min_gaps` (ms, one per AUTH) an AUTH that comes sooner
 *          than its gap after the previous AUTH is logged as an ERROR
 *          ("Fake smtp: AUTH too early"): a test of the client's pacing.
 *
 *          With `banner_delay` the 220 greeting waits that many ms after the
 *          connection: the client stays in its handshake that long. With
 *          `notify_service` that service is sent EV_FAKE_CLIENT_CONNECTED
 *          `notify_delay` ms after a client connects (and before the
 *          greeting, which then waits banner_delay more), so a test can act
 *          DURING the handshake.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <strings.h>
#include <limits.h>

#include "c_fake_smtp.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int send_reply(hgobj gobj, const char *reply);
PRIVATE int process_line(hgobj gobj, const char *line);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
GOBJ_DEFINE_EVENT(EV_FAKE_CLIENT_CONNECTED);

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_LIST,        "auth_replies",     SDF_RD,             "[\"235 2.7.0 Authentication successful\"]", "Answers to AUTH, one per AUTH, the last one repeated"),
SDATA (DTP_INTEGER,     "banner_delay",     SDF_RD,             "0",        "ms to wait before greeting a client with 220"),
SDATA (DTP_INTEGER,     "notify_delay",     SDF_RD,             "500",      "ms after a connection to tell notify_service"),
SDATA (DTP_STRING,      "notify_service",   SDF_RD,             "",         "service told of each client connected (EV_FAKE_CLIENT_CONNECTED)"),
SDATA (DTP_LIST,        "auth_min_gaps",    SDF_RD,             "[]",       "ms that AUTH n must come after AUTH n-1 (entry 0 unused)"),
SDATA (DTP_BOOLEAN,     "die_on_delivery",  SDF_RD,             "1",        "End the yuno a second after a message is delivered"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
PRIVATE const trace_level_t s_user_trace_level[16] = {
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    hgobj tcp;
    BOOL notify_pending;
    BOOL banner_pending;
    BOOL in_data;
    size_t auth_count;
    uint64_t auth_not_before;   /* msectimer: the next AUTH must not come sooner */
    char line[LINE_MAX];
    size_t line_len;
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

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  One reply line, CRLF added
 ***************************************************************************/
PRIVATE int send_reply(hgobj gobj, const char *reply)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t len = strlen(reply);
    gbuffer_t *gbuf = gbuffer_create(len + 2, len + 2);
    if(!gbuf) {
        // Error already logged
        return -1;
    }
    gbuffer_append(gbuf, (void *)reply, len);
    gbuffer_append(gbuf, "\r\n", 2);

    json_t *kw_tx = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    return gobj_send_event(priv->tcp, EV_TX_DATA, kw_tx, gobj);
}

/***************************************************************************
 *  One command line of the client, CRLF removed
 ***************************************************************************/
PRIVATE int process_line(hgobj gobj, const char *line)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->in_data) {
        if(strcmp(line, ".") == 0) {
            priv->in_data = FALSE;
            gobj_log_info(gobj, 0,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Fake smtp: message delivered",
                NULL
            );
            send_reply(gobj, "250 2.0.0 Ok: queued");
            if(gobj_read_bool_attr(gobj, "die_on_delivery")) {
                set_timeout(priv->timer, 1000);
            }
        }
        return 0;
    }

    if(strncasecmp(line, "EHLO", 4) == 0 || strncasecmp(line, "HELO", 4) == 0) {
        return send_reply(gobj, "250 fake.smtp");
    }
    if(strncasecmp(line, "AUTH ", 5) == 0) {
        if(priv->auth_not_before && !test_msectimer(priv->auth_not_before)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Fake smtp: AUTH too early",
                "auth",         "%d", (int)priv->auth_count,
                NULL
            );
        }
        json_t *jn_gaps = gobj_read_json_attr(gobj, "auth_min_gaps");
        json_int_t next_gap = json_integer_value(json_array_get(jn_gaps, priv->auth_count + 1));
        priv->auth_not_before = next_gap > 0? start_msectimer((uint64_t)next_gap) : 0;

        json_t *jn_replies = gobj_read_json_attr(gobj, "auth_replies");
        size_t n = json_array_size(jn_replies);
        size_t idx = priv->auth_count < n? priv->auth_count : n - 1;
        const char *reply = json_string_value(json_array_get(jn_replies, idx));
        priv->auth_count++;
        if(!reply) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "auth_replies has no string answer",
                NULL
            );
            return -1;
        }
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Fake smtp: AUTH answered",
            "reply",        "%s", reply,
            NULL
        );
        return send_reply(gobj, reply);
    }
    if(strncasecmp(line, "MAIL FROM:", 10) == 0 || strncasecmp(line, "RCPT TO:", 8) == 0) {
        return send_reply(gobj, "250 2.1.0 Ok");
    }
    if(strcasecmp(line, "DATA") == 0) {
        priv->in_data = TRUE;
        return send_reply(gobj, "354 End data with <CR><LF>.<CR><LF>");
    }
    if(strcasecmp(line, "QUIT") == 0) {
        return send_reply(gobj, "221 2.0.0 Bye");
    }
    return send_reply(gobj, "500 5.5.2 Unknown command");
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  A client: greet it
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->tcp = src;
    priv->in_data = FALSE;
    priv->line_len = 0;

    if(!empty_string(gobj_read_str_attr(gobj, "notify_service"))) {
        priv->notify_pending = TRUE;
        set_timeout(priv->timer, gobj_read_integer_attr(gobj, "notify_delay"));
        KW_DECREF(kw)
        return 0;
    }

    json_int_t banner_delay = gobj_read_integer_attr(gobj, "banner_delay");
    if(banner_delay > 0) {
        priv->banner_pending = TRUE;
        set_timeout(priv->timer, banner_delay);
    } else {
        send_reply(gobj, "220 fake.smtp ESMTP");
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Bytes of the client: cut them in lines
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    const char *p = gbuf? gbuffer_cur_rd_pointer(gbuf) : "";
    size_t len = gbuf? gbuffer_leftbytes(gbuf) : 0;

    for(size_t i = 0; i < len; i++) {
        if(p[i] == '\n') {
            if(priv->line_len > 0 && priv->line[priv->line_len - 1] == '\r') {
                priv->line_len--;
            }
            priv->line[priv->line_len] = 0;
            process_line(gobj, priv->line);
            priv->line_len = 0;
            continue;
        }
        if(priv->line_len >= sizeof(priv->line) - 1) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL,
                "msg",          "%s", "Fake smtp: line too long",
                NULL
            );
            priv->line_len = 0;
            continue;
        }
        priv->line[priv->line_len++] = p[i];
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  In order: tell notify_service of the client, then greet it, then (a
 *  second after a message was delivered) end the test
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->notify_pending) {
        priv->notify_pending = FALSE;
        gobj_send_event(
            gobj_find_service(gobj_read_str_attr(gobj, "notify_service"), TRUE),
            EV_FAKE_CLIENT_CONNECTED, 0, gobj
        );
        json_int_t banner_delay = gobj_read_integer_attr(gobj, "banner_delay");
        if(banner_delay > 0) {
            priv->banner_pending = TRUE;
            set_timeout(priv->timer, banner_delay);
        } else {
            send_reply(gobj, "220 fake.smtp ESMTP");
        }
        KW_DECREF(kw)
        return 0;
    }

    if(priv->banner_pending) {
        priv->banner_pending = FALSE;
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Fake smtp: greeting after the delay",
            NULL
        );
        send_reply(gobj, "220 fake.smtp ESMTP");
        KW_DECREF(kw)
        return 0;
    }

    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the transport says that the server does not use
 ***************************************************************************/
PRIVATE int ac_ignore(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
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
    .mt_start = mt_start,
    .mt_stop = mt_stop,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_FAKE_SMTP);

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
        {EV_CONNECTED,              ac_connected,               0},
        {EV_RX_DATA,                ac_rx_data,                 0},
        {EV_DISCONNECTED,           ac_ignore,                  0},
        {EV_TX_READY,               ac_ignore,                  0},
        {EV_STOPPED,                ac_ignore,                  0},
        {EV_TIMEOUT,                ac_timeout,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_CONNECTED,      0},
        {EV_RX_DATA,        0},
        {EV_DISCONNECTED,   0},
        {EV_TX_READY,       0},
        {EV_STOPPED,        0},
        {EV_TIMEOUT,        0},
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
        s_user_trace_level,
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
PUBLIC int register_c_fake_smtp(void)
{
    return create_gclass(C_FAKE_SMTP);
}
