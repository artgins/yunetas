/****************************************************************************
 *          TR2_QUEUE.H
 *
 *          Persistent queue based in TimeRanger
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/

#pragma once

#include "timeranger2.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
#define __MD_TR2Q__ "__md_tr2q__"

#define TR2Q_MSG_PENDING   0x0001

/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct {
    json_t *tranger;
    json_t *topic;
    char topic_name[256];

    size_t max_inflight_bytes;
    size_t max_inflight_messages;
    size_t max_queued_bytes;
    size_t max_queued_messages;
    size_t message_size_limit;

    dl_list_t dl_inflight;
    dl_list_t dl_queued;
    uint64_t first_rowid;
} tr2_queue_t;

typedef struct {
    DL_ITEM_FIELDS

    tr2_queue_t *trq;
    md2_record_ex_t md_record;
    uint64_t mark;          // soft mark.
    json_int_t rowid;       // global rowid that it must match the rowid in md_record
    json_int_t mid;         // Yes, it's an ease for mqtt protocol
    json_t *kw_record;      // It may have gbuffer
} q2_msg_t;


/***************************************************************
 *              Prototypes
 ***************************************************************/

/**
    Open queue (Remember before open to call tranger2_startup())
*/
PUBLIC tr2_queue_t *tr2q_open(
    json_t *tranger,
    const char *topic_name,
    const char *tkey,
    system_flag2_t system_flag, // KEY_TYPE_MASK forced to sf_rowid_key
    size_t max_inflight_messages,
    size_t backup_queue_size
);

/**
    Close queue (After close the queue, remember to do tranger2_shutdown())
*/
PUBLIC void tr2q_close(tr2_queue_t *trq);

/**
    Append a new message to queue forcing t

    If t (__t__) is 0 then the time will be set by TimeRanger with now time.

    The message (kw) is saved in disk with the user_flag TR2Q_MSG_PENDING,
    and enqueued in inflight queue if it has space (until max_inflight_messages msgs)
    otherwise it's enqueued in queued only with metadata (to save memory)
    If max_inflight_messages is 0 then all messages are inflight
    If max_inflight_messages is 1, the messages are sent in order and one by one.

    You can recover the message content with tr2q_msg_json().

    You must use tr2q_unload_msg() to mark a message as processed, removing from memory and
    resetting in disk the TR2Q_MSG_PENDING user flag.
*/
PUBLIC q2_msg_t * tr2q_append(
    tr2_queue_t *trq,
    json_int_t t,       // __t__ if 0 then the time will be set by TimeRanger with now time
    json_t *kw,         // owned
    uint16_t user_flag  // extra flags in addition to TR2Q_MSG_PENDING
);

/**
    Unload a message successfully from iter (disk TR2Q_MSG_PENDING set to 0)
*/
PUBLIC void tr2q_unload_msg(q2_msg_t *msg, int32_t result);

/**
    Get a message from iter by his rowid
*/
PUBLIC q2_msg_t *tr2q_get_by_rowid(tr2_queue_t *trq, uint64_t rowid);

/**
    Mark a message.
    You must flag a message with TR2Q_MSG_PENDING after append it to queue
        if you want recover it in the next open
        with the flag used in tr2q_load()
*/
PUBLIC int tr2q_set_hard_flag(q2_msg_t *msg, uint32_t hard_mark, BOOL set);

static inline q2_msg_t *tr2q_first_inflight_msg(tr2_queue_t *trq)
{
    return dl_first(&((tr2_queue_t *)trq)->dl_inflight);
}
static inline q2_msg_t *tr2q_last_inflight_msg(tr2_queue_t *trq)
{
    return dl_last(&((tr2_queue_t *)trq)->dl_inflight);
}

static inline q2_msg_t *tr2q_first_queued_msg(tr2_queue_t *trq)
{
    return dl_first(&((tr2_queue_t *)trq)->dl_queued);
}
static inline q2_msg_t *tr2q_last_queued_msg(tr2_queue_t *trq)
{
    return dl_last(&((tr2_queue_t *)trq)->dl_queued);
}

static inline q2_msg_t *tr2q_next_msg(q2_msg_t *msg)
{
    return dl_next(msg);
}
static inline q2_msg_t *tr2q_prev_msg(q2_msg_t *msg)
{
    return dl_prev(msg);
}

/**
    Return number of inflight messages
*/
static inline size_t tr2q_inflight_size(tr2_queue_t *trq)
{
    return dl_size(&trq->dl_inflight);
}

/**
    Return number of queued messages
*/
static inline size_t tr2q_queued_size(tr2_queue_t *trq)
{
    return dl_size(&trq->dl_queued);
}

/**
    Return tranger of queue
*/
static inline json_t * tr2q_tranger(tr2_queue_t *trq)
{
    return trq->tranger;
}

/**
    Return topic of queue
*/
static inline json_t * tr2q_topic(tr2_queue_t *trq)
{
    return trq->topic;
}

/**
    Load pending messages (with TR2Q_MSG_PENDING flag)
    Content is not loaded or is discarded
*/
PUBLIC int tr2q_load(tr2_queue_t *trq);

/**
    Load all messages, filtering by rowid (with or without TR2Q_MSG_PENDING flag)
    Content is not loaded or is discarded
*/
PUBLIC int tr2q_load_all(tr2_queue_t *trq, int64_t from_rowid, int64_t to_rowid);

/**
    Load all messages, filtering by time (with or without TR2Q_MSG_PENDING flag)
    Content is not loaded or is discarded
*/
PUBLIC int tr2q_load_all_by_time(tr2_queue_t *trq, int64_t from_t, int64_t to_t);

/**
    Check pending status of a rowid (low level)
    Return -1 if rowid not exists, 1 if pending, 0 if not pending
*/
PUBLIC int tr2q_check_pending_rowid(
    tr2_queue_t *trq,
    uint64_t __t__,
    uint64_t rowid
);

/**
    Set soft mark
*/
PUBLIC uint64_t tr2q_set_soft_mark(q2_msg_t *msg, uint64_t soft_mark, BOOL set);

/**
    Get if it's msg pending of ack
*/
static inline uint64_t tr2q_get_soft_mark(q2_msg_t *msg)
{
    return msg->mark;
}


static inline md2_record_ex_t *tr2q_msg_md(q2_msg_t *msg)
{
    return &msg->md_record;
}

/**
    Get info of message
*/
PUBLIC json_t *tr2q_msg_json(q2_msg_t *msg); // Load the message, Return json is YOURS!!

static inline json_int_t tr2q_msg_rowid(q2_msg_t *msg)
{
    return msg->rowid;
}
static inline uint64_t tr2q2_msg_time(q2_msg_t *msg)
{
    return msg->md_record.__t__;
}
static inline uint16_t tr2q_msg_hard_flag(q2_msg_t *msg)
{
    return msg->md_record.user_flag;
}

/**
    Metadata
*/
PUBLIC int tr2q_set_metadata(
    json_t *kw,
    const char *key,
    json_t *jn_value // owned
);
PUBLIC json_t *tr2q_get_metadata( // WARNING The return json is not yours!
    json_t *kw
);

/**
    Return a new kw only with the message metadata
*/
PUBLIC json_t *tr2q_answer(
    json_t *jn_message,  // not owned, Gps message, to get only __MD_TR2Q__
    int result
);

/**
    Do backup if needed.
*/
PUBLIC int tr2q_check_backup(tr2_queue_t *trq);

/**
    Walk over instances
*/
#define q2msg_foreach_forward(trq, msg) \
    for((msg) = t2rq_first_msg(trq); \
        (msg)!=0 ; \
        (msg) = t2rq_next_msg(msg))

#define q2msg_foreach_forward_safe(trq, msg, next) \
   for((msg) = t2rq_first_msg(trq), (next) = (msg) ? t2rq_next_msg(msg) : 0; \
       (msg)!=0 ; \
       (msg) = (next), (next) = (msg) ? t2rq_next_msg(msg) : 0)

#define q2msg_foreach_backward(trq, msg) \
   for((msg) = t2rq_last_msg(trq); \
       (msg)!=0 ; \
       (msg) = t2rq_prev_msg(msg))

#define q2msg_foreach_backward_safe(trq, msg, prev) \
   for((msg) = t2rq_last_msg(trq), (prev) = t2rq_prev_msg(msg); \
       (msg)!=0 ; \
       (msg) = (prev), (prev) = t2rq_prev_msg(msg))

#ifdef __cplusplus
}
#endif
