/****************************************************************************
 *          tr_mqtt_queue.h
 *
 *          MQTT Persistent queue based in TimeRanger
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
#define __MD_TRMQTT__ "__md_trmqtt__"

#define TRQ_MSG_QSO1   0x0001
#define TRQ_MSG_QSO2   0x0002

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
} tr_mqtt_t;

typedef struct {
    DL_ITEM_FIELDS

    tr_mqtt_t *trq;
    md2_record_ex_t md_record;
    uint64_t mark;          // soft mark.
    json_int_t rowid;
} mq_msg_t;


/***************************************************************
 *              Prototypes
 ***************************************************************/

/**
    Open queue (Remember previously open tranger2_startup())
*/
PUBLIC tr_mqtt_t *trmq_open(
    json_t *tranger,
    const char *topic_name,
    const char *tkey,
    system_flag2_t system_flag, // KEY_TYPE_MASK2 forced to sf_rowid_key
    size_t backup_queue_size
);

/**
    Close queue (After close the queue, remember do tranger2_shutdown())
*/
PUBLIC void trmq_close(tr_mqtt_t * trq);

/**
    Return size of queue (messages in queue)
*/
static inline size_t trmq_size(tr_mqtt_t * trq)
{
    return dl_size(&trq->dl_q_msg);
}

/**
    Return tranger of queue
*/
static inline json_t * trmq_tranger(tr_mqtt_t * trq)
{
    return trq->tranger;
}


/**
    Return topic of queue
*/
static inline json_t * trmq_topic(tr_mqtt_t * trq)
{
    return trq->topic;
}


/**
    Load pending messages
*/
PUBLIC int trmq_load(tr_mqtt_t * trq);

/**
    Load all messages, filtering by rowid
*/
PUBLIC int trmq_load_all(tr_mqtt_t * trq, int64_t from_rowid, int64_t to_rowid);

/**
    Load all messages, filtering by time
*/
PUBLIC int trmq_load_all_by_time(tr_mqtt_t * trq, int64_t from_t, int64_t to_t);

/**
    Append a new message to queue simulating t
*/
PUBLIC mq_msg_t * trmq_append2(
    tr_mqtt_t * trq,
    json_int_t t,   // __t__
    json_t *kw  // owned
);

/**
    Append a new message to queue
*/
static inline mq_msg_t *trmq_append(
    tr_mqtt_t * trq,
    json_t *kw  // owned
)
{
    return trmq_append2(trq, 0, kw);
}

/**
    Get a message from iter by his rowid
*/
PUBLIC mq_msg_t * trmq_get_by_rowid(tr_mqtt_t * trq, uint64_t rowid);

/**
    Check pending status of a rowid (low level)
    Return -1 if rowid not exists, 1 if pending, 0 if not pending
*/
PUBLIC int trmq_check_pending_rowid(
    tr_mqtt_t * trq,
    uint64_t __t__,
    uint64_t rowid
);

/**
    Unload a message successfully from iter (TRQ_MSG_PENDING set to 0)
*/
PUBLIC void trmq_unload_msg(mq_msg_t *msg, int32_t result);

/**
    Mark a message.
    You must flag a message after append to queue
        if you want recover it in the next open
        with the flag used in trmq_load()
*/
PUBLIC int trmq_set_hard_flag(mq_msg_t *msg, uint32_t hard_mark, BOOL set);

/**
    Set soft mark
*/
PUBLIC uint64_t trmq_set_soft_mark(mq_msg_t *msg, uint64_t soft_mark, BOOL set);

/**
    Get if it's msg pending of ack
*/
static inline uint64_t trmq_get_soft_mark(mq_msg_t *msg)
{
    return msg->mark;
}

static inline mq_msg_t *trmq_first_msg(tr_mqtt_t * trq)
{
    return dl_first(&((tr_mqtt_t *)trq)->dl_q_msg);
}
static inline mq_msg_t *trmq_last_msg(tr_mqtt_t * trq)
{
    return dl_last(&((tr_mqtt_t *)trq)->dl_q_msg);
}

static inline mq_msg_t *trmq_next_msg(mq_msg_t *msg)
{
    return dl_next(msg);
}
static inline mq_msg_t *trmq_prev_msg(mq_msg_t *msg)
{
    return dl_prev(msg);
}

static inline md2_record_ex_t *trmq_msg_md(mq_msg_t *msg)
{
    return &msg->md_record;
}

/**
    Walk over instances
*/
#define qmsg_foreach_forward(trq, msg) \
    for(msg = trmq_first_msg(trq); \
        msg!=0 ; \
        msg = trmq_next_msg(msg))

#define qmsg_foreach_forward_safe(trq, msg, next) \
    for(msg = trmq_first_msg(trq), n = trmq_next_msg(msg); \
        msg!=0 ; \
        msg = n, n = trmq_next_msg(msg))


/**
    Get info of message
*/
PUBLIC md2_record_ex_t *trmq_msg_md(mq_msg_t *msg);
PUBLIC json_t *trmq_msg_json(mq_msg_t *msg); // Load the message, Return json is YOURS!!

static inline json_int_t trmq_msg_rowid(mq_msg_t *msg)
{
    return msg->rowid;
}
static inline uint64_t trmq_msg_time(mq_msg_t *msg)
{
    return msg->md_record.__t__;
}

/**
    Metadata
*/
PUBLIC int trmq_set_metadata(
    json_t *kw,
    const char *key,
    json_t *jn_value // owned
);
PUBLIC json_t *trmq_get_metadata( // WARNING The return json is not yours!
    json_t *kw
);

/**
    Return a new kw only with the message metadata
*/
PUBLIC json_t *trmq_answer(
    json_t *jn_message,  // not owned, Gps message, to get only __MD_TRQ__
    int result
);

/**
    Do backup if needed.
*/
PUBLIC int trmq_check_backup(tr_mqtt_t * trq);

#ifdef __cplusplus
}
#endif
