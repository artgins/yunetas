/***********************************************************************
 *          c_smtp_session.c
 *          Smtp_session GClass.
 *
 *          SMTP client protocol on top of C_TCP.
 *
 *          Wire model:
 *                  > C_SMTP_SESSION  <- this gclass
 *                      > C_TCP        (smtps://host:465)
 *
 *          The transport is implicit TLS (SMTPS, RFC 8314); STARTTLS
 *          is not implemented. The bottom C_TCP carries the TLS, this
 *          gclass only speaks line-based SMTP over what it sees as a
 *          plain CRLF stream.
 *
 *          A refused AUTH is told apart by its code: a 5xx is the server
 *          saying the credentials are wrong (auth_rejected on EV_ON_CLOSE,
 *          the owner stops trying them), anything else is a transient
 *          failure of the login (454, 421, ...) and closes the session like
 *          any other drop, to be tried again. Every close caused by a reply
 *          carries the reply's text on EV_ON_CLOSE (`reply`).
 *
 *          A session the SERVER ends (a refusal, an unexpected or malformed
 *          reply, a reply that never comes) is logged as a WARNING of
 *          MSGSET_PROTOCOL with the reply, capped: a remote peer can cause
 *          it. An ERROR is for our own failures only (no memory, an encoder).
 *
 *          The next session after an aborted one is paced. The transport
 *          (C_TCP) reconnects by itself after `timeout_between_connections`,
 *          and resets its own backoff as soon as the TCP connection is up,
 *          which a server refusing the login or the message never stops it
 *          from being. So the session, which knows the SMTP session failed,
 *          sets that delay: `timeout_retry` after the first failure in a row,
 *          doubled at each next one up to `timeout_retry_max`, and back at
 *          `timeout_retry` once a message is delivered. The waiting is the
 *          transport's own timer; nothing here kicks it.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <strings.h>

#include <istream.h>
#include "c_smtp_session.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define DEFAULT_TIMEOUT_RESPONSE_MS    30000   /* per-command server response watchdog */
#define LINE_BUFFER_INITIAL            512
#define LINE_BUFFER_MAX                8192    /* RFC 5321 §4.5.3.1.6 reply line limit */
#define REPLY_TEXT_MAX                 512     /* RFC 5321 §4.5.3.1.5, the text kept of a reply */

#define SMTP_CODE_SERVICE_READY        220
#define SMTP_CODE_GOODBYE              221
#define SMTP_CODE_AUTH_OK              235
#define SMTP_CODE_OK                   250
#define SMTP_CODE_AUTH_CHALLENGE       334
#define SMTP_CODE_START_INPUT          354

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int send_smtp_line(hgobj gobj, const char *line);
PRIVATE int parse_response_code(const char *bf, size_t len, int *code, BOOL *is_final);
PRIVATE int begin_send_current_message(hgobj gobj);
PRIVATE int enter_idle_after_handshake(hgobj gobj);
PRIVATE int reject_current_message(hgobj gobj, int code, const char *reply, const char *reason);
PRIVATE int send_next_rcpt_or_data(hgobj gobj);
PRIVATE json_t *gather_recipients(hgobj gobj, json_t *jn_msg);
PRIVATE int dot_stuff_into(gbuffer_t *out, const char *body, size_t len);
PRIVATE void cleanup_current_message(hgobj gobj);
PRIVATE int abort_session_by_peer(hgobj gobj, const char *reason, int code, const char *reply);
PRIVATE int abort_session_on_error(hgobj gobj, const char *reason);
PRIVATE int drop_session(hgobj gobj, const char *reply);

/*
 *  Internal states and event for the SMTP FSM. Defined early (instead of in
 *  the FSM block at the bottom) so the action functions can reference them.
 *  The kernel-provided ST_DISCONNECTED / ST_WAIT_CONNECTED / ST_IDLE come
 *  from g_st_kernel.h.
 */
GOBJ_DEFINE_STATE(ST_WAIT_BANNER);
GOBJ_DEFINE_STATE(ST_WAIT_EHLO_RESP);
GOBJ_DEFINE_STATE(ST_WAIT_AUTH_RESP);
GOBJ_DEFINE_STATE(ST_WAIT_MAIL_FROM_RESP);
GOBJ_DEFINE_STATE(ST_WAIT_RCPT_TO_RESP);
GOBJ_DEFINE_STATE(ST_WAIT_DATA_GO);
GOBJ_DEFINE_STATE(ST_WAIT_DATA_RESP);
GOBJ_DEFINE_EVENT(EV_RX_LINE);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type--------name----------------flag--------default-----description---------- */
SDATA (DTP_INTEGER, "timeout_inactivity", SDF_RD, "-1", "Inactivity timeout in milliseconds to close the connection. Reconnect when new data arrived. With -1 never close."),
SDATA (DTP_STRING,  "url",              SDF_RD,     "",         "SMTP server URL (smtps://host:465). If set, mt_start auto-creates a C_TCP bottom."),
SDATA (DTP_DICT,    "crypto",           SDF_RD,     "{\"ssl_use_system_ca\": true, \"ssl_verify_mode\": \"required\"}", "TLS crypto for the smtps:// bottom C_TCP. Verifies the server cert against the system CA by default; override ssl_trusted_certificate for a private mail CA, or ssl_allow_insecure_client=true to skip (MITM risk)."),
SDATA (DTP_STRING,  "helo_name",        SDF_RD,     "localhost","EHLO domain advertised to the server"),
SDATA (DTP_STRING,  "username",         SDF_RD,     "",         "SMTP AUTH PLAIN username"),
SDATA (DTP_STRING,  "password",         SDF_RD|SDF_SECRET,     "",         "SMTP AUTH PLAIN password"),
SDATA (DTP_INTEGER, "timeout_response", SDF_RD,     "30000",    "Per-command server response timeout (ms)"),
SDATA (DTP_INTEGER, "timeout_retry",    SDF_RD,     "2000",     "ms the transport waits before connecting again after the server ended a session. Doubles at each such failure in a row, up to timeout_retry_max; back to this once a message is delivered"),
SDATA (DTP_INTEGER, "timeout_retry_max",SDF_RD,     "600000",   "Cap of the doubling of timeout_retry (ms)"),
SDATA (DTP_POINTER, "subscriber",       0,          0,          "Subscriber of output-events. Default if null is parent."),
SDATA (DTP_POINTER, "user_data",        0,          0,          "user data"),
SDATA (DTP_POINTER, "user_data2",       0,          0,          "more user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_SMTP    = 0x0001,
    TRACE_TRAFFIC = 0x0002,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"smtp",     "Trace SMTP FSM phases (commands sent, AUTH without its credentials, + reply codes)"},
{"traffic",  "Trace raw bytes in/out (hex dump)"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    istream_h istream_in;
    int32_t timeout_response;
    BOOL inform_on_close;
    json_t *jn_current_msg;     /* envelope of the message being sent; NULL when idle */
    json_t *jn_recipients;      /* flat json_array of unique RCPT TO addresses */
    int recipient_index;        /* next RCPT TO index to send */
    int reject_code;            /* SMTP reply code of a per-message rejection, forwarded on EV_ON_CLOSE; 0 = transient/link error */
    int auth_reject_code;       /* SMTP reply code of a refused AUTH (5xx), forwarded on EV_ON_CLOSE as auth_rejected; 0 = none */
    char close_reply[REPLY_TEXT_MAX]; /* text of the reply that closed the session, forwarded on EV_ON_CLOSE as reply */
    json_int_t retry_delay;     /* ms the transport waits after the next aborted session; 0 = timeout_retry */
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

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout_response,      gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout_response,    gobj_read_integer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->istream_in) {
        istream_destroy(priv->istream_in);
        priv->istream_in = NULL;
    }
    cleanup_current_message(gobj);
}

/***************************************************************************
 *      Framework Method start
 *
 *      Mirrors c_prot_tcp4h's pattern: if a url attribute is set and no
 *      bottom gobj exists yet, auto-create a C_TCP client child with that
 *      url and start it. The C_TCP child decides TLS vs plain from the
 *      URL schema (smtps:// → implicit TLS from byte zero).
 *
 *      A bottom built before the url was changed (the owner writes it:
 *      set-email-user, set-url-from) is given the new url before it starts:
 *      C_TCP takes its url at each start and at each connect. It is given
 *      a fresh copy of the crypto too, because C_TCP writes into the one it
 *      holds the ssl_server_name of the host it connected to; that is also
 *      why the bottom gets a COPY and not our own attr.
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    const char *url = gobj_read_str_attr(gobj, "url");
    hgobj bottom = gobj_bottom_gobj(gobj);

    if(!empty_string(url) && !bottom) {
        json_t *kw_tcp = json_pack("{s:s, s:I, s:o}",
            "url", url,
            "timeout_inactivity", gobj_read_integer_attr(gobj, "timeout_inactivity"),
            "crypto", json_deep_copy(gobj_read_json_attr(gobj, "crypto"))
        );
        if(!kw_tcp) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY,
                "msg",          "%s", "json_pack() FAILED for the bottom C_TCP",
                NULL
            );
            return -1;
        }
        bottom = gobj_create_pure_child(gobj_name(gobj), C_TCP, kw_tcp, gobj);
        if(!bottom) {
            /* Error already logged by gobj_create_pure_child */
            return -1;
        }
        gobj_set_bottom_gobj(gobj, bottom);

    } else if(!empty_string(url) && bottom &&
            strcmp(url, gobj_read_str_attr(bottom, "url")) != 0) {
        gobj_write_str_attr(bottom, "url", url);
        gobj_write_new_json_attr(bottom, "crypto",
            json_deep_copy(gobj_read_json_attr(gobj, "crypto"))
        );
    }

    if(bottom) {
        gobj_start(bottom);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 *
 *      Polite shutdown: if we are past the banner, fire a best-effort QUIT
 *      down to the bottom before stopping it. We do not wait for the 221
 *      reply — server tolerates the early close, and waiting would require
 *      an extra state plus a timeout. Skipping QUIT entirely is also valid
 *      SMTP, but a graceful close keeps the server-side logs clean.
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    gobj_state_t st = gobj_current_state(gobj);
    if(st != ST_DISCONNECTED && st != ST_WAIT_CONNECTED && st != ST_WAIT_BANNER) {
        send_smtp_line(gobj, "QUIT");
    }

    gobj_stop(gobj_bottom_gobj(gobj));

    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Write one SMTP command line (CRLF appended) to the bottom transport.
 ***************************************************************************/
PRIVATE int send_smtp_line(hgobj gobj, const char *line)
{
    size_t line_len = strlen(line);
    gbuffer_t *gbuf = gbuffer_create(line_len + 2, line_len + 2);
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "gbuffer_create() FAILED",
            "line_len",     "%d", (int)line_len,
            NULL
        );
        return -1;
    }
    /*
     *  Before the append: a secret gbuffer is wiped when freed or grown, and
     *  the traffic dumps of the bottom C_TCP show it as "<N bytes hidden>".
     */
    gbuffer_set_secret(gbuf, strncasecmp(line, "AUTH ", 5)==0? TRUE : FALSE);
    gbuffer_append(gbuf, (void *)line, line_len);
    gbuffer_append(gbuf, "\r\n", 2);

    if(gobj_trace_level(gobj) & TRACE_SMTP) {
        /*
         *  AUTH carries the credentials (PLAIN: base64 of user and password,
         *  i.e. in clear): the trace names the mechanism and nothing else.
         *  Up to 7.25.17 it wrote the whole line to the log, and from there
         *  to the logcenter.
         */
        if(strncasecmp(line, "AUTH ", 5)==0) {
            const char *mech = line + 5;
            const char *end = strchr(mech, ' ');
            int mech_len = end? (int)(end - mech) : (int)strlen(mech);
            gobj_trace_msg(gobj, ">>> AUTH %.*s <credentials not traced>", mech_len, mech);
        } else {
            gobj_trace_msg(gobj, ">>> %s", line);
        }
    }

    json_t *kw_tx = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    return gobj_send_event(gobj_bottom_gobj(gobj), EV_TX_DATA, kw_tx, gobj);
}

/***************************************************************************
 *  Parse a single SMTP response line.
 *      "250-foo\r\n"   -> code=250, is_final=FALSE  (continuation)
 *      "250 foo\r\n"   -> code=250, is_final=TRUE   (last line of the reply)
 *      "250\r\n"       -> code=250, is_final=TRUE
 *  Returns 0 on success, -1 on malformed input.
 ***************************************************************************/
PRIVATE int parse_response_code(const char *bf, size_t len, int *code, BOOL *is_final)
{
    *code = 0;
    *is_final = TRUE;

    if(len < 3) {
        return -1;
    }
    if(bf[0] < '0' || bf[0] > '9' ||
       bf[1] < '0' || bf[1] > '9' ||
       bf[2] < '0' || bf[2] > '9') {
        return -1;
    }
    *code = (bf[0] - '0') * 100 + (bf[1] - '0') * 10 + (bf[2] - '0');

    if(len >= 4 && bf[3] == '-') {
        *is_final = FALSE;
    } else {
        *is_final = TRUE;
    }
    return 0;
}

/***************************************************************************
 *  Drive the current outgoing message through MAIL FROM / RCPT TO / DATA.
 *  Called once we are in ST_IDLE (post-AUTH) and an EV_SEND_MESSAGE arrived.
 *  Returns 0 on success, -1 on missing fields (caller already logged).
 ***************************************************************************/
PRIVATE int begin_send_current_message(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->jn_current_msg) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "begin_send_current_message without current msg",
            NULL
        );
        return -1;
    }
    const char *from = kw_get_str(gobj, priv->jn_current_msg, "from", "", 0);
    if(empty_string(from)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "EV_SEND_MESSAGE without 'from'",
            NULL
        );
        return -1;
    }

    char line[LINE_BUFFER_MAX];
    snprintf(line, sizeof(line), "MAIL FROM:<%s>", from);
    priv->recipient_index = 0;
    gobj_change_state(gobj, ST_WAIT_MAIL_FROM_RESP);
    set_timeout(priv->timer, priv->timeout_response);
    return send_smtp_line(gobj, line);
}

/***************************************************************************
 *  Walk through priv->jn_recipients sending one RCPT TO per call. When the
 *  list is exhausted, transition to ST_WAIT_DATA_GO and send DATA.
 ***************************************************************************/
PRIVATE int send_next_rcpt_or_data(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->jn_recipients &&
       priv->recipient_index < (int)json_array_size(priv->jn_recipients)) {
        const char *rcpt = json_string_value(
            json_array_get(priv->jn_recipients, priv->recipient_index)
        );
        priv->recipient_index++;

        char line[LINE_BUFFER_MAX];
        snprintf(line, sizeof(line), "RCPT TO:<%s>", rcpt);
        gobj_change_state(gobj, ST_WAIT_RCPT_TO_RESP);
        set_timeout(priv->timer, priv->timeout_response);
        return send_smtp_line(gobj, line);
    }

    gobj_change_state(gobj, ST_WAIT_DATA_GO);
    set_timeout(priv->timer, priv->timeout_response);
    return send_smtp_line(gobj, "DATA");
}

/***************************************************************************
 *  Append one parsed recipient token to (jn_set, jn_list), normalising:
 *      - trims leading/trailing whitespace
 *      - if "Name <addr>" form, keeps only what's inside angle brackets
 *      - skips empties
 *      - deduplicates via jn_set
 *  jn_set is a json_object used purely as a hash set (values = json_true()).
 ***************************************************************************/
PRIVATE void add_recipient_token(json_t *jn_set, json_t *jn_list, char *token)
{
    while(*token && (*token == ' ' || *token == '\t')) {
        token++;
    }
    size_t len = strlen(token);
    while(len > 0 && (token[len - 1] == ' ' || token[len - 1] == '\t' ||
                       token[len - 1] == '\r' || token[len - 1] == '\n')) {
        token[--len] = '\0';
    }
    if(len == 0) {
        return;
    }
    char *lt = strrchr(token, '<');
    if(lt) {
        char *gt = strrchr(lt, '>');
        if(gt && gt > lt + 1) {
            *gt = '\0';
            token = lt + 1;
            while(*token && (*token == ' ' || *token == '\t')) {
                token++;
            }
        }
    }
    if(empty_string(token)) {
        return;
    }
    if(json_object_get(jn_set, token)) {
        return;
    }
    json_object_set_new(jn_set, token, json_true());
    json_array_append_new(jn_list, json_string(token));
}

/***************************************************************************
 *  Build a deduplicated json_array of RCPT TO addresses from the message
 *  envelope. Reads "to", "cc", "bcc" as comma- or semicolon-separated strings
 *  (semicolon is the Outlook-style separator and also covers a stray trailing
 *  ';'). Returns a new owned array, or NULL if no valid recipient was found.
 ***************************************************************************/
PRIVATE json_t *gather_recipients(hgobj gobj, json_t *jn_msg)
{
    json_t *jn_set = json_object();
    json_t *jn_list = json_array();
    if(!jn_set || !jn_list) {
        JSON_DECREF(jn_set)
        JSON_DECREF(jn_list)
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "json_object/array() FAILED",
            NULL
        );
        return NULL;
    }

    const char *fields[] = {"to", "cc", "bcc", NULL};
    for(int f = 0; fields[f]; f++) {
        const char *src = kw_get_str(gobj, jn_msg, fields[f], "", 0);
        if(empty_string(src)) {
            continue;
        }
        char *buf = gbmem_strdup(src);
        if(!buf) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY,
                "msg",          "%s", "gbmem_strdup() FAILED",
                NULL
            );
            JSON_DECREF(jn_set)
            JSON_DECREF(jn_list)
            return NULL;
        }
        char *save = NULL;
        for(char *tok = strtok_r(buf, ",;", &save);
            tok;
            tok = strtok_r(NULL, ",;", &save)
        ) {
            add_recipient_token(jn_set, jn_list, tok);
        }
        gbmem_free(buf);
    }

    JSON_DECREF(jn_set)
    if(json_array_size(jn_list) == 0) {
        JSON_DECREF(jn_list)
        return NULL;
    }
    return jn_list;
}

/***************************************************************************
 *  Append body to out, doubling any leading '.' at the start of a line
 *  per RFC 5321 §4.5.2. Treats '\n' as line terminator (works for both
 *  CRLF and LF inputs). Returns 0 on success, -1 on gbuffer overflow.
 ***************************************************************************/
PRIVATE int dot_stuff_into(gbuffer_t *out, const char *body, size_t len)
{
    BOOL at_line_start = TRUE;
    for(size_t i = 0; i < len; i++) {
        if(at_line_start && body[i] == '.') {
            if(gbuffer_append(out, ".", 1) != 1) {
                gobj_log_error(0, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MEMORY,
                    "msg",          "%s", "gbuffer_append() FAILED (dot stuff)",
                    NULL
                );
                return -1;
            }
        }
        if(gbuffer_append(out, (void *)&body[i], 1) != 1) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY,
                "msg",          "%s", "gbuffer_append() FAILED (body)",
                NULL
            );
            return -1;
        }
        if(body[i] == '\n') {
            at_line_start = TRUE;
        } else {
            at_line_start = FALSE;
        }
    }
    return 0;
}

/***************************************************************************
 *  Drop the in-flight message bookkeeping. Safe to call when nothing is in
 *  flight (JSON_DECREF tolerates NULL).
 ***************************************************************************/
PRIVATE void cleanup_current_message(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(priv->jn_current_msg)
    JSON_DECREF(priv->jn_recipients)
    priv->recipient_index = 0;
}

/***************************************************************************
 *  Tear the SMTP session down because of the server: a refusal, an
 *  unexpected or malformed reply, a reply that never came. A remote peer
 *  can cause it, so it is a WARNING (house decoder-severity rule), with the
 *  reply (code 0 and reply NULL when there is none) -- capped, it is the
 *  dump of what the peer sent.
 ***************************************************************************/
PRIVATE int abort_session_by_peer(hgobj gobj, const char *reason, int code, const char *reply)
{
    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PROTOCOL,
        "msg",          "%s", reason,
        "code",         "%d", code,
        "reply",        "%s", reply? reply : "",
        NULL
    );
    return drop_session(gobj, reply);
}

/***************************************************************************
 *  Tear the SMTP session down because of our own failure (no memory, an
 *  encoder): an ERROR.
 ***************************************************************************/
PRIVATE int abort_session_on_error(hgobj gobj, const char *reason)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", reason,
        NULL
    );
    return drop_session(gobj, NULL);
}

/***************************************************************************
 *  Drop the underlying TCP, keeping the text of the reply that caused it:
 *  ac_disconnected publishes EV_ON_CLOSE upward with it.
 *
 *  The transport reconnects by itself after its timeout_between_connections,
 *  which it reads when the drop completes: it is given the paced delay
 *  first, and the next failure in a row waits twice as long.
 ***************************************************************************/
PRIVATE int drop_session(hgobj gobj, const char *reply)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    snprintf(priv->close_reply, sizeof(priv->close_reply), "%s", reply? reply : "");

    json_int_t base = gobj_read_integer_attr(gobj, "timeout_retry");
    json_int_t cap = gobj_read_integer_attr(gobj, "timeout_retry_max");
    if(priv->retry_delay < base) {
        priv->retry_delay = base;
    }
    hgobj bottom = gobj_bottom_gobj(gobj);
    gobj_write_integer_attr(bottom, "timeout_between_connections", priv->retry_delay);
    priv->retry_delay = (priv->retry_delay * 2 < cap)? priv->retry_delay * 2 : cap;

    gobj_send_event(bottom, EV_DROP, 0, gobj);
    return -1;
}

/***************************************************************************
 *  Reach ST_IDLE after a successful banner+EHLO[+AUTH] handshake and tell
 *  the owner the session is usable (EV_ON_OPEN).
 *
 *  An owner that reacts to EV_ON_OPEN by sending EV_SEND_MESSAGE will — since
 *  we are already in ST_IDLE — have its message begun by ac_send_message
 *  itself. We must therefore NOT begin it again here, or MAIL FROM goes out
 *  twice and the server answers the duplicate with "503 MAIL already given".
 *  Snapshot the pending flag BEFORE publishing: only a message stashed DURING
 *  the handshake (while we were not yet idle) still needs kicking off here.
 ***************************************************************************/
PRIVATE int enter_idle_after_handshake(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_change_state(gobj, ST_IDLE);
    BOOL had_pending = (priv->jn_current_msg != NULL);
    gobj_publish_event(gobj, EV_ON_OPEN, 0);
    if(had_pending) {
        return begin_send_current_message(gobj);
    }
    return 0;
}

/***************************************************************************
 *  A per-message SMTP rejection mid-transaction (a non-2xx/3xx reply to
 *  MAIL FROM / RCPT TO / DATA). Remember the reply code so ac_disconnected
 *  can forward it to the owner in EV_ON_CLOSE — a 5xx is permanent (the owner
 *  dead-letters the message instead of retrying) — then drop the session.
 *
 *  We resolve the in-flight message through EV_ON_CLOSE rather than
 *  EV_ON_MESSAGE here on purpose: the owner sees smtp_ready=FALSE before it
 *  tries to dispatch the next queued message, so it cannot push it into the
 *  dying connection.
 ***************************************************************************/
PRIVATE int reject_current_message(hgobj gobj, int code, const char *reply, const char *reason)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->reject_code = code;
    return abort_session_by_peer(gobj, reason, code, reply);
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  TCP layer reports the socket is up. Arm the line reader and wait for
 *  the server's 220 banner.
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->istream_in) {
        priv->istream_in = istream_create(gobj, LINE_BUFFER_INITIAL, LINE_BUFFER_MAX);
        if(!priv->istream_in) {
            /* Error already logged by istream_create */
            KW_DECREF(kw)
            return -1;
        }
    }
    if(istream_read_until_delimiter(priv->istream_in, "\r\n", 2, EV_RX_LINE) < 0) {
        /* Error already logged by istream_read_until_delimiter */
        KW_DECREF(kw)
        return -1;
    }

    priv->inform_on_close = TRUE;
    gobj_change_state(gobj, ST_WAIT_BANNER);
    set_timeout(priv->timer, priv->timeout_response);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  TCP layer reports disconnect. Publish EV_ON_CLOSE upward if the socket had
 *  reached the SMTP layer (inform_on_close, armed in ac_connected) — this also
 *  covers handshake failures (banner/EHLO/AUTH) that never emitted EV_ON_OPEN,
 *  so the owner is told the link is down even then. Reset our line buffer.
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    if(priv->istream_in) {
        istream_destroy(priv->istream_in);
        priv->istream_in = NULL;
    }
    cleanup_current_message(gobj);

    if(priv->inform_on_close) {
        priv->inform_on_close = FALSE;
        /*
         *  Forward the per-message rejection code (if any) so the owner can
         *  tell a permanent 5xx (dead-letter) from a transient link drop
         *  (retry). 0/absent = transient.
         *
         *  A refused AUTH travels apart, as auth_rejected: it says nothing
         *  about the message (the server never saw it), it says the
         *  credentials are wrong, and retrying them is what gets the node's
         *  address banned by the mail provider.
         *
         *  The text of the reply that closed the session goes with them,
         *  so the owner can say why.
         */
        json_t *kw_close = json_object();
        if(priv->reject_code) {
            json_object_set_new(kw_close, "code", json_integer(priv->reject_code));
        }
        if(priv->auth_reject_code) {
            json_object_set_new(kw_close, "auth_rejected", json_integer(priv->auth_reject_code));
        }
        if(priv->close_reply[0]) {
            json_object_set_new(kw_close, "reply", json_string(priv->close_reply));
        }
        gobj_publish_event(gobj, EV_ON_CLOSE, kw_close);
    }
    priv->reject_code = 0;
    priv->auth_reject_code = 0;
    priv->close_reply[0] = 0;

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Bytes arriving from C_TCP. Feed them to the line accumulator; each full
 *  CRLF-terminated line will fire EV_RX_LINE back to us (handled by
 *  ac_rx_line). The dispatch is synchronous, so the loop drains the gbuf
 *  one line at a time.
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);

    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "rx %s <== %s",
            gobj_short_name(gobj),
            gobj_short_name(src)
        );
    }

    while(gbuffer_leftbytes(gbuf) > 0) {
        char *bf = gbuffer_cur_rd_pointer(gbuf);
        size_t len = gbuffer_leftbytes(gbuf);
        size_t consumed = istream_consume(priv->istream_in, bf, len);
        if(consumed == 0) {
            /*
             *  The istream is full: the server sent a line longer than
             *  LINE_BUFFER_MAX. Stop, to avoid spinning.
             */
            char dump[REPLY_TEXT_MAX];
            snprintf(dump, sizeof(dump), "%.*s",
                (int)(len < sizeof(dump) - 1? len : sizeof(dump) - 1), bf
            );
            abort_session_by_peer(gobj, "SMTP reply line too long", 0, dump);
            break;
        }
        gbuffer_get(gbuf, consumed);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A complete SMTP reply line is ready. The istream delivered it as
 *  kw["gbuffer"] including the trailing CRLF. Parse the 3-digit code and
 *  dispatch by current FSM state.
 ***************************************************************************/
PRIVATE int ac_rx_line(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);

    char *line = gbuffer_cur_rd_pointer(gbuf);
    size_t line_len = gbuffer_leftbytes(gbuf);

    int code = 0;
    BOOL is_final = TRUE;
    if(parse_response_code(line, line_len, &code, &is_final) < 0) {
        char dump[REPLY_TEXT_MAX];
        snprintf(dump, sizeof(dump), "%.*s",
            (int)(line_len < sizeof(dump) - 1? line_len : sizeof(dump) - 1), line
        );
        abort_session_by_peer(gobj, "malformed SMTP reply line", 0, dump);
        KW_DECREF(kw)
        return -1;
    }

    if(gobj_trace_level(gobj) & TRACE_SMTP) {
        /* trim CRLF for the trace */
        size_t tlen = line_len;
        while(tlen > 0 && (line[tlen - 1] == '\r' || line[tlen - 1] == '\n')) {
            tlen--;
        }
        gobj_trace_msg(gobj, "<<< %.*s", (int)tlen, line);
    }

    /*
     *  The text of the reply, kept: `line` lives in the kw's gbuffer, which
     *  the KW_DECREF below releases, and a refusal says WHY in its text
     *  ("535 5.7.1 Authentication failed" is a blocked account at OVH, not
     *  a wrong password -- the code alone cannot tell them apart).
     */
    char reply[REPLY_TEXT_MAX];
    size_t rlen = line_len;
    while(rlen > 0 && (line[rlen - 1] == '\r' || line[rlen - 1] == '\n')) {
        rlen--;
    }
    snprintf(reply, sizeof(reply), "%.*s", (int)rlen, line);

    /*
     *  Re-arm for the next line BEFORE running any handler that may send.
     *  KW_DECREF below will release the gbuffer payload via gobj's
     *  registered auto-cleanup for the "gbuffer" key (see gobj.c:562) —
     *  we MUST NOT decref it manually or we'd double-free.
     */
    if(istream_read_until_delimiter(priv->istream_in, "\r\n", 2, EV_RX_LINE) < 0) {
        /* Error already logged by istream_read_until_delimiter */
        KW_DECREF(kw)
        return -1;
    }

    KW_DECREF(kw)

    /*
     *  Multi-line replies: only act on the final line of a reply group.
     *  Continuation lines (250-FOO, 250-BAR, ..., 250 BAZ) are logged and
     *  ignored by the FSM.
     */
    if(!is_final) {
        return 0;
    }

    clear_timeout(priv->timer);

    gobj_state_t st = gobj_current_state(gobj);

    if(st == ST_WAIT_BANNER) {
        if(code != SMTP_CODE_SERVICE_READY) {
            return abort_session_by_peer(gobj, "server did not greet with 220", code, reply);
        }
        char line_ehlo[NAME_MAX];
        const char *helo_name = gobj_read_str_attr(gobj, "helo_name");
        snprintf(line_ehlo, sizeof(line_ehlo), "EHLO %s", helo_name);
        gobj_change_state(gobj, ST_WAIT_EHLO_RESP);
        set_timeout(priv->timer, priv->timeout_response);
        return send_smtp_line(gobj, line_ehlo);
    }

    if(st == ST_WAIT_EHLO_RESP) {
        if(code != SMTP_CODE_OK) {
            return abort_session_by_peer(gobj, "EHLO rejected", code, reply);
        }
        const char *username = gobj_read_str_attr(gobj, "username");
        const char *password = gobj_read_str_attr(gobj, "password");
        if(empty_string(username) || empty_string(password)) {
            /* No credentials: skip AUTH, jump straight to ST_IDLE */
            return enter_idle_after_handshake(gobj);
        }
        size_t ulen = strlen(username);
        size_t plen = strlen(password);
        /* AUTH PLAIN payload: \0 username \0 password */
        size_t plain_len = 1 + ulen + 1 + plen;
        char *plain = gbmem_malloc(plain_len);
        if(!plain) {
            return abort_session_on_error(gobj, "no memory for AUTH PLAIN payload");
        }
        plain[0] = '\0';
        memcpy(plain + 1, username, ulen);
        plain[1 + ulen] = '\0';
        memcpy(plain + 1 + ulen + 1, password, plen);

        /*
         *  plain, its base64 and the AUTH line hold the password in clear:
         *  wiped before their memory is given back.
         */
        gbuffer_t *b64 = gbuffer_binary_to_base64(plain, plain_len);
        explicit_bzero(plain, plain_len);
        gbmem_free(plain);
        if(!b64) {
            return abort_session_on_error(gobj, "base64 encode failed");
        }
        char auth_line[LINE_BUFFER_MAX];
        size_t b64_len = gbuffer_leftbytes(b64);
        if(b64_len + sizeof("AUTH PLAIN ") >= sizeof(auth_line)) {
            explicit_bzero(gbuffer_cur_rd_pointer(b64), b64_len);
            GBUFFER_DECREF(b64)
            return abort_session_on_error(gobj, "AUTH PLAIN line too long");
        }
        snprintf(auth_line, sizeof(auth_line), "AUTH PLAIN %.*s",
            (int)b64_len, (char *)gbuffer_cur_rd_pointer(b64));
        explicit_bzero(gbuffer_cur_rd_pointer(b64), b64_len);
        GBUFFER_DECREF(b64)

        gobj_change_state(gobj, ST_WAIT_AUTH_RESP);
        set_timeout(priv->timer, priv->timeout_response);
        int ret = send_smtp_line(gobj, auth_line);
        explicit_bzero(auth_line, sizeof(auth_line));
        return ret;
    }

    if(st == ST_WAIT_AUTH_RESP) {
        if(code == SMTP_CODE_AUTH_OK) {
            return enter_idle_after_handshake(gobj);
        }
        if(code >= 500 && code < 600) {
            /*
             *  A permanent refusal (535, 534, 530, 538, ...): the
             *  credentials are wrong, and the owner must stop trying them.
             */
            priv->auth_reject_code = code;
            return abort_session_by_peer(gobj, "AUTH PLAIN rejected", code, reply);
        }
        /*
         *  A transient one (454 temporary authentication failure, 421,
         *  432, ...) says nothing of the credentials: it closes the session
         *  like any drop, and the login is tried again at the next
         *  connection. Up to 7.25.20 it was taken as a refusal, and the
         *  owner exited, not to be relaunched, over a provider's hiccup.
         */
        return abort_session_by_peer(gobj, "AUTH PLAIN failed, transient: will retry", code, reply);
    }

    if(st == ST_WAIT_MAIL_FROM_RESP) {
        if(code != SMTP_CODE_OK) {
            return reject_current_message(gobj, code, reply, "MAIL FROM rejected");
        }
        return send_next_rcpt_or_data(gobj);
    }

    if(st == ST_WAIT_RCPT_TO_RESP) {
        if(code != SMTP_CODE_OK) {
            return reject_current_message(gobj, code, reply, "RCPT TO rejected");
        }
        return send_next_rcpt_or_data(gobj);
    }

    if(st == ST_WAIT_DATA_GO) {
        if(code != SMTP_CODE_START_INPUT) {
            return reject_current_message(gobj, code, reply, "DATA not accepted");
        }
        /*
         *  Body is pre-formed by the caller as kw["body"] — full RFC 5322
         *  message with headers (From, To, Subject, Date, MIME-*, ...).
         *  Dot-stuffing and the terminating "\r\n.\r\n" are appended here.
         *  TODO: MIME multipart encoder will live next to this gclass and
         *  produce the body before EV_SEND_MESSAGE; this stage stays the
         *  same.
         */
        const char *body = kw_get_str(gobj, priv->jn_current_msg, "body", "", 0);
        size_t body_len = strlen(body);

        /* Worst case: every byte is a leading '.' → body_len * 2 */
        size_t cap = body_len * 2 + 8;
        gbuffer_t *gbuf_body = gbuffer_create(body_len + 8, cap);
        if(!gbuf_body) {
            return abort_session_on_error(gobj, "no memory for DATA body");
        }
        if(dot_stuff_into(gbuf_body, body, body_len) < 0) {
            GBUFFER_DECREF(gbuf_body)
            return abort_session_on_error(gobj, "dot-stuff into gbuffer failed");
        }
        if(body_len < 2 ||
           body[body_len - 2] != '\r' || body[body_len - 1] != '\n') {
            gbuffer_append(gbuf_body, "\r\n", 2);
        }
        gbuffer_append(gbuf_body, ".\r\n", 3);

        json_t *kw_tx = json_pack("{s:I}",
            "gbuffer", (json_int_t)(uintptr_t)gbuf_body
        );
        gobj_change_state(gobj, ST_WAIT_DATA_RESP);
        set_timeout(priv->timer, priv->timeout_response);
        return gobj_send_event(gobj_bottom_gobj(gobj), EV_TX_DATA, kw_tx, gobj);
    }

    if(st == ST_IDLE) {
        /*
         *  Server-initiated speak while we are idle — almost always a
         *  421 timeout ("Service not available, closing transmission
         *  channel") because submission servers (OVH ssl0.ovh.net in
         *  particular) close inactive sessions aggressively. Treat as
         *  a graceful close: drop the TCP cleanly, log INFO not ERROR.
         *  C_TCP will auto-reconnect when the next email is enqueued.
         */
        gobj_log_info(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INFO,
            "msg",      "%s", "server closed idle SMTP session",
            "code",     "%d", code,
            NULL
        );
        gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
        return 0;
    }

    if(st == ST_WAIT_DATA_RESP) {
        BOOL ok = (code == SMTP_CODE_OK);
        if(!ok) {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL,
                "msg",          "%s", "DATA body rejected by server",
                "code",         "%d", code,
                "reply",        "%s", reply,
                NULL
            );
        }
        json_t *kw_ack = json_pack("{s:b, s:i}",
            "ok", ok ? 1 : 0,
            "code", code
        );
        /*
         *  Resolve BEFORE publishing: a subscriber reacting to EV_ON_MESSAGE
         *  may dispatch the next queued message straight away (the connection
         *  stays up), and it must find us idle with nothing in flight.
         */
        cleanup_current_message(gobj);
        if(ok) {
            /*
             *  The server works: the next aborted session starts the
             *  pacing again from timeout_retry.
             */
            priv->retry_delay = 0;
            gobj_write_integer_attr(gobj_bottom_gobj(gobj), "timeout_between_connections",
                gobj_read_integer_attr(gobj, "timeout_retry")
            );
        }
        gobj_change_state(gobj, ST_IDLE);
        gobj_publish_event(gobj, EV_ON_MESSAGE, kw_ack);
        return 0;
    }

    /* Any other state: unexpected reply, drop the session. */
    return abort_session_by_peer(gobj, "unexpected SMTP reply for current state", code, reply);
}

/***************************************************************************
 *  Per-command server response watchdog. Treat as a soft failure: log,
 *  ack-nack the current message (if any), drop the TCP.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Per-command watchdog fired. Treat as a transient failure: drop the
     *  session with reject_code 0. ac_disconnected forwards EV_ON_CLOSE
     *  (no code) and the owner retries the in-flight message on reconnect.
     *  Resolving via EV_ON_CLOSE (not EV_ON_MESSAGE) avoids dispatching the
     *  next message into the session we are about to tear down.
     */
    priv->reject_code = 0;
    abort_session_by_peer(gobj, "timeout waiting for SMTP response", 0, NULL);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Incoming send-message request from above. kw owns:
 *      "from"    str  (envelope sender)
 *      "to"      str  (comma-separated recipients — primary)
 *      "cc"      str  (comma-separated recipients — copy)
 *      "bcc"     str  (comma-separated recipients — blind)
 *      "body"    str  (full RFC 5322 message with headers)
 *
 *  Recipients across to/cc/bcc are gathered into a flat, deduplicated list
 *  for the SMTP envelope (one RCPT TO per address). The body is the caller's
 *  problem: it must already carry the visible To: / Cc: headers and OMIT
 *  Bcc: (per RFC 5322 §3.6.3) — c_smtp_session does not edit the body.
 *
 *  If we are not authenticated yet, stash the message and the handshake
 *  loop will pick it up on entry to ST_IDLE.
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->jn_current_msg) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "EV_SEND_MESSAGE while another message in flight",
            NULL
        );
        json_t *kw_ack = json_pack("{s:b, s:i}",
            "ok", 0,
            "code", 0
        );
        gobj_publish_event(gobj, EV_ON_MESSAGE, kw_ack);
        KW_DECREF(kw)
        return -1;
    }

    json_t *jn_rcpts = gather_recipients(gobj, kw);
    if(!jn_rcpts) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "EV_SEND_MESSAGE has no valid recipients",
            NULL
        );
        json_t *kw_ack = json_pack("{s:b, s:i}",
            "ok", 0,
            "code", 0
        );
        gobj_publish_event(gobj, EV_ON_MESSAGE, kw_ack);
        KW_DECREF(kw)
        return -1;
    }

    JSON_INCREF(kw)
    priv->jn_current_msg = kw;
    priv->jn_recipients = jn_rcpts;
    priv->recipient_index = 0;

    if(gobj_current_state(gobj) == ST_IDLE) {
        int ret = begin_send_current_message(gobj);
        KW_DECREF(kw)
        return ret;
    }

    /*
     *  Not ready to send yet. The message is stashed; enter_idle_after_handshake
     *  begins it when we reach ST_IDLE. If the bottom C_TCP idle-closed
     *  (timeout_inactivity, so it does not auto-reconnect), bring it back up now
     *  — reconnection is this session's job (it owns the transport and must
     *  redo the SMTP handshake), NOT the owner's. If a connect/handshake is
     *  already in progress, just wait for it.
     */
    hgobj bottom = gobj_bottom_gobj(gobj);
    if(bottom && gobj_current_state(bottom) == ST_DISCONNECTED) {
        gobj_send_event(bottom, EV_CONNECT, 0, gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  External request to drop the session — forward to bottom.
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Bottom child stopped.
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
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
    .mt_create  = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_writing = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_SMTP_SESSION);

/*------------------------*
 *      States
 *------------------------*/
/* All custom states defined near the top of the file. */

/*------------------------*
 *      Events
 *------------------------*/
/* EV_RX_LINE defined near the top of the file. */

/***************************************************************************
 *          Create the GClass
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

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_disconnected[] = {
        {EV_SEND_MESSAGE,       ac_send_message,        0},
        {EV_CONNECTED,          ac_connected,           0},
        {EV_DISCONNECTED,       ac_disconnected,        0},
        {EV_STOPPED,            ac_stopped,             0},
        {0,0,0}
    };
    /*
     *  EV_SEND_MESSAGE is taken in every state before ST_IDLE: the owner
     *  sends when it has a message, whether or not the session is up, and
     *  ac_send_message stashes it until enter_idle_after_handshake begins
     *  it. Up to 7.25.20 only ST_DISCONNECTED and ST_IDLE took it, and a
     *  message sent during the handshake was refused ("Event NOT DEFINED")
     *  and spent a retry -- all of them at once, since the owner retries a
     *  refused send in the same cycle.
     */
    ev_action_t st_wait_connected[] = {
        {EV_SEND_MESSAGE,       ac_send_message,        0},
        {EV_CONNECTED,          ac_connected,           0},
        {EV_DISCONNECTED,       ac_disconnected,        ST_DISCONNECTED},
        {0,0,0}
    };
    /*
     *  In every state with the istream armed: EV_RX_DATA from C_TCP feeds
     *  raw bytes into the istream (ac_rx_data); EV_RX_LINE from the istream
     *  delivers a parsed CRLF line to ac_rx_line, which advances the FSM.
     */
    ev_action_t st_wait_banner[] = {
        {EV_SEND_MESSAGE,       ac_send_message,        0},
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_TIMEOUT,            ac_timeout,             0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        ST_DISCONNECTED},
        {0,0,0}
    };
    ev_action_t st_wait_ehlo_resp[] = {
        {EV_SEND_MESSAGE,       ac_send_message,        0},
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_TIMEOUT,            ac_timeout,             0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        ST_DISCONNECTED},
        {0,0,0}
    };
    ev_action_t st_wait_auth_resp[] = {
        {EV_SEND_MESSAGE,       ac_send_message,        0},
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_TIMEOUT,            ac_timeout,             0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        ST_DISCONNECTED},
        {0,0,0}
    };
    ev_action_t st_idle[] = {
        {EV_SEND_MESSAGE,       ac_send_message,        0},
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        ST_DISCONNECTED},
        {0,0,0}
    };
    ev_action_t st_wait_mail_from_resp[] = {
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_TIMEOUT,            ac_timeout,             0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        ST_DISCONNECTED},
        {0,0,0}
    };
    ev_action_t st_wait_rcpt_to_resp[] = {
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_TIMEOUT,            ac_timeout,             0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        ST_DISCONNECTED},
        {0,0,0}
    };
    ev_action_t st_wait_data_go[] = {
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_TIMEOUT,            ac_timeout,             0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        ST_DISCONNECTED},
        {0,0,0}
    };
    ev_action_t st_wait_data_resp[] = {
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_TIMEOUT,            ac_timeout,             0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        ST_DISCONNECTED},
        {0,0,0}
    };

    states_t states[] = {
        {ST_DISCONNECTED,           st_disconnected},
        {ST_WAIT_CONNECTED,         st_wait_connected},
        {ST_WAIT_BANNER,            st_wait_banner},
        {ST_WAIT_EHLO_RESP,         st_wait_ehlo_resp},
        {ST_WAIT_AUTH_RESP,         st_wait_auth_resp},
        {ST_IDLE,                   st_idle},
        {ST_WAIT_MAIL_FROM_RESP,    st_wait_mail_from_resp},
        {ST_WAIT_RCPT_TO_RESP,      st_wait_rcpt_to_resp},
        {ST_WAIT_DATA_GO,           st_wait_data_go},
        {ST_WAIT_DATA_RESP,         st_wait_data_resp},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_RX_DATA,            0},
        {EV_RX_LINE,            0},
        {EV_TX_READY,           0},
        {EV_SEND_MESSAGE,       0},
        {EV_CONNECTED,          0},
        {EV_DISCONNECTED,       0},
        {EV_DROP,               0},
        {EV_STOPPED,            0},
        {EV_TIMEOUT,            0},
        {EV_ON_OPEN,            EVF_OUTPUT_EVENT},
        {EV_ON_CLOSE,           EVF_OUTPUT_EVENT},
        {EV_ON_MESSAGE,         EVF_OUTPUT_EVENT},
        {NULL, 0}
    };

    /*----------------------------------------*
     *          Register GClass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0, // local methods
        attrs_table,
        sizeof(PRIVATE_DATA),
        0, // authz_table,
        0, // command_table,
        s_user_trace_level,
        gcflag_manual_start // gcflags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_smtp_session(void)
{
    return create_gclass(C_SMTP_SESSION);
}
