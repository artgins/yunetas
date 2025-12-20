/****************************************************************************
 *          TR_QUEUE.H
 *
 *          Persistent queue based in TimeRanger
 *
 *          Copyright (c) 2019 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
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
#define __MD_TRQ__ "__md_trq__"

#define TRQ_MSG_PENDING   0x0001

/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct {
    json_t *tranger;
    json_t *topic;
    char topic_name[256];
    int maximum_retries;
    dl_list_t dl_q_msg;
    uint64_t first_rowid;
} tr_queue_t;

typedef struct {
    DL_ITEM_FIELDS

    tr_queue_t *trq;
    md2_record_ex_t md_record;
    uint64_t mark;          // soft mark.
    json_int_t rowid;       // global rowid that it must match the rowid in md_record
} q_msg_t;


/***************************************************************
 *              Prototypes
 ***************************************************************/

/**
    Open queue (Remember previously open tranger2_startup())
*/
PUBLIC tr_queue_t *trq_open(
    json_t *tranger,
    const char *topic_name,
    const char *tkey,
    system_flag2_t system_flag, // KEY_TYPE_MASK2 forced to sf_rowid_key
    size_t backup_queue_size
);

/**
    Close queue (After close the queue, remember do tranger2_shutdown())
*/
PUBLIC void trq_close(tr_queue_t * trq);

/**
    Return size of queue (messages in queue)
*/
static inline size_t trq_size(tr_queue_t * trq)
{
    return dl_size(&trq->dl_q_msg);
}

/**
    Return tranger of queue
*/
static inline json_t * trq_tranger(tr_queue_t * trq)
{
    return trq->tranger;
}


/**
    Return topic of queue
*/
static inline json_t * trq_topic(tr_queue_t * trq)
{
    return trq->topic;
}


/**
    Load pending messages (with TRQ_MSG_PENDING flag)
    Content is not loaded or is discarded
*/
PUBLIC int trq_load(tr_queue_t * trq);

/**
    Load all messages, filtering by rowid (with or without TRQ_MSG_PENDING flag)
    Content is not loaded or is discarded
*/
PUBLIC int trq_load_all(tr_queue_t * trq, int64_t from_rowid, int64_t to_rowid);

/**
    Load all messages, filtering by time (with or without TRQ_MSG_PENDING flag)
    Content is not loaded or is discarded
*/
PUBLIC int trq_load_all_by_time(tr_queue_t * trq, int64_t from_t, int64_t to_t);

/**
    Append a new message to queue forcing t

    If t (__t__) is 0 then the time will be set by TimeRanger with now time.

    The message (kw) is saved in disk with the user_flag TRQ_MSG_PENDING,
    leaving in q_msg_t only the metadata (to save memory).

    You can recover the message content with trq_msg_json().

    You must use trq_unload_msg() to mark a message as processed, removing from memory and
    resetting in disk the TRQ_MSG_PENDING user flag.
*/
PUBLIC q_msg_t * trq_append2(
    tr_queue_t * trq,
    json_int_t t,       // __t__ if 0 then the time will be set by TimeRanger with now time
    json_t *kw,         // owned
    uint16_t user_flag  // extra flags in addition to TRQ_MSG_PENDING
);

/**
    Append a new message to queue with the current time.
*/
static inline q_msg_t *trq_append(
    tr_queue_t * trq,
    json_t *kw  // owned
)
{
    return trq_append2(trq, 0, kw, 0);
}

/**
    Get a message from iter by his rowid
*/
PUBLIC q_msg_t * trq_get_by_rowid(tr_queue_t * trq, uint64_t rowid);

/**
    Check pending status of a rowid (low level)
    Return -1 if rowid not exists, 1 if pending, 0 if not pending
*/
PUBLIC int trq_check_pending_rowid(
    tr_queue_t * trq,
    uint64_t __t__,
    uint64_t rowid
);

/**
    Unload a message successfully from iter (disk TRQ_MSG_PENDING set to 0)
*/
PUBLIC void trq_unload_msg(q_msg_t *msg, int32_t result);

/**
    Mark a message.
    You must flag a message with TRQ_MSG_PENDING after append it to queue
        if you want recover it in the next open
        with the flag used in trq_load()
*/
PUBLIC int trq_set_hard_flag(q_msg_t *msg, uint32_t hard_mark, BOOL set);

/**
    Set soft mark
*/
PUBLIC uint64_t trq_set_soft_mark(q_msg_t *msg, uint64_t soft_mark, BOOL set);

/**
    Get if it's msg pending of ack
*/
static inline uint64_t trq_get_soft_mark(q_msg_t *msg)
{
    return msg->mark;
}

static inline q_msg_t *trq_first_msg(tr_queue_t * trq)
{
    return dl_first(&((tr_queue_t *)trq)->dl_q_msg);
}
static inline q_msg_t *trq_last_msg(tr_queue_t * trq)
{
    return dl_last(&((tr_queue_t *)trq)->dl_q_msg);
}

static inline q_msg_t *trq_next_msg(q_msg_t *msg)
{
    return dl_next(msg);
}
static inline q_msg_t *trq_prev_msg(q_msg_t *msg)
{
    return dl_prev(msg);
}

static inline md2_record_ex_t *trq_msg_md(q_msg_t *msg)
{
    return &msg->md_record;
}

/**
    Walk over instances
*/
#define qmsg_foreach_forward(trq, msg) \
    for((msg) = trq_first_msg(trq); \
        (msg)!=0 ; \
        (msg) = trq_next_msg(msg))

#define qmsg_foreach_forward_safe(trq, msg, next) \
   for((msg) = trq_first_msg(trq), (next) = (msg) ? trq_next_msg(msg) : 0; \
       (msg)!=0 ; \
       (msg) = (next), (next) = (msg) ? trq_next_msg(msg) : 0)

#define qmsg_foreach_backward(trq, msg) \
   for((msg) = trq_last_msg(trq); \
       (msg)!=0 ; \
       (msg) = trq_prev_msg(msg))

#define qmsg_foreach_backward_safe(trq, msg, prev) \
   for((msg) = trq_last_msg(trq), (prev) = trq_prev_msg(msg); \
       (msg)!=0 ; \
       (msg) = (prev), (prev) = trq_prev_msg(msg))

/**
    Get info of message
*/
PUBLIC md2_record_ex_t *trq_msg_md(q_msg_t *msg);
PUBLIC json_t *trq_msg_json(q_msg_t *msg); // Load the message, Return json is YOURS!!

static inline json_int_t trq_msg_rowid(q_msg_t *msg)
{
    return msg->rowid;
}
static inline uint64_t trq_msg_time(q_msg_t *msg)
{
    return msg->md_record.__t__;
}
static inline uint16_t trq_msg_hard_flag(q_msg_t *msg)
{
    return msg->md_record.user_flag;
}

/**
    Metadata
*/
PUBLIC int trq_set_metadata(
    json_t *kw,
    const char *key,
    json_t *jn_value // owned
);
PUBLIC json_t *trq_get_metadata( // WARNING The return json is not yours!
    json_t *kw
);

/**
    Return a new kw only with the message metadata
*/
PUBLIC json_t *trq_answer(
    json_t *jn_message,  // not owned, Gps message, to get only __MD_TRQ__
    int result
);

/**
    Do backup if needed.
*/
PUBLIC int trq_check_backup(tr_queue_t * trq);

#ifdef __cplusplus
}
#endif
