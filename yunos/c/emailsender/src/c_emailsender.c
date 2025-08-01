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
#include "c_curl.h"
#include "c_emailsender.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
enum {
    MARK_PENDING_ACK = 1, // TODO debería ser configurable
};

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int open_queue(hgobj gobj);
PRIVATE int close_queue(hgobj gobj);

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

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name--------------------flag--------------------default-----description---------- */
SDATA (DTP_STRING,      "username",             SDF_RD,                 0,      "email username"),
SDATA (DTP_STRING,      "password",             SDF_RD,                 0,      "email password"),
SDATA (DTP_STRING,      "url",                  SDF_RD|SDF_REQUIRED,    0,      "smtp URL"),
SDATA (DTP_STRING,      "from",                 SDF_RD|SDF_REQUIRED,    0,      "default from"),
SDATA (DTP_STRING,      "from_beautiful",       SDF_RD,                 "",     "from with name"),
SDATA (DTP_INTEGER,     "timeout_dequeue",      SDF_PERSIST|SDF_WR,     "10",   "Timeout in miliseconds to dequeue msgs."),
SDATA (DTP_INTEGER,     "max_retries",          SDF_PERSIST|SDF_WR,     "4",    "Maximum retries to send email"),
SDATA (DTP_BOOLEAN,     "only_test",            SDF_PERSIST|SDF_WR,     0,      "True when testing, send only to test_email"),
SDATA (DTP_BOOLEAN,     "add_test",             SDF_PERSIST|SDF_WR,     0,      "True when testing, add test_email to send"),
SDATA (DTP_STRING,      "test_email",           SDF_PERSIST|SDF_WR,     "",     "test email"),

SDATA (DTP_INTEGER,     "send",                 SDF_STATS,              0,      "Emails send"),
SDATA (DTP_INTEGER,     "sent",                 SDF_STATS,              0,      "Emails sent"),
SDATA (DTP_BOOLEAN,     "disable_alarm_emails", SDF_PERSIST|SDF_WR,     0,      "True to don't send alarm emails"),

SDATA (DTP_STRING,      "tranger_path",         SDF_RD,                 "",     "tranger path"),
SDATA (DTP_STRING,      "tranger_database",     SDF_RD,                 "",     "tranger database"),
SDATA (DTP_STRING,      "topic_name",           SDF_RD,                 "",     "trq_open topic_name"),
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
    hgobj curl;
    hgobj timer;
    hgobj gobj_input_side;
    hgobj persist;
    json_int_t timeout_dequeue;
    json_int_t max_retries;
    json_t *sd_cur_email;

    json_int_t send;
    json_int_t sent;
    json_int_t pending_acks;

    const char *username;
    const char *password;
    const char *url;
    const char *from;

    json_t *tb_queue;
    int inform_on_close;
    int inform_no_more_email;


    hgobj gobj_tranger_queues;
    json_t *tranger;
    tr_queue_t *trq_msgs;
    int32_t alert_queue_size;
    BOOL with_metadata;
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

    priv->tb_queue = json_array();
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    priv->curl = gobj_create_pure_child(gobj_name(gobj), C_CURL, 0, gobj);
    //priv->persist = gobj_find_service("persist", FALSE);

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
    SET_PRIV(username,              gobj_read_str_attr)
    SET_PRIV(password,              gobj_read_str_attr)
    SET_PRIV(url,                   gobj_read_str_attr)
    SET_PRIV(from,                  gobj_read_str_attr)
    SET_PRIV(max_retries,           gobj_read_integer_attr)
    SET_PRIV(timeout_dequeue,       gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout_dequeue,     gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(max_retries,       gobj_read_integer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t size = json_array_size(priv->tb_queue);
    if(size) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Emails lost by destroying gobj",
            "size",         "%s", size,
            NULL
        );
    }
    JSON_DECREF(priv->tb_queue)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    gobj_start(priv->curl);

    /*--------------------------------*
     *      Output queue
     *--------------------------------*/
    open_queue(gobj);
    trq_load(priv->trq_msgs);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->curl);
    //TODO V2 GBMEM_FREE(priv->mail_ref);

    close_queue(gobj);

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

    /*
     *  Start services
     */
    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    // gobj_subscribe_event(priv->gobj_input_side, 0, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

    /*
     *  Periodic timer
     */
    set_timeout(priv->timer, priv->timeout_dequeue); // pull from queue, QUICK

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

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
    int len = (int)strlen(body);
    gbuffer_t *gbuf = gbuffer_create(len, len);
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




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int open_queue(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->tranger) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "tranger NOT NULL",
            NULL
        );
        tranger2_shutdown(priv->tranger);
    }

    const char *path = gobj_read_str_attr(gobj, "tranger_path");
    if(empty_string(path)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "tranger database EMPTY",
            NULL
        );
        return -1;
    }

    const char *topic_name = gobj_read_str_attr(gobj, "topic_name");
    if(empty_string(topic_name)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "tranger topic_name EMPTY",
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
        "subscriber", (json_int_t)(size_t)gobj,
        "on_critical_error", (int)gobj_read_integer_attr(gobj, "on_critical_error")
    );
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "tranger_%s", gobj_name(gobj));
    priv->gobj_tranger_queues = gobj_create_service(
        name,
        C_TRANGER,
        kw_tranger,
        gobj
    );
    gobj_start(priv->gobj_tranger_queues);
    priv->tranger = gobj_read_pointer_attr(priv->gobj_tranger_queues, "tranger");

    priv->trq_msgs = trq_open(
        priv->tranger,
        topic_name,
        gobj_read_str_attr(gobj, "tkey"),
        tranger2_str2system_flag(gobj_read_str_attr(gobj, "system_flag")),
        gobj_read_integer_attr(gobj, "backup_queue_size")
    );

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int close_queue(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    EXEC_AND_RESET(trq_close, priv->trq_msgs);

    /*----------------------------------*
     *      Close Timeranger queues
     *----------------------------------*/
    gobj_stop(priv->gobj_tranger_queues);
    EXEC_AND_RESET(gobj_destroy, priv->gobj_tranger_queues);
    priv->tranger = 0;

    return 0;
}

/***************************************************************************
 *  Enqueue message
 ***************************************************************************/
PRIVATE q_msg_t *enqueue_message(
    hgobj gobj,
    json_t *kw  // not owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(!priv->trq_msgs) {
        gobj_log_critical(gobj, LOG_OPT_ABORT|LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "trq_msgs NULL",
            NULL
        );
        return 0;
    }

    json_t *kw_clean_clone;

    if(!priv->with_metadata) {
        kw_incref(kw);
        kw_clean_clone = kw_filter_metadata(gobj, kw);
    } else {
        kw_clean_clone = kw_incref(kw);
    }
    q_msg_t *msg = trq_append(
        priv->trq_msgs,
        kw_clean_clone
    );
    if(!msg) {
        gobj_log_critical(gobj, LOG_OPT_ABORT|LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Message NOT SAVED in the queue",
            NULL
        );
        return 0;
    }

    return msg;
}

/***************************************************************************
 *  Resetea los timeout_ack y los MARK_PENDING_ACK
 ***************************************************************************/
PRIVATE int reset_soft_queue(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->trq_msgs) { // 0 if not running
        q_msg_t *msg;
        qmsg_foreach_forward(priv->trq_msgs, msg) {
            trq_set_soft_mark(msg, MARK_PENDING_ACK, FALSE);
        }
    }
    priv->pending_acks = 0;
    priv->last_msg_sent = 0;

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int dequeue_msg(
    hgobj gobj,
    uint64_t __t__,
    uint64_t rowid,
    int result
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    q_msg_t *msg = trq_get_by_rowid(priv->trq_msgs, rowid);
    if(msg) {
        if(priv->last_msg_sent == msg) {
            priv->last_msg_sent = 0;
        }
        if((trq_get_soft_mark(msg) & MARK_PENDING_ACK)) {
            trq_set_soft_mark(msg, MARK_PENDING_ACK, FALSE);

            if (priv->pending_acks > 0) {
                priv->pending_acks--;
            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "ppending_acks ZERO or NEGATIVE",
                    "pending_acks", "%ld", (unsigned long) priv->pending_acks,
                    NULL
                );
            }
        }

        trq_unload_msg(msg, result);

    } else {
        if(trq_check_pending_rowid(
            priv->trq_msgs,
            __t__,
            rowid
        )!=0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "Message not found in the queue",
                "rowid",        "%ld", (unsigned long)rowid,
                NULL
            );
        }
    }

    return 0;
}

/***************************************************************************
 *  Process ACK message
 ***************************************************************************/
PRIVATE int process_ack(
    hgobj gobj,
    const char *event,
    json_t *kw, // owned
    hgobj src
) {
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

    gbuffer_incref(gbuf);
    json_t *jn_ack_message = gbuf2json(gbuf, 2);

    json_t *trq_md = trq_get_metadata(jn_ack_message);
    uint64_t rowid = kw_get_int(
        gobj,
        trq_md,
        "__msg_rowid__",
        0,
        KW_REQUIRED
    );
    uint64_t __t__ = kw_get_int(
        gobj,
        trq_md,
        "__msg_t__",
        0,
        KW_REQUIRED
    );
    int result = (int)kw_get_int(
        gobj,
        trq_md,
        "result",
        0,
        KW_REQUIRED
    );

    dequeue_msg(gobj, __t__, rowid, result);

    JSON_DECREF(jn_ack_message)

    KW_DECREF(kw)

    // TODO send more, set timeout
    return 0;
}




            /***************************
             *      Actions
             ***************************/




/********************************************************************
 *  Guarda el mensaje para enviarlo cuando se pueda
 ********************************************************************/
PRIVATE int ac_enqueue_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

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
     *  Crea el registro del queue
     */
    json_array_append(priv->tb_queue, kw);

    if(gobj_in_this_state(gobj, ST_IDLE)) {
        set_timeout(priv->timer, priv->timeout_dequeue); // pull from queue, QUICK
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_to_dequeue(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->sd_cur_email) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Already a current sending email",
            NULL
        );
        gobj_trace_json(gobj, priv->sd_cur_email, "Already a current sending email");
        JSON_DECREF(priv->sd_cur_email)
    }

    priv->sd_cur_email = kw_get_list_value(gobj, priv->tb_queue, 0, KW_EXTRACT);
    if(priv->sd_cur_email) {
        gobj_send_event(gobj, EV_CURL_COMMAND, json_incref(priv->sd_cur_email), src);
    }

    KW_DECREF(kw);
    return 0;
}

/********************************************************************
 *
 ********************************************************************/
PRIVATE int ac_curl_command(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->sd_cur_email) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "NOT a current sending email",
            NULL
        );
        set_timeout(priv->timer, priv->timeout_dequeue); // pull from queue, QUICK
        KW_DECREF(kw);
        return -1;
    }

    const char *from = kw_get_str(gobj, kw, "from", 0, 0);
    if(empty_string(from)) {
        from = priv->from;
    }
    const char *from_beautiful = kw_get_str(gobj, kw, "from_beautiful", "", 0);
    if(empty_string(from_beautiful)) {
        from_beautiful = gobj_read_str_attr(gobj, "from_beautiful");
    }
    const char *to = kw_get_str(gobj, kw, "to", 0, 0);
    const char *cc = kw_get_str(gobj, kw, "cc", "", 0);
    const char *reply_to = kw_get_str(gobj, kw, "reply_to", "", 0);
    const char *subject = kw_get_str(gobj, kw, "subject", "", 0);
    BOOL is_html = kw_get_bool(gobj, kw, "is_html", 0, 0);
    const char *attachment = kw_get_str(gobj, kw, "attachment", "", 0);
    const char *inline_file_id = kw_get_str(gobj, kw, "inline_file_id", 0, 0);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf NULL",
            "url",          "%s", priv->url,
            NULL
        );
        set_timeout(priv->timer, priv->timeout_dequeue); // pull from queue, QUICK
        KW_DECREF(kw);
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
        set_timeout(priv->timer, priv->timeout_dequeue); // pull from queue, QUICK
        KW_DECREF(kw);
        return -1;
    }

    /*
     *  Como la url esté mal y no se resuelva libcurl NO RETORNA NUNCA!
     *  Usa ips numéricas!
     */
    gbuffer_incref(gbuf);
    json_t *kw_curl = json_pack(
        "{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:b, s:s, s:I}",
        "username", priv->username,
        "password", priv->password,
        "url", priv->url,
        "from", from,
        "from_beautiful", from_beautiful?from_beautiful:"",
        "to", to,
        "cc", cc,
        "reply_to", reply_to,
        "subject", subject,
        "is_html", is_html,
        "mail_ref", "",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    if(!kw_curl) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "json_pack() FAILED",
            NULL
        );
        set_timeout(priv->timer, priv->timeout_dequeue); // pull from queue, QUICK
        KW_DECREF(kw);
        return -1;
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

    priv->send++;

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(gobj, kw_curl, "SEND EMAIL to %s", priv->url);
        gobj_trace_buffer(gobj,
            gbuffer_cur_rd_pointer(gbuf),
            gbuffer_leftbytes(gbuf),
            "SEND EMAIL to %s",
            priv->url
        );
    }

    gobj_change_state(gobj, ST_WAIT_RESPONSE);
    gobj_send_event(priv->curl, EV_CURL_COMMAND, kw_curl, gobj);

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

    int result = (int)kw_get_int(gobj, kw, "result", 0, FALSE);
    if(result) {
        // Error already logged
    } else {
        JSON_DECREF(priv->sd_cur_email)
        gobj_trace_msg(gobj, "EMAIL SENT to %s", priv->url);
        priv->sent++;
    }

    gobj_change_state(gobj, ST_IDLE);

    set_timeout(priv->timer, priv->timeout_dequeue); // pull from queue, QUICK

    KW_DECREF(kw);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    // Some app is connected, ignore
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    // Some app is disconnected, ignore
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
        {EV_CURL_COMMAND,   ac_curl_command,        0},
        {EV_SEND_EMAIL,     ac_enqueue_message,     0},
        {EV_SEND_MESSAGE,   ac_enqueue_message,     0},
        {EV_TIMEOUT,        ac_timeout_to_dequeue,  0},
        {EV_ON_OPEN,        ac_on_open,             0},
        {EV_ON_CLOSE,       ac_on_close,            0},
        {0,0,0}
    };

    ev_action_t st_wait_response[] = {
        {EV_CURL_RESPONSE,  ac_curl_response,       0},
        {EV_SEND_EMAIL,     ac_enqueue_message,     0},
        {EV_SEND_MESSAGE,   ac_enqueue_message,     0},
        {EV_ON_OPEN,        ac_on_open,             0},
        {EV_ON_CLOSE,       ac_on_close,            0},
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
        {EV_CURL_COMMAND,       0},
        {EV_CURL_RESPONSE,      0},
        {EV_ON_OPEN,            0},
        {EV_ON_CLOSE,           0},
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
