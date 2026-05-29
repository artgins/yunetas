/***********************************************************************
 *          C_EMAILSENDER.C
 *          Emailsender GClass.
 *
 *          Email sender
 *
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

#include "c_smtp_session.h"
#include "mime_encoder.h"
#include "c_emailsender.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
/*
 *  Reconnection backoff bounds (milliseconds). The SMTP link is brought up
 *  on demand when there is mail to send; if the connect or handshake fails
 *  (server down, bad credentials, ...) we retry on a timer with exponential
 *  backoff between RECONNECT_BACKOFF_MIN_MS and RECONNECT_BACKOFF_MAX_MS,
 *  reset to MIN on a successful open. This bounds the retry rate (no hammer)
 *  and drains the queue automatically when the server/config recovers.
 */
#define RECONNECT_BACKOFF_MIN_MS    2000
#define RECONNECT_BACKOFF_MAX_MS    300000

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int open_queues(hgobj gobj);
PRIVATE int close_queues(hgobj gobj);
PRIVATE int process_smtp_response(
    hgobj gobj,
    q_msg_t *msg,
    int result,
    BOOL permanent,
    const char *to
);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_email_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_url_and_from(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_send_email(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_enable_alarm_emails(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_disable_alarm_emails(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_queues(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_remove_emails_failed(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_send_email[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "to",           0,              0,          "To field."),
SDATAPM (DTP_STRING,    "reply-to",     0,              0,          "Reply-To field."),
SDATAPM (DTP_STRING,    "subject",      0,              0,          "Subject field."),
SDATAPM (DTP_STRING,    "attachment",   0,              0,          "Attachment file."),
SDATAPM (DTP_STRING,    "inline_file_id",0,             0,          "Inline file ID (must be attachment too)."),
SDATAPM (DTP_BOOLEAN,   "is_html",      0,              0,          "Is html"),
SDATAPM (DTP_STRING,    "body",         0,              0,          "Email body."),
SDATA_END()
};

PRIVATE sdata_desc_t pm_set_email_user[] = {
/*-PM----type-----------name------------flag----default-----description---------- */
SDATAPM (DTP_STRING,    "username",     0,      0,          "Username"),
SDATAPM (DTP_STRING,    "password",     0,      0,          "Password"),
SDATAPM (DTP_STRING,    "url",          0,      0,          "SMTP url"),
SDATAPM (DTP_STRING,    "from",         0,      0,          "Default from"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_set_url_from[] = {
/*-PM----type-----------name------------flag----default-----description---------- */
SDATAPM (DTP_STRING,    "url",          0,      0,          "SMTP url"),
SDATAPM (DTP_STRING,    "from",         0,      0,          "Default from"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_list_queues[] = {
/*-PM----type-----------name------------flag----default-----description---------- */
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help, pm_help,        cmd_help,       "Command's help"),
SDATACM (DTP_SCHEMA,    "send-email",       0,      pm_send_email,  cmd_send_email, "Send email."),
SDATACM (DTP_SCHEMA,    "disable-alarm-emails",0,   0,              cmd_disable_alarm_emails, "Disable send alarm emails."),
SDATACM (DTP_SCHEMA,    "enable-alarm-emails",0,    0,              cmd_enable_alarm_emails, "Enable send alarm emails."),
SDATACM (DTP_SCHEMA,    "list-queues",      0,      pm_list_queues, cmd_list_queues, "List email queues"),
SDATACM (DTP_SCHEMA,    "remove-emails-failed",0,   0,              cmd_remove_emails_failed, "Remove emails failed"),

/*-CMD2---type------name------------flag------------ali-items---------------json_fn-------------description--*/
SDATACM2(DTP_SCHEMA,"set-email-user",SDF_AUTHZ_X,   0,  pm_set_email_user,  cmd_set_email_user, "Set email user"),
SDATACM2(DTP_SCHEMA,"set-url-from", SDF_AUTHZ_X,    0,  pm_set_url_from,    cmd_set_url_and_from, "Set url and/or from"),

SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name--------------------flag--------------------default-----description---------- */
SDATA (DTP_STRING,      "username",             SDF_PERSIST|SDF_REQUIRED,"",    "email username"),
SDATA (DTP_STRING,      "password",             SDF_PERSIST|SDF_REQUIRED,"",    "email password"),
SDATA (DTP_STRING,      "url",                  SDF_PERSIST|SDF_REQUIRED,"",    "smtp URL"),
SDATA (DTP_STRING,      "from",                 SDF_PERSIST|SDF_REQUIRED,"",    "default from"),
SDATA (DTP_STRING,      "from_beautiful",       SDF_PERSIST,            "",     "from with name"),
SDATA (DTP_INTEGER,     "max_retries",          SDF_PERSIST|SDF_WR,     "4",    "Maximum retries to send email"),
SDATA (DTP_INTEGER,     "timeout_inactivity",   SDF_PERSIST,            "30000", "Inactivity timeout in milliseconds to close the connection. Reconnect when new data arrived. With -1 never close."),
SDATA (DTP_BOOLEAN,     "only_test",            SDF_PERSIST|SDF_WR,     0,      "True when testing, send only to test_email"),
SDATA (DTP_BOOLEAN,     "add_test",             SDF_PERSIST|SDF_WR,     0,      "True when testing, add test_email to send"),
SDATA (DTP_STRING,      "test_email",           SDF_PERSIST|SDF_WR,     "",     "test email"),

SDATA (DTP_INTEGER,     "send",                 SDF_STATS,              0,      "Emails send"),
SDATA (DTP_INTEGER,     "sent",                 SDF_STATS,              0,      "Emails sent"),
SDATA (DTP_BOOLEAN,     "disable_alarm_emails", SDF_PERSIST|SDF_WR,     0,      "True to don't send alarm emails"),

SDATA (DTP_STRING,      "tranger_path",         SDF_RD,                 "",     "tranger path"),
SDATA (DTP_STRING,      "tranger_database",     SDF_RD,                 "",     "tranger database"),
SDATA (DTP_STRING,      "topic_emails_queue",   SDF_RD,                 "",     "queue topic for emails"),
SDATA (DTP_STRING,      "topic_emails_failed",   SDF_RD,                "",     "queue topic for emails failing"),
SDATA (DTP_STRING,      "tkey",                 SDF_RD,                 "",     "trq_open tkey"),
SDATA (DTP_STRING,      "system_flag",          SDF_RD,                 "",     "trq_open system_flag"),
SDATA (DTP_INTEGER,     "on_critical_error",    SDF_RD,                 "0",    "LOG_OPT_TRACE_STACK"),
SDATA (DTP_INTEGER,     "backup_queue_size",SDF_WR|SDF_PERSIST, "1000000",  "Do backup at this size"),

SDATA (DTP_POINTER,     "user_data",            0,                      0,      "user data"),
SDATA (DTP_POINTER,     "user_data2",           0,                      0,      "more user data"),
SDATA (DTP_POINTER,     "subscriber",           0,                      0,      "subscriber of output-events. If it's null then subscriber is the parent."),

SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};


/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj smtp;                 /* C_SMTP_SESSION pure_child (owns its C_TCP) */
    hgobj timer;                /* C_TIMER pure_child: reconnection backoff / queue-drain retry */
    hgobj gobj_input_side;
    json_int_t max_retries;
    q_msg_t *qmsg_cur_email;
    json_int_t cur_retries;     /* failed attempts for the current head message */
    json_int_t reconnect_backoff; /* current reconnect backoff (ms); grows on failure, resets on open */
    BOOL smtp_ready;            /* TRUE between EV_ON_OPEN and EV_ON_CLOSE (child connected+authed) */

    json_int_t send;
    json_int_t sent;

    const char *username;
    const char *password;
    const char *url;
    const char *from;

    hgobj gobj_tranger_queues;
    json_t *tranger;
    tr_queue_t *trq_emails_queue;
    tr_queue_t *trq_emails_failed;
    int32_t alert_queue_size;
    q_msg_t *last_msg_sent;

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

    /*
     *  Build the SMTP session child. It auto-creates its own C_TCP bottom
     *  on mt_start using the url attr. EHLO domain defaults to the local
     *  hostname so the receiving server logs an identifiable peer.
     */
    char hostname[HOST_NAME_MAX + 1];
    if(gethostname(hostname, sizeof(hostname)) != 0) {
        snprintf(hostname, sizeof(hostname), "localhost");
    }
    hostname[sizeof(hostname) - 1] = '\0';

    json_t *kw_smtp = json_pack("{s:s, s:I, s:s, s:s, s:s}",
        "url", gobj_read_str_attr(gobj, "url"),
        "timeout_inactivity", gobj_read_integer_attr(gobj, "timeout_inactivity"),
        "username", gobj_read_str_attr(gobj, "username"),
        "password", gobj_read_str_attr(gobj, "password"),
        "helo_name", hostname
    );
    priv->smtp = gobj_create_pure_child(
        gobj_name(gobj),
        C_SMTP_SESSION,
        kw_smtp,
        gobj
    );

    /*
     *  Timer for the reconnection backoff / queue-drain retry.
     */
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    priv->reconnect_backoff = RECONNECT_BACKOFF_MIN_MS;

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    } else if(gobj_is_pure_child(gobj)) {
        subscriber = gobj_parent(gobj);
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(username,              gobj_read_str_attr)
    SET_PRIV(password,              gobj_read_str_attr)
    SET_PRIV(url,                   gobj_read_str_attr)
    SET_PRIV(from,                  gobj_read_str_attr)
    SET_PRIV(max_retries,           gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(max_retries,         gobj_read_integer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    // PRIVATE_DATA *priv = gobj_priv_data(gobj);
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
 *  Yuneta rule:
 *  If service has mt_play then start only the service gobj.
 *      (Let mt_play be responsible to start their tree)
 *  If service has not mt_play then start the tree with gobj_start_tree().
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*--------------------------------*
     *      Open queues
     *--------------------------------*/
    open_queues(gobj);

    /*--------------------------------*
     *      Start smtp
     *--------------------------------*/
    gobj_start(priv->smtp);

    /*
     *  Start services
     */

    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    // gobj_subscribe_event(priv->gobj_input_side, 0, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*--------------------------------*
     *      Stop smtp and timer
     *--------------------------------*/
    clear_timeout(priv->timer);
    gobj_stop(priv->smtp);

    /*--------------------------------*
     *      Close queues
     *--------------------------------*/
    close_queues(gobj);

    /*
     *  Stop services
     */
    EXEC_AND_RESET(gobj_stop_tree, priv->gobj_input_side);

    return 0;
}

/***************************************************************************
 *      Framework Method reading
 ***************************************************************************/
PRIVATE SData_Value_t mt_reading(hgobj gobj, const char *name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    SData_Value_t v = {0,{0}};
    if(strcmp(name, "send")==0) {
        v.found = 1;
        v.v.i = priv->send;
    } else if(strcmp(name, "sent")==0) {
        v.found = 1;
        v.v.i = priv->sent;
    }

    return v;
}




                    /***************************
                     *      Commands
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw);
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_set_email_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    /*--------------------------*
     *      Get parameters
     *--------------------------*/
    const char *username = kw_get_str(gobj, kw, "username", "", 0);
    if(empty_string(username)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What username?"),
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------*
     *      Password
     *-----------------------------*/
    const char *password = kw_get_str(gobj, kw, "password", "", 0);
    if(empty_string(password)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What password?"),
            0,
            0,
            kw  // owned
        );
    }

    gobj_write_str_attr(gobj, "username", username);
    gobj_write_str_attr(gobj, "password", password);
    gobj_save_persistent_attrs(gobj, json_pack("[s,s]", "username", "password"));

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Email username set: %s", username),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_set_url_and_from(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    /*--------------------------*
     *      Get parameters
     *--------------------------*/
    const char *url = kw_get_str(gobj, kw, "url", "", 0);
    const char *from = kw_get_str(gobj, kw, "from", "", 0);

    if(empty_string(url) && empty_string(from)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What url or from?"),
            0,
            0,
            kw  // owned
        );
    }

    if(!empty_string(url)) {
        gobj_write_str_attr(gobj, "url", url);
        gobj_save_persistent_attrs(gobj, json_string("url"));
    }
    if(!empty_string(from)) {
        gobj_write_str_attr(gobj, "from", from);
        gobj_save_persistent_attrs(gobj, json_string("from"));
    }

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("URL/from set: %s/%s", url, from),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_send_email(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *to = kw_get_str(gobj, kw, "to", 0, 0);
    const char *reply_to = kw_get_str(gobj, kw, "reply-to", 0, 0);
    const char *subject = kw_get_str(gobj, kw, "subject", "", 0);
    const char *attachment = kw_get_str(gobj, kw, "attachment", 0, 0);
    const char *inline_file_id = kw_get_str(gobj, kw, "inline_file_id", 0, 0);
    const char *body = kw_get_str(gobj, kw, "body", "", 0);

    /*
     *  is_html arrives from the agent's command parser as a JSON integer
     *  (the parser does not coerce DTP_BOOLEAN -> json bool, see memory
     *  feedback_command_parser_via_agent_quirks). KW_WILD_NUMBER lets
     *  kw_get_bool accept int/real/string/bool without erroring.
     */
    BOOL is_html = kw_get_bool(gobj, kw, "is_html", 0, KW_WILD_NUMBER);

    if(empty_string(to)) {
        return msg_iev_build_response(
            gobj,
            -200,
            json_sprintf(
                "Field 'to' is empty."
            ),
            0,
            0,
            kw  // owned
        );

    }
    /*
     *  Body travels as a plain string so it is persisted with the queued
     *  message (and survives retries / yuno restarts). ac_enqueue_message
     *  also accepts a transient "gbuffer" pointer from other senders and
     *  normalises it to this same "body" string.
     */
    json_t *kw_send = json_pack("{s:s, s:s, s:s, s:s, s:b}",
        "to", to,
        "reply_to", reply_to?reply_to:"",
        "subject", subject,
        "body", body,
        "is_html", is_html
    );
    if(attachment && access(attachment, 0)==0) {
        json_object_set_new(kw_send, "attachment", json_string(attachment));
        if(!empty_string(inline_file_id)) {
            json_object_set_new(kw_send, "inline_file_id", json_string(inline_file_id));
        }

    }
    gobj_send_event(gobj, EV_SEND_EMAIL, kw_send, gobj);


    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf(
            "Email enqueue to '%s'.",
            to
        ),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_disable_alarm_emails(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    gobj_write_bool_attr(gobj, "disable_alarm_emails", TRUE);
    gobj_save_persistent_attrs(gobj, 0);

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Alarm emails disabled"),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_queues(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int total_queues = 0;
    int total_fails = 0;

    json_t *jn_data = json_object();

    if(!gobj_is_playing(gobj)) {
        // In pause the queues are closed
        open_queues(gobj);
    }

    if(priv->trq_emails_queue) { // 0 if not running
        json_t *q = json_array();
        json_object_set_new(jn_data, "emails_queue", q);

        q_msg_t *qmsg;
        qmsg_foreach_forward(priv->trq_emails_queue, qmsg) {
            json_t *msg = trq_msg_json(qmsg);
            json_array_append_new(q, msg);
            total_queues++;
        }
    }

    if(priv->trq_emails_failed) { // 0 if not running
        json_t *q = json_array();
        json_object_set_new(jn_data, "emails_failed", q);

        q_msg_t *qmsg;
        qmsg_foreach_forward(priv->trq_emails_failed, qmsg) {
            json_t *msg = trq_msg_json(qmsg);
            json_array_append_new(q, msg);
            total_fails++;
        }
    }

    if(!gobj_is_playing(gobj)) {
        // In pause the queues are closed
        close_queues(gobj);
    }

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Email Queues: in queue %d, failed %d", total_queues, total_fails),
        0,
        jn_data,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_remove_emails_failed(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int total_fails = 0;

    if(!gobj_is_playing(gobj)) {
        // In pause the queues are closed
        open_queues(gobj);
    }

    if(priv->trq_emails_failed) { // 0 if not running
        q_msg_t *qmsg, *prev;
        qmsg_foreach_forward_safe(priv->trq_emails_failed, qmsg, prev) {
            trq_unload_msg(qmsg, 0);
            total_fails++;
        }
    }

    if(!gobj_is_playing(gobj)) {
        // In pause the queues are closed
        close_queues(gobj);
    }

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Deleted Emails failed: %d", total_fails),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_enable_alarm_emails(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    gobj_write_bool_attr(gobj, "disable_alarm_emails", FALSE);
    gobj_save_persistent_attrs(gobj, 0);

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Alarm emails enabled"),
        0,
        0,
        kw  // owned
    );
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int open_queues(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->tranger) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "tranger NOT NULL",
            NULL
        );
        tranger2_shutdown(priv->tranger);
    }

    const char *path = gobj_read_str_attr(gobj, "tranger_path");
    if(empty_string(path)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "tranger path EMPTY",
            NULL
        );
        return -1;
    }

    if(!is_directory(path)) {
        mkrdir(path, yuneta_xpermission());
    }

    const char *database = gobj_read_str_attr(gobj, "tranger_database");
    if(empty_string(database)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "tranger database EMPTY",
            NULL
        );
        return -1;
    }

    const char *topic_emails_queue = gobj_read_str_attr(gobj, "topic_emails_queue");
    if(empty_string(topic_emails_queue)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "tranger topic_emails_queue EMPTY",
            NULL
        );
        return -1;
    }

    const char *topic_emails_failed = gobj_read_str_attr(gobj, "topic_emails_failed");
    if(empty_string(topic_emails_failed)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "tranger topic_emails_failed EMPTY",
            NULL
        );
        return -1;
    }

    /*---------------------------------*
     *      Open Timeranger queues
     *---------------------------------*/
    json_t *kw_tranger = json_pack("{s:s, s:s, s:b, s:I, s:i}",
        "path", path,
        "database", database,
        "master", 1,
        "subscriber", (json_int_t)(uintptr_t)gobj,
        "on_critical_error", (int)gobj_read_integer_attr(gobj, "on_critical_error")
    );
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "gobj_tranger_%s", gobj_name(gobj));
    priv->gobj_tranger_queues = gobj_create_service(
        name,
        C_TRANGER,
        kw_tranger,
        gobj
    );
    gobj_start(priv->gobj_tranger_queues);
    priv->tranger = gobj_read_pointer_attr(priv->gobj_tranger_queues, "tranger");

    priv->trq_emails_queue = trq_open(
        priv->tranger,
        topic_emails_queue,
        gobj_read_str_attr(gobj, "tkey"),
        tranger2_str2system_flag(gobj_read_str_attr(gobj, "system_flag")),
        gobj_read_integer_attr(gobj, "backup_queue_size")
    );

    priv->trq_emails_failed = trq_open(
        priv->tranger,
        topic_emails_failed,
        gobj_read_str_attr(gobj, "tkey"),
        tranger2_str2system_flag(gobj_read_str_attr(gobj, "system_flag")),
        gobj_read_integer_attr(gobj, "backup_queue_size")
    );

    trq_load(priv->trq_emails_queue);
    trq_load(priv->trq_emails_failed);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int close_queues(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    EXEC_AND_RESET(trq_close, priv->trq_emails_queue);
    EXEC_AND_RESET(trq_close, priv->trq_emails_failed);

    /*----------------------------------*
     *      Close Timeranger queues
     *----------------------------------*/
    gobj_stop(priv->gobj_tranger_queues);
    EXEC_AND_RESET(gobj_destroy, priv->gobj_tranger_queues);
    priv->tranger = 0;

    return 0;
}

/***************************************************************************
 *  Enqueue a new message
 ***************************************************************************/
PRIVATE q_msg_t *enqueue_message(
    hgobj gobj,
    json_t *kw  // not owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(!priv->trq_emails_queue) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "trq_msgs NULL",
            NULL
        );
        return 0;
    }

    q_msg_t *msg = trq_append(
        priv->trq_emails_queue,
        kw_incref(kw)
    );

    if(!msg) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Message NOT SAVED in the emails queue",
            NULL
        );
        return 0;
    }

    return msg;
}

/***************************************************************************
 *  Enqueue failing message
 ***************************************************************************/
PRIVATE q_msg_t *enqueue_failing_message(
    hgobj gobj,
    q_msg_t *msg
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->trq_emails_failed) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "trq_msgs NULL",
            NULL
        );
        return 0;
    }

    json_t *kw = trq_msg_json(msg);
    msg = trq_append(
        priv->trq_emails_failed,
        kw
    );

    return msg;
}

/***************************************************************************
 *  Dequeue a message
 *  Build the MIME message and dispatch it to the SMTP child.
 *  Async: the response (success / failure) comes back via EV_ON_MESSAGE
 *  handled by ac_on_message.
 ***************************************************************************/
PRIVATE int tira_dela_cola(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->qmsg_cur_email) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A pending current sending email",
            NULL
        );
    }

    /*
     *  Only dispatch when the SMTP session is connected and authenticated
     *  (between EV_ON_OPEN and EV_ON_CLOSE). Otherwise leave the message in
     *  the persistent queue and wait: C_TCP reconnects on its own and
     *  ac_on_open kicks the dequeue again. This avoids dead-lettering
     *  perfectly good messages just because the link is momentarily down
     *  (server restart, transient network, or simply still handshaking at
     *  startup with a backlog loaded from disk).
     */
    if(!priv->smtp_ready) {
        /*
         *  Session not up. The bottom C_TCP closes on inactivity
         *  (timeout_inactivity) and does NOT auto-reconnect. Act only if mail
         *  is actually waiting: kick an on-demand (re)connect when the SMTP
         *  child is idle-closed (ST_DISCONNECTED) and arm the backoff timer so
         *  a failed connect/handshake is retried later (ac_retry) instead of
         *  stalling. EV_ON_OPEN then re-enters this dequeue; if a connect/
         *  handshake is already in progress we just wait for it.
         */
        if(priv->trq_emails_queue && trq_first_msg(priv->trq_emails_queue)) {
            if(gobj_current_state(priv->smtp) == ST_DISCONNECTED) {
                gobj_send_event(priv->smtp, EV_CONNECT, 0, gobj);
            }
            set_timeout(priv->timer, priv->reconnect_backoff);
        }
        return 0;
    }

    /*
     *  Dequeue the message (head of queue; it stays there until unloaded,
     *  so a retry sees the same message again).
     */
    priv->qmsg_cur_email = trq_first_msg(priv->trq_emails_queue);
    if(!priv->qmsg_cur_email) {
        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj, "No more messages");
        }
        return 0;
    }

    q_msg_t *qmsg_for_fail = priv->qmsg_cur_email;
    json_t *msg = trq_msg_json(priv->qmsg_cur_email);

    const char *from = kw_get_str(gobj, msg, "from", 0, 0);
    if(empty_string(from)) {
        from = priv->from;
    }
    const char *from_beautiful = gobj_read_str_attr(gobj, "from_beautiful");
    const char *to = kw_get_str(gobj, msg, "to", 0, 0);
    const char *cc = kw_get_str(gobj, msg, "cc", "", 0);
    const char *bcc = kw_get_str(gobj, msg, "bcc", "", 0);
    const char *reply_to = kw_get_str(gobj, msg, "reply_to", "", 0);
    const char *subject = kw_get_str(gobj, msg, "subject", "", 0);
    const char *attachment = kw_get_str(gobj, msg, "attachment", "", 0);
    if(empty_string(attachment)) {
        /* Legacy alias from c_curl era. */
        attachment = kw_get_str(gobj, msg, "attachments", "", 0);
    }
    const char *inline_file_id = kw_get_str(gobj, msg, "inline_file_id", 0, 0);
    BOOL is_html = kw_get_bool(gobj, msg, "is_html", 0, 0);

    /*--------------------------------*
     *      MIME encode the message
     *--------------------------------*/
    /*
     *  Body is persisted either as a UTF-8 "body" string or, when it carried
     *  non-UTF-8 bytes at enqueue time, as base64 under "body_base64". Decode
     *  whichever is present into a transient gbuffer for the MIME encoder
     *  (which does not take ownership). Freed right after build.
     */
    gbuffer_t *gbuf_body = NULL;
    const char *body_base64 = kw_get_str(gobj, msg, "body_base64", "", 0);
    if(!empty_string(body_base64)) {
        gbuf_body = gbuffer_base64_to_binary(body_base64, strlen(body_base64));
    } else {
        const char *body = kw_get_str(gobj, msg, "body", "", 0);
        size_t body_src_len = strlen(body);
        if(body_src_len > 0) {
            gbuf_body = gbuffer_create(body_src_len, body_src_len);
            gbuffer_append(gbuf_body, (void *)body, body_src_len);
        }
    }
    mime_email_t m = {
        .from = from,
        .from_beautiful = from_beautiful,
        .to = to,
        .cc = cc,
        .reply_to = reply_to,
        .subject = subject,
        .is_html = is_html,
        .body = gbuf_body,
        .attachment = empty_string(attachment) ? NULL : attachment,
        .inline_file_id = empty_string(inline_file_id) ? NULL : inline_file_id,
    };
    gbuffer_t *mime_body = mime_build_message(gobj, &m);
    GBUFFER_DECREF(gbuf_body)
    if(!mime_body) {
        /* Error already logged */
        priv->qmsg_cur_email = NULL;
        process_smtp_response(gobj, qmsg_for_fail, -1, TRUE, to); // permanent: bad content
        KW_DECREF(msg);
        return -1;
    }

    const char *body_str = (const char *)gbuffer_cur_rd_pointer(mime_body);

    json_t *kw_send = json_pack(
        "{s:s, s:s, s:s, s:s, s:s}",
        "from", from,
        "to", to,
        "cc", cc,
        "bcc", bcc,
        "body", body_str
    );
    if(!kw_send) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "json_pack() FAILED for kw_send",
            NULL
        );
        gobj_trace_json(gobj, msg, "json_pack() FAILED for kw_send");
        GBUFFER_DECREF(mime_body)
        priv->qmsg_cur_email = NULL;
        process_smtp_response(gobj, qmsg_for_fail, -1, TRUE, to); // permanent: cannot build request
        KW_DECREF(msg);
        return -1;
    }
    GBUFFER_DECREF(mime_body)
    KW_DECREF(msg);

    priv->send++;

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(gobj, kw_send, "SEND EMAIL to %s", priv->url);
    }

    gobj_change_state(gobj, ST_WAIT_RESPONSE);

    /*
     *  Async: the smtp child will publish EV_ON_MESSAGE when the server
     *  acks/nacks the DATA stage. priv->qmsg_cur_email stays set until then.
     */
    int ret = gobj_send_event(priv->smtp, EV_SEND_MESSAGE, kw_send, gobj);
    if(ret < 0) {
        /*
         *  SMTP child not in ST_IDLE (raced a close between the dequeue guard
         *  and here). Transient: retry on the next dequeue / reconnect.
         */
        priv->qmsg_cur_email = NULL;
        process_smtp_response(gobj, qmsg_for_fail, -1, FALSE, to);
    }

    return 0;
}

/***************************************************************************
 *  Resolve the in-flight message after an SMTP send attempt.
 *      result >= 0 -> sent OK: unload from the queue.
 *      result <  0 -> failed:
 *          - transient (permanent==FALSE) with retries left -> keep the
 *            message at the head of the queue (NOT unloaded) so the next
 *            dequeue retries it. max_retries bounds total attempts.
 *          - permanent (bad content), or retries exhausted -> move to the
 *            failed (dead-letter) queue and unload.
 *  The message is never lost: it stays persisted in emails_queue until it is
 *  either sent or copied into emails_failed.
 ***************************************************************************/
PRIVATE int process_smtp_response(
    hgobj gobj,
    q_msg_t *msg,
    int result,
    BOOL permanent,
    const char *to
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!msg) {
        /*
         *  Spurious ack with nothing in flight (e.g. ac_on_close arriving
         *  twice). Just clear state and re-arm.
         */
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "NO pending current email",
            NULL
        );
        gobj_change_state(gobj, ST_IDLE);
        return tira_dela_cola(gobj);
    }

    if(result >= 0) {
        gobj_log_info(gobj, 0,
            "msgset",   "%s", MSGSET_INFO,
            "msg",      "%s", "email sent",
            "to",       "%s", to,
            "url",      "%s", priv->url,
            NULL
        );
        priv->sent++;
        priv->cur_retries = 0;
        trq_unload_msg(msg, result);
    } else {
        priv->cur_retries++;
        if(!permanent && priv->cur_retries < priv->max_retries) {
            /*
             *  Transient failure with retries left. The underlying SMTP /
             *  transport error is already logged downstream. Keep the message
             *  at the head of the queue (do NOT unload): the next dequeue
             *  (or the next EV_ON_OPEN, if the link dropped) retries it.
             */
            gobj_log_warning(gobj, 0,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", "email NOT sent, will retry",
                "to",           "%s", to,
                "url",          "%s", priv->url,
                "attempt",      "%ld", (long)priv->cur_retries,
                "max_retries",  "%ld", (long)priv->max_retries,
                NULL
            );
        } else {
            /*
             *  Permanent failure, or retries exhausted: move to the failed
             *  (dead-letter) queue and unload from the main queue.
             */
            gobj_log_error(gobj, 0,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", "email NOT sent, moved to failed queue",
                "to",           "%s", to,
                "url",          "%s", priv->url,
                "attempt",      "%ld", (long)priv->cur_retries,
                "max_retries",  "%ld", (long)priv->max_retries,
                "permanent",    "%d", permanent?1:0,
                NULL
            );
            enqueue_failing_message(gobj, msg);
            trq_unload_msg(msg, result);
            priv->cur_retries = 0;
        }
    }

    gobj_change_state(gobj, ST_IDLE);
    tira_dela_cola(gobj);

    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/********************************************************************
 *  Guarda el mensaje para enviarlo cuando se pueda
 ********************************************************************/
PRIVATE int ac_enqueue_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    /*
     *  Normalise the body to a persisted string. Senders may pass it as a
     *  transient "gbuffer" pointer (logcenter, legacy callers); the queue
     *  must store the bytes, not a pointer, or the body is lost on the first
     *  retry (the dequeued kw's auto-decref frees the gbuffer) and on restart.
     *  Extract the pointer (KW_EXTRACT removes the key, so the auto-decref
     *  won't double-free) and drain it into "body".
     */
    gbuffer_t *gbuf_body = (gbuffer_t *)(uintptr_t)kw_get_int(
        gobj, kw, "gbuffer", 0, KW_EXTRACT
    );
    if(gbuf_body) {
        char *p = gbuffer_cur_rd_pointer(gbuf_body);
        size_t l = gbuffer_leftbytes(gbuf_body);
        json_t *jn_body = json_stringn(p, l);
        if(jn_body) {
            json_object_set_new(kw, "body", jn_body);
        } else {
            /*
             *  Body is not valid UTF-8 (binary content): json_stringn refuses
             *  it and would otherwise drop the body silently. Persist it
             *  base64-encoded under "body_base64" so no byte is lost across
             *  retries / restarts; tira_dela_cola decodes it back to raw bytes.
             */
            gbuffer_t *b64 = gbuffer_binary_to_base64(p, l);
            if(b64) {
                json_object_set_new(kw, "body_base64",
                    json_stringn(gbuffer_cur_rd_pointer(b64), gbuffer_leftbytes(b64))
                );
                GBUFFER_DECREF(b64)
            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MEMORY,
                    "msg",          "%s", "base64 encode of binary body FAILED",
                    NULL
                );
            }
        }
        GBUFFER_DECREF(gbuf_body)
    }

    if(gobj_read_bool_attr(gobj, "disable_alarm_emails")) {
        const char *subject = kw_get_str(gobj, kw, "subject", "", 0);
        if(strstr(subject, "ALERTA Encolamiento") || strstr(subject, "ALERT Queuing")
        ) { // WARNING repeated in c_qiogate.c
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Ignore 'ALERT Queuing' email",
                NULL
            );
            KW_DECREF(kw);
            return 0;
        }
    }

    /*
     *  Enqueue the message
     */
    enqueue_message(
        gobj,
        kw  // not owned
    );

    if(gobj_in_this_state(gobj, ST_IDLE)) {
        tira_dela_cola(gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  EV_ON_OPEN fired by the SMTP child after a successful banner+EHLO[+AUTH].
 *  Kick the dequeue loop immediately
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->smtp) {
        priv->smtp_ready = TRUE;
        /* Session is healthy: cancel any pending reconnect retry and reset
           the backoff so the next outage starts from the minimum again. */
        clear_timeout(priv->timer);
        priv->reconnect_backoff = RECONNECT_BACKOFF_MIN_MS;
        tira_dela_cola(gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  EV_ON_CLOSE from the SMTP child. If a message was in flight, fail it
 *  so it lands in the failed queue / triggers a retry on the next dequeue.
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->smtp) {
        priv->smtp_ready = FALSE;
        int code = (int)kw_get_int(gobj, kw, "code", 0, 0);
        if(priv->qmsg_cur_email) {
            /*
             *  Session dropped mid-send. A 5xx reply code forwarded by the
             *  SMTP child means the server rejected this specific message
             *  (permanent) → dead-letter it directly. No code / a 4xx means a
             *  transient transport error → keep it queued for a retry on
             *  reconnect (until max_retries). process_smtp_response re-arms the
             *  retry timer via tira_dela_cola if mail remains.
             */
            BOOL permanent = (code >= 500 && code < 600);
            q_msg_t *qmsg = priv->qmsg_cur_email;
            priv->qmsg_cur_email = NULL;
            process_smtp_response(gobj, qmsg, -1, permanent, "");
        } else {
            /*
             *  Session went down with no message in flight: either a normal
             *  idle-close, or a handshake failure (banner/EHLO/AUTH) that
             *  blocks ALL queued mail, not one message.
             */
            if(code >= 500 && code < 600) {
                /*
                 *  Session-level permanent rejection (e.g. AUTH 535 bad
                 *  credentials, EHLO/banner 5xx). Log loudly and back off hard;
                 *  keep mail queued — do NOT dead-letter, it will flow once the
                 *  credentials/config are fixed.
                 */
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_APP,
                    "msg",          "%s", "SMTP session rejected, check credentials/config",
                    "code",         "%d", code,
                    "url",          "%s", priv->url,
                    NULL
                );
                priv->reconnect_backoff = RECONNECT_BACKOFF_MAX_MS;
            }
            /*
             *  If mail is still queued, schedule a backoff retry so it drains
             *  when the link / config recovers (ac_retry). Guard against the
             *  queue being closed (NULL) during shutdown: a deferred TCP
             *  disconnect can deliver EV_ON_CLOSE after mt_pause/close_queues.
             */
            if(priv->trq_emails_queue && trq_first_msg(priv->trq_emails_queue)) {
                set_timeout(priv->timer, priv->reconnect_backoff);
            }
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  EV_ON_MESSAGE callback from the SMTP child. kw carries {ok: bool,
 *  code: int}. ok=true → email sent; ok=false → enqueue to failed.
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src != priv->smtp) {
        /* Unexpected source — ignore but log. */
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "EV_ON_MESSAGE from unexpected source",
            NULL
        );
        KW_DECREF(kw);
        return 0;
    }

    BOOL ok = kw_get_bool(gobj, kw, "ok", 0, 0);
    int code = (int)kw_get_int(gobj, kw, "code", 0, 0);
    BOOL permanent = (!ok && code >= 500 && code < 600);

    q_msg_t *qmsg = priv->qmsg_cur_email;
    priv->qmsg_cur_email = NULL;
    process_smtp_response(gobj, qmsg, ok ? 0 : -1, permanent, "");

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Reconnect backoff timer fired. If the SMTP session is still down and mail
 *  is waiting, grow the backoff (capped) and try again via tira_dela_cola
 *  (which re-issues EV_CONNECT and re-arms this timer). The backoff resets to
 *  the minimum on a successful open (ac_on_open). This bounds the retry rate
 *  against a down server / bad config and drains the queue on recovery.
 ***************************************************************************/
PRIVATE int ac_retry(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->smtp_ready && priv->trq_emails_queue && trq_first_msg(priv->trq_emails_queue)) {
        priv->reconnect_backoff *= 2;
        if(priv->reconnect_backoff > RECONNECT_BACKOFF_MAX_MS) {
            priv->reconnect_backoff = RECONNECT_BACKOFF_MAX_MS;
        }
        tira_dela_cola(gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create      = mt_create,
    .mt_destroy     = mt_destroy,
    .mt_start       = mt_start,
    .mt_stop        = mt_stop,
    .mt_writing     = mt_writing,
    .mt_reading     = mt_reading,
    .mt_play        = mt_play,
    .mt_pause       = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_EMAILSENDER);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

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
    ev_action_t st_idle[] = {
        {EV_SEND_EMAIL,     ac_enqueue_message,     0},
        {EV_SEND_MESSAGE,   ac_enqueue_message,     0},
        {EV_ON_OPEN,        ac_on_open,             0},
        {EV_ON_CLOSE,       ac_on_close,            0},
        {EV_TIMEOUT,        ac_retry,               0},
        {0,0,0}
    };

    ev_action_t st_wait_response[] = {
        {EV_SEND_EMAIL,     ac_enqueue_message,     0},
        {EV_SEND_MESSAGE,   ac_enqueue_message,     0},
        {EV_ON_MESSAGE,     ac_on_message,          0},
        {EV_ON_CLOSE,       ac_on_close,            0},
        {EV_TIMEOUT,        ac_retry,               0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,           st_idle},
        {ST_WAIT_RESPONSE,  st_wait_response},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_SEND_MESSAGE,       EVF_PUBLIC_EVENT},
        {EV_SEND_EMAIL,         EVF_PUBLIC_EVENT},
        {EV_ON_OPEN,            0},
        {EV_ON_CLOSE,           0},
        {EV_ON_MESSAGE,         0},
        {EV_TIMEOUT,            0},
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
        0, // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,                          // acl
        command_table,
        s_user_trace_level,
        gcflag_required_start_to_play
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_emailsender(void)
{
    return create_gclass(C_EMAILSENDER);
}
