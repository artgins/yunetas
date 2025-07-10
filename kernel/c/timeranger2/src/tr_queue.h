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
    json_int_t rowid;
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
    Load pending messages
*/
PUBLIC int trq_load(tr_queue_t * trq);

/**
    Load all messages, filtering by rowid
*/
PUBLIC int trq_load_all(tr_queue_t * trq, int64_t from_rowid, int64_t to_rowid);

/**
    Load all messages, filtering by time
*/
PUBLIC int trq_load_all_by_time(tr_queue_t * trq, int64_t from_t, int64_t to_t);

/**
    Append a new message to queue simulating t
*/
PUBLIC q_msg_t * trq_append2(
    tr_queue_t * trq,
    json_int_t t,   // __t__
    json_t *jn_msg  // owned
);

/**
    Append a new message to queue
*/
static inline q_msg_t *trq_append(
    tr_queue_t * trq,
    json_t *jn_msg  // owned
)
{
    return trq_append2(trq, 0, jn_msg);
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
    Unload a message from iter
*/
PUBLIC void trq_unload_msg(q_msg_t *msg, int32_t result);

/**
    Mark a message.
    You must flag a message after append to queue
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
    for(msg = trq_first_msg(trq); \
        msg!=0 ; \
        msg = trq_next_msg(msg))

#define qmsg_foreach_forward_safe(trq, msg, next) \
    for(msg = trq_first_msg(trq), n = trq_next_msg(msg); \
        msg!=0 ; \
        msg = n, n = trq_next_msg(msg))


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
