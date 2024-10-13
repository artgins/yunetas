/***********************************************************************
 *          C_QIOGATE.C
 *          Qiogate GClass.
 *
 *          IOGate with persistent queue
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
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
PRIVATE json_t *cmd_reset_maxtxrx(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_OCTET_STR, "cmd",          0,              0,          "command about you want help."),
SDATAPM (ASN_UNSIGNED,  "level",        0,              0,          "command search level in childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_queue[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_OCTET_STR, "key",          0,              0,          "Key"),
SDATAPM (ASN_INTEGER64, "from-rowid",   0,              0,          "From rowid"),
SDATAPM (ASN_INTEGER64, "to-rowid",     0,              0,          "To rowid"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (ASN_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,       "Command's help"),

SDATACM (ASN_SCHEMA,    "reset_maxtxrx",        0,          0,              cmd_reset_maxtxrx, "Reset max tx rx stats"),
SDATACM (ASN_SCHEMA,    "queue_mark_pending",   0,          pm_queue,       cmd_queue_mark_pending, "Mark selected messages as pending (Will be resend)."),
SDATACM (ASN_SCHEMA,    "queue_mark_notpending", 0,         pm_queue,       cmd_queue_mark_notpending,"Mark selected messages as notpending (Will NOT be send or resend)."),
SDATA_END()
};


/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag------------------------default---------description---------- */
SDATA (ASN_COUNTER64,   "txMsgs",           SDF_RD|SDF_RSTATS,  0,          "Messages transmitted"),
SDATA (ASN_COUNTER64,   "rxMsgs",           SDF_RD|SDF_RSTATS,  0,          "Messages receiveds"),

SDATA (ASN_COUNTER64,   "txMsgsec",         SDF_RD|SDF_RSTATS,  0,          "Messages by second"),
SDATA (ASN_COUNTER64,   "rxMsgsec",         SDF_RD|SDF_RSTATS,  0,          "Messages by second"),
SDATA (ASN_COUNTER64,   "maxtxMsgsec",      SDF_RD|SDF_RSTATS,  0,          "Max Messages by second"),
SDATA (ASN_COUNTER64,   "maxrxMsgsec",      SDF_RD|SDF_RSTATS,  0,          "Max Messages by second"),

SDATA (ASN_INTEGER,     "timeout_poll",     SDF_RD,             1*1000,     "Timeout polling, in miliseconds"),
SDATA (ASN_UNSIGNED,    "msgs_in_queue",    SDF_RD|SDF_STATS,   0,          "Messages in queue"),
SDATA (ASN_UNSIGNED,    "pending_acks",     SDF_RD|SDF_STATS,   0,          "Messages pending of ack"),

SDATA (ASN_OCTET_STR,   "tranger_path",     SDF_RD,             "",         "tranger path"),
SDATA (ASN_OCTET_STR,   "tranger_database", SDF_RD,             "",         "tranger database"),
SDATA (ASN_OCTET_STR,   "topic_name",       SDF_RD,             "",         "trq_open topic_name"),
SDATA (ASN_OCTET_STR,   "pkey",             SDF_RD,             "",         "trq_open pkey"),
SDATA (ASN_OCTET_STR,   "tkey",             SDF_RD,             "",         "trq_open tkey"),
SDATA (ASN_OCTET_STR,   "system_flag",      SDF_RD,             "",         "trq_open system_flag"),
SDATA (ASN_UNSIGNED,    "on_critical_error",SDF_RD,             LOG_OPT_TRACE_STACK, "tranger parameter"),
SDATA (ASN_OCTET_STR,   "alert_message",    SDF_WR|SDF_PERSIST, "ALERTA Encolamiento", "Alert message"),
SDATA (ASN_UNSIGNED,    "max_pending_acks", SDF_WR|SDF_PERSIST, 1,          "Maximum messages pending of ack"),
SDATA (ASN_UNSIGNED,    "maximum_retries",  SDF_WR|SDF_PERSIST, 0,          "Maximum tx retries per message"),
SDATA (ASN_UNSIGNED64,  "backup_queue_size",SDF_WR|SDF_PERSIST, 1*1000000,  "Do backup at this size"),
SDATA (ASN_INTEGER,     "alert_queue_size", SDF_WR|SDF_PERSIST, 2*1000,     "Limit alert queue size"),
SDATA (ASN_INTEGER,     "timeout_ack",      SDF_WR|SDF_PERSIST, 60,         "Timeout ack in seconds"),
SDATA (ASN_BOOLEAN,     "drop_on_timeout_ack",SDF_WR|SDF_PERSIST, 1,        "On ack timeout => True: drop connection, False: resend message"),

SDATA (ASN_BOOLEAN,     "with_metadata",    SDF_RD,             0,          "Don't filter metadata"),
SDATA (ASN_BOOLEAN,     "disable_alert",    SDF_WR|SDF_PERSIST, 0,          "Disable alert"),
SDATA (ASN_OCTET_STR,   "alert_from",       SDF_WR,             "",         "Alert from"),
SDATA (ASN_OCTET_STR,   "alert_to",         SDF_WR|SDF_PERSIST, "",         "Alert destination"),

SDATA (ASN_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (ASN_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (ASN_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
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
    int32_t timeout_poll;
    int32_t timeout_ack;
    hgobj timer;

    hgobj gobj_tranger_queues;
    json_t *tranger;
    tr_queue trq_msgs;
    int32_t alert_queue_size;
    BOOL with_metadata;

    hgobj gobj_bottom_side;
    BOOL bottom_side_opened;

    uint32_t *ppending_acks;
    uint32_t max_pending_acks;

    uint64_t *ptxMsgs;
    uint64_t *prxMsgs;
    uint64_t txMsgsec;
    uint64_t rxMsgsec;
    BOOL drop_on_timeout_ack;
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

    priv->timer = gobj_create("", GCLASS_TIMER, 0, gobj);
    priv->ptxMsgs = gobj_danger_attr_ptr(gobj, "txMsgs");
    priv->prxMsgs = gobj_danger_attr_ptr(gobj, "rxMsgs");
    priv->ppending_acks = gobj_danger_attr_ptr(gobj, "pending_acks");

    gobj_set_qs(QS_LOWER_RESPONSE_TIME, -1);
    gobj_set_qs(QS_MEDIUM_RESPONSE_TIME, 0);
    gobj_set_qs(QS_HIGHER_RESPONSE_TIME, 0);

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout_poll,              gobj_read_int32_attr)
    SET_PRIV(timeout_ack,               gobj_read_int32_attr)
    SET_PRIV(with_metadata,             gobj_read_bool_attr)
    SET_PRIV(alert_queue_size,          gobj_read_int32_attr)
    SET_PRIV(max_pending_acks,          gobj_read_uint32_attr)
    SET_PRIV(drop_on_timeout_ack,       gobj_read_bool_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout_poll,            gobj_read_int32_attr)
    ELIF_EQ_SET_PRIV(timeout_ack,           gobj_read_int32_attr)
    ELIF_EQ_SET_PRIV(alert_queue_size,      gobj_read_int32_attr)
    ELIF_EQ_SET_PRIV(max_pending_acks,      gobj_read_uint32_attr)
    ELIF_EQ_SET_PRIV(drop_on_timeout_ack,   gobj_read_bool_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method reading
 ***************************************************************************/
PRIVATE SData_Value_t mt_reading(hgobj gobj, const char *name, int type, SData_Value_t data)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(strcmp(name, "msgs_in_queue")==0) {
        data.u32 = trq_size(priv->trq_msgs);
    }
    return data;
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
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "timeout_ack EMPTY",
            NULL
        );
        priv->timeout_ack = 60;
    }

    /*--------------------------------*
     *      Ouput queue
     *--------------------------------*/
    open_queue(gobj);
    trq_load(priv->trq_msgs);
    //gobj_set_qs(QS_INTERNAL_QUEUE, trq_size(priv->trq_msgs)); // TODO gestiona colas múltiples

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
PRIVATE json_t *cmd_reset_maxtxrx(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    gobj_write_uint64_attr(gobj, "maxtxMsgsec", 0);
    gobj_write_uint64_attr(gobj, "maxrxMsgsec", 0);
    return msg_iev_build_webix(
        gobj,
        0,
        json_sprintf("Max tx rx reset done."),
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
        return msg_iev_build_webix(
            gobj,
            0,
            json_sprintf("you must PAUSE the yuno before executing this command."),
            0,
            0,
            kw  // owned
        );
    }

    const char *key = kw_get_str(kw, "key", 0, 0);
    int64_t from_rowid = kw_get_int(kw, "from-rowid", 0, KW_WILD_NUMBER);
    int64_t to_rowid = kw_get_int(kw, "to-rowid", 0, KW_WILD_NUMBER);
    if(from_rowid == 0) {
        return msg_iev_build_webix(
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
    trq_set_first_rowid(priv->trq_msgs, 0);
    trq_load_all(priv->trq_msgs, key, from_rowid, to_rowid);

    /*----------------------------------*
     *      Mark pending
     *----------------------------------*/
    int count = 0;
    q_msg msg;
    qmsg_foreach_forward(priv->trq_msgs, msg) {
        count++;
        trq_set_hard_flag(msg, TRQ_MSG_PENDING, 1);
    }
    /*
     *  Close que queue
     */
    close_queue(gobj);


    return msg_iev_build_webix(
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
        return msg_iev_build_webix(
            gobj,
            0,
            json_sprintf("you must PAUSE the yuno before executing this command."),
            0,
            0,
            kw  // owned
        );
    }

    const char *key = kw_get_str(kw, "key", 0, 0);
    int64_t from_rowid = kw_get_int(kw, "from-rowid", 0, KW_WILD_NUMBER);
    int64_t to_rowid = kw_get_int(kw, "to-rowid", 0, KW_WILD_NUMBER);
    if(from_rowid == 0) {
        return msg_iev_build_webix(
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
    trq_set_first_rowid(priv->trq_msgs, 0);
    trq_load_all(priv->trq_msgs, key, from_rowid, to_rowid);

    /*----------------------------------*
     *      Unmark pending
     *----------------------------------*/
    int count = 0;
    q_msg msg;
    qmsg_foreach_forward(priv->trq_msgs, msg) {
        count++;
        trq_set_hard_flag(msg, TRQ_MSG_PENDING, 0);
    }

    /*
     *  Close que queue
     */
    close_queue(gobj);

    return msg_iev_build_webix(
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
        log_error(0,
            "gobj",         "%s", gobj_yuno_role_plus_name(),
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

    GBUFFER *gbuf = gbuf_create(strlen(message), strlen(message), 0, 0);
    gbuf_append_string(gbuf, message);

    json_object_set_new(kw_email,
        "gbuffer",
        json_integer((json_int_t)(size_t)gbuf)
    );
    gobj_send_event(gobj_emailsender, "EV_SEND_EMAIL", kw_email, gobj_default_service());
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int open_queue(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->tranger) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "tranger NOT NULL",
            NULL
        );
        tranger_shutdown(priv->tranger);
    }

    const char *path = gobj_read_str_attr(gobj, "tranger_path");
    if(empty_string(path)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "tranger path EMPTY",
            NULL
        );
        return -1;
    }

    const char *database = gobj_read_str_attr(gobj, "tranger_database");
    if(empty_string(database)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "tranger database EMPTY",
            NULL
        );
        return -1;
    }

    const char *topic_name = gobj_read_str_attr(gobj, "topic_name");
    if(empty_string(topic_name)) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
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
        "on_critical_error", (int)gobj_read_uint32_attr(gobj, "on_critical_error")
    );
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "tranger_%s", gobj_name(gobj));
    priv->gobj_tranger_queues = gobj_create_service(
        name,
        GCLASS_TRANGER,
        kw_tranger,
        gobj
    );
    gobj_start(priv->gobj_tranger_queues);
    priv->tranger = gobj_read_pointer_attr(priv->gobj_tranger_queues, "tranger");

    priv->trq_msgs = trq_open(
        priv->tranger,
        topic_name,
        gobj_read_str_attr(gobj, "pkey"),
        gobj_read_str_attr(gobj, "tkey"),
        tranger_str2system_flag(gobj_read_str_attr(gobj, "system_flag")),
        gobj_read_uint64_attr(gobj, "backup_queue_size")
    );

    trq_set_maximum_retries(priv->trq_msgs, gobj_read_uint32_attr(gobj, "maximum_retries"));

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
PRIVATE q_msg enqueue_message(
    hgobj gobj,
    json_t *kw  // not owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(!priv->trq_msgs) {
        log_critical(LOG_OPT_ABORT|LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
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
        kw_clean_clone = kw_filter_metadata(kw);
    } else {
        kw_clean_clone = kw_incref(kw);
    }
    q_msg msg = trq_append(
        priv->trq_msgs,
        kw_clean_clone
    );
    if(!msg) {
        log_critical(LOG_OPT_ABORT|LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Message NOT SAVED in the queue",
            NULL
        );
        return 0;
    }

    if(!gobj_read_bool_attr(gobj, "disable_alert")) {
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
                    get_host_name(),
                    gobj_yuno_role_plus_name(),
                    gobj_read_str_attr(gobj, "tranger_database"),
                    gobj_node_owner()
                );
                snprintf(
                    alert,
                    sizeof(alert),
                    "%s %ld msgs, node '%s', yuno '%s' %s\nUTC %s, LOCAL %s",
                    gobj_read_str_attr(gobj, "alert_message"), //"ALERTA Encolamiento",
                    (unsigned long)trq_size(priv->trq_msgs),
                    get_host_name(),
                    gobj_yuno_role_plus_name(),
                    gobj_read_str_attr(gobj, "tranger_database"),
                    utc_stamp, local_stamp
                );
                log_error(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_QUEUE_ALARM,
                    "msg",          "%s", subject,
                    "node",         "%s", get_host_name(),
                    "yuno",         "%s", gobj_yuno_role_plus_name(),
                    "owner",        "%s", gobj_node_owner(),
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

    q_msg msg;
    qmsg_foreach_forward(priv->trq_msgs, msg) {
        trq_set_soft_mark(msg, MARK_PENDING_ACK, FALSE);
        trq_set_ack_timer(msg, 0);
    }

    (*priv->ppending_acks) = 0;

    return 0;
}

/***************************************************************************
 *  Send message to bottom side
 ***************************************************************************/
PRIVATE int send_message_to_bottom_side(hgobj gobj, q_msg msg)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint64_t rowid = trq_msg_rowid(msg);
    json_t *jn_msg = trq_msg_json(msg);

    json_t *kw_clone = kw_duplicate(jn_msg);
    trq_set_metadata(kw_clone, "__msg_key__", json_integer(rowid));

    if(gobj_trace_level(gobj) & (TRACE_MESSAGES|TRACE_QUEUE_PROT)) {
        trace_msg("QIOGATE send %s ==> %s, %"PRIu64,
            gobj_short_name(gobj),
            gobj_short_name(priv->gobj_bottom_side),
            (uint64_t)rowid
        );
    }

    json_t *kw_send = json_pack("{s:I}",
        "gbuffer", (json_int_t)(size_t)json2gbuf(0, kw_clone, JSON_COMPACT)
    );
    return gobj_send_event(priv->gobj_bottom_side, "EV_SEND_MESSAGE", kw_send, gobj);
}

/***************************************************************************
 *  Send batch of messages to bottom side
 *  ``id``  is 0 when calling from timer
 *          or not 0 when calling from receiving message
 *  // reenvia al abrir la salida
 *  // reenvia al recibir (único con id):   -> (*) ->
 *  // reenvia al recibir ack:                 (*) <->
 *  // reenvia por timeout periódico           (*) ->
 ***************************************************************************/
PRIVATE int send_batch_messages(hgobj gobj, q_msg msg, BOOL retransmit)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    void *(*discard_message)(hgobj gobj, q_msg msg) = 0;

    if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
//        trace_msg("======BATCH======> %s - msg %s, retransmit %s", gobj_name(gobj), msg?"True":"False", retransmit?"True":"False");
    }

    /*------------------------------*
     *      Sending one message
     *------------------------------*/
    if(msg) {
        uint64_t rowid = trq_msg_rowid(msg);
        uint64_t t = trq_msg_time(msg);
        if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
            trace_msg("  -> (1)     - rowid %"PRIu64", t %"PRIu64", pending_acks %d", rowid, t, *priv->ppending_acks);
        }

        /*
         *  Check: no send if the previous to last has no MARK_PENDING_ACK
         *  (the last is this message)
         */
        q_msg last_msg = trq_last_msg(priv->trq_msgs);
        q_msg prev_last_msg = trq_prev_msg(last_msg);
        if(prev_last_msg) {
            if(!(trq_get_soft_mark(prev_last_msg) & MARK_PENDING_ACK)) {
                if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                    trace_msg("     (1) xxx - rowid %"PRIu64", t %"PRIu64"", rowid, t);
                }
                return 0; // Previous message has not sent
            }
        }

        if((*priv->ppending_acks) < priv->max_pending_acks) {
            if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                trace_msg("     (1) ->  - rowid %"PRIu64", t %"PRIu64"", rowid, t);
            }
            int ret = send_message_to_bottom_side(gobj, msg);
            if(ret == 0) {
                (*priv->ptxMsgs)++;
                priv->txMsgsec++;
                gobj_incr_qs(QS_TXMSGS, 1);

                (*priv->ppending_acks)++;
                trq_set_soft_mark(msg, MARK_PENDING_ACK, TRUE);
                trq_set_ack_timer(msg, priv->timeout_ack);
                return 1; // Sent one message

            } else {
                // Error sending the message
                if(discard_message) { // TODO like georeverse
                    if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                        trace_msg("     (1) xx!  - rowid %"PRIu64", t %"PRIu64"", rowid, t);
                    }
                    discard_message(gobj, msg);
                    return 0;
                } else {
                    if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                        trace_msg("     (1) xx  - rowid %"PRIu64", t %"PRIu64"", rowid, t);
                    }
                    return -1;
                }
            }
        } else {
            // Max pending reached
            if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                trace_msg("     (1) x   - rowid %"PRIu64", t %"PRIu64"", rowid, t);
            }
            return 0;
        }
    }

    /*----------------------------------*
     *      Sending batch messages
     *----------------------------------*/
    int sent = 0;
    q_msg n;
    qmsg_foreach_forward_safe(priv->trq_msgs, msg, n) {
        uint64_t rowid = trq_msg_rowid(msg);
        uint64_t t = trq_msg_time(msg);

        if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
            trace_msg("     ( )     - rowid %"PRIu64", t %"PRIu64", pending_acks %d", rowid, t, *priv->ppending_acks);
        }

        if((trq_get_soft_mark(msg) & MARK_PENDING_ACK)) {
            /*
             *  Resend msgs with MARK_PENDING_ACK and timer fulfilled
             */
            if(!retransmit) {
                if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                    trace_msg("     (X)     - rowid %"PRIu64", t %"PRIu64", sent %d", rowid, t, sent);
                }
                continue; // Opportunity for Send new msgs without MARK_PENDING_ACK
            }
            if(trq_test_ack_timer(msg)) {
                /*
                 *  En conexiones con respuesta asegurada (tcp con ack) el timeout sin respuesta
                 *  deber provocar un drop!!
                 *
                 *  EN conexiones sin respuesta asegurada,
                 *  pero ojo solo con procesos asíncronos verificados,
                 *  SÍ que se puede usar reenvío. Lo suyo, con ack inteligente,
                 *  adaptado a los tiempos de respuesta del peer, en tiempo real.
                 */
                // Timeout reached
                trq_add_retries(msg, 1);
                if(trq_test_retries(msg)) {
                    // No more retries
                    if(discard_message) {
                        if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                            trace_msg("     (+) XX  - rowid %"PRIu64", t %"PRIu64"", rowid, t);
                        }
                        discard_message(gobj, msg);
                    } else {
                        log_error(0,
                            "gobj",         "%s", gobj_yuno_role_plus_name(),
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                            "msg",          "%s", "queue with max retries but without discard method",
                            "rowid",        "%ld", (unsigned long)rowid,
                            NULL
                        );
                        if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                            trace_msg("     (+) XX! - rowid %"PRIu64", t %"PRIu64"", rowid, t);
                        }
                    }
                } else {
                    // Timeout reached, resend, infinity retry or max retry not reached
                    gobj_incr_qs(QS_REPEATED, 1);

                    if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                        trace_msg("     (+) ->  - rowid %"PRIu64", t %"PRIu64"", rowid, t);
                    }
                    int ret = send_message_to_bottom_side(gobj, msg);
                    if (ret == 0) {
                        trq_set_ack_timer(msg, priv->timeout_ack);
                        sent++;
                    } else {
                        if(discard_message) {
                            if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                                trace_msg("     (+) xx  - rowid %"PRIu64", t %"PRIu64"", rowid, t);
                            }
                            discard_message(gobj, msg);
                        } else {
                            if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                                trace_msg("     (+) xx! - rowid %"PRIu64", t %"PRIu64"", rowid, t);
                            }
                            return -1;
                        }
                    }
                }
            }
        } else {
            /*
             *  Send new msgs without MARK_PENDING_ACK
             */
            if((*priv->ppending_acks) < priv->max_pending_acks) {
                if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                    trace_msg("     (-) ->  - rowid %"PRIu64", t %"PRIu64"", rowid, t);
                }
                int ret = send_message_to_bottom_side(gobj, msg);
                if(ret == 0) {
                    sent++;
                    (*priv->ptxMsgs)++;
                    priv->txMsgsec++;
                    gobj_incr_qs(QS_TXMSGS, 1);

                    (*priv->ppending_acks)++;
                    trq_set_soft_mark(msg, MARK_PENDING_ACK, TRUE);
                    trq_set_ack_timer(msg, priv->timeout_ack);
                } else {
                    if(discard_message) {
                        if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                            trace_msg("     (-) xx  - rowid %"PRIu64", t %"PRIu64"", rowid, t);
                        }
                        discard_message(gobj, msg);
                    } else {
                        if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                            trace_msg("     (-) xx! - rowid %"PRIu64", t %"PRIu64"", rowid, t);
                        }
                        return -1;
                    }
                }
            } else {
                // Max pending reached
                if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
                    trace_msg("     (-) x   - rowid %"PRIu64", t %"PRIu64"", rowid, t);
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
    uint64_t rowid,
    int result
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    q_msg msg = trq_get_by_rowid(priv->trq_msgs, rowid);
    if(msg) {
        uint64_t tt = trq_msg_time(msg);
        if(gobj_trace_level(gobj) & TRACE_QUEUE_PROT) {
            trace_msg("     ( ) <-  - rowid %"PRIu64", t %"PRIu64" ACK", rowid, tt);
        }

        uint64_t t = time_in_seconds() - tt;

        uint64_t t_lower = gobj_get_qs(QS_LOWER_RESPONSE_TIME);
        uint64_t t_medium = gobj_get_qs(QS_MEDIUM_RESPONSE_TIME);
        uint64_t t_higher = gobj_get_qs(QS_HIGHER_RESPONSE_TIME);
        if(t < t_lower) {
            gobj_set_qs(QS_LOWER_RESPONSE_TIME, t);
        }
        if(t > t_higher) {
            gobj_set_qs(QS_HIGHER_RESPONSE_TIME, t);
        }
        t = (t + t_medium)/2;
        gobj_set_qs(QS_MEDIUM_RESPONSE_TIME, t);

        if((trq_get_soft_mark(msg) & MARK_PENDING_ACK)) {
            trq_set_soft_mark(msg, MARK_PENDING_ACK, FALSE);

            if (*priv->ppending_acks > 0) {
                (*priv->ppending_acks)--;
            } else {
                log_error(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "ppending_acks ZERO or NEGATIVE",
                    "pending_acks", "%ld", (unsigned long) priv->ppending_acks,
                    NULL
                );
            }
        }

        trq_clear_ack_timer(msg);
        trq_unload_msg(msg, result);


    } else {
        if(trq_check_pending_rowid(priv->trq_msgs, rowid)!=0) {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
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
PRIVATE int process_ack(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBUFFER *gbuf = (GBUFFER *)(size_t)kw_get_int(kw, "gbuffer", 0, 0);

    gbuf_incref(gbuf);
    json_t *jn_ack_message = gbuf2json(gbuf, 2);

    uint64_t rowid = kw_get_int(trq_get_metadata(jn_ack_message), "__msg_key__", 0, KW_REQUIRED);
    int result = kw_get_int(trq_get_metadata(jn_ack_message), "result", 0, KW_REQUIRED);

    if(gobj_trace_level(gobj) & (TRACE_MESSAGES|TRACE_QUEUE_PROT)) {
        trace_msg("QIOGATE ack %s <== %s, %"PRIu64,
            gobj_short_name(gobj),
            gobj_short_name(priv->gobj_bottom_side),
            rowid
        );
    }

    dequeue_msg(gobj, rowid, result);

    JSON_DECREF(jn_ack_message)
    KW_DECREF(kw)

    if(priv->bottom_side_opened) {
        send_batch_messages(gobj, 0, FALSE); // try to send more messages after receiving ack
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
        send_batch_messages(gobj, 0, TRUE);  // On open send or resend
        set_timeout_periodic(priv->timer, priv->timeout_poll);
    } else {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
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
        json_int_t channels_opened = kw_get_int(kw, "__temp__`channels_opened", 0, KW_REQUIRED);
        if(channels_opened==0) {
            clear_timeout(priv->timer);     // Active only when bottom side is open
            priv->bottom_side_opened = FALSE;
            reset_soft_queue(gobj); // Resetea los timeout_ack y los MARK_PENDING_ACK
        }
    } else {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
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
        (*priv->prxMsgs)++;
        priv->rxMsgsec++;
        gobj_incr_qs(QS_RXMSGS, 1);
        return process_ack(gobj, event, kw, src);
    }

    log_error(0,
        "gobj",         "%s", gobj_full_name(gobj),
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

    q_msg msg = enqueue_message(
        gobj,
        kw  // not owned
    );

    if(msg) {
        if(priv->bottom_side_opened) {
            send_batch_messages(gobj, msg, FALSE); // Send the received msg
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

    uint64_t maxtxMsgsec = gobj_read_uint64_attr(gobj, "maxtxMsgsec");
    uint64_t maxrxMsgsec = gobj_read_uint64_attr(gobj, "maxrxMsgsec");
    if(priv->txMsgsec > maxtxMsgsec) {
        gobj_write_uint64_attr(gobj, "maxtxMsgsec", priv->txMsgsec);
    }
    if(priv->rxMsgsec > maxrxMsgsec) {
        gobj_write_uint64_attr(gobj, "maxrxMsgsec", priv->rxMsgsec);
    }

    gobj_write_uint64_attr(gobj, "txMsgsec", priv->txMsgsec);
    gobj_write_uint64_attr(gobj, "rxMsgsec", priv->rxMsgsec);

    priv->rxMsgsec = 0;
    priv->txMsgsec = 0;

    if(priv->bottom_side_opened) {
        int ret = send_batch_messages(gobj, 0, TRUE); // Resend by periodic timeout
        if(ret < 0) {
            if(priv->drop_on_timeout_ack) {
                if(gobj_trace_level(gobj) & (TRACE_MESSAGES|TRACE_QUEUE_PROT)) {
                    trace_msg("QIOGATE drop %s", gobj_short_name(priv->gobj_bottom_side));
                }
                log_error(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "Dropping by timeout ack",
                    "gobj_bottom",  "%s", gobj_short_name(priv->gobj_bottom_side),
                    NULL
                );

                gobj_send_event(priv->gobj_bottom_side, "EV_DROP", 0, gobj);
            }
        }
    }

    if(trq_size(priv->trq_msgs)==0 && *priv->ppending_acks==0) {
        // Check and do backup only when no message
        trq_check_backup(priv->trq_msgs);
    }

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
PRIVATE const EVENT input_events[] = {
    // top input
    {"EV_ON_MESSAGE",   0,   0,  ""},
    {"EV_SEND_MESSAGE", 0,   0,  ""},
    {"EV_ON_OPEN",      0,   0,  ""},
    {"EV_ON_CLOSE",     0,   0,  ""},
    // bottom input
    {"EV_TIMEOUT",      0,  0,  0},
    {"EV_STOPPED",      0,  0,  0},
    // internal
    {NULL, 0, 0, 0}
};
PRIVATE const EVENT output_events[] = {
    {NULL, 0, 0, 0}
};
PRIVATE const char *state_names[] = {
    "ST_IDLE",
    NULL
};

PRIVATE EV_ACTION ST_IDLE[] = {
    {"EV_SEND_MESSAGE",         ac_send_message,        0},
    {"EV_ON_MESSAGE",           ac_on_message,          0},
    {"EV_ON_OPEN",              ac_on_open,             0},
    {"EV_ON_CLOSE",             ac_on_close,            0},
    {"EV_STOPPED",              ac_stopped,             0},
    {"EV_TIMEOUT",              ac_timeout,             0},
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_IDLE,
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
    GCLASS_QIOGATE_NAME,
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
        mt_reading,
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
    0,  // gcflag
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC GCLASS *gclass_qiogate(void)
{
    return &_gclass;
}
