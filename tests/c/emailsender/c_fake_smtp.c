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
 *          received, the last one again when they run out; after a 334 the
 *          next line is taken as the response to it, and answered from the
 *          same list. EHLO is answered from `ehlo_replies`. Each answer and
 *          each message delivered is logged (INFO), so a test says in its
 *          list of expected logs what the server saw, and in which order.
 *
 *          With `die_on_delivery` the yuno ends `die_delay` ms (a second)
 *          after the first message delivered: time for the client to read
 *          the 250 and say so. With `idle_close_after` the server ends the
 *          session that delivered, that many ms later, with a 421 (as OVH
 *          ends idle sessions); `die_delay` must then be longer.
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
 *          RCPT TO and the end of DATA are answered with the lines of
 *          `rcpt_replies` and `data_replies`, like AUTH. Only a 250 to the
 *          end of DATA is a delivery; any other answer is logged as
 *          "Fake smtp: message refused". After a 421, whatever it answers,
 *          the server closes the connection, as RFC 5321 says it does.
 *
 *          `connection_plan` says what the server does with each connection
 *          (one entry per connection, "greet" once they run out): "greet",
 *          "drop" (close it at once, nothing said), "refuse" (a 554 greeting),
 *          "garbage" (two malformed
 *          lines in one write) or "long_line" (a line longer than a client's
 *          reply buffer). Each one but "greet" is logged (INFO).
 *
 *          MAIL FROM is answered from `mail_replies` (a refusal is logged:
 *          "Fake smtp: MAIL FROM refused").
 *          RSET is answered from `rset_replies` the same way, and logged
 *          ("Fake smtp: RSET answered"). With `max_connections` a connection
 *          beyond that many is an ERROR ("Fake smtp: one connection too
 *          many"): a test that the client goes on on the same session.
 *
 *          `connect_min_gaps` / `connect_max_gaps` (ms, one per connection)
 *          and `data_min_gaps` (ms, one per end of DATA) check the pacing of
 *          the client like `auth_min_gaps`: "Fake smtp: connection too
 *          early", "... too late", "Fake smtp: DATA too early" (ERRORs).
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
PRIVATE const char *nth_reply(hgobj gobj, const char *name, size_t count);
PRIVATE json_int_t nth_gap(hgobj gobj, const char *name, size_t count);
PRIVATE void drop_client(hgobj gobj);
PRIVATE int send_reply(hgobj gobj, const char *reply);
PRIVATE int greet_client(hgobj gobj);
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
SDATA (DTP_LIST,        "ehlo_replies",     SDF_RD,             "[\"250 fake.smtp\"]", "Answers to EHLO/HELO, one per EHLO, the last one repeated"),
SDATA (DTP_LIST,        "rcpt_replies",     SDF_RD,             "[\"250 2.1.5 Ok\"]", "Answers to RCPT TO, one per RCPT, the last one repeated"),
SDATA (DTP_LIST,        "data_replies",     SDF_RD,             "[\"250 2.0.0 Ok: queued\"]", "Answers to the end of DATA, one per message, the last one repeated"),
SDATA (DTP_LIST,        "mail_replies",     SDF_RD,             "[\"250 2.1.0 Ok\"]", "Answers to MAIL FROM, one per MAIL FROM, the last one repeated"),
SDATA (DTP_LIST,        "rset_replies",     SDF_RD,             "[\"250 2.0.0 Ok\"]", "Answers to RSET, one per RSET, the last one repeated"),
SDATA (DTP_INTEGER,     "max_connections",  SDF_RD,             "0",        "Connections allowed; one more is an ERROR. 0: no check"),
SDATA (DTP_LIST,        "data_min_gaps",    SDF_RD,             "[]",       "ms that the end of DATA n must come after the end of DATA n-1 (entry 0 unused)"),
SDATA (DTP_LIST,        "connection_plan",  SDF_RD,             "[]",       "What to do with each connection: greet, drop, refuse, garbage, long_line. greet when they run out"),
SDATA (DTP_LIST,        "connect_min_gaps", SDF_RD,             "[]",       "ms that connection n must come after connection n-1 (entry 0 unused)"),
SDATA (DTP_LIST,        "connect_max_gaps", SDF_RD,             "[]",       "ms that connection n must come within after connection n-1 (entry 0 unused, 0 = no check)"),
SDATA (DTP_BOOLEAN,     "die_on_delivery",  SDF_RD,             "1",        "End the yuno die_delay ms after a message is delivered"),
SDATA (DTP_INTEGER,     "die_delay",        SDF_RD,             "1000",     "ms from the delivery to the end of the yuno"),
SDATA (DTP_INTEGER,     "idle_close_after", SDF_RD,             "0",        "ms after a delivery to end the idle session with a 421. 0: never"),
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
    BOOL die_pending;
    BOOL idle_close_pending;
    uint64_t die_at;            /* msectimer of the end of the yuno, with an idle close before it */
    BOOL in_data;
    size_t auth_count;
    size_t rcpt_count;
    size_t ehlo_count;
    size_t rset_count;
    size_t mail_count;
    BOOL auth_continuation;     /* the last AUTH was answered 334: the next line is its response */
    size_t data_count;
    size_t conn_count;
    uint64_t auth_not_before;   /* msectimer: the next AUTH must not come sooner */
    uint64_t data_not_before;   /* msectimer: the next end of DATA must not come sooner */
    uint64_t conn_not_before;   /* msectimer: the next connection must not come sooner */
    uint64_t conn_not_after;    /* msectimer: the next connection must not come later */
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
 *  The answer number `count` of the list attr `name`, the last one when
 *  they run out
 ***************************************************************************/
PRIVATE const char *nth_reply(hgobj gobj, const char *name, size_t count)
{
    json_t *jn_replies = gobj_read_json_attr(gobj, name);
    size_t n = json_array_size(jn_replies);
    if(n == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "list of replies is empty",
            "attr",         "%s", name,
            NULL
        );
        return NULL;
    }
    size_t idx = count < n? count : n - 1;
    const char *reply = json_string_value(json_array_get(jn_replies, idx));
    if(!reply) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "list of replies has no string answer",
            "attr",         "%s", name,
            NULL
        );
    }
    return reply;
}

/***************************************************************************
 *  Entry `count` of the list attr `name` of gaps, 0 when there is none
 ***************************************************************************/
PRIVATE json_int_t nth_gap(hgobj gobj, const char *name, size_t count)
{
    json_t *jn_gaps = gobj_read_json_attr(gobj, name);
    return json_integer_value(json_array_get(jn_gaps, count));
}

/***************************************************************************
 *  Close the connection of the client
 ***************************************************************************/
PRIVATE void drop_client(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->notify_pending = FALSE;
    priv->banner_pending = FALSE;
    gobj_send_event(priv->tcp, EV_DROP, 0, gobj);
}

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
    int ret = gobj_send_event(priv->tcp, EV_TX_DATA, kw_tx, gobj);

    if(strncmp(reply, "421", 3) == 0) {
        drop_client(gobj);
    }
    return ret;
}

/***************************************************************************
 *  Greet a client, or do with it what the connection plan says
 ***************************************************************************/
PRIVATE int greet_client(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_plan = gobj_read_json_attr(gobj, "connection_plan");
    const char *plan = json_string_value(json_array_get(jn_plan, priv->conn_count - 1));
    if(!plan || strcmp(plan, "greet") == 0) {
        return send_reply(gobj, "220 fake.smtp ESMTP");
    }

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "Fake smtp: connection not greeted",
        "plan",         "%s", plan,
        NULL
    );

    if(strcmp(plan, "drop") == 0) {
        drop_client(gobj);
        return 0;
    }
    if(strcmp(plan, "garbage") == 0) {
        return send_reply(gobj, "garbage one\r\ngarbage two");
    }
    if(strcmp(plan, "refuse") == 0) {
        return send_reply(gobj, "554 5.7.1 Service unavailable; client host blocked");
    }
    if(strcmp(plan, "long_line") == 0) {
        char line[10000];
        memset(line, 'x', sizeof(line) - 1);
        memcpy(line, "220-", 4);
        line[sizeof(line) - 1] = 0;
        return send_reply(gobj, line);
    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER,
        "msg",          "%s", "connection_plan: unknown entry",
        "plan",         "%s", plan,
        NULL
    );
    return -1;
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

            if(priv->data_not_before && !test_msectimer(priv->data_not_before)) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "Fake smtp: DATA too early",
                    "data",         "%d", (int)priv->data_count,
                    NULL
                );
            }
            json_int_t next_gap = nth_gap(gobj, "data_min_gaps", priv->data_count + 1);
            priv->data_not_before = next_gap > 0? start_msectimer((uint64_t)next_gap) : 0;

            const char *reply = nth_reply(gobj, "data_replies", priv->data_count);
            priv->data_count++;
            if(!reply) {
                // Error already logged
                return -1;
            }
            if(strncmp(reply, "250", 3) != 0) {
                gobj_log_info(gobj, 0,
                    "msgset",       "%s", MSGSET_INFO,
                    "msg",          "%s", "Fake smtp: message refused",
                    "reply",        "%s", reply,
                    NULL
                );
                return send_reply(gobj, reply);
            }

            gobj_log_info(gobj, 0,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Fake smtp: message delivered",
                NULL
            );
            send_reply(gobj, reply);
            json_int_t idle_close = gobj_read_integer_attr(gobj, "idle_close_after");
            if(gobj_read_bool_attr(gobj, "die_on_delivery")) {
                priv->die_pending = TRUE;
                priv->die_at = start_msectimer((uint64_t)gobj_read_integer_attr(gobj, "die_delay"));
            }
            if(idle_close > 0) {
                priv->idle_close_pending = TRUE;
                set_timeout(priv->timer, idle_close);
            } else if(priv->die_pending) {
                set_timeout(priv->timer, gobj_read_integer_attr(gobj, "die_delay"));
            }
        }
        return 0;
    }

    if(strncasecmp(line, "EHLO", 4) == 0 || strncasecmp(line, "HELO", 4) == 0) {
        const char *reply = nth_reply(gobj, "ehlo_replies", priv->ehlo_count);
        priv->ehlo_count++;
        if(!reply) {
            // Error already logged
            return -1;
        }
        return send_reply(gobj, reply);
    }
    BOOL continuation = priv->auth_continuation;
    priv->auth_continuation = FALSE;
    if(continuation || strncasecmp(line, "AUTH ", 5) == 0) {
        if(priv->auth_not_before && !test_msectimer(priv->auth_not_before)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Fake smtp: AUTH too early",
                "auth",         "%d", (int)priv->auth_count,
                NULL
            );
        }
        json_int_t next_gap = nth_gap(gobj, "auth_min_gaps", priv->auth_count + 1);
        priv->auth_not_before = next_gap > 0? start_msectimer((uint64_t)next_gap) : 0;

        const char *reply = nth_reply(gobj, "auth_replies", priv->auth_count);
        priv->auth_count++;
        if(!reply) {
            // Error already logged
            return -1;
        }
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Fake smtp: AUTH answered",
            "reply",        "%s", reply,
            "continuation", "%d", continuation? 1 : 0,
            NULL
        );
        if(strncmp(reply, "334", 3) == 0) {
            priv->auth_continuation = TRUE;
        }
        return send_reply(gobj, reply);
    }
    if(strncasecmp(line, "MAIL FROM:", 10) == 0) {
        const char *reply = nth_reply(gobj, "mail_replies", priv->mail_count);
        priv->mail_count++;
        if(!reply) {
            // Error already logged
            return -1;
        }
        if(strncmp(reply, "250", 3) != 0) {
            gobj_log_info(gobj, 0,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Fake smtp: MAIL FROM refused",
                "reply",        "%s", reply,
                NULL
            );
        }
        return send_reply(gobj, reply);
    }
    if(strncasecmp(line, "RCPT TO:", 8) == 0) {
        const char *reply = nth_reply(gobj, "rcpt_replies", priv->rcpt_count);
        priv->rcpt_count++;
        if(!reply) {
            // Error already logged
            return -1;
        }
        if(strncmp(reply, "250", 3) != 0) {
            gobj_log_info(gobj, 0,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Fake smtp: RCPT refused",
                "reply",        "%s", reply,
                NULL
            );
        }
        return send_reply(gobj, reply);
    }
    if(strcasecmp(line, "DATA") == 0) {
        priv->in_data = TRUE;
        return send_reply(gobj, "354 End data with <CR><LF>.<CR><LF>");
    }
    if(strcasecmp(line, "RSET") == 0) {
        const char *reply = nth_reply(gobj, "rset_replies", priv->rset_count);
        priv->rset_count++;
        if(!reply) {
            // Error already logged
            return -1;
        }
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Fake smtp: RSET answered",
            "reply",        "%s", reply,
            NULL
        );
        return send_reply(gobj, reply);
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

    if(priv->conn_not_before && !test_msectimer(priv->conn_not_before)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Fake smtp: connection too early",
            "connection",   "%d", (int)priv->conn_count,
            NULL
        );
    }
    if(priv->conn_not_after && test_msectimer(priv->conn_not_after)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Fake smtp: connection too late",
            "connection",   "%d", (int)priv->conn_count,
            NULL
        );
    }
    json_int_t max_connections = gobj_read_integer_attr(gobj, "max_connections");
    if(max_connections > 0 && (json_int_t)priv->conn_count >= max_connections) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Fake smtp: one connection too many",
            "connection",   "%d", (int)priv->conn_count,
            NULL
        );
    }
    json_int_t min_gap = nth_gap(gobj, "connect_min_gaps", priv->conn_count + 1);
    json_int_t max_gap = nth_gap(gobj, "connect_max_gaps", priv->conn_count + 1);
    priv->conn_not_before = min_gap > 0? start_msectimer((uint64_t)min_gap) : 0;
    priv->conn_not_after = max_gap > 0? start_msectimer((uint64_t)max_gap) : 0;
    priv->conn_count++;

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
        greet_client(gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The client is gone: what was waiting for it is not done
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->notify_pending = FALSE;
    priv->banner_pending = FALSE;
    priv->idle_close_pending = FALSE;
    if(!gobj_is_running(gobj) || gobj_is_shutdowning()) {
        // the end of the yuno: its own stop clears the timer
    } else if(priv->die_pending) {
        json_int_t left = (json_int_t)(priv->die_at - time_in_milliseconds_monotonic());
        set_timeout(priv->timer, left > 0? left : 1);
    } else {
        clear_timeout(priv->timer);
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
            greet_client(gobj);
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
        greet_client(gobj);
        KW_DECREF(kw)
        return 0;
    }

    if(priv->idle_close_pending) {
        priv->idle_close_pending = FALSE;
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Fake smtp: idle session ended",
            NULL
        );
        send_reply(gobj, "421 4.4.2 Idle timeout, closing");
        if(priv->die_pending) {
            json_int_t left = (json_int_t)(priv->die_at - time_in_milliseconds_monotonic());
            set_timeout(priv->timer, left > 0? left : 1);
        }
        KW_DECREF(kw)
        return 0;
    }

    if(!priv->die_pending) {
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
        {EV_DISCONNECTED,           ac_disconnected,            0},
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
