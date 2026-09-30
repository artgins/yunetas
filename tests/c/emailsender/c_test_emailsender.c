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
 *                      with the code, and the reply text.
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

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int send_one_email(hgobj gobj);
PRIVATE int start_scenario(hgobj gobj);

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
SDATA (DTP_STRING,      "server_service",   SDF_RD,             "__input_side__", "service of the fake server, started here in set_url and session"),
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
    hgobj smtp;             // scenario "session": the session under test
    hgobj input_side;       // scenarios "set_url" and "session": the fake server
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

    if(priv->smtp && gobj_is_running(priv->smtp)) {
        gobj_stop(priv->smtp);
    }
    if(priv->input_side) {
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

    if(strcmp(scenario, "set_url") == 0 || strcmp(scenario, "session") == 0) {
        priv->input_side = gobj_find_service(gobj_read_str_attr(gobj, "server_service"), TRUE);
        gobj_start_tree(priv->input_side);
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
        hgobj emailsender = gobj_find_service("emailsender", TRUE);
        char command[PATH_MAX];
        snprintf(command, sizeof(command),
            "set-url-from url=%s",
            gobj_read_str_attr(gobj, "smtp_url")
        );
        json_t *jn_resp = gobj_command(emailsender, command, json_object(), gobj);
        const char *comment = kw_get_str(gobj, jn_resp, "comment", "", 0);
        if(strstr(comment, "pause and play")) {
            gobj_log_info(gobj, 0,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "set-url-from says the url waits for a play",
                NULL
            );
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "set-url-from does not say the url waits",
                "comment",      "%s", comment,
                NULL
            );
        }
        JSON_DECREF(jn_resp)

        gobj_pause(emailsender);
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
 *  One email to the emailsender service
 ***************************************************************************/
PRIVATE int send_one_email(hgobj gobj)
{
    json_t *kw_email = json_pack("{s:s, s:s, s:s, s:s, s:b}",
        "to", TEST_TO,
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

    if(gobj_read_bool_attr(gobj, "send_on_connect") && !priv->sent_on_connect) {
        priv->sent_on_connect = TRUE;
        send_one_email(gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Scenario "set_url": the service was paused, play it and send
 ***************************************************************************/
PRIVATE int ac_play_emailsender(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_play(gobj_find_service("emailsender", TRUE));
    send_one_email(gobj);

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
    int auth_rejected = (int)kw_get_int(gobj, kw, "auth_rejected", 0, 0);
    const char *reply = kw_get_str(gobj, kw, "reply", "", 0);

    if(auth_rejected == 535 && strncmp(reply, "535 5.7.8", 9) == 0) {
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
