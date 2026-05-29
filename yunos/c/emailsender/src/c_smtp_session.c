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
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include <istream.h>
#include "c_smtp_session.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define DEFAULT_TIMEOUT_RESPONSE_MS    30000   /* per-command server response watchdog */
#define LINE_BUFFER_INITIAL            512
#define LINE_BUFFER_MAX                8192    /* RFC 5321 §4.5.3.1.6 reply line limit */

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
PRIVATE int send_next_rcpt_or_data(hgobj gobj);
PRIVATE json_t *gather_recipients(hgobj gobj, json_t *jn_msg);
PRIVATE int dot_stuff_into(gbuffer_t *out, const char *body, size_t len);
PRIVATE void cleanup_current_message(hgobj gobj);
PRIVATE int abort_session(hgobj gobj, const char *reason);

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
SDATA (DTP_POINTER, "subscriber",       0,          0,          "Subscriber of output-events. Default if null is parent."),
SDATA (DTP_POINTER, "user_data",        0,          0,          "user data"),
SDATA (DTP_POINTER, "user_data2",       0,          0,          "more user data"),
SDATA (DTP_STRING,  "url",              SDF_RD,     "",         "SMTP server URL (smtps://host:465). If set, mt_start auto-creates a C_TCP bottom."),
SDATA (DTP_STRING,  "helo_name",        SDF_RD,     "localhost","EHLO domain advertised to the server"),
SDATA (DTP_STRING,  "username",         SDF_RD,     "",         "SMTP AUTH PLAIN username"),
SDATA (DTP_STRING,  "password",         SDF_RD,     "",         "SMTP AUTH PLAIN password"),
SDATA (DTP_INTEGER, "timeout_response", SDF_RD,     "30000",    "Per-command server response timeout (ms)"),
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
{"smtp",     "Trace SMTP FSM phases (commands sent + reply codes)"},
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
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    const char *url = gobj_read_str_attr(gobj, "url");
    hgobj bottom = gobj_bottom_gobj(gobj);

    if(!empty_string(url) && !bottom) {
        json_t *kw_tcp = json_pack("{s:s}",
            "url", url
        );
        bottom = gobj_create_pure_child(gobj_name(gobj), C_TCP, kw_tcp, gobj);
        if(!bottom) {
            /* Error already logged by gobj_create_pure_child */
            return -1;
        }
        gobj_set_bottom_gobj(gobj, bottom);
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
    gbuffer_append(gbuf, (void *)line, line_len);
    gbuffer_append(gbuf, "\r\n", 2);

    if(gobj_trace_level(gobj) & TRACE_SMTP) {
        gobj_trace_msg(gobj, ">>> %s", line);
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
 *  Tear the SMTP session down on protocol error / unexpected reply.
 *  Logs once with reason, drops the underlying TCP, and lets ac_disconnected
 *  publish EV_ON_CLOSE upward.
 ***************************************************************************/
PRIVATE int abort_session(hgobj gobj, const char *reason)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PROTOCOL,
        "msg",          "%s", reason,
        NULL
    );
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
    return -1;
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
 *  TCP layer reports disconnect. Publish EV_ON_CLOSE upward if we had
 *  reported an EV_ON_OPEN, and reset our line buffer.
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
        gobj_publish_event(gobj, EV_ON_CLOSE, 0);
    }

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
            /* istream rejected (e.g. full); stop to avoid spinning */
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "istream_consume returned 0, dropping connection",
                "len",          "%d", (int)len,
                NULL
            );
            abort_session(gobj, "line too long or istream full");
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
        abort_session(gobj, "malformed SMTP reply line");
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
            return abort_session(gobj, "server did not greet with 220");
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
            return abort_session(gobj, "EHLO rejected");
        }
        const char *username = gobj_read_str_attr(gobj, "username");
        const char *password = gobj_read_str_attr(gobj, "password");
        if(empty_string(username) || empty_string(password)) {
            /* No credentials: skip AUTH, jump straight to ST_IDLE */
            gobj_change_state(gobj, ST_IDLE);
            gobj_publish_event(gobj, EV_ON_OPEN, 0);
            return 0;
        }
        size_t ulen = strlen(username);
        size_t plen = strlen(password);
        /* AUTH PLAIN payload: \0 username \0 password */
        size_t plain_len = 1 + ulen + 1 + plen;
        char *plain = gbmem_malloc(plain_len);
        if(!plain) {
            return abort_session(gobj, "no memory for AUTH PLAIN payload");
        }
        plain[0] = '\0';
        memcpy(plain + 1, username, ulen);
        plain[1 + ulen] = '\0';
        memcpy(plain + 1 + ulen + 1, password, plen);

        gbuffer_t *b64 = gbuffer_binary_to_base64(plain, plain_len);
        gbmem_free(plain);
        if(!b64) {
            return abort_session(gobj, "base64 encode failed");
        }
        char auth_line[LINE_BUFFER_MAX];
        size_t b64_len = gbuffer_leftbytes(b64);
        if(b64_len + sizeof("AUTH PLAIN ") >= sizeof(auth_line)) {
            GBUFFER_DECREF(b64)
            return abort_session(gobj, "AUTH PLAIN line too long");
        }
        snprintf(auth_line, sizeof(auth_line), "AUTH PLAIN %.*s",
            (int)b64_len, (char *)gbuffer_cur_rd_pointer(b64));
        GBUFFER_DECREF(b64)

        gobj_change_state(gobj, ST_WAIT_AUTH_RESP);
        set_timeout(priv->timer, priv->timeout_response);
        return send_smtp_line(gobj, auth_line);
    }

    if(st == ST_WAIT_AUTH_RESP) {
        if(code != SMTP_CODE_AUTH_OK) {
            return abort_session(gobj, "AUTH PLAIN rejected");
        }
        gobj_change_state(gobj, ST_IDLE);
        gobj_publish_event(gobj, EV_ON_OPEN, 0);
        if(priv->jn_current_msg) {
            return begin_send_current_message(gobj);
        }
        return 0;
    }

    if(st == ST_WAIT_MAIL_FROM_RESP) {
        if(code != SMTP_CODE_OK) {
            return abort_session(gobj, "MAIL FROM rejected");
        }
        return send_next_rcpt_or_data(gobj);
    }

    if(st == ST_WAIT_RCPT_TO_RESP) {
        if(code != SMTP_CODE_OK) {
            return abort_session(gobj, "RCPT TO rejected");
        }
        return send_next_rcpt_or_data(gobj);
    }

    if(st == ST_WAIT_DATA_GO) {
        if(code != SMTP_CODE_START_INPUT) {
            return abort_session(gobj, "DATA not accepted");
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
            return abort_session(gobj, "no memory for DATA body");
        }
        if(dot_stuff_into(gbuf_body, body, body_len) < 0) {
            GBUFFER_DECREF(gbuf_body)
            return abort_session(gobj, "dot-stuff into gbuffer failed");
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
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL,
                "msg",          "%s", "DATA body rejected by server",
                "code",         "%d", code,
                NULL
            );
        }
        json_t *kw_ack = json_pack("{s:b, s:i}",
            "ok", ok ? 1 : 0,
            "code", code
        );
        gobj_publish_event(gobj, EV_ON_MESSAGE, kw_ack);
        cleanup_current_message(gobj);
        gobj_change_state(gobj, ST_IDLE);
        return 0;
    }

    /* Any other state: unexpected reply, drop the session. */
    return abort_session(gobj, "unexpected SMTP reply for current state");
}

/***************************************************************************
 *  Per-command server response watchdog. Treat as a soft failure: log,
 *  ack-nack the current message (if any), drop the TCP.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->jn_current_msg) {
        json_t *kw_ack = json_pack("{s:b, s:i}",
            "ok", 0,
            "code", 0
        );
        gobj_publish_event(gobj, EV_ON_MESSAGE, kw_ack);
        cleanup_current_message(gobj);
    }
    abort_session(gobj, "timeout waiting for SMTP response");

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

    /* Not authenticated yet: the handshake will dispatch on entry to ST_IDLE. */
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
        {EV_CONNECTED,          ac_connected,           0},
        {EV_DISCONNECTED,       ac_disconnected,        0},
        {EV_STOPPED,            ac_stopped,             0},
        {0,0,0}
    };
    ev_action_t st_wait_connected[] = {
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
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_TIMEOUT,            ac_timeout,             0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        ST_DISCONNECTED},
        {0,0,0}
    };
    ev_action_t st_wait_ehlo_resp[] = {
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_TIMEOUT,            ac_timeout,             0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        ST_DISCONNECTED},
        {0,0,0}
    };
    ev_action_t st_wait_auth_resp[] = {
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
