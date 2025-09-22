/***********************************************************************
 *          C_MQTT.C
 *          GClass of MQTT protocol.
 *
 *          A lot of code is inspired in the great mosquitto project:
 *
 *              Copyright (c) 2009-2020 Roger Light <roger@atchoo.org>
 *              All rights reserved. This program and the accompanying materials
 *              are made available under the terms of the Eclipse Public License 2.0
 *              and Eclipse Distribution License v1.0 which accompany this distribution.
 *              The Eclipse Public License is available at
 *              https://www.eclipse.org/legal/epl-2.0/
 *              and the Eclipse Distribution License is available at
 *              http://www.eclipse.org/org/documents/edl-v10.php.
 *              SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 *
 *          Copyright (c) 2022 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdint.h>
#include <endian.h>
#include <time.h>
#include <arpa/inet.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>

#if defined(CONFIG_HAVE_OPENSSL)
    #include <openssl/ssl.h>
    #include <openssl/rand.h>
#elif defined(CONFIG_HAVE_MBEDTLS)
    #include <mbedtls/md.h>
    #include <mbedtls/ctr_drbg.h>
    #include <mbedtls/pkcs5.h>
#else
    #error "No crypto library defined"
#endif

#include "command_parser.h"
#include "msg_ievent.h"
#include "c_timer.h"
#include "c_tcp.h"
#include "istream.h"
#include "c_prot_mqtt.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define MOSQ_MSB(A) (uint8_t)((A & 0xFF00) >> 8)
#define MOSQ_LSB(A) (uint8_t)(A & 0x00FF)

#define UNUSED(A) (void)(A)

#define PW_DEFAULT_ITERATIONS 101

#define PROTOCOL_NAME_v31 "MQIsdp"
#define PROTOCOL_VERSION_v31 3

#define PROTOCOL_NAME "MQTT"

#define PROTOCOL_VERSION_v311 4
#define PROTOCOL_VERSION_v5 5

#define TOPIC_HIERARCHY_LIMIT 200
#define TOPIC_MAX 4000

#define SAFE_PRINT(A) (A)?(A):""

/* Message types */
typedef enum {
    CMD_RESERVED    = 0x00U,
    CMD_CONNECT     = 0x10U,
    CMD_CONNACK     = 0x20U,
    CMD_PUBLISH     = 0x30U,
    CMD_PUBACK      = 0x40U,
    CMD_PUBREC      = 0x50U,
    CMD_PUBREL      = 0x60U,
    CMD_PUBCOMP     = 0x70U,
    CMD_SUBSCRIBE   = 0x80U,
    CMD_SUBACK      = 0x90U,
    CMD_UNSUBSCRIBE = 0xA0U,
    CMD_UNSUBACK    = 0xB0U,
    CMD_PINGREQ     = 0xC0U,
    CMD_PINGRESP    = 0xD0U,
    CMD_DISCONNECT  = 0xE0U,
    CMD_AUTH        = 0xF0U
} mqtt_message_t;

/* Mosquitto only: for distinguishing CONNECT and WILL properties */
#define CMD_WILL 0x100

/* Enum: mqtt311_connack_codes
 *
 * The CONNACK results for MQTT v3.1.1, and v3.1.
 *
 * Values:
 *  CONNACK_ACCEPTED - 0
 *  CONNACK_REFUSED_PROTOCOL_VERSION - 1
 *  CONNACK_REFUSED_IDENTIFIER_REJECTED - 2
 *  CONNACK_REFUSED_SERVER_UNAVAILABLE - 3
 *  CONNACK_REFUSED_BAD_USERNAME_PASSWORD - 4
 *  CONNACK_REFUSED_NOT_AUTHORIZED - 5
 */
enum mqtt311_connack_codes {
    CONNACK_ACCEPTED = 0,
    CONNACK_REFUSED_PROTOCOL_VERSION = 1,
    CONNACK_REFUSED_IDENTIFIER_REJECTED = 2,
    CONNACK_REFUSED_SERVER_UNAVAILABLE = 3,
    CONNACK_REFUSED_BAD_USERNAME_PASSWORD = 4,
    CONNACK_REFUSED_NOT_AUTHORIZED = 5,
};

/* Enum: mqtt5_return_codes
 * The reason codes returned in various MQTT commands.
 *
 * Values:
 *  MQTT_RC_SUCCESS - 0
 *  MQTT_RC_NORMAL_DISCONNECTION - 0
 *  MQTT_RC_GRANTED_QOS0 - 0
 *  MQTT_RC_GRANTED_QOS1 - 1
 *  MQTT_RC_GRANTED_QOS2 - 2
 *  MQTT_RC_DISCONNECT_WITH_WILL_MSG - 4
 *  MQTT_RC_NO_MATCHING_SUBSCRIBERS - 16
 *  MQTT_RC_NO_SUBSCRIPTION_EXISTED - 17
 *  MQTT_RC_CONTINUE_AUTHENTICATION - 24
 *  MQTT_RC_REAUTHENTICATE - 25
 *  MQTT_RC_UNSPECIFIED - 128
 *  MQTT_RC_MALFORMED_PACKET - 129
 *  MQTT_RC_PROTOCOL_ERROR - 130
 *  MQTT_RC_IMPLEMENTATION_SPECIFIC - 131
 *  MQTT_RC_UNSUPPORTED_PROTOCOL_VERSION - 132
 *  MQTT_RC_CLIENTID_NOT_VALID - 133
 *  MQTT_RC_BAD_USERNAME_OR_PASSWORD - 134
 *  MQTT_RC_NOT_AUTHORIZED - 135
 *  MQTT_RC_SERVER_UNAVAILABLE - 136
 *  MQTT_RC_SERVER_BUSY - 137
 *  MQTT_RC_BANNED - 138
 *  MQTT_RC_SERVER_SHUTTING_DOWN - 139
 *  MQTT_RC_BAD_AUTHENTICATION_METHOD - 140
 *  MQTT_RC_KEEP_ALIVE_TIMEOUT - 141
 *  MQTT_RC_SESSION_TAKEN_OVER - 142
 *  MQTT_RC_TOPIC_FILTER_INVALID - 143
 *  MQTT_RC_TOPIC_NAME_INVALID - 144
 *  MQTT_RC_PACKET_ID_IN_USE - 145
 *  MQTT_RC_PACKET_ID_NOT_FOUND - 146
 *  MQTT_RC_RECEIVE_MAXIMUM_EXCEEDED - 147
 *  MQTT_RC_TOPIC_ALIAS_INVALID - 148
 *  MQTT_RC_PACKET_TOO_LARGE - 149
 *  MQTT_RC_MESSAGE_RATE_TOO_HIGH - 150
 *  MQTT_RC_QUOTA_EXCEEDED - 151
 *  MQTT_RC_ADMINISTRATIVE_ACTION - 152
 *  MQTT_RC_PAYLOAD_FORMAT_INVALID - 153
 *  MQTT_RC_RETAIN_NOT_SUPPORTED - 154
 *  MQTT_RC_QOS_NOT_SUPPORTED - 155
 *  MQTT_RC_USE_ANOTHER_SERVER - 156
 *  MQTT_RC_SERVER_MOVED - 157
 *  MQTT_RC_SHARED_SUBS_NOT_SUPPORTED - 158
 *  MQTT_RC_CONNECTION_RATE_EXCEEDED - 159
 *  MQTT_RC_MAXIMUM_CONNECT_TIME - 160
 *  MQTT_RC_SUBSCRIPTION_IDS_NOT_SUPPORTED - 161
 *  MQTT_RC_WILDCARD_SUBS_NOT_SUPPORTED - 162
 */
enum mqtt5_return_codes {
    MQTT_RC_SUCCESS = 0,    /* CONNACK, PUBACK, PUBREC, PUBREL, PUBCOMP, UNSUBACK, AUTH */
    MQTT_RC_NORMAL_DISCONNECTION = 0,           /* DISCONNECT */
    MQTT_RC_GRANTED_QOS0 = 0,                   /* SUBACK */
    MQTT_RC_GRANTED_QOS1 = 1,                   /* SUBACK */
    MQTT_RC_GRANTED_QOS2 = 2,                   /* SUBACK */
    MQTT_RC_DISCONNECT_WITH_WILL_MSG = 4,       /* DISCONNECT */
    MQTT_RC_NO_MATCHING_SUBSCRIBERS = 16,       /* PUBACK, PUBREC */
    MQTT_RC_NO_SUBSCRIPTION_EXISTED = 17,       /* UNSUBACK */
    MQTT_RC_CONTINUE_AUTHENTICATION = 24,       /* AUTH */
    MQTT_RC_REAUTHENTICATE = 25,                /* AUTH */

    MQTT_RC_UNSPECIFIED = 128,      /* CONNACK, PUBACK, PUBREC, SUBACK, UNSUBACK, DISCONNECT */
    MQTT_RC_MALFORMED_PACKET = 129,             /* CONNACK, DISCONNECT */
    MQTT_RC_PROTOCOL_ERROR = 130,               /* DISCONNECT */
    MQTT_RC_IMPLEMENTATION_SPECIFIC = 131, /* CONNACK, PUBACK, PUBREC, SUBACK, UNSUBACK, DISCONNECT */
    MQTT_RC_UNSUPPORTED_PROTOCOL_VERSION = 132, /* CONNACK */
    MQTT_RC_CLIENTID_NOT_VALID = 133,           /* CONNACK */
    MQTT_RC_BAD_USERNAME_OR_PASSWORD = 134,     /* CONNACK */
    MQTT_RC_NOT_AUTHORIZED = 135,        /* CONNACK, PUBACK, PUBREC, SUBACK, UNSUBACK, DISCONNECT */
    MQTT_RC_SERVER_UNAVAILABLE = 136,           /* CONNACK */
    MQTT_RC_SERVER_BUSY = 137,                  /* CONNACK, DISCONNECT */
    MQTT_RC_BANNED = 138,                       /* CONNACK */
    MQTT_RC_SERVER_SHUTTING_DOWN = 139,         /* DISCONNECT */
    MQTT_RC_BAD_AUTHENTICATION_METHOD = 140,    /* CONNACK */
    MQTT_RC_KEEP_ALIVE_TIMEOUT = 141,           /* DISCONNECT */
    MQTT_RC_SESSION_TAKEN_OVER = 142,           /* DISCONNECT */
    MQTT_RC_TOPIC_FILTER_INVALID = 143,         /* SUBACK, UNSUBACK, DISCONNECT */
    MQTT_RC_TOPIC_NAME_INVALID = 144,           /* CONNACK, PUBACK, PUBREC, DISCONNECT */
    MQTT_RC_PACKET_ID_IN_USE = 145,             /* PUBACK, SUBACK, UNSUBACK */
    MQTT_RC_PACKET_ID_NOT_FOUND = 146,          /* PUBREL, PUBCOMP */
    MQTT_RC_RECEIVE_MAXIMUM_EXCEEDED = 147,     /* DISCONNECT */
    MQTT_RC_TOPIC_ALIAS_INVALID = 148,          /* DISCONNECT */
    MQTT_RC_PACKET_TOO_LARGE = 149,             /* CONNACK, PUBACK, PUBREC, DISCONNECT */
    MQTT_RC_MESSAGE_RATE_TOO_HIGH = 150,        /* DISCONNECT */
    MQTT_RC_QUOTA_EXCEEDED = 151,               /* PUBACK, PUBREC, SUBACK, DISCONNECT */
    MQTT_RC_ADMINISTRATIVE_ACTION = 152,        /* DISCONNECT */
    MQTT_RC_PAYLOAD_FORMAT_INVALID = 153,       /* CONNACK, PUBACK, PUBREC, DISCONNECT */
    MQTT_RC_RETAIN_NOT_SUPPORTED = 154,         /* CONNACK, DISCONNECT */
    MQTT_RC_QOS_NOT_SUPPORTED = 155,            /* CONNACK, DISCONNECT */
    MQTT_RC_USE_ANOTHER_SERVER = 156,           /* CONNACK, DISCONNECT */
    MQTT_RC_SERVER_MOVED = 157,                 /* CONNACK, DISCONNECT */
    MQTT_RC_SHARED_SUBS_NOT_SUPPORTED = 158,    /* SUBACK, DISCONNECT */
    MQTT_RC_CONNECTION_RATE_EXCEEDED = 159,     /* CONNACK, DISCONNECT */
    MQTT_RC_MAXIMUM_CONNECT_TIME = 160,         /* DISCONNECT */
    MQTT_RC_SUBSCRIPTION_IDS_NOT_SUPPORTED = 161,   /* SUBACK, DISCONNECT */
    MQTT_RC_WILDCARD_SUBS_NOT_SUPPORTED = 162,      /* SUBACK, DISCONNECT */
};

/* Enum: mqtt5_property
 * Options for use with MQTTv5 properties.
 * Options:
 *
 *    MQTT_PROP_PAYLOAD_FORMAT_INDICATOR - property option.
 *    MQTT_PROP_MESSAGE_EXPIRY_INTERVAL - property option.
 *    MQTT_PROP_CONTENT_TYPE - property option.
 *    MQTT_PROP_RESPONSE_TOPIC - property option.
 *    MQTT_PROP_CORRELATION_DATA - property option.
 *    MQTT_PROP_SUBSCRIPTION_IDENTIFIER - property option.
 *    MQTT_PROP_SESSION_EXPIRY_INTERVAL - property option.
 *    MQTT_PROP_ASSIGNED_CLIENT_IDENTIFIER - property option.
 *    MQTT_PROP_SERVER_KEEP_ALIVE - property option.
 *    MQTT_PROP_AUTHENTICATION_METHOD - property option.
 *    MQTT_PROP_AUTHENTICATION_DATA - property option.
 *    MQTT_PROP_REQUEST_PROBLEM_INFORMATION - property option.
 *    MQTT_PROP_WILL_DELAY_INTERVAL - property option.
 *    MQTT_PROP_REQUEST_RESPONSE_INFORMATION - property option.
 *    MQTT_PROP_RESPONSE_INFORMATION - property option.
 *    MQTT_PROP_SERVER_REFERENCE - property option.
 *    MQTT_PROP_REASON_STRING - property option.
 *    MQTT_PROP_RECEIVE_MAXIMUM - property option.
 *    MQTT_PROP_TOPIC_ALIAS_MAXIMUM - property option.
 *    MQTT_PROP_TOPIC_ALIAS - property option.
 *    MQTT_PROP_MAXIMUM_QOS - property option.
 *    MQTT_PROP_RETAIN_AVAILABLE - property option.
 *    MQTT_PROP_USER_PROPERTY - property option.
 *    MQTT_PROP_MAXIMUM_PACKET_SIZE - property option.
 *    MQTT_PROP_WILDCARD_SUB_AVAILABLE - property option.
 *    MQTT_PROP_SUBSCRIPTION_ID_AVAILABLE - property option.
 *    MQTT_PROP_SHARED_SUB_AVAILABLE - property option.
 */
enum mqtt5_property {
    MQTT_PROP_PAYLOAD_FORMAT_INDICATOR = 1,     /* Byte :               PUBLISH, Will Properties */
    MQTT_PROP_MESSAGE_EXPIRY_INTERVAL = 2,      /* 4 byte int :         PUBLISH, Will Properties */
    MQTT_PROP_CONTENT_TYPE = 3,                 /* UTF-8 string :       PUBLISH, Will Properties */
    MQTT_PROP_RESPONSE_TOPIC = 8,               /* UTF-8 string :       PUBLISH, Will Properties */
    MQTT_PROP_CORRELATION_DATA = 9,             /* Binary Data :        PUBLISH, Will Properties */
    MQTT_PROP_SUBSCRIPTION_IDENTIFIER = 11,     /* Variable byte int :  PUBLISH, SUBSCRIBE */
    MQTT_PROP_SESSION_EXPIRY_INTERVAL = 17,     /* 4 byte int :         CONNECT, CONNACK, DISCONNECT */
    MQTT_PROP_ASSIGNED_CLIENT_IDENTIFIER = 18,  /* UTF-8 string :       CONNACK */
    MQTT_PROP_SERVER_KEEP_ALIVE = 19,           /* 2 byte int :         CONNACK */
    MQTT_PROP_AUTHENTICATION_METHOD = 21,       /* UTF-8 string :       CONNECT, CONNACK, AUTH */
    MQTT_PROP_AUTHENTICATION_DATA = 22,         /* Binary Data :        CONNECT, CONNACK, AUTH */
    MQTT_PROP_REQUEST_PROBLEM_INFORMATION = 23, /* Byte :               CONNECT */
    MQTT_PROP_WILL_DELAY_INTERVAL = 24,         /* 4 byte int :         Will properties */
    MQTT_PROP_REQUEST_RESPONSE_INFORMATION = 25,/* Byte :               CONNECT */
    MQTT_PROP_RESPONSE_INFORMATION = 26,        /* UTF-8 string :       CONNACK */
    MQTT_PROP_SERVER_REFERENCE = 28,            /* UTF-8 string :       CONNACK, DISCONNECT */
    MQTT_PROP_REASON_STRING = 31,               /* UTF-8 string :       All except Will properties */
    MQTT_PROP_RECEIVE_MAXIMUM = 33,             /* 2 byte int :         CONNECT, CONNACK */
    MQTT_PROP_TOPIC_ALIAS_MAXIMUM = 34,         /* 2 byte int :         CONNECT, CONNACK */
    MQTT_PROP_TOPIC_ALIAS = 35,                 /* 2 byte int :         PUBLISH */
    MQTT_PROP_MAXIMUM_QOS = 36,                 /* Byte :               CONNACK */
    MQTT_PROP_RETAIN_AVAILABLE = 37,            /* Byte :               CONNACK */
    MQTT_PROP_USER_PROPERTY = 38,               /* UTF-8 string pair :  All */
    MQTT_PROP_MAXIMUM_PACKET_SIZE = 39,         /* 4 byte int :         CONNECT, CONNACK */
    MQTT_PROP_WILDCARD_SUB_AVAILABLE = 40,      /* Byte :               CONNACK */
    MQTT_PROP_SUBSCRIPTION_ID_AVAILABLE = 41,   /* Byte :               CONNACK */
    MQTT_PROP_SHARED_SUB_AVAILABLE = 42,        /* Byte :               CONNACK */
};

enum mqtt5_property_type {
    MQTT_PROP_TYPE_BYTE = 1,
    MQTT_PROP_TYPE_INT16 = 2,
    MQTT_PROP_TYPE_INT32 = 3,
    MQTT_PROP_TYPE_VARINT = 4,
    MQTT_PROP_TYPE_BINARY = 5,
    MQTT_PROP_TYPE_STRING = 6,
    MQTT_PROP_TYPE_STRING_PAIR = 7
};

/* Enum: mqtt5_sub_options
 * Options for use with MQTTv5 subscriptions.
 *
 * What is a retained Message?
 *  Each client that subscribes to a topic pattern that matches the topic of the retained message
 *  receives the retained message immediately after they subscribe.
 *  The broker stores only one retained message per topic.
 *
 * MQTT_SUB_OPT_NO_LOCAL - with this option set, if this client publishes to
 * a topic to which it is subscribed, the broker will not publish the
 * message back to the client.
 *
 * MQTT_SUB_OPT_RETAIN_AS_PUBLISHED - with this option set, messages
 * published for this subscription will keep the retain flag as was set by
 * the publishing client. The default behaviour without this option set has
 * the retain flag indicating whether a message is fresh/stale.
 *
 * MQTT_SUB_OPT_SEND_RETAIN_ALWAYS - with this option set, pre-existing
 * retained messages are sent as soon as the subscription is made, even
 * if the subscription already exists. This is the default behaviour, so
 * it is not necessary to set this option.
 *
 * MQTT_SUB_OPT_SEND_RETAIN_NEW - with this option set, pre-existing retained
 * messages for this subscription will be sent when the subscription is made,
 * but only if the subscription does not already exist.
 *
 * MQTT_SUB_OPT_SEND_RETAIN_NEVER - with this option set, pre-existing
 * retained messages will never be sent for this subscription.
 */
enum mqtt5_sub_options {
    MQTT_SUB_OPT_NO_LOCAL = 0x04,
    MQTT_SUB_OPT_RETAIN_AS_PUBLISHED = 0x08,
    MQTT_SUB_OPT_SEND_RETAIN_ALWAYS = 0x00,
    MQTT_SUB_OPT_SEND_RETAIN_NEW = 0x10,
    MQTT_SUB_OPT_SEND_RETAIN_NEVER = 0x20,
};

#define MQTT_MAX_PAYLOAD 268435455U

typedef enum mosquitto_protocol {
    mosq_p_invalid = 0,
    mosq_p_mqtt31 = 1,
    mosq_p_mqtt311 = 2,
    mosq_p_mqtts = 3,
    mosq_p_mqtt5 = 5,
} mosquitto_protocol_t ;

/* Error values */
typedef enum mosq_err_t {
    MOSQ_ERR_SUCCESS = 0,
    MOSQ_ERR_PROTOCOL = -2,
    MOSQ_ERR_INVAL = -3,
    MOSQ_ERR_NO_CONN = -4,
    MOSQ_ERR_CONN_REFUSED = -5,
    MOSQ_ERR_NOT_FOUND = -6,
    MOSQ_ERR_CONN_LOST = -7,
    MOSQ_ERR_TLS = -8,
    MOSQ_ERR_PAYLOAD_SIZE = -9,
    MOSQ_ERR_NOT_SUPPORTED = -10,
    MOSQ_ERR_AUTH = -11,
    MOSQ_ERR_ACL_DENIED = -12,
    MOSQ_ERR_UNKNOWN = -13,
    MOSQ_ERR_ERRNO = -14,
    MOSQ_ERR_EAI = -15,
    MOSQ_ERR_PROXY = -16,
    MOSQ_ERR_PLUGIN_DEFER = -17,
    MOSQ_ERR_MALFORMED_UTF8 = -18,
    MOSQ_ERR_KEEPALIVE = -19,
    MOSQ_ERR_LOOKUP = -20,
    MOSQ_ERR_MALFORMED_PACKET = -21,
    MOSQ_ERR_DUPLICATE_PROPERTY = -22,
    MOSQ_ERR_TLS_HANDSHAKE = -23,
    MOSQ_ERR_QOS_NOT_SUPPORTED = -24,
    MOSQ_ERR_OVERSIZE_PACKET = -25,
    MOSQ_ERR_OCSP = -26,
    MOSQ_ERR_TIMEOUT = -27,
    MOSQ_ERR_RETAIN_NOT_SUPPORTED = -28,
    MOSQ_ERR_TOPIC_ALIAS_INVALID = -29,
    MOSQ_ERR_ADMINISTRATIVE_ACTION = -30,
    MOSQ_ERR_ALREADY_EXISTS = -31,
    MOSQ_ERR_NOMEM = -32,
    MOSQ_ERR_AUTH_CONTINUE = -44,
    MOSQ_ERR_NO_SUBSCRIBERS = -43,
    MOSQ_ERR_SUB_EXISTS = -42,
    MOSQ_ERR_CONN_PENDING = -41,
} mosq_err_t;

/* Option values */
typedef enum mosq_opt_t {
    MOSQ_OPT_PROTOCOL_VERSION = 1,
    MOSQ_OPT_SSL_CTX = 2,
    MOSQ_OPT_SSL_CTX_WITH_DEFAULTS = 3,
    MOSQ_OPT_RECEIVE_MAXIMUM = 4,
    MOSQ_OPT_SEND_MAXIMUM = 5,
    MOSQ_OPT_TLS_KEYFORM = 6,
    MOSQ_OPT_TLS_ENGINE = 7,
    MOSQ_OPT_TLS_ENGINE_KPASS_SHA1 = 8,
    MOSQ_OPT_TLS_OCSP_REQUIRED = 9,
    MOSQ_OPT_TLS_ALPN = 10,
    MOSQ_OPT_TCP_NODELAY = 11,
    MOSQ_OPT_BIND_ADDRESS = 12,
    MOSQ_OPT_TLS_USE_OS_CERTS = 13,
} mosq_opt_t;


/* MQTT specification restricts client ids to a maximum of 23 characters */
#define MOSQ_MQTT_ID_MAX_LENGTH 23

#define MQTT_PROTOCOL_V31 3
#define MQTT_PROTOCOL_V311 4
#define MQTT_PROTOCOL_V5 5

enum mosquitto_msg_direction {
    mosq_md_in = 0,
    mosq_md_out = 1
};

typedef enum mosquitto_client_state {
    mosq_cs_new = 0,
    mosq_cs_connected = 1,
    mosq_cs_disconnecting = 2,
    mosq_cs_active = 3,
    mosq_cs_connect_pending = 4,
    mosq_cs_connect_srv = 5,
    mosq_cs_disconnect_ws = 6,
    mosq_cs_disconnected = 7,
    mosq_cs_socks5_new = 8,
    mosq_cs_socks5_start = 9,
    mosq_cs_socks5_request = 10,
    mosq_cs_socks5_reply = 11,
    mosq_cs_socks5_auth_ok = 12,
    mosq_cs_socks5_userpass_reply = 13,
    mosq_cs_socks5_send_userpass = 14,
    mosq_cs_expiring = 15,
    mosq_cs_duplicate = 17, /* client that has been taken over by another with the same id */
    mosq_cs_disconnect_with_will = 18,
    mosq_cs_disused = 19, /* client that has been added to the disused list to be freed */
    mosq_cs_authenticating = 20, /* Client has sent CONNECT but is still undergoing extended authentication */
    mosq_cs_reauthenticating = 21, /* Client is undergoing reauthentication and shouldn't do anything else until complete */
} mosquitto_client_state_t;

/***************************************************************************
 *              Structures
 ***************************************************************************/
enum mosquitto_msg_state {
    mosq_ms_invalid = 0,
    mosq_ms_publish_qos0 = 1,
    mosq_ms_publish_qos1 = 2,
    mosq_ms_wait_for_puback = 3,
    mosq_ms_publish_qos2 = 4,
    mosq_ms_wait_for_pubrec = 5,
    mosq_ms_resend_pubrel = 6,
    mosq_ms_wait_for_pubrel = 7,
    mosq_ms_resend_pubcomp = 8,
    mosq_ms_wait_for_pubcomp = 9,
    mosq_ms_send_pubrec = 10,
    mosq_ms_queued = 11
};

/* Struct: mosquitto_message
 *
 * Contains details of a PUBLISH message.
 *
 * int mid - the message/packet ID of the PUBLISH message, assuming this is a
 *           QoS 1 or 2 message. Will be set to 0 for QoS 0 messages.
 *
 * char *topic - the topic the message was delivered on.
 *
 * void *payload - the message payload. This will be payloadlen bytes long, and
 *                 may be NULL if a zero length payload was sent.
 *
 * int payloadlen - the length of the payload, in bytes.
 *
 * int qos - the quality of service of the message, 0, 1, or 2.
 *
 * bool retain - set to true for stale retained messages.
 */

struct mosquitto_msg_store {
    char *topic;
    void *payload;
    int payloadlen; // uint32_t
    int mid;        // uint16_t
    int qos;        // uint8_t
    BOOL retain;

    time_t message_expiry_time;
    char *source_id;
    char *source_username;
    int ref_count;
    uint16_t source_mid;
    json_t *properties;
};

struct mosquitto_client_msg {
    DL_ITEM_FIELDS

    struct mosquitto_msg_store *store;
    uint16_t mid;
    uint8_t qos;
    BOOL retain;
    time_t timestamp;
    enum mosquitto_msg_direction direction;
    enum mosquitto_msg_state state;
    BOOL dup;
    json_t *properties;
};

typedef struct _FRAME_HEAD {
    // Information of the first two bytes header
    mqtt_message_t command;
    uint8_t flags;

    // state of frame
    char busy;              // in half of header
    char header_complete;   // Set True when header is completed

    // must do
    char must_read_remaining_length_2;
    char must_read_remaining_length_3;
    char must_read_remaining_length_4;

    size_t frame_length;
} FRAME_HEAD;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int XXX_sub__messages_queue(
    hgobj gobj,
    json_t *jn_subscribers,
    const char *topic_name,
    uint8_t qos,
    int retain,
    struct mosquitto_msg_store *stored
);
PRIVATE void db_free_client_msg(void *client_msg);
PRIVATE void db_free_msg_store(void *store);
PRIVATE struct mosquitto_msg_store *db_duplicate_msg(
    hgobj gobj,
    struct mosquitto_msg_store *stored
);
PRIVATE int send_disconnect(
    hgobj gobj,
    uint8_t reason_code,
    json_t *properties
);
PRIVATE json_t *hash_password(
    hgobj gobj,
    const char *password,
    const char *algorithm,
    int iterations
);
PRIVATE void start_wait_frame_header(hgobj gobj);
PRIVATE void ws_close(hgobj gobj, int code);

PRIVATE int framehead_prepare_new_frame(FRAME_HEAD *frame);
PRIVATE int framehead_consume(hgobj gobj, FRAME_HEAD *frame, istream_h istream, char *bf, int len);
PRIVATE int frame_completed(hgobj gobj);
PRIVATE int set_client_disconnected(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE char *command_name[] = {
    "???",
    "CMD_CONNECT",
    "CMD_CONNACK",
    "CMD_PUBLISH",
    "CMD_PUBACK",
    "CMD_PUBREC",
    "CMD_PUBREL",
    "CMD_PUBCOMP",
    "CMD_SUBSCRIBE",
    "CMD_SUBACK",
    "CMD_UNSUBSCRIBE",
    "CMD_UNSUBACK",
    "CMD_PINGREQ",
    "CMD_PINGRESP",
    "CMD_DISCONNECT",
    "CMD_AUTH"
};

PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_topics(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_clients(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_users(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_create_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "level=1: search in bottoms, level=2: search in all childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_create_user[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "username",     0,              0,          "User name"),
SDATAPM (DTP_STRING,    "password",     0,              0,          "Password"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name------------flag------------alias---items-----------json_fn---------description---------- */
SDATACM2(DTP_SCHEMA,    "help",         0,              a_help, pm_help,        cmd_help,       "Command's help"),
SDATACM2(DTP_SCHEMA,    "list-topics",  SDF_AUTHZ_X,    0,      0,              cmd_list_topics, "List topics"),
SDATACM2(DTP_SCHEMA,    "list-clients", SDF_AUTHZ_X,    0,      0,              cmd_list_clients, "List clients"),
SDATACM2(DTP_SCHEMA,    "list-users",   SDF_AUTHZ_X,    0,      0,              cmd_list_users, "List users"),
SDATACM2(DTP_SCHEMA,    "create-user",  SDF_AUTHZ_X,    0,      pm_create_user, cmd_create_user, "Create user"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag------------------------default-description---------- */
SDATA (DTP_STRING,      "url",              SDF_PERSIST,                "",         "Url to connect"),
SDATA (DTP_STRING,      "cert_pem",         SDF_PERSIST,                "",         "SSL server certificate, PEM format"),
SDATA (DTP_BOOLEAN,     "in_session",       SDF_VOLATIL|SDF_STATS,      0,      "CONNECT mqtt done"),
SDATA (DTP_BOOLEAN,     "send_disconnect",  SDF_VOLATIL,                0,      "send DISCONNECT"),
SDATA (DTP_JSON,        "client",           SDF_VOLATIL,                0,      "client online"),
SDATA (DTP_INTEGER,     "timeout_handshake",SDF_WR|SDF_PERSIST,    "5000",      "Timeout to handshake"),
SDATA (DTP_INTEGER,     "timeout_close",    SDF_WR|SDF_PERSIST,    "3000",      "Timeout to close"),
SDATA (DTP_INTEGER,     "pingT",            SDF_WR|SDF_PERSIST,   "50000",      "Ping interval. If value <= 0 then No ping"),

SDATA (DTP_POINTER,     "gobj_mqtt_topics", 0,                          0,      "global gobj to save topics"),
SDATA (DTP_POINTER,     "gobj_mqtt_clients",0,                          0,      "global gobj with clients"),
SDATA (DTP_POINTER,     "gobj_mqtt_users",  0,                          0,      "global gobj with users"),

/*
 *  Configuration
 */

SDATA (DTP_INTEGER,     "max_inflight_bytes",SDF_WR|SDF_PERSIST,        0,      "Outgoing QoS 1 and 2 messages will be allowed in flight until this byte limit is reached. This allows control of outgoing message rate based on message size rather than message count. If the limit is set to 100, messages of over 100 bytes are still allowed, but only a single message can be in flight at once. Defaults to 0. (No limit)."),

SDATA (DTP_INTEGER,     "max_inflight_messages",SDF_WR|SDF_PERSIST,    "20",      "The maximum number of outgoing QoS 1 or 2 messages that can be in the process of being transmitted simultaneously. This includes messages currently going through handshakes and messages that are being retried. Defaults to 20. Set to 0 for no maximum. If set to 1, this will guarantee in-order delivery of messages"),

SDATA (DTP_INTEGER,     "max_queued_bytes", SDF_WR|SDF_PERSIST,         0,      "The number of outgoing QoS 1 and 2 messages above those currently in-flight will be queued (per client) by the broker. Once this limit has been reached, subsequent messages will be silently dropped. This is an important option if you are sending messages at a high rate and/or have clients who are slow to respond or may be offline for extended periods of time. Defaults to 0. (No maximum).See also the max_queued_messages option. If both max_queued_messages and max_queued_bytes are specified, packets will be queued until the first limit is reached."),

SDATA (DTP_INTEGER,     "max_queued_messages",SDF_WR|SDF_PERSIST,       "1000",   "The maximum number of QoS 1 or 2 messages to hold in the queue (per client) above those messages that are currently in flight. Defaults to 1000. Set to 0 for no maximum (not recommended). See also the queue_qos0_messages and max_queued_bytes options."),

SDATA (DTP_INTEGER,     "message_size_limit",SDF_WR|SDF_PERSIST,        0,      "This option sets the maximum publish payload size that the broker will allow. Received messages that exceed this size will not be accepted by the broker. This means that the message will not be forwarded on to subscribing clients, but the QoS flow will be completed for QoS 1 or QoS 2 messages. MQTT v5 clients using QoS 1 or QoS 2 will receive a PUBACK or PUBREC with the 'implementation specific error' reason code. The default value is 0, which means that all valid MQTT messages are accepted. MQTT imposes a maximum payload size of 268435455 bytes."),

SDATA (DTP_INTEGER,     "max_keepalive",    SDF_WR|SDF_PERSIST,         "65535",  "For MQTT v5 clients, it is possible to have the server send a 'server keepalive' value that will override the keepalive value set by the client. This is intended to be used as a mechanism to say that the server will disconnect the client earlier than it anticipated, and that the client should use the new keepalive value. The max_keepalive option allows you to specify that clients may only connect with keepalive less than or equal to this value, otherwise they will be sent a server keepalive telling them to use max_keepalive. This only applies to MQTT v5 clients. The maximum value allowable, and default value, is 65535. Set to 0 to allow clients to set keepalive = 0, which means no keepalive checks are made and the client will never be disconnected by the broker if no messages are received. You should be very sure this is the behaviour that you want.For MQTT v3.1.1 and v3.1 clients, there is no mechanism to tell the client what keepalive value they should use. If an MQTT v3.1.1 or v3.1 client specifies a keepalive time greater than max_keepalive they will be sent a CONNACK message with the 'identifier rejected' reason code, and disconnected."),

SDATA (DTP_INTEGER,     "max_packet_size",  SDF_WR|SDF_PERSIST,         0,      "For MQTT v5 clients, it is possible to have the server send a 'maximum packet size' value that will instruct the client it will not accept MQTT packets with size greater than value bytes. This applies to the full MQTT packet, not just the payload. Setting this option to a positive value will set the maximum packet size to that number of bytes. If a client sends a packet which is larger than this value, it will be disconnected. This applies to all clients regardless of the protocol version they are using, but v3.1.1 and earlier clients will of course not have received the maximum packet size information. Defaults to no limit. This option applies to all clients, not just those using MQTT v5, but it is not possible to notify clients using MQTT v3.1.1 or MQTT v3.1 of the limit. Setting below 20 bytes is forbidden because it is likely to interfere with normal client operation even with small payloads."),

SDATA (DTP_BOOLEAN,     "persistence",      SDF_WR|SDF_PERSIST,         "1",   "If TRUE, connection, subscription and message data will be written to the disk"), // TODO

SDATA (DTP_BOOLEAN,     "retain_available", SDF_WR|SDF_PERSIST,         "1",   "If set to FALSE, then retained messages are not supported. Clients that send a message with the retain bit will be disconnected if this option is set to FALSE. Defaults to TRUE."),

SDATA (DTP_INTEGER,     "max_qos",          SDF_WR|SDF_PERSIST,         "2",      "Limit the QoS value allowed for clients connecting to this listener. Defaults to 2, which means any QoS can be used. Set to 0 or 1 to limit to those QoS values. This makes use of an MQTT v5 feature to notify clients of the limitation. MQTT v3.1.1 clients will not be aware of the limitation. Clients publishing to this listener with a too-high QoS will be disconnected."),

SDATA (DTP_BOOLEAN,     "allow_zero_length_clientid",SDF_WR|SDF_PERSIST, "0",   "MQTT 3.1.1 and MQTT 5 allow clients to connect with a zero length client id and have the broker generate a client id for them. Use this option to allow/disallow this behaviour. Defaults to FALSE."),

SDATA (DTP_BOOLEAN,     "use_username_as_clientid",SDF_WR|SDF_PERSIST,  "0",  "Set use_username_as_clientid to TRUE to replace the clientid that a client connected with its username. This allows authentication to be tied to the clientid, which means that it is possible to prevent one client disconnecting another by using the same clientid. Defaults to FALSE."),

SDATA (DTP_BOOLEAN,     "allow_anonymous",  SDF_WR|SDF_PERSIST,         "1",   "Boolean value that determines whether clients that connect without providing a username are allowed to connect. If set to FALSE then another means of connection should be created to control authenticated client access. Defaults to TRUE, (TODO but connections are only allowed from the local machine)."),

SDATA (DTP_INTEGER,     "max_topic_alias",  SDF_WR|SDF_PERSIST,         "10",     "This option sets the maximum number topic aliases that an MQTT v5 client is allowed to create. This option applies per listener. Defaults to 10. Set to 0 to disallow topic aliases. The maximum value possible is 65535."),

/*
 *  Dynamic Data
 */
SDATA (DTP_STRING,      "protocol_name",    SDF_VOLATIL,                0,      "Protocol name"),
SDATA (DTP_INTEGER,     "protocol_version", SDF_VOLATIL,                0,      "Protocol version"),
SDATA (DTP_BOOLEAN,     "is_bridge",        SDF_VOLATIL,                0,      "Connexion is a bridge"),
SDATA (DTP_BOOLEAN,     "will",             SDF_VOLATIL,                0,      "Will"),
SDATA (DTP_JSON,        "will_struct",      SDF_VOLATIL,                0,      "Will struc"),
SDATA (DTP_BOOLEAN,     "will_retain",      SDF_VOLATIL,                0,      "Will retain"),
SDATA (DTP_INTEGER,     "will_qos",         SDF_VOLATIL,                0,      "QoS"),
SDATA (DTP_BOOLEAN,     "assigned_id",      SDF_VOLATIL,                0,      "Auto client id"),
SDATA (DTP_STRING,      "client_id",        SDF_VOLATIL,                0,      "Client id"),
SDATA (DTP_STRING,      "username",         SDF_VOLATIL,                0,      "Username"),
SDATA (DTP_STRING,      "password",         SDF_VOLATIL,                0,      "Password"),
SDATA (DTP_BOOLEAN,     "clean_start",      SDF_VOLATIL,                0,      "New session"),
SDATA (DTP_INTEGER,     "session_expiry_interval",SDF_VOLATIL,          0,      "Session expiry interval in ?"),
SDATA (DTP_INTEGER,     "keepalive",        SDF_VOLATIL,                0,      "Keepalive in ?"),
SDATA (DTP_STRING,      "auth_method",      SDF_VOLATIL,                0,      "Auth method"),
SDATA (DTP_STRING,      "auth_data",        SDF_VOLATIL,                0,      "Auth data (in base64)"),
SDATA (DTP_INTEGER,     "state",            SDF_VOLATIL,                0,      "State"),

SDATA (DTP_INTEGER,     "msgs_out_inflight_maximum", SDF_VOLATIL,       0,      "Connect property"),
SDATA (DTP_INTEGER,     "msgs_out_inflight_quota", SDF_VOLATIL,         0,      "Connect property"),
SDATA (DTP_INTEGER,     "maximum_packet_size", SDF_VOLATIL,             0,      "Connect property"),
SDATA (DTP_INTEGER,     "will_delay_interval", SDF_VOLATIL,             0,      "Will property"),
SDATA (DTP_INTEGER,     "will_expiry_interval",SDF_VOLATIL,             0,      "Will property"),
SDATA (DTP_STRING,      "will_topic",       SDF_VOLATIL,                0,      "Will property"),


SDATA (DTP_POINTER,     "user_data",        0,                          0,      "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                          0,      "more user data"),
SDATA (DTP_BOOLEAN,     "iamServer",        SDF_RD,                     0,      "What side? server or client"),
SDATA (DTP_POINTER,     "subscriber",       0,                          0,      "subscriber of output-events. Default if null is parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_CONNECT_DISCONNECT    = 0x0001,
    TRAFFIC                     = 0x0002,
    SHOW_DECODE                 = 0x0004,
    TRAFFIC_PAYLOAD             = 0x0008,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"connections",     "Trace connections and disconnections"},
{"traffic",         "Trace input/output data (without payload"},
{"show-decode",     "Print decode"},
{"traffic-payload", "Trace payload data"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj gobj_mqtt_topics;
    hgobj gobj_mqtt_clients;
    hgobj gobj_mqtt_users;
    hgobj timer;
    BOOL iamServer;         // What side? server or client
    int pingT;

    FRAME_HEAD frame_head;
    istream_h istream_frame;
    istream_h istream_payload;

    FRAME_HEAD message_head;

    char must_broadcast_on_close;       // event on_open already broadcasted
    json_t *jn_alias_list;
    dl_list_t dl_msgs_out;  // Output queue of messages
    dl_list_t dl_msgs_in;   // Input queue of messages (qos 2, waiting for pubrel)

    /*
     *  Config
     */
    uint32_t max_inflight_bytes;
    uint32_t max_inflight_messages;
    uint32_t max_keepalive;
    uint32_t max_packet_size;
    uint32_t max_queued_bytes;
    uint32_t max_queued_messages;
    uint32_t message_size_limit;
    BOOL persistence;
    BOOL retain_available;
    uint32_t max_qos;
    BOOL allow_zero_length_clientid;
    BOOL use_username_as_clientid;
    BOOL allow_anonymous;
    uint32_t max_topic_alias;

    /*
     *  Dynamic data (reset per connection)
     */
    BOOL in_session;
    BOOL send_disconnect;
    json_t *client;
    const char *protocol_name;
    uint32_t protocol_version;
    BOOL is_bridge;
    BOOL will;
    json_t *will_struct;
    BOOL will_retain;
    uint32_t will_qos;
    BOOL assigned_id;
    const char *client_id;
    const char *username;
    const char *password;
    BOOL clean_start;
    uint32_t session_expiry_interval;
    uint32_t keepalive;
    const char *auth_method;
    const char *auth_data;
    uint32_t state; // TODO enum mosquitto_client_state state;
    uint32_t msgs_out_inflight_maximum;
    uint32_t msgs_out_inflight_quota;
    uint32_t maximum_packet_size;
    uint32_t will_delay_interval;
    uint32_t will_expiry_interval;
    const char *will_topic;
    gbuffer_t *gbuf_will_payload;

} PRIVATE_DATA;





            /***************************
             *      Framework Methods
             ***************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->iamServer = gobj_read_bool_attr(gobj, "iamServer");
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    dl_init(&priv->dl_msgs_out, gobj);
    dl_init(&priv->dl_msgs_in, gobj);

    priv->istream_frame = istream_create(gobj, 14, 14);
    if(!priv->istream_frame) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "istream_create() FAILED",
            NULL
        );
        return;
    }

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(pingT,                     gobj_read_integer_attr)
    SET_PRIV(in_session,                gobj_read_bool_attr)
    SET_PRIV(send_disconnect,           gobj_read_bool_attr)
    SET_PRIV(client,                    gobj_read_json_attr)

    SET_PRIV(gobj_mqtt_topics,          gobj_read_pointer_attr)
    SET_PRIV(gobj_mqtt_clients,         gobj_read_pointer_attr)
    SET_PRIV(gobj_mqtt_users,           gobj_read_pointer_attr)

    SET_PRIV(max_inflight_bytes,        gobj_read_integer_attr)
    SET_PRIV(max_inflight_messages,     gobj_read_integer_attr)
    SET_PRIV(max_keepalive,             gobj_read_integer_attr)
    SET_PRIV(max_packet_size,           gobj_read_integer_attr)
    SET_PRIV(max_queued_bytes,          gobj_read_integer_attr)
    SET_PRIV(max_queued_messages,       gobj_read_integer_attr)
    SET_PRIV(message_size_limit,        gobj_read_integer_attr)
    SET_PRIV(persistence,               gobj_read_bool_attr)
    SET_PRIV(retain_available,          gobj_read_bool_attr)
    SET_PRIV(max_qos,                   gobj_read_integer_attr)
    SET_PRIV(allow_zero_length_clientid,gobj_read_bool_attr)
    SET_PRIV(use_username_as_clientid,  gobj_read_bool_attr)
    SET_PRIV(allow_anonymous,           gobj_read_bool_attr)
    SET_PRIV(max_topic_alias,           gobj_read_integer_attr)

    SET_PRIV(protocol_name,             gobj_read_str_attr)
    SET_PRIV(protocol_version,          gobj_read_integer_attr)
    SET_PRIV(is_bridge,                 gobj_read_bool_attr)
    SET_PRIV(will,                      gobj_read_bool_attr)
    SET_PRIV(will_struct,               gobj_read_json_attr)
    SET_PRIV(will_retain,               gobj_read_bool_attr)
    SET_PRIV(will_qos,                  gobj_read_integer_attr)
    SET_PRIV(assigned_id,               gobj_read_bool_attr)
    SET_PRIV(client_id,                 gobj_read_str_attr)
    SET_PRIV(username,                  gobj_read_str_attr)
    SET_PRIV(password,                  gobj_read_str_attr)
    SET_PRIV(clean_start,               gobj_read_bool_attr)
    SET_PRIV(session_expiry_interval,   gobj_read_integer_attr)
    SET_PRIV(keepalive,                 gobj_read_integer_attr)
    SET_PRIV(auth_method,               gobj_read_str_attr)
    SET_PRIV(auth_data,                 gobj_read_str_attr)
    SET_PRIV(state,                     gobj_read_integer_attr)

    SET_PRIV(msgs_out_inflight_maximum, gobj_read_integer_attr)
    SET_PRIV(msgs_out_inflight_quota,   gobj_read_integer_attr)
    SET_PRIV(maximum_packet_size,       gobj_read_integer_attr)
    SET_PRIV(will_delay_interval,       gobj_read_integer_attr)
    SET_PRIV(will_expiry_interval,      gobj_read_integer_attr)
    SET_PRIV(will_topic,                gobj_read_str_attr)

}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(pingT,                       gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(in_session,                gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(send_disconnect,           gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(client,                    gobj_read_json_attr)

    ELIF_EQ_SET_PRIV(gobj_mqtt_topics,          gobj_read_pointer_attr)
    ELIF_EQ_SET_PRIV(gobj_mqtt_clients,         gobj_read_pointer_attr)
    ELIF_EQ_SET_PRIV(gobj_mqtt_users,           gobj_read_pointer_attr)

    ELIF_EQ_SET_PRIV(max_inflight_bytes,        gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(max_inflight_messages,     gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(max_keepalive,             gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(max_packet_size,           gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(max_queued_bytes,          gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(max_queued_messages,       gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(message_size_limit,        gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(persistence,               gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(retain_available,          gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(max_qos,                   gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(allow_zero_length_clientid,gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(use_username_as_clientid,  gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(allow_anonymous,           gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(max_topic_alias,           gobj_read_integer_attr)

    ELIF_EQ_SET_PRIV(protocol_name,             gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(protocol_version,          gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(is_bridge,                 gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(will,                      gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(will_struct,               gobj_read_json_attr)
    ELIF_EQ_SET_PRIV(will_retain,               gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(will_qos,                  gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(assigned_id,               gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(client_id,                 gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(username,                  gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(password,                  gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(clean_start,               gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(session_expiry_interval,   gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(keepalive,                 gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(auth_method,               gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(auth_data,                 gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(state,                     gobj_read_integer_attr)

    ELIF_EQ_SET_PRIV(msgs_out_inflight_maximum, gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(msgs_out_inflight_quota,   gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(maximum_packet_size,       gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(will_delay_interval,       gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(will_expiry_interval,      gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(will_topic,                gobj_read_str_attr)

    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method start
 *
 *      Start Point for external http server
 *      They must pass the `tcp0` with the connection done
 *      and the http `request`.
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  The bottom must be a C_TCP (it has manual start/stop!).
     *  If it's a client then start to begin the connection.
     *  If it's a server, wait to give the connection done by C_TCP_S.
     */

    if(!priv->iamServer) {
        /*
         *  Client side
         */
        hgobj bottom_gobj = gobj_bottom_gobj(gobj);
        if(!bottom_gobj) {
            json_t *kw = json_pack("{s:s, s:s}",
                "cert_pem", gobj_read_str_attr(gobj, "cert_pem"),
                "url", gobj_read_str_attr(gobj, "url")
            );

#ifdef ESP_PLATFORM
            bottom_gobj = gobj_create_pure_child(gobj_name(gobj), C_ESP_TRANSPORT, kw, gobj);
#endif
#ifdef __linux__
            bottom_gobj = gobj_create_pure_child(gobj_name(gobj), C_TCP, kw, gobj);
#endif
            gobj_set_bottom_gobj(gobj, bottom_gobj);
            //gobj_write_str_attr(bottom_gobj, "tx_ready_event_name", 0);
        }

        gobj_start(bottom_gobj);
    }

    gobj_start(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_client_disconnected(gobj);

    if(priv->timer) {
        clear_timeout(priv->timer);
        gobj_stop(priv->timer);
    }

    hgobj tcp0 = gobj_bottom_gobj(gobj);
    if(tcp0) {
        if(gobj_is_running(tcp0)) {
            gobj_stop(tcp0);
        }
    }

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->istream_frame) {
        istream_destroy(priv->istream_frame);
        priv->istream_frame = 0;
    }
    if(priv->istream_payload) {
        istream_destroy(priv->istream_payload);
        priv->istream_payload = 0;
    }

    priv->client = 0;
    JSON_DECREF(priv->jn_alias_list)
    GBUFFER_DECREF(priv->gbuf_will_payload);

    dl_flush(&priv->dl_msgs_in, db_free_client_msg);
    dl_flush(&priv->dl_msgs_out, db_free_client_msg);
}




            /***************************
             *      Commands
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw);
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_topics(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_resp = gobj_list_resource(priv->gobj_mqtt_topics, "", kw_incref(kw), 0);

    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_clients(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_resp = gobj_list_resource(priv->gobj_mqtt_clients, "", kw_incref(kw), 0);

    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_users(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_resp = gobj_list_resource(priv->gobj_mqtt_users, "", kw_incref(kw), 0);

    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_create_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *username = kw_get_str(gobj, kw, "username", "", 0);
    const char *password = kw_get_str(gobj, kw, "password", "", 0);

    if(empty_string(username)) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("What username?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(password)) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("What password?"),
            0,
            0,
            kw  // owned
        );
    }
    json_t *kw_user = hash_password(
        gobj,
        password,
        "sha512",
        PW_DEFAULT_ITERATIONS
    );

    json_t *jn_resp = gobj_create_resource(priv->gobj_mqtt_users, username, kw_user, 0);

    return msg_iev_build_response(
        gobj,
        jn_resp?0:-1,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void print_queue(const char *name, dl_list_t *dl_list)
{
    printf("====================> Queue: %s\n", name);
    int idx = 0;
    struct mosquitto_client_msg *tail = dl_first(dl_list);
    while(tail) {
        printf("  client %d\n", idx++);
        printf("    mid %d\n", tail->mid);
        printf("    qos %d\n", tail->qos);
        printf("    retain %d\n", tail->retain);
        printf("    timestamp %ld\n", (long)tail->timestamp);
        printf("    direction %d\n", tail->direction);
        printf("    state %d\n", tail->state);
        printf("    dup %d\n", tail->dup);
        //print_json(tail->properties);

        printf("  store\n");
        printf("    topic %s\n", tail->store->topic);
        printf("    mid %d\n", tail->store->mid);
        printf("    qos %d\n", tail->store->qos);
        printf("    retain %d\n", tail->store->retain);
        printf("    message_expiry_time %ld\n", (long)tail->store->message_expiry_time);
        printf("    source_id %s\n", tail->store->source_id);
        printf("    source_username %s\n", tail->store->source_username);
        printf("    source_mid %d\n", tail->store->source_mid);

        //log_debug_dump(0, tail->store->payload, tail->store->payloadlen, "store");
        //print_json(tail->store->properties);
        printf("\n");

        /*
         *  Next
         */
        tail = dl_next(tail);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *get_command_name(int cmd_)
{
    int cmd = cmd_ >> 4;
    int max_cmd = sizeof(command_name)/sizeof(command_name[0]);

    if(cmd >= 0 && cmd < max_cmd) {
        return command_name[cmd];
    }
    return "???";
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *mosquitto_reason_string(int reason_code)
{
    switch(reason_code) {
        case MQTT_RC_SUCCESS:
            return "Success";
        case MQTT_RC_GRANTED_QOS1:
            return "Granted QoS 1";
        case MQTT_RC_GRANTED_QOS2:
            return "Granted QoS 2";
        case MQTT_RC_DISCONNECT_WITH_WILL_MSG:
            return "Disconnect with Will Message";
        case MQTT_RC_NO_MATCHING_SUBSCRIBERS:
            return "No matching subscribers";
        case MQTT_RC_NO_SUBSCRIPTION_EXISTED:
            return "No subscription existed";
        case MQTT_RC_CONTINUE_AUTHENTICATION:
            return "Continue authentication";
        case MQTT_RC_REAUTHENTICATE:
            return "Re-authenticate";

        case MQTT_RC_UNSPECIFIED:
            return "Unspecified error";
        case MQTT_RC_MALFORMED_PACKET:
            return "Malformed Packet";
        case MQTT_RC_PROTOCOL_ERROR:
            return "Protocol Error";
        case MQTT_RC_IMPLEMENTATION_SPECIFIC:
            return "Implementation specific error";
        case MQTT_RC_UNSUPPORTED_PROTOCOL_VERSION:
            return "Unsupported Protocol Version";
        case MQTT_RC_CLIENTID_NOT_VALID:
            return "Client Identifier not valid";
        case MQTT_RC_BAD_USERNAME_OR_PASSWORD:
            return "Bad User Name or Password";
        case MQTT_RC_NOT_AUTHORIZED:
            return "Not authorized";
        case MQTT_RC_SERVER_UNAVAILABLE:
            return "Server unavailable";
        case MQTT_RC_SERVER_BUSY:
            return "Server busy";
        case MQTT_RC_BANNED:
            return "Banned";
        case MQTT_RC_SERVER_SHUTTING_DOWN:
            return "Server shutting down";
        case MQTT_RC_BAD_AUTHENTICATION_METHOD:
            return "Bad authentication method";
        case MQTT_RC_KEEP_ALIVE_TIMEOUT:
            return "Keep Alive timeout";
        case MQTT_RC_SESSION_TAKEN_OVER:
            return "Session taken over";
        case MQTT_RC_TOPIC_FILTER_INVALID:
            return "Topic Filter invalid";
        case MQTT_RC_TOPIC_NAME_INVALID:
            return "Topic Name invalid";
        case MQTT_RC_PACKET_ID_IN_USE:
            return "Packet Identifier in use";
        case MQTT_RC_PACKET_ID_NOT_FOUND:
            return "Packet Identifier not found";
        case MQTT_RC_RECEIVE_MAXIMUM_EXCEEDED:
            return "Receive Maximum exceeded";
        case MQTT_RC_TOPIC_ALIAS_INVALID:
            return "Topic Alias invalid";
        case MQTT_RC_PACKET_TOO_LARGE:
            return "Packet too large";
        case MQTT_RC_MESSAGE_RATE_TOO_HIGH:
            return "Message rate too high";
        case MQTT_RC_QUOTA_EXCEEDED:
            return "Quota exceeded";
        case MQTT_RC_ADMINISTRATIVE_ACTION:
            return "Administrative action";
        case MQTT_RC_PAYLOAD_FORMAT_INVALID:
            return "Payload format invalid";
        case MQTT_RC_RETAIN_NOT_SUPPORTED:
            return "Retain not supported";
        case MQTT_RC_QOS_NOT_SUPPORTED:
            return "QoS not supported";
        case MQTT_RC_USE_ANOTHER_SERVER:
            return "Use another server";
        case MQTT_RC_SERVER_MOVED:
            return "Server moved";
        case MQTT_RC_SHARED_SUBS_NOT_SUPPORTED:
            return "Shared Subscriptions not supported";
        case MQTT_RC_CONNECTION_RATE_EXCEEDED:
            return "Connection rate exceeded";
        case MQTT_RC_MAXIMUM_CONNECT_TIME:
            return "Maximum connect time";
        case MQTT_RC_SUBSCRIPTION_IDS_NOT_SUPPORTED:
            return "Subscription identifiers not supported";
        case MQTT_RC_WILDCARD_SUBS_NOT_SUPPORTED:
            return "Wildcard Subscriptions not supported";
        default:
            return "Unknown reason";
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *mqtt_property_identifier_to_string(int identifier)
{
    hgobj gobj = 0;

    switch(identifier) {
        case MQTT_PROP_PAYLOAD_FORMAT_INDICATOR:
            return "payload-format-indicator";
        case MQTT_PROP_MESSAGE_EXPIRY_INTERVAL:
            return "message-expiry-interval";
        case MQTT_PROP_CONTENT_TYPE:
            return "content-type";
        case MQTT_PROP_RESPONSE_TOPIC:
            return "response-topic";
        case MQTT_PROP_CORRELATION_DATA:
            return "correlation-data";
        case MQTT_PROP_SUBSCRIPTION_IDENTIFIER:
            return "subscription-identifier";
        case MQTT_PROP_SESSION_EXPIRY_INTERVAL:
            return "session-expiry-interval";
        case MQTT_PROP_ASSIGNED_CLIENT_IDENTIFIER:
            return "assigned-client-identifier";
        case MQTT_PROP_SERVER_KEEP_ALIVE:
            return "server-keep-alive";
        case MQTT_PROP_AUTHENTICATION_METHOD:
            return "authentication-method";
        case MQTT_PROP_AUTHENTICATION_DATA:
            return "authentication-data";
        case MQTT_PROP_REQUEST_PROBLEM_INFORMATION:
            return "request-problem-information";
        case MQTT_PROP_WILL_DELAY_INTERVAL:
            return "will-delay-interval";
        case MQTT_PROP_REQUEST_RESPONSE_INFORMATION:
            return "request-response-information";
        case MQTT_PROP_RESPONSE_INFORMATION:
            return "response-information";
        case MQTT_PROP_SERVER_REFERENCE:
            return "server-reference";
        case MQTT_PROP_REASON_STRING:
            return "reason-string";
        case MQTT_PROP_RECEIVE_MAXIMUM:
            return "receive-maximum";
        case MQTT_PROP_TOPIC_ALIAS_MAXIMUM:
            return "topic-alias-maximum";
        case MQTT_PROP_TOPIC_ALIAS:
            return "topic-alias";
        case MQTT_PROP_MAXIMUM_QOS:
            return "maximum-qos";
        case MQTT_PROP_RETAIN_AVAILABLE:
            return "retain-available";
        case MQTT_PROP_USER_PROPERTY:
            return "user-property";
        case MQTT_PROP_MAXIMUM_PACKET_SIZE:
            return "maximum-packet-size";
        case MQTT_PROP_WILDCARD_SUB_AVAILABLE:
            return "wildcard-subscription-available";
        case MQTT_PROP_SUBSCRIPTION_ID_AVAILABLE:
            return "subscription-identifier-available";
        case MQTT_PROP_SHARED_SUB_AVAILABLE:
            return "shared-subscription-available";
        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt unknown property",
                "identifier",   "%d", (int)identifier,
                NULL
            );
            return 0;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int mosquitto_string_to_property_info(const char *propname, int *identifier, int *type)
{
    hgobj gobj = 0;

    if(empty_string(propname)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt empty property",
            NULL
        );
        *type = 0;
        *identifier = 0;
        return -1;
    }

    if(!strcasecmp(propname, "payload-format-indicator")) {
        *identifier = MQTT_PROP_PAYLOAD_FORMAT_INDICATOR;
        *type = MQTT_PROP_TYPE_BYTE;
    } else if(!strcasecmp(propname, "message-expiry-interval")) {
        *identifier = MQTT_PROP_MESSAGE_EXPIRY_INTERVAL;
        *type = MQTT_PROP_TYPE_INT32;
    } else if(!strcasecmp(propname, "content-type")) {
        *identifier = MQTT_PROP_CONTENT_TYPE;
        *type = MQTT_PROP_TYPE_STRING;
    } else if(!strcasecmp(propname, "response-topic")) {
        *identifier = MQTT_PROP_RESPONSE_TOPIC;
        *type = MQTT_PROP_TYPE_STRING;
    } else if(!strcasecmp(propname, "correlation-data")) {
        *identifier = MQTT_PROP_CORRELATION_DATA;
        *type = MQTT_PROP_TYPE_BINARY;
    } else if(!strcasecmp(propname, "subscription-identifier")) {
        *identifier = MQTT_PROP_SUBSCRIPTION_IDENTIFIER;
        *type = MQTT_PROP_TYPE_VARINT;
    } else if(!strcasecmp(propname, "session-expiry-interval")) {
        *identifier = MQTT_PROP_SESSION_EXPIRY_INTERVAL;
        *type = MQTT_PROP_TYPE_INT32;
    } else if(!strcasecmp(propname, "assigned-client-identifier")) {
        *identifier = MQTT_PROP_ASSIGNED_CLIENT_IDENTIFIER;
        *type = MQTT_PROP_TYPE_STRING;
    } else if(!strcasecmp(propname, "server-keep-alive")) {
        *identifier = MQTT_PROP_SERVER_KEEP_ALIVE;
        *type = MQTT_PROP_TYPE_INT16;
    } else if(!strcasecmp(propname, "authentication-method")) {
        *identifier = MQTT_PROP_AUTHENTICATION_METHOD;
        *type = MQTT_PROP_TYPE_STRING;
    } else if(!strcasecmp(propname, "authentication-data")) {
        *identifier = MQTT_PROP_AUTHENTICATION_DATA;
        *type = MQTT_PROP_TYPE_BINARY;
    } else if(!strcasecmp(propname, "request-problem-information")) {
        *identifier = MQTT_PROP_REQUEST_PROBLEM_INFORMATION;
        *type = MQTT_PROP_TYPE_BYTE;
    } else if(!strcasecmp(propname, "will-delay-interval")) {
        *identifier = MQTT_PROP_WILL_DELAY_INTERVAL;
        *type = MQTT_PROP_TYPE_INT32;
    } else if(!strcasecmp(propname, "request-response-information")) {
        *identifier = MQTT_PROP_REQUEST_RESPONSE_INFORMATION;
        *type = MQTT_PROP_TYPE_BYTE;
    } else if(!strcasecmp(propname, "response-information")) {
        *identifier = MQTT_PROP_RESPONSE_INFORMATION;
        *type = MQTT_PROP_TYPE_STRING;
    } else if(!strcasecmp(propname, "server-reference")) {
        *identifier = MQTT_PROP_SERVER_REFERENCE;
        *type = MQTT_PROP_TYPE_STRING;
    } else if(!strcasecmp(propname, "reason-string")) {
        *identifier = MQTT_PROP_REASON_STRING;
        *type = MQTT_PROP_TYPE_STRING;
    } else if(!strcasecmp(propname, "receive-maximum")) {
        *identifier = MQTT_PROP_RECEIVE_MAXIMUM;
        *type = MQTT_PROP_TYPE_INT16;
    } else if(!strcasecmp(propname, "topic-alias-maximum")) {
        *identifier = MQTT_PROP_TOPIC_ALIAS_MAXIMUM;
        *type = MQTT_PROP_TYPE_INT16;
    } else if(!strcasecmp(propname, "topic-alias")) {
        *identifier = MQTT_PROP_TOPIC_ALIAS;
        *type = MQTT_PROP_TYPE_INT16;
    } else if(!strcasecmp(propname, "maximum-qos")) {
        *identifier = MQTT_PROP_MAXIMUM_QOS;
        *type = MQTT_PROP_TYPE_BYTE;
    } else if(!strcasecmp(propname, "retain-available")) {
        *identifier = MQTT_PROP_RETAIN_AVAILABLE;
        *type = MQTT_PROP_TYPE_BYTE;
    } else if(!strcasecmp(propname, "user-property")) {
        *identifier = MQTT_PROP_USER_PROPERTY;
        *type = MQTT_PROP_TYPE_STRING_PAIR;
    } else if(!strcasecmp(propname, "maximum-packet-size")) {
        *identifier = MQTT_PROP_MAXIMUM_PACKET_SIZE;
        *type = MQTT_PROP_TYPE_INT32;
    } else if(!strcasecmp(propname, "wildcard-subscription-available")) {
        *identifier = MQTT_PROP_WILDCARD_SUB_AVAILABLE;
        *type = MQTT_PROP_TYPE_BYTE;
    } else if(!strcasecmp(propname, "subscription-identifier-available")) {
        *identifier = MQTT_PROP_SUBSCRIPTION_ID_AVAILABLE;
        *type = MQTT_PROP_TYPE_BYTE;
    } else if(!strcasecmp(propname, "shared-subscription-available")) {
        *identifier = MQTT_PROP_SHARED_SUB_AVAILABLE;
        *type = MQTT_PROP_TYPE_BYTE;
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt unknown property",
            "property",     "%s", propname,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *protocol_version_name(mosquitto_protocol_t mosquitto_protocol)
{
    switch(mosquitto_protocol) {
        case mosq_p_mqtt31:
            return "mqtt31";
        case mosq_p_mqtt311:
            return "mqtt311";
        case mosq_p_mqtts:
            return "mqtts";
        case mosq_p_mqtt5:
            return "mqtt5";
        case mosq_p_invalid:
        default:
            return "invalid protocol version";
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void do_disconnect(hgobj gobj, int reason)
{
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void ws_close(hgobj gobj, int reason)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->in_session) {
        if(priv->send_disconnect) {
            // Fallan los test con el send_disconnect
            //send_disconnect(gobj, code, NULL);
        }
    }

    do_disconnect(gobj, reason);

    if(priv->iamServer) {
        hgobj tcp0 = gobj_bottom_gobj(gobj);
        if(gobj_is_running(tcp0)) {
            gobj_stop(tcp0);
        }
    }
    set_timeout(priv->timer, gobj_read_integer_attr(gobj, "timeout_close"));
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int mosquitto_validate_utf8(const char *str, int len)
{
    int i;
    int j;
    int codelen;
    int codepoint;
    const uint8_t *ustr = (const unsigned char *)str;

    if(!str) {
        return -1;
    }
    if(len < 0 || len > 65536) {
        return -1;
    }

    for(i=0; i<len; i++) {
        if(ustr[i] == 0) {
            return -1;
        } else if(ustr[i] <= 0x7f) {
            codelen = 1;
            codepoint = ustr[i];
        } else if((ustr[i] & 0xE0) == 0xC0) {
            /* 110xxxxx - 2 byte sequence */
            if(ustr[i] == 0xC0 || ustr[i] == 0xC1) {
                /* Invalid bytes */
                return -1;
            }
            codelen = 2;
            codepoint = (ustr[i] & 0x1F);
        } else if((ustr[i] & 0xF0) == 0xE0) {
            /* 1110xxxx - 3 byte sequence */
            codelen = 3;
            codepoint = (ustr[i] & 0x0F);
        } else if((ustr[i] & 0xF8) == 0xF0) {
            /* 11110xxx - 4 byte sequence */
            if(ustr[i] > 0xF4) {
                /* Invalid, this would produce values > 0x10FFFF. */
                return -1;
            }
            codelen = 4;
            codepoint = (ustr[i] & 0x07);
        } else {
            /* Unexpected continuation byte. */
            return -1;
        }

        /* Reconstruct full code point */
        if(i == len-codelen+1) {
            /* Not enough data */
            return -1;
        }
        for(j=0; j<codelen-1; j++) {
            if((ustr[++i] & 0xC0) != 0x80) {
                /* Not a continuation byte */
                return -1;
            }
            codepoint = (codepoint<<6) | (ustr[i] & 0x3F);
        }

        /* Check for UTF-16 high/low surrogates */
        if(codepoint >= 0xD800 && codepoint <= 0xDFFF) {
            return -1;
        }

        /* Check for overlong or out of range encodings */
        /* Checking codelen == 2 isn't necessary here, because it is already
         * covered above in the C0 and C1 checks.
         * if(codelen == 2 && codepoint < 0x0080) {
         *     return MOSQ_ERR_MALFORMED_UTF8;
         * } else
        */
        if(codelen == 3 && codepoint < 0x0800) {
            return -1;
        } else if(codelen == 4 && (codepoint < 0x10000 || codepoint > 0x10FFFF)) {
            return -1;
        }

        /* Check for non-characters */
        if(codepoint >= 0xFDD0 && codepoint <= 0xFDEF) {
            return -1;
        }
        if((codepoint & 0xFFFF) == 0xFFFE || (codepoint & 0xFFFF) == 0xFFFF) {
            return -1;
        }
        /* Check for control characters */
        if(codepoint <= 0x001F || (codepoint >= 0x007F && codepoint <= 0x009F)) {
            return -1;
        }
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int check_passwd(
    hgobj gobj,
    const char *password,
    const char *hash,
    const char *salt,
    const char *algorithm, // hashtype
    json_int_t iterations
)
{
#if defined(CONFIG_HAVE_OPENSSL)
    const EVP_MD *digest;
    unsigned char hash_[EVP_MAX_MD_SIZE+1];
    unsigned int hash_len_ = EVP_MAX_MD_SIZE;

    if(empty_string(algorithm)) {
        algorithm = "sha512";
    }
    digest = EVP_get_digestbyname(algorithm);
    if(!digest) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Unable to get openssl digest",
            "digest",       "%s", algorithm,
            NULL
        );
        return -1;
    }

    if(0) { //strcasecmp(algorithm, "sha512")==0) {
        EVP_MD_CTX *context = EVP_MD_CTX_new();
        EVP_DigestInit_ex(context, digest, NULL);
        EVP_DigestUpdate(context, password, strlen(password));
        EVP_DigestUpdate(context, salt, (size_t)strlen(salt));
        EVP_DigestFinal_ex(context, hash_, &hash_len_);
        EVP_MD_CTX_free(context);
    } else {
        PKCS5_PBKDF2_HMAC(password, (int)strlen(password),
            (const unsigned char *)salt, (int)strlen(salt), iterations,
            digest, (int)hash_len_, hash_
        );
    }

    if(hash_len_ == strlen(hash) && memcmp(hash, hash_, hash_len_)==0) {
        return 0;
    }
#elif defined(CONFIG_HAVE_MBEDTLS)
    unsigned char derived_hash[64]; // SHA512 max size
    size_t hash_len;
    const mbedtls_md_info_t *md_info = NULL;

    if(empty_string(algorithm)) {
        algorithm = "sha512";
    }

    md_info = mbedtls_md_info_from_string(algorithm);
    if(!md_info) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "Unable to get mbedtls digest",
            "digest",   "%s", algorithm,
            NULL
        );
        return -1;
    }

    hash_len = mbedtls_md_get_size(md_info);

    /* Decode salt from base64 */
    gbuffer_t *gbuf_salt = gbuffer_base64_to_string(salt, strlen(salt));
    if(!gbuf_salt) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "Invalid base64 salt",
            NULL
        );
        return -1;
    }

    if(mbedtls_pkcs5_pbkdf2_hmac(
            md_info,
            (const unsigned char *)password, strlen(password),
            (const unsigned char *)gbuffer_cur_rd_pointer(gbuf_salt), gbuffer_leftbytes(gbuf_salt),
            iterations,
            hash_len,
            derived_hash
        ) != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "mbedtls_pbkdf2_hmac() FAILED",
            NULL
        );
        GBUFFER_DECREF(gbuf_salt);
        return -1;
    }

    /* Decode input hash from base64 */
    gbuffer_t *gbuf_hash = gbuffer_base64_to_string(hash, strlen(hash));
    if(!gbuf_hash) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "Invalid base64 hash",
            NULL
        );
        GBUFFER_DECREF(gbuf_salt);
        return -1;
    }

    const unsigned char *stored_hash = (const unsigned char *)gbuffer_cur_rd_pointer(gbuf_hash);
    size_t stored_hash_len = gbuffer_leftbytes(gbuf_hash);

    int result = -1;
    if(stored_hash_len == hash_len && memcmp(derived_hash, stored_hash, hash_len) == 0) {
        result = 0; // Match
    }

    GBUFFER_DECREF(gbuf_salt);
    GBUFFER_DECREF(gbuf_hash);
    return result;
#else
    #error "No crypto library defined"
#endif
    return -1;
}

/***************************************************************************
    "credentials" : [
        {
            "type": "password",
            "createdDate": 1581316153674,
            "secretData": {
                "value": "???",
                "salt": "???=="
            },
            "credentialData" : {
                "hashIterations": 27500,
                "algorithm": "sha512",
                "additionalParameters": {
                }
            }
        }
    ]
 ***************************************************************************/
PRIVATE json_t *hash_password(
    hgobj gobj,
    const char *password,
    const char *algorithm,
    int iterations
) {
#if defined(CONFIG_HAVE_OPENSSL)
    #define SALT_LEN 12
    unsigned int hash_len;
    unsigned char hash[64]; /* For SHA512 */
    unsigned char salt[SALT_LEN];

    if(empty_string(algorithm)) {
        algorithm = "sha512";
    }
    if(iterations < 1) {
        iterations = PW_DEFAULT_ITERATIONS;
    }
    if(RAND_bytes(salt, sizeof(salt))<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "RAND_bytes() FAILED",
            "digest",       "%s", algorithm,
            NULL
        );
        return 0;
    }

    const EVP_MD *digest = EVP_get_digestbyname(algorithm);
    if(!digest) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Unable to get openssl digest",
            "digest",       "%s", algorithm,
            NULL
        );
        return 0;
    }

    hash_len = sizeof(hash);
    PKCS5_PBKDF2_HMAC(password, (int)strlen(password),
        salt, sizeof(salt), iterations,
        digest, (int)hash_len, hash
    );

    gbuffer_t *gbuf_hash = gbuffer_string_to_base64((const char *)hash, hash_len);
    gbuffer_t *gbuf_salt = gbuffer_string_to_base64((const char *)salt, sizeof(salt));
    char *hash_b64 = gbuffer_cur_rd_pointer(gbuf_hash);
    char *salt_b64 = gbuffer_cur_rd_pointer(gbuf_salt);

    json_t *credentials = json_object();
    json_t *credential_list = kw_get_list(gobj, credentials, "credentials", json_array(), KW_CREATE);
    json_t *credential = json_pack("{s:s, s:I, s:{s:s, s:s}, s:{s:I, s:s, s:{}}}",
        "type", "password",
        "createdDate", (json_int_t)time_in_milliseconds(),
        "secretData",
            "value", hash_b64,
            "salt", salt_b64,
        "credentialData",
            "hashIterations", iterations,
            "algorithm", algorithm,
            "additionalParameters"
    );
    json_array_append_new(credential_list, credential);

    GBUFFER_DECREF(gbuf_hash);
    GBUFFER_DECREF(gbuf_salt);

    return credentials;
#elif defined(CONFIG_HAVE_MBEDTLS)
    #define SALT_LEN 12
    unsigned char hash[64];  // Support up to SHA512
    unsigned char salt[SALT_LEN];
    const mbedtls_md_info_t *md_info = NULL;

    if(empty_string(algorithm)) {
        algorithm = "sha512";
    }
    if(iterations < 1) {
        iterations = PW_DEFAULT_ITERATIONS;
    }

    /* Resolve hash algorithm */
    md_info = mbedtls_md_info_from_string(algorithm);
    if(!md_info) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "Unable to get mbedtls digest",
            "digest",   "%s", algorithm,
            NULL
        );
        return 0;
    }
    size_t hash_len = mbedtls_md_get_size(md_info);

    /* Initialize RNG */
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char *pers = "hash_password";

    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    if(mbedtls_ctr_drbg_seed(
            &ctr_drbg,
            mbedtls_entropy_func,
            &entropy,
            (const unsigned char *)pers,
            strlen(pers)
        ) != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "mbedtls_ctr_drbg_seed() FAILED",
            NULL
        );
        goto error;
    }

    if(mbedtls_ctr_drbg_random(&ctr_drbg, salt, SALT_LEN) != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "mbedtls_ctr_drbg_random() FAILED",
            NULL
        );
        goto error;
    }

    if(mbedtls_pkcs5_pbkdf2_hmac(
            md_info,
            (const unsigned char *)password, strlen(password),
            salt, SALT_LEN,
            iterations,
            hash_len,
            hash
        ) != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "mbedtls_pkcs5_pbkdf2_hmac() FAILED",
            NULL
        );
        goto error;
    }

    gbuffer_t *gbuf_hash = gbuffer_string_to_base64((const char *)hash, hash_len);
    gbuffer_t *gbuf_salt = gbuffer_string_to_base64((const char *)salt, SALT_LEN);
    char *hash_b64 = gbuffer_cur_rd_pointer(gbuf_hash);
    char *salt_b64 = gbuffer_cur_rd_pointer(gbuf_salt);

    json_t *credential_list = json_array();
    json_t *credentials = json_pack("{s:o}", "credentials", credential_list);
    json_t *credential = json_pack("{s:s, s:I, s:{s:s, s:s}, s:{s:I, s:s, s:{}}}",
        "type", "password",
        "createdDate", (json_int_t)time_in_milliseconds(),
        "secretData",
            "value", hash_b64,
            "salt", salt_b64,
        "credentialData",
            "hashIterations", iterations,
            "algorithm", algorithm,
            "additionalParameters"
    );
    json_array_append_new(credential_list, credential);

    GBUFFER_DECREF(gbuf_hash);
    GBUFFER_DECREF(gbuf_salt);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return credentials;

error:
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return 0;
#else
    #error "No crypto library defined"
#endif
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int mqtt_check_password(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->allow_anonymous) {
        return 0;
    }
    if(empty_string(priv->username)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH_ERROR,
            "msg",          "%s", "No username given to check password",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        return -1;
    }
    json_t *user = gobj_get_resource(priv->gobj_mqtt_users, priv->username, 0, 0);
    if(!user) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH_ERROR,
            "msg",          "%s", "Username not exist",
            "client_id",    "%s", priv->client_id,
            "username",     "%s", priv->username,
            NULL
        );
        return -1;
    }

    json_t *credentials = kw_get_list(gobj, user, "credentials", 0, KW_REQUIRED);

    int idx; json_t *credential;
    json_array_foreach(credentials, idx, credential) {
        const char *password_saved = kw_get_str(gobj, credential, "secretData`value", "", KW_REQUIRED);
        const char *salt = kw_get_str(gobj, credential, "secretData`salt", "", KW_REQUIRED);
        json_int_t hashIterations = kw_get_int(gobj,
            credential, "credentialData`hashIterations", 0, KW_REQUIRED
        );
        const char *algorithm = kw_get_str(gobj, credential, "credentialData`algorithm", "", KW_REQUIRED);

        if(check_passwd(
            gobj,
            priv->password,
            password_saved,
            salt,
            algorithm,
            hashIterations
        )==0) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Username authorized",
                "client_id",    "%s", priv->client_id,
                "username",     "%s", priv->username,
                NULL
            );
            return 0;
        }
    }

    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_AUTH_ERROR,
        "msg",          "%s", "Username not authorized",
        "client_id",    "%s", priv->client_id,
        "username",     "%s", priv->username,
        NULL
    );
    return -1;
}

/***************************************************************************
 *  Start to wait frame header
 ***************************************************************************/
PRIVATE void start_wait_frame_header(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_is_running(gobj)) {
        return;
    }
    gobj_change_state(gobj, ST_WAITING_FRAME_HEADER);
    if(priv->pingT>0) {
        set_timeout(priv->timer, priv->pingT);
    }
    istream_reset_wr(priv->istream_frame);  // Reset buffer for next frame
    memset(&priv->frame_head, 0, sizeof(priv->frame_head));
}

/***************************************************************************
 *  Reset variables for a new read.
 ***************************************************************************/
PRIVATE int framehead_prepare_new_frame(FRAME_HEAD *frame)
{
    /*
     *  state of frame
     */
    memset(frame, 0, sizeof(*frame));
    frame->busy = 1;    //in half of header

    return 0;
}

/***************************************************************************
 *  Decode the two bytes head.
 ***************************************************************************/
PRIVATE int decode_head(hgobj gobj, FRAME_HEAD *frame, char *data)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    unsigned char byte1, byte2;

    byte1 = *(data+0);
    byte2 = *(data+1);

    /*
     *  decod byte1
     */
    frame->command = byte1 & 0xF0;
    frame->flags = byte1 & 0x0F;

    if(!priv->in_session) {
        if(frame->command != CMD_CONNECT) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "First command MUST be CONNECT",
                "command",      "%s", get_command_name(frame->command),
                NULL
            );
            return -1;
        }
    }

    /*
     *  decod byte2
     */
    frame->frame_length = byte2 & 0x7F;
    if(byte2 & 0x80) {
        frame->must_read_remaining_length_2 = 1;
    }

    /*
     *  analize
     */

    return 0;
}

/***************************************************************************
 *  Consume input data to get and analyze the frame header.
 *  Return the consumed size.
 ***************************************************************************/
PRIVATE int framehead_consume(hgobj gobj, FRAME_HEAD *frame, istream_h istream, char *bf, int len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int total_consumed = 0;
    int consumed;
    char *data;

    /*
     *
     */
    if (!frame->busy) {
        /*
         * waiting the first two byte's head
         */
        istream_read_until_num_bytes(istream, 2, 0); // idempotent
        consumed = istream_consume(istream, bf, len);
        total_consumed += consumed;
        bf += consumed;
        len -= consumed;
        if(!istream_is_completed(istream)) {
            return total_consumed;  // wait more data
        }

        /*
         *  we've got enough data! Start a new frame
         */
        framehead_prepare_new_frame(frame);  // `busy` flag is set.
        data = istream_extract_matched_data(istream, 0);
        if(decode_head(gobj, frame, data)<0) {
            // Error already logged
            return -1;
        }
    }

    /*
     *  processing remaining_length
     */
    if(frame->must_read_remaining_length_2) {
        istream_read_until_num_bytes(istream, 1, 0);  // idempotent
        consumed = istream_consume(istream, bf, len);
        total_consumed += consumed;
        bf += consumed;
        len -= consumed;
        if(!istream_is_completed(istream)) {
            return total_consumed;  // wait more data
        }

        /*
         *  Read 1 bytes of remaining_length
         */
        data = istream_extract_matched_data(istream, 0);
        unsigned char byte = *data;
        frame->frame_length += (byte & 0x7F) * (1*128);
        if(byte & 0x80) {
            frame->must_read_remaining_length_3 = 1;
        }
    }
    if(frame->must_read_remaining_length_3) {
        istream_read_until_num_bytes(istream, 1, 0);  // idempotent
        consumed = istream_consume(istream, bf, len);
        total_consumed += consumed;
        bf += consumed;
        len -= consumed;
        if(!istream_is_completed(istream)) {
            return total_consumed;  // wait more data
        }

        /*
         *  Read 1 bytes of remaining_length
         */
        data = istream_extract_matched_data(istream, 0);
        unsigned char byte = *data;
        frame->frame_length += (byte & 0x7F) * (128*128);
        if(byte & 0x80) {
            frame->must_read_remaining_length_4 = 1;
        }
    }
    if(frame->must_read_remaining_length_4) {
        istream_read_until_num_bytes(istream, 1, 0);  // idempotent
        consumed = istream_consume(istream, bf, len);
        total_consumed += consumed;
        bf += consumed;
        len -= consumed;
        if(!istream_is_completed(istream)) {
            return total_consumed;  // wait more data
        }

        /*
         *  Read 1 bytes of remaining_length
         */
        data = istream_extract_matched_data(istream, 0);
        unsigned char byte = *data;
        frame->frame_length += (byte & 0x7F) * (128*128*128);
        if(byte & 0x80) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Fourth remaining_length byte MUST be without 0x80",
                NULL
            );
            return MOSQ_ERR_PROTOCOL;
        }
    }

    frame->header_complete = TRUE;

    if(priv->iamServer) {
        switch(frame->command) {
            case CMD_CONNECT:
                if(frame->frame_length > 100000) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MQTT_ERROR,
                        "msg",          "%s", "CONNECT command too large",
                        "frame_length", "%d", (int)frame->frame_length,
                        NULL
                    );
                    return -1;
                }
                break;
            case CMD_DISCONNECT:
                break;

            case CMD_CONNACK:
            case CMD_PUBLISH:
            case CMD_PUBACK:
            case CMD_PUBREC:
            case CMD_PUBREL:
            case CMD_PUBCOMP:
            case CMD_SUBSCRIBE:
            case CMD_SUBACK:
            case CMD_UNSUBSCRIBE:
            case CMD_UNSUBACK:
            case CMD_AUTH:
                break;

            case CMD_PINGREQ:
            case CMD_PINGRESP:
                if(frame->frame_length != 0) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MQTT_ERROR,
                        "msg",          "%s", "PING command must be 0 large",
                        "frame_length", "%d", (int)frame->frame_length,
                        NULL
                    );
                    return -1;
                }
                break;

            default:
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt command unknown",
                    "command",      "%d", (int)frame->command,
                    NULL
                );
                if(priv->in_session) {
                    send_disconnect(gobj, MQTT_RC_PROTOCOL_ERROR, NULL);
                }
                return -1;
        }
    }

    return total_consumed;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE gbuffer_t *build_mqtt_packet(hgobj gobj, uint8_t command, uint32_t size)
{
    uint32_t remaining_length = size;
    uint8_t remaining_bytes[5], byte;

    int remaining_count = 0;
    do {
        byte = remaining_length % 128;
        remaining_length = remaining_length / 128;
        /* If there are more digits to encode, set the top bit of this digit */
        if(remaining_length > 0) {
            byte = byte | 0x80;
        }
        remaining_bytes[remaining_count] = byte;
        remaining_count++;
    } while(remaining_length > 0 && remaining_count < 5);

    if(remaining_count == 5) {
        // return MOSQ_ERR_PAYLOAD_SIZE;
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt packet TOO LARGE",
            "size",         "%d", (int)size,
            NULL
        );
        return 0;
    }

    uint32_t packet_length = size + 1 + (uint8_t)remaining_count;

    gbuffer_t *gbuf = gbuffer_create(packet_length, packet_length);
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "Mqtt Not enough memory",
            "size",         "%d", (int)packet_length,
            NULL
        );
        return 0;
    }
    gbuffer_append_char(gbuf, command);

    for(int i=0; i<remaining_count; i++) {
        gbuffer_append_char(gbuf, remaining_bytes[i]);
    }
    return gbuf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE unsigned int packet_varint_bytes(uint32_t word)
{
    if(word < 128) {
        return 1;
    } else if(word < 16384) {
        return 2;
    } else if(word < 2097152) {
        return 3;
    } else if(word < 268435456) {
        return 4;
    } else {
        return 5;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE unsigned int property_get_length(const char *property_name, json_t *value)
{
    hgobj gobj = 0;
    int str_len = 0;
    unsigned long v = 0;
    const char *name = 0;
    int identifier;
    int type;
    mosquitto_string_to_property_info(property_name, &identifier, &type);

    if(json_is_object(value)) {
        name = kw_get_str(gobj, value, "name", "", KW_REQUIRED);
        value = kw_get_dict_value(gobj, value, "value", 0, KW_REQUIRED);
    }

    if(json_is_string(value)) {
        if(strcmp(
            property_name, mqtt_property_identifier_to_string(MQTT_PROP_CORRELATION_DATA))==0
        ) {
            gbuffer_t *gbuf_correlation_data = 0;
            const char *b64 = json_string_value(value);
            gbuf_correlation_data = gbuffer_string_to_base64(b64, strlen(b64));
            str_len += gbuffer_leftbytes(gbuf_correlation_data);
            GBUFFER_DECREF(gbuf_correlation_data);

        } else if(strcmp(
            property_name, mqtt_property_identifier_to_string(MQTT_PROP_USER_PROPERTY))==0
        ) {
            str_len += strlen(name);
            str_len += strlen(json_string_value(value));

        } else {
            str_len += strlen(json_string_value(value));
        }
    }
    if(json_is_integer(value)) {
        v = json_integer_value(value);
    }

    switch(identifier) {
        /* Byte */
        case MQTT_PROP_PAYLOAD_FORMAT_INDICATOR:
        case MQTT_PROP_REQUEST_PROBLEM_INFORMATION:
        case MQTT_PROP_REQUEST_RESPONSE_INFORMATION:
        case MQTT_PROP_MAXIMUM_QOS:
        case MQTT_PROP_RETAIN_AVAILABLE:
        case MQTT_PROP_WILDCARD_SUB_AVAILABLE:
        case MQTT_PROP_SUBSCRIPTION_ID_AVAILABLE:
        case MQTT_PROP_SHARED_SUB_AVAILABLE:
            return 2; /* 1 (identifier) + 1 byte */

        /* uint16 */
        case MQTT_PROP_SERVER_KEEP_ALIVE:
        case MQTT_PROP_RECEIVE_MAXIMUM:
        case MQTT_PROP_TOPIC_ALIAS_MAXIMUM:
        case MQTT_PROP_TOPIC_ALIAS:
            return 3; /* 1 (identifier) + 2 bytes */

        /* uint32 */
        case MQTT_PROP_MESSAGE_EXPIRY_INTERVAL:
        case MQTT_PROP_WILL_DELAY_INTERVAL:
        case MQTT_PROP_MAXIMUM_PACKET_SIZE:
        case MQTT_PROP_SESSION_EXPIRY_INTERVAL:
            return 5; /* 1 (identifier) + 4 bytes */

        /* varint */
        case MQTT_PROP_SUBSCRIPTION_IDENTIFIER:
            if(v < 128) {
                return 2;
            } else if(v < 16384) {
                return 3;
            } else if(v < 2097152) {
                return 4;
            } else if(v < 268435456) {
                return 5;
            } else {
                return 0;
            }

        /* binary */
        case MQTT_PROP_CORRELATION_DATA:
        case MQTT_PROP_AUTHENTICATION_DATA:
            return 3U + str_len; /* 1 + 2 bytes (len) + X bytes (payload) */

        /* string */
        case MQTT_PROP_CONTENT_TYPE:
        case MQTT_PROP_RESPONSE_TOPIC:
        case MQTT_PROP_ASSIGNED_CLIENT_IDENTIFIER:
        case MQTT_PROP_AUTHENTICATION_METHOD:
        case MQTT_PROP_RESPONSE_INFORMATION:
        case MQTT_PROP_SERVER_REFERENCE:
        case MQTT_PROP_REASON_STRING:
            return 3U + str_len; /* 1 + 2 bytes (len) + X bytes (string) */

        /* string pair */
        case MQTT_PROP_USER_PROPERTY:
            return 5U + str_len;

        default:
            break;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE unsigned int property_get_length_all(json_t *props)
{
    unsigned int len = 0;

    const char *property_name; json_t *value;
    json_object_foreach(props, property_name, value) {
        len += property_get_length(property_name, value);
    }
    return len;
}

/***************************************************************************
 * Return the number of bytes we need to add on to the remaining length when
 * encoding these properties.
 ***************************************************************************/
PRIVATE unsigned int property_get_remaining_length(json_t *props)
{
    unsigned int proplen, varbytes;

    proplen = property_get_length_all(props);
    varbytes = packet_varint_bytes(proplen);
    return proplen + varbytes;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int mqtt_property_add_byte(hgobj gobj, json_t *proplist, int identifier, uint8_t value)
{
    if(!proplist) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt proplist NULL",
            "identifier",   "%d", identifier,
            NULL
        );
        return -1;
    }
    if(identifier != MQTT_PROP_PAYLOAD_FORMAT_INDICATOR
            && identifier != MQTT_PROP_REQUEST_PROBLEM_INFORMATION
            && identifier != MQTT_PROP_REQUEST_RESPONSE_INFORMATION
            && identifier != MQTT_PROP_MAXIMUM_QOS
            && identifier != MQTT_PROP_RETAIN_AVAILABLE
            && identifier != MQTT_PROP_WILDCARD_SUB_AVAILABLE
            && identifier != MQTT_PROP_SUBSCRIPTION_ID_AVAILABLE
            && identifier != MQTT_PROP_SHARED_SUB_AVAILABLE)
    {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt property byte unknown",
            "identifier",   "%d", identifier,
            NULL
        );
        return -1;
        return MOSQ_ERR_INVAL;
    }

    const char *property_name = mqtt_property_identifier_to_string(identifier);
    json_object_set_new(proplist, property_name, json_integer(value));
    //prop->client_generated = TRUE;
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int mqtt_property_add_int16(hgobj gobj, json_t *proplist, int identifier, uint16_t value)
{
    if(!proplist) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt proplist NULL",
            "identifier",   "%d", identifier,
            NULL
        );
        return -1;
    }
    if(identifier != MQTT_PROP_SERVER_KEEP_ALIVE
            && identifier != MQTT_PROP_RECEIVE_MAXIMUM
            && identifier != MQTT_PROP_TOPIC_ALIAS_MAXIMUM
            && identifier != MQTT_PROP_TOPIC_ALIAS)
    {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt property int16 unknown",
            "identifier",   "%d", identifier,
            NULL
        );
        return -1;
    }

    const char *property_name = mqtt_property_identifier_to_string(identifier);
    json_object_set_new(proplist, property_name, json_integer(value));
    // prop->client_generated = TRUE; TODO vale para algo?
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int mqtt_property_add_int32(hgobj gobj, json_t *proplist, int identifier, uint32_t value)
{
    if(!proplist) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt proplist NULL",
            "identifier",   "%d", identifier,
            NULL
        );
        return -1;
    }
    if(identifier != MQTT_PROP_MESSAGE_EXPIRY_INTERVAL
            && identifier != MQTT_PROP_SESSION_EXPIRY_INTERVAL
            && identifier != MQTT_PROP_WILL_DELAY_INTERVAL
            && identifier != MQTT_PROP_MAXIMUM_PACKET_SIZE)
    {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt property int32 unknown",
            "identifier",   "%d", identifier,
            NULL
        );
        return -1;
    }

    const char *property_name = mqtt_property_identifier_to_string(identifier);
    json_object_set_new(proplist, property_name, json_integer(value));
    // prop->client_generated = TRUE; TODO vale para algo?
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int mosquitto_property_add_varint(hgobj gobj, json_t *proplist, int identifier, uint32_t value)
{
    if(!proplist || value > 268435455) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt proplist NULL or value too big",
            "identifier",   "%d", identifier,
            NULL
        );
        return MOSQ_ERR_INVAL;
    }
    if(identifier != MQTT_PROP_SUBSCRIPTION_IDENTIFIER) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "No MQTT_PROP_SUBSCRIPTION_IDENTIFIER",
            "identifier",   "%d", identifier,
            NULL
        );
        return MOSQ_ERR_INVAL;
    }

    const char *property_name = mqtt_property_identifier_to_string(identifier);
    json_object_set_new(proplist, property_name, json_integer(value));

    return MOSQ_ERR_SUCCESS;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int mqtt_property_add_string(
    hgobj gobj,
    json_t *proplist,
    int identifier,
    const char *value
)
{
    size_t slen = 0;

    if(!proplist) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt proplist NULL",
            "identifier",   "%d", identifier,
            NULL
        );
        return -1;
    }
    if(value) {
        slen = strlen(value);
        if(mosquitto_validate_utf8(value, (int)slen)<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt bad utf8",
                NULL
            );
            return -1;
        }
    }

    if(identifier != MQTT_PROP_CONTENT_TYPE
            && identifier != MQTT_PROP_RESPONSE_TOPIC
            && identifier != MQTT_PROP_ASSIGNED_CLIENT_IDENTIFIER
            && identifier != MQTT_PROP_AUTHENTICATION_METHOD
            && identifier != MQTT_PROP_RESPONSE_INFORMATION
            && identifier != MQTT_PROP_SERVER_REFERENCE
            && identifier != MQTT_PROP_REASON_STRING
      ) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt property int16 unknown",
            "identifier",   "%d", identifier,
            NULL
        );
        return -1;
    }

    //prop->client_generated = TRUE; // TODO
    const char *property_name = mqtt_property_identifier_to_string(identifier);
    json_object_set_new(proplist, property_name, json_string(value?value:""));

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void mqtt_write_byte(gbuffer_t *gbuf, uint8_t byte)
{
    gbuffer_append_char(gbuf, byte);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void mqtt_write_uint16(gbuffer_t *gbuf, uint16_t word)
{
    gbuffer_append_char(gbuf, MOSQ_MSB(word));
    gbuffer_append_char(gbuf, MOSQ_LSB(word));
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void mqtt_write_uint32(gbuffer_t *gbuf, uint32_t word)
{
    gbuffer_append_char(gbuf, (uint8_t)((word & 0xFF000000) >> 24));
    gbuffer_append_char(gbuf, (uint8_t)((word & 0x00FF0000) >> 16));
    gbuffer_append_char(gbuf, (uint8_t)((word & 0x0000FF00) >> 8));
    gbuffer_append_char(gbuf, (uint8_t)((word & 0x000000FF)));
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int mqtt_write_varint(gbuffer_t *gbuf, uint32_t word)
{
    uint8_t byte;
    int count = 0;

    do {
        byte = (uint8_t)(word % 128);
        word = word / 128;
        /* If there are more digits to encode, set the top bit of this digit */
        if(word > 0) {
            byte = byte | 0x80;
        }
        mqtt_write_byte(gbuf, byte);
        count++;
    } while(word > 0 && count < 5);

    if(count == 5) {
        return -1;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void mqtt_write_bytes(gbuffer_t *gbuf, const void *bytes, uint32_t count)
{
    gbuffer_append(gbuf, (void *)bytes, count);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void mqtt_write_string(gbuffer_t *gbuf, const char *str)
{
    uint16_t length = strlen(str);
    mqtt_write_uint16(gbuf, length);
    mqtt_write_bytes(gbuf, str, length);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int property__write(hgobj gobj, gbuffer_t *gbuf, const char *property_name, json_t *value_)
{
    json_t *value;
    int identifier;
    int type;
    mosquitto_string_to_property_info(property_name, &identifier, &type);

    if(json_is_object(value_)) {
        value = kw_get_dict_value(gobj, value_, "value", 0, KW_REQUIRED);
    } else {
        value = value_;
    }

    mqtt_write_varint(gbuf, identifier);

    switch(identifier) {
        case MQTT_PROP_PAYLOAD_FORMAT_INDICATOR:
        case MQTT_PROP_REQUEST_PROBLEM_INFORMATION:
        case MQTT_PROP_REQUEST_RESPONSE_INFORMATION:
        case MQTT_PROP_MAXIMUM_QOS:
        case MQTT_PROP_RETAIN_AVAILABLE:
        case MQTT_PROP_WILDCARD_SUB_AVAILABLE:
        case MQTT_PROP_SUBSCRIPTION_ID_AVAILABLE:
        case MQTT_PROP_SHARED_SUB_AVAILABLE:
            mqtt_write_byte(gbuf, json_integer_value(value));
            break;

        case MQTT_PROP_SERVER_KEEP_ALIVE:
        case MQTT_PROP_RECEIVE_MAXIMUM:
        case MQTT_PROP_TOPIC_ALIAS_MAXIMUM:
        case MQTT_PROP_TOPIC_ALIAS:
            mqtt_write_uint16(gbuf, json_integer_value(value));
            break;

        case MQTT_PROP_MESSAGE_EXPIRY_INTERVAL:
        case MQTT_PROP_SESSION_EXPIRY_INTERVAL:
        case MQTT_PROP_WILL_DELAY_INTERVAL:
        case MQTT_PROP_MAXIMUM_PACKET_SIZE:
            mqtt_write_uint32(gbuf, json_integer_value(value));
            break;

        case MQTT_PROP_SUBSCRIPTION_IDENTIFIER:
            return mqtt_write_varint(gbuf, json_integer_value(value));

        case MQTT_PROP_CONTENT_TYPE:
        case MQTT_PROP_RESPONSE_TOPIC:
        case MQTT_PROP_ASSIGNED_CLIENT_IDENTIFIER:
        case MQTT_PROP_AUTHENTICATION_METHOD:
        case MQTT_PROP_RESPONSE_INFORMATION:
        case MQTT_PROP_SERVER_REFERENCE:
        case MQTT_PROP_REASON_STRING:
            mqtt_write_string(gbuf, json_string_value(value));
            break;

        case MQTT_PROP_AUTHENTICATION_DATA:
        case MQTT_PROP_CORRELATION_DATA:
            {
                const char *b64 = json_string_value(value);
                gbuffer_t *gbuf_binary_data = gbuffer_base64_to_string(b64, strlen(b64));
                void *p = gbuffer_cur_rd_pointer(gbuf_binary_data);
                uint16_t len = gbuffer_leftbytes(gbuf_binary_data);
                mqtt_write_uint16(gbuf, len);
                mqtt_write_bytes(gbuf, p, len);
                GBUFFER_DECREF(gbuf_binary_data);
            }
            break;

        case MQTT_PROP_USER_PROPERTY:
            {
                const char *name = kw_get_str(gobj, value_, "name", "", KW_REQUIRED);
                mqtt_write_string(gbuf, name);
                mqtt_write_string(gbuf, json_string_value(value));
            }
            break;

        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt auth: Unsupported property",
                "identifier",   "%d", identifier,
                NULL
            );
            return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int property_write_all(hgobj gobj, gbuffer_t *gbuf, json_t *props, BOOL write_len)
{
    if(write_len) {
        mqtt_write_varint(gbuf, property_get_length_all(props));
    }

    const char *property_name; json_t *value;
    json_object_foreach(props, property_name, value) {
        property__write(gobj, gbuf, property_name, value);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int mqtt_read_uint16(hgobj gobj, gbuffer_t *gbuf, uint16_t *word)
{
    uint8_t msb, lsb;

    if(gbuffer_leftbytes(gbuf) < 2) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt malformed packet, not enough data",
            NULL
        );
        //log_debug_full_gbuf(0, gbuf, "Mqtt malformed packet, not enough data");
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    msb = gbuffer_getchar(gbuf);
    lsb = gbuffer_getchar(gbuf);

    *word = (uint16_t)((msb<<8) + lsb);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int mqtt_read_uint32(hgobj gobj, gbuffer_t *gbuf, uint32_t *word)
{
    uint32_t val = 0;

    if(gbuffer_leftbytes(gbuf) < 4) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt malformed packet, not enough data",
            NULL
        );
        //log_debug_full_gbuf(0, gbuf, "Mqtt malformed packet, not enough data");
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    for(int i=0; i<4; i++) {
        uint8_t c = gbuffer_getchar(gbuf);
        val = (val << 8) + c;
    }

    *word = val;

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int mqtt_read_bytes(hgobj gobj, gbuffer_t *gbuf, void *bf, int bflen)
{
    if(gbuffer_leftbytes(gbuf) < bflen) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt malformed packet, not enough data",
            NULL
        );
        //log_debug_full_gbuf(0, gbuf, "Mqtt malformed packet, not enough data");
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    memmove(bf, gbuffer_get(gbuf, bflen), bflen);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int mqtt_read_byte(hgobj gobj, gbuffer_t *gbuf, uint8_t *byte)
{
    if(gbuffer_leftbytes(gbuf) < 1) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt malformed packet, not enough data",
            NULL
        );
        //log_debug_full_gbuf(0, gbuf, "Mqtt malformed packet, not enough data");
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    *byte = gbuffer_getchar(gbuf);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int mqtt_read_binary(hgobj gobj, gbuffer_t *gbuf, uint8_t **data, uint16_t *length)
{
    uint16_t slen;

    *data = NULL;
    *length = 0;

    if(mqtt_read_uint16(gobj, gbuf, &slen)<0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt malformed packet, not enough data",
            NULL
        );
        //log_debug_full_gbuf(0, gbuf, "Mqtt malformed packet, not enough data");
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(slen == 0) {
        *data = NULL;
        *length = 0;
        return 0;
    }

    if(gbuffer_leftbytes(gbuf) < slen) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt malformed packet, not enough data",
            NULL
        );
        //log_debug_full_gbuf(0, gbuf, "Mqtt malformed packet, not enough data");
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    *data = gbuffer_get(gbuf, slen);
    *length = slen;
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int mqtt_read_string(hgobj gobj, gbuffer_t *gbuf, char **str, uint16_t *length)
{
    *length = 0;
    *str = NULL;

    if(mqtt_read_binary(gobj, gbuf, (uint8_t **)str, length)<0) {
        // Error already logged
        return -1;
    }
    if(*length == 0) {
        return 0;
    }

    if(mosquitto_validate_utf8(*str, (int)(*length))<0) {
        *str = NULL;
        *length = 0;
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "malformed utf8",
            NULL
        );
        return MOSQ_ERR_MALFORMED_UTF8;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int mqtt_read_varint(hgobj gobj, gbuffer_t *gbuf, uint32_t *word, uint8_t *bytes)
{
    int i;
    uint8_t byte;
    unsigned int remaining_mult = 1;
    uint32_t lword = 0;
    uint8_t lbytes = 0;

    for(i=0; i<4; i++) {
        if(gbuffer_leftbytes(gbuf)>0) {
            lbytes++;
            byte = gbuffer_getchar(gbuf);
            lword += (byte & 127) * remaining_mult;
            remaining_mult *= 128;
            if((byte & 128) == 0) {
                if(lbytes > 1 && byte == 0) {
                    /* Catch overlong encodings */
                    break;
                } else {
                    *word = lword;
                    if(bytes) {
                        (*bytes) = lbytes;
                    }
                    return 0;
                }
            }
        } else {
            break;
        }
    }

    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_MQTT_ERROR,
        "msg",          "%s", "Mqtt malformed packet, not enough data",
        NULL
    );
    //log_debug_full_gbuf(0, gbuf, "Mqtt malformed packet, not enough data");
    return MOSQ_ERR_MALFORMED_PACKET;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int mosquitto_property_check_command(hgobj gobj, int command, int identifier)
{
    switch(identifier) {
        case MQTT_PROP_PAYLOAD_FORMAT_INDICATOR:
        case MQTT_PROP_MESSAGE_EXPIRY_INTERVAL:
        case MQTT_PROP_CONTENT_TYPE:
        case MQTT_PROP_RESPONSE_TOPIC:
        case MQTT_PROP_CORRELATION_DATA:
            if(command != CMD_PUBLISH && command != CMD_WILL) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt invalid property of command",
                    "command",      "%d", get_command_name(command),
                    "identifier",   "%d", identifier,
                    NULL
                );
                return MOSQ_ERR_PROTOCOL;
            }
            break;

        case MQTT_PROP_SUBSCRIPTION_IDENTIFIER:
            if(command != CMD_PUBLISH && command != CMD_SUBSCRIBE) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt invalid property of command",
                    "command",      "%d", get_command_name(command),
                    "identifier",   "%d", identifier,
                    NULL
                );
                return MOSQ_ERR_PROTOCOL;
            }
            break;

        case MQTT_PROP_SESSION_EXPIRY_INTERVAL:
            if(command != CMD_CONNECT && command != CMD_CONNACK && command != CMD_DISCONNECT) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt invalid property of command",
                    "command",      "%d", get_command_name(command),
                    "identifier",   "%d", identifier,
                    NULL
                );
                return MOSQ_ERR_PROTOCOL;
            }
            break;

        case MQTT_PROP_AUTHENTICATION_METHOD:
        case MQTT_PROP_AUTHENTICATION_DATA:
            if(command != CMD_CONNECT && command != CMD_CONNACK && command != CMD_AUTH) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt invalid property of command",
                    "command",      "%d", get_command_name(command),
                    "identifier",   "%d", identifier,
                    NULL
                );
                return MOSQ_ERR_PROTOCOL;
            }
            break;

        case MQTT_PROP_ASSIGNED_CLIENT_IDENTIFIER:
        case MQTT_PROP_SERVER_KEEP_ALIVE:
        case MQTT_PROP_RESPONSE_INFORMATION:
        case MQTT_PROP_MAXIMUM_QOS:
        case MQTT_PROP_RETAIN_AVAILABLE:
        case MQTT_PROP_WILDCARD_SUB_AVAILABLE:
        case MQTT_PROP_SUBSCRIPTION_ID_AVAILABLE:
        case MQTT_PROP_SHARED_SUB_AVAILABLE:
            if(command != CMD_CONNACK) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt invalid property of command",
                    "command",      "%d", get_command_name(command),
                    "identifier",   "%d", identifier,
                    NULL
                );
                return MOSQ_ERR_PROTOCOL;
            }
            break;

        case MQTT_PROP_WILL_DELAY_INTERVAL:
            if(command != CMD_WILL) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt invalid property of command",
                    "command",      "%d", get_command_name(command),
                    "identifier",   "%d", identifier,
                    NULL
                );
                return MOSQ_ERR_PROTOCOL;
            }
            break;

        case MQTT_PROP_REQUEST_PROBLEM_INFORMATION:
        case MQTT_PROP_REQUEST_RESPONSE_INFORMATION:
            if(command != CMD_CONNECT) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt invalid property of command",
                    "command",      "%d", get_command_name(command),
                    "identifier",   "%d", identifier,
                    NULL
                );
                return MOSQ_ERR_PROTOCOL;
            }
            break;

        case MQTT_PROP_SERVER_REFERENCE:
            if(command != CMD_CONNACK && command != CMD_DISCONNECT) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt invalid property of command",
                    "command",      "%d", get_command_name(command),
                    "identifier",   "%d", identifier,
                    NULL
                );
                return MOSQ_ERR_PROTOCOL;
            }
            break;

        case MQTT_PROP_REASON_STRING:
            if(command == CMD_CONNECT || command == CMD_PUBLISH ||
                command == CMD_SUBSCRIBE || command == CMD_UNSUBSCRIBE) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt invalid property of command",
                    "command",      "%d", get_command_name(command),
                    "identifier",   "%d", identifier,
                    NULL
                );
                return MOSQ_ERR_PROTOCOL;
            }
            break;

        case MQTT_PROP_RECEIVE_MAXIMUM:
        case MQTT_PROP_TOPIC_ALIAS_MAXIMUM:
        case MQTT_PROP_MAXIMUM_PACKET_SIZE:
            if(command != CMD_CONNECT && command != CMD_CONNACK) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt invalid property of command",
                    "command",      "%d", get_command_name(command),
                    "identifier",   "%d", identifier,
                    NULL
                );
                return MOSQ_ERR_PROTOCOL;
            }
            break;

        case MQTT_PROP_TOPIC_ALIAS:
            if(command != CMD_PUBLISH) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt invalid property of command",
                    "command",      "%d", get_command_name(command),
                    "identifier",   "%d", identifier,
                    NULL
                );
                return MOSQ_ERR_PROTOCOL;
            }
            break;

        case MQTT_PROP_USER_PROPERTY:
            break;

        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt unknown property of command",
                "command",      "%d", get_command_name(command),
                "identifier",   "%d", identifier,
                NULL
            );
            return MOSQ_ERR_PROTOCOL;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int property_read(hgobj gobj, gbuffer_t *gbuf, uint32_t *len, json_t *all_properties)
{
    uint8_t byte;
    uint8_t byte_count;
    uint16_t uint16;
    uint32_t uint32;
    uint32_t varint;
    char *str1, *str2;
    uint16_t slen1, slen2;

    uint32_t property_identifier;
    if(mqtt_read_varint(gobj, gbuf, &property_identifier, NULL)<0) {
        // Error already logged
        return MOSQ_ERR_MALFORMED_PACKET;
    }
    const char *property_name = mqtt_property_identifier_to_string(property_identifier);

    /* Check for duplicates (only if not MQTT_PROP_USER_PROPERTY, why?)*/
    if(property_identifier != MQTT_PROP_USER_PROPERTY) {
        if(kw_has_key(all_properties, property_name)) {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt duplicate property",
                "property_type","%d", (int)property_identifier,
                "property_name","%s", property_name,
                "mqtt_error",   "%d", MOSQ_ERR_DUPLICATE_PROPERTY,
                NULL
            );
            return MOSQ_ERR_DUPLICATE_PROPERTY;
        }
    }

    json_t *property = json_object();
    json_object_set_new(property, "identifier", json_integer(property_identifier));
    json_object_set_new(
        property,
        "name",
        json_string(property_name)
    );
    int identifier_, type_;
    mosquitto_string_to_property_info(property_name, &identifier_, &type_);
    json_object_set_new(
        property,
        "type",
        json_integer(type_)
    );

    *len -= 1;

    switch(property_identifier) {
        case MQTT_PROP_PAYLOAD_FORMAT_INDICATOR:
        case MQTT_PROP_REQUEST_PROBLEM_INFORMATION:
        case MQTT_PROP_REQUEST_RESPONSE_INFORMATION:
        case MQTT_PROP_MAXIMUM_QOS:
        case MQTT_PROP_RETAIN_AVAILABLE:
        case MQTT_PROP_WILDCARD_SUB_AVAILABLE:
        case MQTT_PROP_SUBSCRIPTION_ID_AVAILABLE:
        case MQTT_PROP_SHARED_SUB_AVAILABLE:
            if(mqtt_read_byte(gobj, gbuf, &byte)<0) {
                // Error already logged
                JSON_DECREF(property);
                return MOSQ_ERR_MALFORMED_PACKET;
            }
            *len -= 1; /* byte */
            json_object_set_new(property, "value", json_integer(byte));
            break;

        case MQTT_PROP_SERVER_KEEP_ALIVE:
        case MQTT_PROP_RECEIVE_MAXIMUM:
        case MQTT_PROP_TOPIC_ALIAS_MAXIMUM:
        case MQTT_PROP_TOPIC_ALIAS:
            if(mqtt_read_uint16(gobj, gbuf, &uint16)<0) {
                // Error already logged
                JSON_DECREF(property);
                return MOSQ_ERR_MALFORMED_PACKET;
            }
            *len -= 2; /* uint16 */
            json_object_set_new(property, "value", json_integer(uint16));
            break;

        case MQTT_PROP_MESSAGE_EXPIRY_INTERVAL:
        case MQTT_PROP_SESSION_EXPIRY_INTERVAL:
        case MQTT_PROP_WILL_DELAY_INTERVAL:
        case MQTT_PROP_MAXIMUM_PACKET_SIZE:
            if(mqtt_read_uint32(gobj, gbuf, &uint32)<0) {
                // Error already logged
                JSON_DECREF(property);
                return MOSQ_ERR_MALFORMED_PACKET;
            }
            *len -= 4; /* uint32 */
            json_object_set_new(property, "value", json_integer(uint32));
            break;

        case MQTT_PROP_SUBSCRIPTION_IDENTIFIER:
            if(mqtt_read_varint(gobj, gbuf, &varint, &byte_count)<0) {
                // Error already logged
                JSON_DECREF(property);
                return MOSQ_ERR_MALFORMED_PACKET;
            }
            *len -= byte_count;
            json_object_set_new(property, "value", json_integer(varint));
            break;

        case MQTT_PROP_CONTENT_TYPE:
        case MQTT_PROP_RESPONSE_TOPIC:
        case MQTT_PROP_ASSIGNED_CLIENT_IDENTIFIER:
        case MQTT_PROP_AUTHENTICATION_METHOD:
        case MQTT_PROP_RESPONSE_INFORMATION:
        case MQTT_PROP_SERVER_REFERENCE:
        case MQTT_PROP_REASON_STRING:
            if(mqtt_read_string(gobj, gbuf, &str1, &slen1)<0) {
                // Error already logged
                JSON_DECREF(property);
                return MOSQ_ERR_MALFORMED_PACKET;
            }
            *len = (*len) - 2 - slen1; /* uint16, string len */
            json_object_set_new(property, "value", json_sprintf("%*.*s", slen1, slen1, str1));
            json_object_set_new(property, "value_length", json_integer(slen1));
            break;

        case MQTT_PROP_AUTHENTICATION_DATA:
        case MQTT_PROP_CORRELATION_DATA:
            if(mqtt_read_binary(gobj, gbuf, (uint8_t **)&str1, &slen1)<0) {
                // Error already logged
                JSON_DECREF(property);
                return MOSQ_ERR_MALFORMED_PACKET;
            }
            *len = (*len) - 2 - slen1; /* uint16, binary len */

            // Save binary data in base64
            gbuffer_t *gbuf_b64 = gbuffer_string_to_base64(str1, slen1);
            json_object_set_new(property, "value", json_string(gbuffer_cur_rd_pointer(gbuf_b64)));
            json_object_set_new(property, "value_length", json_integer(slen1));
            GBUFFER_DECREF(gbuf_b64);
            break;

        case MQTT_PROP_USER_PROPERTY:
            if(mqtt_read_string(gobj, gbuf, &str1, &slen1)<0) {
                // Error already logged
                JSON_DECREF(property);
                return MOSQ_ERR_MALFORMED_PACKET;
            }
            *len = (*len) - 2 - slen1; /* uint16, string len */

            if(mqtt_read_string(gobj, gbuf, &str2, &slen2)<0) {
                // Error already logged
                JSON_DECREF(property);
                return MOSQ_ERR_MALFORMED_PACKET;
            }

            *len = (*len) - 2 - slen2; /* uint16, string len */

            json_object_set_new(property, "name", json_sprintf("%*.*s", slen1, slen1, str1));
            json_object_set_new(property, "name_length", json_integer(slen1));

            json_object_set_new(property, "value", json_sprintf("%*.*s", slen2, slen2, str2));
            json_object_set_new(property, "value_length", json_integer(slen2));
            break;

        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt Unsupported property type",
                "property_type","%d", (int)property_identifier,
                NULL
            );
            JSON_DECREF(property);
            return MOSQ_ERR_MALFORMED_PACKET;
    }

    json_object_set_new(all_properties, property_name, property);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int mqtt_property_check_all(hgobj gobj, int command, json_t *all_properties)
{
    int ret = 0;
    const char *property_name; json_t *property;
    json_object_foreach(all_properties, property_name, property) {
        /* Validity checks */
        int identifier = kw_get_int(gobj, property, "identifier", 0, KW_REQUIRED);
        if(identifier == MQTT_PROP_REQUEST_PROBLEM_INFORMATION
                || identifier == MQTT_PROP_PAYLOAD_FORMAT_INDICATOR
                || identifier == MQTT_PROP_REQUEST_RESPONSE_INFORMATION
                || identifier == MQTT_PROP_MAXIMUM_QOS
                || identifier == MQTT_PROP_RETAIN_AVAILABLE
                || identifier == MQTT_PROP_WILDCARD_SUB_AVAILABLE
                || identifier == MQTT_PROP_SUBSCRIPTION_ID_AVAILABLE
                || identifier == MQTT_PROP_SHARED_SUB_AVAILABLE) {

            int value = kw_get_int(gobj, property, "value", 0, KW_REQUIRED);
            if(value > 1) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt check property failed 1",
                    "property",     "%j", property,
                    NULL
                );
                return MOSQ_ERR_PROTOCOL;
            }
        } else if(identifier == MQTT_PROP_MAXIMUM_PACKET_SIZE) {
            int value = kw_get_int(gobj, property, "value", 0, KW_REQUIRED);
            if(value == 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt check property failed 2",
                    "property",     "%j", property,
                    NULL
                );
                return MOSQ_ERR_PROTOCOL;
            }
        } else if(identifier == MQTT_PROP_RECEIVE_MAXIMUM
                || identifier == MQTT_PROP_TOPIC_ALIAS) {

            int value = kw_get_int(gobj, property, "value", 0, KW_REQUIRED);
            if(value == 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt check property failed 3",
                    "property",     "%j", property,
                    NULL
                );
                return MOSQ_ERR_PROTOCOL;
            }
        }

        /* Check for properties on incorrect commands */
        if((ret=mosquitto_property_check_command(gobj, command, identifier))<0) {
            // Error already logged
            return ret;
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *property_read_all(hgobj gobj, gbuffer_t *gbuf, int command, int *error)
{
    uint32_t proplen;
    int ret;
    if(error) {
        *error = 0;
    }

    if(mqtt_read_varint(gobj, gbuf, &proplen, NULL)<0) {
        // Error already logged
        return 0;
    }

    json_t *all_properties = json_object();

    while(proplen > 0) {
        if((ret=property_read(gobj, gbuf, &proplen, all_properties))<0) {
            // Error already logged
            JSON_DECREF(all_properties);
            if(error) {
                *error = ret;
            }
            return 0;
        }
    }

    if((ret=mqtt_property_check_all(gobj, command, all_properties))<0) {
        // Error already logged
        JSON_DECREF(all_properties);
        if(error) {
            *error = ret;
        }
        return 0;
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        gobj_trace_json(gobj, all_properties, "all_properties, command %s", get_command_name(command));
    }

    return all_properties;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *property_get_property(json_t *properties, int identifier)
{
    hgobj gobj = 0;
    const char *identifier_name = mqtt_property_identifier_to_string(identifier);
    json_t *property = kw_get_dict(gobj, properties, identifier_name, 0, 0);
    return property;
}

/***************************************************************************
 *  Use instead of: mosquitto_property_read_varint
 ***************************************************************************/
PRIVATE json_int_t property_get_int(json_t *properties, int identifier)
{
    hgobj gobj = 0;
    json_t *property = property_get_property(properties, identifier);
    return kw_get_int(gobj, property, "value", -1, 0);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int property_process_connect(hgobj gobj, json_t *all_properties)
{
    const char *property_name; json_t *property;
    json_object_foreach(all_properties, property_name, property) {
        json_int_t identifier = kw_get_int(gobj, property, "identifier", 0, KW_REQUIRED);

        switch(identifier) {
            case MQTT_PROP_SESSION_EXPIRY_INTERVAL:
                {
                    json_int_t value = kw_get_int(gobj, property, "value", 0, KW_REQUIRED);
                    gobj_write_integer_attr(gobj, "session_expiry_interval", value);
                }
                break;

            case MQTT_PROP_RECEIVE_MAXIMUM:
                {
                    json_int_t value = kw_get_int(gobj, property, "value", 0, KW_REQUIRED);
                    if(value != 0) {
                        //return -1;
                    } else {
                        gobj_write_integer_attr(gobj, "msgs_out_inflight_maximum", value);
                        gobj_write_integer_attr(gobj, "msgs_out_inflight_quota", value);
                    }
                }
                break;

            case MQTT_PROP_MAXIMUM_PACKET_SIZE:
                {
                    json_int_t value = kw_get_int(gobj, property, "value", 0, KW_REQUIRED);
                    if(value == 0) {
                        //return -1;
                    } else {
                        gobj_write_integer_attr(gobj, "maximum_packet_size", value);
                    }
                }
                break;

            case MQTT_PROP_AUTHENTICATION_METHOD:
                {
                    const char *value = kw_get_str(gobj, property, "value", "", KW_REQUIRED);
                    gobj_write_str_attr(gobj, "auth_method", value);
                }
                break;

            case MQTT_PROP_AUTHENTICATION_DATA:
                {
                    const char *value = kw_get_str(gobj, property, "value", "", KW_REQUIRED);
                    gobj_write_str_attr(gobj, "auth_data", value);
                }
                break;
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int property_process_will(hgobj gobj, json_t *all_properties)
{
    const char *property_name; json_t *property;
    json_object_foreach(all_properties, property_name, property) {
        json_int_t identifier = kw_get_int(gobj, property, "identifier", 0, KW_REQUIRED);
        switch(identifier) {
            case MQTT_PROP_CONTENT_TYPE:
            case MQTT_PROP_CORRELATION_DATA:
            case MQTT_PROP_PAYLOAD_FORMAT_INDICATOR:
            case MQTT_PROP_RESPONSE_TOPIC:
            case MQTT_PROP_USER_PROPERTY:
                break;

            case MQTT_PROP_WILL_DELAY_INTERVAL:
                {
                    json_int_t value = kw_get_int(gobj, property, "value", 0, KW_REQUIRED);
                    gobj_write_integer_attr(gobj, "will_delay_interval", value);
                }
                break;

            case MQTT_PROP_MESSAGE_EXPIRY_INTERVAL:
                {
                    json_int_t value = kw_get_int(gobj, property, "value", 0, KW_REQUIRED);
                    gobj_write_integer_attr(gobj, "will_expiry_interval", value);
                }
                break;

            default:
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt auth: will property unknown",
                    "identifier",   "%d", identifier,
                    NULL
                );
                return -1;
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int packet_check_oversize(hgobj gobj, uint32_t remaining_length)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint32_t len;

    if(priv->maximum_packet_size == 0) {
        return 0;
    }

    len = remaining_length + packet_varint_bytes(remaining_length);
    if(len > priv->maximum_packet_size) {
        return -1;
    } else {
        return 0;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send_packet(hgobj gobj, gbuffer_t *gbuf)
{
    if(gobj_trace_level(gobj) & TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "%s ==> %s",
            gobj_short_name(gobj),
            gobj_short_name(gobj_bottom_gobj(gobj))
        );
    }
    json_t *kw = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    return gobj_send_event(gobj_bottom_gobj(gobj), EV_TX_DATA, kw, gobj);
}

/***************************************************************************
 *  For DISCONNECT, PINGREQ and PINGRESP
 ***************************************************************************/
PRIVATE int send_simple_command(hgobj gobj, uint8_t command)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = build_mqtt_packet(gobj, command, 0);
    if(!gbuf) {
        // Error already logged
        return MOSQ_ERR_NOMEM;
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("👉👉 Sending %s to '%s'",
            get_command_name(command),
            priv->client_id
        );
    }
    return send_packet(gobj, gbuf);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send_connack(
    hgobj gobj,
    uint8_t ack,
    uint8_t reason_code,
    json_t *connack_props // owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint32_t remaining_length = 2;

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("👉👉 Sending CONNACK to '%s' %s (ack %d, reason code %d)",
            priv->client_id,
            gobj_short_name(gobj_bottom_gobj(gobj)),
            ack,
            reason_code
        );
        if(connack_props) {
            gobj_trace_json(gobj, connack_props, "Sending CONNACK properties");
        }
    }

    if(priv->protocol_version == mosq_p_mqtt5) {
        if(reason_code < 128 && priv->retain_available == FALSE) {
            mqtt_property_add_byte(gobj, connack_props, MQTT_PROP_RETAIN_AVAILABLE, 0);
        }
        if(reason_code < 128 && priv->max_packet_size > 0) {
            mqtt_property_add_int32(gobj, connack_props, MQTT_PROP_MAXIMUM_PACKET_SIZE, priv->max_packet_size);
        }
        if(reason_code < 128 && priv->max_inflight_messages > 0) {
            mqtt_property_add_int16(
                gobj, connack_props, MQTT_PROP_RECEIVE_MAXIMUM, priv->max_inflight_messages
            );
        }
        if(priv->max_qos != 2) {
            mqtt_property_add_byte(gobj, connack_props, MQTT_PROP_MAXIMUM_QOS, priv->max_qos);
        }

        remaining_length += property_get_remaining_length(connack_props);
    }

    if(packet_check_oversize(gobj, remaining_length)) {
        JSON_DECREF(connack_props);
        return -1;
    }

    gbuffer_t *gbuf = build_mqtt_packet(gobj, CMD_CONNACK, remaining_length);
    if(!gbuf) {
        // Error already logged
        JSON_DECREF(connack_props);
        return MOSQ_ERR_NOMEM;
    }
    gbuffer_append_char(gbuf, ack);
    gbuffer_append_char(gbuf, reason_code);
    if(priv->protocol_version == mosq_p_mqtt5) {
        property_write_all(gobj, gbuf, connack_props, TRUE);
    }
    JSON_DECREF(connack_props);
    return send_packet(gobj, gbuf);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send_disconnect(
    hgobj gobj,
    uint8_t reason_code,
    json_t *properties
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_bool_attr(gobj, "send_disconnect", FALSE);

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        if(priv->iamServer) {
            if(priv->is_bridge) {
                trace_msg0("👉👉 Bridge Sending DISCONNECT to '%s' ('%s', %d)",
                    priv->client_id,
                    mosquitto_reason_string(reason_code),
                    reason_code
                );
            } else  {
                trace_msg0("👉👉 Sending DISCONNECT to '%s' ('%s', %d)",
                    priv->client_id,
                    mosquitto_reason_string(reason_code),
                    reason_code
                );
            }
        } else {
            trace_msg0("👉👉 Sending client DISCONNECT to '%s'", priv->client_id);
        }
    }

    uint32_t remaining_length;

    if(priv->protocol_version == mosq_p_mqtt5 && (reason_code != 0 || properties)) {
        remaining_length = 1;
        if(properties) {
            remaining_length += property_get_remaining_length(properties);
        }
    } else {
        remaining_length = 0;
    }

    gbuffer_t *gbuf = build_mqtt_packet(gobj, CMD_DISCONNECT, remaining_length);
    if(!gbuf) {
        // Error already logged
        return MOSQ_ERR_NOMEM;
    }

    if(priv->protocol_version == mosq_p_mqtt5 && (reason_code != 0 || properties)) {
        gbuffer_append_char(gbuf, reason_code);
        if(properties) {
            property_write_all(gobj, gbuf, properties, TRUE);
        }
    }

    return send_packet(gobj, gbuf);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send__suback(hgobj gobj, uint16_t mid, uint32_t payloadlen, const void *payload)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *properties = NULL;
    uint32_t remaining_length = 2 + payloadlen;

    if(priv->protocol_version == mosq_p_mqtt5) {
        /* We don't use Reason String or User Property yet. */
        remaining_length += property_get_remaining_length(properties);
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("👉👉 Sending SUBACK to '%s' %s",
            priv->client_id,
            gobj_short_name(gobj_bottom_gobj(gobj))
        );
        if(payloadlen > 0) {
            gobj_trace_dump(gobj, payload, payloadlen, "   SUBACK payload");
        }
        if(properties) {
            gobj_trace_json(gobj, properties, "   SUBACK properties");
        }
    }

    gbuffer_t *gbuf = build_mqtt_packet(gobj, CMD_SUBACK, remaining_length);
    if(!gbuf) {
        // Error already logged
        return MOSQ_ERR_NOMEM;
    }

    mqtt_write_uint16(gbuf, mid);

    if(priv->protocol_version == mosq_p_mqtt5) {
        property_write_all(gobj, gbuf, properties, TRUE);
    }

    if(payloadlen) {
        mqtt_write_bytes(gbuf, payload, payloadlen);
    }

    return send_packet(gobj, gbuf);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send__unsuback(
    hgobj gobj,
    uint16_t mid,
    int reason_code_count,
    uint8_t *reason_codes,
    json_t *properties)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("👉👉 Sending UNSUBACK to '%s' %s",
            priv->client_id,
            gobj_short_name(gobj_bottom_gobj(gobj))
        );
    }

    uint32_t remaining_length = 2;

    if(priv->protocol_version == mosq_p_mqtt5) {
        remaining_length += property_get_remaining_length(properties);
        remaining_length += (uint32_t)reason_code_count;
    }

    gbuffer_t *gbuf = build_mqtt_packet(gobj, CMD_UNSUBACK, remaining_length);
    if(!gbuf) {
        // Error already logged
        return MOSQ_ERR_NOMEM;
    }

    mqtt_write_uint16(gbuf, mid);

    if(priv->protocol_version == mosq_p_mqtt5) {
        property_write_all(gobj, gbuf, properties, TRUE);
        mqtt_write_bytes(gbuf, reason_codes, (uint32_t)reason_code_count);
    }

    return send_packet(gobj, gbuf);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__pingreq(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->in_session) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt CMD_PINGREQ: not in session",
            NULL
        );
        return -1;
    }
    if(!priv->iamServer) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt CMD_PINGREQ: not server",
            NULL
        );
        return -1;
    }

    if(priv->frame_head.flags != 0) {
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    // TODO mosq->ping_t = mosquitto_time(); esto no está en esta función!!
    return send_simple_command(gobj, CMD_PINGRESP);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__pingresp(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->in_session) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt CMD_PINGRESP: not in session",
            NULL
        );
        return -1;
    }

    // mosq->ping_t = 0; /* No longer waiting for a PINGRESP. */

    if(!priv->is_bridge) {
        // Parece que el broker no debe recibir pingresp
        return MOSQ_ERR_PROTOCOL;
    }

    return send_simple_command(gobj, CMD_PINGRESP);
}

/***************************************************************************
 *  For PUBACK, PUBCOMP, PUBREC, and PUBREL
 ***************************************************************************/
PRIVATE int send_command_with_mid(
    hgobj gobj,
    uint8_t command,
    uint16_t mid,
    BOOL dup,
    uint8_t reason_code,
    json_t *properties
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int remaining_length = 2;

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("👉👉 Sending %s to '%s', mid %ld ('%s', %d)",
            get_command_name(command & 0xF0),
            priv->client_id,
            (long)mid,
            mosquitto_reason_string(reason_code),
            reason_code
        );
    }

    if(dup) {
        command |= 8;
    }
    if(priv->protocol_version == mosq_p_mqtt5) {
        if(reason_code != 0 || properties) {
            remaining_length += 1;
        }

        if(properties) {
            remaining_length += property_get_remaining_length(properties);
        }
    }
    gbuffer_t *gbuf = build_mqtt_packet(gobj, command, remaining_length);
    if(!gbuf) {
        // Error already logged
        return MOSQ_ERR_NOMEM;
    }

    mqtt_write_uint16(gbuf, mid);

    if(priv->protocol_version == mosq_p_mqtt5) {
        if(reason_code != 0 || properties) {
            mqtt_write_byte(gbuf, reason_code);
        }
        if(properties) {
            property_write_all(gobj, gbuf, properties, TRUE);
        }
    }

    JSON_DECREF(properties)

    return send_packet(gobj, gbuf);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send_puback(hgobj gobj, uint16_t mid, uint8_t reason_code, json_t *properties)
{
    //util__increment_receive_quota(mosq);
    /* We don't use Reason String or User Property yet. */
    return send_command_with_mid(gobj, CMD_PUBACK, mid, FALSE, reason_code, properties);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send_pubcomp(hgobj gobj, uint16_t mid, json_t *properties)
{
    //util__increment_receive_quota(mosq);
    /* We don't use Reason String or User Property yet. */
    return send_command_with_mid(gobj, CMD_PUBCOMP, mid, FALSE, 0, properties);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send_pubrec(hgobj gobj, uint16_t mid, uint8_t reason_code, json_t *properties)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(reason_code >= 0x80 && priv->protocol_version == mosq_p_mqtt5) {
        //util__increment_receive_quota(mosq);
    }
    /* We don't use Reason String or User Property yet. */
    return send_command_with_mid(gobj, CMD_PUBREC, mid, FALSE, reason_code, properties);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send__pubrel(hgobj gobj, uint16_t mid, json_t *properties)
{
    /* We don't use Reason String or User Property yet. */
    return send_command_with_mid(gobj, CMD_PUBREL|2, mid, FALSE, 0, properties);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send_publish(
    hgobj gobj,
    uint16_t mid,
    const char *topic,
    uint32_t payloadlen,
    const void *payload,
    uint8_t qos,
    BOOL retain,
    BOOL dup,
    json_t *cmsg_props, // not owned
    json_t *store_props, // not owned
    uint32_t expiry_interval
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->retain_available) {
        retain = FALSE;
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("👉👉 Sending PUBLISH to '%s', topic '%s' (dup %d, qos %d, retain %d, mid %d)",
            SAFE_PRINT(priv->client_id),
            topic,
            dup,
            qos,
            retain,
            mid
        );
    }

    unsigned int packetlen;
    unsigned int proplen = 0, varbytes;
    json_t *expiry_prop = 0;

    if(topic) {
        packetlen = 2 + (unsigned int)strlen(topic) + payloadlen;
    } else {
        packetlen = 2 + payloadlen;
    }
    if(qos > 0) {
        packetlen += 2; /* For message id */
    }
    if(priv->protocol_version == mosq_p_mqtt5) {
        proplen = 0;
        proplen += property_get_length_all(cmsg_props);
        proplen += property_get_length_all(store_props);
        if(expiry_interval > 0) {
            expiry_prop = json_object();

            mqtt_property_add_int32(
                gobj, expiry_prop, MQTT_PROP_MESSAGE_EXPIRY_INTERVAL, expiry_interval
            );
            // expiry_prop.client_generated = FALSE;
            proplen += property_get_length_all(expiry_prop);
        }

        varbytes = packet_varint_bytes(proplen);
        if(varbytes > 4) {
            /* FIXME - Properties too big, don't publish any - should remove some first really */
            cmsg_props = NULL;
            store_props = NULL;
            expiry_interval = 0;
        } else {
            packetlen += proplen + varbytes;
        }
    }
    if(packet_check_oversize(gobj, packetlen)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Dropping too large outgoing PUBLISH",
            "packetlen",    "%d", packetlen,
            NULL
        );
        return MOSQ_ERR_OVERSIZE_PACKET;
    }

    uint8_t command = (uint8_t)(CMD_PUBLISH | (uint8_t)((dup&0x1)<<3) | (uint8_t)(qos<<1) | retain);

    gbuffer_t *gbuf = build_mqtt_packet(gobj, command, packetlen);
    if(!gbuf) {
        // Error already logged
        return MOSQ_ERR_NOMEM;
    }

    /* Variable header (topic string) */
    if(topic) {
        mqtt_write_string(gbuf, topic);
    } else {
        mqtt_write_uint16(gbuf, 0);
    }
    if(qos > 0) {
        mqtt_write_uint16(gbuf, mid);
    }

    if(priv->protocol_version == mosq_p_mqtt5) {
        mqtt_write_varint(gbuf, proplen);
        property_write_all(gobj, gbuf, cmsg_props, FALSE);
        property_write_all(gbuf, gbuf, store_props, FALSE);
        if(expiry_interval > 0) {
            property_write_all(gobj, gbuf, expiry_prop, FALSE);
        }
    }
    JSON_DECREF(expiry_prop);

    /* Payload */
    if(payloadlen) {
        mqtt_write_bytes(gbuf, payload, payloadlen);
    }

    return send_packet(gobj, gbuf);
}

/***************************************************************************
 * Check that a topic used for publishing is valid.
 * Search for + or # in a topic. Return MOSQ_ERR_INVAL if found.
 * Also returns MOSQ_ERR_INVAL if the topic string is too long.
 * Returns MOSQ_ERR_SUCCESS if everything is fine.
 ***************************************************************************/
PRIVATE int mosquitto_pub_topic_check(const char *topic)
{
    int len = 0;
    int hier_count = 0;

    if(topic == NULL) {
        return -1;
    }
    const char *str = topic;
    while(str && *str) {
        if(*str == '+' || *str == '#') {
            return MOSQ_ERR_INVAL;
        } else if(*str == '/') {
            hier_count++;
        }
        len++;
        str++;
    }
    if(len > TOPIC_MAX) {
        return -1;
    }
    if(hier_count > TOPIC_HIERARCHY_LIMIT) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 * Check that a topic used for subscriptions is valid.
 * Search for + or # in a topic, check they aren't in invalid positions such as
 * foo/#/bar, foo/+bar or foo/bar#.
 * Return MOSQ_ERR_INVAL if invalid position found.
 * Also returns MOSQ_ERR_INVAL if the topic string is too long.
 * Returns MOSQ_ERR_SUCCESS if everything is fine.
 ***************************************************************************/
PRIVATE int mosquitto_sub_topic_check(const char *str)
{
    char c = '\0';
    int len = 0;
    int hier_count = 0;

    if(str == NULL) {
        return MOSQ_ERR_INVAL;
    }

    while(str[0]) {
        if(str[0] == '+') {
            if((c != '\0' && c != '/') || (str[1] != '\0' && str[1] != '/')) {
                return MOSQ_ERR_INVAL;
            }
        } else if(str[0] == '#') {
            if((c != '\0' && c != '/')  || str[1] != '\0') {
                return MOSQ_ERR_INVAL;
            }
        } else if(str[0] == '/') {
            hier_count++;
        }
        len++;
        c = str[0];
        str = &str[1];
    }
    if(len > 65535) {
        return MOSQ_ERR_INVAL;
    }
    if(hier_count > TOPIC_HIERARCHY_LIMIT) {
        return MOSQ_ERR_INVAL;
    }

    return MOSQ_ERR_SUCCESS;
}

/***************************************************************************
 *  Publishing: get subscribers
 ***************************************************************************/
PRIVATE json_t *sub_get_subscribers(
    hgobj gobj,
    const char *topic_name
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *jn_subscribers = json_object();

    /*
     *  Search subscriptions in clients
     */
    json_t *jn_clients = gobj_list_resource(priv->gobj_mqtt_clients, "", 0, 0);

    int idx; json_t *client;
    json_array_foreach(jn_clients, idx, client) {
        json_t *jn_subscriptions = kw_get_dict(gobj, client, "subscriptions", 0, KW_REQUIRED);
        if(json_object_size(jn_subscriptions)==0) {
            continue;
        }
        BOOL isConnected = kw_get_bool(gobj, client, "isConnected", 0, KW_REQUIRED);
        const char *client_id = kw_get_str(gobj, client, "id", "", KW_REQUIRED);
        const char *topic_name_; json_t *subscription;
        json_object_foreach(jn_subscriptions, topic_name_, subscription) {
            int qos = kw_get_int(gobj, subscription, "qos", 0, KW_REQUIRED);
            if(isConnected || (!isConnected && qos > 0)) {
                if(strcmp(topic_name, topic_name_)==0) { // TODO change strcmp() by match_topic()
                    json_t *client_with_subscriptions = kw_get_dict(gobj,
                        jn_subscribers, client_id, json_object(), KW_CREATE
                    );
                    json_t *subscriptions = kw_get_dict(gobj,
                        client_with_subscriptions, "subscriptions", json_object(), KW_CREATE
                    );
                    json_object_set(subscriptions, topic_name, subscription);
                }
            }
        }
    }
    JSON_DECREF(jn_clients);
    return jn_subscribers;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE struct mosquitto_msg_store *db_message_store_find(hgobj gobj, int mid)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    struct mosquitto_client_msg *tail = dl_first(&priv->dl_msgs_in);
    while(tail) {
        if(tail->store->source_mid == mid) {
            return tail->store;
        }
        /*
         *  Next
         */
        tail = dl_next(tail);
    }

    return 0;
}

/***************************************************************************
 *  Entrega mensajes con qos 2
 ***************************************************************************/
PRIVATE int db__message_release_incoming(hgobj gobj, uint16_t mid)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char *topic;
    BOOL deleted = FALSE;

    struct mosquitto_client_msg *tail = dl_first(&priv->dl_msgs_in);
    while(tail) {
        if(tail->mid == mid) {
            if(tail->store->qos != 2) {
                return MOSQ_ERR_PROTOCOL;
            }
            topic = tail->store->topic;

            /* topic==NULL should be a QoS 2 message that was
             * denied/dropped and is being processed so the client doesn't
             * keep resending it. That means we don't send it to other
             * clients. */
            if(topic == NULL) {
                dl_delete(&priv->dl_msgs_in, tail, db_free_client_msg);
                deleted = TRUE;
            } else {
                struct mosquitto_msg_store *stored = tail->store;
                json_t *jn_subscribers = sub_get_subscribers(gobj, stored->topic);
                XXX_sub__messages_queue(
                    gobj,
                    jn_subscribers,
                    stored->topic,
                    2,
                    stored->retain,
                    stored
                );

                dl_delete(&priv->dl_msgs_in, tail, db_free_client_msg);
                deleted = TRUE;
            }
            break; // TODO salgo? sino salgo asegura el bucle
        }

        /*
         *  Next
         */
        tail = dl_next(tail);
    }

    if(deleted) {
        return MOSQ_ERR_SUCCESS;
    } else {
        return MOSQ_ERR_NOT_FOUND;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int db__message_remove_incoming(hgobj gobj, int mid)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    struct mosquitto_client_msg *tail = dl_first(&priv->dl_msgs_in);
    while(tail) {
        if(tail->mid == mid) {
            if(tail->qos != 2) {
                return MOSQ_ERR_PROTOCOL;
            }
            dl_delete(&priv->dl_msgs_in, tail, db_free_client_msg);
            return MOSQ_ERR_SUCCESS;
        }
        /*
         *  Next
         */
        tail = dl_next(tail);
    }

    return MOSQ_ERR_NOT_FOUND;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int db__message_update_outgoing(
    hgobj gobj,
    uint16_t mid,
    enum mosquitto_msg_state state,
    int qos
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    struct mosquitto_client_msg *msg = dl_first(&priv->dl_msgs_out);
    while(msg) {
        if(msg->mid == mid) {
            if(msg->qos != qos) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "msg qos not match",
                    "client_id",    "%s", SAFE_PRINT(priv->client_id),
                    "mid",          "%d", (int)mid,
                    "msg qos",      "%d", (int)msg->qos,
                    "qos",          "%d", (int)qos,
                    NULL
                );
                return MOSQ_ERR_PROTOCOL;
            }
            msg->state = state;
            msg->timestamp = time_in_seconds();
            return MOSQ_ERR_SUCCESS;
        }
        /*
         *  Next
         */
        msg = dl_next(msg);
    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "msg not found",
        "client_id",    "%s", SAFE_PRINT(priv->client_id),
        "mid",          "%d", (int)mid,
        NULL
    );

    return MOSQ_ERR_NOT_FOUND;
}

/***************************************************************************
 * Is this context ready to take more in flight messages right now?
 * @param context the client context of interest
 * @param qos qos for the packet of interest
 * @return TRUE if more in flight are allowed.
 ***************************************************************************/
PRIVATE BOOL db__ready_for_flight(hgobj gobj, enum mosquitto_msg_direction dir, int qos)
{
    return TRUE;
}

/***************************************************************************
 *  This function requires topic to be allocated on the heap.
 *  Once called, it owns topic and will free it on error.
 *  Likewise payload and properties
 ***************************************************************************/
PRIVATE int db__message_store(
    hgobj gobj,
    struct mosquitto_msg_store *stored,
    uint32_t message_expiry_interval
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBMEM_FREE(stored->source_id);
    if(priv->client_id) {
        stored->source_id = gbmem_strdup(priv->client_id);
    } else {
        stored->source_id = gbmem_strdup("");
    }
    if(!stored->source_id) {
        // Error already logged
        GBMEM_FREE(stored);
        return MOSQ_ERR_NOMEM;
    }

    GBMEM_FREE(stored->source_username);
    if(priv->username) {
        stored->source_username = gbmem_strdup(priv->username);
        if(!stored->source_username) {
            // Error already logged
            GBMEM_FREE(stored);
            return MOSQ_ERR_NOMEM;
        }
    }
    stored->mid = 0;
    if(message_expiry_interval > 0) {
        stored->message_expiry_time = time_in_seconds() + message_expiry_interval; // TODO is in seconds?
    } else {
        stored->message_expiry_time = 0;
    }

    //db.msg_store_count++;
    //db.msg_store_bytes += stored->payloadlen;
    //db__msg_store_add(gobj, stored);

    return MOSQ_ERR_SUCCESS;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE struct mosquitto_msg_store *db_duplicate_msg(
    hgobj gobj,
    struct mosquitto_msg_store *stored
)
{
    struct mosquitto_msg_store *store_dup = GBMEM_MALLOC(sizeof(struct mosquitto_msg_store));

    store_dup->topic = gbmem_strdup(stored->topic);
    store_dup->payload = gbmem_malloc(stored->payloadlen);
    memcpy(store_dup->payload, stored->payload, stored->payloadlen);
    store_dup->payloadlen = stored->payloadlen;
    store_dup->mid = stored->mid;
    store_dup->qos = stored->qos;
    store_dup->retain = stored->retain;

    store_dup->message_expiry_time = stored->message_expiry_time;
    store_dup->source_id = gbmem_strdup(stored->source_id);
    store_dup->source_username = gbmem_strdup(stored->source_username);
    // TODO int ref_count; usa referencias
    store_dup->source_mid = stored->source_mid;
    store_dup->properties = json_incref(stored->properties);

    return store_dup;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void db_free_client_msg(void *client_msg_)
{
    struct mosquitto_client_msg *client_msg = client_msg_;

    JSON_DECREF(client_msg->properties);
    db_free_msg_store(client_msg->store);
    GBMEM_FREE(client_msg);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void db_free_msg_store(void *store_)
{
    struct mosquitto_msg_store *store = store_;

    GBMEM_FREE(store->source_id);
    GBMEM_FREE(store->source_username);
    GBMEM_FREE(store->topic);
    JSON_DECREF(store->properties);
    GBMEM_FREE(store->payload);
    GBMEM_FREE(store);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int db__message_delete_outgoing(
    hgobj gobj,
    uint16_t mid,
    enum mosquitto_msg_state expect_state,
    int qos
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    struct mosquitto_client_msg *tail;

    tail = dl_first(&priv->dl_msgs_out);
    while(tail) {
        if(tail->mid == mid) {
            if(tail->qos != qos) {
                return MOSQ_ERR_PROTOCOL;
            } else if(qos == 2 && tail->state != expect_state) {
                return MOSQ_ERR_PROTOCOL;
            }
            dl_delete(&priv->dl_msgs_out, tail, db_free_client_msg);
            break;
        }

        /*
         *  Next
         */
        tail = dl_next(tail);
    }
    return 0;
}

/***************************************************************************
 *  Publishing: send the message to subscriber
 ***************************************************************************/
PRIVATE int XXX_db__message_insert(
    hgobj gobj,
    uint16_t mid,
    uint8_t qos,
    BOOL retain,
    struct mosquitto_msg_store *stored,
    json_t *properties // not owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int rc = 0;

    //printf("================> OUT Client %s\n", priv->client_id);
    //print_queue("dl_msgs_out", &priv->dl_msgs_out);

    uint32_t expiry_interval = 0;

    if(stored->message_expiry_time) {
        if(time_in_seconds() > stored->message_expiry_time) {
            /* Message is expired, must not send. */
            // TODO db__message_remove(&context->msgs_out, msg);
            return MOSQ_ERR_SUCCESS;
        } else {
            expiry_interval = (uint32_t)(stored->message_expiry_time - time_in_seconds());
        }
    }

    struct mosquitto_client_msg *msg;
    enum mosquitto_msg_state state = mosq_ms_invalid;

    switch(qos) {
        case 0:
            state = mosq_ms_publish_qos0;
            break;
        case 1:
            state = mosq_ms_publish_qos1;
            break;
        case 2:
            state = mosq_ms_publish_qos2;
            break;
    }

    msg = GBMEM_MALLOC(sizeof(struct mosquitto_client_msg));
    if(!msg) {
        // Error already logged
        return MOSQ_ERR_NOMEM;
    }

    msg->store = db_duplicate_msg(gobj, stored);
    msg->mid = mid;
    msg->timestamp = time_in_seconds();
    msg->direction = mosq_md_out;
    msg->state = state;
    msg->dup = FALSE;
    if(qos > priv->max_qos) {
        msg->qos = priv->max_qos;
    } else {
        msg->qos = qos;
    }
    msg->retain = retain;
    msg->properties = json_incref(properties);

    dl_insert(&priv->dl_msgs_out, msg);

    switch(msg->state) {
        case mosq_ms_publish_qos0:
            rc = send_publish(
                gobj,
                mid,
                stored->topic,
                stored->payloadlen,
                stored->payload,
                qos,
                retain,
                msg->dup,
                properties,
                stored->properties,
                expiry_interval
            );
            dl_delete(&priv->dl_msgs_out, msg, db_free_client_msg);
            break;

        case mosq_ms_publish_qos1:
            rc = send_publish(
                gobj,
                mid,
                stored->topic,
                stored->payloadlen,
                stored->payload,
                qos,
                retain,
                msg->dup,
                properties,
                stored->properties,
                expiry_interval
            );
            if(rc == MOSQ_ERR_SUCCESS) {
                msg->timestamp = time_in_seconds();
                msg->dup = 1; /* Any retry attempts are a duplicate. */
                msg->state = mosq_ms_wait_for_puback;
            } else if(rc == MOSQ_ERR_OVERSIZE_PACKET) {
                dl_delete(&priv->dl_msgs_out, msg, db_free_client_msg);
            }
            break;

        case mosq_ms_publish_qos2:
            rc = send_publish(
                gobj,
                mid,
                stored->topic,
                stored->payloadlen,
                stored->payload,
                qos,
                retain,
                msg->dup,
                properties,
                stored->properties,
                expiry_interval
            );
            if(rc == MOSQ_ERR_SUCCESS) {
                msg->timestamp = time_in_seconds();
                msg->dup = 1; /* Any retry attempts are a duplicate. */
                msg->state = mosq_ms_wait_for_pubrec;
            } else if(rc == MOSQ_ERR_OVERSIZE_PACKET) {
                dl_delete(&priv->dl_msgs_out, msg, db_free_client_msg);
            }
            break;

        case mosq_ms_resend_pubrel:
            send__pubrel(gobj, mid, NULL);
            msg->state = mosq_ms_wait_for_pubcomp;
            break;

        case mosq_ms_invalid:
        case mosq_ms_send_pubrec:
        case mosq_ms_resend_pubcomp:
        case mosq_ms_wait_for_puback:
        case mosq_ms_wait_for_pubrec:
        case mosq_ms_wait_for_pubrel:
        case mosq_ms_wait_for_pubcomp:
        case mosq_ms_queued:
            break;
    }

    return rc;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int XXX_save_message_to_pubrec(
    hgobj gobj,
    uint16_t mid,
    uint8_t qos,
    BOOL retain,
    struct mosquitto_msg_store *stored,
    json_t *properties
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int rc = 0;

    //printf("================> IN Client %s\n", priv->client_id);
    //print_queue("dl_msgs_in", &priv->dl_msgs_in);

    struct mosquitto_client_msg *msg;
    enum mosquitto_msg_state state = mosq_ms_invalid;
//     int i;
//     char **dest_ids;

    if(qos == 2) {
        state = mosq_ms_wait_for_pubrel;
    } else {
        JSON_DECREF(properties)
        return 1;
    }

    msg = GBMEM_MALLOC(sizeof(struct mosquitto_client_msg));
    if(!msg) {
        // Error already logged
        return MOSQ_ERR_NOMEM;
    }

    msg->store = db_duplicate_msg(gobj, stored);
    msg->mid = mid;
    msg->timestamp = time_in_seconds();
    msg->direction = mosq_md_in;
    msg->state = state;
    msg->dup = FALSE;
    if(qos > priv->max_qos) {
        msg->qos = priv->max_qos;
    } else {
        msg->qos = qos;
    }
    msg->retain = retain;
    msg->properties = properties;

    dl_insert(&priv->dl_msgs_in, msg);

    return rc;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE uint16_t mosquitto__mid_generate(hgobj gobj, const char *client_id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *client = gobj_get_resource(priv->gobj_mqtt_clients, client_id, 0, 0);
    uint16_t last_mid = (uint16_t)kw_get_int(gobj, client, "last_mid", 0, KW_REQUIRED);

    // TODO it seems that does nothing saving in resource
    last_mid++;
    if(last_mid == 0) {
        last_mid++;
    }
    gobj_save_resource(priv->gobj_mqtt_clients, client_id, client, 0);

    return last_mid;
}

/***************************************************************************
 *  Publishing: send the message to subscriber
 ***************************************************************************/
PRIVATE int XXX_subs__send(
    hgobj gobj,
    const char *client_id,
    const char *topic_name, // used in mosquitto_acl_check()
    json_t *subscription,
    uint8_t qos,
    int retain,
    struct mosquitto_msg_store *stored
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *properties = json_object();
    int rc2;

    /* Check for ACL topic access. */
    rc2 = MOSQ_ERR_SUCCESS; // rc2 = mosquitto_acl_check()
    if(rc2 == MOSQ_ERR_ACL_DENIED) {
        JSON_DECREF(properties)
        return MOSQ_ERR_SUCCESS;
    }

    uint8_t msg_qos;
    uint8_t client_qos = kw_get_int(gobj, subscription, "qos", 0, KW_REQUIRED);
    if(qos > client_qos) {
        msg_qos = client_qos;
    } else {
        msg_qos = qos;
    }

    uint16_t mid;
    if(msg_qos) {
        mid = mosquitto__mid_generate(gobj, client_id);
    } else {
        mid = 0;
    }

    BOOL client_retain;
    if(kw_get_bool(gobj, subscription, "retain_as_published", 0, KW_REQUIRED)) {
        client_retain = retain;
    } else {
        client_retain = FALSE;
    }
    int identifier = kw_get_int(gobj, subscription, "identifier", -1, KW_REQUIRED);
    if(identifier > 0) {
        mosquitto_property_add_varint(gobj, properties, MQTT_PROP_SUBSCRIPTION_IDENTIFIER, identifier);
    }

    json_t *client = gobj_get_resource(priv->gobj_mqtt_clients, client_id, 0, 0);
    BOOL isConnected = kw_get_bool(gobj, client, "isConnected", 0, KW_REQUIRED);
    if(isConnected) {
        hgobj gobj_client = (hgobj)(size_t)kw_get_int(gobj, client, "_gobj", 0, KW_REQUIRED);
        if(gobj_client) {
            XXX_db__message_insert(gobj_client, mid, msg_qos, client_retain, stored, properties);
        }
    } else {
        // TODO save the message if qos > 0 ?
        // gobj_save_resource(priv->gobj_mqtt_clients, client_id, client, 0);
    }

    JSON_DECREF(properties)
    return 0;
}

/***************************************************************************
 *  Publishing: send the message to subscribers
 ***************************************************************************/
PRIVATE int XXX_sub__messages_queue(
    hgobj gobj,
    json_t *jn_subscribers,
    const char *topic_name,
    uint8_t qos,
    int retain,
    struct mosquitto_msg_store *stored
)
{
    /*
     *  Search subscriptions in clients
     */
    const char *client_id; json_t *client;
    json_object_foreach(jn_subscribers, client_id, client) {
        json_t *jn_subscriptions = kw_get_dict(gobj, client, "subscriptions", 0, KW_REQUIRED);
        if(json_object_size(jn_subscriptions)==0) {
            continue;
        }
        const char *topic_name_; json_t *subscription;
        json_object_foreach(jn_subscriptions, topic_name_, subscription) {
            XXX_subs__send(gobj, client_id, topic_name, subscription, qos, retain, stored);
        }
    }

    if(retain) {
        // TODO int rc2 = retain__store(topic, *stored, split_topics);
        //if(rc2) {
        //    rc = rc2;
        //}
    }
    JSON_DECREF(jn_subscribers);

    gbuffer_t *gbuf_message = gbuffer_create(stored->payloadlen, stored->payloadlen);
    if(gbuf_message) {
        if(stored->payloadlen > 0) {
            // Can become without payload
            gbuffer_append(gbuf_message, stored->payload, stored->payloadlen);
        }
        json_t *kw = json_pack("{s:s, s:s, s:I}",
            "mqtt_action", "publishing",
            "topic", topic_name,
            "gbuffer", (json_int_t)(uintptr_t)gbuf_message
        );
        gobj_publish_event(gobj, EV_ON_MESSAGE, kw);
    }

    return MOSQ_ERR_SUCCESS;
}

/***************************************************************************
 *  Add a subscription, return MOSQ_ERR_SUB_EXISTS or MOSQ_ERR_SUCCESS
 ***************************************************************************/
PRIVATE int add_subscription(
    hgobj gobj,
    const char *sub, // topic? TODO change name
    uint8_t qos,
    json_int_t identifier,
    int options
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    // "$share" TODO shared not implemented

    int rc = MOSQ_ERR_SUCCESS;
    BOOL no_local = ((options & MQTT_SUB_OPT_NO_LOCAL) != 0);
    BOOL retain_as_published = ((options & MQTT_SUB_OPT_RETAIN_AS_PUBLISHED) != 0);

    /*
     *  Find client
     */
    json_t *client  = gobj_get_resource(priv->gobj_mqtt_clients, priv->client_id, 0, 0);
    if(!client) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "client not found",
            "client_id",    "%s", SAFE_PRINT(priv->client_id),
            NULL
        );
        return -1;
    }

    /*
     *  Get subscriptions
     */
    json_t *subscriptions = kw_get_dict(gobj, client, "subscriptions", 0, KW_REQUIRED);
    if(!subscriptions) {
        // Error already logged
        return -1;
    }

    json_t *subscription_record = kw_get_dict(gobj, subscriptions, sub, 0, 0);
    if(subscription_record) {
        /*
         *  Client making a second subscription to same topic.
         *  Only need to update QoS and identifier (TODO sure?)
         *  Return MOSQ_ERR_SUB_EXISTS to indicate this to the calling function.
         */
        rc = MOSQ_ERR_SUB_EXISTS;
        if(gobj_trace_level(gobj) & SHOW_DECODE) {
            trace_msg0("  👈 🔴 subscription already exists: client '%s', topic '%s'",
                priv->client_id,
                sub
            );
        } else {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "subscription already exists",
                "client_id",    "%s", SAFE_PRINT(priv->client_id),
                "sub",          "%s", sub,
                NULL
            );
        }

        json_t *kw_subscription = json_pack("{s:i, s:I}",
            "qos", (int)qos,
            "identifier", (json_int_t)identifier
        );
        if(!kw_subscription) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "json_pack() FAILED",
                NULL
            );
            return MOSQ_ERR_NOMEM;
        }
        json_object_update_new(subscription_record, kw_subscription);

    } else {
        /*
         *  New subscription
         */
        subscription_record = json_pack("{s:s, s:i, s:I, s:b, s:b}",
            "id", sub,
            "qos", (int)qos,
            "identifier", (json_int_t)identifier,
            "no_local", no_local,
            "retain_as_published", retain_as_published
        );
        if(!subscription_record) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "json_pack() FAILED",
                NULL
            );
            return MOSQ_ERR_NOMEM;
        }
        if(gobj_trace_level(gobj) & SHOW_DECODE) {
            gobj_trace_json(gobj, subscription_record, "new subscription");
        }
        json_object_set_new(subscriptions, sub, subscription_record);
    }

    // TODO don't save if qos == 0
    // ??? gobj_save_resource(priv->gobj_mqtt_topics, sub, subscription_record, 0);
    return rc;
}

/***************************************************************************
 *  Remove a subscription
 ***************************************************************************/
PRIVATE int remove_subscription(
    hgobj gobj,
    const char *sub,// topic? TODO change name
    uint8_t *reason
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    // "$share" TODO shared not implemented

    *reason = 0;

    /*
     *  Find client
     */
    json_t *client  = gobj_get_resource(priv->gobj_mqtt_clients, priv->client_id, 0, 0);
    if(!client) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "client not found",
            "client_id",    "%s", SAFE_PRINT(priv->client_id),
            NULL
        );
        return -1;
    }

    /*
     *  Get subscriptions
     */
    json_t *subscriptions = kw_get_dict(gobj, client, "subscriptions", 0, KW_REQUIRED);
    if(!subscriptions) {
        // Error already logged
        return -1;
    }

    json_t *subs = kw_get_dict(gobj, subscriptions, sub, 0, KW_EXTRACT);
    if(!subs) {
        *reason = MQTT_RC_NO_SUBSCRIPTION_EXISTED;
    }
    JSON_DECREF(subs);

    return 0;
}

/***************************************************************************
 *  Remove all subscriptions for a client.
 ***************************************************************************/
PRIVATE int sub__clean_session(hgobj gobj, json_t *client)
{
    /*
     *  Reset subscriptions
     */
    json_object_set_new(client, "subscriptions", json_object());

    return 0;
}

/***************************************************************************
 *  Subscription: search if the topic has a retain message and process
 ***************************************************************************/
PRIVATE int retain__queue(
    hgobj gobj,
    const char *sub,
    uint8_t sub_qos,
    uint32_t subscription_identifier
)
{
    if(strncmp(sub, "$share/", strlen("$share/"))==0) {
        return MOSQ_ERR_SUCCESS;
    }
    // TODO
    return MOSQ_ERR_SUCCESS;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int will_read(
    hgobj gobj,
    gbuffer_t *gbuf
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;
    if(priv->protocol_version == PROTOCOL_VERSION_v5) {
        json_t *properties = property_read_all(gobj, gbuf, CMD_WILL, &ret);
        if(!properties) {
            return ret;
        }
        if(property_process_will(gobj, properties)<0) {
            // Error already logged
            JSON_DECREF(properties)
            return -1;
        };
        JSON_DECREF(properties)
    }
    char *will_topic; uint16_t tlen;
    if((ret=mqtt_read_string(gobj, gbuf, &will_topic, &tlen))<0) {
        // Error already logged
        return ret;
    }
    if(!tlen) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt will: not topic",
            NULL
        );
        return MOSQ_ERR_PROTOCOL;
    }
    gobj_write_strn_attr(gobj, "will_topic", will_topic, tlen);

    if((ret=mosquitto_pub_topic_check(will_topic))<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt will: invalid topic",
            "topic",        "%s", will_topic,
            NULL
        );
        return ret;
    }

    uint16_t payloadlen;
    if((ret=mqtt_read_uint16(gobj, gbuf, &payloadlen))<0) {
        // Error already logged
        return ret;
    }
    if(payloadlen > 0) {
        GBUFFER_DECREF(priv->gbuf_will_payload);
        priv->gbuf_will_payload = gbuffer_create(payloadlen, payloadlen);
        if(!priv->gbuf_will_payload) {
            // Error already logged
            return MOSQ_ERR_NOMEM;
        }
        uint8_t *p = gbuffer_cur_rd_pointer(priv->gbuf_will_payload);
        if((ret=mqtt_read_bytes(gobj, gbuf, p, (uint32_t)payloadlen))<0) {
            // Error already logged
            return ret;
        }
        gbuffer_set_wr(priv->gbuf_will_payload, payloadlen);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int save_topic_alias(hgobj gobj, int topic_alias, const char *topic)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char tmp_alias[64];
    snprintf(tmp_alias, sizeof(tmp_alias), "%d", topic_alias);

    json_object_set_new(priv->jn_alias_list, tmp_alias, json_string(topic));
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char *find_alias_topic(hgobj gobj, uint16_t alias)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char tmp_alias[64];
    snprintf(tmp_alias, sizeof(tmp_alias), "%d", alias);

    const char *topic_name = kw_get_str(gobj, priv->jn_alias_list, tmp_alias, 0, 0);
    if(!topic_name) {
        return 0;
    }
    return gbmem_strdup(topic_name);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__auth_s(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    if(priv->protocol_version != mosq_p_mqtt5) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt auth: not mqtt5",
            NULL
        );
        return -1;
    }

    if(!priv->iamServer) {
        /*-----------------------------------*
         *      Client, no procede no?
         *-----------------------------------*/
        uint8_t reason_code;
        if(mqtt_read_byte(gobj, gbuf, &reason_code)<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt auth: malformed packet",
                NULL
            );
            return -1;
        }
        json_decref(property_read_all(gobj, gbuf, CMD_AUTH, &ret));
        return ret;
    }

    /*-----------------------------------*
     *      Server
     *-----------------------------------*/
    if(empty_string(priv->auth_method)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt auth: not auth method",
            NULL
        );
        return -1;
    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_MQTT_ERROR,
        "msg",          "%s", "Mqtt auth: auth command not supported",
        NULL
    );
    return -1;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__auth_c(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    // TODO

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
// copia client session TODO
//     if(found_context->msgs_in.inflight || found_context->msgs_in.queued
//             || found_context->msgs_out.inflight || found_context->msgs_out.queued) {
//
//         in_quota = context->msgs_in.inflight_quota;
//         out_quota = context->msgs_out_inflight_quota;
//         in_maximum = context->msgs_in.inflight_maximum;
//         out_maximum = context->msgs_out_inflight_maximum;
//
//         memcpy(&context->msgs_in, &found_context->msgs_in, sizeof(struct mosquitto_msg_data));
//         memcpy(&context->msgs_out, &found_context->msgs_out, sizeof(struct mosquitto_msg_data));
//
//         memset(&found_context->msgs_in, 0, sizeof(struct mosquitto_msg_data));
//         memset(&found_context->msgs_out, 0, sizeof(struct mosquitto_msg_data));
//
//         context->msgs_in.inflight_quota = in_quota;
//         context->msgs_out_inflight_quota = out_quota;
//         context->msgs_in.inflight_maximum = in_maximum;
//         context->msgs_out_inflight_maximum = out_maximum;
//
//         db__message_reconnect_reset(context);
//     }
//     context->subs = found_context->subs;
//     found_context->subs = NULL;
//     context->sub_count = found_context->sub_count;
//     found_context->sub_count = 0;
//     context->last_mid = found_context->last_mid;
//
//     for(i=0; i<context->sub_count; i++) {
//         if(context->subs[i]) {
//             leaf = context->subs[i]->hier->subs;
//             while(leaf) {
//                 if(leaf->context == found_context) {
//                     leaf->context = context;
//                 }
//                 leaf = leaf->next;
//             }
//
//             if(context->subs[i]->shared) {
//                 leaf = context->subs[i]->shared->subs;
//                 while(leaf) {
//                     if(leaf->context == found_context) {
//                         leaf->context = context;
//                     }
//                     leaf = leaf->next;
//                 }
//             }
//         }
//     }



/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int save_client(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->assigned_id && !empty_string(priv->client_id)) {
        gobj_save_resource(priv->gobj_mqtt_clients, priv->client_id, priv->client, 0);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_client_disconnected(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(!empty_json(priv->client)) {
        kw_set_dict_value(gobj, priv->client, "isConnected", json_false());
        kw_set_dict_value(gobj, priv->client, "_gobj", json_integer(0));
        kw_set_dict_value(gobj, priv->client, "_gobj_bottom", json_integer(0));
        save_client(gobj);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int connect_on_authorised(
    hgobj gobj
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint8_t connect_ack = 0;
    json_t *connack_props = json_object();

    /*--------------------------------------------------------------*
     *  Find if this client already has an entry.
     *  With assigned_id the id is random!, not a persistent id
     *  (HACK client_id is really a device_id)
     *--------------------------------------------------------------*/
    json_t *client = 0;
    if(priv->assigned_id) {
        json_t *jn_options = json_pack("{s:b}", "volatil", 1);
        client = gobj_create_resource(priv->gobj_mqtt_clients, priv->client_id, 0, jn_options);
        if(!client) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "Mqtt auth: cannot create client",
                NULL
            );
            JSON_DECREF(connack_props);
            return -1;
        }
        kw_set_dict_value(gobj, client, "id", json_string(priv->client_id));
        kw_set_dict_value(gobj, client, "assigned_id", json_true());
        kw_set_dict_value(gobj, client, "subscriptions", json_object());
    } else {
        client = gobj_get_resource(priv->gobj_mqtt_clients, priv->client_id, 0, 0); // NOT YOURS
        if(!client) {
            // New client (device)
            json_t *kw_client = json_pack("{s:s, s:b, s:i, s:{}}",
                "id", priv->client_id,
                "assigned_id", 0,
                "last_mid", 0,
                "subscriptions"
            );
            client = gobj_create_resource(priv->gobj_mqtt_clients, priv->client_id, kw_client, 0);
            if(!client) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "Mqtt auth: cannot create client",
                    NULL
                );
                JSON_DECREF(connack_props);
                return -1;
            }
        }
    }
    if(!client) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Mqtt auth: cannot create client",
            NULL
        );
        JSON_DECREF(connack_props);
        return -1;
    }

    /*
     *  Check if duplicate (device already connected)
     */
    BOOL isConnected = kw_get_bool(gobj, client, "isConnected", 0, KW_CREATE);

    uint32_t prev_session_expiry_interval = kw_get_int(gobj,
        client, "session_expiry_interval", 0, KW_CREATE
    );
    uint32_t prev_protocol_version = kw_get_int(gobj,
        client, "protocol_version", 0, KW_CREATE
    );
    BOOL prev_clean_start = kw_get_bool(gobj,
        client, "clean_start", 0, KW_CREATE
    );
    if(priv->clean_start == FALSE && prev_session_expiry_interval > 0) {
        if(priv->protocol_version == mosq_p_mqtt311 || priv->protocol_version == mosq_p_mqtt5) {
            connect_ack |= 0x01;
        }
        // copia client session TODO
    }

    if(priv->clean_start == TRUE) {
        sub__clean_session(gobj, client);
    }
    if((prev_protocol_version == mosq_p_mqtt5 && prev_session_expiry_interval == 0)
            || (prev_protocol_version != mosq_p_mqtt5 && prev_clean_start == TRUE)
            || (priv->clean_start == TRUE)
            ) {
        // TODO context__send_will(found_context);
    }

    // TODO session_expiry__remove(found_context);
    // TODO will_delay__remove(found_context);
    // TODO will__clear(found_context);

    //found_context->clean_start = TRUE;
    //found_context->session_expiry_interval = 0;
    //mosquitto__set_state(found_context, mosq_cs_duplicate);

    if(isConnected) {
        hgobj gobj_bottom = (hgobj)(size_t)kw_get_int(gobj, client, "_gobj_bottom", 0, KW_REQUIRED);
        if(gobj_bottom) {
            gobj_send_event(gobj_bottom, EV_DROP, 0, gobj);
        }
    }

    /*-----------------------------*
     *  Trace connection
     *-----------------------------*/
    if(gobj_trace_level(gobj) & TRACE_CONNECT_DISCONNECT) {
        if(priv->is_bridge) {
            gobj_log_info(gobj, 0,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "Mqtt: New BRIDGE connected",
                "client_id",    "%s", priv->client_id,
                "protocol",     "%d", (int)priv->protocol_version, // TODO pon nombre
                "clean_start",  "%d", (int)priv->clean_start,
                "keepalive",    "%d", (int)priv->keepalive,
                "username",     "%s", priv->username?priv->username:"",
                NULL
            );
        } else {
            gobj_log_info(gobj, 0,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "Mqtt: New CLIENT connected",
                "client_id",    "%s", priv->client_id,
                "username",     "%s", priv->username?priv->username:"",
                "protocol",     "%d", (int)priv->protocol_version, // TODO pon nombre
                "clean_start",  "%d", (int)priv->clean_start,
                "keepalive",    "%d", (int)priv->keepalive,
                NULL
            );
        }

        if(priv->will) {
            gobj_log_info(gobj, 0,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "Mqtt: Will",
                "client_id",    "%s", priv->client_id,
                "username",     "%s", priv->username?priv->username:"",
                "topic",        "%s", "", // TODO will->msg.topic
                "will payload", "%ld", (long)0, // TODO (long)will->msg.payloadlen,
                "will_retain",  "%d", priv->will_retain,
                "will_qos",     "%d", priv->will_qos,
                NULL
            );
        }
    }

//     kw_set_dict_value(gobj, client, "ping_t", json_integer(0));
//     kw_set_dict_value(gobj, client, "is_dropping", json_false());

    /*-----------------------------*
     *  Check acl acl__find_acls
     *-----------------------------*/
    //connection_check_acl(context, &context->msgs_in.inflight);
    //connection_check_acl(context, &context->msgs_in.queued);
    //connection_check_acl(context, &context->msgs_out.inflight);
    //connection_check_acl(context, &context->msgs_out.queued);

    //context__add_to_by_id(context); TODO

    kw_set_dict_value(gobj, client, "max_qos", json_integer(priv->max_qos));
    if(priv->max_keepalive &&
            (priv->keepalive > priv->max_keepalive || priv->keepalive == 0)) {

        kw_set_dict_value(gobj, client, "keepalive", json_integer(priv->max_keepalive));
        if(priv->protocol_version == mosq_p_mqtt5) {
            mqtt_property_add_int16(gobj, connack_props, MQTT_PROP_SERVER_KEEP_ALIVE, priv->keepalive);
        } else {
            send_connack(gobj, connect_ack, CONNACK_REFUSED_IDENTIFIER_REJECTED, NULL);
            JSON_DECREF(connack_props);
            return -1;
        }
    }

    if(priv->protocol_version == mosq_p_mqtt5) {
        if(priv->max_topic_alias > 0) {
            if(mqtt_property_add_int16 (
                gobj, connack_props, MQTT_PROP_TOPIC_ALIAS_MAXIMUM, priv->max_topic_alias)<0)
            {
                // Error already logged
                JSON_DECREF(connack_props);
                return -1;
            }
        }
        if(priv->assigned_id) {
            if(mqtt_property_add_string(
                gobj, connack_props, MQTT_PROP_ASSIGNED_CLIENT_IDENTIFIER, priv->client_id)<0)
            {
                // Error already logged
                JSON_DECREF(connack_props);
                return -1;
            }
        }
        if(priv->auth_method) {
            // No tenemos auth method
        }
    }

    //mosquitto__set_state(context, mosq_cs_active);

    int ret = send_connack(gobj, connect_ack, CONNACK_ACCEPTED, connack_props);
    if(ret == 0) {
        kw_set_dict_value(gobj, client, "isConnected", json_true());
        kw_set_dict_value(gobj,
            client, "_gobj", json_integer((json_int_t)(uintptr_t)(gobj))
        );
        kw_set_dict_value(gobj,
            client, "_gobj_bottom", json_integer((json_int_t)(uintptr_t)gobj_bottom_gobj(gobj))
        );
        gobj_write_bool_attr(gobj, "in_session", TRUE);
        gobj_write_json_attr(gobj, "client", client);
        gobj_write_bool_attr(gobj, "send_disconnect", TRUE);
        priv->must_broadcast_on_close = TRUE;
        priv->client = client;
        save_client(gobj);

        json_t *kw = json_pack("{s:s}",
            "client_id", priv->client_id
        );
        gobj_publish_event(gobj, EV_ON_OPEN, kw);

        // db__message_write_queued_out(context); TODO
        //db__message_write_inflight_out_all(context); TODO
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__connect(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->in_session) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt CMD_CONNECT: already in session",
            NULL
        );
        return -1;
    }

    if(!priv->iamServer) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt CMD_CONNECT: i am not server",
            NULL
        );
        return -1;
    }

    /*-------------------------------------------*
     *      Protocol name and version
     *-------------------------------------------*/
    char protocol_name[7];
    mosquitto_protocol_t protocol_version;
    BOOL is_bridge = FALSE;
    uint8_t version_byte;

    uint16_t ll;
    if(mqtt_read_uint16(gobj, gbuf, &ll)<0) {
        // Error already logged
        return -1;
    }
    if(ll != 4 /* MQTT */ && ll != 6 /* MQIsdp */) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt CMD_CONNECT: MQTT bad length",
            NULL
        );
        return -1;
    }

    if(mqtt_read_bytes(gobj, gbuf, protocol_name, ll) < 0) {
        // Error already logged
        return -1;
    }
    protocol_name[ll] = '\0';

    if(mqtt_read_byte(gobj, gbuf, &version_byte)<0) {
        // Error already logged
        return -1;
    }
    if(strcmp(protocol_name, PROTOCOL_NAME_v31)==0) {
        if((version_byte & 0x7F) != PROTOCOL_VERSION_v31) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt Invalid protocol version",
                "version",      "%d", (int)version_byte,
                NULL
            );
            send_connack(gobj, 0, CONNACK_REFUSED_PROTOCOL_VERSION, NULL);
            return -1;
        }
        protocol_version = mosq_p_mqtt31;
        if((version_byte & 0x80) == 0x80) {
            is_bridge = TRUE;
        }
    } else if(strcmp(protocol_name, PROTOCOL_NAME)==0) {
        if((version_byte & 0x7F) == PROTOCOL_VERSION_v311) {
            protocol_version = mosq_p_mqtt311;

            if((version_byte & 0x80) == 0x80) {
                is_bridge = TRUE;
            }
        } else if((version_byte & 0x7F) == PROTOCOL_VERSION_v5) {
            protocol_version = mosq_p_mqtt5;
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt Invalid protocol version",
                "version",      "%d", (int)version_byte,
                NULL
            );
            send_connack(gobj, 0, CONNACK_REFUSED_PROTOCOL_VERSION, NULL);
            return -1;
        }
        if(priv->frame_head.flags != 0x00) {
            /* Reserved flags not set to 0, must disconnect. */
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt Reserved flags not set to 0",
                "flags",        "%d", (int)priv->frame_head.flags,
                NULL
            );
            return -1;
        }

    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt Invalid protocol",
            "protocol",     "%s", protocol_name,
            NULL
        );
        return -1;
    }

    gobj_write_str_attr(gobj, "protocol_name", protocol_name);
    gobj_write_integer_attr(gobj, "protocol_version", protocol_version);
    gobj_write_bool_attr(gobj, "is_bridge", is_bridge);

    /*-------------------------------------------*
     *      Connect flags
     *-------------------------------------------*/
    uint8_t connect_flags;
    uint32_t session_expiry_interval;
    uint8_t clean_start;
    uint8_t will, will_retain, will_qos;
    uint8_t username_flag, password_flag;

    if(mqtt_read_byte(gobj, gbuf, &connect_flags)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: no connect_flags ",
            NULL
        );
        return -1;
    }

    if(protocol_version == mosq_p_mqtt311 || protocol_version == mosq_p_mqtt5) {
        if((connect_flags & 0x01) != 0x00) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt: bad connect_flags",
                "connect_flags","%d", (int)connect_flags,
                NULL
            );
            return -1;
        }
    }

    clean_start = (connect_flags & 0x02) >> 1;

    /* session_expiry_interval will be overriden if the properties are read later */
    if(clean_start == FALSE && version_byte != PROTOCOL_VERSION_v5) {
        /* v3* has clean_start == FALSE mean the session never expires */
        session_expiry_interval = UINT32_MAX;
    } else {
        session_expiry_interval = 0;
    }

    will = connect_flags & 0x04;
    will_qos = (connect_flags & 0x18) >> 3;
    if(will_qos == 3) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: Invalid Will QoS",
            "connect_flags","%d", (int)connect_flags,
            NULL
        );
        return -1;
    }

    will_retain = ((connect_flags & 0x20) == 0x20);
    password_flag = connect_flags & 0x40;
    username_flag = connect_flags & 0x80;

    if(will && will_retain && priv->retain_available == FALSE) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: retain not available",
            NULL
        );
        if(version_byte == mosq_p_mqtt5) {
            send_connack(gobj, 0, MQTT_RC_RETAIN_NOT_SUPPORTED, NULL);
        }
        return -1;
    }

    if(will && will_qos > priv->max_qos) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: QoS not supported",
            "will_qos",     "%d", (int)will_qos,
            NULL
        );
        if(version_byte == mosq_p_mqtt5) {
            send_connack(gobj, 0, MQTT_RC_QOS_NOT_SUPPORTED, NULL);
        }
        return -1;
    }

    gobj_write_bool_attr(gobj, "clean_start", clean_start);
    gobj_write_integer_attr(gobj, "session_expiry_interval", session_expiry_interval);
    gobj_write_bool_attr(gobj, "will", will);
    gobj_write_bool_attr(gobj, "will_retain", will_retain);
    gobj_write_integer_attr(gobj, "will_qos", will_qos);

    /*-------------------------------------------*
     *      Keepalive
     *-------------------------------------------*/
    uint16_t keepalive;
    if(mqtt_read_uint16(gobj,gbuf, &keepalive)<0) {
        // Error already logged
        return -1;
    }
    gobj_write_integer_attr(gobj, "keepalive", keepalive);

    /*-------------------------------------------*
     *      Properties
     *-------------------------------------------*/
    if(version_byte == PROTOCOL_VERSION_v5) {
        json_t *all_properties = property_read_all(gobj, gbuf, CMD_CONNECT, 0);
        if(!all_properties) {
            // Error already logged
            return -1;
        }
        property_process_connect(gobj, all_properties);

        // Auth method not supported
        //if(mosquitto_property_read_string(
        //    properties,
        //    MQTT_PROP_AUTHENTICATION_METHOD,
        //    &context->auth_method,
        //    FALSE)
        //) {
        //    mosquitto_property_read_binary(
        //        properties,
        //        MQTT_PROP_AUTHENTICATION_DATA,
        //        &auth_data,
        //        &auth_data_len,
        //        FALSE
        //    );
        //}

        JSON_DECREF(all_properties);
    }

    /*-------------------------------------------*
     *      Client id
     *-------------------------------------------*/
    char uuid[60];
    char *client_id = NULL;
    BOOL assigned_id = FALSE;
    uint16_t client_id_len;

    if(mqtt_read_string(gobj, gbuf, &client_id, &client_id_len)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: bad client_id",
            NULL
        );
        return -1;
    }

    if(client_id_len == 0) {
        if(protocol_version == mosq_p_mqtt31) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt: no client_id",
                NULL
            );
            send_connack(gobj, 0, CONNACK_REFUSED_IDENTIFIER_REJECTED, NULL);
            return -1;

        } else { /* mqtt311/mqtt5 */
            client_id = NULL;
            if((protocol_version == mosq_p_mqtt311 && clean_start == 0) ||
                    priv->allow_zero_length_clientid == FALSE) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt: refuse empty client id",
                    NULL
                );
                if(protocol_version == mosq_p_mqtt311) {
                    send_connack(gobj, 0, CONNACK_REFUSED_IDENTIFIER_REJECTED, NULL);
                } else {
                    send_connack(gobj, 0, MQTT_RC_UNSPECIFIED, NULL);
                }
                return -1;
            } else {
                create_random_uuid(uuid, sizeof(uuid));
                client_id = uuid;
                if(!client_id) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MQTT_ERROR,
                        "msg",          "%s", "Mqtt: client_id_gen() FAILED",
                        NULL
                    );
                    return -1;
                }
                client_id_len = strlen(client_id);
                assigned_id = TRUE;
            }
        }
    }

    gobj_write_bool_attr(gobj, "assigned_id", assigned_id);
    gobj_write_strn_attr(gobj, "client_id", client_id, client_id_len);

    if(will) {
        if(will_read(gobj, gbuf)<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt: will_read FAILED()",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            return -1;
        }
    } else {
        if(protocol_version == mosq_p_mqtt311 || protocol_version == mosq_p_mqtt5) {
            if(will_qos != 0 || will_retain != 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt: will_qos will_retain",
                    "client_id",    "%s", priv->client_id,
                    NULL
                );
                return -1;
            }
        }
    }

    /*-------------------------------------------*
     *      Username and password
     *-------------------------------------------*/
    char *username = NULL, *password = NULL;
    uint16_t username_len = 0;
    uint16_t password_len = 0;
    if(username_flag) {
        if(mqtt_read_string(gobj, gbuf, &username, &username_len)<0) {
            if(protocol_version == mosq_p_mqtt31) {
                /* Username flag given, but no username. Ignore. */
                username_flag = 0;
            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt: no username",
                    "client_id",    "%s", priv->client_id,
                    NULL
                );
                return -1;
            }
        }
    } else {
        if(protocol_version == mosq_p_mqtt311 || protocol_version == mosq_p_mqtt31) {
            if(password_flag) {
                /* username_flag == 0 && password_flag == 1 is forbidden */
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt: password without username",
                    "client_id",    "%s", priv->client_id,
                    NULL
                );
                return -1;
            }
        }
    }
    if(password_flag) {
        if(mqtt_read_binary(gobj, gbuf, (uint8_t **)&password, &password_len)<0) {
            if(protocol_version == mosq_p_mqtt31) {
                /* Password flag given, but no password. Ignore. */
            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt: Password flag given, but no password",
                    "client_id",    "%s", priv->client_id,
                    NULL
                );
                return -1;
            }
        }
    }

    if(gbuffer_leftbytes(gbuf)>0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: too much data",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        return -1;
    }

    if(username_len) {
        gobj_write_strn_attr(gobj, "username", username, username_len);
    }
    if(password_len) {
        gobj_write_strn_attr(gobj, "password", password, password_len);
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        gobj_trace_msg(gobj,
        "  👈 CONNECT\n"
        "   client '%s', assigned_id %d\n"
        "   username '%s', password '%s'\n"
        "   protocol_name '%s', protocol_version '%s', is_bridge %d\n"
        "   clean_start %d, session_expiry_interval %d\n"
        "   will %d, will_retain %d, will_qos %d\n"
        "   username_flag %d, password_flag %d, keepalive %d\n",
            priv->client_id,
            priv->assigned_id,
            SAFE_PRINT(username),
            SAFE_PRINT(password),
            protocol_name,
            protocol_version_name(protocol_version),
            is_bridge,
            clean_start,
            session_expiry_interval,
            will,
            will_retain,
            will_qos,
            username_flag,
            password_flag,
            keepalive
        );
        if(priv->gbuf_will_payload) {
            gobj_trace_dump_gbuf(gobj, priv->gbuf_will_payload, "gbuf_will_payload");
        }
    }

    if(gobj_read_bool_attr(gobj, "use_username_as_clientid")) {
        const char *username = gobj_read_str_attr(gobj, "username");
        if(!empty_string(username)) {
            gobj_write_str_attr(gobj, "client_id", username);
        } else {
            if(protocol_version == mosq_p_mqtt5) {
                send_connack(gobj, 0, MQTT_RC_NOT_AUTHORIZED, NULL);
            } else {
                send_connack(gobj, 0, CONNACK_REFUSED_NOT_AUTHORIZED, NULL);
            }
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Mqtt: not authorized, use_username_as_clientid and no username",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            return -1;
        }
    }

    if(!empty_string(priv->auth_method)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: too AUTHORIZATION METHOD not supported",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        send_connack(gobj, 0, MQTT_RC_BAD_AUTHENTICATION_METHOD, NULL); // por contestar algo

    } else {
        if(mqtt_check_password(gobj)<0) {
            if(priv->protocol_version == mosq_p_mqtt5) {
                send_connack(gobj, 0, MQTT_RC_NOT_AUTHORIZED, NULL);
            } else {
                send_connack(gobj, 0, CONNACK_REFUSED_NOT_AUTHORIZED, NULL);
            }
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Mqtt: not authorized, use_username_as_clientid and no username",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            return -1;
        }
        return connect_on_authorised(gobj);
    }

    return -1;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__disconnect_s(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;
    json_t *properties = 0;
    uint8_t reason_code = 0;

    if(priv->frame_head.flags != 0) {
        return MOSQ_ERR_MALFORMED_PACKET;
    }
    if(priv->protocol_version == mosq_p_mqtt5 && gbuf && gbuffer_leftbytes(gbuf) > 0) {
        if(mqtt_read_byte(gobj, gbuf, &reason_code)<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt malformed packet, not enough data",
                NULL
            );
            return MOSQ_ERR_MALFORMED_PACKET;
        }

        if(gbuffer_leftbytes(gbuf) > 0) {
            properties = property_read_all(gobj, gbuf, CMD_DISCONNECT, &ret);
            if(!properties) {
                return ret;
            }
        }
    }
    if(properties) {
        json_t *property = property_get_property(properties, MQTT_PROP_SESSION_EXPIRY_INTERVAL);
        int session_expiry_interval = kw_get_int(gobj, property, "value", -1, 0);
        if(session_expiry_interval != -1) {
            if(priv->session_expiry_interval == 0 && session_expiry_interval!= 0) {
                JSON_DECREF(properties)
                return MOSQ_ERR_PROTOCOL;
            }
            priv->session_expiry_interval = session_expiry_interval;
        }
        JSON_DECREF(properties)
    }

    if(gbuf && gbuffer_leftbytes(gbuf)>0) {
        return MOSQ_ERR_PROTOCOL;
    }
    if(priv->protocol_version == mosq_p_mqtt311 || priv->protocol_version == mosq_p_mqtt5) {
        if(priv->frame_head.flags != 0x00) {
            do_disconnect(gobj, MOSQ_ERR_PROTOCOL);
            return MOSQ_ERR_PROTOCOL;
        }
    }
    if(reason_code == MQTT_RC_DISCONNECT_WITH_WILL_MSG) {
        // TODO mosquitto__set_state(context, mosq_cs_disconnect_with_will);
    } else {
        //will__clear(context);
        //mosquitto__set_state(context, mosq_cs_disconnecting);
    }

    do_disconnect(gobj, MOSQ_ERR_SUCCESS);
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__disconnect_c(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    // TODO

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__connack(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint8_t max_qos = 255;
    int ret = 0;

    // TODO bridge not tested

    if(!priv->is_bridge) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt CMD_CONNACK: i am not bridge",
            NULL
        );
        return -1;
    }

    uint8_t connect_acknowledge;
    if(mqtt_read_byte(gobj, gbuf, &connect_acknowledge)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt malformed packet, not enough data",
            NULL
        );
        return -1;
    }

    uint8_t reason_code;
    if(mqtt_read_byte(gobj, gbuf, &reason_code)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt malformed packet, not enough data",
            NULL
        );
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(priv->protocol_version == mosq_p_mqtt5) {
        if(gbuffer_leftbytes(gbuf) == 2 && reason_code == CONNACK_REFUSED_PROTOCOL_VERSION) {
            /* We have connected to a MQTT v3.x broker that doesn't support MQTT v5.0
             * It has correctly replied with a CONNACK code of a bad protocol version.
             */
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Remote bridge does not support MQTT v5.0, reconnecting using MQTT v3.1.1.",
                //"bridge_name",  "%s", priv->bridge_name,
                NULL
            );
            priv->protocol_version = mosq_p_mqtt311;
            //priv->bridge->protocol_version = mosq_p_mqtt311;
            return -1;
        }

        json_t *properties = property_read_all(gobj, gbuf, CMD_CONNACK, &ret);
        if(!properties) {
            return ret;
        }
        /* maximum-qos */
        max_qos = kw_get_int(gobj,
            properties, mqtt_property_identifier_to_string(MQTT_PROP_MAXIMUM_QOS), 0, 0
        );

        /* maximum-packet-size */
        int maximum_packet_size = kw_get_int(gobj,
            properties, mqtt_property_identifier_to_string(MQTT_PROP_MAXIMUM_PACKET_SIZE), -1, 0
        );
        if(maximum_packet_size != -1) {
            if(priv->maximum_packet_size == 0 || priv->maximum_packet_size > maximum_packet_size) {
                priv->maximum_packet_size = maximum_packet_size;
            }
        }

        /* receive-maximum */
        int inflight_maximum = kw_get_int(gobj,
            properties,
            mqtt_property_identifier_to_string(MQTT_PROP_RECEIVE_MAXIMUM),
            priv->msgs_out_inflight_maximum, // TODO client->msgs_out.inflight_maximum;
            0
        );
        if(priv->msgs_out_inflight_maximum != inflight_maximum) {
            priv->msgs_out_inflight_maximum = inflight_maximum;
            // TODO db__message_reconnect_reset(context);
        }

        /* retain-available */
        int retain_available = kw_get_int(gobj,
            properties,
            mqtt_property_identifier_to_string(MQTT_PROP_RETAIN_AVAILABLE),
            -1,
            0
        );
        if(retain_available != -1) {
            /* Only use broker provided value if the local config is set to available==TRUE */
            if(priv->retain_available) {
                priv->retain_available = retain_available;
            }
        }

        /* server-keepalive */
        int server_keepalive = kw_get_int(gobj,
            properties,
            mqtt_property_identifier_to_string(MQTT_PROP_SERVER_KEEP_ALIVE),
            -1,
            0
        );
        if(server_keepalive != -1) {
            priv->keepalive = server_keepalive;
        }
        JSON_DECREF(properties)
    }

    if(reason_code == 0) {
        if(priv->is_bridge) {
            //if(bridge__on_connect(context)<0) {
                return -1;
            //}
        }
        if(max_qos != 255) {
            priv->max_qos = max_qos;
        }
        //mosquitto__set_state(context, mosq_cs_active);
        //rc = db__message_write_queued_out(context);
        //if(rc) return rc;
        //rc = db__message_write_inflight_out_all(context);
        //return rc;
        return -1;
    } else {
        if(priv->protocol_version == mosq_p_mqtt5) {
            switch(reason_code) {
                case MQTT_RC_RETAIN_NOT_SUPPORTED:
                    priv->retain_available = 0;
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MQTT_ERROR,
                        "msg",          "%s", "Connection Refused: retain not available (will retry)",
                        NULL
                    );
                    return -1;
                case MQTT_RC_QOS_NOT_SUPPORTED:
                    if(max_qos == 255) {
                        if(priv->max_qos != 0) {
                            priv->max_qos--;
                        }
                    } else {
                        priv->max_qos = max_qos;
                    }
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MQTT_ERROR,
                        "msg",          "%s", "Connection Refused: QoS not supported (will retry)",
                        NULL
                    );
                    return -1;
                default:
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MQTT_ERROR,
                        "msg",          "%s", "Connection Refused",
                        "reason",       "%s", mosquitto_reason_string(reason_code),
                        NULL
                    );
                    return -1;
            }
        } else {
            switch(reason_code) {
                case CONNACK_REFUSED_PROTOCOL_VERSION:
                    if(priv->is_bridge) {
                        // TODO priv->bridge->try_private_accepted = FALSE;
                    }
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MQTT_ERROR,
                        "msg",          "%s", "Connection Refused: unacceptable protocol version",
                        NULL
                    );
                    return -1;
                case CONNACK_REFUSED_IDENTIFIER_REJECTED:
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MQTT_ERROR,
                        "msg",          "%s", "Connection Refused: identifier rejected",
                        NULL
                    );
                    return -1;
                case CONNACK_REFUSED_SERVER_UNAVAILABLE:
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MQTT_ERROR,
                        "msg",          "%s", "Connection Refused: broker unavailable",
                        NULL
                    );
                    return -1;
                case CONNACK_REFUSED_BAD_USERNAME_PASSWORD:
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MQTT_ERROR,
                        "msg",          "%s", "Connection Refused: bad user/password",
                        NULL
                    );
                    return -1;
                case CONNACK_REFUSED_NOT_AUTHORIZED:
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MQTT_ERROR,
                        "msg",          "%s", "Connection Refused: not authorised",
                        NULL
                    );
                    return -1;
                default:
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MQTT_ERROR,
                        "msg",          "%s", "Connection Refused: unknown reason",
                        "reason",       "%d", reason_code,
                        NULL
                    );
                    return -1;
            }
        }
    }
    return -1;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__pubackcomp(hgobj gobj, gbuffer_t *gbuf, const char *type)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint8_t reason_code = 0;
    uint16_t mid;
    int rc;
    json_t *properties = NULL;
    int qos;

    if(priv->protocol_version != mosq_p_mqtt31) {
        if((priv->frame_head.flags) != 0x00) {
            return MOSQ_ERR_MALFORMED_PACKET;
        }
    }

    //util__increment_send_quota(mosq);

    rc = mqtt_read_uint16(gobj, gbuf, &mid);
    if(rc<0) {
        return rc;
    }
    if(type[3] == 'A') { /* pubAck or pubComp */
        if(priv->frame_head.command != CMD_PUBACK) {
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        qos = 1;
    } else {
        if(priv->frame_head.command != CMD_PUBCOMP) {
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        qos = 2;
    }
    if(mid == 0) {
        return MOSQ_ERR_PROTOCOL;
    }

    if(priv->protocol_version == mosq_p_mqtt5 && gbuffer_leftbytes(gbuf) > 0) {
        rc = mqtt_read_byte(gobj, gbuf, &reason_code);
        if(rc) {
            return rc;
        }

        if(gbuffer_leftbytes(gbuf) > 0) {
            properties = property_read_all(gobj, gbuf, CMD_PUBACK, &rc);
            if(rc<0) {
                JSON_DECREF(properties)
                return rc;
            }
        }
        if(type[3] == 'A') { /* pubAck or pubComp */
            if(reason_code != MQTT_RC_SUCCESS
                    && reason_code != MQTT_RC_NO_MATCHING_SUBSCRIBERS
                    && reason_code != MQTT_RC_UNSPECIFIED
                    && reason_code != MQTT_RC_IMPLEMENTATION_SPECIFIC
                    && reason_code != MQTT_RC_NOT_AUTHORIZED
                    && reason_code != MQTT_RC_TOPIC_NAME_INVALID
                    && reason_code != MQTT_RC_PACKET_ID_IN_USE
                    && reason_code != MQTT_RC_QUOTA_EXCEEDED
                    && reason_code != MQTT_RC_PAYLOAD_FORMAT_INVALID
            ) {
                JSON_DECREF(properties)
                return MOSQ_ERR_PROTOCOL;
            }
        } else {
            if(reason_code != MQTT_RC_SUCCESS
                    && reason_code != MQTT_RC_PACKET_ID_NOT_FOUND
            ) {
                JSON_DECREF(properties)
                return MOSQ_ERR_PROTOCOL;
            }
        }
    }
    if(gbuffer_leftbytes(gbuf)) {
        JSON_DECREF(properties)
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        if(strcmp(type, "PUBACK")==0) {
            trace_msg0("  👈 Received PUBACK from client '%s' (Mid: %d, RC:%d)",
                SAFE_PRINT(priv->client_id),
                mid,
                reason_code
            );
        } else {
            trace_msg0("  👈 Received PUBCOMP from client '%s' (Mid: %d, RC:%d)",
                SAFE_PRINT(priv->client_id),
                mid,
                reason_code
            );
        }
    }

    if(priv->iamServer) {
        /* Immediately free, we don't do anything with Reason String or User Property at the moment */
        JSON_DECREF(properties)

        rc = db__message_delete_outgoing(gobj, mid, mosq_ms_wait_for_pubcomp, qos);
        if(rc == MOSQ_ERR_NOT_FOUND) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt: Received for an unknown packet",
                "client_id",    "%s", priv->client_id,
                "type",         "%s", type,
                "mid",          "%d", mid,
                NULL
            );
            return MOSQ_ERR_SUCCESS;
        } else {
            return rc;
        }
    } else {
        if(gobj_trace_level(gobj) & SHOW_DECODE) {
            trace_msg0("Client %s received %s (Mid: %d, RC:%d)",
                SAFE_PRINT(priv->client_id),
                type,
                mid,
                reason_code
            );
        }

        // TODO client
        // rc = message__delete(gobj, mid, mosq_md_out, qos);
        // if(rc == MOSQ_ERR_SUCCESS) {
        //     // mosq->in_callback = TRUE;
        //     on_publish_v5(gobj, mid, reason_code, properties);
        //     // mosq->in_callback = FALSE;
        // } else if(rc != MOSQ_ERR_NOT_FOUND){
        //     JSON_DECREF(properties)
        //     return rc;
        // }
        //
        // message__release_to_inflight(gobj, mosq_md_out);
        JSON_DECREF(properties)
    	return MOSQ_ERR_SUCCESS;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__pubrec(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint8_t reason_code = 0;
    uint16_t mid;
    int rc;
    json_t *properties = NULL;

    if(priv->frame_head.flags != 0) {
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    rc = mqtt_read_uint16(gobj, gbuf, &mid);
    if(rc<0) {
        return rc;
    }
    if(mid == 0) {
        return MOSQ_ERR_PROTOCOL;
    }

    if(priv->protocol_version == mosq_p_mqtt5 && gbuffer_leftbytes(gbuf) > 0) {
        rc = mqtt_read_byte(gobj, gbuf, &reason_code);
        if(rc<0) {
            return rc;
        }

        if(reason_code != MQTT_RC_SUCCESS
                && reason_code != MQTT_RC_NO_MATCHING_SUBSCRIBERS
                && reason_code != MQTT_RC_UNSPECIFIED
                && reason_code != MQTT_RC_IMPLEMENTATION_SPECIFIC
                && reason_code != MQTT_RC_NOT_AUTHORIZED
                && reason_code != MQTT_RC_TOPIC_NAME_INVALID
                && reason_code != MQTT_RC_PACKET_ID_IN_USE
                && reason_code != MQTT_RC_QUOTA_EXCEEDED
                && reason_code != MQTT_RC_PAYLOAD_FORMAT_INVALID) {
            return MOSQ_ERR_PROTOCOL;
        }

        if(gbuffer_leftbytes(gbuf) > 0) {
            properties = property_read_all(gobj, gbuf, CMD_PUBREC, &rc);
            if(rc<0) {
                return rc;
            }

            /*
             *  Immediately free, we don't do anything with Reason String or
             *  User Property at the moment
             */
            JSON_DECREF(properties)
        }
    }

    if(gbuffer_leftbytes(gbuf)>0) {
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("  👈 Received PUBREC from client '%s' (Mid: %d, reason code: %02X)",
            SAFE_PRINT(priv->client_id),
            mid,
            reason_code
        );
    }
    printf("================> PUBREC OUT Client %s\n", priv->client_id);
    print_queue("dl_msgs_out", &priv->dl_msgs_out);

    if(priv->iamServer) {
        if(reason_code < 0x80) {
            rc = db__message_update_outgoing(gobj, mid, mosq_ms_wait_for_pubcomp, 2);
        } else {
            return db__message_delete_outgoing(gobj, mid, mosq_ms_wait_for_pubrec, 2);
        }
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: Received PUBREC being client",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        return -1;
        // Código con IFDEF solo en cliente
        // if(reason_code < 0x80 || priv->protocol_version != mosq_p_mqtt5) {
        //     rc = message__out_update(gobj, mid, mosq_ms_wait_for_pubcomp, 2);
        // } else {
        //     if(!message__delete(gobj, mid, mosq_md_out, 2)) {
        //         /* Only inform the client the message has been sent once. */
        //         if(mosq->on_publish_v5) {
        //            mosq->in_callback = TRUE;
        //            mosq->on_publish_v5(mosq, mosq->userdata, mid, reason_code, properties);
        //            mosq->in_callback = FALSE;
        //         }
        //     }
        //     //util__increment_send_quota(mosq);
        //     message__release_to_inflight(gobj, mosq_md_out);
        //     return MOSQ_ERR_SUCCESS;
        // }
    }

    if(rc == MOSQ_ERR_NOT_FOUND) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: Received for an unknown packet",
            "client_id",    "%s", priv->client_id,
            "mid",          "%d", mid,
            NULL
        );
    } else if(rc != MOSQ_ERR_SUCCESS) {
        return rc;
    }
    rc = send__pubrel(gobj, mid, NULL);
    if(rc) {
        return rc;
    }

    return MOSQ_ERR_SUCCESS;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__pubrel(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint8_t reason_code;
    uint16_t mid;
    int rc;
    json_t *properties = NULL;

    if(priv->protocol_version != mosq_p_mqtt31 && priv->frame_head.flags != 0x02) {
        return MOSQ_ERR_MALFORMED_PACKET;
    }
    rc = mqtt_read_uint16(gobj, gbuf, &mid);
    if(rc) {
        return rc;
    }
    if(mid == 0) {
        return MOSQ_ERR_PROTOCOL;
    }

    if(priv->protocol_version == mosq_p_mqtt5 && gbuffer_leftbytes(gbuf) > 0) {
        rc = mqtt_read_byte(gobj, gbuf, &reason_code);
        if(rc) {
            return rc;
        }

        if(reason_code != MQTT_RC_SUCCESS && reason_code != MQTT_RC_PACKET_ID_NOT_FOUND) {
            return MOSQ_ERR_PROTOCOL;
        }

        if(gbuffer_leftbytes(gbuf) > 0) {
            properties = property_read_all(gobj, gbuf, CMD_PUBREL, &rc);
            if(rc) {
                return rc;
            }
        }
    }

    if(gbuffer_leftbytes(gbuf)>0) {
        JSON_DECREF(properties)
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("  👈 Received PUBREL from client '%s' (Mid: %d)",
            SAFE_PRINT(priv->client_id),
            mid
        );
    }

    if(priv->iamServer) {
        /* Immediately free, we don't do anything with Reason String or User Property at the moment */
        JSON_DECREF(properties)

        rc = db__message_release_incoming(gobj, mid);
        if(rc == MOSQ_ERR_NOT_FOUND) {
            /* Message not found. Still send a PUBCOMP anyway because this could be
            * due to a repeated PUBREL after a client has reconnected. */
        } else if(rc != MOSQ_ERR_SUCCESS) {
            return rc;
        }

        rc = send_pubcomp(gobj, mid, NULL);
        if(rc) {
            return rc;
        }
    } else {
        rc = -1; // original tiene un IFDEF codigo no incorporado al broker
//         struct mosquitto_message_all *message = NULL;
//         rc = send_pubcomp(gobj, mid, NULL);
//         if(rc) {
//             message__remove(gobj, mid, mosq_md_in, &message, 2);
//             return rc;
//         }
//
//         rc = message__remove(gobj, mid, mosq_md_in, &message, 2);
//         if(rc == MOSQ_ERR_SUCCESS) {
//             /* Only pass the message on if we have removed it from the queue - this
//             * prevents multiple callbacks for the same message. */
//             if(mosq->on_message) {
//                mosq->in_callback = TRUE;
//                mosq->on_message(mosq, mosq->userdata, &message->msg);
//                mosq->in_callback = FALSE;
//             }
//             if(mosq->on_message_v5) {
//                mosq->in_callback = TRUE;
//                mosq->on_message_v5(mosq, mosq->userdata, &message->msg, message->properties);
//                mosq->in_callback = FALSE;
//             }
//             JSON_DECREF(properties)
//             message__cleanup(&message);
//         } else if(rc == MOSQ_ERR_NOT_FOUND) {
//             return MOSQ_ERR_SUCCESS;
//         } else {
//             return rc;
//         }
    }

    return MOSQ_ERR_SUCCESS;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__suback(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint16_t mid = 0;
    uint8_t qos;
    int *granted_qos;
    int qos_count;
    int i = 0;
    int rc;
    json_t *properties = NULL;

    // TODO bridge not tested

    if(priv->frame_head.flags != 0) {
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(priv->iamServer) {
        if(priv->is_bridge == 0) {
            /* Client is not a bridge, so shouldn't be sending SUBACK */
            return MOSQ_ERR_PROTOCOL;
        }
    }
    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("  👈 Received SUBACK from client '%s' (Mid: %d)",
            SAFE_PRINT(priv->client_id),
            mid
        );
    }

    rc = mqtt_read_uint16(gobj, gbuf, &mid);
    if(rc) {
        return rc;
    }
    if(mid == 0) {
        return MOSQ_ERR_PROTOCOL;
    }

    if(priv->protocol_version == mosq_p_mqtt5) {
        properties = property_read_all(gobj, gbuf, CMD_SUBACK, &rc);
        if(rc) {
            return rc;
        }
    }

    qos_count = (int)(gbuffer_leftbytes(gbuf));
    granted_qos = GBMEM_MALLOC((size_t)qos_count*sizeof(int));
    if(!granted_qos) {
        JSON_DECREF(properties)
        return MOSQ_ERR_NOMEM;
    }
    while(gbuffer_leftbytes(gbuf)>0) {
        rc = mqtt_read_byte(gobj, gbuf, &qos);
        if(rc) {
            GBMEM_FREE(granted_qos);
            JSON_DECREF(properties)
            return rc;
        }
        granted_qos[i] = (int)qos;
        i++;
    }
    if(priv->iamServer) {
        /* Immediately free, we don't do anything with Reason String or User Property at the moment */
        JSON_DECREF(properties)
    } else {
        // TODO PUBLISH if(mosq->on_subscribe) {
        //    mosq->in_callback = TRUE;
        //    mosq->on_subscribe(mosq, mosq->userdata, mid, qos_count, granted_qos);
        //    mosq->in_callback = FALSE;
        //}
        //if(mosq->on_subscribe_v5) {
        //    mosq->in_callback = TRUE;
        //    mosq->on_subscribe_v5(mosq, mosq->userdata, mid, qos_count, granted_qos, properties);
        //    mosq->in_callback = FALSE;
        //}
        JSON_DECREF(properties)
    }
    GBMEM_FREE(granted_qos);

    return MOSQ_ERR_SUCCESS;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__unsuback(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint16_t mid = 0;
    int rc;
    json_t *properties = NULL;

    // TODO bridge not tested

    if(priv->frame_head.flags != 0) {
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(priv->iamServer) {
        if(priv->is_bridge == 0) {
            /* Client is not a bridge, so shouldn't be sending SUBACK */
            return MOSQ_ERR_PROTOCOL;
        }
    }
    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("  👈 Received UNSUBACK from client '%s' (Mid: %d)",
            SAFE_PRINT(priv->client_id),
            mid
        );
    }

    rc = mqtt_read_uint16(gobj, gbuf, &mid);
    if(rc) {
        return rc;
    }
    if(mid == 0) {
        return MOSQ_ERR_PROTOCOL;
    }

    if(priv->protocol_version == mosq_p_mqtt5) {
        properties = property_read_all(gobj, gbuf, CMD_UNSUBACK, &rc);
        if(rc) {
            return rc;
        }
    }

    if(priv->iamServer) {
        /* Immediately free, we don't do anything with Reason String or User Property at the moment */
        JSON_DECREF(properties)
    } else {
        // TODO publish if(mosq->on_unsubscribe) {
        //    mosq->in_callback = TRUE;
        //    mosq->on_unsubscribe(mosq, mosq->userdata, mid);
        //    mosq->in_callback = FALSE;
        //}
        //if(mosq->on_unsubscribe_v5) {
        //    mosq->in_callback = TRUE;
        //    mosq->on_unsubscribe_v5(mosq, mosq->userdata, mid, properties);
        //    mosq->in_callback = FALSE;
        //}
        JSON_DECREF(properties)
    }

    return MOSQ_ERR_SUCCESS;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__publish_s(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint8_t dup;
    int rc = 0;
    struct mosquitto_msg_store *msg, *stored = NULL;
    uint16_t slen;
    json_t *properties = NULL;
    uint32_t message_expiry_interval = 0;
    int topic_alias = -1;
    uint8_t reason_code = 0;
    uint16_t mid = 0;

    msg = GBMEM_MALLOC(sizeof(struct mosquitto_msg_store));
    if(msg == NULL) {
        return MOSQ_ERR_NOMEM;
    }

    uint8_t header = priv->frame_head.flags;
    dup = (header & 0x08)>>3;
    msg->qos = (header & 0x06)>>1;
    if(dup == 1 && msg->qos == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: Invalid PUBLISH (QoS=0 and DUP=1)",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        db_free_msg_store(msg);
        return MOSQ_ERR_MALFORMED_PACKET;
    }
    if(msg->qos == 3) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: Invalid QoS in PUBLISH",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        db_free_msg_store(msg);
        return MOSQ_ERR_MALFORMED_PACKET;
    }
    if(msg->qos > priv->max_qos) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: Too high QoS in PUBLISH",
            "client_id",    "%s", priv->client_id,
            "max_qos",      "%d", (int)priv->max_qos,
            "qos",          "%d", msg->qos,
            NULL
        );
        db_free_msg_store(msg);
        return MOSQ_ERR_QOS_NOT_SUPPORTED;
    }
    msg->retain = (header & 0x01);

    if(msg->retain && priv->retain_available == FALSE) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: retain not supported",
            "client_id",    "%s", priv->client_id,
            "max_qos",      "%d", (int)priv->max_qos,
            "qos",          "%d", msg->qos,
            NULL
        );
        db_free_msg_store(msg);
        return MOSQ_ERR_RETAIN_NOT_SUPPORTED;
    }

    char *topic_;
    if(mqtt_read_string(gobj, gbuf, &topic_, &slen)<0) {
        // Error already logged
        db_free_msg_store(msg);
        return MOSQ_ERR_MALFORMED_PACKET;
    }
    msg->topic = gbmem_strndup(topic_, slen);

    if(!slen && priv->protocol_version != mosq_p_mqtt5) {
        /* Invalid publish topic, disconnect client. */
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: topic len 0 and not mqtt5",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        db_free_msg_store(msg);
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(msg->qos > 0) {
        if(mqtt_read_uint16(gobj, gbuf, &mid)<0) {
            // Error already logged
            db_free_msg_store(msg);
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        if(mid == 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt: qos>0 and mid=0",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            db_free_msg_store(msg);
            return MOSQ_ERR_PROTOCOL;
        }
        /* It is important to have a separate copy of mid, because msg may be
         * freed before we want to send a PUBACK/PUBREC. */
        msg->source_mid = mid;
    }

    /* Handle properties */
    if(priv->protocol_version == mosq_p_mqtt5) {
        properties = property_read_all(gobj, gbuf, CMD_PUBLISH, &rc);
        if(rc<0) {
            db_free_msg_store(msg);
            return rc;
        }

        const char *property_name; json_t *property;
        json_object_foreach(properties, property_name, property) {
            json_int_t identifier = kw_get_int(gobj, property, "identifier", 0, KW_REQUIRED);

            switch(identifier) {
                case MQTT_PROP_CONTENT_TYPE:
                case MQTT_PROP_CORRELATION_DATA:
                case MQTT_PROP_PAYLOAD_FORMAT_INDICATOR:
                case MQTT_PROP_RESPONSE_TOPIC:
                case MQTT_PROP_USER_PROPERTY:
                    {
                        if(!msg->properties) {
                            msg->properties = json_object();
                        }
                        json_object_set(msg->properties, property_name, property);
                    }
                    break;

                case MQTT_PROP_TOPIC_ALIAS:
                    topic_alias = kw_get_int(gobj, property, "value", 0, KW_REQUIRED);
                    break;

                case MQTT_PROP_MESSAGE_EXPIRY_INTERVAL:
                    message_expiry_interval = kw_get_int(gobj, property, "value", 0, KW_REQUIRED);
                    break;

                case MQTT_PROP_SUBSCRIPTION_IDENTIFIER:
                    break;

                default:
                    break;
            }
        }
    }
    JSON_DECREF(properties)

    if(topic_alias == 0 || (topic_alias > (int)priv->max_topic_alias)) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MQTT_ERROR,
            "msg",              "%s", "Mqtt: invalid topic alias",
            "client_id",        "%s", priv->client_id,
            "max_topic_alias",  "%d", priv->max_topic_alias,
            "topic_alias",      "%d", topic_alias,
            NULL
        );
        db_free_msg_store(msg);
        return MOSQ_ERR_TOPIC_ALIAS_INVALID;

    } else if(topic_alias > 0) {
        if(msg->topic) {
            save_topic_alias(gobj, topic_alias, msg->topic);
            //rc = alias__add(context, msg->topic, (uint16_t)topic_alias);
            //if(rc){
            //    db_free_msg_store(msg);
            //    return rc;
            //}
        } else {
            char *alias = find_alias_topic(gobj, (uint16_t)topic_alias);
            if(alias) {
                GBMEM_FREE(msg->topic);
                msg->topic = alias;
            } else {
                gobj_log_error(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_MQTT_ERROR,
                    "msg",              "%s", "Mqtt: topic alias NOT FOUND",
                    "client_id",        "%s", priv->client_id,
                    "max_topic_alias",  "%d", priv->max_topic_alias,
                    "topic_alias",      "%d", topic_alias,
                    NULL
                );
                db_free_msg_store(msg);
                return MOSQ_ERR_PROTOCOL;
            }
        }
    }

    if(priv->is_bridge)  {
        //rc = bridge__remap_topic_in(context, &msg->topic);
        //if(rc) {
        //    db_free_msg_store(msg);
        //    return rc;
        //}
    }

    if(mosquitto_pub_topic_check(msg->topic)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt will: invalid topic",
            "topic",        "%s", msg->topic,
            NULL
        );
        db_free_msg_store(msg);
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    msg->payloadlen = gbuffer_leftbytes(gbuf);
    //G_PUB_BYTES_RECEIVED_INC(msg->payloadlen);

    if(msg->payloadlen) {
        if(priv->message_size_limit && msg->payloadlen > priv->message_size_limit) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MQTT_ERROR,
                "msg",              "%s", "Mqtt: Dropped too large PUBLISH",
                "client_id",        "%s", priv->client_id,
                "topic",            "%d", msg->topic,
                NULL
            );
            db_free_msg_store(msg);
            reason_code = MQTT_RC_PACKET_TOO_LARGE;
            goto process_bad_message;
        }
        msg->payload = GBMEM_MALLOC(msg->payloadlen + 1);
        if(msg->payload == NULL) {
            // Error already logged
            db_free_msg_store(msg);
            return MOSQ_ERR_NOMEM;
        }

        if(mqtt_read_bytes(gobj, gbuf, msg->payload, msg->payloadlen)) {
            db_free_msg_store(msg);
            return MOSQ_ERR_MALFORMED_PACKET;
        }
    }

    /* Check for topic access */
    rc = 0; // TODO mosquitto_acl_check(gobj, msg, MOSQ_ACL_WRITE);
    if(rc == MOSQ_ERR_ACL_DENIED) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MQTT_ERROR,
            "msg",              "%s", "Mqtt: Denied PUBLISH",
            "client_id",        "%s", priv->client_id,
            "topic",            "%d", msg->topic,
            NULL
        );
        reason_code = MQTT_RC_NOT_AUTHORIZED;
        goto process_bad_message;
    } else if(rc != MOSQ_ERR_SUCCESS) {
        // Error already logged
        db_free_msg_store(msg);
        return rc;
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("  👈 Received PUBLISH from client '%s', topic '%s' (dup %d, qos %d, retain %d, mid %d, len %ld)",
            priv->client_id,
            msg->topic,
            dup,
            msg->qos,
            msg->retain,
            msg->source_mid,
            (long)msg->payloadlen
        );
    }
    if(strncmp(msg->topic, "$CONTROL/", 9)==0) {
        reason_code = MQTT_RC_IMPLEMENTATION_SPECIFIC;
        goto process_bad_message;
    }
    // plugin__handle_message(): No plugins in use

    if(msg->qos > 0) {
        stored = db_message_store_find(gobj, msg->source_mid);
    }

    if(stored && msg->source_mid != 0 &&
            (stored->qos != msg->qos
             || stored->payloadlen != msg->payloadlen
             || strcmp(stored->topic, msg->topic)
             || memcmp(stored->payload, msg->payload, msg->payloadlen) )) {
        gobj_log_warning(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INFO,
            "msg",              "%s", "Mqtt: Reused message ID",
            "client_id",        "%s", priv->client_id,
            "topic",            "%d", msg->topic,
            "mid",              "%d", msg->source_mid,
            NULL
        );
        db__message_remove_incoming(gobj, msg->source_mid);
        stored = NULL;
    }

    if(!stored) {
        if(msg->qos == 0
                || db__ready_for_flight(gobj, mosq_md_in, msg->qos)
          ) {
            dup = 0;
            rc = db__message_store(gobj, msg, message_expiry_interval);
            if(rc) {
                return rc;
            }
        } else {
            /* Client isn't allowed any more incoming messages, so fail early */
            reason_code = MQTT_RC_QUOTA_EXCEEDED;
            goto process_bad_message;
        }
        stored = msg;
        msg = NULL;
    } else {
        db_free_msg_store(msg);
        msg = NULL;
        dup = 1;
    }

    //stored->qos = 0; // TODO TEST

    switch(stored->qos) {
        case 0:
            {
                json_t *jn_subscribers = sub_get_subscribers(gobj, stored->topic);
                XXX_sub__messages_queue(
                    gobj,
                    jn_subscribers,
                    stored->topic,
                    stored->qos,
                    stored->retain,
                    stored
                );
            }
            break;
        case 1:
            /* stored may now be free, so don't refer to it */
            {
                json_t *jn_subscribers = sub_get_subscribers(gobj, stored->topic);

                BOOL has_subscribers = json_array_size(jn_subscribers)?TRUE:FALSE;
                //util__decrement_receive_quota(context);
                XXX_sub__messages_queue(
                    gobj,
                    jn_subscribers,
                    stored->topic,
                    stored->qos,
                    stored->retain,
                    stored
                );
                if(has_subscribers || priv->protocol_version != mosq_p_mqtt5) {
                    if(send_puback(gobj, mid, 0, NULL)<0) {
                        rc = MOSQ_ERR_NOMEM;
                    }
                } else {
                    if(send_puback(gobj, mid, MQTT_RC_NO_MATCHING_SUBSCRIBERS, NULL)<0) {
                        rc = MOSQ_ERR_NOMEM;
                    }
                }
            }
            break;
        case 2:
            if(dup == 0) {
                XXX_save_message_to_pubrec( // guarda el mensaje hasta el PUBREL
                    gobj,
                    stored->source_mid,
                    stored->qos,
                    stored->retain,
                    stored,
                    NULL
                );
            }
            if(send_pubrec(gobj, stored->source_mid, 0, NULL)<0) {
                rc = MOSQ_ERR_NOMEM;
            }
            break;
    }

    db_free_msg_store(stored);
    return rc;

process_bad_message:
    rc = MOSQ_ERR_NOMEM;
    if(msg) {
        switch(msg->qos) {
            case 0:
                rc = MOSQ_ERR_SUCCESS;
                break;
            case 1:
                rc = send_puback(gobj, msg->source_mid, reason_code, NULL);
                break;
            case 2:
                rc = send_pubrec(gobj, msg->source_mid, reason_code, NULL);
                break;
        }
        db_free_msg_store(msg);
    }
    return rc;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__publish_c(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int rc = 0;
    // TODO
    return rc;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__subscribe(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int rc = 0;
    int rc2;
    uint16_t mid;
    uint8_t subscription_options;
    json_int_t subscription_identifier = 0;
    uint8_t qos;
    uint8_t retain_handling = 0;
    uint8_t *payload = NULL;
    uint8_t *tmp_payload;
    uint32_t payloadlen = 0;
    uint16_t slen;
    json_t *properties = NULL;
    BOOL allowed;

    if(priv->frame_head.flags != 2) {
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(mqtt_read_uint16(gobj, gbuf, &mid)) {
        return MOSQ_ERR_MALFORMED_PACKET;
    }
    if(mid == 0) {
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(priv->protocol_version == mosq_p_mqtt5) {
        properties = property_read_all(gobj, gbuf, CMD_SUBSCRIBE, &rc);
        if(rc) {
            /* FIXME - it would be better if property__read_all() returned
             * MOSQ_ERR_MALFORMED_PACKET, but this is would change the library
             * return codes so needs doc changes as well. */
            if(rc == MOSQ_ERR_PROTOCOL) {
                return MOSQ_ERR_MALFORMED_PACKET;
            } else {
                return rc;
            }
        }

        subscription_identifier = property_get_int(properties, MQTT_PROP_SUBSCRIPTION_IDENTIFIER);
        if(subscription_identifier != -1) {
            /* If the identifier was force set to 0, this is an error */
            if(subscription_identifier == 0) {
                JSON_DECREF(properties)
                return MOSQ_ERR_MALFORMED_PACKET;
            }
        }

        JSON_DECREF(properties)
        /* Note - User Property not handled */
    }

    json_t *jn_list = json_array();

    while(gbuffer_leftbytes(gbuf)>0) {
        char *sub_ = NULL;
        char *sub = NULL;
        if(mqtt_read_string(gobj, gbuf, &sub_, &slen)<0) {
            GBMEM_FREE(payload)
            JSON_DECREF(jn_list)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        if(sub_) {
            if(!slen) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Empty subscription string, disconnecting",
                    "client_id",    "%s", priv->client_id,
                    NULL
                );
                GBMEM_FREE(payload)
                JSON_DECREF(jn_list)
                return MOSQ_ERR_MALFORMED_PACKET;
            }
            sub = gbmem_strndup(sub_, slen); // Por algún motivo es necesario
            if(mosquitto_sub_topic_check(sub)) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Invalid subscription string, disconnecting",
                    "client_id",    "%s", priv->client_id,
                    NULL
                );
                GBMEM_FREE(sub)
                GBMEM_FREE(payload)
                JSON_DECREF(jn_list)
                return MOSQ_ERR_MALFORMED_PACKET;
            }

            if(mqtt_read_byte(gobj, gbuf, &subscription_options)) {
                GBMEM_FREE(sub)
                GBMEM_FREE(payload)
                JSON_DECREF(jn_list)
                return MOSQ_ERR_MALFORMED_PACKET;
            }
            if(priv->protocol_version == mosq_p_mqtt31 || priv->protocol_version == mosq_p_mqtt311) {
                qos = subscription_options;
                if(priv->is_bridge) {
                    subscription_options = MQTT_SUB_OPT_RETAIN_AS_PUBLISHED | MQTT_SUB_OPT_NO_LOCAL;
                }
            } else {
                qos = subscription_options & 0x03;
                subscription_options &= 0xFC;

                retain_handling = (subscription_options & 0x30);
                if(retain_handling == 0x30 || (subscription_options & 0xC0) != 0) {
                    GBMEM_FREE(sub)
                    GBMEM_FREE(payload)
                    JSON_DECREF(jn_list)
                    return MOSQ_ERR_MALFORMED_PACKET;
                }
            }
            if(qos > 2) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Invalid QoS in subscription command, disconnecting",
                    "client_id",    "%s", priv->client_id,
                    NULL
                );
                GBMEM_FREE(sub)
                GBMEM_FREE(payload)
                JSON_DECREF(jn_list)
                return MOSQ_ERR_MALFORMED_PACKET;
            }
            if(qos > priv->max_qos) {
                qos = priv->max_qos;
            }

            if(gobj_trace_level(gobj) & SHOW_DECODE) {
                trace_msg0("  👈 Received SUBSCRIBE from client '%s', topic '%s' (QoS %d)",
                    priv->client_id,
                    sub,
                    qos
                );
            }

            allowed = TRUE;
            //rc2 = mosquitto_acl_check(context, sub, 0, NULL, qos, FALSE, MOSQ_ACL_SUBSCRIBE);

            if(allowed) {
                rc2 = add_subscription(
                    gobj,
                    sub,
                    qos,
                    subscription_identifier,
                    subscription_options
                );
                if(rc2 < 0) {
                    GBMEM_FREE(sub)
                    GBMEM_FREE(payload)
                    JSON_DECREF(jn_list)
                    return rc2;
                }

                json_array_append_new(jn_list, json_string(sub));

                if(priv->protocol_version == mosq_p_mqtt311 ||
                    priv->protocol_version == mosq_p_mqtt31
                ) {
                    if(rc2 == MOSQ_ERR_SUCCESS || rc2 == MOSQ_ERR_SUB_EXISTS) {
                        if(retain__queue(gobj, sub, qos, 0)) {
                            rc = MOSQ_ERR_NOMEM;
                        }
                    }
                } else {
                    if((retain_handling == MQTT_SUB_OPT_SEND_RETAIN_ALWAYS)
                            || (rc2 == MOSQ_ERR_SUCCESS && retain_handling == MQTT_SUB_OPT_SEND_RETAIN_NEW)
                      ) {
                        if(retain__queue(gobj, sub, qos, subscription_identifier)) {
                            rc = MOSQ_ERR_NOMEM;
                        }
                    }
                }
            }

            tmp_payload = GBMEM_REALLOC(payload, payloadlen + 1);
            if(tmp_payload) {
                payload = tmp_payload;
                payload[payloadlen] = qos;
                payloadlen++;
            } else {
                GBMEM_FREE(sub)
                GBMEM_FREE(payload)
                JSON_DECREF(jn_list)
                return MOSQ_ERR_NOMEM;
            }
            GBMEM_FREE(sub)
        }
    }

    if(priv->protocol_version != mosq_p_mqtt31) {
        if(payloadlen == 0) {
            /* No subscriptions specified, protocol error. */
            JSON_DECREF(jn_list)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
    }
    if(send__suback(gobj, mid, payloadlen, payload)!=0) {
        rc = MOSQ_ERR_NOMEM;
    }
    GBMEM_FREE(payload)

    save_client(gobj);

    json_t *kw = json_pack("{s:s, s:s, s:o}",
        "client_id", priv->client_id,
        "mqtt_action", "subscribing",
        "list", jn_list
    );
    gobj_publish_event(gobj, EV_ON_MESSAGE, kw);

//     if(priv->current_out_packet == NULL) {
//         db__message_write_queued_out(gobj);
//         db__message_write_inflight_out_latest(gobj);
//     }

    return rc;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__unsubscribe(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint16_t mid;
    uint16_t slen;
    int rc;
    uint8_t reason = 0;
    int reason_code_count = 0;
    int reason_code_max;
    uint8_t *reason_codes = NULL, *reason_tmp;
    json_t *properties = NULL;
    BOOL allowed;

    if(priv->frame_head.flags != 2) {
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(mqtt_read_uint16(gobj, gbuf, &mid)) {
        return MOSQ_ERR_MALFORMED_PACKET;
    }
    if(mid == 0) {
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(priv->protocol_version == mosq_p_mqtt5) {
        properties = property_read_all(gobj, gbuf, CMD_UNSUBSCRIBE, &rc);
        if(rc) {
            /* FIXME - it would be better if property__read_all() returned
             * MOSQ_ERR_MALFORMED_PACKET, but this is would change the library
             * return codes so needs doc changes as well. */
            if(rc == MOSQ_ERR_PROTOCOL) {
                return MOSQ_ERR_MALFORMED_PACKET;
            } else {
                return rc;
            }
        }
        /* Immediately free, we don't do anything with User Property at the moment */
        JSON_DECREF(properties)
    }

    if(priv->protocol_version == mosq_p_mqtt311 || priv->protocol_version == mosq_p_mqtt5) {
        if(gbuffer_leftbytes(gbuf)==0) {
            /* No topic specified, protocol error. */
            return MOSQ_ERR_MALFORMED_PACKET;
        }
    }

    reason_code_max = 10;
    reason_codes = GBMEM_MALLOC((size_t)reason_code_max);
    if(!reason_codes) {
        return MOSQ_ERR_NOMEM;
    }

    json_t *jn_list = json_array();

    while(gbuffer_leftbytes(gbuf)>0) {
        char *sub = NULL;
        if(mqtt_read_string(gobj, gbuf, &sub, &slen)<0) {
            GBMEM_FREE(reason_codes)
            JSON_DECREF(jn_list)
            return MOSQ_ERR_MALFORMED_PACKET;
        }

        if(!slen) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Empty unsubscription string, disconnecting",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            GBMEM_FREE(reason_codes);
            JSON_DECREF(jn_list)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        if(mosquitto_sub_topic_check(sub)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Invalid unsubscription string, disconnecting",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            GBMEM_FREE(reason_codes);
            JSON_DECREF(jn_list)
            return MOSQ_ERR_MALFORMED_PACKET;
        }

        /* ACL check */
        allowed = TRUE;
        //rc = mosquitto_acl_check(context, sub, 0, NULL, 0, FALSE, MOSQ_ACL_UNSUBSCRIBE);

        if(gobj_trace_level(gobj) & SHOW_DECODE) {
            trace_msg0("  👈 Received UNSUBSCRIBE from client '%s', topic '%s'",
                priv->client_id,
                sub
            );
        }
        if(allowed) {
            rc = remove_subscription(gobj, sub, &reason);
        } else {
            rc = MOSQ_ERR_SUCCESS;
        }

        if(rc<0) {
            GBMEM_FREE(reason_codes);
            JSON_DECREF(jn_list)
            return rc;
        }

        json_array_append_new(jn_list, json_string(sub));

        reason_codes[reason_code_count] = reason;
        reason_code_count++;
        if(reason_code_count == reason_code_max) {
            reason_tmp = GBMEM_REALLOC(reason_codes, (size_t)(reason_code_max*2));
            if(!reason_tmp) {
                GBMEM_FREE(reason_codes);
                JSON_DECREF(jn_list)
                return MOSQ_ERR_NOMEM;
            }
            reason_codes = reason_tmp;
            reason_code_max *= 2;
        }
    }

    /* We don't use Reason String or User Property yet. */
    rc = send__unsuback(gobj, mid, reason_code_count, reason_codes, NULL);
    GBMEM_FREE(reason_codes);
    save_client(gobj);

    json_t *kw = json_pack("{s:s, s:s, s:o}",
        "client_id", priv->client_id,
        "mqtt_action", "unsubscribing",
        "list", jn_list
    );
    gobj_publish_event(gobj, EV_ON_MESSAGE, kw);

    return rc;
}

/***************************************************************************
 *  Process the completed frame
 ***************************************************************************/
PRIVATE int frame_completed(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    FRAME_HEAD *frame = &priv->frame_head;
    gbuffer_t *gbuf = 0;

    if(frame->frame_length) {
        gbuf = istream_pop_gbuffer(priv->istream_payload);
        istream_destroy(priv->istream_payload);
        priv->istream_payload = 0;
    }

    int ret = 0;

    switch(frame->command) {
        case CMD_PINGREQ:
            ret = handle__pingreq(gobj);        // common to server/client
            break;
        case CMD_PINGRESP:
            ret = handle__pingresp(gobj);       // common to server/client
            break;
        case CMD_PUBACK:
            ret = handle__pubackcomp(gobj, gbuf, "PUBACK"); // common to server/client
            break;
        case CMD_PUBCOMP:
            ret = handle__pubackcomp(gobj, gbuf, "PUBCOMP"); // common to server/client
            break;
        case CMD_PUBLISH: // NOT common to server/client
            if(priv->iamServer) {
                ret = handle__publish_s(gobj, gbuf);
            } else {
                ret = handle__publish_c(gobj, gbuf);
            }
            break;
        case CMD_PUBREC:
            ret = handle__pubrec(gobj, gbuf);   // common to server/client
            break;
        case CMD_PUBREL:
            ret = handle__pubrel(gobj, gbuf);   // common to server/client
            break;
        case CMD_DISCONNECT: // NOT common to server/client TODO
            if(priv->iamServer) {
                ret = handle__disconnect_s(gobj, gbuf);
            } else {
                ret = handle__disconnect_c(gobj, gbuf);
            }
            break;
        case CMD_AUTH: // NOT common to server/client TODO
            if(priv->iamServer) {
                ret = handle__auth_s(gobj, gbuf);
            } else {
                ret = handle__auth_c(gobj, gbuf);
            }
        break;

        /*
         *  If server only with BRIDGE
         */
        case CMD_CONNACK:
            ret = handle__connack(gobj, gbuf);  // NOT common to server/client TODO
            break;
        case CMD_SUBACK:
            ret = handle__suback(gobj, gbuf);   // common to server/client
            break;
        case CMD_UNSUBACK:
            ret = handle__unsuback(gobj, gbuf); // common to server/client
            break;

        /*
         *  Only Server
         */
        case CMD_CONNECT:
            ret = handle__connect(gobj, gbuf);
            break;
        case CMD_SUBSCRIBE:
            ret = handle__subscribe(gobj, gbuf);
            break;
        case CMD_UNSUBSCRIBE:
            ret  = handle__unsubscribe(gobj, gbuf);
            break;

        case CMD_RESERVED:
            ret = MOSQ_ERR_PROTOCOL;
            break;
    }

    GBUFFER_DECREF(gbuf);

    if(frame->command != CMD_CONNECT && priv->protocol_version == mosq_p_mqtt5) {
        if(ret == MOSQ_ERR_PROTOCOL || ret == MOSQ_ERR_DUPLICATE_PROPERTY) {
            send_disconnect(gobj, MQTT_RC_PROTOCOL_ERROR, NULL);
        } else if(ret == MOSQ_ERR_MALFORMED_PACKET) {
            send_disconnect(gobj, MQTT_RC_MALFORMED_PACKET, NULL);
        } else if(ret == MOSQ_ERR_QOS_NOT_SUPPORTED) {
            send_disconnect(gobj, MQTT_RC_QOS_NOT_SUPPORTED, NULL);
        } else if(ret == MOSQ_ERR_RETAIN_NOT_SUPPORTED) {
            send_disconnect(gobj, MQTT_RC_RETAIN_NOT_SUPPORTED, NULL);
        } else if(ret == MOSQ_ERR_TOPIC_ALIAS_INVALID) {
            send_disconnect(gobj, MQTT_RC_TOPIC_ALIAS_INVALID, NULL);
        } else if(ret == MOSQ_ERR_UNKNOWN || ret == MOSQ_ERR_NOMEM) {
            send_disconnect(gobj, MQTT_RC_UNSPECIFIED, NULL);
        } else if(ret<0) {
            send_disconnect(gobj, MQTT_RC_PROTOCOL_ERROR, NULL);
        }
    }

    start_wait_frame_header(gobj);
    return ret;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  iam client. send the request
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_reset_volatil_attrs(gobj);
    start_wait_frame_header(gobj);
    priv->send_disconnect = FALSE;
    gobj_write_bool_attr(gobj, "connected", TRUE);
    GBUFFER_DECREF(priv->gbuf_will_payload);
    priv->jn_alias_list = json_object();

    if (priv->iamServer) {
        /*
         * wait the request
         */
    } else {
        /*
         * send the request
         */
    }
    set_timeout(priv->timer, gobj_read_integer_attr(gobj, "timeout_handshake"));
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_client_disconnected(gobj);

    JSON_DECREF(priv->jn_alias_list);

    gobj_reset_volatil_attrs(gobj);
    GBUFFER_DECREF(priv->gbuf_will_payload);

    if(gobj_is_volatil(src)) {
        gobj_set_bottom_gobj(gobj, 0);
    }

    if(priv->istream_payload) {
        istream_destroy(priv->istream_payload);
        priv->istream_payload = 0;
    }
    if (priv->must_broadcast_on_close) {
        priv->must_broadcast_on_close = FALSE;

        json_t *kw = json_pack("s:s",
            "client_id", priv->client_id
        );
        gobj_publish_event(gobj, EV_ON_CLOSE, kw);
    }
    if(priv->timer) {
        clear_timeout(priv->timer);
    }

    JSON_DECREF(priv->jn_alias_list)

    gobj_write_str_attr(gobj, "client_id", "");
    gobj_write_str_attr(gobj, "username", "");
    gobj_write_bool_attr(gobj, "connected", FALSE);

    dl_flush(&priv->dl_msgs_in, db_free_client_msg);
    dl_flush(&priv->dl_msgs_out, db_free_client_msg);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Child stopped
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Too much time waiting disconnected
 ***************************************************************************/
PRIVATE int ac_timeout_waiting_disconnected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_log_warning(gobj, 0,
        "msgset",       "%s", MSGSET_MQTT_ERROR,
        "msg",          "%s", "Timeout waiting mqtt disconnected",
        NULL
    );

    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Process the header.
 ***************************************************************************/
PRIVATE int ac_process_frame_header(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);
    FRAME_HEAD *frame = &priv->frame_head;
    istream_h istream = priv->istream_frame;

    if(gobj_trace_level(gobj) & TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "HEADER %s <== %s",
            gobj_short_name(gobj),
            gobj_short_name(src)
        );
    }

    if(priv->pingT>0) {
        set_timeout(priv->timer, priv->pingT);
    }

    while(gbuffer_leftbytes(gbuf)) {
        size_t ln = gbuffer_leftbytes(gbuf);
        char *bf = gbuffer_cur_rd_pointer(gbuf);
        int n = framehead_consume(gobj, frame, istream, bf, ln);
        if (n <= 0) {
            // Some error in parsing
            // on error do break the connection
            ws_close(gobj, MQTT_RC_PROTOCOL_ERROR);
            break;
        } else if (n > 0) {
            gbuffer_get(gbuf, n);  // take out the bytes consumed
        }

        if(frame->header_complete) {
            if(gobj_trace_level(gobj) & SHOW_DECODE) {
                trace_msg0("👈👈rx COMMAND=%s (%d), FRAME_LEN=%d",
                    get_command_name(frame->command),
                    (int)frame->command,
                    (int)frame->frame_length
                );
            }
            if(frame->frame_length) {
                /*
                 *
                 */
                if(priv->istream_payload) {
                    istream_destroy(priv->istream_payload);
                    priv->istream_payload = 0;
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                        "msg",          "%s", "istream_payload NOT NULL",
                        NULL
                    );
                }

                /*
                 *  Creat a new buffer for payload data
                 */
                size_t frame_length = frame->frame_length;
                if(!frame_length) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MEMORY_ERROR,
                        "msg",          "%s", "no memory for istream_payload",
                        "frame_length", "%d", frame_length,
                        NULL
                    );
                    ws_close(gobj, MQTT_RC_PROTOCOL_ERROR);
                    break;
                }
                priv->istream_payload = istream_create(
                    gobj,
                    4*1024,
                    gbmem_get_maximum_block()
                );
                if(!priv->istream_payload) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MEMORY_ERROR,
                        "msg",          "%s", "no memory for istream_payload",
                        "frame_length", "%d", frame_length,
                        NULL
                    );
                    ws_close(gobj, MQTT_RC_PROTOCOL_ERROR);
                    break;
                }
                istream_read_until_num_bytes(priv->istream_payload, frame_length, 0);

                gobj_change_state(gobj, ST_WAITING_PAYLOAD_DATA);
                return gobj_send_event(gobj, EV_RX_DATA, kw, gobj);

            } else {
                if(frame_completed(gobj)<0) {
                    //priv->send_disconnect = TRUE;
                    ws_close(gobj, MQTT_RC_PROTOCOL_ERROR);
                    break;
                }
            }
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  No activity, send ping
 ***************************************************************************/
PRIVATE int ac_timeout_waiting_frame_header(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->pingT > 0) {
        set_timeout(priv->timer, priv->pingT);
        //ping(gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Get payload data
 ***************************************************************************/
PRIVATE int ac_process_payload_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);

    if(gobj_trace_level(gobj) & TRAFFIC_PAYLOAD) {
        gobj_trace_dump_gbuf(gobj, gbuf, "PAYLOAD %s <== %s (accumulated %lu)",
            gobj_short_name(gobj),
            gobj_short_name(src),
            (unsigned long)istream_length(priv->istream_payload)
        );
    }

    size_t bf_len = gbuffer_leftbytes(gbuf);
    char *bf = gbuffer_cur_rd_pointer(gbuf);

    int consumed = istream_consume(priv->istream_payload, bf, bf_len);
    if(consumed > 0) {
        gbuffer_get(gbuf, consumed);  // take out the bytes consumed
    }
    if(istream_is_completed(priv->istream_payload)) {
        int ret;
        if((ret=frame_completed(gobj))<0) {
            if(gobj_trace_level(gobj) & SHOW_DECODE) {
                trace_msg0("❌❌ Mqtt error, disconnect: %d", ret);
            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "Mqtt error, disconnect",
                    NULL
                );
                //log_debug_full_gbuf(0, gbuf, "Mqtt error, disconnect");
            }
            ws_close(gobj, MQTT_RC_PROTOCOL_ERROR);
            KW_DECREF(kw)
            return -1;
        }
    }
    if(gbuffer_leftbytes(gbuf)) {
        return gobj_send_event(gobj, EV_RX_DATA, kw, gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Too much time waiting payload data
 ***************************************************************************/
PRIVATE int ac_timeout_waiting_payload_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_MQTT_ERROR,
        "msg",          "%s", "Timeout waiting mqtt PAYLOAD data",
        NULL
    );

    ws_close(gobj, MOSQ_ERR_PROTOCOL);
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Send data
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*---------------------------------------------*
     *   Entry parameters
     *---------------------------------------------*/
    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", KW_REQUIRED);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(gobj_trace_level(gobj) & TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "%s, topic_name %s", gobj_short_name(gobj), topic_name);
    }
    char *payload = gbuffer_cur_rd_pointer(gbuf);
    int payloadlen = (int)gbuffer_leftbytes(gbuf);
    // These parameters are fixed by now
    int qos = 0; // Only let 0
    BOOL retain = FALSE;
    json_t * properties = 0;

    // Local variables
    json_t *outgoing_properties = NULL;
    size_t tlen = 0;
    uint32_t remaining_length;

    if(priv->protocol_version != mosq_p_mqtt5 && properties) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt properties and not mqtt5",
            NULL
        );
        KW_DECREF(kw)
        return MOSQ_ERR_NOT_SUPPORTED;
    }

//    if(properties) {
//        if(properties->client_generated) {
//            outgoing_properties = properties;
//        } else {
//            memcpy(&local_property, properties, sizeof(mosquitto_property));
//            local_property.client_generated = TRUE;
//            local_property.next = NULL;
//            outgoing_properties = &local_property;
//        }
//        int rc = mosquitto_property_check_all(CMD_PUBLISH, outgoing_properties);
//        if(rc) return rc;
//    }

    tlen = strlen(topic_name);
    if(mosquitto_validate_utf8(topic_name, (int)tlen)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt malformed utf8",
            NULL
        );
        KW_DECREF(kw)
        return MOSQ_ERR_MALFORMED_UTF8;
    }
    if(payloadlen < 0 || payloadlen > (int)MQTT_MAX_PAYLOAD) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt payload size",
            NULL
        );
        KW_DECREF(kw)
        return MOSQ_ERR_PAYLOAD_SIZE;
    }
    if(mosquitto_pub_topic_check(topic_name) != MOSQ_ERR_SUCCESS) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt topic check failed",
            NULL
        );
        KW_DECREF(kw)
        return MOSQ_ERR_INVAL;
    }

    if(priv->maximum_packet_size > 0) {
        remaining_length = 1 + 2+(uint32_t)tlen + (uint32_t)payloadlen +
            property_get_length_all(outgoing_properties);
        if(qos > 0) {
            remaining_length++;
        }
        if(packet_check_oversize(gobj, remaining_length)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt oversize packet",
                NULL
            );
            KW_DECREF(kw)
            return MOSQ_ERR_OVERSIZE_PACKET;
        }
    }

    uint16_t mid = mosquitto__mid_generate(gobj, priv->client_id);
    json_object_set_new(kw, "mid", json_integer(mid));

    send_publish(
        gobj,
        mid,
        topic_name,
        (uint32_t)payloadlen,
        payload,
        (uint8_t)qos,
        retain,
        FALSE,
        outgoing_properties,
        NULL,
        0
    );

// TODO esto en el nivel superior
//    /*
//     *  Search subscriptions in clients
//     */
//    json_t *jn_subscribers = sub_get_subscribers(gobj, topic_name);
//
//    const char *client_id; json_t *client;
//    json_object_foreach(jn_subscribers, client_id, client) {
//        json_t *jn_subscriptions = kw_get_dict(gobj, client, "subscriptions", 0, KW_REQUIRED);
//        if(json_object_size(jn_subscriptions)==0) {
//            continue;
//        }
//
//        BOOL isConnected = kw_get_bool(gobj, client, "isConnected", 0, KW_REQUIRED);
//        if(isConnected) {
//            hgobj gobj_client = (hgobj)(size_t)kw_get_int(gobj, client, "_gobj", 0, KW_REQUIRED);
//            if(gobj_client) {
//                gobj_send_event(gobj_client, EV_SEND_MESSAGE, 0, gobj);
//            }
//        } else {
//            // TODO save the message if qos > 0 ?
//        }
//    }
//    JSON_DECREF(jn_subscribers)

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_PROT_MQTT);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/


/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_disconnected[] = {
        {EV_CONNECTED,        ac_connected,                       ST_WAITING_FRAME_HEADER},
        {EV_DISCONNECTED,     ac_disconnected,                    0},
        {EV_TIMEOUT,          ac_timeout_waiting_disconnected,    0},
        {EV_STOPPED,          ac_stopped,                         0},
        {EV_TX_READY,         0,                                  0},
        {0,0,0}
    };
    ev_action_t st_waiting_frame_header[] = {
        {EV_RX_DATA,          ac_process_frame_header,            0},
        {EV_SEND_MESSAGE,     ac_send_message,                    0},
        {EV_DISCONNECTED,     ac_disconnected,                    ST_DISCONNECTED},
        {EV_TIMEOUT,          ac_timeout_waiting_frame_header,    0},
        {EV_DROP,             ac_drop,                            0},
        {EV_TX_READY,         0,                                  0},
        {0,0,0}
    };
    ev_action_t st_waiting_payload_data[] = {
        {EV_RX_DATA,          ac_process_payload_data,            0},
        {EV_SEND_MESSAGE,     ac_send_message,                    0},
        {EV_DISCONNECTED,     ac_disconnected,                    ST_DISCONNECTED},
        {EV_TIMEOUT,          ac_timeout_waiting_payload_data,    0},
        {EV_DROP,             ac_drop,                            0},
        {EV_TX_READY,         0,                                  0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_DISCONNECTED,           st_disconnected},
        {ST_WAITING_FRAME_HEADER,   st_waiting_frame_header},
        {ST_WAITING_PAYLOAD_DATA,   st_waiting_payload_data},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_RX_DATA,            0},
        {EV_SEND_MESSAGE,       0},
        {EV_TX_READY,           0},
        {EV_TIMEOUT,            0},
        {EV_CONNECTED,          0},
        {EV_DISCONNECTED,       0},
        {EV_STOPPED,            0},
        {EV_DROP,               0},

        {EV_ON_OPEN,            EVF_OUTPUT_EVENT},
        {EV_ON_CLOSE,           EVF_OUTPUT_EVENT},
        {EV_ON_MESSAGE,         EVF_OUTPUT_EVENT},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        command_table,
        s_user_trace_level,
        0   // gcflag_t
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    /*----------------------------------------*
     *          Register comm protocol
     *----------------------------------------*/
    comm_prot_register(gclass_name, "mqtt");
    comm_prot_register(gclass_name, "mqtts");

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_prot_mqtt(void)
{
    return create_gclass(C_PROT_MQTT);
}
