/****************************************************************************
 *          TR2Q_MQTT.C
 *
 *          Defines and functions for tr2_queue md2_record_ex_t.user_flag
 *          and manage Persistent mqtt queues based in TimeRanger
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "tr2q_mqtt.h"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void free_msg(void *msg_);
/**
    Mark a message.
    You must flag a message with TR2Q_MSG_PENDING after append it to queue
        if you want to recover it in the next open
        with the flag used in tr2q_load()
*/
PRIVATE int tr2q_set_hard_flag(q2_msg_t *msg, uint16_t hard_mark, BOOL set);

/***************************************************************
 *              Data
 ***************************************************************/

/********************************************************************
 * String Functions
 ********************************************************************/
const char *msg_flag_state_to_str(mqtt_msg_state_t state)
{
    switch (state) {
        case mosq_ms_invalid:
            return "invalid";
        case mosq_ms_publish_qos0:
            return "publish_qos0";
        case mosq_ms_publish_qos1:
            return "publish_qos1";
        case mosq_ms_wait_for_puback:
            return "wait_for_puback";
        case mosq_ms_publish_qos2:
            return "publish_qos2";
        case mosq_ms_wait_for_pubrec:
            return "wait_for_pubrec";
        case mosq_ms_resend_pubrel:
            return "resend_pubrel";
        case mosq_ms_wait_for_pubrel:
            return "wait_for_pubrel";
        case mosq_ms_resend_pubcomp:
            return "resend_pubcomp";
        case mosq_ms_wait_for_pubcomp:
            return "wait_for_pubcomp";
        case mosq_ms_send_pubrec:
            return "send_pubrec";
        case mosq_ms_queued:
            return "queued";
        default:
            return "unknown";
    }
}

/********************************************************************
 * String Functions
 ********************************************************************/
const char *msg_flag_direction_to_str(mqtt_msg_direction_t dir)
{
    switch (dir) {
        case mosq_md_in:
            return "IN";
        case mosq_md_out:
            return "OUT";
        default:
            return "";
    }
}

/********************************************************************
 * String Functions
 ********************************************************************/
const char *msg_flag_origin_to_str(mqtt_msg_origin_t orig)
{
    switch (orig) {
        case mosq_mo_client:
            return "Client";
        case mosq_mo_broker:
            return "Broker";
        default:
            return "None";
    }
}

/********************************************************************
 *  Build queue name
 ********************************************************************/
PUBLIC int build_queue_name(
    char *bf,
    size_t bfsize,
    const char *client_id,
    mqtt_msg_direction_t mqtt_msg_direction
) {
    return snprintf(
        bf,
        bfsize,
        "%s%s%s",
        client_id,
        mqtt_msg_direction?"-":"",
        msg_flag_direction_to_str(mqtt_msg_direction)
    );
}

/***************************************************************************
    Open queue
 ***************************************************************************/
PUBLIC tr2_queue_t *tr2q_open(
    json_t *tranger,
    const char *topic_name,
    const char *tkey,
    system_flag2_t system_flag, // KEY_TYPE_MASK forced to sf_rowid_key
    size_t max_inflight_messages,
    size_t backup_queue_size
) {
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*-------------------------------*
     *  Alloc memory
     *-------------------------------*/
    tr2_queue_t *trq = GBMEM_MALLOC(sizeof(tr2_queue_t));
    if(!trq) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "Cannot create tr_queue. GBMEM_MALLOC() FAILED",
            NULL
        );
        return NULL;
    }
    trq->tranger = tranger;
    trq->max_inflight_messages = max_inflight_messages;
    snprintf(trq->topic_name, sizeof(trq->topic_name), "%s", topic_name);

    json_t *jn_topic_ext = json_object();
    json_object_set_new(jn_topic_ext, "filename_mask", json_string("queue"));

    system_flag &= ~KEY_TYPE_MASK2;
    system_flag |= sf_rowid_key;

    /*-------------------------------*
     *  Open/Create topic
     *-------------------------------*/
    trq->topic = tranger2_create_topic( // IDEMPOTENT function
        trq->tranger,
        topic_name,
        "",
        tkey,
        jn_topic_ext,
        system_flag,
        0,
        0
    );
    if(!trq->topic) {
        // Error already logged
        tr2q_close(trq);
        return NULL;
    }
    dl_init(&trq->dl_inflight, 0);
    dl_init(&trq->dl_queued, 0);

    if(backup_queue_size > 0 && kw_get_bool(gobj, trq->tranger, "master", 0, KW_REQUIRED)) {
        json_t *jn_topic_var = json_object();
        json_object_set_new(
            jn_topic_var,
            "backup_queue_size",
            json_integer((json_int_t)backup_queue_size)
        );
        tranger2_write_topic_var(
            trq->tranger,
            topic_name,
            jn_topic_var  // owned
        );
    }

    return trq;
}

/***************************************************************************
    Close queue (After close the queue remember tranger2_shutdown())
 ***************************************************************************/
PUBLIC void tr2q_close(tr2_queue_t *trq)
{
    dl_flush(&((tr2_queue_t *)trq)->dl_inflight, free_msg);
    dl_flush(&((tr2_queue_t *)trq)->dl_queued, free_msg);
    GBMEM_FREE(trq);
}

/***************************************************************************
    Set first rowid to search
 ***************************************************************************/
PRIVATE void tr2q_set_first_rowid(tr2_queue_t *trq, uint64_t first_rowid)
{
    hgobj gobj = 0;

    trq->first_rowid = first_rowid;

    if(kw_get_bool(gobj, trq->tranger, "master", 0, KW_REQUIRED)) {
        json_t *jn_topic_var = json_object();
        json_object_set_new(
            jn_topic_var,
            "first_rowid",
            json_integer((json_int_t)first_rowid)
        );
        // first_rowid will be set in trq->topic too
        tranger2_write_topic_var(
            trq->tranger,
            tranger2_topic_name(trq->topic),
            jn_topic_var  // owned
        );
    }
}

/***************************************************************************
    New msg in memory
 ***************************************************************************/
PRIVATE q2_msg_t *new_msg(
    tr2_queue_t *trq,
    json_int_t rowid, // global rowid that it must match the rowid in md_record
    uint16_t mid,
    const md2_record_ex_t *md_record,
    json_t *kw_record // owned
) {
    hgobj gobj = 0;

    /*
     *  Alloc memory
     */
    q2_msg_t *msg = GBMEM_MALLOC(sizeof(q2_msg_t));
    if(!msg) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "Cannot create msg. GBMEM_MALLOC() FAILED",
            NULL
        );
        KW_DECREF(kw_record)
        return NULL;
    }
    if(rowid != md_record->rowid) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "rowid NOT MATCH",
            "rowid",        "%d", (int)rowid,
            "md_rowid",     "%d", (int)md_record->rowid,
            NULL
        );
    }
    memmove(&msg->md_record, md_record, sizeof(md2_record_ex_t));
    msg->trq = trq;
    msg->mid = mid;
    msg->rowid = rowid;

    if(trq->max_inflight_messages == 0 || tr2q_inflight_size(trq) < trq->max_inflight_messages) {
        dl_add(&trq->dl_inflight, msg);
        msg->kw_record = kw_record;
        msg->inflight = TRUE;
    } else {
        dl_add(&trq->dl_queued, msg);
        msg->inflight = FALSE;
        KW_DECREF(kw_record)
    }

    return msg;
}

/***************************************************************************
    Free msg
 ***************************************************************************/
PRIVATE void free_msg(void *msg_)
{
    q2_msg_t *msg = msg_;
    KW_DECREF(msg->kw_record)
    memset(msg, 0, sizeof(q2_msg_t));
    GBMEM_FREE(msg);
}

/***************************************************************************

 ***************************************************************************/
PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list, // iterator or rt_list/rt_disk id, don't own
    json_int_t rowid,   // global rowid of key
    md2_record_ex_t *md_record,
    json_t *jn_record // must be owned, can be null if only_md
) {
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    tr2_queue_t *trq = (tr2_queue_t *)(uintptr_t)kw_get_int(gobj, list, "trq", 0, KW_REQUIRED);

    if(trq->first_rowid==0) {
        // The first record called is the first pending record
        trq->first_rowid = rowid;
    }
    uint16_t mid = kw_get_int(gobj, jn_record, "mid", 0, KW_REQUIRED);
    new_msg(trq, rowid, mid, md_record, jn_record);

    return 0;
}

PUBLIC int tr2q_load(tr2_queue_t *trq)
{
    hgobj gobj = 0;

    if(!trq) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "trq NULL",
            NULL
        );
        return -1;
    }

    json_t *match_cond = json_object();
    json_object_set_new(
        match_cond,
        "user_flag_mask_set",
        json_integer(TR2Q_MSG_PENDING)
    );

    uint64_t last_first_rowid = kw_get_int(
        gobj,
        trq->topic,
        "first_rowid",  // get from topic_var (tr2q_set_first_rowid)
        0,
        0
    );
    if(last_first_rowid) {
        if(last_first_rowid <= tranger2_topic_size(trq->tranger, trq->topic_name)) {
            json_object_set_new(
                match_cond,
                "from_rowid",
                json_integer((json_int_t)last_first_rowid)
            );
        }
    }

    /*
     *  We manage the callback, user not implied.
     *  Maintains a list of message's metadata.
     *  Load the message when need it.
     */
    json_object_set_new(
        match_cond,
        "load_record_callback",
        json_integer((json_int_t)(uintptr_t)load_record_callback)
    );

    trq->first_rowid = 0;

    json_t *jn_extra = json_pack("{s:s, s:I}",
        "topic_name", trq->topic_name,
        "trq", (json_int_t)(uintptr_t)trq
    );

    json_t *tr_list = tranger2_open_list(
        trq->tranger,
        trq->topic_name,
        match_cond, // owned
        jn_extra,   // owned
        NULL,       // rt_id
        FALSE,
        NULL        // creator
    );
    if(!tr_list) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "tranger2_open_list() failed",
            "topic_name",   "%s", trq->topic_name,
            NULL
        );
    }
    tranger2_close_list(trq->tranger, tr_list);

    if(trq->first_rowid==0) {
        // No pending msg, set the last rowid
        trq->first_rowid = tranger2_topic_size(trq->tranger, trq->topic_name);
    }
    if(trq->first_rowid) {
        tr2q_set_first_rowid(trq, trq->first_rowid);
    }

    return 0;
}

/***************************************************************************

 ***************************************************************************/
PUBLIC int tr2q_load_all(tr2_queue_t *trq, int64_t from_rowid, int64_t to_rowid)
{
    json_t *match_cond = json_object();
    if(from_rowid) {
        json_object_set_new(match_cond, "from_rowid", json_integer(from_rowid));
    }
    if(to_rowid) {
        json_object_set_new(match_cond, "to_rowid", json_integer(to_rowid));
    }
    json_object_set_new(match_cond, "load_record_callback", json_integer((json_int_t)(uintptr_t)load_record_callback));

    json_t *jn_extra = json_pack("{s:s, s:I}",
        "topic_name", trq->topic_name,
        "trq", (json_int_t)(uintptr_t)trq
    );
    json_t *tr_list = tranger2_open_list(
        trq->tranger,
        trq->topic_name,
        match_cond, // owned
        jn_extra,   // owned
        NULL,       // rt_id
        FALSE,
        NULL        // creator
    );
    tranger2_close_list(trq->tranger, tr_list);

    return 0;
}

/***************************************************************************

 ***************************************************************************/
PUBLIC int tr2q_load_all_by_time(tr2_queue_t *trq, int64_t from_t, int64_t to_t)
{
    json_t *match_cond = json_object();
    if(from_t) {
        json_object_set_new(match_cond, "from_t", json_integer(from_t));
    }
    if(to_t) {
        json_object_set_new(match_cond, "to_t", json_integer(to_t));
    }
    json_object_set_new(
        match_cond,
        "load_record_callback",
        json_integer((json_int_t)(uintptr_t)load_record_callback)
    );

    json_t *jn_extra = json_pack("{s:s, s:I}",
        "topic_name", trq->topic_name,
        "trq", (json_int_t)(uintptr_t)trq
    );
    json_t *tr_list = tranger2_open_list(
        trq->tranger,
        trq->topic_name,
        match_cond, // owned
        jn_extra,   // owned
        NULL,       // rt_id
        FALSE,
        NULL        // creator
    );
    tranger2_close_list(trq->tranger, tr_list);

    return 0;
}

/***************************************************************************
    Append a new message to queue forcing t

    If t (__t__) is 0 then the time will be set by TimeRanger with now time.

    The message (kw) is saved in disk with the user_flag TR2Q_MSG_PENDING,
    leaving in q2_msg_t only the metadata (to save memory).

    You can recover the message content with tr2q_msg_json().

    You must use tr2q_unload_msg() to mark a message as processed, removing from memory and
    resetting in disk the TR2Q_MSG_PENDING user flag.
 ***************************************************************************/
PUBLIC q2_msg_t *tr2q_append(
    tr2_queue_t *trq,
    json_int_t t,   // __t__ if 0 then the time will be set by TimeRanger with now time
    json_t *kw,     // owned
    uint16_t user_flag  // extra flags in addition to TR2Q_MSG_PENDING
) {
    hgobj gobj = 0;

    if(!kw || kw->refcount <= 0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "kw NULL",
            "topic",        "%s", trq->topic_name,
            NULL
        );
        return NULL;
    }

    /*
     *  Remove gbuffer from kw, serialize to save in tranger2, and restore later
     */
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(
        gobj, kw, "gbuffer", 0, KW_REQUIRED|KW_EXTRACT
    );
    if(gbuf) {
        json_object_set_new(kw, "payload", gbuffer_serialize(gobj, gbuf));
    }
    uint16_t mid = kw_get_int(gobj, kw, "mid", 0, KW_REQUIRED);

    md2_record_ex_t md_record;
    tranger2_append_record(
        trq->tranger,
        trq->topic_name,
        t,                              // __t__
        user_flag | TR2Q_MSG_PENDING,   // __flag__
        &md_record,
        json_incref(kw) // owned
    );

    json_object_set_new(kw, "gbuffer", json_integer((json_int_t)(uintptr_t)gbuf));

    q2_msg_t *msg = new_msg(
        trq,
        (json_int_t)md_record.rowid,
        mid,
        &md_record,
        kw  // owned
    );

    return msg;
}

/***************************************************************************
    Move a message from queued list to inflight list
 ***************************************************************************/
PUBLIC int tr2q_move_from_queued_to_inflight(q2_msg_t *msg)
{
    tr2_queue_t *trq = msg->trq;
    int ret = dl_delete(&trq->dl_queued, msg, NULL);
    if(ret<0) {
        // Error already logged
        return ret;
    }

    dl_add(&trq->dl_inflight, msg);
    json_t *kw_mqtt_msg = tr2q_msg_json(msg); // Load the message
    msg->inflight = TRUE;

    return kw_mqtt_msg?0:-1;
}

/***************************************************************************
    Unload a message from iter and hard mark with TR2Q_MSG_PENDING set to 0
 ***************************************************************************/
PUBLIC void tr2q_unload_msg(q2_msg_t *msg, int32_t result)
{
    tr2q_set_hard_flag(msg, TR2Q_MSG_PENDING, 0);

    if(msg->inflight) {
        dl_delete(&msg->trq->dl_inflight, msg, free_msg);
    } else {
        dl_delete(&msg->trq->dl_queued, msg, free_msg);
    }
}

/***************************************************************************
    Get a message from iter by his rowid
 ***************************************************************************/
PUBLIC q2_msg_t *tr2q_get_by_rowid(tr2_queue_t *trq, uint64_t rowid)
{
    register q2_msg_t *msg;

    Q2MSG_FOREACH_FORWARD_INFLIGHT(trq, msg) {
        if(msg->rowid == rowid) {
            msg->inflight = TRUE;
            return msg;
        }
    }
    Q2MSG_FOREACH_FORWARD_QUEUED(trq, msg) {
        if(msg->rowid == rowid) {
            msg->inflight = FALSE;
            return msg;
        }
    }

    return NULL;
}

/***************************************************************************
    Get a message from iter by his mid
 ***************************************************************************/
PUBLIC q2_msg_t *tr2q_get_by_mid(tr2_queue_t *trq, json_int_t mid)
{
    register q2_msg_t *msg;

    Q2MSG_FOREACH_FORWARD_INFLIGHT(trq, msg) {
        if(msg->mid == mid) {
            msg->inflight = TRUE;
            return msg;
        }
    }
    Q2MSG_FOREACH_FORWARD_QUEUED(trq, msg) {
        if(msg->mid == mid) {
            msg->inflight = FALSE;
            return msg;
        }
    }

    return NULL;
}

/***************************************************************************
    Get the message content
 ***************************************************************************/
PUBLIC json_t *tr2q_msg_json(q2_msg_t *msg) // Return is not yours, free with tr2q_unload_msg()
{
    if(msg->kw_record) {
        return msg->kw_record;
    }

    hgobj gobj = (hgobj)json_integer_value(json_object_get(msg->trq->tranger, "gobj"));

    msg->kw_record = tranger2_read_record_content( // return is yours
        msg->trq->tranger,
        msg->trq->topic,
        "",
        &msg->md_record
    );

    /*
     *  Deserialize payload back to gbuffer
     */
    if(msg->kw_record) {
        json_t *jn_payload = kw_get_dict_value(gobj, msg->kw_record, "payload", 0, 0);
        if(jn_payload) {
            gbuffer_t *gbuf = gbuffer_deserialize(gobj, jn_payload);
            if(gbuf) {
                json_object_del(msg->kw_record, "payload");
                json_object_set_new(
                    msg->kw_record,
                    "gbuffer",
                    json_integer((json_int_t)(uintptr_t)gbuf)
                );
            }
        }
    }

    return msg->kw_record;
}

/***************************************************************************
    Put a hard flag in a message.
    You must flag a message after append it if you want recover it in the next open.
 ***************************************************************************/
PRIVATE int tr2q_set_hard_flag(q2_msg_t *msg, uint16_t hard_mark, BOOL set)
{
    return tranger2_set_user_flag(
        msg->trq->tranger,
        tranger2_topic_name(msg->trq->topic),
        "",
        msg->md_record.__t__,
        msg->md_record.rowid,
        hard_mark,
        set
    );
}

/***************************************************************************
    Save hard_mark (user_flag of timeranger2 metadata)
    Return the new value
 ***************************************************************************/
PUBLIC int tr2q_save_hard_mark(q2_msg_t *msg, uint16_t value)
{
    msg->md_record.user_flag = value | TR2Q_MSG_PENDING;

    return tranger2_write_user_flag(
        msg->trq->tranger,
        tranger2_topic_name(msg->trq->topic),
        "",
        msg->md_record.__t__,
        msg->md_record.rowid,
        msg->md_record.user_flag   // __flag__
    );
}

/***************************************************************************
 *  Do backup
 ***************************************************************************/
PUBLIC int tr2q_check_backup(tr2_queue_t *trq)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(trq->tranger, "gobj"));
    uint64_t backup_queue_size = kw_get_int(gobj, trq->topic, "backup_queue_size", 0, 0);

    if(backup_queue_size) {
        uint64_t sz = tranger2_topic_size(trq->tranger, trq->topic_name);
        if(sz >= backup_queue_size) {
            tr2q_set_first_rowid(trq, sz);
            trq->topic = tranger2_backup_topic(
                trq->tranger,
                trq->topic_name,
                0,
                0,
                TRUE,
                0
            );
            tr2q_set_first_rowid(trq, 0);
        }
    }

    return 0;
}
