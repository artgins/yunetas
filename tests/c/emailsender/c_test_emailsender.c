/***********************************************************************
 *          c_test_emailsender.c
 *
 *          Drives emailsender (or its SMTP session) against the fake SMTP
 *          server (C_FAKE_SMTP), one scenario per test:
 *
 *          "send"      send one email through the `emailsender` service,
 *                      at the play, or with `send_on_connect` when the
 *                      fake server says a client connected (the SMTP
 *                      session is in its handshake then). The fake server
 *                      ends the yuno when it is delivered.
 *          "set_user"  the `emailsender` service starts with no credentials
 *                      and a url where nobody listens: set-email-user gives
 *                      it the credentials AND the url of the fake server,
 *                      then one email is sent. The fake server ends the
 *                      yuno when it is delivered.
 *          "set_url"   the `emailsender` service starts WITH credentials
 *                      and a url where nobody listens, so its SMTP session
 *                      runs: set-url-from gives it the url of the fake
 *                      server, the service is paused and played again (the
 *                      answer of the command says the url waits for that),
 *                      then one email is sent. The fake server ends the
 *                      yuno when it is delivered. The fake server is NOT
 *                      the service's __input_side__ here (a pause stops
 *                      that one): the driver starts it.
 *          "session"   a C_SMTP_SESSION of our own (no emailsender) sends
 *                      one message; the fake server refuses the login. The
 *                      EV_ON_CLOSE of the session must say so: auth_rejected
 *                      with the code (`expect_auth_code`), and the reply
 *                      text (starting with `expect_reply`).
 *          "pause"     one email is sent; when the fake server says the
 *                      session connected (at its `act_on_connect_n`th
 *                      connection; it is in its handshake, the email in
 *                      flight), the `emailsender` service is paused and,
 *                      in the next cycle of the loop, played again. The
 *                      email must be delivered once, with no retry spent.
 *          "shutdown"  one email is sent; when the fake server says the
 *                      session connected, the yuno is told to die. The
 *                      email stays queued, with nothing said about it.
 *          "set_url_stash"  the `emailsender` service starts WITH
 *                      credentials and a dead url: the email sent at the
 *                      play waits in the SMTP session. `action_delay` ms
 *                      later set-url-from gives it the fake server, and the
 *                      service is paused and played. The email must be
 *                      delivered once, at the new url.
 *          "no_recipients"  an EV_SEND_EMAIL whose `to` holds no address
 *                      (",", no cc), then a good one: the first goes to the failed
 *                      queue once, the second is delivered.
 *          "refill"    one email at the play, a second `action_delay` ms
 *                      later, while the session waits to reconnect.
 *          "late_server"  one email at the play, with the fake server down;
 *                      the driver starts it `action_delay` ms later and
 *                      measures how long the session takes to connect: at
 *                      least `min_wait` ms, or its reconnection is not
 *                      paced.
 *          "url_log"   set-url-from gives the running service a dead url,
 *                      then one email is sent: the session goes on with the
 *                      url it runs on, and the log must say THAT one.
 *          "bad_burst" one good email, and behind it, while it is in
 *                      flight, `bad_count` emails with no recipient: when
 *                      the good one is delivered the emailsender takes them
 *                      one after another, each refused by the session at
 *                      once. `action_delay` ms after the start the queues
 *                      must hold nothing pending and `bad_count` failed
 *                      (list-queues), and the yuno ends.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <c_smtp_session.h>
#include "c_fake_smtp.h"
#include "c_test_emailsender.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TEST_FROM   "sender@example.com"
#define TEST_TO     "reader@example.com"
#define TEST_CC     "copy@example.com"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int send_one_email(hgobj gobj);
PRIVATE int send_email_to(hgobj gobj, const char *to, const char *cc);
PRIVATE int start_scenario(hgobj gobj);
PRIVATE int set_url_from(hgobj gobj, const char *url, BOOL expect_wait);
PRIVATE int check_queues(hgobj gobj, int expect_queued, int expect_failed);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
GOBJ_DEFINE_EVENT(EV_START_SCENARIO);
GOBJ_DEFINE_EVENT(EV_PLAY_EMAILSENDER);

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_STRING,      "scenario",         SDF_RD,             "send",     "send, set_user, set_url or session"),
SDATA (DTP_STRING,      "smtp_url",         SDF_RD,             "",         "url of the fake server (set_user, set_url, session)"),
SDATA (DTP_BOOLEAN,     "send_on_connect",  SDF_RD,             "0",        "scenario send: send when the fake server has a client"),
SDATA (DTP_STRING,      "server_service",   SDF_RD,             "__input_side__", "service of the fake server, started here in set_url, set_url_stash, late_server and session"),
SDATA (DTP_STRING,      "dead_url",         SDF_RD,             "",         "url where nobody listens (url_log)"),
SDATA (DTP_INTEGER,     "expect_auth_code", SDF_RD,             "535",      "scenario session: the code expected on EV_ON_CLOSE"),
SDATA (DTP_STRING,      "expect_close_key", SDF_RD,             "auth_rejected", "scenario session: the key of EV_ON_CLOSE that carries the code (auth_rejected, refused)"),
SDATA (DTP_STRING,      "expect_reply",     SDF_RD,             "535 5.7.8","scenario session: how the reply expected starts"),
SDATA (DTP_INTEGER,     "action_delay",     SDF_RD,             "0",        "ms to the second step of set_url_stash, refill and late_server"),
SDATA (DTP_INTEGER,     "min_wait",         SDF_RD,             "0",        "scenario late_server: ms the session must wait at least"),
SDATA (DTP_INTEGER,     "act_on_connect_n", SDF_RD,             "1",        "scenarios pause and shutdown: act at this connection of the fake server (1 = the first)"),
SDATA (DTP_INTEGER,     "bad_count",        SDF_RD,             "0",        "scenario bad_burst: emails with no recipient queued behind the good one"),
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
    BOOL sent_on_connect;
    BOOL acted_on_connect;  // scenarios "pause" and "shutdown": done once
    int connects_seen;      // EV_FAKE_CLIENT_CONNECTED received
    hgobj smtp;             // scenario "session": the session under test
    hgobj input_side;       // the fake server, when the driver starts it
    hgobj timer;            // the second step of a scenario
    uint64_t server_started;// scenario "late_server": when (msectimer)
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

    if(strcmp(gobj_read_str_attr(gobj, "scenario"), "session") == 0) {
        json_t *kw_smtp = json_pack("{s:s, s:I, s:s, s:s, s:s}",
            "url", gobj_read_str_attr(gobj, "smtp_url"),
            "timeout_inactivity", (json_int_t)30000,
            "username", "user",
            "password", "wrong",
            "helo_name", "test"
        );
        priv->smtp = gobj_create_pure_child("smtp", C_SMTP_SESSION, kw_smtp, gobj);
    }

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method play
 *
 *  The scenario starts in the next cycle of the loop: the services of the
 *  yuno are played one after the other, and the emailsender opens its
 *  queues in its own play.
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    gobj_post_event(gobj, EV_START_SCENARIO, 0, gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    if(priv->smtp && gobj_is_running(priv->smtp)) {
        gobj_stop(priv->smtp);
    }
    if(priv->input_side && gobj_is_running(priv->input_side)) {
        gobj_stop_tree(priv->input_side);
    }

    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Run the scenario
 ***************************************************************************/
PRIVATE int start_scenario(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *scenario = gobj_read_str_attr(gobj, "scenario");

    if(strcmp(scenario, "set_url") == 0 || strcmp(scenario, "session") == 0 ||
            strcmp(scenario, "set_url_stash") == 0 || strcmp(scenario, "pause") == 0 ||
            strcmp(scenario, "shutdown") == 0) {
        priv->input_side = gobj_find_service(gobj_read_str_attr(gobj, "server_service"), TRUE);
        gobj_start_tree(priv->input_side);
    }

    if(strcmp(scenario, "bad_burst") == 0) {
        set_timeout(priv->timer, gobj_read_integer_attr(gobj, "action_delay"));
        send_one_email(gobj);
        int bad_count = (int)gobj_read_integer_attr(gobj, "bad_count");
        for(int i = 0; i < bad_count; i++) {
            send_email_to(gobj, ",", "");
        }
        return 0;
    }

    if(strcmp(scenario, "late_server") == 0 || strcmp(scenario, "refill") == 0 ||
            strcmp(scenario, "set_url_stash") == 0) {
        if(strcmp(scenario, "late_server") == 0) {
            priv->input_side = gobj_find_service(gobj_read_str_attr(gobj, "server_service"), TRUE);
        }
        set_timeout(priv->timer, gobj_read_integer_attr(gobj, "action_delay"));
        return send_one_email(gobj);
    }

    if(strcmp(scenario, "no_recipients") == 0) {
        send_email_to(gobj, ",", "");
        return send_one_email(gobj);
    }

    if(strcmp(scenario, "url_log") == 0) {
        set_url_from(gobj, gobj_read_str_attr(gobj, "dead_url"), TRUE);
        return send_one_email(gobj);
    }

    if(strcmp(scenario, "session") == 0) {
        gobj_start(priv->smtp);

        json_t *kw_msg = json_pack("{s:s, s:s, s:s}",
            "from", TEST_FROM,
            "to", TEST_TO,
            "body", "Subject: test\r\n\r\nbody\r\n"
        );
        return gobj_send_event(priv->smtp, EV_SEND_MESSAGE, kw_msg, gobj);
    }

    if(strcmp(scenario, "set_url") == 0) {
        set_url_from(gobj, gobj_read_str_attr(gobj, "smtp_url"), TRUE);
        gobj_pause(gobj_find_service("emailsender", TRUE));
        return gobj_post_event(gobj, EV_PLAY_EMAILSENDER, 0, gobj);
    }

    if(strcmp(scenario, "set_user") == 0) {
        char command[PATH_MAX];
        snprintf(command, sizeof(command),
            "set-email-user username=user password=secret url=%s",
            gobj_read_str_attr(gobj, "smtp_url")
        );
        json_t *jn_resp = gobj_command(
            gobj_find_service("emailsender", TRUE), command, json_object(), gobj
        );
        int result = (int)kw_get_int(gobj, jn_resp, "result", -1, 0);
        JSON_DECREF(jn_resp)
        if(result < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "set-email-user refused",
                NULL
            );
            set_yuno_must_die();
            return -1;
        }
    }

    if(gobj_read_bool_attr(gobj, "send_on_connect")) {
        return 0;
    }
    return send_one_email(gobj);
}

/***************************************************************************
 *  set-url-from url=<url> to the emailsender service. With expect_wait
 *  the answer must say that the url waits for the next start.
 ***************************************************************************/
PRIVATE int set_url_from(hgobj gobj, const char *url, BOOL expect_wait)
{
    hgobj emailsender = gobj_find_service("emailsender", TRUE);
    char command[PATH_MAX];
    snprintf(command, sizeof(command), "set-url-from url=%s", url);
    json_t *jn_resp = gobj_command(emailsender, command, json_object(), gobj);
    const char *comment = kw_get_str(gobj, jn_resp, "comment", "", 0);
    BOOL waits = strstr(comment, "pause and play")? TRUE : FALSE;
    if(waits == expect_wait) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "set-url-from says the url waits for a play",
            NULL
        );
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "set-url-from does not say what is expected of the url",
            "comment",      "%s", comment,
            NULL
        );
    }
    JSON_DECREF(jn_resp)
    return waits? 0 : -1;
}

/***************************************************************************
 *  list-queues to the emailsender service: an INFO if the queues hold what
 *  is expected, an ERROR if not.
 ***************************************************************************/
PRIVATE int check_queues(hgobj gobj, int expect_queued, int expect_failed)
{
    json_t *jn_resp = gobj_command(
        gobj_find_service("emailsender", TRUE), "list-queues", json_object(), gobj
    );
    json_t *jn_data = kw_get_dict(gobj, jn_resp, "data", 0, 0);
    int queued = (int)json_array_size(kw_get_list(gobj, jn_data, "emails_queue", 0, 0));
    int failed = (int)json_array_size(kw_get_list(gobj, jn_data, "emails_failed", 0, 0));
    JSON_DECREF(jn_resp)

    if(queued == expect_queued && failed == expect_failed) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "The queues hold what is expected",
            NULL
        );
        return 0;
    }
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "The queues do NOT hold what is expected",
        "queued",       "%d", queued,
        "failed",       "%d", failed,
        "expect_queued","%d", expect_queued,
        "expect_failed","%d", expect_failed,
        NULL
    );
    return -1;
}

/***************************************************************************
 *  One email to the emailsender service
 ***************************************************************************/
PRIVATE int send_one_email(hgobj gobj)
{
    return send_email_to(gobj, TEST_TO, TEST_CC);
}

/***************************************************************************
 *  One email to `to` and `cc`, through the public EV_SEND_EMAIL
 ***************************************************************************/
PRIVATE int send_email_to(hgobj gobj, const char *to, const char *cc)
{
    json_t *kw_email = json_pack("{s:s, s:s, s:s, s:s, s:s, s:b}",
        "to", to,
        "cc", cc,
        "reply_to", "",
        "subject", "test",
        "body", "body of the test",
        "is_html", 0
    );
    return gobj_send_event(
        gobj_find_service("emailsender", TRUE), EV_SEND_EMAIL, kw_email, gobj
    );
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Every service plays: go
 ***************************************************************************/
PRIVATE int ac_start_scenario(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    start_scenario(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Scenario "send" with send_on_connect: the fake server has a client,
 *  send now (the first time only)
 ***************************************************************************/
PRIVATE int ac_fake_client_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *scenario = gobj_read_str_attr(gobj, "scenario");
    priv->connects_seen++;
    BOOL its_turn = priv->connects_seen >= (int)gobj_read_integer_attr(gobj, "act_on_connect_n");

    if(gobj_read_bool_attr(gobj, "send_on_connect") && !priv->sent_on_connect) {
        priv->sent_on_connect = TRUE;
        send_one_email(gobj);
    }

    if(strcmp(scenario, "pause") == 0 && !priv->acted_on_connect && its_turn) {
        /*
         *  The session is in its handshake, the email in flight: pause, and
         *  play in the next cycle, while its C_TCP is still closing.
         */
        priv->acted_on_connect = TRUE;
        gobj_pause(gobj_find_service("emailsender", TRUE));
        gobj_post_event(gobj, EV_PLAY_EMAILSENDER, 0, gobj);
    }

    if(strcmp(scenario, "shutdown") == 0 && !priv->acted_on_connect && its_turn) {
        priv->acted_on_connect = TRUE;
        set_yuno_must_die();
    }

    if(strcmp(scenario, "late_server") == 0 && !priv->acted_on_connect) {
        priv->acted_on_connect = TRUE;
        uint64_t waited = time_in_milliseconds_monotonic() - priv->server_started;
        json_int_t min_wait = gobj_read_integer_attr(gobj, "min_wait");
        if((json_int_t)waited >= min_wait) {
            gobj_log_info(gobj, 0,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "The session waited its paced time to connect",
                NULL
            );
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The session connected before its paced time",
                "waited",       "%ld", (long)waited,
                "min_wait",     "%ld", (long)min_wait,
                NULL
            );
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The second step of a scenario
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *scenario = gobj_read_str_attr(gobj, "scenario");

    if(strcmp(scenario, "refill") == 0) {
        send_one_email(gobj);

    } else if(strcmp(scenario, "bad_burst") == 0) {
        check_queues(gobj, 0, (int)gobj_read_integer_attr(gobj, "bad_count"));
        set_yuno_must_die();

    } else if(strcmp(scenario, "late_server") == 0) {
        priv->server_started = time_in_milliseconds_monotonic();
        gobj_start_tree(priv->input_side);

    } else if(strcmp(scenario, "set_url_stash") == 0) {
        set_url_from(gobj, gobj_read_str_attr(gobj, "smtp_url"), TRUE);
        gobj_pause(gobj_find_service("emailsender", TRUE));
        gobj_post_event(gobj, EV_PLAY_EMAILSENDER, 0, gobj);

    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "EV_TIMEOUT in a scenario without a second step",
            "scenario",     "%s", scenario,
            NULL
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The service was paused, play it (and send, in scenario "set_url")
 ***************************************************************************/
PRIVATE int ac_play_emailsender(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_play(gobj_find_service("emailsender", TRUE));
    if(strcmp(gobj_read_str_attr(gobj, "scenario"), "set_url") == 0) {
        send_one_email(gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Scenario "session": the session is up (it must not be: bad password)
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "The session opened with a refused login",
        NULL
    );

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Scenario "session": the session is down, and says why
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    int auth_rejected = (int)kw_get_int(gobj, kw, gobj_read_str_attr(gobj, "expect_close_key"), 0, 0);
    const char *reply = kw_get_str(gobj, kw, "reply", "", 0);
    int expect_code = (int)gobj_read_integer_attr(gobj, "expect_auth_code");
    const char *expect_reply = gobj_read_str_attr(gobj, "expect_reply");

    if(auth_rejected == expect_code &&
            strncmp(reply, expect_reply, strlen(expect_reply)) == 0) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Refused login reported with its reply",
            NULL
        );
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Refused login NOT reported with its reply",
            "auth_rejected","%d", auth_rejected,
            "reply",        "%s", reply,
            NULL
        );
    }
    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Scenario "session": a message went out (it must not: bad password)
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "A message was sent with a refused login",
        NULL
    );

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
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_EMAILSENDER);

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
        {EV_START_SCENARIO,         ac_start_scenario,          0},
        {EV_PLAY_EMAILSENDER,       ac_play_emailsender,        0},
        {EV_FAKE_CLIENT_CONNECTED,  ac_fake_client_connected,   0},
        {EV_ON_OPEN,                ac_on_open,                 0},
        {EV_ON_CLOSE,               ac_on_close,                0},
        {EV_ON_MESSAGE,             ac_on_message,              0},
        {EV_TIMEOUT,                ac_timeout,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_START_SCENARIO, 0},
        {EV_PLAY_EMAILSENDER, 0},
        {EV_FAKE_CLIENT_CONNECTED, 0},
        {EV_ON_OPEN,        0},
        {EV_ON_CLOSE,       0},
        {EV_ON_MESSAGE,     0},
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
PUBLIC int register_c_test_emailsender(void)
{
    return create_gclass(C_TEST_EMAILSENDER);
}
