/***********************************************************************
 *          C_EMAILSENDER.C
 *          Emailsender GClass.
 *
 *          Email sender
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "c_emailsender.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_send_email(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_enable_alarm_emails(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_disable_alarm_emails(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

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

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,       "Command's help"),
SDATACM (DTP_SCHEMA,    "send-email",       0,                  pm_send_email,  cmd_send_email, "Send email."),
SDATACM (DTP_SCHEMA,    "disable-alarm-emails",0,               0,              cmd_disable_alarm_emails, "Disable send alarm emails."),
SDATACM (DTP_SCHEMA,    "enable-alarm-emails",0,                0,              cmd_enable_alarm_emails, "Enable send alarm emails."),
SDATA_END()
};

PRIVATE sdata_desc_t queueTb_it[] = {
/*-ATTR-type------------name----------------flag------------------------default---------description----------*/
SDATA (DTP_JSON,        "kw_email",         0,                          0,              "kw email"),
SDATA (DTP_INTEGER,     "retries",          0,                          0,              "Retries send email"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name--------------------flag------------------------default---------description---------- */
SDATA (DTP_INTEGER,     "timeout_response",     0,                          60*1000L,       "Timer curl response"),
SDATA (DTP_STRING,      "on_open_event_name",   0,                          EV_ON_OPEN,   "Must be empty if you don't want receive this event"),
SDATA (DTP_STRING,      "on_close_event_name",  0,                          EV_ON_CLOSE,  "Must be empty if you don't want receive this event"),
SDATA (DTP_STRING,      "on_message_event_name",0,                          EV_ON_MESSAGE,"Must be empty if you don't want receive this event"),
SDATA (DTP_BOOLEAN,     "as_yuno",              SDF_RD,                     FALSE,          "True when acting as yuno"),

SDATA (DTP_STRING,      "username",             SDF_RD,                     0,              "email username"),
SDATA (DTP_STRING,      "password",             SDF_RD,                     0,              "email password"),
SDATA (DTP_STRING,      "url",                  SDF_RD|SDF_REQUIRED,        0,              "smtp URL"),
SDATA (DTP_STRING,      "from",                 SDF_RD|SDF_REQUIRED,        0,              "default from"),
SDATA (DTP_STRING,      "from_beatiful",        SDF_RD,                     "",             "from with name"),
SDATA (DTP_INTEGER,     "max_tx_queue",         SDF_PERSIST|SDF_WR,         200,            "Maximum messages in tx queue."),
SDATA (DTP_INTEGER,     "timeout_dequeue",      SDF_PERSIST|SDF_WR,         10,             "Timeout to dequeue msgs."),
SDATA (DTP_INTEGER,     "max_retries",          SDF_PERSIST|SDF_WR,         4,              "Maximum retries to send email"),
SDATA (DTP_BOOLEAN,     "only_test",            SDF_PERSIST|SDF_WR,         FALSE,          "True when testing, send only to test_email"),
SDATA (DTP_BOOLEAN,     "add_test",             SDF_PERSIST|SDF_WR,         FALSE,          "True when testing, add test_email to send"),
SDATA (DTP_STRING,      "test_email",           SDF_PERSIST|SDF_WR,         "",             "test email"),

SDATA (DTP_INTEGER,     "send",                 SDF_RD|SDF_STATS,           0,              "Emails send"),
SDATA (DTP_INTEGER,     "sent",                 SDF_RD|SDF_STATS,           0,              "Emails sent"),
SDATA (DTP_BOOLEAN,     "disable_alarm_emails", SDF_PERSIST|SDF_WR,         FALSE,          "True to don't send alarm emails"),

SDATA (DTP_POINTER,     "user_data",            0,                          0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",           0,                          0,              "more user data"),
SDATA (DTP_POINTER,     "subscriber",           0,                          0,              "subscriber of output-events. If it's null then subscriber is the parent."),

/*-DB----type-----------name----------------flag------------------------schema----------free_fn---------header-----------*/
SDATADB (ASN_ITER,      "queueTb",          SDF_RD,                     queueTb_it,     sdata_destroy,  "Queue table"),
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
    hgobj curl;
    hgobj timer;
    hgobj persist;
    int32_t timeout_response;
    uint32_t timeout_dequeue;
    uint32_t max_tx_queue;
    uint32_t max_retries;
    hsdata sd_cur_email;

    const char *on_open_event_name;
    const char *on_close_event_name;
    const char *on_message_event_name;
    BOOL as_yuno;

    uint64_t *psend;
    uint64_t *psent;

    const char *username;
    const char *password;
    const char *url;
    const char *from;

    dl_list_t *tb_queue;
    int inform_on_close;
    int inform_no_more_email;
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

    priv->tb_queue = gobj_read_iter_attr(gobj, "queueTb");
    priv->timer = gobj_create("", C_TIMER, 0, gobj);
    priv->curl = gobj_create("emailsender", GCLASS_CURL, 0, gobj);
    gobj_set_bottom_gobj(gobj, priv->curl);
    //priv->persist = gobj_find_service("persist", FALSE);

    priv->psend = gobj_danger_attr_ptr(gobj, "send");
    priv->psent = gobj_danger_attr_ptr(gobj, "sent");

    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        /*
         *  SERVICE subscription model
         */
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout_response,      gobj_read_integer_attr)
    SET_PRIV(on_open_event_name,    gobj_read_str_attr)
    SET_PRIV(on_close_event_name,   gobj_read_str_attr)
    SET_PRIV(on_message_event_name, gobj_read_str_attr)
    SET_PRIV(username,              gobj_read_str_attr)
    SET_PRIV(password,              gobj_read_str_attr)
    SET_PRIV(url,                   gobj_read_str_attr)
    SET_PRIV(from,                  gobj_read_str_attr)
    SET_PRIV(as_yuno,               gobj_read_bool_attr)
    SET_PRIV(max_tx_queue,          gobj_read_uint32_attr)
    SET_PRIV(max_retries,           gobj_read_uint32_attr)
    SET_PRIV(timeout_dequeue,       gobj_read_uint32_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(max_tx_queue,        gobj_read_uint32_attr)
    ELIF_EQ_SET_PRIV(timeout_dequeue,   gobj_read_uint32_attr)
    ELIF_EQ_SET_PRIV(max_retries,       gobj_read_uint32_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t size = rc_iter_size(priv->tb_queue);
    if(size) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Emails lost by destroying gobj",
            "size",         "%s", size,
            NULL
        );
    }
    gobj_free_iter(priv->tb_queue, FALSE, sdata_destroy); // remove all rows
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    gobj_start(priv->curl);

    gobj_publish_event(gobj, priv->on_open_event_name, 0); // virtual

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_publish_event(gobj, priv->on_close_event_name, 0); // virtual

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    gobj_stop(priv->curl);
    //TODO V2 GBMEM_FREE(priv->mail_ref);

    return 0;
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
PRIVATE json_t *cmd_send_email(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *to = kw_get_str(gobj, kw, "to", 0, 0);
    const char *reply_to = kw_get_str(gobj, kw, "reply-to", 0, 0);
    const char *subject = kw_get_str(gobj, kw, "subject", "", 0);
    const char *attachment = kw_get_str(gobj, kw, "attachment", 0, 0);
    BOOL is_html = kw_get_bool(gobj, kw, "is_html", 0, 0);
    const char *inline_file_id = kw_get_str(gobj, kw, "inline_file_id", 0, 0);
    const char *body = kw_get_str(gobj, kw, "body", "", 0);

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
    int len = strlen(body);
    gbuffer_t *gbuf = gbuffer_create(len, len, 0, 0);
    if(len > 0) {
        gbuffer_append(gbuf, (void *)body, len);
    }

    json_t *kw_send = json_pack("{s:s, s:s, s:s, s:I, s:b, s:b, s:s}",
        "to", to,
        "reply_to", reply_to?reply_to:"",
        "subject", subject,
        "gbuffer", (json_int_t)(size_t)gbuf,
        "is_html", is_html,
        "__persistent_event__", 1,
        "__persistence_reference__", "asdfasdfasfdsdf" // TODO no se usa persistencia
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




            /***************************
             *      Actions
             ***************************/




/********************************************************************
 *  It can receive local messages (from command or beging child-gobj)
 *  or inter-events from others yunos.
 ********************************************************************/
PRIVATE int ac_send_curl(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    const char *from = kw_get_str(gobj, kw, "from", 0, 0);
    if(empty_string(from)) {
        from = priv->from;
    }
    const char *from_beatiful = kw_get_str(gobj, kw, "from_beatiful", "", 0);
    if(empty_string(from_beatiful)) {
        from_beatiful = gobj_read_str_attr(gobj, "from_beatiful");
    }
    const char *to = kw_get_str(gobj, kw, "to", 0, 0);
    const char *cc = kw_get_str(gobj, kw, "cc", "", 0);
    const char *reply_to = kw_get_str(gobj, kw, "reply_to", "", 0);
    const char *subject = kw_get_str(gobj, kw, "subject", "", 0);
    BOOL is_html = kw_get_bool(gobj, kw, "is_html", 0, 0);
    const char *attachment = kw_get_str(gobj, kw, "attachment", "", 0);
    const char *inline_file_id = kw_get_str(gobj, kw, "inline_file_id", 0, 0);

    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf NULL",
            "url",          "%s", priv->url,
            NULL
        );
        KW_DECREF(kw);
        set_timeout(priv->timer, priv->timeout_dequeue);
        return -1;
    }
    if(empty_string(to)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "<to> field is NULL",
            "url",          "%s", priv->url,
            NULL
        );
        KW_DECREF(kw);
        set_timeout(priv->timer, priv->timeout_dequeue);
        return -1;
    }


    /*
     *  Como la url esté mal y no se resuelva libcurl NO RETORNA NUNCA!
     *  Usa ips numéricas!
     */
    json_t *kw_curl = json_pack(
        "{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:b, s:s, s:I}",
        "command", "SEND",
        "dst_event", "EV_CURL_RESPONSE",
        "username", priv->username,
        "password", priv->password,
        "url", priv->url,
        "from", from,
        "from_beatiful", from_beatiful?from_beatiful:"",
        "to", to,
        "cc", cc,
        "reply_to", reply_to,
        "subject", subject,
        "is_html", is_html,
        "mail_ref", "",
        "gbuffer", (json_int_t)(size_t)gbuf
    );
    if(!kw_curl) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "json_pack() FAILED",
            NULL
        );
        KW_DECREF(kw);
        set_timeout(priv->timer, priv->timeout_dequeue);
        return 0;
    }

    if(!empty_string(attachment)) {
        if(access(attachment, 0)==0) {
            json_object_set_new(kw_curl, "attachment", json_string(attachment));
            if(!empty_string(inline_file_id)) {
                json_object_set_new(kw_curl, "inline_file_id", json_string(inline_file_id));
            }
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "attachment file not found, ignoring it",
                "attachment",   "%s", attachment,
                NULL
            );
        }
    }

    (*priv->psend)++;

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(gobj, kw_curl, "SEND EMAIL to %s", priv->url);
        log_debug_bf(0, gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf), "SEND EMAIL to %s", priv->url);
    }

    gobj_change_state(gobj, "ST_WAIT_SEND_ACK");
    set_timeout(priv->timer, priv->timeout_response);
    gobj_send_event(priv->curl, "EV_CURL_COMMAND", kw_curl, gobj);

    KW_DECREF(kw);
    return 0;
}

/********************************************************************
 *  Guarda el mensaje para enviarlo cuando se pueda
 ********************************************************************/
PRIVATE int ac_enqueue_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_read_bool_attr(gobj, "disable_alarm_emails")) {
        const char *subject = kw_get_str(gobj, kw, "subject", "", 0);
        if(strstr(subject, "ALERTA Encolamiento")) { // WARNING repeated in c_qiogate.c
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Ignore 'ALERTA Encolamiento' email",
                NULL
            );
            KW_DECREF(kw);
            return 0;
        }
    }

    size_t size = rc_iter_size(priv->tb_queue);
    if(size >= priv->max_tx_queue) {
        hsdata sd_email;
        rc_first_instance(priv->tb_queue, (rc_resource_t **)&sd_email);
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Tiro EMAIL por cola llena",
            NULL
        );
        json_t *jn_msg = sdata2json(sd_email, -1, 0);
        gobj_trace_json(gobj, jn_msg, "Tiro EMAIL por cola llena");
        json_decref(jn_msg);

        rc_delete_resource(sd_email, sdata_destroy);
    }

    /*
     *  Crea el registro del queue
     */

    hsdata sd_email = sdata_create(queueTb_it, 0,0,0,0,0);
    sdata_write_json(sd_email, "kw_email", kw); // kw incref
    rc_add_instance(priv->tb_queue, sd_email, 0);

    if(gobj_in_this_state(gobj, ST_IDLE)) {
        set_timeout(priv->timer, priv->timeout_dequeue);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_curl_response(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    int result = kw_get_int(gobj, kw, "result", 0, FALSE);
    if(result) {
        // Error already logged
    } else {
        rc_delete_resource(priv->sd_cur_email, sdata_destroy);
        priv->sd_cur_email = 0;
        gobj_trace_msg(gobj, "EMAIL SENT to %s", priv->url);
        (*priv->psent)++;
    }

    gobj_change_state(gobj, ST_IDLE);

    set_timeout(priv->timer, priv->timeout_dequeue); // tira de la cola, QUICK

    KW_DECREF(kw);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_dequeue(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->sd_cur_email) {
        int retries = sdata_read_int32(priv->sd_cur_email, "retries");
        retries++;
        sdata_write_int32(priv->sd_cur_email, "retries", retries);
        if(retries >= priv->max_retries) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBCURL_ERROR,
                "msg",          "%s", "Tiro email por maximo reintentos",
                NULL
            );
            json_t *jn_msg = sdata2json(priv->sd_cur_email, -1, 0);
            gobj_trace_json(gobj, jn_msg, "Tiro email por maximo reintentos");
            gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, jn_msg, "kw_email`gbuffer", 0, 0);
            gbuf_reset_rd(gbuf);
            gobj_trace_dump_gbuf(gobj, gbuf, "Tiro email por maximo reintentos");
            json_decref(jn_msg);

            rc_delete_resource(priv->sd_cur_email, sdata_destroy);
            priv->sd_cur_email = 0;
            set_timeout(priv->timer, priv->timeout_dequeue);
        } else {
            json_t *kw_email = sdata_read_json(priv->sd_cur_email, "kw_email");
            KW_INCREF(kw_email);
            gobj_send_event(gobj, "EV_SEND_CURL", kw_email, src);
            KW_DECREF(kw);
            return 0;
        }
    }

    hsdata sd_email; rc_instance_t *i_queue;
    i_queue = rc_first_instance(priv->tb_queue, (rc_resource_t **)&sd_email);
    if(i_queue) {
        priv->sd_cur_email = sd_email;
        json_t *kw_email = sdata_read_json(sd_email, "kw_email");
        KW_INCREF(kw_email);
        gobj_send_event(gobj, "EV_SEND_CURL", kw_email, src);
    }
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_response(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  WARNING No response: check your curl parameters!
     *  Retry in poll time.
     */
    //  TODO V2 GBMEM_FREE(priv->mail_ref);
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_LIBCURL_ERROR,
        "msg",          "%s", "Timeout curl",
        "url",          "%s", priv->url,
        "state",        "%s", gobj_current_state(gobj),
        NULL
    );
    gobj_change_state(gobj, ST_IDLE);

    set_timeout(priv->timer, 10); // tira de la cola, QUICK

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Child stopped
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw);
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
    .mt_writing     = mt_writing
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_EMAILSENDER);

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
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
        {EV_SEND_CURL,      ac_send_curl,           0},
        {EV_SEND_MESSAGE,   ac_enqueue_message,     0},
        {EV_SEND_EMAIL,     ac_enqueue_message,     0},
        {EV_TIMEOUT,        ac_dequeue,             0},
        {EV_ON_OPEN,        ac_on_open,             0},
        {EV_ON_CLOSE,       ac_on_close,            0},
        {EV_STOPPED,        ac_stopped,             0},
        {0,0,0}
    };

    ev_action_t st_wait_send_ack[] = {
        {EV_CURL_RESPONSE,  ac_curl_response,       0},
        {EV_SEND_MESSAGE,   ac_enqueue_message,     0},
        {EV_SEND_EMAIL,     ac_enqueue_message,     0},
        {EV_ON_OPEN,        ac_on_open,             0},
        {EV_ON_CLOSE,       ac_on_close,            0},
        {EV_TIMEOUT,        ac_timeout_response,    0},
        {EV_STOPPED,        ac_stopped,             0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,           st_idle},
        {ST_WAIT_SEND_ACK,  st_wait_send_ack},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_SEND_MESSAGE,       EVF_PUBLIC_EVENT},
        {EV_SEND_EMAIL,         EVF_PUBLIC_EVENT},
        {EV_SEND_CURL,          0},
        {EV_CURL_RESPONSE,      0},
        {EV_ON_OPEN,            EVF_NO_WARN_SUBS},
        {EV_ON_CLOSE,           EVF_NO_WARN_SUBS},
        {EV_ON_MESSAGE,         EVF_NO_WARN_SUBS},
        {EV_STOPPED,            0},
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
        lmt,
        tattr_desc,
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
