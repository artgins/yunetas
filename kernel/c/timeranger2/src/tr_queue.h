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

#include <time.h>
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
typedef void *tr_queue;
typedef void *q_msg;

/***************************************************************
 *              Prototypes
 ***************************************************************/

/**
    Open queue (Remember previously open tranger2_startup())
*/
PUBLIC tr_queue trq_open(
    json_t *tranger,
    const char *topic_name,
    const char *pkey,
    const char *tkey,
    system_flag2_t system_flag,
    size_t backup_queue_size
);

/**
    Close queue (After close the queue, remember do tranger2_shutdown())
*/
PUBLIC void trq_close(tr_queue trq);

/**
    Return size of queue (messages in queue)
*/
PUBLIC size_t trq_size(tr_queue trq);

/**
    Return tranger of queue
*/
PUBLIC json_t * trq_tranger(tr_queue trq);

/**
    Return topic of queue
*/
PUBLIC json_t * trq_topic(tr_queue trq);

/**
    Set from_rowid to improve speed
*/
PUBLIC void trq_set_first_rowid(tr_queue trq, uint64_t first_rowid);

/**
    Load pending messages, return a iter
*/
PUBLIC int trq_load(tr_queue trq);

/**
    Load all messages, return a iter
*/
PUBLIC int trq_load_all(tr_queue trq, const char *key, int64_t from_rowid, int64_t to_rowid);

/**
    Append a new message to queue
*/
PUBLIC q_msg trq_append(
    tr_queue trq,
    json_t *jn_msg  // owned
);

/**
    Append a new message to queue simulating t
*/
PUBLIC q_msg trq_append2(
    tr_queue trq,
    json_int_t t,   // __t__
    json_t *jn_msg  // owned
);

/**
    Get a message from iter by his rowid
*/
PUBLIC q_msg trq_get_by_rowid(tr_queue trq, uint64_t rowid);

/**
    Get number of messages from iter by his key
*/
PUBLIC int trq_size_by_key(tr_queue trq, const char *key);

/**
    Check pending status of a rowid (low level)
    Return -1 if rowid not exists, 1 if pending, 0 if not pending
*/
PUBLIC int trq_check_pending_rowid(
    tr_queue trq_,
    const char *key,        // In tranger2 ('key', '__t__', 'rowid') is required
    uint64_t __t__,
    uint64_t rowid
);

/**
    Unload a message from iter
*/
PUBLIC void trq_unload_msg(q_msg msg, int32_t result);

/**
    Mark a message.
    You must flag a message after append to queue
        if you want recover it in the next open
        with the flag used in trq_load()
*/
PUBLIC int trq_set_hard_flag(q_msg msg, uint32_t hard_mark, BOOL set);

/**
    Set soft mark
*/
PUBLIC uint64_t trq_set_soft_mark(q_msg msg, uint64_t soft_mark, BOOL set);

/**
    Get if it's msg pending of ack
*/
PUBLIC uint64_t trq_get_soft_mark(q_msg msg);

/**
    Set ack timer
*/
PUBLIC time_t trq_set_ack_timer(q_msg msg, time_t seconds);

/**
    Clear ack timer
*/
PUBLIC void trq_clear_ack_timer(q_msg msg);

/**
    Test ack timer
*/
PUBLIC BOOL trq_test_ack_timer(q_msg msg);

/**
    Set maximum retries
*/
PUBLIC void trq_set_maximum_retries(tr_queue trq, int maximum_retries);

/**
    Add retries
*/
PUBLIC void trq_add_retries(q_msg msg, int retries);

/**
    Clear retries
*/
PUBLIC void trq_clear_retries(q_msg msg);

/**
    Test retries
*/
PUBLIC BOOL trq_test_retries(q_msg msg);

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

#define qmsg_foreach_backward(trq, msg) \
    for(msg = trq_last_msg(trq); \
        msg!=0 ; \
        msg = trq_prev_msg(msg))

/**
 *  Example

    q_msg msg;
    qmsg_foreach_forward(trq, msg) {
        json_t *jn_gps_msg = trq_msg_json(msg);
    }

*/
PUBLIC q_msg trq_first_msg(tr_queue trq);
PUBLIC q_msg trq_last_msg(tr_queue trq);
PUBLIC q_msg trq_next_msg(q_msg msg);
PUBLIC q_msg trq_prev_msg(q_msg msg);

/**
    Get info of message
*/
PUBLIC md2_record_ex_t *trq_msg_md(q_msg msg);
PUBLIC json_int_t trq_msg_rowid(q_msg msg);
PUBLIC json_t *trq_msg_json(q_msg msg); // Load the message, Return json is NOT YOURS!!
PUBLIC uint64_t trq_msg_time(q_msg msg);
PUBLIC const char *trq_msg_key(q_msg msg);

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
PUBLIC int trq_check_backup(tr_queue trq);

#ifdef __cplusplus
}
#endif
