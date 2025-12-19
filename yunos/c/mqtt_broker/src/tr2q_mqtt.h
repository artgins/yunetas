/****************************************************************************
 *          TR2Q_MQTT.H
 *
 *          Defines and functions for tr2_queue md2_record_ex_t.user_flag
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

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
} user_flag_bitfield_t;

typedef union {
    uint16_t value;
    user_flag_bitfield_t bits;
} user_flag_t;

/********************************************************************
 *  Inline Functions - Pending
 ********************************************************************/

static inline void user_flag_set_pending(user_flag_t *uf, int pending) {
    if (pending) {
        uf->value |= TR2Q_MSG_PENDING;
    } else {
        uf->value &= ~TR2Q_MSG_PENDING;
    }
}

static inline int user_flag_get_pending(const user_flag_t *uf) {
    return (uf->value & TR2Q_MSG_PENDING) ? 1 : 0;
}

/********************************************************************
 *  Inline Functions - QoS Level (0, 1, 2)
 ********************************************************************/

static inline void user_flag_set_qos(user_flag_t *uf, mqtt_msg_qos_t qos) {
    uf->value = (uf->value & ~TR2Q_QOS_MASK) | (qos & TR2Q_QOS_MASK);
}

static inline mqtt_msg_qos_t user_flag_get_qos(const user_flag_t *uf) {
    return (mqtt_msg_qos_t)(uf->value & TR2Q_QOS_MASK);
}

static inline int user_flag_get_qos_level(const user_flag_t *uf) {
    uint16_t qos = uf->value & TR2Q_QOS_MASK;
    if (qos == mosq_m_qos2) return 2;
    if (qos == mosq_m_qos1) return 1;
    return 0;
}

static inline void user_flag_set_qos_level(user_flag_t *uf, int level) {
    uf->value &= ~TR2Q_QOS_MASK;
    switch (level) {
        case 1:
            uf->value |= mosq_m_qos1;
            break;
        case 2:
            uf->value |= mosq_m_qos2;
            break;
        default:
            // QoS 0, nothing to set
            break;
    }
}

/********************************************************************
 *  Inline Functions - Retain Flag
 ********************************************************************/

static inline void user_flag_set_retain(user_flag_t *uf, int retain) {
    if (retain) {
        uf->value |= TR2Q_RETAIN_MASK;
    } else {
        uf->value &= ~TR2Q_RETAIN_MASK;
    }
}

static inline int user_flag_get_retain(const user_flag_t *uf) {
    return (uf->value & TR2Q_RETAIN_MASK) ? 1 : 0;
}

/********************************************************************
 *  Inline Functions - Dup Flag
 ********************************************************************/

static inline void user_flag_set_dup(user_flag_t *uf, int dup) {
    if (dup) {
        uf->value |= TR2Q_DUP_MASK;
    } else {
        uf->value &= ~TR2Q_DUP_MASK;
    }
}

static inline int user_flag_get_dup(const user_flag_t *uf) {
    return (uf->value & TR2Q_DUP_MASK) ? 1 : 0;
}

/********************************************************************
 *  Inline Functions - Direction
 ********************************************************************/

static inline void user_flag_set_direction(user_flag_t *uf, mqtt_msg_direction_t dir) {
    uf->value = (uf->value & ~TR2Q_DIR_MASK) | (dir & TR2Q_DIR_MASK);
}

static inline mqtt_msg_direction_t user_flag_get_direction(const user_flag_t *uf) {
    return (mqtt_msg_direction_t)(uf->value & TR2Q_DIR_MASK);
}

/********************************************************************
 *  Inline Functions - Origin
 ********************************************************************/

static inline void user_flag_set_origin(user_flag_t *uf, mqtt_msg_origin_t orig) {
    uf->value = (uf->value & ~TR2Q_ORIG_MASK) | (orig & TR2Q_ORIG_MASK);
}

static inline mqtt_msg_origin_t user_flag_get_origin(const user_flag_t *uf) {
    return (mqtt_msg_origin_t)(uf->value & TR2Q_ORIG_MASK);
}

/********************************************************************
 *  Inline Functions - State
 ********************************************************************/

static inline void user_flag_set_state(user_flag_t *uf, mqtt_msg_state_t state) {
    uf->value = (uf->value & ~TR2Q_STATE_MASK) | (state & TR2Q_STATE_MASK);
}

static inline mqtt_msg_state_t user_flag_get_state(const user_flag_t *uf) {
    return (mqtt_msg_state_t)(uf->value & TR2Q_STATE_MASK);
}

/********************************************************************
 *  Inline Functions - Utility
 ********************************************************************/

static inline void user_flag_clear(user_flag_t *uf) {
    uf->value = 0;
}

static inline void user_flag_init(user_flag_t *uf, uint16_t value) {
    uf->value = value;
}

/********************************************************************
 *  Function Prototypes (Implemented in .c file)
 ********************************************************************/

PUBLIC const char *user_flag_state_to_str(mqtt_msg_state_t state);
PUBLIC const char *user_flag_direction_to_str(mqtt_msg_direction_t dir);
PUBLIC const char *user_flag_origin_to_str(mqtt_msg_origin_t orig);
