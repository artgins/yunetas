/***********************************************************************
 *          C_QIOGATE.C
 *          Qiogate GClass.
 *
 *          IOGate with persistent queue
 *
 *          Copyright (c) 2019 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include <timeranger2.h>
#include <command_parser.h>
#include <tr_queue.h>
#include "msg_ievent.h"
#include "yunetas_environment.h"
#include "c_timer.h"
#include "c_qiogate.h"
#include "c_tranger.h"

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
PRIVATE json_t *cmd_queue_mark_pending(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_queue_mark_notpending(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "level=1: search in bottoms, level=2: search in all childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_queue[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "key",          0,              0,          "Key"),
SDATAPM (DTP_INTEGER,   "from-rowid",   0,              0,          "From rowid"),
SDATAPM (DTP_INTEGER,   "to-rowid",     0,              0,          "To rowid"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-----------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,         pm_help,        cmd_help,       "Command's help"),

SDATACM2(DTP_SCHEMA,    "queue_mark_pending", SDF_AUTHZ_X, 0,pm_queue,       cmd_queue_mark_pending, "Mark selected messages as pending (Will be resend)."),
SDATACM2(DTP_SCHEMA,    "queue_mark_notpending", SDF_AUTHZ_X, 0, pm_queue,   cmd_queue_mark_notpending,"Mark selected messages as notpending (Will NOT be send or resend)."),
SDATA_END()
};


/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag------------------------default---------description---------- */
SDATA (DTP_INTEGER,     "timeout_poll",     SDF_RD,             "1000",     "Timeout polling, in milliseconds"),
SDATA (DTP_INTEGER,     "timeout_backup",   SDF_RD,             "1",        "Timeout to check backup, in seconds"),

SDATA (DTP_STRING,      "tranger_path",     SDF_RD,             "",         "tranger path"),
SDATA (DTP_STRING,      "tranger_database", SDF_RD,             "",         "tranger database"),
SDATA (DTP_STRING,      "topic_name",       SDF_RD,             "",         "trq_open topic_name"),
SDATA (DTP_STRING,      "tkey",             SDF_RD,             "",         "trq_open tkey"),
SDATA (DTP_STRING,      "system_flag",      SDF_RD,             "",         "trq_open system_flag"),
SDATA (DTP_INTEGER,     "on_critical_error",SDF_RD,             "0x0010",   "LOG_OPT_TRACE_STACK"),
SDATA (DTP_STRING,      "alert_message",    SDF_WR|SDF_PERSIST, "ALERTA Encolamiento", "Alert message"),
SDATA (DTP_INTEGER,     "max_pending_acks", SDF_WR|SDF_PERSIST, "10000",    "Maximum messages pending of ack"),
SDATA (DTP_INTEGER,     "backup_queue_size",SDF_WR|SDF_PERSIST, "1000000",  "Do backup at this size"),
SDATA (DTP_INTEGER,     "alert_queue_size", SDF_WR|SDF_PERSIST, "2000",     "Limit alert queue size"),
SDATA (DTP_INTEGER,     "timeout_ack",      SDF_WR|SDF_PERSIST, "60",       "Timeout ack in seconds"),

SDATA (DTP_BOOLEAN,     "with_metadata",    SDF_RD,             0,          "Don't filter metadata"),
SDATA (DTP_BOOLEAN,     "disable_alert",    SDF_WR|SDF_PERSIST, 0,          "Disable alert"),
SDATA (DTP_STRING,      "alert_from",       SDF_WR,             "",         "Alert from"),
SDATA (DTP_STRING,      "alert_to",         SDF_WR|SDF_PERSIST, "",         "Alert destination"),

SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES      = 0x0001,
    TRACE_QUEUE_PROT    = 0x0002,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",                "Trace messages"},
{"trace_queue_prot",        "Trace queue protocol"},
{0, 0},
};


/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    json_int_t timeout_poll;
    json_int_t timeout_ack;
    json_int_t timeout_backup;
    time_t t_backup;

    hgobj timer;

    hgobj gobj_tranger_queues;
    json_t *tranger;
    tr_queue_t *trq_msgs;
    int32_t alert_queue_size;
    BOOL with_metadata;
    q_msg_t *last_msg_sent;

    hgobj gobj_bottom_side;
    BOOL bottom_side_opened;

    BOOL disable_alert;
    uint32_t pending_acks;
    uint32_t max_pending_acks;

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
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout_poll,              gobj_read_integer_attr)
    SET_PRIV(timeout_ack,               gobj_read_integer_attr)
    SET_PRIV(timeout_backup,            gobj_read_integer_attr)
    SET_PRIV(with_metadata,             gobj_read_bool_attr)
    SET_PRIV(alert_queue_size,          gobj_read_integer_attr)
    SET_PRIV(max_pending_acks,          gobj_read_integer_attr)
    SET_PRIV(disable_alert,             gobj_read_bool_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout_poll,            gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_ack,           gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_backup,        gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(alert_queue_size,      gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(max_pending_acks,      gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(disable_alert,         gobj_read_bool_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);

    priv->gobj_bottom_side = gobj_bottom_gobj(gobj);

    /*--------------------------------*
     *      Checks
     *--------------------------------*/
    if(priv->timeout_ack <= 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "timeout_ack EMPTY",
            NULL
        );
        priv->timeout_ack = 60;
    }

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
    gobj_stop(priv->timer);

    close_queue(gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method stats
 ***************************************************************************/
PRIVATE json_t *mt_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_data = json_object();

    json_object_update_new(jn_data, gobj_stats(priv->gobj_bottom_side, stats, 0, gobj));

    json_object_set_new(jn_data, "msgs_in_queue", json_integer((json_int_t)trq_size(priv->trq_msgs)));
    json_object_set_new(jn_data, "pending_acks", json_integer((json_int_t)priv->pending_acks));

    KW_DECREF(kw)
    return jn_data;
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
PRIVATE json_t *cmd_queue_mark_pending(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_is_playing(gobj)) {
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("you must PAUSE the yuno before executing this command."),
            0,
            0,
            kw  // owned
        );
    }

    int64_t from_rowid = kw_get_int(gobj, kw, "from-rowid", 0, KW_WILD_NUMBER);
    int64_t to_rowid = kw_get_int(gobj, kw, "to-rowid", 0, KW_WILD_NUMBER);
    if(from_rowid == 0) {
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("Please, specify some from-rowid."),
            0,
            0,
            kw  // owned
        );
    }

    /*--------------------------------*
     *      Ouput queue
     *--------------------------------*/
    open_queue(gobj);
    trq_load_all(priv->trq_msgs, from_rowid, to_rowid);

    /*----------------------------------*
     *      Mark pending
     *----------------------------------*/
    int count = 0;
    q_msg_t *msg;
    qmsg_foreach_forward(priv->trq_msgs, msg) {
        count++;
        trq_set_hard_flag(msg, TRQ_MSG_PENDING, 1);
    }
    /*
     *  Close que queue
     */
    close_queue(gobj);


    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%d messages marked as PENDING", count),
        0,
        0,
        kw  // owned
    );
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_queue_mark_notpending(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_is_playing(gobj)) {
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("you must PAUSE the yuno before executing this command."),
            0,
            0,
            kw  // owned
        );
    }

    int64_t from_rowid = kw_get_int(gobj, kw, "from-rowid", 0, KW_WILD_NUMBER);
    int64_t to_rowid = kw_get_int(gobj, kw, "to-rowid", 0, KW_WILD_NUMBER);
    if(from_rowid == 0) {
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("Please, specify some from-rowid."),
            0,
            0,
            kw  // owned
        );
    }

    /*--------------------------------*
     *      Ouput queue
     *--------------------------------*/
    open_queue(gobj);
    trq_load_all(priv->trq_msgs, from_rowid, to_rowid);

    /*----------------------------------*
     *      Unmark pending
     *----------------------------------*/
    int count = 0;
    q_msg_t *msg;
    qmsg_foreach_forward(priv->trq_msgs, msg) {
        count++;
        trq_set_hard_flag(msg, TRQ_MSG_PENDING, 0);
    }

    /*
     *  Close que queue
     */
    close_queue(gobj);

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%d messages marked as NOT-PENDING", count),
        0,
        0,
        kw  // owned
    );
    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Send alert
 ***************************************************************************/
PRIVATE int send_alert(hgobj gobj, const char *subject, const char *message)
{
    static time_t t = 0;

    if(t==0) {
        t = start_sectimer(1*60);
    } else {
        if(!test_sectimer(t)) {
            return 0;
        }
        t = start_sectimer(1*60);
    }
    const char *from = gobj_read_str_attr(gobj, "alert_from");
    const char *to = gobj_read_str_attr(gobj, "alert_to");
    hgobj gobj_emailsender = gobj_find_service("emailsender", FALSE);
    if(!gobj_emailsender) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SERVICE_ERROR,
            "msg",          "%s", "Service 'emailsender' not found",
            NULL
        );
        return -1;
    }
    json_t *kw_email = json_object();
    json_object_set_new(kw_email, "from", json_string(from));
    json_object_set_new(kw_email, "subject", json_string(subject));
    json_object_set_new(kw_email, "is_html", json_true());
    json_object_set_new(kw_email, "to", json_string(to));

    gbuffer_t *gbuf = gbuffer_create(strlen(message), strlen(message));
    gbuffer_append_string(gbuf, message);

    json_object_set_new(kw_email,
        "gbuffer",
        json_integer((json_int_t)(size_t)gbuf)
    );
    gobj_send_event(gobj_emailsender, EV_SEND_EMAIL, kw_email, gobj_default_service());
    return 0;
}

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

    if(!priv->disable_alert) {
        if(priv->alert_queue_size > 0 && trq_size(priv->trq_msgs) >= priv->alert_queue_size) {
            if(trq_size(priv->trq_msgs) % priv->alert_queue_size == 0) {
                char subject[280];
                char alert[512];
                struct tm *tm;
                char utc_stamp[164];
                time_t timestamp;
                time(&timestamp);
                tm = gmtime(&timestamp);
                strftime(utc_stamp, sizeof(utc_stamp), "%Y-%m-%dT%H:%M:%S%z", tm);

                char local_stamp[164];
                tm = localtime(&timestamp);
                strftime(local_stamp, sizeof(local_stamp), "%Y-%m-%dT%H:%M:%S%z", tm);

                // WARNING "ALERTA Encolamiento" used in emailsender
                snprintf(
                    subject,
                    sizeof(subject),
                    "%s %ld msgs, node '%s', yuno '%s', queue %s, owner %s",
                    gobj_read_str_attr(gobj, "alert_message"), //"ALERTA Encolamiento",
                    (unsigned long)trq_size(priv->trq_msgs),
                    get_hostname(),
                    gobj_yuno_role_plus_name(),
                    gobj_read_str_attr(gobj, "tranger_database"),
                    gobj_yuno_node_owner()
                );
                snprintf(
                    alert,
                    sizeof(alert),
                    "%s %ld msgs, node '%s', yuno '%s' %s\nUTC %s, LOCAL %s",
                    gobj_read_str_attr(gobj, "alert_message"), //"ALERTA Encolamiento",
                    (unsigned long)trq_size(priv->trq_msgs),
                    get_hostname(),
                    gobj_yuno_role_plus_name(),
                    gobj_read_str_attr(gobj, "tranger_database"),
                    utc_stamp, local_stamp
                );
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_QUEUE_ALARM,
                    "msg",          "%s", subject,
                    "node",         "%s", get_hostname(),
                    "yuno",         "%s", gobj_yuno_role_plus_name(),
                    "owner",        "%s", gobj_yuno_node_owner(),
                    "queue",        "%s", gobj_read_str_attr(gobj, "tranger_database"),
                    "queue_size",   "%ld", (unsigned long)trq_size(priv->trq_msgs),
                    "utc",          "%ld", (long)timestamp,
                    NULL
                );

                send_alert(gobj, subject, alert);
            }
        }
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
 *  Send message to bottom side
 ***************************************************************************/
PRIVATE int send_message_to_bottom_side(hgobj gobj, q_msg_t *msg)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_msg = trq_msg_json(msg);
    uint64_t rowid = trq_msg_rowid(msg);
    uint64_t __t__ = trq_msg_time(msg);
    md2_record_ex_t *md_record_ex = trq_msg_md(msg);

    trq_set_metadata(jn_msg, "__msg_rowid__", json_integer((json_int_t)rowid));
    trq_set_metadata(
        jn_msg, "__msg_md_rowid__", json_integer((json_int_t)md_record_ex->rowid)
    );
    trq_set_metadata(jn_msg, "__msg_t__", json_integer((json_int_t)__t__));

    priv->last_msg_sent = msg;

    if(gobj_trace_level(gobj) & (TRACE_MESSAGES|TRACE_QUEUE_PROT)) {
        gobj_trace_msg(gobj, "QIOGATE send %s ==> %s, %"PRIu64,
            gobj_short_name(gobj),
            gobj_short_name(priv->gobj_bottom_side),
            (uint64_t)rowid
        );
    }

    json_t *kw_send = json_pack("{s:I}",
        "gbuffer", (json_int_t)(size_t)json2gbuf(0, jn_msg, JSON_COMPACT)
    );
    return gobj_send_event(priv->gobj_bottom_side, EV_SEND_MESSAGE, kw_send, gobj);
}

/***************************************************************************
 *  Send batch of messages to bottom side
 *  ``id``  is 0 when calling from timer
 *          or not 0 when calling from receiving message
 *  // re-send al abrir la salida
 *  // re-send al recibir (único con id):   -> (*) ->
 *  // re-send al recibir ack:                 (*) <->
 *  // re-send por timeout periódico           (*) ->
 ***************************************************************************/
PRIVATE int send_batch_messages(hgobj gobj, q_msg_t *msg)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*------------------------------*
     *      Sending one message
     *------------------------------*/
    if(msg) {
        uint64_t rowid = trq_msg_rowid(msg);
        uint64_t t = trq_msg_time(msg);
        if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
            gobj_trace_msg(gobj, "  -> (1)     - rowid %"PRIu64", t %"PRIu64", pending_acks %d", rowid, t, priv->pending_acks);
        }

        /*
         *  Check: no send if the previous to last has no MARK_PENDING_ACK
         *  (the last is this message)
         */
        q_msg_t *last_msg = trq_last_msg(priv->trq_msgs);
        q_msg_t *prev_last_msg = trq_prev_msg(last_msg);
        if(prev_last_msg) {
            if(!(trq_get_soft_mark(prev_last_msg) & MARK_PENDING_ACK)) {
                if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                    gobj_trace_msg(gobj, "     (1) xxx - rowid %"PRIu64", t %"PRIu64"", rowid, t);
                }
                return 0; // Previous message has not sent
            }
        }

        if(priv->max_pending_acks==0 || priv->pending_acks < priv->max_pending_acks) {
            if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                gobj_trace_msg(gobj, "     (1) ->  - rowid %"PRIu64", t %"PRIu64"", rowid, t);
            }
            send_message_to_bottom_side(gobj, msg);
            priv->pending_acks++;
            trq_set_soft_mark(msg, MARK_PENDING_ACK, TRUE);
            return 1; // Sent one message

        } else {
            // Max pending reached
            if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                gobj_trace_msg(gobj, "     (1) x   - rowid %"PRIu64", t %"PRIu64"", rowid, t);
            }
            return 0;
        }
    }

    /*----------------------------------*
     *      Sending batch messages
     *----------------------------------*/
    int sent = 0;
    q_msg_t * n;

    if(priv->last_msg_sent) {
        msg = priv->last_msg_sent;
    } else {
        msg = trq_first_msg(priv->trq_msgs);
    }
    for(n = trq_next_msg(msg);
        msg!=0 ;
        msg = n, n = trq_next_msg(msg)
    ) {
        uint64_t rowid = trq_msg_rowid(msg);
        uint64_t t = trq_msg_time(msg);

        if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
            gobj_trace_msg(gobj, "     ( )     - rowid %"PRIu64", t %"PRIu64", pending_acks %d", rowid, t, priv->pending_acks);
        }

        if((trq_get_soft_mark(msg) & MARK_PENDING_ACK)) {
            /*
             *  with MARK_PENDING_ACK
             */
            if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                gobj_trace_msg(gobj, "     (X)     - rowid %"PRIu64", t %"PRIu64", sent %d", rowid, t, sent);
            }
        } else {
            /*
             *  Send new msgs without MARK_PENDING_ACK
             */
            if(priv->max_pending_acks==0 ||  priv->pending_acks < priv->max_pending_acks) {
                if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                    gobj_trace_msg(gobj, "     (-) ->  - rowid %"PRIu64", t %"PRIu64"", rowid, t);
                }
                send_message_to_bottom_side(gobj, msg);
                sent++;
                priv->pending_acks++;
                trq_set_soft_mark(msg, MARK_PENDING_ACK, TRUE);
            } else {
                // Max pending reached
                if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                    gobj_trace_msg(gobj, "     (-) x   - rowid %"PRIu64", t %"PRIu64"", rowid, t);
                }
                return sent;
            }
        }
    }

    return sent;
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
        uint64_t tt = trq_msg_time(msg);
        if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
            gobj_trace_msg(gobj, "     ( ) <-  - rowid %"PRIu64", t %"PRIu64" ACK", rowid, tt);
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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

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

    if(gobj_trace_level(gobj) & (TRACE_MESSAGES|TRACE_QUEUE_PROT)) {
        gobj_trace_msg(gobj, "QIOGATE ack %s <== %s, %"PRIu64,
            gobj_short_name(gobj),
            gobj_short_name(priv->gobj_bottom_side),
            rowid
        );
    }

    dequeue_msg(gobj, __t__, rowid, result);

    JSON_DECREF(jn_ack_message)

    KW_DECREF(kw)

    if(priv->bottom_side_opened) {
        send_batch_messages(gobj, 0); // try to send more messages after receiving ack
    }
    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  Connected
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_bottom_side) {
        priv->bottom_side_opened = TRUE;
        send_batch_messages(gobj, 0);  // On open send or resend
        set_timeout(priv->timer, priv->timeout_poll);

        if(priv->timeout_backup > 0) {
            priv->t_backup = start_sectimer(priv->timeout_backup);
        } else {
            priv->t_backup = 0;
        }

    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "source unknown",
            "src",          "%s", gobj_full_name(src),
            NULL
        );
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Disconnected
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(src == priv->gobj_bottom_side) {
        clear_timeout(priv->timer);     // Active only when bottom side is open
        priv->bottom_side_opened = FALSE;
        reset_soft_queue(gobj); // Resetea los timeout_ack y los MARK_PENDING_ACK
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "source unknown",
            "src",          "%s", gobj_full_name(src),
            NULL
        );
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_bottom_side) {
        return process_ack(gobj, event, kw, src);
    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
        "msg",          "%s", "source unknown",
        "src",          "%s", gobj_full_name(src),
        NULL
    );
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    q_msg_t *msg = enqueue_message(
        gobj,
        kw  // not owned
    );

    if(msg) {
        if(priv->bottom_side_opened) {
            send_batch_messages(gobj, msg); // Send the received msg
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->bottom_side_opened) {
        send_batch_messages(gobj, 0); // Resend by periodic timeout
    }

    if(priv->timeout_backup > 0 && test_sectimer(priv->t_backup)) {
        if(trq_size(priv->trq_msgs)==0 && priv->pending_acks==0) {
            // Check and do backup only when no message
            trq_check_backup(priv->trq_msgs);
        }
        priv->t_backup = start_sectimer(priv->timeout_backup);
    }

    set_timeout(priv->timer, priv->timeout_poll);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  This event comes from clisrv TCP gobjs
 *  that haven't found a free server link.
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
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
    .mt_create = mt_create,
    .mt_destroy = mt_destroy,
    .mt_writing = mt_writing,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_stats = mt_stats,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_QIOGATE);

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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
        {EV_SEND_MESSAGE,       ac_send_message,        0},
        {EV_ON_MESSAGE,         ac_on_message,          0},
        {EV_ON_OPEN,            ac_on_open,             0},
        {EV_ON_CLOSE,           ac_on_close,            0},
        {EV_TIMEOUT,            ac_timeout,             0},
        {EV_STOPPED,            ac_stopped,             0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_SEND_MESSAGE,       0},
        {EV_ON_MESSAGE,         0},
        {EV_ON_OPEN,            0},
        {EV_ON_CLOSE,           0},
        // bottom input
        {EV_TIMEOUT,            0},
        {EV_STOPPED,            0},
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
        command_table,  // command_table,
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
PUBLIC int register_c_qiogate(void)
{
    return create_gclass(C_QIOGATE);
}
