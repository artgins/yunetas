/****************************************************************************
 *          TR2Q_MQTT.H
 *
 *          Defines and functions for tr2_queue md2_record_ex_t.user_flag
 *          and manage Persistent mqtt queues based in TimeRanger
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>
#include "timeranger2.h"

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

    dl_list_t dl_inflight;  // Queue with messages in memory (and in disk of course)
    dl_list_t dl_queued;    // Queue with messages in disk, avoiding overload of memory.
    uint64_t first_rowid;
} tr2_queue_t;

typedef struct {
    DL_ITEM_FIELDS

    tr2_queue_t *trq;
    md2_record_ex_t md_record;
    json_int_t rowid;       // global rowid that it must match the rowid in md_record
    uint16_t mid;           // Yes, it's an ease for mqtt protocol
    BOOL inflight;          // True if it's in inflight dl_list, otherwise in the queued dl_list
    json_t *kw_record;      // It may have gbuffer
} q2_msg_t;

/********************************************************************
 *  Bit Masks
 ********************************************************************/

#define TR2Q_MSG_PENDING    0x0001
#define TR2Q_RESERVED_MASK  0x000E
#define TR2Q_QOS_MASK       0x0030  // Only QoS bits (bits 4-5)
#define TR2Q_RETAIN_MASK    0x0040  // Retain bit (bit 6)
#define TR2Q_DUP_MASK       0x0080  // Dup bit (bit 7)
#define TR2Q_QOS_FLAGS_MASK 0x00F0  // All QoS-related flags (bits 4-7)
#define TR2Q_DIR_MASK       0x0300
#define TR2Q_ORIG_MASK      0x0C00
#define TR2Q_DIR_ORIG_MASK  0x0F00
#define TR2Q_STATE_MASK     0xF000

/********************************************************************
 *  Mosquitto Compatible Enums
 ********************************************************************/

typedef enum mqtt_msg_qos {
    mosq_m_qos0     = 0x0000,
    mosq_m_qos1     = 0x0010,
    mosq_m_qos2     = 0x0020
} mqtt_msg_qos_t;

typedef enum mqtt_msg_retain {
    mosq_m_retain   = 0x0040
} mqtt_msg_retain_t;

typedef enum mqtt_msg_dup {
    mosq_m_dup      = 0x0080
} mqtt_msg_dup_t;

typedef enum mqtt_msg_direction {
    mosq_md_in      = 0x0100,
    mosq_md_out     = 0x0200
} mqtt_msg_direction_t;

typedef enum mqtt_msg_origin {
    mosq_mo_client  = 0x0400,
    mosq_mo_broker  = 0x0800
} mqtt_msg_origin_t;

typedef enum mqtt_msg_state {
    mosq_ms_invalid             = 0x0000,
    mosq_ms_publish_qos0        = 0x1000,
    mosq_ms_publish_qos1        = 0x2000,
    mosq_ms_wait_for_puback     = 0x3000,
    mosq_ms_publish_qos2        = 0x4000,
    mosq_ms_wait_for_pubrec     = 0x5000,
    mosq_ms_resend_pubrel       = 0x6000,
    mosq_ms_wait_for_pubrel     = 0x7000,
    mosq_ms_resend_pubcomp      = 0x8000,
    mosq_ms_wait_for_pubcomp    = 0x9000,
    mosq_ms_send_pubrec         = 0xA000,
    mosq_ms_queued              = 0xB000
} mqtt_msg_state_t;

/********************************************************************
 *  User Flag Structure (md2_record.user_flag)
 ********************************************************************/

typedef struct {
    uint16_t tr2q_msg_pending: 1;
    uint16_t reserved: 3;
    uint16_t qos: 4;
    uint16_t dir_orig: 4;
    uint16_t state: 4;
} msg_flag_bitfield_t;

typedef union {
    uint16_t value;
    msg_flag_bitfield_t bits;
} msg_flag_t;

/***************************************************************
 *              Prototypes queues
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
    Move a message from queued list to inflight list
*/
PUBLIC int tr2q_move_from_queued_to_inflight(q2_msg_t *msg);

/**
    Unload a message from iter and hard mark with TR2Q_MSG_PENDING set to 0
*/
PUBLIC void tr2q_unload_msg(q2_msg_t *msg, int32_t result);

/**
    Get a message from iter by his rowid
*/
PUBLIC q2_msg_t *tr2q_get_by_rowid(tr2_queue_t *trq, uint64_t rowid);

/**
    Get a message from iter by his mid
*/
PUBLIC q2_msg_t *tr2q_get_by_mid(tr2_queue_t *trq, json_int_t mid);

/**
    Get the message content
 */
PUBLIC json_t *tr2q_msg_json(q2_msg_t *msg); // Return is not yours, free with tr2q_unload_msg()


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
    Save hard_mark (user_flag of timeranger2 metadata)
*/
PUBLIC int tr2q_save_hard_mark(q2_msg_t *msg, uint16_t value);

/**
    Get info of message
*/
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
    Do backup if needed.
*/
PUBLIC int tr2q_check_backup(tr2_queue_t *trq);

/**
    Walk over instances
*/
#define Q2MSG_FOREACH_FORWARD_INFLIGHT(trq, msg) \
    for((msg) = tr2q_first_inflight_msg(trq); \
        (msg)!=0 ; \
        (msg) = tr2q_next_msg(msg))

#define Q2MSG_FOREACH_FORWARD_INFLIGHT_SAFE(trq, msg, next) \
   for((msg) = tr2q_first_inflight_msg(trq), (next) = (msg) ? tr2q_next_msg(msg) : 0; \
       (msg)!=0 ; \
       (msg) = (next), (next) = (msg) ? tr2q_next_msg(msg) : 0)

#define Q2MSG_FOREACH_FORWARD_QUEUED(trq, msg) \
    for((msg) = tr2q_first_queued_msg(trq); \
        (msg)!=0 ; \
        (msg) = tr2q_next_msg(msg))

#define Q2MSG_FOREACH_FORWARD_QUEUED_SAFE(trq, msg, next) \
   for((msg) = tr2q_first_queued_msg(trq), (next) = (msg) ? tr2q_next_msg(msg) : 0; \
       (msg)!=0 ; \
       (msg) = (next), (next) = (msg) ? tr2q_next_msg(msg) : 0)

/********************************************************************
 *  Inline Functions - Pending
 ********************************************************************/

static inline void msg_flag_set_pending(msg_flag_t *uf, int pending) {
    if (pending) {
        uf->value |= TR2Q_MSG_PENDING;
    } else {
        uf->value &= ~TR2Q_MSG_PENDING;
    }
}

static inline int msg_flag_get_pending(const msg_flag_t *uf) {
    return (uf->value & TR2Q_MSG_PENDING) ? 1 : 0;
}

/********************************************************************
 *  Inline Functions - QoS Level (0, 1, 2)
 ********************************************************************/

static inline void msg_flag_set_qos(msg_flag_t *uf, mqtt_msg_qos_t qos) {
    uf->value = (uf->value & ~TR2Q_QOS_MASK) | (qos & TR2Q_QOS_MASK);
}

static inline mqtt_msg_qos_t msg_flag_get_qos(const msg_flag_t *uf) {
    return (mqtt_msg_qos_t)(uf->value & TR2Q_QOS_MASK);
}

static inline int msg_flag_get_qos_level(const msg_flag_t *uf) {
    uint16_t qos = uf->value & TR2Q_QOS_MASK;
    if (qos == mosq_m_qos2) return 2;
    if (qos == mosq_m_qos1) return 1;
    return 0;
}

static inline void msg_flag_set_qos_level(msg_flag_t *uf, uint8_t level) {
    uf->value &= ~TR2Q_QOS_MASK;
    switch (level) {
        case 1:
            uf->value |= mosq_m_qos1;
            break;
        case 2:
            uf->value |= mosq_m_qos2;
            break;
        default:
            uf->value |= mosq_m_qos0;
            break;
    }
}

/********************************************************************
 *  Inline Functions - Retain Flag
 ********************************************************************/

static inline void msg_flag_set_retain(msg_flag_t *uf, int retain) {
    if (retain) {
        uf->value |= TR2Q_RETAIN_MASK;
    } else {
        uf->value &= ~TR2Q_RETAIN_MASK;
    }
}

static inline int msg_flag_get_retain(const msg_flag_t *uf) {
    return (uf->value & TR2Q_RETAIN_MASK) ? 1 : 0;
}

/********************************************************************
 *  Inline Functions - Dup Flag
 ********************************************************************/

static inline void msg_flag_set_dup(msg_flag_t *uf, int dup) {
    if (dup) {
        uf->value |= TR2Q_DUP_MASK;
    } else {
        uf->value &= ~TR2Q_DUP_MASK;
    }
}

static inline int msg_flag_get_dup(const msg_flag_t *uf) {
    return (uf->value & TR2Q_DUP_MASK) ? 1 : 0;
}

/********************************************************************
 *  Inline Functions - Direction
 ********************************************************************/

static inline void msg_flag_set_direction(msg_flag_t *uf, mqtt_msg_direction_t dir) {
    uf->value = (uf->value & ~TR2Q_DIR_MASK) | (dir & TR2Q_DIR_MASK);
}

static inline mqtt_msg_direction_t msg_flag_get_direction(const msg_flag_t *uf) {
    return (mqtt_msg_direction_t)(uf->value & TR2Q_DIR_MASK);
}

/********************************************************************
 *  Inline Functions - Origin
 ********************************************************************/

static inline void msg_flag_set_origin(msg_flag_t *uf, mqtt_msg_origin_t orig) {
    uf->value = (uf->value & ~TR2Q_ORIG_MASK) | (orig & TR2Q_ORIG_MASK);
}

static inline mqtt_msg_origin_t msg_flag_get_origin(const msg_flag_t *uf) {
    return (mqtt_msg_origin_t)(uf->value & TR2Q_ORIG_MASK);
}

/********************************************************************
 *  Inline Functions - State
 ********************************************************************/

static inline void msg_flag_set_state(msg_flag_t *uf, mqtt_msg_state_t state) {
    uf->value = (uf->value & ~TR2Q_STATE_MASK) | (state & TR2Q_STATE_MASK);
}

static inline mqtt_msg_state_t msg_flag_get_state(const msg_flag_t *uf) {
    return (mqtt_msg_state_t)(uf->value & TR2Q_STATE_MASK);
}

/********************************************************************
 *  Inline Functions - Utility
 ********************************************************************/

static inline void msg_flag_clear(msg_flag_t *uf) {
    uf->value = 0;
}

static inline void msg_flag_init(msg_flag_t *uf, uint16_t value) {
    uf->value = value;
}

/********************************************************************
 *  Function Prototypes (Implemented in .c file)
 ********************************************************************/

PUBLIC const char *msg_flag_state_to_str(mqtt_msg_state_t state);
PUBLIC const char *msg_flag_direction_to_str(mqtt_msg_direction_t dir);
PUBLIC const char *msg_flag_origin_to_str(mqtt_msg_origin_t orig);

PUBLIC int build_queue_name(
    char *bf,
    size_t bfsize,
    const char *client_id,
    mqtt_msg_direction_t mqtt_msg_direction
);
