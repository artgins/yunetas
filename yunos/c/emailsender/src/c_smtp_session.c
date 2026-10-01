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
 *          any other drop, to be tried again. A 334 asks for the response on
 *          a line of its own (RFC 4954): it is given once, the same
 *          credentials; a second 334 is a refusal (auth_rejected: 334). A
 *          5xx to the greeting or to EHLO says the server does not take
 *          this client: `refused` on EV_ON_CLOSE, and the owner stops.
 *          Every close caused by a reply carries the reply's text on
 *          EV_ON_CLOSE (`reply`).
 *
 *          A refused sender (any reply but 250 to MAIL FROM) is not the
 *          message's fault: the sender is the same for every message (the
 *          account, its quota, its right to send). It is a failure of the
 *          server, paced, with no retry charged to the message.
 *
 *          A 5xx to RCPT TO refuses that recipient: the message goes to the
 *          others (RFC 5321 §3.3), each refused one a WARNING. A 5xx to
 *          every recipient, to DATA or to the end of DATA refuses THAT
 *          message (a recipient that does not exist, a content refused): it
 *          is answered at once on EV_ON_MESSAGE {ok: false, code, permanent},
 *          and the session goes on. It sends RSET, and the next message goes
 *          on the same connection; a server that refuses RSET with a 5xx is
 *          told QUIT, and the next connection waits timeout_retry. That is
 *          no failure of the server -- for the first FREE_REFUSALS_IN_ROW
 *          refusals in a row. From then on until a delivery the server is
 *          refusing every message (a blocked account): each refusal drops
 *          the session as a failure, paced, and the streak is said ("SMTP
 *          server refusing every message"). A 4xx there (a rate limit, a
 *          greylist, a 421) is a failure, like a reply that never comes.
 *
 *          A session the SERVER ends (a refusal, an unexpected or malformed
 *          reply, a reply that never comes) is logged as a WARNING of
 *          MSGSET_PROTOCOL with the reply, capped: a remote peer can cause
 *          it. An ERROR is for our own failures only (no memory, an encoder),
 *          and for a configuration it cannot work with (a timeout_retry or a
 *          timeout_response below its minimum, taken as the default).
 *
 *          Every connection is the session's: its C_TCP never connects by
 *          itself (connect_on_start FALSE, timeout_between_connections -1),
 *          and the session connects ON DEMAND only -- when it holds a message
 *          to send -- and never before the paced delay. The connect (TCP and
 *          TLS) is watched by timeout_response. After a failure of any kind
 *          -- a 4xx, a refused sender, a reply that never comes, a server
 *          that closes the connection by itself (RST, TLS, a close with no
 *          reply), a connection that cannot even be made (refused, timed
 *          out: seen as the C_TCP's EV_STATE_CHANGED out of
 *          ST_WAIT_CONNECTED, with its `disconnect_cause`) -- the next
 *          connection waits `timeout_retry`, doubled at each failure in a row
 *          up to `timeout_retry_max`. A close that is no failure (the session
 *          was idle and nothing went wrong in it) starts the doubling again,
 *          and nothing connects until there is a message -- then as soon as
 *          the transport is down: a message that comes while it closes is
 *          driven again when it reaches ST_DISCONNECTED
 *          (EV_CONNECT_AFTER_CLOSE, posted to ourselves). The waiting is our
 *          own timer, and it survives a pause and a play of the owner: a
 *          stop in the middle of a failing streak counts as one more
 *          failure. A url changed (the owner writes it, and starts us again)
 *          starts the pacing and the streak afresh: they were another
 *          server's.
 *
 *          The first failure of a streak, while a message waits, is a
 *          WARNING ("SMTP server failing", or "SMTP server refusing every
 *          message"), with its cause; when it has lasted
 *          `timeout_failing_alarm` it is an ERROR, said again at most once
 *          per that period while it lasts (at the retries, there is no timer
 *          for it). A delivery ends it with an INFO ("SMTP server works
 *          again"); a streak of connections and handshakes only ends at the
 *          next handshake that works ("SMTP server answers again"); any
 *          streak ends at a close with no failure. With no message waiting
 *          there is no streak.
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

#define FREE_REFUSALS_IN_ROW           2       /* 5xx refusals of messages in a row that go on at once */
#define MIN_TIMEOUT_RETRY              1000    /* ms, smallest timeout_retry taken */
#define DEFAULT_TIMEOUT_RETRY          2000
#define DEFAULT_TIMEOUT_RETRY_MAX      600000
#define MIN_TIMEOUT_RESPONSE           1000    /* ms, smallest timeout_response taken */

#define MSG_SERVER_FAILING      "SMTP server failing: emails wait, the retries are paced"
#define MSG_SERVER_REFUSING     "SMTP server refusing every message: the next ones are paced"

#define SMTP_CODE_SERVICE_READY        220
#define SMTP_CODE_GOODBYE              221
#define SMTP_CODE_AUTH_OK              235
#define SMTP_CODE_OK                   250
#define SMTP_CODE_AUTH_CHALLENGE       334     /* to AUTH PLAIN with its initial response: see ST_WAIT_AUTH_RESP */
#define SMTP_CODE_START_INPUT          354

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int start_bottom(hgobj gobj);
PRIVATE int send_smtp_line(hgobj gobj, const char *line);
PRIVATE int send_line(hgobj gobj, const char *line, BOOL secret);
PRIVATE int build_auth_plain(hgobj gobj, char *bf, size_t bfsize);
PRIVATE int parse_response_code(const char *bf, size_t len, int *code, BOOL *is_final);
PRIVATE int begin_send_current_message(hgobj gobj);
PRIVATE int enter_idle_after_handshake(hgobj gobj);
PRIVATE int reject_current_message(hgobj gobj, int code, const char *reply, const char *reason);
PRIVATE int refuse_current_message(hgobj gobj, int code, const char *reply, const char *reason);
PRIVATE int enter_idle_after_reset(hgobj gobj);
PRIVATE int quit_session(hgobj gobj);
PRIVATE int send_next_rcpt_or_data(hgobj gobj);
PRIVATE json_t *gather_recipients(hgobj gobj, json_t *jn_msg);
PRIVATE int dot_stuff_into(gbuffer_t *out, const char *body, size_t len);
PRIVATE void cleanup_current_message(hgobj gobj);
PRIVATE int fail_current_message(hgobj gobj, const char *reason);
PRIVATE int abort_session_by_peer(hgobj gobj, const char *reason, int code, const char *reply);
PRIVATE int abort_session_on_error(hgobj gobj, const char *reason);
PRIVATE int drop_session(hgobj gobj, const char *reply);
PRIVATE void pace_next_connection(hgobj gobj, BOOL failure);
PRIVATE void note_failure(hgobj gobj, const char *what, const char *cause, BOOL ends_at_handshake, BOOL waiting);
PRIVATE void note_delivery(hgobj gobj);
PRIVATE void end_failing_streak(hgobj gobj, const char *info);
PRIVATE void reset_pacing(hgobj gobj);
PRIVATE void check_timeouts(hgobj gobj);
PRIVATE BOOL bottom_is_connected(hgobj gobj);
PRIVATE int request_connection(hgobj gobj);

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
GOBJ_DEFINE_STATE(ST_WAIT_RSET_RESP);
GOBJ_DEFINE_STATE(ST_WAIT_QUIT_RESP);
GOBJ_DEFINE_EVENT(EV_RX_LINE);
GOBJ_DEFINE_EVENT(EV_CONNECT_AFTER_CLOSE);

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
SDATA (DTP_INTEGER, "timeout_retry",    SDF_RD,     "2000",     "ms before connecting again after a failed session or connection. Doubles at each failure in a row, up to timeout_retry_max; back to this after a session that ends with no failure"),
SDATA (DTP_INTEGER, "timeout_retry_max",SDF_RD,     "600000",   "Cap of the doubling of timeout_retry (ms)"),
SDATA (DTP_INTEGER, "timeout_failing_alarm",SDF_RD, "3600000",  "ms a server may fail, with emails waiting, before it is an ERROR (said again at most once per this period). 0: never"),
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
    int refuse_code;            /* 5xx to the greeting or to EHLO, forwarded on EV_ON_CLOSE as refused; 0 = none */
    BOOL auth_continued;        /* the 334 of this login was answered already */
    char close_reply[REPLY_TEXT_MAX]; /* text of the reply that closed the session, forwarded on EV_ON_CLOSE as reply */
    json_int_t retry_delay;     /* ms before the connection after the next failed one; 0 = timeout_retry */
    BOOL failed;                /* this connection is being dropped for a failure (drop_session) */
    uint64_t connect_not_before;/* msectimer: no connection on demand before it; 0 = none */
    uint64_t failing_since;     /* monotonic ms of the first failure of the streak; 0 = none */
    uint64_t alarm_not_before;  /* msectimer: the next failing ERROR; 0 = not said yet */
    BOOL detached;              /* stopped: the connection closing is ours, nothing of it is told */
    BOOL connect_posted;        /* EV_CONNECT_AFTER_CLOSE is on its way */
    BOOL connecting;            /* EV_CONNECT sent: the timer is the watchdog of the connect and TLS */
    BOOL connect_timed_out;     /* the watchdog dropped the attempt */
    int refused_in_row;         /* messages refused with a 5xx since the last delivery */
    BOOL refusing;              /* this drop is for a run of refusals */
    BOOL streak_ends_at_handshake; /* the failing streak is of connections/handshakes only */
    int rcpt_accepted;          /* RCPT TO answered 250, of the message in hand */
    int rcpt_last_code;         /* reply to the last RCPT TO refused */
    char rcpt_last_reply[REPLY_TEXT_MAX];
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

    check_timeouts(gobj);

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
        /*
         *  The C_TCP never connects by itself: not at its start
         *  (connect_on_start), not after a close (a timeout_between_connections
         *  of -1 arms no timer). Every connection is ours, on demand, paced
         *  (see the header). Up to 7.25.20 it connected at each start and
         *  reconnected every 2 s for ever, whether there was anything to send
         *  or not.
         */
        json_t *kw_tcp = json_pack("{s:s, s:I, s:I, s:I, s:b, s:o}",
            "url", url,
            "timeout_inactivity", gobj_read_integer_attr(gobj, "timeout_inactivity"),
            "timeout_between_connections", (json_int_t)-1,
            "timeout_between_connections_max", (json_int_t)0,
            "connect_on_start", 0,
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
        /*
         *  Another server: the backoff and the streak were the old one's.
         *  Kept, the corrected server waited the old one's delay (up to
         *  timeout_retry_max) and its first failure could raise the ERROR
         *  of a long failure at once.
         */
        reset_pacing(gobj);
    }

    start_bottom(gobj);

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

    /*
     *  The owner stops us when it pauses, and closes the queue the message
     *  in hand came from: it is dropped, the owner sends it again after its
     *  play. The connection now closing is ours, not the server's: nothing
     *  of it is told upward, and nothing of it paces the next one (see
     *  ac_disconnected). Up to 7.25.20 the message survived the stop and
     *  went out after the next start, answered to an owner that had
     *  forgotten it.
     */
    cleanup_current_message(gobj);
    if(st != ST_DISCONNECTED) {
        priv->detached = TRUE;
        priv->inform_on_close = FALSE;
    }

    /*
     *  Stopped in the middle of a failing streak (a connection or a session
     *  under way, the server failing before it): it counts as one more
     *  failure, so a pause and a play do not buy an attempt at once. Up to
     *  7.25.20 they did, at each play. A stop in a session that works (no
     *  streak) counts nothing.
     */
    hgobj bottom = gobj_bottom_gobj(gobj);
    gobj_state_t bst = bottom? gobj_current_state(bottom) : ST_STOPPED;
    BOOL under_way = (st != ST_DISCONNECTED && st != ST_IDLE &&
            st != ST_WAIT_RSET_RESP && st != ST_WAIT_QUIT_RESP) ||
        bst == ST_WAIT_CONNECTED || bst == ST_WAIT_HANDSHAKE;
    if(under_way && priv->failing_since) {
        pace_next_connection(gobj, TRUE);
    }
    priv->connecting = FALSE;
    priv->connect_timed_out = FALSE;

    if(bottom && gobj_is_running(bottom)) {
        gobj_stop(bottom);
    }

    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Start the bottom C_TCP, unless its last stop has not ended yet: a pause
 *  and a play in a row find it still closing (ST_WAIT_STOPPED), and C_TCP
 *  refuses to start then ("Initial wrong tcp state"). It is started when
 *  its EV_STOPPED comes (ac_stopped). Up to 7.25.20 a quick pause and
 *  play left the session with no transport.
 *
 *  Started, it does not connect (connect_on_start FALSE): request_connection
 *  does, when there is a message.
 ***************************************************************************/
PRIVATE int start_bottom(hgobj gobj)
{
    hgobj bottom = gobj_bottom_gobj(gobj);
    if(!bottom || gobj_is_running(bottom)) {
        return 0;
    }

    gobj_state_t st = gobj_current_state(bottom);
    if(st != ST_STOPPED && st != ST_DISCONNECTED) {
        return 0;   // still closing: ac_stopped starts it
    }
    return gobj_start(bottom);
}

/***************************************************************************
 *  Write one SMTP command line (CRLF appended) to the bottom transport.
 *  An AUTH line carries the credentials: it goes as a secret line.
 ***************************************************************************/
PRIVATE int send_smtp_line(hgobj gobj, const char *line)
{
    return send_line(gobj, line, strncasecmp(line, "AUTH ", 5)==0? TRUE : FALSE);
}

/***************************************************************************
 *  Write one line; a secret one is hidden from the traffic dumps, wiped
 *  when freed, and not written by the smtp trace.
 ***************************************************************************/
PRIVATE int send_line(hgobj gobj, const char *line, BOOL secret)
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
    gbuffer_set_secret(gbuf, secret);
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
        } else if(secret) {
            gobj_trace_msg(gobj, ">>> <credentials not traced>");
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
        return fail_current_message(gobj, "EV_SEND_MESSAGE without 'from'");
    }

    char line[LINE_BUFFER_MAX];
    snprintf(line, sizeof(line), "MAIL FROM:<%s>", from);
    priv->recipient_index = 0;
    priv->rcpt_accepted = 0;
    priv->rcpt_last_code = 0;
    priv->rcpt_last_reply[0] = 0;
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

    if(priv->rcpt_accepted == 0) {
        // every recipient refused, each one logged above
        return refuse_current_message(gobj, priv->rcpt_last_code, priv->rcpt_last_reply,
            "every recipient refused"
        );
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
 *  The message in hand cannot be sent, whatever the server says (no
 *  recipient, no sender): it is answered, once, with EV_ON_MESSAGE
 *  {ok: false, permanent: true}, and dropped. The session stays as it is.
 *
 *  That answer is the ONLY one: whoever sent the message is resolved by
 *  it, and must not resolve it again by the return of its send. Up to
 *  7.25.20 ac_send_message() published the answer AND returned -1: the
 *  emailsender resolved the message twice, re-sending it from inside the
 *  first answer until the failed queue freed it, and then touched it.
 *  The reason is a WARNING: it is the content of the message, not a
 *  failure of ours, and the emailsender logs what it does with it.
 ***************************************************************************/
PRIVATE int fail_current_message(hgobj gobj, const char *reason)
{
    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER,
        "msg",          "%s", reason,
        NULL
    );

    cleanup_current_message(gobj);

    json_t *kw_ack = json_pack("{s:b, s:i, s:b}",
        "ok", 0,
        "code", 0,
        "permanent", 1
    );
    gobj_publish_event(gobj, EV_ON_MESSAGE, kw_ack);
    return -1;
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
 *  Drop the underlying TCP for a failure, keeping the text of the reply
 *  that caused it: ac_disconnected publishes EV_ON_CLOSE upward with it,
 *  and paces the next connection.
 *
 *  Once per connection: a second failure found in the same read (two bad
 *  lines in one packet) is the same failed session, and must not pace the
 *  next connection twice as long.
 ***************************************************************************/
PRIVATE int drop_session(hgobj gobj, const char *reply)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->failed) {
        return -1;  // dropping already
    }
    priv->failed = TRUE;

    snprintf(priv->close_reply, sizeof(priv->close_reply), "%s", reply? reply : "");

    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
    return -1;
}

/***************************************************************************
 *  Set the delay of the next connection, at the end of one.
 *
 *  After a failure: `retry_delay` (timeout_retry the first time), doubled
 *  for the next failure in a row up to timeout_retry_max; no connection is
 *  made before it.
 *
 *  After a close that is no failure: the doubling starts again, and the
 *  next message connects at once. Nothing connects before there is one:
 *  up to 7.25.20 the C_TCP reconnected 2 s after such a close (a server
 *  ending an idle session, a message sent to the failed queue), and logged
 *  in again with nothing to send.
 ***************************************************************************/
PRIVATE void pace_next_connection(hgobj gobj, BOOL failure)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t base = gobj_read_integer_attr(gobj, "timeout_retry");
    json_int_t cap = gobj_read_integer_attr(gobj, "timeout_retry_max");

    if(failure) {
        json_int_t delay = priv->retry_delay < base? base : priv->retry_delay;
        if(delay > cap) {
            delay = cap;
        }
        priv->retry_delay = (delay * 2 < cap)? delay * 2 : cap;
        priv->connect_not_before = start_msectimer((uint64_t)delay);
    } else {
        priv->retry_delay = 0;
        priv->connect_not_before = 0;
    }
}

/***************************************************************************
 *  A failure of the server, from a connection that could not be made to a
 *  run of refused messages, while a message waits (`waiting`): the first of
 *  a streak is a WARNING (`what`) with its cause; a streak longer than
 *  timeout_failing_alarm is an ERROR, said again at most once per that
 *  period. Said here, at the failures, which keep coming at the paced
 *  retries while a message waits: no timer of its own. With no message
 *  waiting there is no streak: nobody waits for the server.
 *
 *  A streak of connections and handshakes only (`ends_at_handshake`) ends
 *  at the next handshake that works; one with a failure of a transaction or
 *  a run of refusals in it ends at a delivery. Any streak ends at a close
 *  with no failure.
 ***************************************************************************/
PRIVATE void note_failure(hgobj gobj, const char *what, const char *cause, BOOL ends_at_handshake, BOOL waiting)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!waiting) {
        return;
    }

    uint64_t now = time_in_milliseconds_monotonic();
    if(!priv->failing_since) {
        priv->failing_since = now;
        priv->streak_ends_at_handshake = ends_at_handshake;
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL,
            "msg",          "%s", what,
            "cause",        "%s", cause? cause : "",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            NULL
        );
    } else if(!ends_at_handshake) {
        priv->streak_ends_at_handshake = FALSE;
    }

    json_int_t alarm = gobj_read_integer_attr(gobj, "timeout_failing_alarm");
    if(alarm <= 0 || now - priv->failing_since < (uint64_t)alarm) {
        return;
    }
    if(priv->alarm_not_before && !test_msectimer(priv->alarm_not_before)) {
        return;     // said less than one period ago
    }
    priv->alarm_not_before = start_msectimer((uint64_t)alarm);
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_OPERATIONAL,
        "msg",          "%s", "SMTP server failing for too long: emails are NOT being sent",
        "failing_s",    "%ld", (long)((now - priv->failing_since) / 1000),
        "cause",        "%s", cause? cause : "",
        "url",          "%s", gobj_read_str_attr(gobj, "url"),
        NULL
    );
}

/***************************************************************************
 *  The failing streak is over: said with `info` (an INFO), or silently.
 ***************************************************************************/
PRIVATE void end_failing_streak(hgobj gobj, const char *info)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->failing_since && info) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL,
            "msg",          "%s", info,
            "failed_s",     "%ld", (long)((time_in_milliseconds_monotonic() - priv->failing_since) / 1000),
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            NULL
        );
    }
    priv->failing_since = 0;
    priv->alarm_not_before = 0;
    priv->streak_ends_at_handshake = FALSE;
}

/***************************************************************************
 *  A message delivered: the streak of failures, if any, is over, and so is
 *  a run of refusals.
 ***************************************************************************/
PRIVATE void note_delivery(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    end_failing_streak(gobj, "SMTP server works again: emails delivered");
    priv->refused_in_row = 0;
}

/***************************************************************************
 *  Forget the pacing and the streak: another server (a url changed).
 ***************************************************************************/
PRIVATE void reset_pacing(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->retry_delay = 0;
    priv->connect_not_before = 0;
    priv->refused_in_row = 0;
    end_failing_streak(gobj, NULL);
}

/***************************************************************************
 *  The timeouts a session cannot work with are refused, logged, and the
 *  default taken: a timeout_retry of 0 reconnected in a tight loop after
 *  each failure of a handshake (which spends no retry), and a timer of 0
 *  arms nothing, so a message waited for ever.
 ***************************************************************************/
PRIVATE void check_timeouts(hgobj gobj)
{
    json_int_t retry = gobj_read_integer_attr(gobj, "timeout_retry");
    if(retry < MIN_TIMEOUT_RETRY) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONFIGURATION,
            "msg",          "%s", "timeout_retry below its minimum: the default is taken",
            "timeout_retry","%ld", (long)retry,
            "minimum",      "%d", MIN_TIMEOUT_RETRY,
            "default",      "%d", DEFAULT_TIMEOUT_RETRY,
            NULL
        );
        retry = DEFAULT_TIMEOUT_RETRY;
        gobj_write_integer_attr(gobj, "timeout_retry", retry);
    }

    json_int_t retry_max = gobj_read_integer_attr(gobj, "timeout_retry_max");
    if(retry_max < retry) {
        json_int_t fixed = DEFAULT_TIMEOUT_RETRY_MAX < retry? retry : DEFAULT_TIMEOUT_RETRY_MAX;
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONFIGURATION,
            "msg",          "%s", "timeout_retry_max below timeout_retry: the default is taken",
            "timeout_retry_max", "%ld", (long)retry_max,
            "timeout_retry","%ld", (long)retry,
            "default",      "%ld", (long)fixed,
            NULL
        );
        gobj_write_integer_attr(gobj, "timeout_retry_max", fixed);
    }

    json_int_t response = gobj_read_integer_attr(gobj, "timeout_response");
    if(response < MIN_TIMEOUT_RESPONSE) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONFIGURATION,
            "msg",          "%s", "timeout_response below its minimum: the default is taken",
            "timeout_response", "%ld", (long)response,
            "minimum",      "%d", MIN_TIMEOUT_RESPONSE,
            "default",      "%d", DEFAULT_TIMEOUT_RESPONSE_MS,
            NULL
        );
        gobj_write_integer_attr(gobj, "timeout_response", DEFAULT_TIMEOUT_RESPONSE_MS);
    }
}

/***************************************************************************
 *  TRUE when the bottom C_TCP is up, not closing: a message can be begun.
 ***************************************************************************/
PRIVATE BOOL bottom_is_connected(hgobj gobj)
{
    hgobj bottom = gobj_bottom_gobj(gobj);
    return (bottom && gobj_current_state(bottom) == ST_CONNECTED)? TRUE : FALSE;
}

/***************************************************************************
 *  A message is waiting and we are disconnected: connect, when the paced
 *  delay allows it. Before, wait for it on our timer (EV_TIMEOUT in
 *  ST_DISCONNECTED comes back here). Up to 7.25.20 a message queued while
 *  a paced reconnection was pending connected at once.
 *
 *  A transport that is still closing (inside the close, or waiting for its
 *  last operation) is left alone: its arrival in ST_DISCONNECTED comes back
 *  here (ac_child_state_changed). Before, a message that came while the
 *  transport closed with no failure waited timeout_retry (inside the close)
 *  or for ever (a failed connect that closed with no message in hand).
 *
 *  The connect is watched: the timer is armed with timeout_response from
 *  the EV_CONNECT to the EV_CONNECTED (TCP and TLS). Up to 7.25.20 nothing
 *  watched them, and a server that took the TCP connection and stalled the
 *  TLS handshake left the message waiting for ever, with nothing logged.
 ***************************************************************************/
PRIVATE int request_connection(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj bottom = gobj_bottom_gobj(gobj);
    if(bottom && !gobj_is_running(bottom)) {
        start_bottom(gobj);     // or later: its close comes back here (ac_stopped)
    }
    if(!bottom || !gobj_is_running(bottom)) {
        return 0;   // not started yet, or closing: ac_stopped comes back here
    }
    if(gobj_current_state(bottom) != ST_DISCONNECTED) {
        return 0;   // connecting or connected (the handshake takes the message), or closing
    }

    if(priv->connect_not_before && !test_msectimer(priv->connect_not_before)) {
        set_timeout(priv->timer,
            (json_int_t)(priv->connect_not_before - time_in_milliseconds_monotonic())
        );
        return 0;
    }

    priv->connecting = TRUE;
    set_timeout(priv->timer, priv->timeout_response);
    return gobj_send_event(bottom, EV_CONNECT, 0, gobj);
}

/***************************************************************************
 *  Write in `bf` the base64 of the AUTH PLAIN response (NUL user NUL
 *  password). The buffers that held the password in clear are wiped
 *  before their memory is given back. 0, or -1 (logged).
 ***************************************************************************/
PRIVATE int build_auth_plain(hgobj gobj, char *bf, size_t bfsize)
{
    const char *username = gobj_read_str_attr(gobj, "username");
    const char *password = gobj_read_str_attr(gobj, "password");
    size_t ulen = strlen(username);
    size_t plen = strlen(password);
    size_t plain_len = 1 + ulen + 1 + plen;

    char *plain = gbmem_malloc(plain_len);
    if(!plain) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "no memory for AUTH PLAIN payload",
            NULL
        );
        return -1;
    }
    plain[0] = '\0';
    memcpy(plain + 1, username, ulen);
    plain[1 + ulen] = '\0';
    memcpy(plain + 1 + ulen + 1, password, plen);

    gbuffer_t *b64 = gbuffer_binary_to_base64(plain, plain_len);
    explicit_bzero(plain, plain_len);
    gbmem_free(plain);
    if(!b64) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "base64 encode failed",
            NULL
        );
        return -1;
    }

    size_t b64_len = gbuffer_leftbytes(b64);
    if(b64_len >= bfsize) {
        explicit_bzero(gbuffer_cur_rd_pointer(b64), b64_len);
        GBUFFER_DECREF(b64)
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "AUTH PLAIN line too long",
            NULL
        );
        return -1;
    }
    memcpy(bf, gbuffer_cur_rd_pointer(b64), b64_len);
    bf[b64_len] = 0;
    explicit_bzero(gbuffer_cur_rd_pointer(b64), b64_len);
    GBUFFER_DECREF(b64)
    return 0;
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
    if(priv->streak_ends_at_handshake) {
        end_failing_streak(gobj, "SMTP server answers again");
    }
    BOOL had_pending = (priv->jn_current_msg != NULL);
    gobj_publish_event(gobj, EV_ON_OPEN, 0);
    if(had_pending) {
        return begin_send_current_message(gobj);
    }
    return 0;
}

/***************************************************************************
 *  A rejection mid-transaction (an unexpected reply to MAIL FROM / RCPT TO
 *  / DATA / the end of DATA).
 *
 *  A 5xx refuses the message, not the server: refuse_current_message().
 *
 *  Anything else (a 4xx: a rate limit, a greylist, a 421) says the server
 *  cannot take it NOW: the session is dropped, and the reconnection paced.
 *  The code goes to the owner on EV_ON_CLOSE, and the owner retries. It is
 *  resolved through EV_ON_CLOSE rather than EV_ON_MESSAGE on purpose: the
 *  owner sees smtp_ready=FALSE before it tries to dispatch the next queued
 *  message, so it cannot push it into the dying connection.
 ***************************************************************************/
PRIVATE int reject_current_message(hgobj gobj, int code, const char *reply, const char *reason)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(code >= 500 && code < 600) {
        return refuse_current_message(gobj, code, reply, reason);
    }
    priv->reject_code = code;
    return abort_session_by_peer(gobj, reason, code, reply);
}

/***************************************************************************
 *  A 5xx refused THIS message (550 no such user, 554 content refused): the
 *  message's fault, not the server's. It is answered at once, once, on
 *  EV_ON_MESSAGE {ok: false, code, permanent: true} (the owner sends it to
 *  its failed queue), and the session goes on: RSET ends the transaction,
 *  and the next message goes on the same connection (a message the owner
 *  sends from inside the answer waits for the RSET's 250). No pacing, no
 *  note_failure(): the server works -- for the first FREE_REFUSALS_IN_ROW
 *  refusals in a row; from then on until a delivery, see above. Up to 7.25.20 a 5xx to MAIL FROM, RCPT TO or DATA dropped the
 *  session, and the next message logged in again.
 *
 *  A WARNING with the reply, as anything a peer causes.
 ***************************************************************************/
PRIVATE int refuse_current_message(hgobj gobj, int code, const char *reply, const char *reason)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->refused_in_row++;
    if(priv->refused_in_row > FREE_REFUSALS_IN_ROW) {
        /*
         *  Refused again, with no delivery since: the server is refusing
         *  every message (an account blocked, a quota, a policy). It still
         *  goes to the failed queue (the code travels on EV_ON_CLOSE), but
         *  the session is dropped as for a failure: the next message waits
         *  the paced delay, and the streak is said ("SMTP server refusing
         *  every message", then the ERROR of timeout_failing_alarm).
         *  Otherwise a queue of such messages would be sent in a few
         *  seconds, one login per message when the server closes after each
         *  refusal.
         */
        priv->refusing = TRUE;
        priv->reject_code = code;
        return abort_session_by_peer(gobj, reason, code, reply);
    }

    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PROTOCOL,
        "msg",          "%s", reason,
        "code",         "%d", code,
        "reply",        "%s", reply? reply : "",
        NULL
    );

    json_t *kw_ack = json_pack("{s:b, s:i, s:b, s:s, s:s}",
        "ok", 0,
        "code", code,
        "permanent", 1,
        "reply", reply? reply : "",
        "url", gobj_read_str_attr(gobj_bottom_gobj(gobj), "url")
    );
    if(!kw_ack) {
        // The message stays in hand: the close answers for it, with its code
        priv->reject_code = code;
        return abort_session_on_error(gobj, "json_pack() FAILED for the answer of a refused message");
    }

    cleanup_current_message(gobj);
    gobj_change_state(gobj, ST_WAIT_RSET_RESP);
    set_timeout(priv->timer, priv->timeout_response);
    send_smtp_line(gobj, "RSET");

    gobj_publish_event(gobj, EV_ON_MESSAGE, kw_ack);
    return 0;
}

/***************************************************************************
 *  RSET answered: the session is idle again, and a message the owner sent
 *  meanwhile is begun.
 ***************************************************************************/
PRIVATE int enter_idle_after_reset(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_change_state(gobj, ST_IDLE);
    if(priv->jn_current_msg) {
        return begin_send_current_message(gobj);
    }
    return 0;
}

/***************************************************************************
 *  The server does not take RSET (a 5xx): say goodbye, and close when the
 *  221 comes (or when it does not). Not a failure: the next message opens
 *  a connection of its own, with no paced delay.
 ***************************************************************************/
PRIVATE int quit_session(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_change_state(gobj, ST_WAIT_QUIT_RESP);
    set_timeout(priv->timer, priv->timeout_response);
    return send_smtp_line(gobj, "QUIT");
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
    priv->failed = FALSE;
    priv->auth_continued = FALSE;
    priv->connecting = FALSE;
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

    /*
     *  The state the connection was in: the EV_DISCONNECTED entries of the
     *  FSM have no next state, the change is ours.
     */
    gobj_state_t prev_state = gobj_current_state(gobj);
    gobj_change_state(gobj, ST_DISCONNECTED);

    if(priv->detached) {
        /*
         *  The close of our own stop (mt_stop). A message the owner sent
         *  after it played again waits here for the new connection.
         */
        priv->detached = FALSE;
        priv->failed = FALSE;
        priv->refusing = FALSE;
        priv->reject_code = 0;
        priv->auth_reject_code = 0;
        priv->refuse_code = 0;
        priv->close_reply[0] = 0;
        KW_DECREF(kw)
        return 0;
    }

    BOOL had_msg = (priv->jn_current_msg != NULL);
    cleanup_current_message(gobj);

    /*
     *  A failure is a close we decided for one (drop_session), or any close
     *  of a session that was not idle: the server closed it in the middle
     *  of the handshake or of a message (a RST, a close after a 421, a TLS
     *  error, a close with no reply). Up to 7.25.20 the second kind was not
     *  one, and the reconnection came after the last delay written.
     */
    /*
     *  A close after a message refused with a 5xx (waiting for the RSET or
     *  the QUIT that follows it: some servers close right after the
     *  refusal) is no failure either, and leaves the pacing as it was.
     */
    BOOL after_refusal = prev_state == ST_WAIT_RSET_RESP || prev_state == ST_WAIT_QUIT_RESP;
    BOOL failure = priv->failed || (prev_state != ST_IDLE && !after_refusal);
    if(failure && !priv->failed) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "SMTP server closed the session",
            "state",        "%s", prev_state,
            NULL
        );
    }
    if(failure) {
        pace_next_connection(gobj, TRUE);
        if(!priv->auth_reject_code && !priv->refuse_code) {
            // a refused login or client is said by the owner, which stops on it
            BOOL in_handshake = prev_state == ST_WAIT_BANNER || prev_state == ST_WAIT_EHLO_RESP ||
                prev_state == ST_WAIT_AUTH_RESP;
            note_failure(gobj,
                priv->refusing? MSG_SERVER_REFUSING : MSG_SERVER_FAILING,
                priv->close_reply[0]? priv->close_reply : "the server closed the session",
                in_handshake && !priv->refusing,
                had_msg
            );
        }
    } else if(after_refusal) {
        /*
         *  The streak and the doubling stay as they were, but the next
         *  connection waits timeout_retry at least: a server that refuses
         *  every message and closes after each refusal would otherwise be
         *  logged in to once per queued message, in a few seconds.
         */
        uint64_t not_before = start_msectimer((uint64_t)gobj_read_integer_attr(gobj, "timeout_retry"));
        if(not_before > priv->connect_not_before) {
            priv->connect_not_before = not_before;
        }
    } else {
        pace_next_connection(gobj, FALSE);
        end_failing_streak(gobj, NULL);     // a session that worked, and nothing more to send
    }
    priv->refusing = FALSE;

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
        /*
         *  `transaction`: the close came during the mail transaction of
         *  the message (its first RCPT TO onwards), so the server may have
         *  seen the message and its failure is the message's. Without it
         *  the session failed in its handshake (banner, EHLO, AUTH), at
         *  MAIL FROM (the sender, the same for every message: the account's
         *  or the server's trouble) or before: the owner must not charge
         *  the message a retry.
         */
        if(prev_state == ST_WAIT_RCPT_TO_RESP ||
                prev_state == ST_WAIT_DATA_GO || prev_state == ST_WAIT_DATA_RESP) {
            json_object_set_new(kw_close, "transaction", json_true());
        }
        if(priv->reject_code) {
            json_object_set_new(kw_close, "code", json_integer(priv->reject_code));
        }
        if(priv->auth_reject_code) {
            json_object_set_new(kw_close, "auth_rejected", json_integer(priv->auth_reject_code));
        }
        if(priv->refuse_code) {
            json_object_set_new(kw_close, "refused", json_integer(priv->refuse_code));
        }
        if(priv->close_reply[0]) {
            json_object_set_new(kw_close, "reply", json_string(priv->close_reply));
        }
        json_object_set_new(kw_close, "url",
            json_string(gobj_read_str_attr(gobj_bottom_gobj(gobj), "url"))
        );
        gobj_publish_event(gobj, EV_ON_CLOSE, kw_close);
    }
    priv->reject_code = 0;
    priv->auth_reject_code = 0;
    priv->refuse_code = 0;
    priv->close_reply[0] = 0;
    priv->failed = FALSE;

    if(priv->jn_current_msg) {
        request_connection(gobj);   // sent again by the owner from our EV_ON_CLOSE
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

    if(priv->detached || priv->failed) {
        /*
         *  A read that completed while our stop, or a drop for a failure,
         *  closes the connection: the session it belongs to is over. A
         *  250 read after the watchdog dropped the session resolved the
         *  message as sent, and the next MAIL FROM went on the dying
         *  connection, whose close then charged a retry to a message that
         *  never left.
         */
        KW_DECREF(kw)
        return 0;
    }

    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "rx %s <== %s",
            gobj_short_name(gobj),
            gobj_short_name(src)
        );
    }

    while(gbuffer_leftbytes(gbuf) > 0) {
        char *bf = gbuffer_cur_rd_pointer(gbuf);
        size_t len = gbuffer_leftbytes(gbuf);

        /*
         *  One line at most per consume, and measured BEFORE it: a line
         *  longer than LINE_BUFFER_MAX is the server's doing, a WARNING.
         *  Up to 7.25.20 it was found by filling the istream, which logs
         *  "gbuf FULL" (and the gbuffer its own two) as ERRORs with a stack
         *  first.
         */
        const char *nl = memchr(bf, '\n', len);
        size_t chunk = nl? (size_t)(nl - bf) + 1 : len;
        if(istream_length(priv->istream_in) + chunk >= LINE_BUFFER_MAX) {
            char dump[REPLY_TEXT_MAX];
            snprintf(dump, sizeof(dump), "%.*s",
                (int)(chunk < sizeof(dump) - 1? chunk : sizeof(dump) - 1), bf
            );
            abort_session_by_peer(gobj, "SMTP reply line too long", 0, dump);
            break;
        }

        size_t consumed = istream_consume(priv->istream_in, bf, chunk);
        if(consumed == 0) {
            // Error already logged by istream_consume
            abort_session_on_error(gobj, "istream_consume() took nothing");
            break;
        }
        gbuffer_get(gbuf, consumed);
        if(priv->failed || !priv->istream_in) {
            /*
             *  The session is over (dropped, and maybe closed already: a
             *  drop with nothing in flight closes synchronously, and its
             *  EV_DISCONNECTED destroyed the istream). The rest of the
             *  bytes belong to it.
             */
            break;
        }
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
        if(code >= 500 && code < 600) {
            /*
             *  A 5xx greeting: the server does not take connections from
             *  this client, and answers every attempt the same way. It goes
             *  up as `refused` on EV_ON_CLOSE, and the owner stops. Before,
             *  it was retried, paced, for ever.
             */
            priv->refuse_code = code;
            return abort_session_by_peer(gobj, "SMTP server refuses this client at its greeting", code, reply);
        }
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
        if(code >= 500 && code < 600) {
            priv->refuse_code = code;   // like a 5xx greeting
            return abort_session_by_peer(gobj, "SMTP server refuses this client at EHLO", code, reply);
        }
        if(code != SMTP_CODE_OK) {
            return abort_session_by_peer(gobj, "EHLO rejected", code, reply);
        }
        const char *username = gobj_read_str_attr(gobj, "username");
        const char *password = gobj_read_str_attr(gobj, "password");
        if(empty_string(username) || empty_string(password)) {
            /* No credentials: skip AUTH, jump straight to ST_IDLE */
            return enter_idle_after_handshake(gobj);
        }
        char b64[LINE_BUFFER_MAX - sizeof("AUTH PLAIN ")];
        if(build_auth_plain(gobj, b64, sizeof(b64)) < 0) {
            return abort_session_on_error(gobj, "AUTH PLAIN response not built");
        }
        char auth_line[LINE_BUFFER_MAX];
        snprintf(auth_line, sizeof(auth_line), "AUTH PLAIN %s", b64);
        explicit_bzero(b64, sizeof(b64));

        priv->auth_continued = FALSE;
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
        if(code == SMTP_CODE_AUTH_CHALLENGE && !priv->auth_continued) {
            /*
             *  334: the server wants the response on a line of its own
             *  (RFC 4954 lets it ask). It is given once, the same base64
             *  credentials: still one login. A second 334 is a refusal.
             */
            priv->auth_continued = TRUE;
            char b64[LINE_BUFFER_MAX];
            if(build_auth_plain(gobj, b64, sizeof(b64)) < 0) {
                return abort_session_on_error(gobj, "AUTH PLAIN response not built");
            }
            set_timeout(priv->timer, priv->timeout_response);
            int ret = send_line(gobj, b64, TRUE);
            explicit_bzero(b64, sizeof(b64));
            return ret;
        }
        if(code == SMTP_CODE_AUTH_CHALLENGE) {
            /*
             *  A second 334: the server did not take the response either
             *  way. It repeats at every connection: reported like refused
             *  credentials (auth_rejected: 334), and the owner stops.
             */
            priv->auth_reject_code = code;
            return abort_session_by_peer(gobj, "AUTH PLAIN not taken after its continuation", code, reply);
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
            /*
             *  The sender is refused, and the sender is the same for every
             *  message (the account: `from`, its quota, its right to send):
             *  not this message's fault, and every message would meet it.
             *  A failure of the server, 4xx or 5xx: the session is dropped,
             *  the reconnection paced, the message waits with no retry
             *  spent, and a long one is the ERROR of timeout_failing_alarm.
             *  Before, it was charged to the message: a 5xx sent the whole
             *  queue to the failed queue, one message after another.
             */
            return abort_session_by_peer(gobj, "MAIL FROM rejected", code, reply);
        }
        return send_next_rcpt_or_data(gobj);
    }

    if(st == ST_WAIT_RCPT_TO_RESP) {
        if(code == SMTP_CODE_OK) {
            priv->rcpt_accepted++;
            return send_next_rcpt_or_data(gobj);
        }
        if(code >= 500 && code < 600) {
            /*
             *  This recipient is refused: the message goes to the others,
             *  as SMTP has it (RFC 5321 §3.3). Only when every one of them
             *  is refused is the message refused (send_next_rcpt_or_data).
             *  Up to 7.25.20 one bad address among several sent the message
             *  to nobody.
             */
            priv->rcpt_last_code = code;
            snprintf(priv->rcpt_last_reply, sizeof(priv->rcpt_last_reply), "%s", reply);
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL,
                "msg",          "%s", "RCPT TO rejected",
                "code",         "%d", code,
                "reply",        "%s", reply,
                NULL
            );
            return send_next_rcpt_or_data(gobj);
        }
        return reject_current_message(gobj, code, reply, "RCPT TO rejected");
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
         *  It is no failure: nothing connects again until the next message
         *  (request_connection), and the pacing starts afresh.
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
        if(code != SMTP_CODE_OK) {
            /*
             *  Refused like a refusal of MAIL FROM or RCPT TO, and resolved
             *  the same way: a 5xx answers the message and the session goes
             *  on; a 4xx drops the session, the reconnection is paced, and
             *  the code goes up on EV_ON_CLOSE. Up to 7.25.20 a 4xx here (451 4.7.1, a rate limit or a
             *  greylist) was answered on EV_ON_MESSAGE with the session up,
             *  and the emailsender uploaded the message again at once, every
             *  retry in the same second.
             */
            return reject_current_message(gobj, code, reply, "DATA body rejected by server");
        }
        /*
         *  `url`: the server it went to, the one of our transport. A url
         *  written to us while we run waits for our next start, so the
         *  owner's own url may already be another.
         */
        json_t *kw_ack = json_pack("{s:b, s:i, s:s}",
            "ok", 1,
            "code", code,
            "url", gobj_read_str_attr(gobj_bottom_gobj(gobj), "url")
        );
        /*
         *  Resolve BEFORE publishing: a subscriber reacting to EV_ON_MESSAGE
         *  may dispatch the next queued message straight away (the connection
         *  stays up), and it must find us idle with nothing in flight.
         */
        cleanup_current_message(gobj);
        priv->retry_delay = 0;  // the server works: a next failure is a first one
        note_delivery(gobj);
        gobj_change_state(gobj, ST_IDLE);
        gobj_publish_event(gobj, EV_ON_MESSAGE, kw_ack);
        return 0;
    }

    if(st == ST_WAIT_RSET_RESP) {
        if(code == SMTP_CODE_OK) {
            return enter_idle_after_reset(gobj);
        }
        if(code >= 500 && code < 600) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "SMTP server does not take RSET: the session ends, the next message opens another",
                "code",         "%d", code,
                "reply",        "%s", reply,
                NULL
            );
            return quit_session(gobj);
        }
        return abort_session_by_peer(gobj, "RSET not answered 250", code, reply);
    }

    if(st == ST_WAIT_QUIT_RESP) {
        gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
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
 *  No 221 to our QUIT: close anyway. Not a failure, the server had refused
 *  a message and RSET, nothing more.
 ***************************************************************************/
PRIVATE int ac_timeout_quit(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The close of the transport has ended (posted by ac_child_state_changed):
 *  connect, if a message still waits for it, when the pace allows it.
 ***************************************************************************/
PRIVATE int ac_connect_after_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->connect_posted = FALSE;
    if(priv->jn_current_msg && gobj_is_running(gobj)) {
        request_connection(gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Our timer in ST_DISCONNECTED: the watchdog of a connect (TCP and TLS
 *  took timeout_response: dropped, a failure, ac_child_state_changed), or
 *  the end of the paced delay with a message waiting.
 ***************************************************************************/
PRIVATE int ac_timeout_reconnect(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->connecting) {
        priv->connecting = FALSE;
        priv->connect_timed_out = TRUE;
        gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
        KW_DECREF(kw)
        return 0;
    }

    start_bottom(gobj);     // if a play found it closing
    if(priv->jn_current_msg) {
        request_connection(gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A state change of a child. Of the C_TCP, two say what it publishes no
 *  event for:
 *
 *  - A connection attempt that ended with no connection (refused, timed
 *    out, a TLS handshake that failed, our watchdog): a change out of
 *    ST_WAIT_CONNECTED or ST_WAIT_HANDSHAKE to a closing or closed state.
 *    As the C_TCP does not reconnect by itself, the failure is paced here,
 *    and said (note_failure), with the cause the C_TCP wrote in its
 *    `disconnect_cause` (not the last message of the log, which is any
 *    ERROR of the process).
 *  - Its close has ended (ST_DISCONNECTED): a message waiting is driven
 *    again, in the next cycle of the loop (EV_CONNECT_AFTER_CLOSE): paced
 *    if the close was a failure, at once if not.
 *
 *  Changes of our own stop of it (not running) are not failures.
 ***************************************************************************/
PRIVATE int ac_child_state_changed(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src != gobj_bottom_gobj(gobj) || !gobj_is_running(src) || priv->detached) {
        KW_DECREF(kw)
        return 0;
    }

    const char *prev = kw_get_str(gobj, kw, "previous_state", "", 0);
    const char *cur = kw_get_str(gobj, kw, "current_state", "", 0);
    BOOL was_connecting = strcmp(prev, ST_WAIT_CONNECTED) == 0 ||
        strcmp(prev, ST_WAIT_HANDSHAKE) == 0;
    BOOL is_down = strcmp(cur, ST_WAIT_STOPPED) == 0 || strcmp(cur, ST_STOPPED) == 0 ||
        strcmp(cur, ST_DISCONNECTED) == 0;

    if(was_connecting && is_down) {
        char cause[REPLY_TEXT_MAX];
        snprintf(cause, sizeof(cause), "cannot connect: %s",
            priv->connect_timed_out?
                "no connection, or no TLS handshake, within timeout_response" :
                gobj_read_str_attr(src, "disconnect_cause")
        );
        if(priv->connecting) {
            clear_timeout(priv->timer);
        }
        priv->connecting = FALSE;
        priv->connect_timed_out = FALSE;
        pace_next_connection(gobj, TRUE);
        note_failure(gobj, MSG_SERVER_FAILING, cause, TRUE, priv->jn_current_msg != NULL);
    }

    if(strcmp(cur, ST_DISCONNECTED) == 0 && priv->jn_current_msg &&
            gobj_current_state(gobj) == ST_DISCONNECTED && !priv->connect_posted) {
        if(gobj_post_event(gobj, EV_CONNECT_AFTER_CLOSE, json_object(), gobj) < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "gobj_post_event() FAILED: the message waits for the next one",
                NULL
            );
        } else {
            priv->connect_posted = TRUE;
        }
    }

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
 *
 *  A message taken is answered exactly once, by EV_ON_MESSAGE (a reply to
 *  its DATA, or a message that cannot be sent at all) or by EV_ON_CLOSE (the
 *  session ended with it in hand), and the return is 0. -1 means the message
 *  was NOT taken, and nothing will answer for it.
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->jn_current_msg) {
        /*
         *  Our owner's contract is broken: one message at a time. The
         *  message is refused by the return only, with no answer: the
         *  answers that will come are the ones of the message in hand.
         */
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "EV_SEND_MESSAGE while another message in flight",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    JSON_INCREF(kw)
    priv->jn_current_msg = kw;
    priv->recipient_index = 0;

    priv->jn_recipients = gather_recipients(gobj, kw);
    if(!priv->jn_recipients) {
        fail_current_message(gobj, "EV_SEND_MESSAGE has no valid recipients");
        KW_DECREF(kw)
        return 0;   // answered, see fail_current_message()
    }

    if(gobj_current_state(gobj) == ST_IDLE && !priv->detached && bottom_is_connected(gobj)) {
        begin_send_current_message(gobj);   // a failure is answered by EV_ON_MESSAGE or EV_ON_CLOSE
        KW_DECREF(kw)
        return 0;
    }
    /*
     *  Idle, but the transport is closing already (its inactivity close,
     *  or our drop after a 421 to the idle session): the message is stashed,
     *  and the close, which is no failure, gives it back to the owner, who
     *  sends it again for the next connection. Up to 7.25.20 its MAIL FROM
     *  went into the closing connection, and the close was counted as a
     *  failure of the server and a retry of the message.
     */

    /*
     *  Not ready to send yet. The message is stashed; enter_idle_after_handshake
     *  begins it when we reach ST_IDLE. If the bottom C_TCP is down (an idle
     *  close does not reconnect by itself), bring it back up -- reconnection
     *  is this session's job (it owns the transport and must redo the SMTP
     *  handshake), NOT the owner's -- when the pace allows it. If a
     *  connect/handshake is already in progress, just wait for it.
     */
    if(gobj_current_state(gobj) == ST_DISCONNECTED) {
        request_connection(gobj);
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
 *  Bottom child stopped. If we were started again while it was closing
 *  (a pause and a play in a row), it is started now; a message sent in
 *  the meantime waits for its connection.
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
        KW_DECREF(kw)
        return 0;
    }

    if(gobj_is_running(gobj) && src == gobj_bottom_gobj(gobj)) {
        start_bottom(gobj);
        if(priv->jn_current_msg) {
            request_connection(gobj);
        }
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
        {EV_STATE_CHANGED,      ac_child_state_changed, 0},
        {EV_STOPPED,            ac_stopped,             0},
        {EV_TIMEOUT,            ac_timeout_reconnect,   0},
        {EV_CONNECT_AFTER_CLOSE,ac_connect_after_close, 0},
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
        {EV_DISCONNECTED,       ac_disconnected,        0},
        {EV_STATE_CHANGED,      ac_child_state_changed, 0},
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
        {EV_DISCONNECTED,       ac_disconnected,        0},
        {EV_STATE_CHANGED,      ac_child_state_changed, 0},
        {0,0,0}
    };
    ev_action_t st_wait_ehlo_resp[] = {
        {EV_SEND_MESSAGE,       ac_send_message,        0},
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_TIMEOUT,            ac_timeout,             0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        0},
        {EV_STATE_CHANGED,      ac_child_state_changed, 0},
        {0,0,0}
    };
    ev_action_t st_wait_auth_resp[] = {
        {EV_SEND_MESSAGE,       ac_send_message,        0},
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_TIMEOUT,            ac_timeout,             0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        0},
        {EV_STATE_CHANGED,      ac_child_state_changed, 0},
        {0,0,0}
    };
    ev_action_t st_idle[] = {
        {EV_SEND_MESSAGE,       ac_send_message,        0},
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        0},
        {EV_STATE_CHANGED,      ac_child_state_changed, 0},
        {0,0,0}
    };
    ev_action_t st_wait_mail_from_resp[] = {
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_TIMEOUT,            ac_timeout,             0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        0},
        {EV_STATE_CHANGED,      ac_child_state_changed, 0},
        {0,0,0}
    };
    ev_action_t st_wait_rcpt_to_resp[] = {
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_TIMEOUT,            ac_timeout,             0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        0},
        {EV_STATE_CHANGED,      ac_child_state_changed, 0},
        {0,0,0}
    };
    ev_action_t st_wait_data_go[] = {
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_TIMEOUT,            ac_timeout,             0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        0},
        {EV_STATE_CHANGED,      ac_child_state_changed, 0},
        {0,0,0}
    };
    ev_action_t st_wait_data_resp[] = {
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_TIMEOUT,            ac_timeout,             0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        0},
        {EV_STATE_CHANGED,      ac_child_state_changed, 0},
        {0,0,0}
    };

    ev_action_t st_wait_rset_resp[] = {
        {EV_SEND_MESSAGE,       ac_send_message,        0},
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_TIMEOUT,            ac_timeout,             0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        0},
        {EV_STATE_CHANGED,      ac_child_state_changed, 0},
        {0,0,0}
    };
    ev_action_t st_wait_quit_resp[] = {
        {EV_SEND_MESSAGE,       ac_send_message,        0},
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_RX_LINE,            ac_rx_line,             0},
        {EV_TX_READY,           0,                      0},
        {EV_TIMEOUT,            ac_timeout_quit,        0},
        {EV_DROP,               ac_drop,                0},
        {EV_DISCONNECTED,       ac_disconnected,        0},
        {EV_STATE_CHANGED,      ac_child_state_changed, 0},
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
        {ST_WAIT_RSET_RESP,         st_wait_rset_resp},
        {ST_WAIT_QUIT_RESP,         st_wait_quit_resp},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_RX_DATA,            0},
        {EV_RX_LINE,            0},
        {EV_CONNECT_AFTER_CLOSE,0},
        {EV_TX_READY,           0},
        {EV_SEND_MESSAGE,       0},
        {EV_CONNECTED,          0},
        {EV_DISCONNECTED,       0},
        {EV_DROP,               0},
        {EV_STOPPED,            0},
        {EV_TIMEOUT,            0},
        /*
         *  Taken from the C_TCP (ac_child_state_changed) and published of
         *  our own, as every gobj does: so it keeps the flags of the system
         *  event, or our own publish of it is refused.
         */
        {EV_STATE_CHANGED,      EVF_SYSTEM_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
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
