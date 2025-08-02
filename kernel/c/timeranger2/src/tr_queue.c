/***********************************************************************
 *          TR_QUEUE.C
 *
 *          Persistent queue based in TimeRanger
 *
 *          Copyright (c) 2019 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>

#include <kwid.h>
#include <helpers.h>
#include "tr_queue.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void free_msg(void *msg_);

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
    Open queue
 ***************************************************************************/
PUBLIC tr_queue_t *trq_open(
    json_t *tranger,
    const char *topic_name,
    const char *tkey,
    system_flag2_t system_flag, // KEY_TYPE_MASK2 forced to sf_rowid_key
    size_t backup_queue_size
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*-------------------------------*
     *  Alloc memory
     *-------------------------------*/
    tr_queue_t *trq = GBMEM_MALLOC(sizeof(tr_queue_t));
    if(!trq) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "Cannot create tr_queue. GBMEM_MALLOC() FAILED",
            NULL
        );
        return 0;
    }
    trq->tranger = tranger;
    snprintf(trq->topic_name, sizeof(trq->topic_name), "%s", topic_name);

    json_t *jn_topic_ext = json_object();
    json_object_set_new(jn_topic_ext, "filename_mask", json_string("queue"));

    system_flag &= KEY_TYPE_MASK2;
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
        trq_close(trq);
        return 0;
    }
    dl_init(&trq->dl_q_msg, 0);

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
PUBLIC void trq_close(tr_queue_t * trq)
{
    dl_flush(&((tr_queue_t *)trq)->dl_q_msg, free_msg);
    GBMEM_FREE(trq);
}

/***************************************************************************
    Set first rowid to search
 ***************************************************************************/
PUBLIC void trq_set_first_rowid(tr_queue_t * trq, uint64_t first_rowid)
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
    New msg
 ***************************************************************************/
PRIVATE q_msg_t *new_msg(
    tr_queue_t *trq,
    json_int_t rowid,
    const md2_record_ex_t *md_record,
    json_t *jn_record // owned
)
{
    hgobj gobj = 0;

    /*
     *  Alloc memory
     */
    q_msg_t *msg = GBMEM_MALLOC(sizeof(q_msg_t));
    if(!msg) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "Cannot create msg. GBMEM_MALLOC() FAILED",
            NULL
        );
        kw_decref(jn_record);
        return 0;
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
    kw_decref(jn_record);
    msg->trq = trq;
    msg->rowid = rowid;

    dl_add(&trq->dl_q_msg, msg);

    return msg;
}

/***************************************************************************
    Free msg
 ***************************************************************************/
PRIVATE void free_msg(void *msg_)
{
    q_msg_t *msg = msg_;
    memset(msg, 0, sizeof(q_msg_t));
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
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    tr_queue_t *trq = (tr_queue_t *)(uintptr_t)kw_get_int(gobj, list, "trq", 0, KW_REQUIRED);

    if(trq->first_rowid==0) {
        // The first record called is the first pending record
        trq->first_rowid = rowid;
    }

    new_msg(trq, rowid, md_record, jn_record);

    return 0;
}

PUBLIC int trq_load(tr_queue_t * trq)
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
        json_integer(TRQ_MSG_PENDING)
    );

    json_object_set_new(match_cond, "only_md", json_true());

    uint64_t last_first_rowid = kw_get_int(
        gobj,
        trq->topic,
        "first_rowid",  // get from topic_var (trq_set_first_rowid)
        0,
        0
    );
    if(last_first_rowid) {
        if(last_first_rowid <= tranger2_topic_size(trq->tranger, trq->topic_name)) {
            json_object_set_new(match_cond, "from_rowid", json_integer((json_int_t)last_first_rowid));
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
        json_integer((json_int_t)(size_t)load_record_callback)
    );

    trq->first_rowid = 0;

    json_t *jn_extra = json_pack("{s:s, s:I}",
        "topic_name", trq->topic_name,
        "trq", (json_int_t)(size_t)trq
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
        trq_set_first_rowid(trq, trq->first_rowid);
    }

    return 0;
}

/***************************************************************************

 ***************************************************************************/
PUBLIC int trq_load_all(tr_queue_t * trq, int64_t from_rowid, int64_t to_rowid)
{
    json_t *match_cond = json_object();
    if(from_rowid) {
        json_object_set_new(match_cond, "from_rowid", json_integer(from_rowid));
    }
    if(to_rowid) {
        json_object_set_new(match_cond, "to_rowid", json_integer(to_rowid));
    }
    json_object_set_new(match_cond, "load_record_callback", json_integer((json_int_t)(size_t)load_record_callback));

    json_t *jn_extra = json_pack("{s:s, s:I}",
        "topic_name", trq->topic_name,
        "trq", (json_int_t)(size_t)trq
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
PUBLIC int trq_load_all_by_time(tr_queue_t * trq, int64_t from_t, int64_t to_t)
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
        json_integer((json_int_t)(size_t)load_record_callback)
    );

    json_object_set_new(match_cond, "only_md", json_true());

    json_t *jn_extra = json_pack("{s:s, s:I}",
        "topic_name", trq->topic_name,
        "trq", (json_int_t)(size_t)trq
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
    Append a new message to queue simulating t
 ***************************************************************************/
PUBLIC q_msg_t *trq_append2(
    tr_queue_t * trq,
    json_int_t t,   // __t__
    json_t *kw  // owned
)
{
    hgobj gobj = 0;

    if(!kw || kw->refcount <= 0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "kw NULL",
            "topic",        "%s", trq->topic_name,
            NULL
        );
        return 0;
    }

    md2_record_ex_t md_record;
    tranger2_append_record(
        trq->tranger,
        trq->topic_name,
        t,      // __t__
        TRQ_MSG_PENDING,    // __flag__
        &md_record,
        kw_incref(kw) // owned
    );

    q_msg_t *msg = new_msg(
        trq,
        (json_int_t)md_record.rowid,
        &md_record,
        kw  // owned
    );

    return msg;
}

/***************************************************************************
    Get a message from iter by his rowid
 ***************************************************************************/
PUBLIC q_msg_t *trq_get_by_rowid(tr_queue_t * trq, uint64_t rowid)
{
    register q_msg_t *msg;

    qmsg_foreach_forward(trq, msg) {
        if(msg->rowid == rowid) {
            return msg;
        }
    }

    return 0;
}

/***************************************************************************
    Check pending status of a rowid (low level)
 ***************************************************************************/
PUBLIC int trq_check_pending_rowid(
    tr_queue_t * trq,
    uint64_t __t__,
    uint64_t rowid
)
{
    uint32_t __user_flag__ = tranger2_read_user_flag(
        trq->tranger,
        trq->topic_name,
        "",
        __t__,
        rowid
    );

    if(__user_flag__ & TRQ_MSG_PENDING) {
        return 1;
    } else {
        return 0;
    }
}

/***************************************************************************
    Unload a message from iter
 ***************************************************************************/
PUBLIC void trq_unload_msg(q_msg_t *msg, int32_t result)
{
    trq_set_hard_flag(msg, TRQ_MSG_PENDING, 0);

    dl_delete(&msg->trq->dl_q_msg, msg, free_msg);
}

/***************************************************************************
    Put a hard flag in a message.
    You must flag a message after append it if you want recover it in the next open.
 ***************************************************************************/
PUBLIC int trq_set_hard_flag(q_msg_t *msg, uint32_t hard_mark, BOOL set)
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
    Set soft mark
 ***************************************************************************/
PUBLIC uint64_t trq_set_soft_mark(q_msg_t *msg, uint64_t soft_mark, BOOL set)
{
    if(set) {
        /*
         *  Set
         */
        ((q_msg_t *)msg)->mark |= soft_mark;
    } else {
        /*
         *  Reset
         */
        ((q_msg_t *)msg)->mark &= ~soft_mark;
    }

    return ((q_msg_t *)msg)->mark;
}

/***************************************************************************
    Get info of message
 ***************************************************************************/
PUBLIC json_t *trq_msg_json(q_msg_t *msg) // Load the message, Return json is YOURS!!
{
    json_t *jn_record = tranger2_read_record_content( // return is yours
        msg->trq->tranger,
        msg->trq->topic,
        "",
        &msg->md_record
    );
    if(!jn_record) {
        hgobj gobj = (hgobj)json_integer_value(json_object_get(msg->trq->tranger, "gobj"));
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "jn_msg NULL",
            "topic",        "%s", msg->trq->topic_name,
            NULL
        );
    }
    return jn_record;
}


//PUBLIC BOOL trq_msg_is_t_ms(q_msg_t *msg)
//{
//    return (((q_msg_t *)msg)->md_record.__system_flag__ & sf_t_ms)?true:FALSE;
//}
//PUBLIC BOOL trq_msg_is_tm_ms(q_msg_t *msg)
//{
//    return (((q_msg_t *)msg)->md_record.__system_flag__ & sf_tm_ms)?true:FALSE;
//}


/***************************************************************************
    Metadata
 ***************************************************************************/
PUBLIC int trq_set_metadata(
    json_t *kw,
    const char *key,
    json_t *jn_value) // owned
{
    hgobj gobj = 0;
    return kw_set_subdict_value(
        gobj,
        kw,
        __MD_TRQ__,
        key,
        jn_value // owned
    );
}

PUBLIC json_t *trq_get_metadata( // WARNING The return json is not yours!
    json_t *kw
)
{
    hgobj gobj = 0;
    return kw_get_dict(gobj,
        kw,
        __MD_TRQ__,
        0,
        KW_REQUIRED
    );
}

/***************************************************************************
 *  Return a new kw only with the message metadata
 ***************************************************************************/
PUBLIC json_t *trq_answer(
    json_t *jn_message,  // not owned, Gps message, to get only __MD_TRQ__
    int result
)
{
    hgobj gobj = 0;

    json_t *kw_response = json_object();
    json_t *__md__ = trq_get_metadata(jn_message);
    if(__md__) {
        json_object_set(kw_response, __MD_TRQ__, __md__);
        trq_set_metadata(kw_response, "result", json_integer(result));
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "queue message without metadata",
            NULL
        );
        gobj_trace_json(gobj, jn_message, "queue message without metadata");
    }

    return kw_response;
}

/***************************************************************************
 *  Do backup
 ***************************************************************************/
PUBLIC int trq_check_backup(tr_queue_t * trq)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(trq->tranger, "gobj"));
    uint64_t backup_queue_size = kw_get_int(gobj, trq->topic, "backup_queue_size", 0, 0);

    if(backup_queue_size) {
        uint64_t sz = tranger2_topic_size(trq->tranger, trq->topic_name);
        if(sz >= backup_queue_size) {
            trq_set_first_rowid(trq, sz);
            trq->topic = tranger2_backup_topic(
                trq->tranger,
                trq->topic_name,
                0,
                0,
                TRUE,
                0
            );
            trq_set_first_rowid(trq, 0);
        }
    }

    return 0;
}
