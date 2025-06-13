/***********************************************************************
 *          TR_QUEUE.C
 *
 *          Persistent queue based in TimeRanger
 *
 *          Copyright (c) 2019 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <kwid.h>
#include "tr_queue.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct {
    json_t *tranger;
    json_t *topic;
    char topic_name[128];
    int maximum_retries;
    dl_list_t dl_q_msg;
    uint64_t first_rowid;
} tr_queue_t;

typedef struct {
    DL_ITEM_FIELDS

    tr_queue_t *trq;
    md2_record_ex_t md_record;
    uint64_t mark;          // soft mark.
    time_t timeout_ack;
    int retries;
    json_t *jn_record;
    json_int_t rowid;
    char key[RECORD_KEY_VALUE_MAX];
} q_msg_t;

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
PUBLIC tr_queue trq_open(
    json_t *tranger,
    const char *topic_name,
    const char *pkey,
    const char *tkey,
    const char *filename_mask,
    system_flag2_t system_flag,
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
    if(!empty_string(filename_mask)) {
        json_object_set_new(jn_topic_ext, "filename_mask", json_string(filename_mask));
    }

    /*-------------------------------*
     *  Open/Create topic
     *-------------------------------*/
    trq->topic = tranger2_create_topic( // IDEMPOTENT function
        trq->tranger,
        topic_name,
        pkey,
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
PUBLIC void trq_close(tr_queue trq)
{
    dl_flush(&((tr_queue_t *)trq)->dl_q_msg, free_msg);
    GBMEM_FREE(trq);
}

/***************************************************************************
    Return size of queue (messages in queue)
 ***************************************************************************/
PUBLIC size_t trq_size(tr_queue trq)
{
    if(!trq) {
        return 0;
    }
    return dl_size(&((tr_queue_t *)trq)->dl_q_msg);
}

/***************************************************************************
    Return tranger of queue
 ***************************************************************************/
PUBLIC json_t * trq_tranger(tr_queue trq)
{
    if(!trq) {
        return 0;
    }
    return ((tr_queue_t *)trq)->tranger;
}

/***************************************************************************
    Return topic of queue
 ***************************************************************************/
PUBLIC json_t * trq_topic(tr_queue trq)
{
    if(!trq) {
        return 0;
    }
    return ((tr_queue_t *)trq)->topic;
}

/***************************************************************************
    Set first rowid to search
 ***************************************************************************/
PUBLIC void trq_set_first_rowid(tr_queue trq_, uint64_t first_rowid)
{
    register tr_queue_t *trq = trq_;
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
    const char *key,
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
        return 0;
    }
    memmove(&msg->md_record, md_record, sizeof(md2_record_ex_t));
    msg->jn_record = 0; // Cárgalo solo cuando se use, jn_record;
    JSON_DECREF(jn_record);
    msg->trq = trq;
    msg->rowid = rowid;
    snprintf(msg->key, sizeof(msg->key), "%s", key);

    dl_add(&trq->dl_q_msg, msg);

    return msg;
}

/***************************************************************************
    Free msg
 ***************************************************************************/
PRIVATE void free_msg(void *msg_)
{
    q_msg_t *msg = msg_;
    JSON_DECREF(msg->jn_record);
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
    tr_queue_t *trq = (tr_queue_t *)(size_t)kw_get_int(gobj, list, "trq", 0, KW_REQUIRED);

    if(trq->first_rowid==0) {
        // The first record called is the first pending record
        trq->first_rowid = rowid;
    }

    new_msg(trq, key, rowid, md_record, jn_record);

    return 0;
}

PUBLIC int trq_load(tr_queue trq_)
{
    register tr_queue_t *trq = trq_;
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
        false,
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
PUBLIC int trq_load_all(tr_queue trq_, const char *key, int64_t from_rowid, int64_t to_rowid)
{
    register tr_queue_t *trq = trq_;

    json_t *match_cond = json_object();
    if(from_rowid) {
        json_object_set_new(match_cond, "from_rowid", json_integer(from_rowid));
    }
    if(to_rowid) {
        json_object_set_new(match_cond, "to_rowid", json_integer(to_rowid));
    }
    if(key) {
        json_object_set_new(
            match_cond,
            "key",
            json_string(key)
        );
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
        false,
        NULL        // creator
    );
    tranger2_close_list(trq->tranger, tr_list);

    return 0;
}

/***************************************************************************

 ***************************************************************************/
PUBLIC int trq_load_all_by_time(tr_queue trq_, const char *key, int64_t from_t, int64_t to_t)
{
    register tr_queue_t *trq = trq_;

    json_t *match_cond = json_object();
    if(from_t) {
        json_object_set_new(match_cond, "from_t", json_integer(from_t));
    }
    if(to_t) {
        json_object_set_new(match_cond, "to_t", json_integer(to_t));
    }
    if(key) {
        json_object_set_new(
            match_cond,
            "key",
            json_string(key)
        );
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
        false,
        NULL        // creator
    );
    tranger2_close_list(trq->tranger, tr_list);

    return 0;
}

/***************************************************************************
    Append a pending message to queue
 ***************************************************************************/
PUBLIC q_msg trq_append(
    tr_queue trq,
    json_t *jn_msg  // owned
)
{
    return trq_append2(trq, 0, jn_msg);
}

/***************************************************************************
    Append a new message to queue simulating t
 ***************************************************************************/
PUBLIC q_msg trq_append2(
    tr_queue trq_,
    json_int_t t,   // __t__
    json_t *jn_msg  // owned
)
{
    register tr_queue_t *trq = trq_;
    hgobj gobj = 0;

    if(!jn_msg || jn_msg->refcount <= 0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "jn_msg NULL",
            "topic",        "%s", trq->topic_name,
            NULL
        );
        return 0;
    }

    /*
     *  Get the pkey, must be a string key.
     */
    json_t *topic = tranger2_topic(trq->tranger, trq->topic_name);
    const char *pkey = json_string_value(json_object_get(topic, "pkey"));
    const char *key = json_string_value(json_object_get(jn_msg, pkey));

    JSON_INCREF(jn_msg);
    md2_record_ex_t md_record;
    tranger2_append_record(
        trq->tranger,
        trq->topic_name,
        t,      // __t__
        TRQ_MSG_PENDING,    // __flag__
        &md_record,
        jn_msg // owned
    );
    q_msg_t *msg = new_msg(
        trq,
        key,
        (json_int_t)md_record.rowid,
        &md_record,
        jn_msg  // owned
    );
    return msg;
}

/***************************************************************************
    Get a message from iter by his rowid
 ***************************************************************************/
PUBLIC q_msg trq_get_by_rowid(tr_queue trq, uint64_t rowid)
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
    Get number of messages from iter by his key
 ***************************************************************************/
PUBLIC int trq_size_by_key(tr_queue trq, const char *key)
{
    register q_msg_t *msg;
    int n = 0;
    qmsg_foreach_forward(trq, msg) {
        if(strcmp(msg->key, key)==0) {
            n++;
        }
    }

    return n;
}

/***************************************************************************
    Check pending status of a rowid (low level)
 ***************************************************************************/
PUBLIC int trq_check_pending_rowid(
    tr_queue trq_,
    const char *key,        // In tranger2 ('key', '__t__', 'rowid') is required
    uint64_t __t__,
    uint64_t rowid      // must be file rowid, not global topic rowid
)
{
    register tr_queue_t *trq = trq_;

    uint32_t __user_flag__ = tranger2_read_user_flag(
        trq->tranger,
        trq->topic_name,
        key,
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
PUBLIC void trq_unload_msg(q_msg msg_, int32_t result)
{
    q_msg_t *msg = msg_;

    trq_set_hard_flag(msg, TRQ_MSG_PENDING, 0);

    dl_delete(&msg->trq->dl_q_msg, msg, free_msg);
}

/***************************************************************************
    Put a hard flag in a message.
    You must flag a message after append it if you want recover it in the next open.
 ***************************************************************************/
PUBLIC int trq_set_hard_flag(q_msg msg_, uint32_t hard_mark, BOOL set)
{
    register q_msg_t *msg = msg_;

    return tranger2_set_user_flag(
        msg->trq->tranger,
        tranger2_topic_name(msg->trq->topic),
        msg->key,
        msg->md_record.__t__,
        msg->md_record.rowid,
        hard_mark,
        set
    );
}

/***************************************************************************
    Set soft mark
 ***************************************************************************/
PUBLIC uint64_t trq_set_soft_mark(q_msg msg, uint64_t soft_mark, BOOL set)
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
    Get soft mark
 ***************************************************************************/
PUBLIC uint64_t trq_get_soft_mark(q_msg msg)
{
    return ((q_msg_t *)msg)->mark;
}

/***************************************************************************
    Set ack timer
 ***************************************************************************/
PUBLIC time_t trq_set_ack_timer(q_msg msg, time_t seconds)
{
    ((q_msg_t *)msg)->timeout_ack = start_sectimer(seconds);
    return ((q_msg_t *)msg)->timeout_ack;
}

/***************************************************************************
    Clear ack timer
 ***************************************************************************/
PUBLIC void trq_clear_ack_timer(q_msg msg)
{
    ((q_msg_t *)msg)->timeout_ack = -1;
}

/***************************************************************************
    Test ack timer
 ***************************************************************************/
PUBLIC BOOL trq_test_ack_timer(q_msg msg)
{
    return test_sectimer(((q_msg_t *)msg)->timeout_ack);
}

/***************************************************************************
    Set maximum retries
 ***************************************************************************/
PUBLIC void trq_set_maximum_retries(tr_queue trq, int maximum_retries)
{
    ((tr_queue_t *)trq)->maximum_retries = maximum_retries;
}

/***************************************************************************
    Add retries
 ***************************************************************************/
PUBLIC void trq_add_retries(q_msg msg, int retries)
{
    ((q_msg_t *)msg)->retries += retries;
}

/***************************************************************************
    Clear retries
 ***************************************************************************/
PUBLIC void trq_clear_retries(q_msg msg)
{
    ((q_msg_t *)msg)->retries = 0;
}

/***************************************************************************
    Test retries
 ***************************************************************************/
PUBLIC BOOL trq_test_retries(q_msg msg)
{
    if( ((q_msg_t *)msg)->trq->maximum_retries <= 0) {
        // No retries no test true
        return 0;
    }
    return (((q_msg_t *)msg)->retries >= ((q_msg_t *)msg)->trq->maximum_retries)?true:false;
}

/***************************************************************************
    Walk over instances
 ***************************************************************************/
PUBLIC q_msg trq_first_msg(tr_queue trq)
{
    if(!trq) {
        return 0;
    }
    return dl_first(&((tr_queue_t *)trq)->dl_q_msg);
}
PUBLIC q_msg trq_last_msg(tr_queue trq)
{
    if(!trq) {
        return 0;
    }
    return dl_last(&((tr_queue_t *)trq)->dl_q_msg);
}

PUBLIC q_msg trq_next_msg(q_msg msg)
{
    return dl_next(msg);
}
PUBLIC q_msg trq_prev_msg(q_msg msg)
{
    return dl_prev(msg);
}

/***************************************************************************
    Get info of message
 ***************************************************************************/
PUBLIC md2_record_ex_t *trq_msg_md(q_msg msg_)
{
    register q_msg_t *msg = msg_;
    return &msg->md_record;
}
PUBLIC json_t *trq_msg_json(q_msg msg_) // Load the message, Return json is NOT YOURS!!
{
    register q_msg_t *msg = msg_;

    if(!msg->jn_record) {
        // Load the message
        msg->jn_record = tranger2_read_record_content( // return is yours
            msg->trq->tranger,
            msg->trq->topic,
            msg->key,
            &msg->md_record
        );
        if(!msg->jn_record) {
            hgobj gobj = (hgobj)json_integer_value(json_object_get(msg->trq->tranger, "gobj"));
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "jn_msg NULL",
                "topic",        "%s", msg->trq->topic_name,
                "key",          "%s", msg->key,
                NULL
            );
        }
    }
    return msg->jn_record;
}

PUBLIC json_int_t trq_msg_rowid(q_msg msg_)
{
    register q_msg_t *msg = msg_;
    if(!msg) {
        return 0;
    }
    return msg->rowid;
}
PUBLIC uint64_t trq_msg_time(q_msg msg_)
{
    register q_msg_t *msg = msg_;
    return msg->md_record.__t__;
}
PUBLIC const char *trq_msg_key(q_msg msg_)
{
    register q_msg_t *msg = msg_;
    return msg->key;
}

//PUBLIC BOOL trq_msg_is_t_ms(q_msg msg)
//{
//    return (((q_msg_t *)msg)->md_record.__system_flag__ & sf_t_ms)?true:false;
//}
//PUBLIC BOOL trq_msg_is_tm_ms(q_msg msg)
//{
//    return (((q_msg_t *)msg)->md_record.__system_flag__ & sf_tm_ms)?true:false;
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
        json_object_set_new(kw_response, __MD_TRQ__, kw_duplicate(gobj, __md__));
        trq_set_metadata(kw_response, "result", json_integer(result));
    }

    return kw_response;
}

/***************************************************************************
 *  Do backup
 ***************************************************************************/
PUBLIC int trq_check_backup(tr_queue trq_)
{
    tr_queue_t *trq = trq_;

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
                true,
                0
            );
            trq_set_first_rowid(trq, 0);
        }
    }

    return 0;
}
