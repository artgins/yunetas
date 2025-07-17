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
SDATAPM (ASN_OCTET_STR, "cmd",          0,              0,          "command about you want help."),
SDATAPM (ASN_UNSIGNED,  "level",        0,              0,          "command search level in childs"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_send_email[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_OCTET_STR, "to",           0,              0,          "To field."),
SDATAPM (ASN_OCTET_STR, "reply-to",     0,              0,          "Reply-To field."),
SDATAPM (ASN_OCTET_STR, "subject",      0,              0,          "Subject field."),
SDATAPM (ASN_OCTET_STR, "attachment",   0,              0,          "Attachment file."),
SDATAPM (ASN_OCTET_STR, "inline_file_id",0,             0,          "Inline file ID (must be attachment too)."),
SDATAPM (ASN_BOOLEAN,   "is_html",      0,              0,          "Is html"),
SDATAPM (ASN_OCTET_STR, "body",         0,              0,          "Email body."),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (ASN_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,       "Command's help"),
SDATACM (ASN_SCHEMA,    "send-email",       0,                  pm_send_email,  cmd_send_email, "Send email."),
SDATACM (ASN_SCHEMA,    "disable-alarm-emails",0,               0,              cmd_disable_alarm_emails, "Disable send alarm emails."),
SDATACM (ASN_SCHEMA,    "enable-alarm-emails",0,                0,              cmd_enable_alarm_emails, "Enable send alarm emails."),
SDATA_END()
};

PRIVATE sdata_desc_t queueTb_it[] = {
/*-ATTR-type------------name----------------flag------------------------default---------description----------*/
SDATA (ASN_JSON,        "kw_email",         0,                          0,              "kw email"),
SDATA (ASN_INTEGER,     "retries",          0,                          0,              "Retries send email"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name--------------------flag------------------------default---------description---------- */
SDATA (ASN_INTEGER,     "timeout_response",     0,                          60*1000L,       "Timer curl response"),
SDATA (ASN_OCTET_STR,   "on_open_event_name",   0,                          "EV_ON_OPEN",   "Must be empty if you don't want receive this event"),
SDATA (ASN_OCTET_STR,   "on_close_event_name",  0,                          "EV_ON_CLOSE",  "Must be empty if you don't want receive this event"),
SDATA (ASN_OCTET_STR,   "on_message_event_name",0,                          "EV_ON_MESSAGE","Must be empty if you don't want receive this event"),
SDATA (ASN_BOOLEAN,     "as_yuno",              SDF_RD,                     FALSE,          "True when acting as yuno"),

SDATA (ASN_OCTET_STR,   "username",             SDF_RD,                     0,              "email username"),
SDATA (ASN_OCTET_STR,   "password",             SDF_RD,                     0,              "email password"),
SDATA (ASN_OCTET_STR,   "url",                  SDF_RD|SDF_REQUIRED,        0,              "smtp URL"),
SDATA (ASN_OCTET_STR,   "from",                 SDF_RD|SDF_REQUIRED,        0,              "default from"),
SDATA (ASN_OCTET_STR,   "from_beatiful",        SDF_RD,                     "",             "from with name"),
SDATA (ASN_UNSIGNED,    "max_tx_queue",         SDF_PERSIST|SDF_WR,         200,            "Maximum messages in tx queue."),
SDATA (ASN_UNSIGNED,    "timeout_dequeue",      SDF_PERSIST|SDF_WR,         10,             "Timeout to dequeue msgs."),
SDATA (ASN_UNSIGNED,    "max_retries",          SDF_PERSIST|SDF_WR,         4,              "Maximum retries to send email"),
SDATA (ASN_BOOLEAN,     "only_test",            SDF_PERSIST|SDF_WR,         FALSE,          "True when testing, send only to test_email"),
SDATA (ASN_BOOLEAN,     "add_test",             SDF_PERSIST|SDF_WR,         FALSE,          "True when testing, add test_email to send"),
SDATA (ASN_OCTET_STR,   "test_email",           SDF_PERSIST|SDF_WR,         "",             "test email"),

SDATA (ASN_COUNTER64,   "send",                 SDF_RD|SDF_STATS,           0,              "Emails send"),
SDATA (ASN_COUNTER64,   "sent",                 SDF_RD|SDF_STATS,           0,              "Emails sent"),
SDATA (ASN_BOOLEAN,     "disable_alarm_emails", SDF_PERSIST|SDF_WR,         FALSE,          "True to don't send alarm emails"),

SDATA (ASN_POINTER,     "user_data",            0,                          0,              "user data"),
SDATA (ASN_POINTER,     "user_data2",           0,                          0,              "more user data"),
SDATA (ASN_POINTER,     "subscriber",           0,                          0,              "subscriber of output-events. If it's null then subscriber is the parent."),

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
    priv->timer = gobj_create("", GCLASS_TIMER, 0, gobj);
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
    SET_PRIV(timeout_response,      gobj_read_int32_attr)
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
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Emails lost by destroying gobj",
            "size",         "%s", size,
            NULL
        );
    }
    rc_free_iter(priv->tb_queue, FALSE, sdata_destroy); // remove all rows
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
    return msg_iev_build_webix(
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
    const char *to = kw_get_str(kw, "to", 0, 0);
    const char *reply_to = kw_get_str(kw, "reply-to", 0, 0);
    const char *subject = kw_get_str(kw, "subject", "", 0);
    const char *attachment = kw_get_str(kw, "attachment", 0, 0);
    BOOL is_html = kw_get_bool(kw, "is_html", 0, 0);
    const char *inline_file_id = kw_get_str(kw, "inline_file_id", 0, 0);
    const char *body = kw_get_str(kw, "body", "", 0);

    if(empty_string(to)) {
        return msg_iev_build_webix(
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
    GBUFFER *gbuf = gbuf_create(len, len, 0, 0);
    if(len > 0) {
        gbuf_append(gbuf, (void *)body, len);
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
    gobj_send_event(gobj, "EV_SEND_EMAIL", kw_send, gobj);


    /*
     *  Inform
     */
    return msg_iev_build_webix(
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
    return msg_iev_build_webix(
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
    return msg_iev_build_webix(
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

    const char *from = kw_get_str(kw, "from", 0, 0);
    if(empty_string(from)) {
        from = priv->from;
    }
    const char *from_beatiful = kw_get_str(kw, "from_beatiful", "", 0);
    if(empty_string(from_beatiful)) {
        from_beatiful = gobj_read_str_attr(gobj, "from_beatiful");
    }
    const char *to = kw_get_str(kw, "to", 0, 0);
    const char *cc = kw_get_str(kw, "cc", "", 0);
    const char *reply_to = kw_get_str(kw, "reply_to", "", 0);
    const char *subject = kw_get_str(kw, "subject", "", 0);
    BOOL is_html = kw_get_bool(kw, "is_html", 0, 0);
    const char *attachment = kw_get_str(kw, "attachment", "", 0);
    const char *inline_file_id = kw_get_str(kw, "inline_file_id", 0, 0);

    GBUFFER *gbuf = (GBUFFER *)(size_t)kw_get_int(kw, "gbuffer", 0, 0);
    if(!gbuf) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
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
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
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
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
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
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
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
        log_debug_json(0, kw_curl, "SEND EMAIL to %s", priv->url);
        log_debug_bf(0, gbuf_cur_rd_pointer(gbuf), gbuf_leftbytes(gbuf), "SEND EMAIL to %s", priv->url);
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
        const char *subject = kw_get_str(kw, "subject", "", 0);
        if(strstr(subject, "ALERTA Encolamiento")) { // WARNING repeated in c_qiogate.c
            log_warning(0,
                "gobj",         "%s", gobj_full_name(gobj),
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
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Tiro EMAIL por cola llena",
            NULL
        );
        json_t *jn_msg = sdata2json(sd_email, -1, 0);
        log_debug_json(0, jn_msg, "Tiro EMAIL por cola llena");
        json_decref(jn_msg);

        rc_delete_resource(sd_email, sdata_destroy);
    }

    /*
     *  Crea el registro del queue
     */

    hsdata sd_email = sdata_create(queueTb_it, 0,0,0,0,0);
    sdata_write_json(sd_email, "kw_email", kw); // kw incref
    rc_add_instance(priv->tb_queue, sd_email, 0);

    if(gobj_in_this_state(gobj, "ST_IDLE")) {
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

    int result = kw_get_int(kw, "result", 0, FALSE);
    if(result) {
        // Error already logged
    } else {
        rc_delete_resource(priv->sd_cur_email, sdata_destroy);
        priv->sd_cur_email = 0;
        trace_msg("EMAIL SENT to %s", priv->url);
        (*priv->psent)++;
    }

    gobj_change_state(gobj, "ST_IDLE");

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
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBCURL_ERROR,
                "msg",          "%s", "Tiro email por maximo reintentos",
                NULL
            );
            json_t *jn_msg = sdata2json(priv->sd_cur_email, -1, 0);
            log_debug_json(0, jn_msg, "Tiro email por maximo reintentos");
            GBUFFER *gbuf = (GBUFFER *)(size_t)kw_get_int(jn_msg, "kw_email`gbuffer", 0, 0);
            gbuf_reset_rd(gbuf);
            log_debug_gbuf(0, gbuf, "Tiro email por maximo reintentos");
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
    log_error(0,
        "gobj",         "%s", gobj_full_name(gobj),
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_LIBCURL_ERROR,
        "msg",          "%s", "Timeout curl",
        "url",          "%s", priv->url,
        "state",        "%s", gobj_current_state(gobj),
        NULL
    );
    gobj_change_state(gobj, "ST_IDLE");

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
PRIVATE const EVENT input_events[] = {
    // top input
    {"EV_SEND_MESSAGE",     EVF_PUBLIC_EVENT,   0,  "Send email event"},
    {"EV_SEND_EMAIL",       EVF_PUBLIC_EVENT,   0,  "Send email event"},
    {"EV_SEND_CURL",        0,                  0,  "Send email to curl"},
    // bottom input
    {"EV_CURL_RESPONSE",    0,  0,  0},
    {"EV_ON_OPEN",          0,  0,  0},
    {"EV_ON_CLOSE",         0,  0,  0},
    {"EV_STOPPED",          0,  0,  0},
    {"EV_TIMEOUT",          0,  0,  0},
    // internal
    {NULL, 0, 0, 0}
};
PRIVATE const EVENT output_events[] = {
    {"EV_ON_OPEN",          EVF_NO_WARN_SUBS,  0,  0},
    {"EV_ON_CLOSE",         EVF_NO_WARN_SUBS,  0,  0},
    {"EV_ON_MESSAGE",       EVF_NO_WARN_SUBS,  0,  0},
    {NULL, 0, 0, 0}
};
PRIVATE const char *state_names[] = {
    "ST_IDLE",
    "ST_WAIT_SEND_ACK",
    NULL
};

PRIVATE EV_ACTION ST_IDLE[] = {
    {"EV_SEND_CURL",            ac_send_curl,           0},
    {"EV_SEND_MESSAGE",         ac_enqueue_message,     0},
    {"EV_SEND_EMAIL",           ac_enqueue_message,     0},
    {"EV_TIMEOUT",              ac_dequeue,             0},
    {"EV_ON_OPEN",              ac_on_open,             0},
    {"EV_ON_CLOSE",             ac_on_close,            0},
    {"EV_STOPPED",              ac_stopped,             0},
    {0,0,0}
};
PRIVATE EV_ACTION ST_WAIT_SEND_ACK[] = {
    {"EV_CURL_RESPONSE",        ac_curl_response,       0},
    {"EV_SEND_MESSAGE",         ac_enqueue_message,     0},
    {"EV_SEND_EMAIL",           ac_enqueue_message,     0},
    {"EV_ON_OPEN",              ac_on_open,             0},
    {"EV_ON_CLOSE",             ac_on_close,            0},
    {"EV_TIMEOUT",              ac_timeout_response,    0},
    {"EV_STOPPED",              ac_stopped,             0},
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_IDLE,
    ST_WAIT_SEND_ACK,
    NULL
};

PRIVATE FSM fsm = {
    input_events,
    output_events,
    state_names,
    states,
};

/***************************************************************************
 *              GClass
 ***************************************************************************/
/*---------------------------------------------*
 *              Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {0, 0, 0}
};

/*---------------------------------------------*
 *              GClass
 *---------------------------------------------*/
PRIVATE GCLASS _gclass = {
    0,  // base
    GCLASS_EMAILSENDER_NAME,
    &fsm,
    {
        mt_create,
        0, //mt_create2,
        mt_destroy,
        mt_start,
        mt_stop,
        0, //mt_play,
        0, //mt_pause,
        mt_writing,
        0, //mt_reading,
        0, //mt_subscription_added,
        0, //mt_subscription_deleted,
        0, //mt_child_added,
        0, //mt_child_removed,
        0, //mt_stats,
        0, //mt_command_parser,
        0, //mt_inject_event,
        0, //mt_create_resource,
        0, //mt_list_resource,
        0, //mt_save_resource,
        0, //mt_delete_resource,
        0, //mt_future21
        0, //mt_future22
        0, //mt_get_resource
        0, //mt_state_changed,
        0, //mt_authenticate,
        0, //mt_list_childs,
        0, //mt_stats_updated,
        0, //mt_disable,
        0, //mt_enable,
        0, //mt_trace_on,
        0, //mt_trace_off,
        0, //mt_gobj_created,
        0, //mt_future33,
        0, //mt_future34,
        0, //mt_publish_event,
        0, //mt_publication_pre_filter,
        0, //mt_publication_filter,
        0, //mt_authz_checker,
        0, //mt_future39,
        0, //mt_create_node,
        0, //mt_update_node,
        0, //mt_delete_node,
        0, //mt_link_nodes,
        0, //mt_future44,
        0, //mt_unlink_nodes,
        0, //mt_topic_jtree,
        0, //mt_get_node,
        0, //mt_list_nodes,
        0, //mt_shoot_snap,
        0, //mt_activate_snap,
        0, //mt_list_snaps,
        0, //mt_treedbs,
        0, //mt_treedb_topics,
        0, //mt_topic_desc,
        0, //mt_topic_links,
        0, //mt_topic_hooks,
        0, //mt_node_parents,
        0, //mt_node_childs,
        0, //mt_list_instances,
        0, //mt_node_tree,
        0, //mt_topic_size,
        0, //mt_future62,
        0, //mt_future63,
        0, //mt_future64
    },
    lmt,
    tattr_desc,
    sizeof(PRIVATE_DATA),
    0,  // acl
    s_user_trace_level,
    command_table,  // command_table
    gcflag_required_start_to_play, // gcflag
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC GCLASS *gclass_emailsender(void)
{
    return &_gclass;
}
