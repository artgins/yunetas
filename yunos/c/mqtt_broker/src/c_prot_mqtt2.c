/***********************************************************************
 *          C_MQTT.C
 *          GClass of MQTT protocol.
 *
 *          This is an adaptation of mosquitto logic to Yunetas'GClass and TreeDB/Timeranger
 *
 *          Copyright (c) 2009-2020 Roger Light <roger@atchoo.org>
 *
 *          This file includes code from the Mosquitto project.
 *          It is made available under the terms of the Eclipse Public License 2.0
 *          and the Eclipse Distribution License v1.0 which accompany this distribution.
 *
 *          EPL: https://www.eclipse.org/legal/epl-2.0/
 *          EDL: https://www.eclipse.org/org/documents/edl-v10.php
 *
 *          SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 *
 *          Modifications:
 *              2025 ArtGins – refactoring, added new functions, structural changes.
 *
 *          Copyright (c) 2025 ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdint.h>
#include <endian.h>
#include <time.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include "tr2_queue.h"

#ifdef __linux__
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
#endif

#ifdef ESP_PLATFORM
#include "c_esp_transport.h"
#endif

#include "command_parser.h"
#include "msg_ievent.h"
#include "c_timer.h"
#include "c_tcp.h"
#include "istream.h"
#include "tr2q_mqtt.h"
#include "c_prot_mqtt2.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define MOSQ_MSB(A) (uint8_t)((A & 0xFF00) >> 8)
#define MOSQ_LSB(A) (uint8_t)(A & 0x00FF)

#define UNUSED(A) (void)(A)

#define SAFE_PRINT(A) (A)?(A):""

#define MQTT_MAX_PAYLOAD 268435455U

/* Error values */
typedef enum mosq_err_t {
    MOSQ_ERR_AUTH_CONTINUE = -44,
    MOSQ_ERR_NO_SUBSCRIBERS = -43,
    MOSQ_ERR_SUB_EXISTS = -42,
    MOSQ_ERR_CONN_PENDING = -41,
    MOSQ_ERR_SUCCESS = 0,
    MOSQ_ERR_NOMEM = -1,
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
} mosq_err_t;


/* MQTT specification restricts client ids to a maximum of 23 characters */
#define MOSQ_MQTT_ID_MAX_LENGTH 23


/***************************************************************************
 *              Structures
 ***************************************************************************/

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
typedef struct mosquitto_message {
    int mid;
    char *topic;
    gbuffer_t *payload;
    int qos;
    BOOL retain;
} mosquitto_message_t;

typedef struct mosquitto_msg_store { // Used in broker
    DL_ITEM_FIELDS

    // dbid_t db_id;
    char *source_id;
    char *source_username;
    // struct mosquitto__listener *source_listener;
    char **dest_ids;
    int dest_id_count;
    int ref_count;
    char *topic;
    json_t *properties;
    gbuffer_t *payload;
    time_t message_expiry_time;
    enum mqtt_msg_origin origin;
    uint16_t source_mid;
    uint16_t mid;
    uint8_t qos;
    BOOL retain;
} mosquitto_msg_store_t;

typedef struct mosquitto_client_msg { // Used in broker
    DL_ITEM_FIELDS

    struct mosquitto_msg_store *store;
    json_t *properties;
    time_t timestamp;
    uint16_t mid;
    uint8_t qos;
    BOOL retain;
    enum mqtt_msg_direction direction;
    enum mqtt_msg_state state;
    uint8_t dup;
} mosquitto_client_msg_t;

typedef struct mosquitto_message_all { // Used in client
    DL_ITEM_FIELDS

    json_t *properties;
    time_t timestamp;
    enum mqtt_msg_state state;
    BOOL dup;
    struct mosquitto_message msg;
    uint32_t expiry_interval;
} mosquitto_message_all_t;

typedef struct mosquitto_msg_data { // Used in client/broker
    // Used in broker
    dl_list_t dl_inflight2;    // struct mosquitto_client_msg *inflight;
    dl_list_t dl_queued;       // struct mosquitto_client_msg *queued;
    long inflight_bytes;
    long inflight_bytes12;
    int inflight_count;
    int inflight_count12;
    size_t queued_bytes;
    size_t queued_bytes12;
    int queued_count;
    int queued_count12;

    // Used in client
    dl_list_t dl_inflight;         // struct mosquitto_message_all *inflight;
    int queue_len;

    int inflight_quota;
    uint16_t inflight_maximum;
} mosquitto_msg_data_t;


typedef struct _FRAME_HEAD {
    // Information of the first two bytes header
    mqtt_message_t command; // byte1 & 0xF0;
    uint8_t flags;          // byte1 & 0x0F; (command&0x0F in mosquitto)

    // state of frame
    char busy;              // in half of header
    char header_complete;   // Set True when header is completed

    // must do
    char must_read_remaining_length_2;  // byte2 & 0x80
    char must_read_remaining_length_3;
    char must_read_remaining_length_4;

    size_t frame_length;    // byte2 & 0x7F;
} FRAME_HEAD;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void restore_client_attributes(hgobj gobj);
PRIVATE void message__cleanup_all(hgobj gobj);
PRIVATE int send__publish(
    hgobj gobj,
    uint16_t mid,
    const char *topic,
    gbuffer_t *payload,
    uint8_t qos,
    BOOL retain,
    BOOL dup,
    json_t *cmsg_props, // not owned
    json_t *store_props, // not owned
    uint32_t expiry_interval
);
PRIVATE int send__suback(
    hgobj gobj,
    uint16_t mid,
    gbuffer_t *gbuf_payload,  // owned
    json_t *properties  // owned
);
PRIVATE int send__unsuback(
    hgobj gobj,
    uint16_t mid,
    gbuffer_t *gbuf_payload,  // owned
    json_t *properties  // owned
);
PRIVATE int send__pubrel(
    hgobj gobj,
    uint16_t mid,
    json_t *properties
);
PRIVATE int send__pubrec(
    hgobj gobj,
    uint16_t mid,
    uint8_t reason_code,
    json_t *properties
);
PRIVATE int send__disconnect(
    hgobj gobj,
    uint8_t reason_code,
    json_t *properties
);
PRIVATE void do_disconnect(hgobj gobj, int reason);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
static const json_desc_t json_mqtt_desc[] = {
// Name             Type        Defaults    Fillspace
{"topic",           "string",   "",         "40"},  // the mqtt 'topic'. First item is the pkey
{"tm",              "time",     "",         "20"},  // timestamp
{"mid",             "int",      "",         "20"},
{"qos",             "int",      "",         "20"},
{"expiry_interval", "int",      "",         "20"},
{"retain",          "bool",     "",         "20"},
{"properties",      "dict",     "{}",       "20"},
{"gbuffer",         "int",      "",         "20"},  // payload

// these are volatiles
{"dup",             "bool",     "",         "20"},
{"state",           "int",      "",         "20"},
{0}
};

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

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "level=1: search in bottoms, level=2: search in all childs"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name------------flag------------alias---items-----------json_fn---------description---------- */
SDATACM2(DTP_SCHEMA,    "help",         0,              a_help, pm_help,        cmd_help,       "Command's help"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag--------default-description---------- */
SDATA (DTP_BOOLEAN,     "iamServer",        SDF_RD,     0,      "What side? server or client"),
SDATA (DTP_POINTER,     "tranger_queues",   0,          0,      "Queues TimeRanger for mqtt messages with qos > 0"),
SDATA (DTP_STRING,      "alert_message",    SDF_RD,     "ALERT Queuing", "Alert message"),
SDATA (DTP_INTEGER,     "max_pending_acks", SDF_RD,     "10000",    "Maximum messages pending of ack"),
SDATA (DTP_INTEGER,     "backup_queue_size",SDF_RD,     "1000000",  "Do backup at this size"),
SDATA (DTP_INTEGER,     "alert_queue_size", SDF_RD,     "2000",     "Limit alert queue size"),
SDATA (DTP_INTEGER,     "timeout_ack",      SDF_RD,     "60",       "Timeout ack in seconds"),

// HACK set by c_authz, this gclass is an external entry gate!
SDATA (DTP_STRING,      "__username__",     SDF_VOLATIL,"",     "Username, WARNING set by c_authz"),
SDATA (DTP_STRING,      "__session_id__",   SDF_VOLATIL,"",     "Session ID, WARNING set by c_authz"),
SDATA (DTP_JSON,        "jwt_payload",      SDF_VOLATIL,0,      "JWT payload (decoded user data) of authenticated user, WARNING set by c_authz"),

/*
 *  Used by mqtt client
 */
SDATA (DTP_STRING,      "mqtt_client_id",   SDF_RD,     "",     "MQTT Client id, used by mqtt client"),
SDATA (DTP_STRING,      "mqtt_protocol",    SDF_RD,     "mqttv5", "MQTT Protocol. Can be mqttv5, mqttv311 or mqttv31. Defaults to mqttv5."),
SDATA (DTP_STRING,      "mqtt_clean_session",0,         "1",            "MQTT clean_session. Default 1. Set to 0 enable persistent mode and the client id must be set. The broker will be instructed not to clean existing sessions for the same client id when the client connects, and sessions will never expire when the client disconnects. MQTT v5 clients can change their session expiry interval"),

SDATA (DTP_STRING,      "mqtt_session_expiry_interval",0,"-1",          "MQTT session expiry interval.  This option allows the session of persistent clients (those with clean session set to false) that are not currently connected to be removed if they do not reconnect within a certain time frame. This is a non-standard option in MQTT v3.1. MQTT v3.1.1 and v5.0 allow brokers to remove client sessions.\n"
"Badly designed clients may set clean session to false whilst using a randomly generated client id. This leads to persistent clients that connect once and never reconnect. This option allows these clients to be removed. This option allows persistent clients (those with clean session set to false) to be removed if they do not reconnect within a certain time frame.\nAs this is a non-standard option, the default if not set is to never expire persistent clients."),

SDATA (DTP_STRING,      "mqtt_keepalive",   0,         "60",    "MQTT keepalive. The number of seconds between sending PING commands to the broker for the purposes of informing it we are still connected and functioning. Defaults to 60 seconds."),

SDATA (DTP_STRING,      "user_id",          0,          "",     "MQTT Username or OAuth2 User Id (interactive jwt)"),
SDATA (DTP_STRING,      "user_passw",       0,          "",     "MQTT Password or OAuth2 User password (interactive jwt)"),
SDATA (DTP_STRING,      "jwt",              0,          "",     "Jwt"),

/*
 *  Configuration
 */
SDATA (DTP_STRING,      "url",              SDF_RD,     "",     "Url to connect"),
SDATA (DTP_STRING,      "cert_pem",         SDF_RD,     "",     "SSL server certificate, PEM format"),
SDATA (DTP_INTEGER,     "timeout_handshake",SDF_RD,     "5000",  "Timeout to handshake"),
SDATA (DTP_INTEGER,     "timeout_payload",  SDF_RD,     "5000",  "Timeout to payload"),
SDATA (DTP_INTEGER,     "timeout_close",    SDF_RD,     "3000",  "Timeout to close"),
SDATA (DTP_INTEGER,     "pingT",            SDF_RD,     "0",    "Ping interval. If value <= 0 then No ping"),

/*
 *  Configuration
 */

SDATA (DTP_INTEGER,     "max_inflight_bytes",SDF_WR,        0,      "Outgoing QoS 1 and 2 messages will be allowed in flight until this byte limit is reached. This allows control of outgoing message rate based on message size rather than message count. If the limit is set to 100, messages of over 100 bytes are still allowed, but only a single message can be in flight at once. Defaults to 0. (No limit)."),

SDATA (DTP_INTEGER,     "max_inflight_messages",SDF_WR,     "20",   "The maximum number of outgoing QoS 1 or 2 messages that can be in the process of being transmitted simultaneously. This includes messages currently going through handshakes and messages that are being retried. Defaults to 20. Set to 0 for no maximum. If set to 1, this will guarantee in-order delivery of messages"),

SDATA (DTP_INTEGER,     "max_queued_bytes", SDF_WR,         0,      "The number of outgoing QoS 1 and 2 messages above those currently in-flight will be queued (per client) by the broker. Once this limit has been reached, subsequent messages will be silently dropped. This is an important option if you are sending messages at a high rate and/or have clients who are slow to respond or may be offline for extended periods of time. Defaults to 0. (No maximum).See also the max_queued_messages option. If both max_queued_messages and max_queued_bytes are specified, packets will be queued until the first limit is reached."),

SDATA (DTP_INTEGER,     "max_queued_messages",SDF_WR,       "1000",   "The maximum number of QoS 1 or 2 messages to hold in the queue (per client) above those messages that are currently in flight. Defaults to 1000. Set to 0 for no maximum (not recommended). See also the queue_qos0_messages and max_queued_bytes options."),

SDATA (DTP_INTEGER,     "message_size_limit",SDF_WR,        0,      "This option sets the maximum publish payload size that the broker will allow. Received messages that exceed this size will not be accepted by the broker. This means that the message will not be forwarded on to subscribing clients, but the QoS flow will be completed for QoS 1 or QoS 2 messages. MQTT v5 clients using QoS 1 or QoS 2 will receive a PUBACK or PUBREC with the 'implementation specific error' reason code. The default value is 0, which means that all valid MQTT messages are accepted. MQTT imposes a maximum payload size of 268435455 bytes."),

SDATA (DTP_INTEGER,     "max_keepalive",    SDF_WR,         "65535",  "For MQTT v5 clients, it is possible to have the server send a 'server keepalive' value that will override the keepalive value set by the client. This is intended to be used as a mechanism to say that the server will disconnect the client earlier than it anticipated, and that the client should use the new keepalive value. The max_keepalive option allows you to specify that clients may only connect with keepalive less than or equal to this value, otherwise they will be sent a server keepalive telling them to use max_keepalive. This only applies to MQTT v5 clients. The maximum value allowable, and default value, is 65535. Set to 0 to allow clients to set keepalive = 0, which means no keepalive checks are made and the client will never be disconnected by the broker if no messages are received. You should be very sure this is the behaviour that you want.For MQTT v3.1.1 and v3.1 clients, there is no mechanism to tell the client what keepalive value they should use. If an MQTT v3.1.1 or v3.1 client specifies a keepalive time greater than max_keepalive they will be sent a CONNACK message with the 'identifier rejected' reason code, and disconnected."),

SDATA (DTP_INTEGER,     "max_packet_size",  SDF_WR,         0,      "For MQTT v5 clients, it is possible to have the server send a 'maximum packet size' value that will instruct the client it will not accept MQTT packets with size greater than value bytes. This applies to the full MQTT packet, not just the payload. Setting this option to a positive value will set the maximum packet size to that number of bytes. If a client sends a packet which is larger than this value, it will be disconnected. This applies to all clients regardless of the protocol version they are using, but v3.1.1 and earlier clients will of course not have received the maximum packet size information. Defaults to no limit. This option applies to all clients, not just those using MQTT v5, but it is not possible to notify clients using MQTT v3.1.1 or MQTT v3.1 of the limit. Setting below 20 bytes is forbidden because it is likely to interfere with normal client operation even with small payloads."),

SDATA (DTP_BOOLEAN,     "persistence",      SDF_WR,         "1",   "If TRUE, connection, subscription and message data will be written to the disk"), // TODO

SDATA (DTP_BOOLEAN,     "retain_available", SDF_WR,         "1",   "If set to FALSE, then retained messages are not supported. Clients that send a message with the retain bit will be disconnected if this option is set to FALSE. Defaults to TRUE."),

SDATA (DTP_INTEGER,     "max_qos",          SDF_WR,         "2",      "Limit the QoS value allowed for clients connecting to this listener. Defaults to 2, which means any QoS can be used. Set to 0 or 1 to limit to those QoS values. This makes use of an MQTT v5 feature to notify clients of the limitation. MQTT v3.1.1 clients will not be aware of the limitation. Clients publishing to this listener with a too-high QoS will be disconnected."),

SDATA (DTP_BOOLEAN,     "allow_zero_length_clientid",SDF_WR, "1",   "MQTT 3.1.1 and MQTT 5 allow clients to connect with a zero length client id and have the broker generate a client id for them. Use this option to allow/disallow this behaviour. Defaults to TRUE."),

SDATA (DTP_BOOLEAN,     "use_username_as_clientid",SDF_WR,  "0",  "Set use_username_as_clientid to TRUE to replace the clientid that a client connected with its username. This allows authentication to be tied to the clientid, which means that it is possible to prevent one client disconnecting another by using the same clientid. Defaults to FALSE."),

SDATA (DTP_INTEGER,     "max_topic_alias",  SDF_WR,         "10",     "This option sets the maximum number topic aliases that an MQTT v5 client is allowed to create. This option applies per listener. Defaults to 10. Set to 0 to disallow topic aliases. The maximum value possible is 65535."),

/*
 *  Dynamic Data
 */
SDATA (DTP_BOOLEAN,     "in_session",       SDF_VOLATIL|SDF_STATS,0,    "CONNECT mqtt done"),
SDATA (DTP_BOOLEAN,     "send_disconnect",  SDF_VOLATIL,        0,      "send DISCONNECT"),

SDATA (DTP_STRING,      "protocol_name",    SDF_VOLATIL,        0,      "Protocol name"),
SDATA (DTP_INTEGER,     "protocol_version", SDF_VOLATIL,        0,      "Protocol version"),
SDATA (DTP_BOOLEAN,     "is_bridge",        SDF_VOLATIL,        0,      "Connexion is a bridge"),
SDATA (DTP_BOOLEAN,     "assigned_id",      SDF_VOLATIL,        0,      "Auto client id"),
SDATA (DTP_STRING,      "client_id",        SDF_VOLATIL,        0,      "Client id"),

SDATA (DTP_BOOLEAN,     "clean_start",      SDF_VOLATIL,        0,      "New session"),
SDATA (DTP_INTEGER,     "session_expiry_interval",SDF_VOLATIL,  0,      "Session expiry interval in ?"),
SDATA (DTP_INTEGER,     "keepalive",        SDF_VOLATIL,        0,      "Keepalive"),
SDATA (DTP_STRING,      "auth_method",      SDF_VOLATIL,        0,      "Auth method"),
SDATA (DTP_STRING,      "auth_data",        SDF_VOLATIL,        0,      "Auth data (in base64)"),

SDATA (DTP_INTEGER,     "msgs_out_inflight_maximum", SDF_VOLATIL,0,     "Connect property"),
SDATA (DTP_INTEGER,     "msgs_out_inflight_quota", SDF_VOLATIL, 0,      "Connect property"),
SDATA (DTP_INTEGER,     "maximum_packet_size", SDF_VOLATIL,     0,      "Connect property"),

SDATA (DTP_BOOLEAN,     "will",             SDF_VOLATIL,        0,      "Will"),
SDATA (DTP_STRING,      "will_topic",       SDF_VOLATIL,        0,      "Will property"),
SDATA (DTP_BOOLEAN,     "will_retain",      SDF_VOLATIL,        0,      "Will retain"),
SDATA (DTP_INTEGER,     "will_qos",         SDF_VOLATIL,        0,      "QoS"),
SDATA (DTP_INTEGER,     "will_delay_interval", SDF_VOLATIL,     0,      "Will property"),
SDATA (DTP_INTEGER,     "will_expiry_interval",SDF_VOLATIL,     0,      "Will property"),

SDATA (DTP_POINTER,     "user_data",        0,                  0,      "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,      "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,      "subscriber of output-events. Default if null is parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRAFFIC                     = 0x0001,
    SHOW_DECODE                 = 0x0002,
    TRAFFIC_PAYLOAD             = 0x0004,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"traffic",         "Trace input/output data (without payload"},
{"show-decode",     "Print decode"},
{"traffic-payload", "Trace payload data"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    BOOL iamServer;         // What side? server or client
    int pingT;

    json_int_t timeout_handshake;
    json_int_t timeout_payload;
    json_int_t timeout_close;

    FRAME_HEAD frame_head;
    istream_h istream_frame;
    istream_h istream_payload;

    FRAME_HEAD message_head;

    BOOL inform_on_close;
    json_t *jn_alias_list;

    struct mosquitto_msg_data msgs_in; // TODO to remove, use tr2_queue instead
    struct mosquitto_msg_data msgs_out; // TODO to remove, use tr2_queue instead

    json_t *tranger_queues;
    tr2_queue_t *trq_cli_in_msgs;
    tr2_queue_t *trq_cli_out_msgs;
    tr2_queue_t *trq_srv_in_msgs;
    tr2_queue_t *trq_srv_out_msgs;

    /*
     *  Config
     */
    uint32_t max_inflight_bytes;
    uint32_t max_inflight_messages;
    uint32_t max_queued_bytes;
    uint32_t max_queued_messages;
    uint32_t max_keepalive;
    uint32_t max_packet_size;
    uint32_t message_size_limit;
    BOOL persistence;
    BOOL retain_available;
    uint32_t max_qos;
    BOOL allow_zero_length_clientid;
    BOOL use_username_as_clientid;
    uint32_t max_topic_alias;

    /*
     *  Dynamic data (reset per connection)
     */
    BOOL in_session;
    BOOL send_disconnect;
    const char *protocol_name;
    mosquitto_protocol_t protocol_version;
    BOOL is_bridge;
    BOOL assigned_id;
    const char *client_id;
    BOOL clean_start;
    uint32_t session_expiry_interval;
    uint32_t keepalive;
    const char *auth_method;
    const char *auth_data;
    uint32_t msgs_out_inflight_maximum;
    uint32_t msgs_out_inflight_quota;
    uint32_t maximum_packet_size;

    BOOL will;
    const char *will_topic;
    BOOL will_retain;
    uint32_t will_qos;
    uint32_t will_delay_interval;
    uint32_t will_expiry_interval;
    gbuffer_t *gbuf_will_payload;
    int out_packet_count;

    time_t db_now_s; // TODO
    time_t db_now_real_s; // TODO
    BOOL allow_duplicate_messages; // TODO
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

    dl_init(&priv->msgs_out.dl_inflight, gobj);
    dl_init(&priv->msgs_out.dl_queued, gobj);
    dl_init(&priv->msgs_in.dl_inflight, gobj);
    dl_init(&priv->msgs_in.dl_queued, gobj);

    priv->msgs_in.inflight_maximum = 20;
    priv->msgs_out.inflight_maximum = 20;
    priv->msgs_in.inflight_quota = 20;
    priv->msgs_out.inflight_quota = 20;

    // The maximum size of a frame header is 5 bytes.
    priv->istream_frame = istream_create(gobj, 5, 5);
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
    SET_PRIV(timeout_handshake,         gobj_read_integer_attr)
    SET_PRIV(timeout_payload,           gobj_read_integer_attr)
    SET_PRIV(timeout_close,             gobj_read_integer_attr)
    SET_PRIV(in_session,                gobj_read_bool_attr)
    SET_PRIV(send_disconnect,           gobj_read_bool_attr)

    SET_PRIV(max_inflight_bytes,        gobj_read_integer_attr)
    SET_PRIV(max_inflight_messages,     gobj_read_integer_attr)
    SET_PRIV(max_queued_bytes,          gobj_read_integer_attr)
    SET_PRIV(max_queued_messages,       gobj_read_integer_attr)
    SET_PRIV(max_keepalive,             gobj_read_integer_attr)
    SET_PRIV(max_packet_size,           gobj_read_integer_attr)
    SET_PRIV(message_size_limit,        gobj_read_integer_attr)
    SET_PRIV(persistence,               gobj_read_bool_attr)
    SET_PRIV(retain_available,          gobj_read_bool_attr)
    SET_PRIV(max_qos,                   gobj_read_integer_attr)
    SET_PRIV(allow_zero_length_clientid,gobj_read_bool_attr)
    SET_PRIV(use_username_as_clientid,  gobj_read_bool_attr)
    SET_PRIV(max_topic_alias,           gobj_read_integer_attr)

    SET_PRIV(protocol_name,             gobj_read_str_attr)
    SET_PRIV(protocol_version,          gobj_read_integer_attr)
    SET_PRIV(is_bridge,                 gobj_read_bool_attr)
    SET_PRIV(assigned_id,               gobj_read_bool_attr)
    SET_PRIV(client_id,                 gobj_read_str_attr)
    SET_PRIV(clean_start,               gobj_read_bool_attr)
    SET_PRIV(session_expiry_interval,   gobj_read_integer_attr)
    SET_PRIV(keepalive,                 gobj_read_integer_attr)
    SET_PRIV(auth_method,               gobj_read_str_attr)
    SET_PRIV(auth_data,                 gobj_read_str_attr)

    SET_PRIV(msgs_out_inflight_maximum, gobj_read_integer_attr)
    SET_PRIV(msgs_out_inflight_quota,   gobj_read_integer_attr)
    SET_PRIV(maximum_packet_size,       gobj_read_integer_attr)

    SET_PRIV(will,                      gobj_read_bool_attr)
    SET_PRIV(will_topic,                gobj_read_str_attr)
    SET_PRIV(will_retain,               gobj_read_bool_attr)
    SET_PRIV(will_qos,                  gobj_read_integer_attr)
    SET_PRIV(will_delay_interval,       gobj_read_integer_attr)
    SET_PRIV(will_expiry_interval,      gobj_read_integer_attr)

    SET_PRIV(tranger_queues,            gobj_read_pointer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(pingT,                       gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_handshake,         gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_payload,           gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_close,             gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(in_session,                gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(send_disconnect,           gobj_read_bool_attr)

    ELIF_EQ_SET_PRIV(max_inflight_bytes,        gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(max_inflight_messages,     gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(max_queued_bytes,          gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(max_queued_messages,       gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(max_keepalive,             gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(max_packet_size,           gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(message_size_limit,        gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(persistence,               gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(retain_available,          gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(max_qos,                   gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(allow_zero_length_clientid,gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(use_username_as_clientid,  gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(max_topic_alias,           gobj_read_integer_attr)

    ELIF_EQ_SET_PRIV(protocol_name,             gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(protocol_version,          gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(is_bridge,                 gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(assigned_id,               gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(client_id,                 gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(clean_start,               gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(session_expiry_interval,   gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(keepalive,                 gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(auth_method,               gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(auth_data,                 gobj_read_str_attr)

    ELIF_EQ_SET_PRIV(msgs_out_inflight_maximum, gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(msgs_out_inflight_quota,   gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(maximum_packet_size,       gobj_read_integer_attr)

    ELIF_EQ_SET_PRIV(will,                      gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(will_topic,                gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(will_retain,               gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(will_qos,                  gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(will_delay_interval,       gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(will_expiry_interval,      gobj_read_integer_attr)

    ELIF_EQ_SET_PRIV(tranger_queues,            gobj_read_pointer_attr)

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

    restore_client_attributes(gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

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

    JSON_DECREF(priv->jn_alias_list)

    message__cleanup_all(gobj);

    // TODO new dl_flush(&priv->dl_msgs_in, db_free_client_msg);
    // dl_flush(&priv->dl_msgs_out, db_free_client_msg);
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
PRIVATE void restore_client_attributes(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->iamServer) {
        return;
    }
    gobj_write_str_attr(gobj, "client_id", gobj_read_str_attr(gobj, "mqtt_client_id"));
}

/***************************************************************************
 *  HACK in queues will use only inflight messages
 *       out queues will use both inflight and queued messages
 ***************************************************************************/
PRIVATE int open_queues(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-----------------------------------*
     *  TODO        With persistence
     *-----------------------------------*/
    char topic_name [NAME_MAX];

    if(priv->iamServer) {
        /*
         *  In messages
         */
        snprintf(
            topic_name,
            sizeof(topic_name),
            "srv-queue-%s-in",
            priv->client_id
        );

        priv->trq_srv_in_msgs = tr2q_open(
            priv->tranger_queues,
            topic_name,
            "tm",
            0,  // system_flag
            0,  // max_inflight_messages
            gobj_read_integer_attr(gobj, "backup_queue_size")
        );

        // TODO tr2q_load(priv->trq_in_msgs);

        /*
         *  Out messages
         */
        snprintf(
            topic_name,
            sizeof(topic_name),
            "srv-queue-%s-out",
            priv->client_id
        );

        priv->trq_srv_out_msgs = tr2q_open(
            priv->tranger_queues,
            topic_name,
            "tm",
            0,  // system_flag
            0,  // max_inflight_messages
            gobj_read_integer_attr(gobj, "backup_queue_size")
        );

        // TODO tr2q_load(priv->trq_out_msgs);

    } else {
        /*
         *  In messages
         */
        snprintf(
            topic_name,
            sizeof(topic_name),
            "cli-queue-%s-in",
            priv->client_id
        );

        priv->trq_cli_in_msgs = tr2q_open(
            priv->tranger_queues,
            topic_name,
            "tm",
            0,  // system_flag
            0,  // max_inflight_messages
            gobj_read_integer_attr(gobj, "backup_queue_size")
        );

        // TODO tr2q_load(priv->trq_in_msgs);

        /*
         *  Out messages
         */
        snprintf(
            topic_name,
            sizeof(topic_name),
            "cli-queue-%s-out",
            priv->client_id
        );

        priv->trq_cli_out_msgs = tr2q_open(
            priv->tranger_queues,
            topic_name,
            "tm",
            0,  // system_flag
            0,  // max_inflight_messages
            gobj_read_integer_attr(gobj, "backup_queue_size")
        );

        // TODO tr2q_load(priv->trq_out_msgs);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void close_queues(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->iamServer) {
        EXEC_AND_RESET(tr2q_close, priv->trq_srv_in_msgs);
        EXEC_AND_RESET(tr2q_close, priv->trq_srv_out_msgs);
    } else {
        EXEC_AND_RESET(tr2q_close, priv->trq_cli_in_msgs);
        EXEC_AND_RESET(tr2q_close, priv->trq_cli_out_msgs);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE time_t mosquitto_time(void)
{
#ifdef WIN32
    return GetTickCount64()/1000;
#elif _POSIX_TIMERS>0 && defined(_POSIX_MONOTONIC_CLOCK)
    struct timespec tp;

    if (clock_gettime(time_clock, &tp) == 0)
        return tp.tv_sec;

    return (time_t) -1;
#elif defined(__APPLE__)
    static mach_timebase_info_data_t tb;
    uint64_t ticks;
    uint64_t sec;

    ticks = mach_absolute_time();

    if(tb.denom == 0) {
        mach_timebase_info(&tb);
    }
    sec = ticks*tb.numer/tb.denom/1000000000;

    return (time_t)sec;
#else
    return time(NULL);
#endif
}

/***************************************************************************
 * Used by client
 ***************************************************************************/
PRIVATE int message__out_update(
    hgobj gobj,
    uint16_t mid,
    enum mqtt_msg_state state,
    int qos
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    struct mosquitto_message_all *message, *tmp;

    DL_FOREACH_SAFE(&priv->msgs_out.dl_inflight, message, tmp) {
        if(message->msg.mid == mid) {
            if(message->msg.qos != qos) {
                return MOSQ_ERR_PROTOCOL;
            }
            message->state = state;
            message->timestamp = mosquitto_time();
            return MOSQ_ERR_SUCCESS;
        }
    }
    return MOSQ_ERR_NOT_FOUND;
}

/***************************************************************************
 *  Used by client
 ***************************************************************************/
PRIVATE int message__release_to_inflight(hgobj gobj, enum mqtt_msg_direction dir)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /* mosq->*_message_mutex should be locked before entering this function */
    struct mosquitto_message_all *cur, *tmp;
    int rc = MOSQ_ERR_SUCCESS;

    if(dir == mosq_md_out) {
        DL_FOREACH_SAFE(&priv->msgs_out.dl_inflight, cur, tmp) {
            if(priv->msgs_out.inflight_quota > 0 || TRUE) { // TODO management quotas!
                if(cur->msg.qos > 0 && cur->state == mosq_ms_invalid) {
                    if(cur->msg.qos == 1) {
                        cur->state = mosq_ms_wait_for_puback;
                    } else if(cur->msg.qos == 2) {
                        cur->state = mosq_ms_wait_for_pubrec;
                    }
                    rc = send__publish(
                        gobj,
                        (uint16_t)cur->msg.mid,
                        cur->msg.topic,
                        cur->msg.payload,
                        (uint8_t)cur->msg.qos,
                        cur->msg.retain,
                        cur->dup,
                        cur->properties,
                        NULL,
                        0
                    );
                    if(rc) {
                        return rc;
                    }
                    // TODO util__decrement_send_quota(mosq);
                }
            } else {
                return MOSQ_ERR_SUCCESS;
            }
        }
    }

    return rc;
}

/***************************************************************************
 *  Used by client
 ***************************************************************************/
PRIVATE int OLD_message__queue(
    hgobj gobj,
    struct mosquitto_message_all *message,
    enum mqtt_msg_direction dir
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(dir == mosq_md_out) {
        dl_add(&priv->msgs_out.dl_inflight, message);
        // TODO priv->msgs_out.queue_len++;
    } else {
        dl_add(&priv->msgs_in.dl_inflight, message);
        // TODO priv->msgs_in.queue_len++;
    }

    return message__release_to_inflight(gobj, dir);
}

/***************************************************************************
 *  Used by client and broker
 ***************************************************************************/
PRIVATE json_t *new_json_message(
    hgobj gobj,
    uint16_t mid,
    const char *topic,
    gbuffer_t *gbuf_payload, // not owned
    uint8_t qos,
    BOOL retain,
    BOOL dup,
    json_t *properties, // not owned
    uint32_t expiry_interval,
    mqtt_msg_origin_t origin,
    mqtt_msg_direction_t dir,
    user_flag_t *p_user_flag,
    json_int_t t
) {
    json_t *kw_mqtt_msg = create_json_record(gobj, json_mqtt_desc);
    if(!kw_mqtt_msg) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt publish: cannot create the message",
            "topic",        "%s", topic,
            NULL
        );
        return NULL;
    }

    json_object_set_new(kw_mqtt_msg, "topic", json_string(topic));
    json_object_set_new(kw_mqtt_msg, "tm", json_integer(t));
    json_object_set_new(kw_mqtt_msg, "qos", json_integer(qos));
    json_object_set_new(kw_mqtt_msg, "expiry_interval", json_integer(expiry_interval));
    json_object_set_new(kw_mqtt_msg, "retain", json_boolean(retain));
    if(properties) {
        json_object_set(kw_mqtt_msg, "properties", properties); // no new, owned by kw
    }
    if(gbuf_payload) {
        gbuffer_incref(gbuf_payload);
        json_object_set_new(
            kw_mqtt_msg,
            "gbuffer",
            json_integer((json_int_t)(uintptr_t)gbuf_payload)
        );
    }

    user_flag_t user_flag = {0};
    user_flag_set_origin(&user_flag, origin);
    user_flag_set_direction(&user_flag, dir);
    user_flag_set_qos_level(&user_flag, qos);
    user_flag_set_retain(&user_flag, retain);
    user_flag_set_dup(&user_flag, dup);
    user_flag_set_state(&user_flag, mosq_ms_invalid);

    *p_user_flag = user_flag;

    return kw_mqtt_msg;
}

/***************************************************************************
 *  Used by client
 ***************************************************************************/
PRIVATE int message__queue(
    hgobj gobj,
    json_t *kw_mqtt_msg,
    mqtt_msg_direction_t dir,
    user_flag_t user_flag,
    json_int_t t
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    q2_msg_t *qmsg = NULL;
    if(dir == mosq_md_out) {
        // dl_add(&priv->msgs_out.dl_inflight, message);
        qmsg = tr2q_append(
            priv->trq_cli_out_msgs,
            t,              // __t__ if 0 then the time will be set by TimeRanger with now time
            kw_mqtt_msg,    // owned
            user_flag.value // extra flags in addition to TRQ_MSG_PENDING
        );
        // TODO priv->msgs_out.queue_len++;

    } else {
        // dl_add(&priv->msgs_in.dl_inflight, message);
        qmsg = tr2q_append(
            priv->trq_cli_in_msgs,
            t,              // __t__ if 0 then the time will be set by TimeRanger with now time
            kw_mqtt_msg,    // owned
            user_flag.value // extra flags in addition to TRQ_MSG_PENDING
        );
        // TODO priv->msgs_in.queue_len++;
    }

    return message__release_to_inflight(gobj, dir);
}

/***************************************************************************
 *  Used by client:
 *      find the message 'mid',
 *      dequeue
 *      return in **message
 *  but don't free!
 ***************************************************************************/
PRIVATE int message__remove(
    hgobj gobj,
    uint16_t mid,
    enum mqtt_msg_direction dir,
    struct mosquitto_message_all **message,
    int qos
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    struct mosquitto_message_all *cur, *tmp;
    BOOL found = FALSE;

    if(dir == mosq_md_out) {
        DL_FOREACH_SAFE(&priv->msgs_out.dl_inflight, cur, tmp) {
            if(found == FALSE && cur->msg.mid == mid) {
                if(cur->msg.qos != qos) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MQTT_ERROR,
                        "msg",          "%s", "Mqtt client: message not match qos",
                        NULL
                    );
                    return MOSQ_ERR_PROTOCOL;
                }
                dl_delete(&priv->msgs_out.dl_inflight, cur, 0);

                *message = cur;
                // priv->msgs_out.queue_len--; TODO
                found = TRUE;
                break;
            }
        }
        if(found) {
            return MOSQ_ERR_SUCCESS;
        } else {
            return MOSQ_ERR_NOT_FOUND;
        }

    } else {
        DL_FOREACH_SAFE(&priv->msgs_in.dl_inflight, cur, tmp) {
            if(cur->msg.mid == mid) {
                if(cur->msg.qos != qos) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MQTT_ERROR,
                        "msg",          "%s", "Mqtt client: message not match qos",
                        NULL
                    );
                    return MOSQ_ERR_PROTOCOL;
                }
                dl_delete(&priv->msgs_in.dl_inflight, cur, 0);
                *message = cur;
                // TODO mosq->msgs_in.queue_len--;
                found = TRUE;
                break;
            }
        }

        if(found) {
            return MOSQ_ERR_SUCCESS;
        } else {
            return MOSQ_ERR_NOT_FOUND;
        }
    }
}

/***************************************************************************
 *  Used by client
 *  Free the message
 ***************************************************************************/
PRIVATE void message__cleanup(struct mosquitto_message_all *msg)
{
    GBMEM_FREE(msg->msg.topic);
    GBUFFER_DECREF(msg->msg.payload);
    JSON_DECREF(msg->properties);
    GBMEM_FREE(msg);
}

/***************************************************************************
 * Used by client
 ***************************************************************************/
PRIVATE void message__cleanup_all(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    struct mosquitto_message_all *tail, *tmp;

    DL_FOREACH_SAFE(&priv->msgs_in.dl_inflight, tail, tmp){
        dl_delete(&priv->msgs_in.dl_inflight, tail, 0);
        message__cleanup(tail);
    }
    DL_FOREACH_SAFE(&priv->msgs_out.dl_inflight, tail, tmp){
        dl_delete(&priv->msgs_out.dl_inflight, tail, 0);
        message__cleanup(tail);
    }
}

/***************************************************************************
 *  Used by client
 ***************************************************************************/
PRIVATE int message__delete(
    hgobj gobj,
    uint16_t mid,
    enum mqtt_msg_direction dir,
    int qos
) {
    struct mosquitto_message_all *message;

    int rc = message__remove(gobj, mid, dir, &message, qos);
    if(rc == MOSQ_ERR_SUCCESS) {
        message__cleanup(message);
    }
    return rc;
}

/***************************************************************************
 *  Enqueue message to subscribers
 ***************************************************************************/
PRIVATE int sub__messages_queue(
    hgobj gobj,
    json_t *kw_mqtt_msg
) {
    int rc = MOSQ_ERR_SUCCESS;
    // int rc_normal = MOSQ_ERR_NO_SUBSCRIBERS, rc_shared = MOSQ_ERR_NO_SUBSCRIBERS;
    // struct mosquitto__subhier *subhier;
    // char **split_topics = NULL;
    // char *local_topic = NULL;
    //
    // assert(topic);
    //
    // if(sub__topic_tokenise(topic, &local_topic, &split_topics, NULL)) return 1;
    //
    // /* Protect this message until we have sent it to all
    // clients - this is required because websockets client calls
    // db__message_write(), which could remove the message if ref_count==0.
    // */
    // db__msg_store_ref_inc(*stored);
    //
    // HASH_FIND(hh, db.normal_subs, split_topics[0], strlen(split_topics[0]), subhier);
    // if(subhier) {
    //     rc_normal = sub__search(subhier, split_topics, source_id, topic, qos, retain, *stored);
    //     if(rc_normal > 0) {
    //         rc = rc_normal;
    //         goto end;
    //     }
    // }
    //
    // HASH_FIND(hh, db.shared_subs, split_topics[0], strlen(split_topics[0]), subhier);
    // if(subhier) {
    //     rc_shared = sub__search(subhier, split_topics, source_id, topic, qos, retain, *stored);
    //     if(rc_shared > 0) {
    //         rc = rc_shared;
    //         goto end;
    //     }
    // }
    //
    // if(rc_normal == MOSQ_ERR_NO_SUBSCRIBERS && rc_shared == MOSQ_ERR_NO_SUBSCRIBERS) {
    //     rc = MOSQ_ERR_NO_SUBSCRIBERS;
    // }
    //
    // if(retain) {
    //     rc2 = retain__store(topic, *stored, split_topics);
    //     if(rc2) rc = rc2;
    // }
    //
    // end:
    //     mosquitto__free(split_topics);
    // mosquitto__free(local_topic);
    // /* Remove our reference and free if needed. */
    // db__msg_store_ref_dec(stored);

    return rc;
}


/***************************************************************************
 *  Using in context context__cleanup()
 ***************************************************************************/
PRIVATE int db__messages_delete(hgobj gobj, BOOL force_free)
{
    return -1;
}

/***************************************************************************
 * This will result in any outgoing packets going unsent. If we're disconnected
 * forcefully then it is usually an error condition and shouldn't be a problem,
 * but it will mean that CONNACK messages will never get sent for bad protocol
 * versions for example.
 ***************************************************************************/
PRIVATE void context__cleanup(hgobj gobj, BOOL force_free)
{
}

/***************************************************************************
 *  Using in handle__connack(), handle__connect()
 ***************************************************************************/
PRIVATE int db__message_reconnect_reset(hgobj gobj)
{
    return -1;
}

/***************************************************************************
 *  Using in handle__connect()
 ***************************************************************************/
PRIVATE void db__expire_all_messages(hgobj gobj)
{
}

/***************************************************************************
 *  Using in handle__connect() and handle__connack()
 ***************************************************************************/
PRIVATE int db__message_write_inflight_out_all(hgobj gobj)
{
    return -1;
}

/***************************************************************************
 *  Using in handle__connack(), handle__connect(), handle__subscribe, websocket
 ***************************************************************************/
PRIVATE int db__message_write_queued_out(hgobj gobj)
{
    return -1;
}

/***************************************************************************
 *  This function requires topic to be allocated on the heap.
 *  Once called, it owns topic and will free it on error.
 *  Likewise payload and properties.
 *  Using in handle__publish, loop, persist_read
 ***************************************************************************/
PRIVATE int db__message_store(
    hgobj gobj,
    struct mosquitto_msg_store *stored,
    uint32_t message_expiry_interval,
    int store_id, // TODO dbid_t store_id,
    enum mqtt_msg_origin origin
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // TODO
    // if(source && source->id) {
    //     stored->source_id = mosquitto__strdup(source->id);
    // } else {
    //     stored->source_id = mosquitto__strdup("");
    // }
    // if(!stored->source_id) {
    //     log__printf(NULL, MOSQ_LOG_ERR, "Error: Out of memory.");
    //     db__msg_store_free(stored);
    //     return MOSQ_ERR_NOMEM;
    // }
    //
    // if(source && source->username) {
    //     stored->source_username = mosquitto__strdup(source->username);
    //     if(!stored->source_username) {
    //         db__msg_store_free(stored);
    //         return MOSQ_ERR_NOMEM;
    //     }
    // }
    // if(source) {
    //     stored->source_listener = source->listener;
    // }
    // stored->mid = 0;
    // stored->origin = origin;
    // if(message_expiry_interval > 0) {
    //     stored->message_expiry_time = db.now_real_s + message_expiry_interval;
    // } else {
    //     stored->message_expiry_time = 0;
    // }
    //
    // stored->dest_ids = NULL;
    // stored->dest_id_count = 0;
    // db.msg_store_count++;
    // db.msg_store_bytes += stored->payloadlen;
    //
    // if(!store_id) {
    //     stored->db_id = ++db.last_db_id;
    // } else {
    //     stored->db_id = store_id;
    // }
    //
    // db__msg_store_add(stored);

    return MOSQ_ERR_SUCCESS;
}

/***************************************************************************
 *  Using in persist_read()
 ***************************************************************************/
PRIVATE void db__msg_add_to_queued_stats(
    struct mosquitto_msg_data *msg_data,
    struct mosquitto_client_msg *msg
) {
    msg_data->queued_count++;
    msg_data->queued_bytes += gbuffer_leftbytes(msg->store->payload);
    if(msg->qos != 0) {
        msg_data->queued_count12++;
        msg_data->queued_bytes12 += gbuffer_leftbytes(msg->store->payload);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void db__msg_remove_from_queued_stats(
    struct mosquitto_msg_data *msg_data,
    struct mosquitto_client_msg *msg
) {
    msg_data->queued_count--;
    msg_data->queued_bytes -= gbuffer_leftbytes(msg->store->payload);
    if(msg->qos != 0) {
        msg_data->queued_count12--;
        msg_data->queued_bytes12 -= gbuffer_leftbytes(msg->store->payload);
    }
}

/***************************************************************************
 *  Using in persist_read()
 ***************************************************************************/
PRIVATE void db__msg_add_to_inflight_stats(struct mosquitto_msg_data *msg_data, struct mosquitto_client_msg *msg)
{
    msg_data->inflight_count++;
    msg_data->inflight_bytes += gbuffer_leftbytes(msg->store->payload);
    if(msg->qos != 0) {
        msg_data->inflight_count12++;
        msg_data->inflight_bytes12 += gbuffer_leftbytes(msg->store->payload);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void db__msg_remove_from_inflight_stats(struct mosquitto_msg_data *msg_data, struct mosquitto_client_msg *msg)
{
    msg_data->inflight_count--;
    msg_data->inflight_bytes -= gbuffer_leftbytes(msg->store->payload);
    if(msg->qos != 0) {
        msg_data->inflight_count12--;
        msg_data->inflight_bytes12 -= gbuffer_leftbytes(msg->store->payload);
    }
}

/***************************************************************************
 *  Using in handle__publish()
 ***************************************************************************/
void db__msg_store_free(struct mosquitto_msg_store *store)
{
    // int i;
    //
    // mosquitto__free(store->source_id);
    // mosquitto__free(store->source_username);
    // if(store->dest_ids) {
    //     for(i=0; i<store->dest_id_count; i++) {
    //         mosquitto__free(store->dest_ids[i]);
    //     }
    //     mosquitto__free(store->dest_ids);
    // }
    gbmem_free(store->topic);
    // mosquitto_property_free_all(&store->properties);
    // mosquitto__free(store->payload);
    gbmem_free(store);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void db__msg_store_remove(struct mosquitto_msg_store *store)
{
    // TODO
    // if(store->prev) {
    //     store->prev->next = store->next;
    //     if(store->next) {
    //         store->next->prev = store->prev;
    //     }
    // } else {
    //     db.msg_store = store->next;
    //     if(store->next) {
    //         store->next->prev = NULL;
    //     }
    // }
    // db.msg_store_count--;
    // db.msg_store_bytes -= store->payloadlen;
    //
    db__msg_store_free(store);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void db__msg_store_ref_dec(struct mosquitto_msg_store **store)
{
    (*store)->ref_count--;
    if((*store)->ref_count == 0) {
        db__msg_store_remove(*store);
        *store = NULL;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void db__message_remove_from_inflight(
    hgobj gobj,
    tr2_queue_t *dl,
    q2_msg_t *qmsg
) {
    // TODO this must to delete from tr2q
    // dl_delete(dl, kw_mqtt_msg, 0);

    // TODO
    // if(item->store) {
    //     db__msg_remove_from_inflight_stats(msg_data, item);
    //     db__msg_store_ref_dec(&item->store);
    // }

    // kw_decref(kw_mqtt_msg);
}

/***************************************************************************
 * Is this context ready to take more in flight messages right now?
 * @param context the client context of interest
 * @param qos qos for the packet of interest
 * @return true if more in flight are allowed.
 ***************************************************************************/
PRIVATE BOOL db__ready_for_flight(hgobj gobj, enum mqtt_msg_direction dir, int qos)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    struct mosquitto_msg_data *msgs;
    BOOL valid_bytes;
    BOOL valid_count;

    if(dir == mosq_md_out) {
        msgs = &priv->msgs_out;
    } else {
        msgs = &priv->msgs_in;
    }

    if(msgs->inflight_maximum == 0 && priv->max_inflight_bytes == 0) {
        return TRUE;
    }

    if(qos == 0) {
        /* Deliver QoS 0 messages unless the queue is already full.
         * For QoS 0 messages the choice is either "inflight" or dropped.
         * There is no queueing option, unless the client is offline and
         * queue_qos0_messages is enabled.
         */
        if(priv->max_queued_messages == 0 && priv->max_inflight_bytes == 0) {
            return TRUE;
        }
        valid_bytes = ((msgs->inflight_bytes - (ssize_t)priv->max_inflight_bytes) < (ssize_t)priv->max_queued_bytes);
        if(dir == mosq_md_out) {
            valid_count = priv->out_packet_count < priv->max_queued_messages;
        } else {
            valid_count = msgs->inflight_count - msgs->inflight_maximum < priv->max_queued_messages;
        }

        if(priv->max_queued_messages == 0) {
            return valid_bytes;
        }
        if(priv->max_queued_bytes == 0) {
            return valid_count;
        }
    } else {
        valid_bytes = (ssize_t)msgs->inflight_bytes12 < (ssize_t)priv->max_inflight_bytes;
        valid_count = msgs->inflight_quota > 0;

        if(msgs->inflight_maximum == 0) {
            return valid_bytes;
        }
        if(priv->max_inflight_bytes == 0) {
            return valid_count;
        }
    }

    return valid_bytes && valid_count;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void db__message_dequeue_first(
    hgobj gobj,
    struct mosquitto_msg_data *msg_data
) {
    struct mosquitto_client_msg *msg;

    // TODO
    // msg = msg_data->queued;
    // DL_DELETE(msg_data->queued, msg);
    // DL_APPEND(msg_data->inflight, msg);
    // if(msg_data->inflight_quota > 0) {
    //     msg_data->inflight_quota--;
    // }

    db__msg_remove_from_queued_stats(msg_data, msg);
    db__msg_add_to_inflight_stats(msg_data, msg);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int db__message_write_inflight_out_single(
    hgobj gobj,
    struct mosquitto_client_msg *msg
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *cmsg_props = NULL, *store_props = NULL;
    int rc;
    uint16_t mid;
    int retries;
    int retain;
    const char *topic;
    uint8_t qos;
    uint32_t payloadlen;
    const void *payload;
    uint32_t expiry_interval;

    expiry_interval = 0;
    if(msg->store->message_expiry_time) {
        if(priv->db_now_real_s > msg->store->message_expiry_time) {
            /* Message is expired, must not send. */
            if(msg->direction == mosq_md_out && msg->qos > 0) {
                // TODO util__increment_send_quota(context);
            }
            db__message_remove_from_inflight(gobj, &priv->msgs_out, msg);
            return MOSQ_ERR_SUCCESS;
        } else {
            expiry_interval = (uint32_t)(msg->store->message_expiry_time - priv->db_now_real_s);
        }
    }
    mid = msg->mid;
    retries = msg->dup;
    retain = msg->retain;
    topic = msg->store->topic;
    qos = (uint8_t)msg->qos;
    cmsg_props = msg->properties;
    store_props = msg->store->properties;

    switch(msg->state) {
        case mosq_ms_publish_qos0:
            rc = send__publish(gobj, mid, topic, msg->store->payload, qos, retain, retries, cmsg_props, store_props, expiry_interval);
            if(rc == MOSQ_ERR_SUCCESS || rc == MOSQ_ERR_OVERSIZE_PACKET) {
                db__message_remove_from_inflight(gobj, &priv->msgs_out, msg);
            } else {
                return rc;
            }
            break;

        case mosq_ms_publish_qos1:
            rc = send__publish(gobj, mid, topic, msg->store->payload, qos, retain, retries, cmsg_props, store_props, expiry_interval);
            if(rc == MOSQ_ERR_SUCCESS) {
                msg->timestamp = priv->db_now_s;
                msg->dup = 1; /* Any retry attempts are a duplicate. */
                msg->state = mosq_ms_wait_for_puback;
            } else if(rc == MOSQ_ERR_OVERSIZE_PACKET) {
                db__message_remove_from_inflight(gobj, &priv->msgs_out, msg);
            } else {
                return rc;
            }
            break;

        case mosq_ms_publish_qos2:
            rc = send__publish(gobj, mid, topic, msg->store->payload, qos, retain, retries, cmsg_props, store_props, expiry_interval);
            if(rc == MOSQ_ERR_SUCCESS) {
                msg->timestamp = priv->db_now_s;
                msg->dup = 1; /* Any retry attempts are a duplicate. */
                msg->state = mosq_ms_wait_for_pubrec;
            } else if(rc == MOSQ_ERR_OVERSIZE_PACKET) {
                db__message_remove_from_inflight(gobj, &priv->msgs_out, msg);
            } else {
                return rc;
            }
            break;

        case mosq_ms_resend_pubrel:
            rc = send__pubrel(gobj, mid, NULL);
            if(!rc) {
                msg->state = mosq_ms_wait_for_pubcomp;
            } else {
                return rc;
            }
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
    return MOSQ_ERR_SUCCESS;
}

/***************************************************************************
 *  Using in handle__subscribe() and websockets
 ***************************************************************************/
PRIVATE int db__message_write_inflight_out_latest(hgobj gobj)
{
    struct mosquitto_client_msg *tail, *next;
    int rc;

    // TODO
    // if(context->state != mosq_cs_active
    //         || context->sock == INVALID_SOCKET
    //         || context->msgs_out.inflight == NULL) {
    //
    //     return MOSQ_ERR_SUCCESS;
    //         }
    //
    // if(context->msgs_out.inflight->prev == context->msgs_out.inflight) {
    //     /* Only one message */
    //     return db__message_write_inflight_out_single(context, context->msgs_out.inflight);
    // }
    //
    // /* Start at the end of the list and work backwards looking for the first
    //  * message in a non-publish state */
    // tail = context->msgs_out.inflight->prev;
    // while(tail != context->msgs_out.inflight &&
    //         (tail->state == mosq_ms_publish_qos0
    //          || tail->state == mosq_ms_publish_qos1
    //          || tail->state == mosq_ms_publish_qos2)) {
    //
    //     tail = tail->prev;
    //          }
    //
    // /* Tail is now either the head of the list, if that message is waiting for
    //  * publish, or the oldest message not waiting for a publish. In the latter
    //  * case, any pending publishes should be next after this message. */
    // if(tail != context->msgs_out.inflight) {
    //     tail = tail->next;
    // }
    //
    // while(tail) {
    //     next = tail->next;
    //     rc = db__message_write_inflight_out_single(context, tail);
    //     if(rc) return rc;
    //     tail = next;
    // }
    return MOSQ_ERR_SUCCESS;
}

/***************************************************************************
 *  Using in handle__pubrec()
 ***************************************************************************/
PRIVATE int db__message_update_outgoing(
    hgobj gobj,
    uint16_t mid,
    enum mqtt_msg_state state,
    int qos
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    struct mosquitto_client_msg *tail;

    DL_FOREACH(&priv->msgs_out.dl_inflight, tail) {
        if(tail->mid == mid) {
            if(tail->qos != qos) {
                return MOSQ_ERR_PROTOCOL;
            }
            tail->state = state;
            tail->timestamp = priv->db_now_s;
            return MOSQ_ERR_SUCCESS;
        }
    }
    return MOSQ_ERR_NOT_FOUND;
}

/***************************************************************************
 *  Using in handle__pubrec()
 ***************************************************************************/
PRIVATE int db__message_delete_outgoing(
    hgobj gobj,
    uint16_t mid,
    enum mqtt_msg_state expect_state,
    int qos
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    struct mosquitto_client_msg *tail, *tmp;

    DL_FOREACH_SAFE(&priv->msgs_out.dl_inflight, tail, tmp) {
        if(tail->mid == mid) {
            if(tail->qos != qos) {
                return MOSQ_ERR_PROTOCOL;
            } else if(qos == 2 && tail->state != expect_state) {
                return MOSQ_ERR_PROTOCOL;
            }
            db__message_remove_from_inflight(gobj, &priv->msgs_out, tail);
            break;
        }
    }

    DL_FOREACH_SAFE(&priv->msgs_out.dl_queued, tail, tmp) {
        if(!db__ready_for_flight(gobj, mosq_md_out, tail->qos)) {
            break;
        }

        // tail->timestamp = priv->db_now_s; TODO
        switch(tail->qos) {
            case 0:
                tail->state = mosq_ms_publish_qos0;
            break;
            case 1:
                tail->state = mosq_ms_publish_qos1;
            break;
            case 2:
                tail->state = mosq_ms_publish_qos2;
            break;
        }
        db__message_dequeue_first(gobj, &priv->msgs_out);
    }
#ifdef WITH_PERSISTENCE
    db.persistence_changes++;
#endif

    return db__message_write_inflight_out_latest(gobj);
}

/***************************************************************************
 *  Using in handle__publish_s()
 *  Check if a mid already exists in inflight queue and remove it
 ***************************************************************************/
PRIVATE int db__message_remove_incoming_dup(hgobj gobj, uint16_t mid)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    q2_msg_t *qmsg, *tmp;

    DL_FOREACH_SAFE(&priv->trq_srv_in_msgs->dl_inflight, qmsg, tmp) {
        if(qmsg->mid == mid) {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "removing an inflight qos2 dup message",
                "client_id",    "%s", SAFE_PRINT(priv->client_id),
                "mid",          "%d", (int)mid,
                NULL
            );
            db__message_remove_from_inflight(
                gobj,
                priv->trq_srv_in_msgs,
                qmsg
            );
            return MOSQ_ERR_SUCCESS;
        }
    }

    // Silence please
    return MOSQ_ERR_NOT_FOUND;
}

/***************************************************************************
 *  Using in handle__pubrel()
 ***************************************************************************/
PRIVATE int db__message_release_incoming(hgobj gobj, uint16_t mid)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    q2_msg_t *tail, *tmp;
    int retain;
    char *topic;
    char *source_id;
    BOOL deleted = FALSE;
    int rc;

    // DL_FOREACH_SAFE(&priv->trq_srv_in_msgs->dl_inflight, tail, tmp) {
    //     if(tail->mid == mid) {
    //         if(tail->store->qos != 2) {
    //             return MOSQ_ERR_PROTOCOL;
    //         }
    //         topic = tail->store->topic;
    //         retain = tail->retain;
    //         source_id = tail->store->source_id;
    //
    //         /* topic==NULL should be a QoS 2 message that was
    //          * denied/dropped and is being processed so the client doesn't
    //          * keep resending it. That means we don't send it to other
    //          * clients. */
    //         if(topic == NULL) {
    //             db__message_remove_from_inflight(gobj, &priv->msgs_in, tail);
    //             deleted = TRUE;
    //         } else {
    //             rc = sub__messages_queue(source_id, topic, 2, retain, &tail->store);
    //             if(rc == MOSQ_ERR_SUCCESS || rc == MOSQ_ERR_NO_SUBSCRIBERS) {
    //                 db__message_remove_from_inflight(gobj, &priv->msgs_in, tail);
    //                 deleted = TRUE;
    //             } else {
    //                 return 1;
    //             }
    //         }
    //     }
    // }
    //
    // DL_FOREACH_SAFE(&priv->msgs_in.dl_queued, tail, tmp) {
    //     if(db__ready_for_flight(gobj, mosq_md_in, tail->qos)) {
    //         break;
    //     }
    //
    //     tail->timestamp = priv->db_now_s;
    //
    //     if(tail->qos == 2) {
    //         send__pubrec(gobj, tail->mid, 0, NULL);
    //         tail->state = mosq_ms_wait_for_pubrel;
    //         db__message_dequeue_first(gobj, &priv->msgs_in);
    //     }
    // }
    if(deleted) {
        return MOSQ_ERR_SUCCESS;
    } else {
        return MOSQ_ERR_NOT_FOUND;
    }
}

/***************************************************************************
 *  Using in handle__publish(), loop, retain
 ***************************************************************************/
PRIVATE int db__message_insert(
    hgobj gobj,
    json_t *kw_mqtt_msg
    // uint16_t mid,
    // enum mqtt_msg_direction dir,
    // uint8_t qos,
    // BOOL retain,
    // struct mosquitto_msg_store *stored,
    // json_t *properties,
    // BOOL update
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int rc = 0;
    // struct mosquitto_client_msg *msg;
    // struct mosquitto_msg_data *msg_data;
    // enum mqtt_msg_state state = mosq_ms_invalid;
    // int i;
    // char **dest_ids;
    //
    // if(!priv->client_id) {
    //     return MOSQ_ERR_SUCCESS; /* Protect against unlikely "client is disconnected but not entirely freed" scenario */
    // }
    //
    // if(dir == mosq_md_out) {
    //     msg_data = &priv->msgs_out;
    // } else {
    //     msg_data = &priv->msgs_in;
    // }
    //
    // /* Check whether we've already sent this message to this client
    //  * for outgoing messages only.
    //  * If retain==true then this is a stale retained message and so should be
    //  * sent regardless. FIXME - this does mean retained messages will received
    //  * multiple times for overlapping subscriptions, although this is only the
    //  * case for SUBSCRIPTION with multiple subs in so is a minor concern.
    //  */
    // if(priv->protocol_version != mosq_p_mqtt5
    //         && priv->allow_duplicate_messages == FALSE
    //         && dir == mosq_md_out && retain == FALSE && stored->dest_ids) {
    //
    //     for(i=0; i<stored->dest_id_count; i++) {
    //         if(stored->dest_ids[i] && !strcmp(stored->dest_ids[i], priv->client_id)) {
    //             /* We have already sent this message to this client. */
    //             JSON_DECREF(properties);
    //             return MOSQ_ERR_SUCCESS;
    //         }
    //     }
    // }

    // TODO
    // if(context->sock == INVALID_SOCKET) {
    //     /* Client is not connected only queue messages with QoS>0. */
    //     if(qos == 0 && !db.config->queue_qos0_messages) {
    //         if(!context->bridge) {
    //             mosquitto_property_free_all(&properties);
    //             return 2;
    //         } else {
    //             if(context->bridge->start_type != bst_lazy) {
    //                 mosquitto_property_free_all(&properties);
    //                 return 2;
    //             }
    //         }
    //     }
    //     if(context->bridge && context->bridge->clean_start_local == true) {
    //         mosquitto_property_free_all(&properties);
    //         return 2;
    //     }
    // }

    // TODO
    // if(context->sock != INVALID_SOCKET) {
    //     if(db__ready_for_flight(context, dir, qos)) {
    //         if(dir == mosq_md_out) {
    //             switch(qos) {
    //                 case 0:
    //                     state = mosq_ms_publish_qos0;
    //                     break;
    //                 case 1:
    //                     state = mosq_ms_publish_qos1;
    //                     break;
    //                 case 2:
    //                     state = mosq_ms_publish_qos2;
    //                     break;
    //             }
    //         } else {
    //             if(qos == 2) {
    //                 state = mosq_ms_wait_for_pubrel;
    //             } else {
    //                 mosquitto_property_free_all(&properties);
    //                 return 1;
    //             }
    //         }
    //     } else if(qos != 0 && db__ready_for_queue(context, qos, msg_data)) {
    //         state = mosq_ms_queued;
    //         rc = 2;
    //     } else {
    //         /* Dropping message due to full queue. */
    //         if(context->is_dropping == FALSE) {
    //             context->is_dropping = true;
    //             log__printf(NULL, MOSQ_LOG_NOTICE,
    //                     "Outgoing messages are being dropped for client %s.",
    //                     context->id);
    //         }
    //         G_MSGS_DROPPED_INC();
    //         mosquitto_property_free_all(&properties);
    //         return 2;
    //     }
    // } else {
    //     if (db__ready_for_queue(context, qos, msg_data)) {
    //         state = mosq_ms_queued;
    //     } else {
    //         G_MSGS_DROPPED_INC();
    //         if(context->is_dropping == FALSE) {
    //             context->is_dropping = true;
    //             log__printf(NULL, MOSQ_LOG_NOTICE,
    //                     "Outgoing messages are being dropped for client %s.",
    //                     context->id);
    //         }
    //         mosquitto_property_free_all(&properties);
    //         return 2;
    //     }
    // }
    // assert(state != mosq_ms_invalid);

#ifdef WITH_PERSISTENCE
    if(state == mosq_ms_queued) {
        db.persistence_changes++;
    }
#endif

    // msg = mosquitto__calloc(1, sizeof(struct mosquitto_client_msg));
    // if(!msg) return MOSQ_ERR_NOMEM;
    // msg->prev = NULL;
    // msg->next = NULL;
    // msg->store = stored;
    // db__msg_store_ref_inc(msg->store);
    // msg->mid = mid;
    // msg->timestamp = db.now_s;
    // msg->direction = dir;
    // msg->state = state;
    // msg->dup = FALSE;
    // if(qos > context->max_qos) {
    //     msg->qos = context->max_qos;
    // } else {
    //     msg->qos = qos;
    // }
    // msg->retain = retain;
    // msg->properties = properties;
    //
    // if(state == mosq_ms_queued) {
    //     DL_APPEND(msg_data->queued, msg);
    //     db__msg_add_to_queued_stats(msg_data, msg);
    // } else {
    //     DL_APPEND(msg_data->inflight, msg);
    //     db__msg_add_to_inflight_stats(msg_data, msg);
    // }
    //
    // if(db.config->allow_duplicate_messages == FALSE && dir == mosq_md_out && retain == FALSE) {
    //     /* Record which client ids this message has been sent to so we can avoid duplicates.
    //      * Outgoing messages only.
    //      * If retain==true then this is a stale retained message and so should be
    //      * sent regardless. FIXME - this does mean retained messages will received
    //      * multiple times for overlapping subscriptions, although this is only the
    //      * case for SUBSCRIPTION with multiple subs in so is a minor concern.
    //      */
    //     dest_ids = mosquitto__realloc(stored->dest_ids, sizeof(char *)*(size_t)(stored->dest_id_count+1));
    //     if(dest_ids) {
    //         stored->dest_ids = dest_ids;
    //         stored->dest_id_count++;
    //         stored->dest_ids[stored->dest_id_count-1] = mosquitto__strdup(context->id);
    //         if(!stored->dest_ids[stored->dest_id_count-1]) {
    //             return MOSQ_ERR_NOMEM;
    //         }
    //     } else {
    //         return MOSQ_ERR_NOMEM;
    //     }
    // }
    //
    // if(dir == mosq_md_out && msg->qos > 0 && state != mosq_ms_queued) {
    //     util__decrement_send_quota(context);
    // } else if(dir == mosq_md_in && msg->qos > 0 && state != mosq_ms_queued) {
    //     util__decrement_receive_quota(context);
    // }
    //
    // if(dir == mosq_md_out && update) {
    //     rc = db__message_write_inflight_out_latest(context);
    //     if(rc) return rc;
    //     rc = db__message_write_queued_out(context);
    //     if(rc) return rc;
    // }

    return rc;
}

/***************************************************************************
 *  Using in handle__subscribe()
 ***************************************************************************/
PRIVATE int db__message_write_queued_in(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    struct mosquitto_client_msg *tail, *tmp;
    int rc;

    DL_FOREACH_SAFE(&priv->msgs_in.dl_queued, tail, tmp) {
        if(priv->msgs_in.inflight_maximum != 0 && priv->msgs_in.inflight_quota == 0) {
            break;
        }

        if(tail->qos == 2) {
            tail->state = mosq_ms_send_pubrec;
            db__message_dequeue_first(priv, &priv->msgs_in);
            rc = send__pubrec(gobj, tail->mid, 0, NULL);
            if(!rc) {
                tail->state = mosq_ms_wait_for_pubrel;
            } else {
                return rc;
            }
        }
    }
    return MOSQ_ERR_SUCCESS;
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
 *  get_name_from_nn_table(command_name) is a slower alternative
 ***************************************************************************/
PRIVATE const char *mosquitto_command_string(mqtt_message_t command)
{
    switch(command) {
        case CMD_CONNECT:
            return "CMD_CONNECT";
        case CMD_CONNACK:
            return "CMD_CONNACK";
        case CMD_PUBLISH:
            return "CMD_PUBLISH";
        case CMD_PUBACK:
            return "CMD_PUBACK";
        case CMD_PUBREC:
            return "CMD_PUBREC";
        case CMD_PUBREL:
            return "CMD_PUBREL";
        case CMD_PUBCOMP:
            return "CMD_PUBCOMP";
        case CMD_SUBSCRIBE:
            return "CMD_SUBSCRIBE";
        case CMD_SUBACK:
            return "CMD_SUBACK";
        case CMD_UNSUBSCRIBE:
            return "CMD_UNSUBSCRIBE";
        case CMD_UNSUBACK:
            return "CMD_UNSUBACK";
        case CMD_PINGREQ:
            return "CMD_PINGREQ";
        case CMD_PINGRESP:
            return "CMD_PINGRESP";
        case CMD_DISCONNECT:
            return "CMD_DISCONNECT";
        case CMD_AUTH:
            return "CMD_AUTH";

        case CMD_RESERVED:
        default:
            return "Unknown command";
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
    Protocol limit: 4 bytes for Remaining Length field (not 5)
    Maximum value: 268,435,455 bytes (~256 MB)
    Actual payload: Slightly less after subtracting headers
    According to the MQTT specification - it's strictly limited to 4 bytes maximum.
 ***************************************************************************/
PRIVATE unsigned int packet__varint_bytes(uint32_t word)
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
PRIVATE unsigned int property__get_length(const char *property_name, json_t *value)
{
    hgobj gobj = 0;
    size_t str_len = 0;
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
PRIVATE unsigned int property__get_length_all(json_t *props)
{
    unsigned int len = 0;

    const char *property_name; json_t *value;
    json_object_foreach(props, property_name, value) {
        len += property__get_length(property_name, value);
    }
    return len;
}

/***************************************************************************
 * Return the number of bytes we need to add on to the remaining length when
 * encoding these properties.
 ***************************************************************************/
PRIVATE unsigned int property__get_remaining_length(json_t *props)
{
    unsigned int proplen, varbytes;

    proplen = property__get_length_all(props);
    varbytes = packet__varint_bytes(proplen);
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
    //proclient_generated = TRUE;
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
    // proclient_generated = TRUE; TODO vale para algo?
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
    // proclient_generated = TRUE; TODO vale para algo?
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
PUBLIC int mqtt_property_add_string(
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

    //proclient_generated = TRUE; // TODO
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
PRIVATE int property__write_all(
    hgobj gobj,
    gbuffer_t *gbuf,
    json_t *props, // not owned
    BOOL write_len
) {
    if(write_len) {
        mqtt_write_varint(gbuf, property__get_length_all(props));
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
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
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
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
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
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
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
                return MOSQ_ERR_MALFORMED_PACKET;
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
                return MOSQ_ERR_MALFORMED_PACKET;
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
                return MOSQ_ERR_MALFORMED_PACKET;
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
                return MOSQ_ERR_MALFORMED_PACKET;
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
                return MOSQ_ERR_MALFORMED_PACKET;
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
                return MOSQ_ERR_MALFORMED_PACKET;
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
                return MOSQ_ERR_MALFORMED_PACKET;
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
                return MOSQ_ERR_MALFORMED_PACKET;
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
                return MOSQ_ERR_MALFORMED_PACKET;
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
                return MOSQ_ERR_MALFORMED_PACKET;
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
                return MOSQ_ERR_MALFORMED_PACKET;
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
            return MOSQ_ERR_MALFORMED_PACKET;
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
                return MOSQ_ERR_MALFORMED_PACKET;
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
                return MOSQ_ERR_MALFORMED_PACKET;
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
                return MOSQ_ERR_MALFORMED_PACKET;
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

    if((ret=mqtt_read_varint(gobj, gbuf, &proplen, NULL))<0) {
        // Error already logged
        if(error) {
            *error = ret;
        }
        return NULL;
    }

    json_t *all_properties = json_object();

    while(proplen > 0) {
        if((ret=property_read(gobj, gbuf, &proplen, all_properties))<0) {
            // Error already logged
            JSON_DECREF(all_properties);
            if(error) {
                *error = ret;
            }
            return NULL;
        }
    }

    if((ret=mqtt_property_check_all(gobj, command, all_properties))<0) {
        // Error already logged
        JSON_DECREF(all_properties);
        if(error) {
            *error = ret;
        }
        return NULL;
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        gobj_trace_json(
            gobj,
            all_properties,
            "all_properties, command %s", get_command_name(command)
        );
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
 *  Return json_int_t, -1 is property not available
 *  return value is an uint8_t
 ***************************************************************************/
PRIVATE json_int_t property_read_byte(json_t *properties, int identifier)
{
    hgobj gobj = 0;

    if(identifier != MQTT_PROP_PAYLOAD_FORMAT_INDICATOR
            && identifier != MQTT_PROP_REQUEST_PROBLEM_INFORMATION
            && identifier != MQTT_PROP_REQUEST_RESPONSE_INFORMATION
            && identifier != MQTT_PROP_MAXIMUM_QOS
            && identifier != MQTT_PROP_RETAIN_AVAILABLE
            && identifier != MQTT_PROP_WILDCARD_SUB_AVAILABLE
            && identifier != MQTT_PROP_SUBSCRIPTION_ID_AVAILABLE
            && identifier != MQTT_PROP_SHARED_SUB_AVAILABLE
    ) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Bad byte property identifier",
            "identifier",   "%d", identifier,
            NULL
        );
    }

    json_t *property = property_get_property(properties, identifier);
    if(!property) {
        return -1;
    }
    return kw_get_int(gobj, property, "value", -1, KW_REQUIRED);
}

/***************************************************************************
 *  Return json_int_t, -1 is property not available
 *  return value is an uint16_t
 ***************************************************************************/
PRIVATE json_int_t property_read_int16(json_t *properties, int identifier)
{
    hgobj gobj = 0;

    if(identifier != MQTT_PROP_SERVER_KEEP_ALIVE
            && identifier != MQTT_PROP_RECEIVE_MAXIMUM
            && identifier != MQTT_PROP_TOPIC_ALIAS_MAXIMUM
            && identifier != MQTT_PROP_TOPIC_ALIAS
    ) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Bad int16 property identifier",
            "identifier",   "%d", identifier,
            NULL
        );
    }

    json_t *property = property_get_property(properties, identifier);
    if(!property) {
        return -1;
    }
    return kw_get_int(gobj, property, "value", -1, KW_REQUIRED);
}

/***************************************************************************
 *  Return json_int_t, -1 is property not available
 *  return value is an uint32_t
 ***************************************************************************/
PRIVATE json_int_t property_read_int32(json_t *properties, int identifier)
{
    hgobj gobj = 0;

    if(identifier != MQTT_PROP_MESSAGE_EXPIRY_INTERVAL
            && identifier != MQTT_PROP_SESSION_EXPIRY_INTERVAL
            && identifier != MQTT_PROP_WILL_DELAY_INTERVAL
            && identifier != MQTT_PROP_MAXIMUM_PACKET_SIZE
    ) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Bad int32 property identifier",
            "identifier",   "%d", identifier,
            NULL
        );
    }

    json_t *property = property_get_property(properties, identifier);
    if(!property) {
        return -1;
    }
    return kw_get_int(gobj, property, "value", -1, KW_REQUIRED);
}

/***************************************************************************
 *  Return json_int_t, -1 is property not available
 *  return value is an uint32_t
 ***************************************************************************/
PRIVATE json_int_t property_read_varint(json_t *properties, int identifier)
{
    hgobj gobj = 0;

    if(identifier != MQTT_PROP_SUBSCRIPTION_IDENTIFIER
    ) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Bad variant property identifier",
            "identifier",   "%d", identifier,
            NULL
        );
    }

    json_t *property = property_get_property(properties, identifier);
    if(!property) {
        return -1;
    }
    return kw_get_int(gobj, property, "value", -1, KW_REQUIRED);
}

/***************************************************************************
 *  Return json_int_t, -1 is property not available
 *  return value is an uint32_t
 ***************************************************************************/
PRIVATE json_int_t property_read_binary(json_t *properties, int identifier) // TODO
{
    hgobj gobj = 0;

    if(identifier != MQTT_PROP_CORRELATION_DATA
            && identifier != MQTT_PROP_AUTHENTICATION_DATA
    ) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Bad binary property identifier",
            "identifier",   "%d", identifier,
            NULL
        );
    }

    json_t *property = property_get_property(properties, identifier);
    if(!property) {
        return -1;
    }
    return kw_get_int(gobj, property, "value", -1, KW_REQUIRED);
}

/***************************************************************************
 *  Return NULL if property is not available
 ***************************************************************************/
PRIVATE const char *property_read_string(json_t *properties, int identifier)
{
    hgobj gobj = 0;

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
            "msg",          "%s", "Bad string property identifier",
            "identifier",   "%d", identifier,
            NULL
        );
    }

    json_t *property = property_get_property(properties, identifier);
    if(!property) {
        return NULL;
    }

    return kw_get_str(gobj, property, "value", "", KW_REQUIRED);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *property_read_string_pair(json_t *properties, int identifier)
{
    hgobj gobj = 0;

    // TODO what must to return?
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
            "msg",          "%s", "Bad string property identifier",
            "identifier",   "%d", identifier,
            NULL
        );
    }

    json_t *property = property_get_property(properties, identifier);
    if(!property) {
        return NULL;
    }
    return kw_get_str(gobj, property, "value", "", 0);
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

    len = remaining_length + packet__varint_bytes(remaining_length);
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
        trace_msg0("👉👉 %s Sending simple command %s to '%s' %s",
            priv->iamServer?"broker":"client",
            get_command_name(command),
            priv->client_id,
            gobj_short_name(gobj_bottom_gobj(gobj))
        );
    }
    return send_packet(gobj, gbuf);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send__connect(
    hgobj gobj,
    uint16_t keepalive,
    BOOL clean_session,
    json_t *properties // owned
) {
    uint32_t payloadlen;
    uint8_t will = 0;
    uint8_t byte;
    int rc;
    uint8_t version;
    uint32_t headerlen;
    uint32_t proplen = 0, varbytes = 0;
    json_t *local_props = NULL;
    uint16_t receive_maximum;
    uint32_t mqtt_session_expiry_interval = 0;

    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *mqtt_client_id = gobj_read_str_attr(gobj, "mqtt_client_id");
    const char *mqtt_protocol = gobj_read_str_attr(gobj, "mqtt_protocol");
    mqtt_session_expiry_interval = atoi(gobj_read_str_attr(gobj, "mqtt_session_expiry_interval"));

    int protocol = mosq_p_mqtt5; // "mqttv5" default
    if(strcasecmp(mqtt_protocol, "mqttv5")==0 || strcasecmp(mqtt_protocol, "v5")==0) {
        protocol = mosq_p_mqtt5;
    } else if(strcasecmp(mqtt_protocol, "mqttv311")==0 || strcasecmp(mqtt_protocol, "v311")==0) {
        protocol = mosq_p_mqtt311;
    } else if(strcasecmp(mqtt_protocol, "mqttv31")==0 || strcasecmp(mqtt_protocol, "v31")==0) {
        protocol = mosq_p_mqtt31;
    }

    gobj_write_integer_attr(gobj, "protocol_version", protocol);

    /* Only MQTT v3.1 requires a client id to be sent */
    if(protocol == mosq_p_mqtt31 && empty_string(mqtt_client_id)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "client id required in mqtt31 protocol",
            NULL
        );
        JSON_DECREF(properties);
        return MOSQ_ERR_PROTOCOL;
    }

    const char *username = gobj_read_str_attr(gobj, "user_id");
    const char *password = gobj_read_str_attr(gobj, "user_passw");

    if(protocol == mosq_p_mqtt5) {
        /* Generate properties from options */
        // TODO
        // if(!mosquitto_property_read_int16(properties, MQTT_PROP_RECEIVE_MAXIMUM, &receive_maximum, FALSE)) {
        //     rc = mosquitto_property_add_int16(&local_props, MQTT_PROP_RECEIVE_MAXIMUM, priv->msgs_in.inflight_maximum);
        //     if(rc) {
        // log_error
        // JSON_DECREF(properties);
        //     return rc;
    // }
        // } else {
        //     priv->msgs_in.inflight_maximum = receive_maximum;
        //     priv->msgs_in.inflight_quota = receive_maximum;
        // }

        mqtt_property_add_int32(
            gobj,
            properties,
            MQTT_PROP_SESSION_EXPIRY_INTERVAL,
            mqtt_session_expiry_interval
        );

        version = PROTOCOL_VERSION_v5;
        headerlen = 10;
        proplen = 0;
        proplen += property__get_length_all(properties);
        proplen += property__get_length_all(local_props);
        varbytes = packet__varint_bytes(proplen);
        headerlen += proplen + varbytes;
    } else if(protocol == mosq_p_mqtt311) {
        version = PROTOCOL_VERSION_v311;
        headerlen = 10;
    } else if(protocol == mosq_p_mqtt31) {
        version = PROTOCOL_VERSION_v31;
        headerlen = 12;
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Protocol unknown",
            "protocol",     "%d", protocol,
            NULL
        );
        JSON_DECREF(properties);
        return MOSQ_ERR_INVAL;
    }

    if(!empty_string(mqtt_client_id)) {
        payloadlen = (uint32_t)(2U+strlen(mqtt_client_id));
    } else {
        payloadlen = 2U;
    }

    // if(mosq->will) { TODO
    //     will = 1;
    //     assert(mosq->will->msg.topic);
    //
    //     payloadlen += (uint32_t)(2+strlen(mosq->will->msg.topic) + 2+(uint32_t)mosq->will->msg.payloadlen);
    //     if(protocol == mosq_p_mqtt5) {
    //         payloadlen += property__get_remaining_length(mosq->will->properties);
    //     }
    // }

    /* After this check we can be sure that the username and password are
     * always valid for the current protocol, so there is no need to check
     * username before checking password. */
    if(protocol == mosq_p_mqtt31 || protocol == mosq_p_mqtt311) {
        if(!empty_string(password) && empty_string(username)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Invalid combination of user/passw",
                NULL
            );
            JSON_DECREF(properties);
            return MOSQ_ERR_INVAL;
        }
    }

    if(!empty_string(username)) {
        payloadlen += (uint32_t)(2+strlen(username));
    }
    if(!empty_string(password)) {
        payloadlen += (uint32_t)(2+strlen(password));
    }

    int remaining_length = headerlen + payloadlen;
    gbuffer_t *gbuf = build_mqtt_packet(gobj, CMD_CONNECT, remaining_length);
    if(!gbuf) {
        // Error already logged
        JSON_DECREF(properties);
        return MOSQ_ERR_NOMEM;
    }

    /* Variable header */
    if(version == PROTOCOL_VERSION_v31) {
        mqtt_write_string(gbuf, PROTOCOL_NAME_v31);
    } else {
        mqtt_write_string(gbuf, PROTOCOL_NAME);
    }
    mqtt_write_byte(gbuf, version);
    byte = (uint8_t)((clean_session&0x1)<<1);
    if(will) {
        // TODO
        // byte = byte | (uint8_t)(((mosq->will->msg.qos&0x3)<<3) | ((will&0x1)<<2));
        // if(mosq->retain_available) {
        //     byte |= (uint8_t)((mosq->will->msg.retain&0x1)<<5);
        // }
    }
    if(!empty_string(username)) {
        byte = byte | 0x1<<7;
    }
    if(!empty_string(password)) {
        byte = byte | 0x1<<6;
    }
    mqtt_write_byte(gbuf, byte);
    mqtt_write_uint16(gbuf, keepalive);

    if(protocol == mosq_p_mqtt5) {
        /* Write properties */
        mqtt_write_varint(gbuf, proplen);
        property__write_all(gobj, gbuf, properties, FALSE);
        property__write_all(gobj, gbuf, local_props, FALSE);
    }
    JSON_DECREF(properties);
    JSON_DECREF(local_props)

    /* Payload */
    if(!empty_string(mqtt_client_id)) {
        mqtt_write_string(gbuf, mqtt_client_id);
    } else {
        mqtt_write_uint16(gbuf, 0);
    }
    if(will) {
        // TODO
        // if(protocol == mosq_p_mqtt5) {
        //     /* Write will properties */
        //     property__write_all(gobj, gbuf, mosq->will->properties, true);
        // }
        // mqtt_write_string(gbuf, mosq->will->msg.topic, (uint16_t)strlen(mosq->will->msg.topic));
        // mqtt_write_string(gbuf, (const char *)mosq->will->msg.payload, (uint16_t)mosq->will->msg.payloadlen);
    }

    if(!empty_string(username)) {
        mqtt_write_string(gbuf, username);
    }
    if(!empty_string(password)) {
        mqtt_write_string(gbuf, password);
    }

    // mosq->keepalive = keepalive; // TODO
    const char *url = gobj_read_str_attr(gobj, "url");
    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0(
        "👉👉 client Sending CONNECT to \n"
        "   url '%s' \n"
        "   client '%s' \n"
        "   username '%s' \n"
        "   protocol_version '%s' \n"
        "   clean_start %d, session_expiry_interval %d \n"
        "   will %d, will_retain ?, will_qos ? \n"
        "   keepalive %d \n",
            (char *)url,
            (char *)mqtt_client_id,
            (char *)username,
            (char *)mqtt_protocol,
            (int)clean_session,
            (int)mqtt_session_expiry_interval,
            (int)will,
            // (int)will_retain,
            // (int)will_qos,
            (int)keepalive
        );
        if(properties) {
            gobj_trace_json(gobj, properties, "Sending CONNECT properties");
        }
    }

    return send_packet(gobj, gbuf);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send__connack(
    hgobj gobj,
    uint8_t ack,
    uint8_t reason_code,
    json_t *connack_props // owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint32_t remaining_length = 2;

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("👉👉 %s Sending CONNACK to '%s' %s (ack %d, reason code %d '%s')",
            priv->iamServer?"broker":"client",
            priv->client_id,
            gobj_short_name(gobj_bottom_gobj(gobj)),
            ack,
            reason_code,
            mosquitto_reason_string(reason_code)
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

        remaining_length += property__get_remaining_length(connack_props);
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
        property__write_all(gobj, gbuf, connack_props, TRUE);
    }
    JSON_DECREF(connack_props);
    return send_packet(gobj, gbuf);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send__disconnect(
    hgobj gobj,
    uint8_t reason_code,
    json_t *properties
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_bool_attr(gobj, "send_disconnect", FALSE);

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        if(priv->iamServer) {
            trace_msg0("👉👉 %s Sending DISCONNECT to '%s' (%d '%s')",
                priv->iamServer?"broker":"client",
                priv->client_id,
                reason_code,
                mosquitto_reason_string(reason_code)
            );
        } else {
            trace_msg0("👉👉 %s Sending client DISCONNECT to '%s'",
                priv->iamServer?"broker":"client",
                priv->client_id
            );
        }
    }

    uint32_t remaining_length;

    if(priv->protocol_version == mosq_p_mqtt5 && (reason_code != 0 || properties)) {
        remaining_length = 1;
        if(properties) {
            remaining_length += property__get_remaining_length(properties);
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
            property__write_all(gobj, gbuf, properties, TRUE);
        }
    }

    return send_packet(gobj, gbuf);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__pingreq(hgobj gobj)
{
    // PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // TODO if mosq->keepalive then send__pingreq()
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
        return MOSQ_ERR_PROTOCOL;
    }

    // priv->ping_t = 0; TODO /* No longer waiting for a PINGRESP. */

    if(priv->iamServer) {
        if(!priv->is_bridge) {
            // Parece que el broker no debe recibir pingresp, solo si es bridge
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt CMD_PINGREQ: not bridge",
                NULL
            );
            return MOSQ_ERR_PROTOCOL;
        }
    }
    return MOSQ_ERR_SUCCESS;
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
        trace_msg0("👉👉 %s Sending %s to '%s', mid %ld (%d '%s')",
            priv->iamServer?"broker":"client",
            get_command_name(command & 0xF0),
            priv->client_id,
            (long)mid,
            reason_code,
            mosquitto_reason_string(reason_code)
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
            remaining_length += property__get_remaining_length(properties);
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
            property__write_all(gobj, gbuf, properties, TRUE);
        }
    }

    JSON_DECREF(properties)

    return send_packet(gobj, gbuf);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send__puback(hgobj gobj, uint16_t mid, uint8_t reason_code, json_t *properties)
{
    //util__increment_receive_quota(mosq);
    /* We don't use Reason String or User Property yet. */
    return send_command_with_mid(gobj, CMD_PUBACK, mid, FALSE, reason_code, properties);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send__pubcomp(hgobj gobj, uint16_t mid, json_t *properties)
{
    //util__increment_receive_quota(mosq);
    /* We don't use Reason String or User Property yet. */
    return send_command_with_mid(gobj, CMD_PUBCOMP, mid, FALSE, 0, properties);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send__pubrec(hgobj gobj, uint16_t mid, uint8_t reason_code, json_t *properties)
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
PRIVATE int send__publish(
    hgobj gobj,
    uint16_t mid,
    const char *topic,
    gbuffer_t *gbuf_payload, // not owned
    uint8_t qos,
    BOOL retain,
    BOOL dup,
    json_t *cmsg_props, // not owned
    json_t *store_props, // not owned
    uint32_t expiry_interval
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->retain_available) {
        retain = FALSE;
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("👉👉 %s Sending PUBLISH to '%s', topic '%s' (dup %d, qos %d, retain %d, mid %d)",
            priv->iamServer?"broker":"client",
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

    char *payload = gbuf_payload?gbuffer_cur_rd_pointer(gbuf_payload):NULL;
    int payloadlen = gbuf_payload?(int)gbuffer_leftbytes(gbuf_payload):0;

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
        proplen += property__get_length_all(cmsg_props);
        proplen += property__get_length_all(store_props);
        if(expiry_interval > 0) {
            expiry_prop = json_object();

            mqtt_property_add_int32(
                gobj, expiry_prop, MQTT_PROP_MESSAGE_EXPIRY_INTERVAL, expiry_interval
            );
            // expiry_prop.client_generated = FALSE;
            proplen += property__get_length_all(expiry_prop);
        }

        varbytes = packet__varint_bytes(proplen);
        if(varbytes > 4) {
            /*
                Protocol limit: 4 bytes for Remaining Length field (not 5)
                Maximum value: 268,435,455 bytes (~256 MB)
                Actual payload: Slightly less after subtracting headers
                According to the MQTT specification - it's strictly limited to 4 bytes maximum.
             */
            // TODO is error?
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
        property__write_all(gobj, gbuf, cmsg_props, FALSE);
        property__write_all(gbuf, gbuf, store_props, FALSE);
        if(expiry_interval > 0) {
            property__write_all(gobj, gbuf, expiry_prop, FALSE);
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
 *
 ***************************************************************************/
PRIVATE int send__subscribe(
    hgobj gobj,
    int mid,
    json_t *subs,
    int qos,
    json_t *properties // not owned
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint32_t packetlen = 2;

    int idx; json_t *jn_sub;
    json_array_foreach(subs, idx, jn_sub) {
        const char *sub = json_string_value(jn_sub);
        size_t tlen = strlen(sub);
        if(tlen > UINT16_MAX) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt subscribe: sub TOO LARGE",
                "subs",         "%j", subs,
                NULL
            );
            return -1;
        }
        packetlen += 2U+(uint16_t)tlen + 1U;
    }

    if(priv->protocol_version == mosq_p_mqtt5) {
        packetlen += property__get_remaining_length(properties);
    }

    uint8_t command = CMD_SUBSCRIBE | (1<<1);
    gbuffer_t *gbuf = build_mqtt_packet(gobj, command, packetlen);
    if(!gbuf) {
        // Error already logged
        return MOSQ_ERR_NOMEM;
    }

    mqtt_write_uint16(gbuf, mid);

    if(priv->protocol_version == mosq_p_mqtt5) {
        property__write_all(gobj, gbuf, properties, TRUE);
    }

    /* Payload */
    json_array_foreach(subs, idx, jn_sub) {
        const char *sub = json_string_value(jn_sub);
        mqtt_write_string(gbuf, sub);
        mqtt_write_byte(gbuf, (uint8_t)qos);
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        char *topics = json2uglystr(subs);
        trace_msg0("👉👉 %s Sending SUBSCRIBE client id '%s', topic '%s' (qos %d, mid %d)",
            priv->iamServer?"broker":"client",
            SAFE_PRINT(priv->client_id),
            topics,
            qos,
            mid
        );
        GBMEM_FREE(topics)
    }

    return send_packet(gobj, gbuf);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send__unsubscribe(
    hgobj gobj,
    int mid,
    json_t *subs,
    json_t *properties
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint32_t packetlen = 2;

    int idx; json_t *jn_sub;
    json_array_foreach(subs, idx, jn_sub) {
        const char *sub = json_string_value(jn_sub);
        size_t tlen = strlen(sub);
        if(tlen > UINT16_MAX) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt subscribe: sub TOO LARGE",
                "subs",         "%j", subs,
                NULL
            );
            return -1;
        }
        packetlen += 2U+(uint16_t)tlen;
    }

    if(priv->protocol_version == mosq_p_mqtt5) {
        packetlen += property__get_remaining_length(properties);
    }

    uint8_t command = CMD_UNSUBSCRIBE | (1<<1);
    gbuffer_t *gbuf = build_mqtt_packet(gobj, command, packetlen);
    if(!gbuf) {
        // Error already logged
        return MOSQ_ERR_NOMEM;
    }

    mqtt_write_uint16(gbuf, mid);

    if(priv->protocol_version == mosq_p_mqtt5) {
        /* We don't use User Property yet. */
        property__write_all(gobj, gbuf, properties, TRUE);
    }

    /* Payload */
    json_array_foreach(subs, idx, jn_sub) {
        const char *sub = json_string_value(jn_sub);
        mqtt_write_string(gbuf, sub);
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        char *topics = json2uglystr(subs);
        trace_msg0("👉👉 %s Sending UNSUBSCRIBE client id '%s', topic '%s' (mid %d)",
            priv->iamServer?"broker":"client",
            SAFE_PRINT(priv->client_id),
            topics,
            mid
        );
        GBMEM_FREE(topics)
    }

    return send_packet(gobj, gbuf);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int will__read(
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
PRIVATE int handle__connect(hgobj gobj, gbuffer_t *gbuf, hgobj src)
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
            "msg",          "%s", "Mqtt CMD_CONNECT: not server",
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

    uint16_t slen;
    if(mqtt_read_uint16(gobj, gbuf, &slen)<0) {
        // Error already logged
        return -1;
    }
    if(slen != 4 /* MQTT */ && slen != 6 /* MQIsdp */) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt CMD_CONNECT: MQTT bad length",
            NULL
        );
        return -1;
    }

    if(mqtt_read_bytes(gobj, gbuf, protocol_name, slen) < 0) {
        // Error already logged
        return -1;
    }
    protocol_name[slen] = '\0';

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
            send__connack(gobj, 0, CONNACK_REFUSED_PROTOCOL_VERSION, NULL);
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
            send__connack(gobj, 0, CONNACK_REFUSED_PROTOCOL_VERSION, NULL);
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
    gobj_write_bool_attr(gobj, "is_bridge", is_bridge); // TODO review and refuse bridge?

    /*-------------------------------------------*
     *      Connect flags
        ## What is the purpose of `cleanSession`?

        `cleanSession` (MQTT 3.1.1) controls whether the broker should create a **new session** or **resume a previous one** for a client.

        ### `cleanSession = 1`
        A **new, non-persistent session** is created:
        - Previous session data is **discarded**.
        - Broker does **not** store:
          - Subscriptions
          - Pending QoS 1/2 messages
          - Inflight message state
        - When the client disconnects, the session is **deleted**.

        Use this when the client does not need message history or persistent subscriptions.

        ### `cleanSession = 0`
        A **persistent session** is used:
        - Broker **keeps**:
          - Subscriptions
          - Pending QoS 1/2 messages for offline delivery
          - Inflight message state
        - If the client reconnects with the same Client ID, the broker **restores** the session.

        Use this when the client must not lose messages during disconnections.

        ### MQTT 5 equivalence
        `cleanSession` was split into:
        - `cleanStart`
        - `sessionExpiryInterval`

        Mapping:
        - `cleanSession=1` → `cleanStart=true`, `sessionExpiry=0`
        - `cleanSession=0` → `cleanStart=FALSE`, `sessionExpiry>0`

        ### Summary
        `cleanSession` determines whether MQTT sessions are **temporary** or **persistent**, controlling how subscriptions and queued messages survive client disconnections.

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
            "msg",          "%s", "Mqtt: Invalid Will QoS in CONNECT",
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
            send__connack(gobj, 0, MQTT_RC_RETAIN_NOT_SUPPORTED, NULL);
        }
        return -1;
    }

    gobj_write_bool_attr(gobj, "clean_start", clean_start);
    gobj_write_integer_attr(gobj, "session_expiry_interval", session_expiry_interval);
    gobj_write_bool_attr(gobj, "will", will);
    gobj_write_bool_attr(gobj, "will_retain", will_retain);
    gobj_write_integer_attr(gobj, "will_qos", will_qos);

    // TODO do remove will if error (refuse connection)
    // if(context->will) {
    //     mosquitto_property_free_all(&context->will->properties);
    //     mosquitto__free(context->will->msg.payload);
    //     mosquitto__free(context->will->msg.topic);
    //     mosquitto__free(context->will);
    //     context->will = NULL;
    // }

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
    json_t *connect_properties = NULL;
    if(version_byte == PROTOCOL_VERSION_v5) {
        connect_properties = property_read_all(gobj, gbuf, CMD_CONNECT, 0);
        if(!connect_properties) {
            // Error already logged
            return -1;
        }
        property_process_connect(gobj, connect_properties);
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
            send__connack(gobj, 0, MQTT_RC_QOS_NOT_SUPPORTED, NULL);
        }
        JSON_DECREF(connect_properties);
        return -1;
    }

    // TODO Auth method not supported
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

    /*-------------------------------------------*
     *      Client id
        ## Is the Client ID required in the MQTT protocol?

        ### MQTT 3.1.1
        Yes — the Client ID **must** be included in the CONNECT packet.
        However:
        - It **may be empty** **only if** `cleanSession = 1`.
        - If empty and the broker does not allow it, the connection is rejected.
        - Some brokers auto-generate a Client ID when it is empty.

        ### MQTT 5.0
        Yes — the Client ID field is still required, **but it may be empty**.
        If empty:
        - The broker **may assign a Server-Generated Client ID**.
        - Acceptance depends on broker configuration.

        ### Summary
        The Client ID is **always required by the protocol**, but:
        - **3.1.1:** can be empty only with `cleanSession=1`.
        - **5.0:** can be empty; broker may generate one.

        For maximum compatibility, it is recommended to **always provide a unique Client ID**.
     *
     *-------------------------------------------*/
    char uuid[70];
    char *client_id = NULL;
    BOOL assigned_id = FALSE;
    uint16_t client_id_len;

    if(mqtt_read_string(gobj, gbuf, &client_id, &client_id_len)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: bad client id",
            NULL
        );
        JSON_DECREF(connect_properties);
        return -1;
    }

    if(client_id_len == 0) {
        if(protocol_version == mosq_p_mqtt31) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt: no client id",
                NULL
            );
            send__connack(gobj, 0, CONNACK_REFUSED_IDENTIFIER_REJECTED, NULL);
            JSON_DECREF(connect_properties);
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
                    send__connack(gobj, 0, CONNACK_REFUSED_IDENTIFIER_REJECTED, NULL);
                } else {
                    send__connack(gobj, 0, MQTT_RC_UNSPECIFIED, NULL);
                }
                JSON_DECREF(connect_properties);
                return -1;
            } else {
                const char *auto_prefix = "auto-";
                snprintf(uuid, sizeof(uuid), "%s", auto_prefix);
                create_random_uuid(
                    uuid + strlen(auto_prefix),
                    (int)(sizeof(uuid) - strlen(auto_prefix))
                );
                client_id = uuid;
                if(!client_id) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MQTT_ERROR,
                        "msg",          "%s", "Mqtt: client_id_gen() FAILED",
                        NULL
                    );
                    JSON_DECREF(connect_properties);
                    return -1;
                }
                client_id_len = strlen(client_id);
                assigned_id = TRUE;
            }
        }
    }

    gobj_write_strn_attr(gobj, "client_id", client_id, client_id_len);
    gobj_write_bool_attr(gobj, "assigned_id", assigned_id);

    /*
     *  TODO let entry of clients with a certain prefix!
     *  clientid_prefixes check
     */
    // if(priv->clientid_prefixes) {
    //     if(strncmp(priv->clientid_prefixes, client_id, strlen(priv->clientid_prefixes))) {
    //         if(priv->protocol_version == mosq_p_mqtt5) {
    //             send__connack(context, 0, MQTT_RC_NOT_AUTHORIZED, NULL);
    //         } else {
    //             send__connack(context, 0, CONNACK_REFUSED_NOT_AUTHORIZED, NULL);
    //         }
    //         rc = MOSQ_ERR_AUTH;
    //         goto handle_connect_error;
    //     }
    // }

    /*-------------------------------------------*
     *      Will
     *-------------------------------------------*/
    if(will) {
        if(will__read(gobj, gbuf)<0) { // Write in gbuf_will_payload the will payload
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt: will__read FAILED()",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            JSON_DECREF(connect_properties);
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
                JSON_DECREF(connect_properties);
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
                JSON_DECREF(connect_properties);
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
                JSON_DECREF(connect_properties);
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
                JSON_DECREF(connect_properties);
                return -1;
            }
        }
    }

    /*-------------------------------------------*
     *      All data readn, cannot be more
     *-------------------------------------------*/
    if(gbuffer_leftbytes(gbuf)>0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: too much data",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        JSON_DECREF(connect_properties);
        return -1;
    }

    if(gobj_read_bool_attr(gobj, "use_username_as_clientid")) {
        if(!empty_string(username)) {
            gobj_write_str_attr(gobj, "client_id", username);
        } else {
            if(protocol_version == mosq_p_mqtt5) {
                send__connack(gobj, 0, MQTT_RC_NOT_AUTHORIZED, NULL);
            } else {
                send__connack(gobj, 0, CONNACK_REFUSED_NOT_AUTHORIZED, NULL);
            }
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Mqtt: not authorized, use_username_as_clientid and no username",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            JSON_DECREF(connect_properties);
            return -1;
        }
    }

    /*-------------------------------------------*
     *      Authenticate
     *-------------------------------------------*/
    if(!empty_string(priv->auth_method)) {
        // TODO implement JWT auth method
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: AUTHORIZATION METHOD not supported",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        send__connack(gobj, 0, MQTT_RC_BAD_AUTHENTICATION_METHOD, NULL); // por contestar algo
        JSON_DECREF(connect_properties);
        return -1;
    }

    /*---------------------------------------------*
     *      Check user/password
     *---------------------------------------------*/
    // TODO join with auth_method
    const char *jwt = ""; // TODO
    const char *peername = gobj_read_str_attr(src, "peername");
    const char *dst_service = "treedb_mqtt_broker"; // TODO too much hardcoded
    int authorization = 0;
    json_t *kw_auth = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s}",
        "client_id", SAFE_PRINT(priv->client_id),
        "username", SAFE_PRINT(username),
        "password", SAFE_PRINT(password),
        "jwt", SAFE_PRINT(jwt),
        "peername", SAFE_PRINT(peername),
        "dst_service", SAFE_PRINT(dst_service)
    );

    json_t *auth = gobj_authenticate(gobj, kw_auth, gobj);
    authorization = COMMAND_RESULT(gobj, auth);
    print_json2("XXX authenticated", auth); // TODO TEST

    if(authorization < 0) {
        if(priv->protocol_version == mosq_p_mqtt5) {
            send__connack(gobj, 0, MQTT_RC_NOT_AUTHORIZED, NULL);
        } else {
            send__connack(gobj, 0, CONNACK_REFUSED_NOT_AUTHORIZED, NULL);
        }
        JSON_DECREF(auth)
        JSON_DECREF(connect_properties);
        return -1;
    }

    /*-------------------------------------------*
     *      Here is connect__on_authorised
     *-------------------------------------------*/

    /*
     *  TODO Find if this client already has an entry.
     *  This must be done *after* any security checks.
     */
    const char *session_id = kw_get_str(gobj, auth, "session_id", "", KW_REQUIRED);
    json_t *services_roles = kw_get_dict(gobj, auth, "services_roles", NULL, KW_REQUIRED);

    // TODO esto debe ir a new client in upper level
    /* Client is already connected, disconnect old version. This is
     * done in context__cleanup() below. */
    // if(priv->connection_messages == true) {
    //     log__printf(NULL, MOSQ_LOG_ERR, "Client %s already connected, closing old connection.", context->id);
    // }

    // if(priv->clean_start == FALSE && prev_session_expiry_interval > 0) {
    //     if(priv->protocol_version == mosq_p_mqtt311 || priv->protocol_version == mosq_p_mqtt5) {
    //         connect_ack |= 0x01;
    //     }
    //     // copia client session TODO
    // }
    //
    // if(priv->clean_start == TRUE) {
    //     // TODO new sub__clean_session(gobj, client);
    // }
    // if((prev_protocol_version == mosq_p_mqtt5 && prev_session_expiry_interval == 0)
    //         || (prev_protocol_version != mosq_p_mqtt5 && prev_clean_start == TRUE)
    //         || (priv->clean_start == TRUE)
    //         ) {
    //     // TODO context__send_will(found_context);
    // }
    //
    // // TODO session_expiry__remove(found_context);
    // // TODO will_delay__remove(found_context);
    // // TODO will__clear(found_context);
    //
    // //found_context->clean_start = TRUE;
    // //found_context->session_expiry_interval = 0;
    // //mosquitto__set_state(found_context, mosq_cs_duplicate);
    //
    // if(isConnected) {
    //     hgobj gobj_bottom = (hgobj)(size_t)kw_get_int(gobj, client, "_gobj_bottom", 0, KW_REQUIRED);
    //     if(gobj_bottom) {
    //         gobj_send_event(gobj_bottom, EV_DROP, 0, gobj);
    //     }
    // }

    // TODO
    // rc = acl__find_acls(context);
    // if(rc) {
    //     free(auth_data_out);
    // JSON_DECREF(auth)
    // JSON_DECREF(connect_properties);
    //     return rc;
    // }

    /*---------------------------------------------*
     *      Print new client connected
     *---------------------------------------------*/
    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0(
        "  👈 CONNECT\n"
        "   client '%s', assigned_id %d\n"
        "   username '%s', password '%s'\n"
        "   protocol_name '%s', protocol_version '%s', is_bridge %d\n"
        "   clean_start %d, session_expiry_interval %d\n"
        "   will %d, will_retain %d, will_qos %d\n"
        "   username_flag %d, password_flag %d, keepalive %d\n",
            priv->client_id,
            priv->assigned_id,
            username,
            password,
            protocol_name,
            protocol_version_name(protocol_version),
            is_bridge,
            (int)clean_start,
            (int)session_expiry_interval,
            (int)will,
            (int)will_retain,
            (int)will_qos,
            (int)username_flag,
            (int)password_flag,
            (int)keepalive
        );
        if(connect_properties) {
            print_json2("CONNECT_PROPERTIES", connect_properties);
        }

        if(priv->gbuf_will_payload) {
            gobj_trace_dump_gbuf(gobj, priv->gbuf_will_payload, "gbuf_will_payload");
        }
    }

    // context->ping_t = 0; TODO
    // context->is_dropping = FALSE;
    // kw_set_dict_value(gobj, client, "ping_t", json_integer(0));
    // kw_set_dict_value(gobj, client, "is_dropping", json_false());

    /*-----------------------------*
     *  Check acl acl__find_acls
     *-----------------------------*/
    // TODO
    //connection_check_acl(context, &context->msgs_in.inflight);
    //connection_check_acl(context, &context->msgs_in.queued);
    //connection_check_acl(context, &context->msgs_out.inflight);
    //connection_check_acl(context, &context->msgs_out.queued);

    //context__add_to_by_id(context); TODO

// // #ifdef WITH_PERSISTENCE TODO
//     if(!context->clean_start) {
//         db.persistence_changes++;
//     }
// // #endif
//     context->max_qos = context->listener->max_qos;

    /*---------------------------------------------*
     *      Prepare the response
     *---------------------------------------------*/
    uint8_t connect_ack = 0;
    json_t *connack_props = json_object();

    if(priv->max_keepalive && (priv->keepalive > priv->max_keepalive || priv->keepalive == 0)) {
        priv->keepalive = priv->max_keepalive;
        if(priv->protocol_version == mosq_p_mqtt5) {
            mqtt_property_add_int16(gobj, connack_props, MQTT_PROP_SERVER_KEEP_ALIVE, priv->keepalive);
        } else {
            send__connack(gobj, connect_ack, CONNACK_REFUSED_IDENTIFIER_REJECTED, NULL);
            JSON_DECREF(auth)
            JSON_DECREF(connect_properties);
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
                JSON_DECREF(auth)
                JSON_DECREF(connect_properties);
                JSON_DECREF(connack_props);
                return -1;
            }
        }
        if(priv->assigned_id) {
            if(mqtt_property_add_string(
                gobj, connack_props, MQTT_PROP_ASSIGNED_CLIENT_IDENTIFIER, priv->client_id)<0)
            {
                // Error already logged
                JSON_DECREF(auth)
                JSON_DECREF(connect_properties);
                JSON_DECREF(connack_props);
                return -1;
            }
        }
        if(priv->auth_method) {
            // TODO No tenemos auth method
            // if(mosquitto_property_add_string(&connack_props, MQTT_PROP_AUTHENTICATION_METHOD, context->auth_method)) {
            //     rc = MOSQ_ERR_NOMEM;
            //     goto error;
            // }
            //
            // if(auth_data_out && auth_data_out_len > 0) {
            //     if(mosquitto_property_add_binary(&connack_props, MQTT_PROP_AUTHENTICATION_DATA, auth_data_out, auth_data_out_len)) {
            //         rc = MOSQ_ERR_NOMEM;
            //         goto error;
            //     }
            // }
        }
    }
    // free(auth_data_out);
    // auth_data_out = NULL;

    //mosquitto__set_state(context, mosq_cs_active);

    /*--------------------------------------------------------------*
     *  Create a client, must be checked in upper level.
     *  This must be done *after* any security checks.
     *  With assigned_id the id is random!, not a persistent id
     *  (HACK client_id is really a device_id)
     *--------------------------------------------------------------*/
    json_t *client = json_pack("{s:s, s:O, s:s, s:s, s:s, s:b, s:b, s:i, s:i, s:i, s:b}",
        "username",                 gobj_read_str_attr(gobj, "__username__"),
        "services_roles",           services_roles,
        "session_id",               gobj_read_str_attr(gobj, "__session_id__"),
        "peername",                 peername,
        "client_id",                priv->client_id,
        "assigned_id",              priv->assigned_id,
        "clean_start",              priv->clean_start,
        "session_expiry_interval",  (int)priv->session_expiry_interval,
        "max_qos",                  (int)priv->max_qos,
        "protocol_version",         (int)priv->protocol_version,
        "will",                     priv->will
    );

    if(priv->will) {
        json_t *jn_will = json_pack("{s:b, s:i, s:s, s:i, s:i}",
            "will_retain",          priv->will_retain,
            "will_qos",             (int)priv->will_qos,
            "will_topic",           priv->will_topic,
            "will_delay_interval",  (int)priv->will_delay_interval,
            "will_expiry_interval", (int)priv->will_expiry_interval
        );
        if(priv->gbuf_will_payload) {
            json_object_set_new(
                jn_will,
                "gbuffer",
                json_integer((json_int_t)(uintptr_t)priv->gbuf_will_payload)
            );
            priv->gbuf_will_payload = NULL;
        }
        json_object_update_new(client, jn_will);
    }
    JSON_DECREF(auth)

    if(connect_properties) {
        json_object_set_new(client, "connect_properties", connect_properties);
        connect_properties = NULL;
    }

    open_queues(gobj);

    priv->inform_on_close = TRUE;
    int ret = gobj_publish_event(gobj, EV_ON_OPEN, client);
    if(ret < 0) {
        if(ret == -2) { // TODO review
            if(priv->protocol_version == mosq_p_mqtt5) {
                send__connack(gobj, 0, MQTT_RC_NOT_AUTHORIZED, NULL);
            } else {
                send__connack(gobj, 0, CONNACK_REFUSED_NOT_AUTHORIZED, NULL);
            }
        }

        JSON_DECREF(connack_props);
        return -1;
    }

    if(ret == 1) {
        connect_ack |= 0x01;
    }
    send__connack(
        gobj,
        connect_ack,
        CONNACK_ACCEPTED,
        connack_props // owned
    );

    gobj_write_bool_attr(gobj, "in_session", TRUE);
    gobj_write_bool_attr(gobj, "send_disconnect", TRUE);


    // TODO
    // db__expire_all_messages(context);
    // db__message_write_queued_out(context);
    // db__message_write_inflight_out_all(context);

    if(priv->keepalive > 0) {
        // priv->timer_ping = start_sectimer(priv->keepalive);
    }

    return 0;
}

/***************************************************************************
 *  Only for clients
 ***************************************************************************/
PRIVATE int handle__connack(
    hgobj gobj,
    gbuffer_t *gbuf,
    json_t *jn_data  // not owned
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint8_t connect_flags;
    uint8_t reason_code;
    int ret = 0;
    json_t *properties = NULL;

    if(mqtt_read_byte(gobj, gbuf, &connect_flags)<0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt malformed packet, not enough data",
            NULL
        );
        return MOSQ_ERR_PROTOCOL;
    }
    if(mqtt_read_byte(gobj, gbuf, &reason_code)<0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt malformed packet, not enough data",
            NULL
        );
        return MOSQ_ERR_PROTOCOL;
    }

    json_object_set_new(jn_data, "reason_code", json_integer(reason_code));
    json_object_set_new(jn_data, "connect_flags", json_integer(connect_flags));
    json_object_set_new(jn_data, "protocol_version", json_integer(priv->protocol_version));

    if(priv->protocol_version == mosq_p_mqtt5) {
        properties = property_read_all(gobj, gbuf, CMD_CONNACK, &ret);
        if(ret == MOSQ_ERR_PROTOCOL && reason_code == CONNACK_REFUSED_PROTOCOL_VERSION) {
            /* This could occur because we are connecting to a v3.x broker and
             * it has replied with "unacceptable protocol version", but with a
             * v3 CONNACK. */
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Error in properties",
                "command",      "%s", get_command_name(CMD_CONNACK),
                "reason",       "%s", mosquitto_reason_string(MQTT_RC_UNSUPPORTED_PROTOCOL_VERSION),
                NULL
            );
            // TODO connack_callback(mosq, MQTT_RC_UNSUPPORTED_PROTOCOL_VERSION, connect_flags, NULL);
            return ret;

        } else if(ret<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Error in properties",
                "command",      "%s", get_command_name(CMD_CONNACK),
                NULL
            );
            return ret;
        }
    }

    if(properties) {
        const char *clientid = property_read_string(properties, MQTT_PROP_ASSIGNED_CLIENT_IDENTIFIER);
        if(!empty_string(clientid)) {
            if(!empty_string(gobj_read_str_attr(gobj, "mqtt_client_id"))) {
                /* We've been sent a client identifier but already have one. This
                 * shouldn't happen. */
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "assigned client identifier",
                    "command",      "%s", get_command_name(CMD_CONNACK),
                    NULL
                );

                JSON_DECREF(properties);
                return MOSQ_ERR_PROTOCOL;
            } else {
                gobj_write_str_attr(gobj, "mqtt_client_id", clientid);
                clientid = NULL;
            }
        }

        // TODO review how save properties
        // const char *key; json_t *value;
        // json_object_foreach(properties, key, value) {
        //     json_object_set(
        //         jn_data,
        //         kw_get_str(gobj, value, "name", "", KW_REQUIRED),
        //         kw_get_dict_value(gobj, value, "value", 0, KW_REQUIRED)
        //     );
        // }

        json_object_set_new(jn_data, "properties", properties);

        // TODO integrate in session the properties
        // uint8_t retain_available = property_read_byte(properties, MQTT_PROP_RETAIN_AVAILABLE);
        // uint8_t max_qos = property_read_byte(properties, MQTT_PROP_MAXIMUM_QOS);
        // uint16_t inflight_maximum = property_read_int16(properties, MQTT_PROP_RECEIVE_MAXIMUM);
        // uint16_t keepalive = property_read_int16(properties, MQTT_PROP_SERVER_KEEP_ALIVE);
        // uint32_t maximum_packet_size = property_read_int32(properties, MQTT_PROP_MAXIMUM_PACKET_SIZE);
    }

    priv->msgs_out.inflight_quota = priv->msgs_out.inflight_maximum;
    // message__reconnect_reset(mosq, true); TODO important?!

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("👈👈 COMMAND=%s, reason code: %d '%s'",
            get_command_name(CMD_CONNACK),
            reason_code,
            priv->protocol_version == mosq_p_mqtt5?
                mosquitto_reason_string(reason_code) :
                mosquitto_connack_string(reason_code)
        );
        if(properties) {
            print_json2("CONNACK properties", properties);
        }
    }

    // json_t *kw_iev = iev_create(
    //     gobj,
    //     EV_MQTT_CONNECTED,
    //     json_incref(jn_data) // owned
    // );
    //
    // gobj_publish_event(gobj, EV_ON_IEV_MESSAGE, kw_iev);

    switch(reason_code) {
        case 0:
            // TODO message__retry_check(mosq); important!
            return MOSQ_ERR_SUCCESS;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            {
                // TODO don't retry connections?
                const char *reason = priv->protocol_version != mosq_p_mqtt5?
                    mosquitto_connack_string(reason_code):mosquitto_reason_string(reason_code);
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt connection refused",
                    "reason",       "%s", reason,
                    NULL
                );
            }
            return MOSQ_ERR_CONN_REFUSED;
        default:
            return MOSQ_ERR_PROTOCOL;
    }
}

/***************************************************************************
 *  Server: process disconnect from client
 ***************************************************************************/
PRIVATE int handle__disconnect_s(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *properties = 0;
    uint8_t reason_code = 0;

    if(priv->protocol_version == mosq_p_mqtt5 && gbuf && gbuffer_leftbytes(gbuf) > 0) {
        /* FIXME - must handle reason code */
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
            int ret;
            properties = property_read_all(gobj, gbuf, CMD_DISCONNECT, &ret);
            if(!properties) {
                return ret;
            }
        }
    }
    if(properties) {
        json_t *property = property_get_property(properties, MQTT_PROP_SESSION_EXPIRY_INTERVAL);
        int session_expiry_interval = (int)kw_get_int(gobj, property, "value", -1, 0);
        if(session_expiry_interval != -1) {
            // TODO review
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

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("  👈 Received DISCONNECT from client '%s' (%d '%s')",
            SAFE_PRINT(priv->client_id),
            reason_code,
            mosquitto_reason_string(reason_code)
        );
        if(properties) {
            gobj_trace_json(gobj, properties, "properties");
        }
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
        // TODO
        //will__clear(context);
        //mosquitto__set_state(context, mosq_cs_disconnecting);
    }

    do_disconnect(gobj, MOSQ_ERR_SUCCESS);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__disconnect_c(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    if(priv->protocol_version != mosq_p_mqtt5) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Receiving cmd_disconnect and not mqtt v5",
            NULL
        );
        return MOSQ_ERR_PROTOCOL;
    }

    uint8_t reason_code = 0;
    mqtt_read_byte(gobj, gbuf, &reason_code);

    json_t *properties = NULL;
    if(gbuf && gbuffer_leftbytes(gbuf) > 2) {
        properties = property_read_all(gobj, gbuf, CMD_DISCONNECT, &ret);
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("  👈 Received DISCONNECT from client '%s' (%d '%s')",
            SAFE_PRINT(priv->client_id),
            reason_code,
            mosquitto_reason_string(reason_code)
        );
        if(properties) {
            gobj_trace_json(gobj, properties, "properties");
        }
    }

    /* TODO Free data and reset values */
    do_disconnect(gobj, reason_code);
    return 0;
}

/***************************************************************************
 *  Add a subscription, return MOSQ_ERR_SUB_EXISTS or MOSQ_ERR_SUCCESS
 ***************************************************************************/
PRIVATE int sub__add(
    hgobj gobj,
    const char *sub, // topic? TODO change name
    uint8_t qos,
    json_int_t identifier,
    mqtt5_sub_options_t options
)
{
#ifdef PEPE
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
#endif
    return 0;
}

/***************************************************************************
 *  Remove a subscription
 ***************************************************************************/
PRIVATE int sub__remove(
    hgobj gobj,
    const char *sub,// topic? TODO change name
    uint8_t *reason
)
{
#ifdef PEPE
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
#endif
    return 0;
}

/***************************************************************************
 *  Subscription: search if the topic has a retain message and process
 *  TODO study retain.c and implement it
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
 *  Server
 ***************************************************************************/
PRIVATE int handle__subscribe(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint16_t mid;
    uint8_t subscription_options;
    json_int_t subscription_identifier = 0;
    uint8_t qos;
    uint8_t retain_handling = 0;
    uint16_t slen;
    json_t *properties = NULL;

    if(priv->frame_head.flags != 2) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt subscribe: flags != 2",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(mqtt_read_uint16(gobj, gbuf, &mid)) {
        // Error already logged
        return MOSQ_ERR_MALFORMED_PACKET;
    }
    if(mid == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt subscribe: mid == 0",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(priv->protocol_version == mosq_p_mqtt5) {
        int rc;
        properties = property_read_all(gobj, gbuf, CMD_SUBSCRIBE, &rc);
        if(rc) {
            // Error already logged
            return rc;
        }

        subscription_identifier = property_read_varint(properties, MQTT_PROP_SUBSCRIPTION_IDENTIFIER);
        if(subscription_identifier != -1) {
            /* If the identifier was force set to 0, this is an error */
            if(subscription_identifier == 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt subscribe: subscription_identifier == 0",
                    "client_id",    "%s", priv->client_id,
                    NULL
                );
                JSON_DECREF(properties)
                return MOSQ_ERR_MALFORMED_PACKET;
            }
        }
    }

    json_t *jn_list = json_array();

    while(gbuffer_leftbytes(gbuf)>0) {
        char *sub = NULL;
        if(mqtt_read_string(gobj, gbuf, &sub, &slen)<0) {
            // Error already logged
            JSON_DECREF(jn_list)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        if(!slen) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt subscribe: Empty subscription string",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            JSON_DECREF(jn_list)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        if(mosquitto_sub_topic_check(sub)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt subscribe: Invalid subscription string",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            JSON_DECREF(jn_list)
            return MOSQ_ERR_MALFORMED_PACKET;
        }

        if(mqtt_read_byte(gobj, gbuf, &subscription_options)) {
            // Error already logged
            JSON_DECREF(jn_list)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        if(priv->protocol_version == mosq_p_mqtt31 ||
            priv->protocol_version == mosq_p_mqtt311
        ) {
            qos = subscription_options;
            if(priv->is_bridge) {
                subscription_options = MQTT_SUB_OPT_RETAIN_AS_PUBLISHED | MQTT_SUB_OPT_NO_LOCAL;
            }
        } else {
            qos = subscription_options & 0x03;
            subscription_options &= 0xFC;

            if((subscription_options & MQTT_SUB_OPT_NO_LOCAL) && !strncmp(sub, "$share/", 7)) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt subscribe: Invalid subscription options 1",
                    "client_id",    "%s", priv->client_id,
                    NULL
                );
                JSON_DECREF(jn_list)
                return MOSQ_ERR_PROTOCOL;
            }

            retain_handling = (subscription_options & 0x30);
            if(retain_handling == 0x30 || (subscription_options & 0xC0) != 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt subscribe: Invalid subscription options 2",
                    "client_id",    "%s", priv->client_id,
                    NULL
                );
                JSON_DECREF(jn_list)
                return MOSQ_ERR_MALFORMED_PACKET;
            }
        }
        if(qos > 2) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt subscribe: Invalid QoS in subscription command",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            JSON_DECREF(jn_list)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        if(qos > priv->max_qos) {
            qos = priv->max_qos;
        }

        json_t *jn_sub = json_pack("{s:s, s:i, s:i, s:i, s:i}",
            "sub", sub,
            "qos", (int)qos,
            "subscription_identifier", (int)subscription_identifier,
            "subscription_options", (int)subscription_options,
            "retain_handling", retain_handling
        );
        json_array_append_new(jn_list, jn_sub);

        if(gobj_trace_level(gobj) & SHOW_DECODE) {
            trace_msg0("  👈 Received SUBSCRIBE from client '%s', topic '%s' (QoS %d)",
                priv->client_id,
                sub,
                qos
            );
        }

    }

    if(priv->protocol_version != mosq_p_mqtt31) {
        if(!json_size(jn_list)) {
            /* No subscriptions specified, protocol error. */
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt subscribe: No subscriptions specified",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            JSON_DECREF(jn_list)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
    }


    gbuffer_t *gbuf_payload = gbuffer_create(256, 12*1024);

    int idx; json_t *jn_sub;
    json_array_foreach(jn_list, idx, jn_sub) {
        const char *sub = kw_get_str(gobj, jn_sub, "sub", NULL, KW_REQUIRED);
        int qos = kw_get_int(gobj, jn_sub, "qos", 0, KW_REQUIRED);
        int subscription_identifier = kw_get_int(gobj, jn_sub, "subscription_identifier", 0, KW_REQUIRED);
        int subscription_options = kw_get_int(gobj, jn_sub, "subscription_options", 0, KW_REQUIRED);
        int retain_handling = kw_get_int(gobj, jn_sub, "retain_handling", 0, KW_REQUIRED);

        BOOL allowed = TRUE;
        // allowed = mosquitto_acl_check(context, sub, 0, NULL, qos, FALSE, MOSQ_ACL_SUBSCRIBE); TODO
        if(!allowed) {
            if(priv->protocol_version == mosq_p_mqtt5) {
                qos = MQTT_RC_NOT_AUTHORIZED;
            } else if(priv->protocol_version == mosq_p_mqtt311) {
                qos = 0x80;
            }

        }

        // if(allowed) {
        //     rc2 = sub__add(
        //         gobj,
        //         sub,
        //         qos,
        //         subscription_identifier,
        //         subscription_options
        //     );
        //     if(rc2 < 0) {
        //         JSON_DECREF(jn_list)
        //         return rc2;
        //     }
        //
        //     if(priv->protocol_version == mosq_p_mqtt311 || priv->protocol_version == mosq_p_mqtt31) {
        //         if(rc2 == MOSQ_ERR_SUCCESS || rc2 == MOSQ_ERR_SUB_EXISTS) {
        //             if(retain__queue(gobj, sub, qos, 0)) {
        //                 // rc = MOSQ_ERR_NOMEM;
        //             }
        //         }
        //     } else {
        //         if((retain_handling == MQTT_SUB_OPT_SEND_RETAIN_ALWAYS)
        //                 || (rc2 == MOSQ_ERR_SUCCESS && retain_handling == MQTT_SUB_OPT_SEND_RETAIN_NEW)
        //           ) {
        //             if(retain__queue(gobj, sub, qos, subscription_identifier)) {
        //                 // rc = MOSQ_ERR_NOMEM;
        //             }
        //         }
        //     }
        // }

        gbuffer_append_char(gbuf_payload, qos);
    }

    // if(priv->current_out_packet == NULL) {
    //     rc = db__message_write_queued_out(gobj);
    //     if(rc) {
    //         return rc;
    //     }
    //     rc = db__message_write_inflight_out_latest(gobj);
    //     if(rc) {
    //         return rc;
    //     }
    // }


    return send__suback(
        gobj,
        mid,
        gbuf_payload, // owned
        properties // owned
    );
}

/***************************************************************************
 *  Server: Send subscribe ack to client
 ***************************************************************************/
PRIVATE int send__suback(
    hgobj gobj,
    uint16_t mid,
    gbuffer_t *gbuf_payload,
    json_t *properties
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint32_t payloadlen = gbuf_payload?gbuffer_leftbytes(gbuf_payload):0;
    uint32_t remaining_length = 2 + payloadlen;

    if(priv->protocol_version == mosq_p_mqtt5) {
        /* We don't use Reason String or User Property yet. */
        if(properties) {
            remaining_length += property__get_remaining_length(properties);
        }
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("👉👉 %s Sending SUBACK to '%s' %s",
            priv->iamServer?"broker":"client",
            priv->client_id,
            gobj_short_name(gobj_bottom_gobj(gobj))
        );
        if(payloadlen > 0) {
            gobj_trace_dump_gbuf(gobj, gbuf_payload, "SUBACK payload");
        }
        if(properties) {
            gobj_trace_json(gobj, properties, "SUBACK properties");
        }
    }

    gbuffer_t *gbuf = build_mqtt_packet(gobj, CMD_SUBACK, remaining_length);
    mqtt_write_uint16(gbuf, mid);

    if(priv->protocol_version == mosq_p_mqtt5) {
        if(properties) {
            property__write_all(gobj, gbuf, properties, TRUE);
        }
    }

    if(payloadlen) {
        mqtt_write_bytes(
            gbuf,
            gbuffer_cur_rd_pointer(gbuf_payload),
            payloadlen
        );
    }

    GBUFFER_DECREF(gbuf_payload)
    JSON_DECREF(properties)

    return send_packet(gobj, gbuf);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int handle__unsubscribe(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint16_t mid;
    uint16_t slen;
    int reason_code_count = 0;
    json_t *properties = NULL;

    if(priv->frame_head.flags != 2) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt unsubscribe: flags != 2",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(mqtt_read_uint16(gobj, gbuf, &mid)) {
        // Error already logged
        return MOSQ_ERR_MALFORMED_PACKET;
    }
    if(mid == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt unsubscribe: mid == 0",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(priv->protocol_version == mosq_p_mqtt5) {
        int rc;
        properties = property_read_all(gobj, gbuf, CMD_UNSUBSCRIBE, &rc);
        if(rc<0) {
            // Error already logged
            return rc;
        }
    }

    if(priv->protocol_version == mosq_p_mqtt311 || priv->protocol_version == mosq_p_mqtt5) {
        if(gbuffer_leftbytes(gbuf)==0) {
            /* No topic specified, protocol error. */
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt unsubscribe: no topic specified",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            return MOSQ_ERR_MALFORMED_PACKET;
        }
    }

    json_t *jn_list = json_array();

    while(gbuffer_leftbytes(gbuf)>0) {
        char *sub = NULL;
        if(mqtt_read_string(gobj, gbuf, &sub, &slen)<0) {
            // Error already logged
            JSON_DECREF(jn_list)
            return MOSQ_ERR_MALFORMED_PACKET;
        }

        if(!slen) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Empty unsubscription string",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            JSON_DECREF(jn_list)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        if(mosquitto_sub_topic_check(sub)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Invalid unsubscription string",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            JSON_DECREF(jn_list)
            return MOSQ_ERR_MALFORMED_PACKET;
        }

        if(gobj_trace_level(gobj) & SHOW_DECODE) {
            trace_msg0("  👈 Received UNSUBSCRIBE from client '%s', topic '%s'",
                priv->client_id,
                sub
            );
        }

        json_t *jn_sub = json_pack("{s:s}",
            "sub", sub
        );
        json_array_append_new(jn_list, jn_sub);
        reason_code_count++;
    }

    gbuffer_t *gbuf_payload = gbuffer_create(256, 12*1024);

    int idx; json_t *jn_sub;
    json_array_foreach(jn_list, idx, jn_sub) {
        const char *sub = kw_get_str(gobj, jn_sub, "sub", NULL, KW_REQUIRED);

        // /* ACL check */
        int reason = 0;
        BOOL allowed = TRUE;
        // allowed = mosquitto_acl_check(context, sub, 0, NULL, 0, FALSE, MOSQ_ACL_UNSUBSCRIBE); TODO
        if(allowed) {
            // sub__remove(gobj, sub, &reason);
        } else {
            reason = MQTT_RC_NOT_AUTHORIZED;
        }
        gbuffer_append_char(gbuf_payload, reason);
    }

    return send__unsuback(
        gobj,
        mid,
        gbuf_payload, // owned
        properties // owned
    );
}

/***************************************************************************
 *  Server: Send unsubscribe ack to client
 ***************************************************************************/
PRIVATE int send__unsuback(
    hgobj gobj,
    uint16_t mid,
    gbuffer_t *gbuf_payload,  // owned
    json_t *properties  //  owned
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("👉👉 %s Sending UNSUBACK to '%s' %s",
            priv->iamServer?"broker":"client",
            priv->client_id,
            gobj_short_name(gobj_bottom_gobj(gobj))
        );
    }

    uint32_t remaining_length = 2;
    uint32_t reason_code_count = gbuffer_leftbytes(gbuf_payload);

    if(priv->protocol_version == mosq_p_mqtt5) {
        if(properties) {
            remaining_length += property__get_remaining_length(properties);
            remaining_length += (uint32_t)reason_code_count;
        }
    }

    gbuffer_t *gbuf = build_mqtt_packet(gobj, CMD_UNSUBACK, remaining_length);
    mqtt_write_uint16(gbuf, mid);

    if(priv->protocol_version == mosq_p_mqtt5) {
        if(properties) {
            property__write_all(gobj, gbuf, properties, TRUE);
        }
        mqtt_write_bytes(
            gbuf,
            gbuffer_cur_rd_pointer(gbuf_payload),
            (uint32_t)reason_code_count
        );
    }

    GBUFFER_DECREF(gbuf_payload)
    JSON_DECREF(properties)

    return send_packet(gobj, gbuf);
}

/***************************************************************************
 *  Only clients, bridge not implemented
 ***************************************************************************/
PRIVATE int handle__suback(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint16_t mid = 0;
    int rc;
    json_t *properties = NULL;

    rc = mqtt_read_uint16(gobj, gbuf, &mid);
    if(rc<0) {
        // Error already logged
        return rc;
    }
    if(mid == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Suback with mid == 0",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        return MOSQ_ERR_PROTOCOL;
    }

    if(priv->protocol_version == mosq_p_mqtt5) {
        properties = property_read_all(gobj, gbuf, CMD_SUBACK, &rc);
        if(rc<0) {
            // Error already logged
            return rc;
        }
    }

    json_t *granted_qos = json_array();

    while(gbuffer_leftbytes(gbuf)>0) {
        uint8_t qos;
        rc = mqtt_read_byte(gobj, gbuf, &qos);
        if(rc<0) {
            // Error already logged
            JSON_DECREF(granted_qos)
            JSON_DECREF(properties)
            return rc;
        }
        json_array_append_new(granted_qos, json_integer(qos));
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("  👈 Received SUBACK from client '%s' (Mid: %d)",
            SAFE_PRINT(priv->client_id),
            mid
        );
        gobj_trace_json(gobj, granted_qos, "granted qos");
    }

    json_t *kw_suback = json_pack("{s:i, s:o, s:o}",
        "mid", (int)mid,
        "granted_qos", granted_qos,
        "properties", properties?properties:json_object()
    );

    json_t *kw_iev = iev_create(
        gobj,
        EV_MQTT_SUBSCRIBE,
        kw_suback // owned
    );

    gobj_publish_event(gobj, EV_ON_IEV_MESSAGE, kw_iev);

    return 0;
}

/***************************************************************************
 *  Only clients, bridge not implemented
 ***************************************************************************/
PRIVATE int handle__unsuback(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint16_t mid = 0;
    int rc;
    json_t *properties = NULL;

    rc = mqtt_read_uint16(gobj, gbuf, &mid);
    if(rc<0) {
        // Error already logged
        return rc;
    }
    if(mid == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Unsuback with mid == 0",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        return MOSQ_ERR_PROTOCOL;
    }

    if(priv->protocol_version == mosq_p_mqtt5) {
        properties = property_read_all(gobj, gbuf, CMD_UNSUBACK, &rc);
        if(rc<0) {
            // Error already logged
            return rc;
        }
    }

    json_t *reason_codes = json_array();

    while(gbuffer_leftbytes(gbuf)>0) {
        uint8_t reason_code;
        rc = mqtt_read_byte(gobj, gbuf, &reason_code);
        if(rc<0) {
            // Error already logged
            JSON_DECREF(reason_codes)
            JSON_DECREF(properties)
            return rc;
        }
        json_array_append_new(reason_codes, json_integer(reason_code));
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("  👈 Received UNSUBACK from client '%s' (Mid: %d)",
            SAFE_PRINT(priv->client_id),
            mid
        );
    }

    json_t *kw_suback = json_pack("{s:i, s:o, s:o}",
        "mid", (int)mid,
        "reason_codes", reason_codes,
        "properties", properties?properties:json_object()
    );

    json_t *kw_iev = iev_create(
        gobj,
        EV_MQTT_UNSUBSCRIBE,
        kw_suback // owned
    );

    gobj_publish_event(gobj, EV_ON_IEV_MESSAGE, kw_iev);

    return 0;
}

/***************************************************************************
 *  Server: receive publish from client
 ***************************************************************************/
PRIVATE int handle__publish_s(
    hgobj gobj,
    gbuffer_t *gbuf // not owned
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int rc = 0;
    int rc2;
    int res = 0;
    uint16_t slen;
    json_t *properties = NULL;
    // mosquitto_property *p, *p_prev;
    // mosquitto_property *msg_properties_last;
    uint32_t message_expiry_interval = 0;
    int topic_alias = -1;
    uint8_t reason_code = 0;
    uint16_t mid = 0;
    gbuffer_t *payload = NULL;

    uint8_t header = priv->frame_head.flags;
    BOOL dup = (header & 0x08)>>3;
    uint8_t qos = (header & 0x06)>>1;
    BOOL retain = (header & 0x01);

    if(dup == 1 && qos == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: Invalid PUBLISH (QoS=0 and DUP=1), disconnecting",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        return MOSQ_ERR_MALFORMED_PACKET;
    }
    if(qos == 3) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: Invalid QoS in PUBLISH, disconnecting",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        return MOSQ_ERR_MALFORMED_PACKET;
    }
    if(qos > priv->max_qos) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: Too high QoS in PUBLISH, disconnecting",
            "client_id",    "%s", priv->client_id,
            "max_qos",      "%d", (int)priv->max_qos,
            "qos",          "%d", (int)qos,
            NULL
        );
        return MOSQ_ERR_QOS_NOT_SUPPORTED;
    }

    if(retain && priv->retain_available == FALSE) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: Retain not supported in PUBLISH, disconnecting",
            "client_id",    "%s", priv->client_id,
            "max_qos",      "%d", (int)priv->max_qos,
            "qos",          "%d", (int)qos,
            NULL
        );
        return MOSQ_ERR_RETAIN_NOT_SUPPORTED;
    }

    char *topic_;
    if(mqtt_read_string(gobj, gbuf, &topic_, &slen)<0) {
        // Error already logged
        return MOSQ_ERR_MALFORMED_PACKET;
    }
    char *topic = gbmem_strndup(topic_, slen);

    if(!slen && priv->protocol_version != mosq_p_mqtt5) {
        /* Invalid publish topic, disconnect client. */
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: topic len 0 and not mqtt5",
            "client_id",    "%s", priv->client_id,
            NULL
        );

        GBMEM_FREE(topic)
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(qos > 0) {
        if(mqtt_read_uint16(gobj, gbuf, &mid)<0) {
            // Error already logged
            GBMEM_FREE(topic)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        if(mid == 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt: qos>0 and mid=0",
                "client_id",    "%s", priv->client_id,
                "topic",        "%s", topic,
                "mid",          "%d", (int)mid,
                NULL
            );
            GBMEM_FREE(topic)
            return MOSQ_ERR_PROTOCOL;
        }
    }

    /* Handle properties */
    if(priv->protocol_version == mosq_p_mqtt5) {
        properties = property_read_all(gobj, gbuf, CMD_PUBLISH, &rc);
        if(rc<0) {
            // Error already logged
            GBMEM_FREE(topic)
            return rc;
        }

        const char *property_name; json_t *property;
        json_object_foreach(properties, property_name, property) {
            json_int_t identifier = kw_get_int(gobj, property, "identifier", 0, KW_REQUIRED);
            // TODO review parece que guarda las properties en msg_properties
            switch(identifier) {
                case MQTT_PROP_CONTENT_TYPE:
                case MQTT_PROP_CORRELATION_DATA:
                case MQTT_PROP_PAYLOAD_FORMAT_INDICATOR:
                case MQTT_PROP_RESPONSE_TOPIC:
                case MQTT_PROP_USER_PROPERTY:
                    {
                        // if(!msg->properties) {
                        //     msg->properties = json_object();
                        // }
                        // json_object_set(msg->properties, property_name, property);
                    }
                    break;

                case MQTT_PROP_TOPIC_ALIAS:
                    topic_alias = (int)kw_get_int(gobj, property, "value", 0, KW_REQUIRED);
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

    if(topic_alias == 0 || (topic_alias > priv->max_topic_alias)) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_MQTT_ERROR,
            "msg",              "%s", "Mqtt: invalid topic alias",
            "client_id",        "%s", priv->client_id,
            "max_topic_alias",  "%d", priv->max_topic_alias,
            "topic_alias",      "%d", topic_alias,
            "topic",            "%s", topic,
            "mid",              "%d", (int)mid,
            NULL
        );
        GBMEM_FREE(topic)
        JSON_DECREF(properties)
        return MOSQ_ERR_TOPIC_ALIAS_INVALID;
    } else if(topic_alias > 0) {
        if(topic) {
            // TODO
            // rc = alias__add(context, msg->topic, (uint16_t)topic_alias);
            // if(rc) {
            // GBMEM_FREE(topic)
        // JSON_DECREF(properties)
            //     return rc;
            // }
        } else {
            // TODO
            // rc = alias__find(context, &msg->topic, (uint16_t)topic_alias);
            // if(rc) {
            //     gobj_log_error(gobj, 0,
            //         "function",         "%s", __FUNCTION__,
            //         "msgset",           "%s", MSGSET_MQTT_ERROR,
            //         "msg",              "%s", "Mqtt: topic alias NOT FOUND",
            //         "client_id",        "%s", priv->client_id,
            //         "max_topic_alias",  "%d", priv->max_topic_alias,
            //         "topic_alias",      "%d", topic_alias,
            //         NULL
            //     );
            // GBMEM_FREE(topic)
        // JSON_DECREF(properties)
            //     return MOSQ_ERR_PROTOCOL;
            // }
        }
    }

    if(mosquitto_pub_topic_check(topic) != MOSQ_ERR_SUCCESS) {
        /* Invalid publish topic, just swallow it. */
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt will: invalid topic",
            "topic",        "%s", topic,
            "mid",          "%d", (int)mid,
            NULL
        );
        GBMEM_FREE(topic)
        JSON_DECREF(properties)
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    size_t payloadlen = gbuffer_leftbytes(gbuf);
    // G_PUB_BYTES_RECEIVED_INC(msg->payloadlen);

    if(payloadlen) {
        if(priv->message_size_limit && payloadlen > priv->message_size_limit) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_MQTT_ERROR,
                "msg",              "%s", "Mqtt: Dropped too large PUBLISH",
                "client_id",        "%s", priv->client_id,
                "topic",            "%d", topic,
                "mid",          "%d", (int)mid,
                NULL
            );
            reason_code = MQTT_RC_PACKET_TOO_LARGE;
            goto process_bad_message;
        }

        payload = gbuffer_create(payloadlen, payloadlen);
        if(!payload) {
            // Error already logged
            GBMEM_FREE(topic)
            JSON_DECREF(properties)
            return MOSQ_ERR_NOMEM;
        }

        if(mqtt_read_bytes(gobj, gbuf, gbuffer_cur_wr_pointer(payload), (int)payloadlen)) {
            GBMEM_FREE(topic)
            JSON_DECREF(properties)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        gbuffer_set_wr(payload, payloadlen);
    }

    /* Check for topic access */
    // TODO
    rc = 0;
    // rc = mosquitto_acl_check(context, msg->topic, msg->payloadlen, msg->payload, msg->qos, msg->retain, MOSQ_ACL_WRITE);
    if(rc == MOSQ_ERR_ACL_DENIED) {
        //     gobj_log_error(gobj, 0,
        //         "function",         "%s", __FUNCTION__,
        //         "msgset",           "%s", MSGSET_MQTT_ERROR,
        //         "msg",              "%s", "Mqtt: Denied PUBLISH",
        //         "client_id",        "%s", priv->client_id,
        //         "topic",            "%d", msg->topic,
            // "mid",          "%d", (int)mid,
        //         NULL
        //     );
        reason_code = MQTT_RC_NOT_AUTHORIZED;
        goto process_bad_message;
    } else if(rc != MOSQ_ERR_SUCCESS) {
        GBMEM_FREE(topic)
        JSON_DECREF(properties)
        return rc;
    }

    time_t t = mosquitto_time();
    user_flag_t user_flag;
    json_t *kw_mqtt_msg = new_json_message(
        gobj,
        mid,
        topic,
        payload, // not owned
        qos,
        retain,
        dup,
        properties, // not owned
        message_expiry_interval,
        mosq_mo_client,
        mosq_md_in,
        &user_flag,
        t
    );

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("  👈 Received PUBLISH from client '%s', topic '%s' (dup %d, qos %d, retain %d, mid %d, len %ld)",
            priv->client_id,
            topic,
            dup,
            (int)qos,
            (int)retain,
            (int)mid,
            (long)payloadlen
        );
    }

    if(!strncmp(topic, "$CONTROL/", 9)) {
#ifdef WITH_CONTROL
        rc = control__process(context, msg);
        db__msg_store_free(msg);
        return rc;
#else
        reason_code = MQTT_RC_IMPLEMENTATION_SPECIFIC;
        goto process_bad_message;
#endif
    }

    {
        // rc = plugin__handle_message(context, msg);
        // if(rc == MOSQ_ERR_ACL_DENIED) {
        //     log__printf(NULL, MOSQ_LOG_DEBUG,
        //             "Denied PUBLISH from %s (d%d, q%d, r%d, m%d, '%s', ... (%ld bytes))",
        //             context->id, dup, msg->qos, msg->retain, msg->source_mid, msg->topic,
        //             (long)msg->payloadlen);
        //
        //     reason_code = MQTT_RC_NOT_AUTHORIZED;
        //     goto process_bad_message;
        // } else if(rc != MOSQ_ERR_SUCCESS) {
        //     db__msg_store_free(msg);
        //     return rc;
        // }
    }

    GBMEM_FREE(topic)
    JSON_DECREF(properties)

    /*
     *  Here mosquitto checked the inflight input queue to search repeated mid's
     *  with db__message_store_find() and db__message_remove_incoming() functions.
     *  Not good for performance, better to have a periodic timer that cleans incomplete mid's
     *  and searching in the inflight queue ONLY if the message has the DUP flag.
     */
    if(dup && qos==2) {
        db__message_remove_incoming_dup(gobj, mid); // Search and remove without warning
    }
    if(qos == 2) {
        if(!db__ready_for_flight(gobj, mosq_md_in, qos)) {
            /* Client isn't allowed any more incoming messages, so fail early */
            reason_code = MQTT_RC_QUOTA_EXCEEDED;
            goto process_bad_message;
        }
    }

    switch(qos) {
        case 0:
            rc = sub__messages_queue(
                gobj,
                kw_mqtt_msg // not owned
            );
            break;

        case 1:
            // util__decrement_receive_quota(context);
            rc = sub__messages_queue(
                gobj,
                kw_mqtt_msg // not owned
            );
            /* stored may now be free, so don't refer to it */
            if(rc == MOSQ_ERR_SUCCESS || priv->protocol_version != mosq_p_mqtt5) {
                send__puback(gobj, mid, 0, NULL);
            } else if(rc == MOSQ_ERR_NO_SUBSCRIBERS) {
                send__puback(gobj, mid, MQTT_RC_NO_MATCHING_SUBSCRIBERS, NULL);
            }
            break;

        case 2:
            rc = db__message_insert(
                gobj,
                kw_mqtt_msg
                // mosq_md_in,
                // NULL,
                // FALSE
            );
            if(rc == MOSQ_ERR_SUCCESS) {
                send__pubrec(gobj, mid, 0, NULL);
            }
            break;
    }

    db__message_write_queued_in(gobj);
    return rc;

process_bad_message:
    rc = MOSQ_ERR_UNKNOWN;
    if(kw_mqtt_msg) { // TODO get user_flag
        switch(user_flag_get_qos_level(&user_flag)) {
            case 0:
                rc = MOSQ_ERR_SUCCESS;
                break;
            case 1:
                rc = send__puback(gobj, mid, reason_code, NULL);
                break;
            case 2:
                rc = send__pubrec(gobj, mid, reason_code, NULL);
                break;
        }
        db__msg_store_free(kw_mqtt_msg);
    }

    // TODO review
    // if(priv->max_queued_messages > 0 && priv->out_packet_count >= priv->max_queued_messages) {
    //     rc = MQTT_RC_QUOTA_EXCEEDED;
    // }

    GBMEM_FREE(topic)
    JSON_DECREF(properties)
    KW_DECREF(kw_mqtt_msg)
    return rc;
}

/***************************************************************************
 *  Client: receive publish from broker
  ***************************************************************************/
PRIVATE int handle__publish_c(
    hgobj gobj,
    gbuffer_t *gbuf // not owned
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int rc = 0;
    uint16_t mid = 0;
    uint16_t slen;
    json_t *properties = NULL;
    uint8_t qos;
    BOOL retain;
    BOOL dup;
    uint32_t expiry_interval;

    uint8_t header = priv->frame_head.flags;
    dup = (header & 0x08)>>3;
    qos = (header & 0x06)>>1;
    retain = (header & 0x01);

    char *topic_;
    if(mqtt_read_string(gobj, gbuf, &topic_, &slen)<0) {
        // Error already logged
        return MOSQ_ERR_MALFORMED_PACKET;
    }
    if(!slen) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: topic len 0",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        return MOSQ_ERR_PROTOCOL;
    }
    char *topic = gbmem_strndup(topic_, slen);

    if(qos > 0) {
        if((rc=mqtt_read_uint16(gobj, gbuf, &mid))<0) {
            // Error already logged
            GBMEM_FREE(topic)
            return rc;
        }

        if(mid == 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt: discard message, qos>0 and mid=0",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            GBMEM_FREE(topic)
            return MOSQ_ERR_PROTOCOL;
        }
    }

    if(priv->protocol_version == mosq_p_mqtt5) {
        properties = property_read_all(gobj, gbuf, CMD_PUBLISH, &rc);
        if(rc<0) {
            // Error already logged
            GBMEM_FREE(topic)
            return rc;
        }
    }

    size_t payloadlen = gbuffer_leftbytes(gbuf);
    gbuffer_t *payload = NULL;
    if(payloadlen) {
        payload = gbuffer_create(payloadlen, payloadlen);
        if(!payload) {
            // Error already logged
            GBMEM_FREE(topic)
            JSON_DECREF(properties)
            return MOSQ_ERR_NOMEM;
        }

        void *pwr = gbuffer_cur_wr_pointer(payload);
        if(mqtt_read_bytes(gobj, gbuf, pwr, (int)payloadlen)) {
            GBMEM_FREE(topic)
            JSON_DECREF(properties)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        gbuffer_set_wr(payload, payloadlen);
    }

    time_t t = mosquitto_time();
    user_flag_t user_flag;
    json_t *kw_mqtt_msg = new_json_message(
        gobj,
        mid,
        topic,
        payload, // not owned
        qos,
        retain,
        dup,
        properties, // not owned
        0, // TODO expiry_interval,
        mosq_mo_broker,
        mosq_md_in,
        &user_flag,
        t
    );

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("  👈 Received PUBLISH from server '%s', topic '%s' (dup %d, qos %d, retain %d, mid %d, len %ld)",
            priv->client_id,
            topic,
            dup,
            qos,
            retain,
            mid,
            (long)payloadlen
        );
    }

    GBMEM_FREE(topic)
    JSON_DECREF(properties)

    switch(qos) {
        case 0:
            {
                json_t *kw_iev = iev_create(
                    gobj,
                    EV_MQTT_MESSAGE,
                    kw_mqtt_msg // owned
                );

                gobj_publish_event(gobj, EV_ON_IEV_MESSAGE, kw_iev);
            }
            return MOSQ_ERR_SUCCESS;

        case 1:
            // util__decrement_receive_quota(mosq);
            rc = send__puback(gobj, mid, 0, NULL);
            {
                json_t *kw_iev = iev_create(
                    gobj,
                    EV_MQTT_MESSAGE,
                    kw_mqtt_msg // owned
                );

                gobj_publish_event(gobj, EV_ON_IEV_MESSAGE, kw_iev);
            }
            return rc;

        case 2:
            // util__decrement_receive_quota(mosq);
            rc = send__pubrec(gobj, mid, 0, NULL);
            user_flag_set_state(&user_flag, mosq_ms_wait_for_pubrel);
            message__queue(gobj, kw_mqtt_msg, mosq_md_in, user_flag, t);
            return rc;

        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt: discard message, qos invalid",
                "client_id",    "%s", priv->client_id,
                "qos",          "%d", qos,
                NULL
            );
            JSON_DECREF(kw_mqtt_msg)
            return -1;
    }
}

/***************************************************************************
 *  Handle PUBACK or PUBCOMP commands
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
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt pubackcomp: flags != 0",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            return MOSQ_ERR_MALFORMED_PACKET;
        }
    }

    //util__increment_send_quota(mosq);

    rc = mqtt_read_uint16(gobj, gbuf, &mid);
    if(rc<0) {
        // Error already logged
        return rc;
    }
    if(type[3] == 'A') { /* pubAck or pubComp */
        if(priv->frame_head.command != CMD_PUBACK) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt pubackcomp: 'A' and no CMD_PUBACK",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        qos = 1;
    } else {
        if(priv->frame_head.command != CMD_PUBCOMP) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt pubackcomp: not 'A' and no CMD_PUBCOMP",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        qos = 2;
    }
    if(mid == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt pubackcomp: mid == 0",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        return MOSQ_ERR_PROTOCOL;
    }

    if(priv->protocol_version == mosq_p_mqtt5 && gbuffer_leftbytes(gbuf) > 2) {
        rc = mqtt_read_byte(gobj, gbuf, &reason_code);
        if(rc<0) {
            // Error already logged
            return rc;
        }

        if(gbuffer_leftbytes(gbuf) > 3) {
            properties = property_read_all(gobj, gbuf, CMD_PUBACK, &rc);
            if(rc<0) {
                // Error already logged
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
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt pubackcomp: bad reason code",
                    "client_id",    "%s", priv->client_id,
                    "reason_code",  "%d", reason_code,
                    NULL
                );
                JSON_DECREF(properties)
                return MOSQ_ERR_PROTOCOL;
            }
        } else {
            if(reason_code != MQTT_RC_SUCCESS
                    && reason_code != MQTT_RC_PACKET_ID_NOT_FOUND
            ) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt pubackcomp: bad reason code",
                    "client_id",    "%s", priv->client_id,
                    "reason_code",  "%d", reason_code,
                    NULL
                );
                JSON_DECREF(properties)
                return MOSQ_ERR_PROTOCOL;
            }
        }
    }
    if(gbuffer_leftbytes(gbuf)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt pubackcomp: packet too long",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        JSON_DECREF(properties)
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(priv->iamServer) {
        if(gobj_trace_level(gobj) & SHOW_DECODE) {
            trace_msg0("  👈 server Received %s from client '%s' (Mid: %d, RC:%d)",
                type,
                SAFE_PRINT(priv->client_id),
                mid,
                reason_code
            );
        }
    } else {
        if(gobj_trace_level(gobj) & SHOW_DECODE) {
            trace_msg0("  👈 client %s Received %s from server (Mid: %d, RC:%d)",
                SAFE_PRINT(priv->client_id),
                type,
                mid,
                reason_code
            );
        }
    }

    if(priv->iamServer) {
        /*
         *  Immediately free,
         *  we don't do anything with Reason String or User Property at the moment
         */
        JSON_DECREF(properties)

        rc = db__message_delete_outgoing(gobj, mid, mosq_ms_wait_for_pubcomp, qos);
        if(rc == MOSQ_ERR_NOT_FOUND) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt: Received for an unknown packet identifier",
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

        rc = message__delete(gobj, mid, mosq_md_out, qos);

        if(rc == MOSQ_ERR_SUCCESS) {
            /*
             *  Callback to user
             *  Only inform the client the message has been sent once, qos 1/2
             */
            json_t *kw_publish = json_pack("{s:i, s:i}",
                "mid", (int)mid,
                "qos", qos
            );
            json_t *kw_iev = iev_create(
                gobj,
                EV_MQTT_PUBLISH,
                kw_publish // owned
            );

            gobj_publish_event(gobj, EV_ON_IEV_MESSAGE, kw_iev);

            // TODO código viejo
            // gbuffer_t *gbuf_message = gbuffer_create(stored->payloadlen, stored->payloadlen);
            // if(gbuf_message) {
            //     if(stored->payloadlen > 0) {
            //         // Can become without payload
            //         gbuffer_append(gbuf_message, stored->payload, stored->payloadlen);
            //     }
            //     json_t *kw = json_pack("{s:s, s:s, s:I}",
            //         "mqtt_action", "publishing",
            //         "topic", topic_name,
            //         "gbuffer", (json_int_t)(uintptr_t)gbuf_message
            //     );
            //     gobj_publish_event(gobj, EV_ON_MESSAGE, kw);
            // }

        } else if(rc != MOSQ_ERR_NOT_FOUND) {
            JSON_DECREF(properties)
            return rc;
        }

        message__release_to_inflight(gobj, mosq_md_out);

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
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt pubrec: malformed packet",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    rc = mqtt_read_uint16(gobj, gbuf, &mid);
    if(rc<0) {
        // Error already logged
        return rc;
    }
    if(mid == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt pubrec: mid == 0",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        return MOSQ_ERR_PROTOCOL;
    }

    if(priv->protocol_version == mosq_p_mqtt5 && gbuffer_leftbytes(gbuf) > 2) {
        rc = mqtt_read_byte(gobj, gbuf, &reason_code);
        if(rc<0) {
            // Error already logged
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
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt pubrec: wrong reason code",
                "client_id",    "%s", priv->client_id,
                "reason_code",  "%d", (int)reason_code,
                NULL
            );
            return MOSQ_ERR_PROTOCOL;
        }

        if(gbuffer_leftbytes(gbuf) > 3) {
            properties = property_read_all(gobj, gbuf, CMD_PUBREC, &rc);
            if(rc<0) {
                // Error already logged
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
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt pubrec: too much data",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        return MOSQ_ERR_MALFORMED_PACKET;
    }


    // printf("================> PUBREC OUT Client %s\n", priv->client_id);
    // print_queue("dl_msgs_out", &priv->dl_msgs_out);

    if(priv->iamServer) {
        if(gobj_trace_level(gobj) & SHOW_DECODE) {
            trace_msg0("  👈 server Received PUBREC from client '%s' (Mid: %d, reason code: %02X)",
                SAFE_PRINT(priv->client_id),
                mid,
                reason_code
            );
        }

        if(reason_code < 0x80) {
            rc = db__message_update_outgoing(gobj, mid, mosq_ms_wait_for_pubcomp, 2);
        } else {
            return db__message_delete_outgoing(gobj, mid, mosq_ms_wait_for_pubrec, 2);
        }

    } else {
        if(gobj_trace_level(gobj) & SHOW_DECODE) {
            trace_msg0("  👈 client %s Received PUBREC from broker (Mid: %d, reason code: %02X)",
                SAFE_PRINT(priv->client_id),
                mid,
                reason_code
            );
        }

        if(reason_code < 0x80 || priv->protocol_version != mosq_p_mqtt5) {
            rc = message__out_update(gobj, mid, mosq_ms_wait_for_pubcomp, 2);
        } else {
            if(!message__delete(gobj, mid, mosq_md_out, 2)) {
                /*
                 *  TODO If not exist it's because must be dup? What is this case?
                 */
                /*
                 *  Callback to user
                 *  Only inform the client the message has been sent once, qos 2
                 */
                json_t *kw_publish = json_pack("{s:i, s:i}",
                    "mid", (int)mid,
                    "qos", 2
                );
                json_t *kw_iev = iev_create(
                    gobj,
                    EV_MQTT_PUBLISH,
                    kw_publish // owned
                );

                gobj_publish_event(gobj, EV_ON_IEV_MESSAGE, kw_iev);
            }
            //util__increment_send_quota(mosq); // TODO
            message__release_to_inflight(gobj, mosq_md_out);
            return MOSQ_ERR_SUCCESS;
        }
    }

    if(rc == MOSQ_ERR_NOT_FOUND) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: Received PUBREC for an unknown packet ID",
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
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt pubrel: malformed packet",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        return MOSQ_ERR_MALFORMED_PACKET;
    }
    rc = mqtt_read_uint16(gobj, gbuf, &mid);
    if(rc) {
        // Error already logged
        return rc;
    }
    if(mid == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt pubrel: mid == 0",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        return MOSQ_ERR_PROTOCOL;
    }

    if(priv->protocol_version == mosq_p_mqtt5 && gbuffer_leftbytes(gbuf) > 2) {
        rc = mqtt_read_byte(gobj, gbuf, &reason_code);
        if(rc) {
            // Error already logged
            return rc;
        }

        if(reason_code != MQTT_RC_SUCCESS && reason_code != MQTT_RC_PACKET_ID_NOT_FOUND) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt pubrel: bad reason",
                "client_id",    "%s", priv->client_id,
                "reason_code",  "%d", (int)reason_code,
                NULL
            );
            return MOSQ_ERR_PROTOCOL;
        }

        if(gbuffer_leftbytes(gbuf) > 3) {
            properties = property_read_all(gobj, gbuf, CMD_PUBREL, &rc);
            if(rc<0) {
                // Error already logged
                return rc;
            }
        }
    }

    if(gbuffer_leftbytes(gbuf)>0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt pubrel: too much data",
            "client_id",    "%s", priv->client_id,
            NULL
        );
        JSON_DECREF(properties)
        return MOSQ_ERR_MALFORMED_PACKET;
    }

    if(priv->iamServer) {
        if(gobj_trace_level(gobj) & SHOW_DECODE) {
            trace_msg0("  👈 server Received PUBREL from client '%s' (Mid: %d)",
                SAFE_PRINT(priv->client_id),
                mid
            );
        }
        /* Immediately free, we don't do anything with Reason String or User Property at the moment */
        JSON_DECREF(properties)

        rc = db__message_release_incoming(gobj, mid);
        if(rc == MOSQ_ERR_NOT_FOUND) {
            /* Message not found. Still send a PUBCOMP anyway because this could be
            * due to a repeated PUBREL after a client has reconnected. */
        } else if(rc != MOSQ_ERR_SUCCESS) {
            // Error already logged
            JSON_DECREF(properties)
            return rc;
        }

        rc = send__pubcomp(gobj, mid, NULL);
        if(rc<0) {
            // Error already logged
            JSON_DECREF(properties)
            return rc;
        }
    } else {
        if(gobj_trace_level(gobj) & SHOW_DECODE) {
            trace_msg0("  👈 client %s Received PUBREL from broker (Mid: %d)",
                SAFE_PRINT(priv->client_id),
                mid
            );
        }
        struct mosquitto_message_all *message = NULL;
        rc = send__pubcomp(gobj, mid, NULL);
        if(rc) {
            message__remove(gobj, mid, mosq_md_in, &message, 2);
            JSON_DECREF(properties)
            return rc;
        }

        rc = message__remove(gobj, mid, mosq_md_in, &message, 2);
        if(rc == MOSQ_ERR_SUCCESS) {
             /* Only pass the message on if we have removed it from the queue - this
              * prevents multiple callbacks for the same message.
              */
            if(1) {
                 json_t *kw_message = json_pack("{s:s, s:i, s:b, s:i, s:i}",
                    "topic", message->msg.topic,
                    "mid", (int)message->msg.mid,
                    "dup", (int)message->dup,
                    "qos", (int)message->msg.qos,
                    "retain", (int)message->msg.retain
                );
                if(properties) {
                    json_object_set(kw_message, "properties", properties);
                }
                if(message->msg.payload) {
                    gbuffer_incref(message->msg.payload);
                    json_object_set_new(
                        kw_message,
                        "gbuffer",
                        json_integer((json_int_t)(uintptr_t)message->msg.payload)
                    );
                }

                json_t *kw_iev = iev_create(
                    gobj,
                    EV_MQTT_MESSAGE,
                    kw_message // owned
                );

                gobj_publish_event(gobj, EV_ON_IEV_MESSAGE, kw_iev);
            }

            JSON_DECREF(properties)
            message__cleanup(message);
        } else if(rc == MOSQ_ERR_NOT_FOUND) {
            JSON_DECREF(properties)
            return MOSQ_ERR_SUCCESS;
        } else {
            JSON_DECREF(properties)
            return rc;
        }
    }

    JSON_DECREF(properties)
    return MOSQ_ERR_SUCCESS;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE uint16_t mosquitto__mid_generate(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // TODO new
    // json_t *client = gobj_get_resource(priv->gobj_mqtt_clients, client_id, 0, 0);
    // uint16_t last_mid = (uint16_t)kw_get_int(gobj, client, "last_mid", 0, KW_REQUIRED);
    static uint16_t last_mid = 0;
    // TODO it seems that does nothing saving in resource
    last_mid++;
    if(last_mid == 0) {
        last_mid++;
    }
    // TODO new
    // gobj_save_resource(priv->gobj_mqtt_clients, client_id, client, 0);

    return last_mid;
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

    // Change firstly for avoid new messages from client
    gobj_change_state(gobj, ST_DISCONNECTED);

    if(priv->in_session) {
        if(priv->send_disconnect) {
            // Fallan los test con el send__disconnect
            //send__disconnect(gobj, code, NULL);
        }
    }

    do_disconnect(gobj, reason);

    if(priv->iamServer) {
        hgobj tcp0 = gobj_bottom_gobj(gobj);
        if(gobj_is_running(tcp0)) {
            gobj_send_event(tcp0, EV_DROP, 0, gobj);
        }
    }
    set_timeout(priv->timer, priv->timeout_close);
}

/***************************************************************************
 *  Start to wait handshake
 ***************************************************************************/
PRIVATE void start_wait_handshake(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_is_running(gobj)) {
        return;
    }
    gobj_change_state(gobj, ST_WAIT_HANDSHAKE);

    istream_reset_wr(priv->istream_frame);  // Reset buffer for next frame
    memset(&priv->frame_head, 0, sizeof(priv->frame_head));
    set_timeout(priv->timer, priv->timeout_handshake);
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
    gobj_change_state(gobj, ST_WAIT_FRAME_HEADER);

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

    // if(!priv->in_session) { // TODO remove when not needed
    //     if(priv->iamServer) {
    //         if(frame->command != CMD_CONNECT) {
    //             gobj_log_error(gobj, 0,
    //                 "function",     "%s", __FUNCTION__,
    //                 "msgset",       "%s", MSGSET_MQTT_ERROR,
    //                 "msg",          "%s", "mqtt server: first command MUST be CONNECT",
    //                 "command",      "%s", get_command_name(frame->command),
    //                 NULL
    //             );
    //             return -1;
    //         }
    //     } else {
    //         if(frame->command != CMD_CONNACK) {
    //             gobj_log_error(gobj, 0,
    //                 "function",     "%s", __FUNCTION__,
    //                 "msgset",       "%s", MSGSET_MQTT_ERROR,
    //                 "msg",          "%s", "mqtt client: first command MUST be CMD_CONNACK",
    //                 "command",      "%s", get_command_name(frame->command),
    //                 NULL
    //             );
    //             return -1;
    //         }
    //     }
    // }

    /*
     *  decod byte2
     */
    frame->frame_length = byte2 & 0x7F;
    if(byte2 & 0x80) {
        frame->must_read_remaining_length_2 = 1;
    }

    return 0;
}

/***************************************************************************
 *  Consume input data to get and analyze the frame header.
 *  Return the consumed size.
 ***************************************************************************/
PRIVATE int framehead_consume(
    hgobj gobj,
    FRAME_HEAD *frame,
    istream_h istream,
    char *bf,
    size_t len
) {
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
        consumed = (int)istream_consume(istream, bf, len);
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
        consumed = (int)istream_consume(istream, bf, len);
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
        consumed = (int)istream_consume(istream, bf, len);
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
        consumed = (int)istream_consume(istream, bf, len);
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
            return -1;
        }
    }

    frame->header_complete = TRUE;

    // if(priv->iamServer) {
    //     switch(frame->command) {
    //         case CMD_CONNECT:
    //             if(frame->frame_length > 100000) {
    //                 gobj_log_error(gobj, 0,
    //                     "function",     "%s", __FUNCTION__,
    //                     "msgset",       "%s", MSGSET_MQTT_ERROR,
    //                     "msg",          "%s", "CONNECT command too large",
    //                     "frame_length", "%d", (int)frame->frame_length,
    //                     NULL
    //                 );
    //                 return 0;
    //             }
    //             break;
    //         case CMD_DISCONNECT:
    //             break;
    //
    //         case CMD_CONNACK:
    //         case CMD_PUBLISH:
    //         case CMD_PUBACK:
    //         case CMD_PUBREC:
    //         case CMD_PUBREL:
    //         case CMD_PUBCOMP:
    //         case CMD_SUBSCRIBE:
    //         case CMD_SUBACK:
    //         case CMD_UNSUBSCRIBE:
    //         case CMD_UNSUBACK:
    //         case CMD_AUTH:
    //             break;
    //
    //         case CMD_PINGREQ:
    //         case CMD_PINGRESP:
    //             if(frame->frame_length != 0) {
    //                 gobj_log_error(gobj, 0,
    //                     "function",     "%s", __FUNCTION__,
    //                     "msgset",       "%s", MSGSET_MQTT_ERROR,
    //                     "msg",          "%s", "PING command must be 0 large",
    //                     "frame_length", "%d", (int)frame->frame_length,
    //                     NULL
    //                 );
    //                 return 0;
    //             }
    //             break;
    //
    //         default:
    //             gobj_log_error(gobj, 0,
    //                 "function",     "%s", __FUNCTION__,
    //                 "msgset",       "%s", MSGSET_MQTT_ERROR,
    //                 "msg",          "%s", "Mqtt command unknown",
    //                 "command",      "%d", (int)frame->command,
    //                 NULL
    //             );
    //             if(priv->in_session) {
    //                 send__disconnect(gobj, MQTT_RC_PROTOCOL_ERROR, NULL);
    //             }
    //             return 0;
    //     }
    // }

    return total_consumed;
}

/***************************************************************************
 *  Process the completed frame
 ***************************************************************************/
PRIVATE int frame_completed(hgobj gobj, hgobj src)
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
        case CMD_PINGREQ:       // common to server/client
            ret = handle__pingreq(gobj);
            break;
        case CMD_PINGRESP:      // common to server/client
            ret = handle__pingresp(gobj);
            break;

        /*
         *  Only Server
         */
        case CMD_CONNECT:       // Only Server
            if(!priv->iamServer) {
                ret = MOSQ_ERR_PROTOCOL;
                break;
            }
            ret = handle__connect(gobj, gbuf, src);
            break;

        case CMD_CONNACK:       // NOT common to server(bridge)/client
            if(priv->iamServer) {
                // Bridge not implemented
                ret = MOSQ_ERR_PROTOCOL;
                break;
            } else {
                json_t *jn_data = json_object();
                ret = handle__connack(gobj, gbuf, jn_data);
                if(ret == 0) {
                    priv->inform_on_close = TRUE;
                    gobj_publish_event(gobj, EV_ON_OPEN, jn_data);
                } else {
                    // Error already logged
                    json_decref(jn_data);
                }
            }
            break;

        case CMD_DISCONNECT:    // NOT common to server/client
            if(priv->iamServer) {
                ret = handle__disconnect_s(gobj, gbuf);
            } else {
                ret = handle__disconnect_c(gobj, gbuf);
            }
            break;

        // case CMD_AUTH:          // NOT common to server/client TODO
        //     if(priv->iamServer) {
        //         ret = handle__auth_s(gobj, gbuf);
        //     } else {
        //         ret = handle__auth_c(gobj, gbuf);
        //     }
        //     break;

        case CMD_PUBLISH:       // NOT common to server/client
            if(priv->iamServer) {
                ret = handle__publish_s(gobj, gbuf);
            } else {
                ret = handle__publish_c(gobj, gbuf);
            }
            break;

        case CMD_PUBACK:        // common to server/client
            ret = handle__pubackcomp(gobj, gbuf, "PUBACK");
            break;
        case CMD_PUBCOMP:       // common to server/client
            ret = handle__pubackcomp(gobj, gbuf, "PUBCOMP");
            break;
        case CMD_PUBREC:        // common to server/client
            ret = handle__pubrec(gobj, gbuf);
            break;
        case CMD_PUBREL:        // common to server/client
            ret = handle__pubrel(gobj, gbuf);
            break;

        case CMD_SUBSCRIBE:     // Only Server
            if(!priv->iamServer) {
                ret = MOSQ_ERR_PROTOCOL;
                break;
            }
            ret = handle__subscribe(gobj, gbuf);
            break;
        case CMD_SUBACK:        // common to server(bridge)/client
            if(priv->iamServer) {
                // Bridge not implemented
                ret = MOSQ_ERR_PROTOCOL;
                break;
            }
            ret = handle__suback(gobj, gbuf);
            break;

        case CMD_UNSUBSCRIBE:   // Only Server
            if(!priv->iamServer) {
                ret = MOSQ_ERR_PROTOCOL;
                break;
            }
            ret  = handle__unsubscribe(gobj, gbuf);
            break;

        case CMD_UNSUBACK:      // common to server(bridge)/client
            if(priv->iamServer) {
                // Bridge not implemented
                ret = MOSQ_ERR_PROTOCOL;
                break;
            }
            ret = handle__unsuback(gobj, gbuf);
            break;

        case CMD_RESERVED:
        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Command unknown or not implemented",
                "command",      "%s", get_command_name(frame->command),
                NULL
            );
            ret = MOSQ_ERR_PROTOCOL;
            break;
    }

    GBUFFER_DECREF(gbuf);

    if(frame->command != CMD_CONNECT && priv->protocol_version == mosq_p_mqtt5) {
        if(ret == MOSQ_ERR_PROTOCOL || ret == MOSQ_ERR_DUPLICATE_PROPERTY) {
            send__disconnect(gobj, MQTT_RC_PROTOCOL_ERROR, NULL);
        } else if(ret == MOSQ_ERR_MALFORMED_PACKET) {
            send__disconnect(gobj, MQTT_RC_MALFORMED_PACKET, NULL);
        } else if(ret == MOSQ_ERR_QOS_NOT_SUPPORTED) {
            send__disconnect(gobj, MQTT_RC_QOS_NOT_SUPPORTED, NULL);
        } else if(ret == MOSQ_ERR_RETAIN_NOT_SUPPORTED) {
            send__disconnect(gobj, MQTT_RC_RETAIN_NOT_SUPPORTED, NULL);
        } else if(ret == MOSQ_ERR_TOPIC_ALIAS_INVALID) {
            send__disconnect(gobj, MQTT_RC_TOPIC_ALIAS_INVALID, NULL);
        } else if(ret == MOSQ_ERR_UNKNOWN || ret == MOSQ_ERR_NOMEM) {
            send__disconnect(gobj, MQTT_RC_UNSPECIFIED, NULL);
        } else if(ret<0) {
            send__disconnect(gobj, MQTT_RC_PROTOCOL_ERROR, NULL);
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

    priv->send_disconnect = FALSE;
    GBUFFER_DECREF(priv->gbuf_will_payload);
    priv->jn_alias_list = json_object();

    start_wait_handshake(gobj); // include the start of the timeout of handshake

    if(priv->iamServer) {
        /*
         * wait the request
         */
    } else {
        /*
         * We are client
         * send the request
         */
        send__connect(
            gobj,
            atoi(gobj_read_str_attr(gobj, "mqtt_keepalive")),
            (atoi(gobj_read_str_attr(gobj, "mqtt_clean_session")))?1:0,
            json_object()   // outgoing_properties TODO get from client
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(priv->jn_alias_list);

    if(priv->istream_payload) {
        istream_destroy(priv->istream_payload);
        priv->istream_payload = 0;
    }

    if(priv->inform_on_close) {
        priv->inform_on_close = FALSE;

        const char *peername;
        if(gobj_has_bottom_attr(src, "peername")) {
            peername = gobj_read_str_attr(src, "peername");
        } else {
            peername = kw_get_str(gobj, kw, "peername", "", 0);
        }

        json_t *kw2 = json_pack("{s:s, s:s, s:s, s:s}",
            "username", gobj_read_str_attr(gobj, "__username__"),
            "client_id", gobj_read_str_attr(gobj, "client_id"),
            "session_id", gobj_read_str_attr(gobj, "__session_id__"),
            "peername", peername
        );
        gobj_publish_event(gobj, EV_ON_CLOSE, kw2);
    }

    gobj_reset_volatil_attrs(gobj);
    restore_client_attributes(gobj);

    GBUFFER_DECREF(priv->gbuf_will_payload);

    if(gobj_is_volatil(src)) {
        gobj_set_bottom_gobj(gobj, 0);
    }

    clear_timeout(priv->timer);

    JSON_DECREF(priv->jn_alias_list)

    close_queues(gobj);

    // TODO new dl_flush(&priv->dl_msgs_in, db_free_client_msg);
    // TODO new dl_flush(&priv->dl_msgs_out, db_free_client_msg);

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
 *  Process the handshake.
 ***************************************************************************/
PRIVATE int ac_process_handshake(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);
    FRAME_HEAD *frame = &priv->frame_head;
    istream_h istream = priv->istream_frame;

    if(gobj_trace_level(gobj) & TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "HANDSHAKE %s <== %s",
            gobj_short_name(gobj),
            gobj_short_name(src)
        );
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
                trace_msg0("👈👈rx HANDSHAKE COMMAND=%s (%d), FRAME_LEN=%d",
                    get_command_name(frame->command),
                    (int)frame->command,
                    (int)frame->frame_length
                );
            }

            if(priv->iamServer) {
                if(frame->command != CMD_CONNECT) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MQTT_ERROR,
                        "msg",          "%s", "mqtt server: first command MUST be CONNECT",
                        "command",      "%s", get_command_name(frame->command),
                        NULL
                    );
                    gobj_trace_dump_full_gbuf(gobj, gbuf, "HANDSHAKE %s <== %s",
                        gobj_short_name(gobj),
                        gobj_short_name(src)
                    );
                    ws_close(gobj, MQTT_RC_PROTOCOL_ERROR);
                    break;
                }
            } else {
                if(frame->command != CMD_CONNACK) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MQTT_ERROR,
                        "msg",          "%s", "mqtt client: first command MUST be CMD_CONNACK",
                        "command",      "%s", get_command_name(frame->command),
                        NULL
                    );
                    gobj_trace_dump_full_gbuf(gobj, gbuf, "HANDSHAKE %s <== %s",
                        gobj_short_name(gobj),
                        gobj_short_name(src)
                    );
                    ws_close(gobj, MQTT_RC_PROTOCOL_ERROR);
                    break;
                }
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

                if(frame->frame_length > 1000) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MQTT_ERROR,
                        "msg",          "%s", "CONNECT command too large",
                        "frame_length", "%d", (int)frame->frame_length,
                        NULL
                    );
                    ws_close(gobj, MQTT_RC_PROTOCOL_ERROR);
                    break;
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
                    frame_length,
                    frame_length
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

                gobj_change_state(gobj, ST_WAIT_PAYLOAD);
                set_timeout(priv->timer, priv->timeout_payload);

                return gobj_send_event(gobj, EV_RX_DATA, kw, gobj);

            } else {
                if(frame_completed(gobj, src)<0) {
                    //priv->send__disconnect = TRUE;
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
 *  Too much time waiting handshake
 ***************************************************************************/
PRIVATE int ac_timeout_wait_handshake(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_MQTT_ERROR,
        "msg",          "%s", "Timeout waiting mqtt HANDSHAKE data",
        NULL
    );
    ws_close(gobj, MQTT_RC_PROTOCOL_ERROR);

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

    clear_timeout(priv->timer);

    if(gobj_trace_level(gobj) & TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "HEADER %s <== %s",
            gobj_short_name(gobj),
            gobj_short_name(src)
        );
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
                trace_msg0("👈👈rx SESSION COMMAND=%s (%d), FRAME_LEN=%d",
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
                    frame_length,
                    frame_length
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

                gobj_change_state(gobj, ST_WAIT_PAYLOAD);
                set_timeout(priv->timer, priv->timeout_payload);

                return gobj_send_event(gobj, EV_RX_DATA, kw, gobj);

            } else {
                if(frame_completed(gobj, src)<0) {
                    //priv->send__disconnect = TRUE;
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
 *
 ***************************************************************************/
PRIVATE int ac_timeout_waiting_frame_header(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // TODO implement ping
    // if(priv->timer_handshake) {
    //     if(test_sectimer(priv->timer_handshake)) {
    //         ws_close(gobj, MQTT_RC_PROTOCOL_ERROR);
    //     }
    // }

    // mosquitto__check_keepalive()
    // if(priv->timer_ping) {
    //     if(test_sectimer(priv->timer_ping)) {
    //         // TODO send send__pingreq(mosq); or close connection if not receive response
    //         if(priv->keepalive > 0) {
    //             priv->timer_ping = start_sectimer(priv->keepalive);
    //         }
    //     }
    // }

    // TODO implement a cleaner of messages inflight with pending acks and timeout reached
    // TODO or remove all messages with same mid when receiving and ack

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

    clear_timeout(priv->timer);

    if(gobj_trace_level(gobj) & TRAFFIC_PAYLOAD) {
        gobj_trace_dump_gbuf(gobj, gbuf, "PAYLOAD %s <== %s (accumulated %lu)",
            gobj_short_name(gobj),
            gobj_short_name(src),
            (unsigned long)istream_length(priv->istream_payload)
        );
    }

    size_t bf_len = gbuffer_leftbytes(gbuf);
    char *bf = gbuffer_cur_rd_pointer(gbuf);

    size_t consumed = istream_consume(priv->istream_payload, bf, bf_len);
    if(consumed > 0) {
        gbuffer_get(gbuf, consumed);  // take out the bytes consumed
    }
    if(istream_is_completed(priv->istream_payload)) {
        int ret;
        if((ret=frame_completed(gobj, src))<0) {
            if(gobj_trace_level(gobj) & SHOW_DECODE) {
                trace_msg0("❌❌ Mqtt error: disconnecting: %d", ret);
                gobj_trace_dump_full_gbuf(gobj, gbuf, "Mqtt error: disconnecting");
            }
            ws_close(gobj, MQTT_RC_PROTOCOL_ERROR);
            KW_DECREF(kw)
            return -1;
        }
    } else {
        set_timeout(priv->timer, priv->timeout_payload);
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
    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_MQTT_ERROR,
        "msg",          "%s", "Timeout waiting mqtt PAYLOAD data",
        "username",     "%s", gobj_read_str_attr(gobj, "__username__"),
        "client_id",    "%s", gobj_read_str_attr(gobj, "client_id"),
        "session_id",   "%s", gobj_read_str_attr(gobj, "__session_id__"),
        NULL
    );
    ws_close(gobj, MQTT_RC_PROTOCOL_ERROR);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Entry of mqtt publishing for clients (mosquitto_publish_v5)
 ***************************************************************************/
PRIVATE int ac_mqtt_client_send_publish(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // TODO check: topic can be empty if using prot v5 and property MQTT_PROP_TOPIC_ALIAS
    const char *topic = kw_get_str(gobj, kw, "topic", "", 0);
    int qos = (int)kw_get_int(gobj, kw, "qos", 0, 0);
    int expiry_interval = (int)kw_get_int(gobj, kw, "expiry_interval", 0, 0);
    BOOL retain = kw_get_bool(gobj, kw, "retain", 0, 0);
    json_t *properties = kw_get_dict(gobj, kw, "properties", 0, 0);
    gbuffer_t *gbuf_payload = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

    if(priv->iamServer) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt publish: not client",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    if(qos < 0 || qos > 2) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt publish: qos invalid",
            "qos",          "%d", qos,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    if(priv->protocol_version != mosq_p_mqtt5 && json_size(properties)>0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt publish: properties not supported",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    if(qos > priv->max_qos) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt publish: qos not supported",
            "max_qos",      "%d", priv->max_qos,
            "qos",          "%d", qos,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    if(!priv->retain_available && retain) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt publish: retain not available",
            NULL
        );
        retain = FALSE;
    }

    if(json_size(properties)>0) {
        if(mqtt_property_check_all(gobj, CMD_PUBLISH, properties)<0) {
            // Error already logged
            KW_DECREF(kw)
            return -1;
        }
    }

    if(empty_string(topic)) {
        if(priv->protocol_version == mosq_p_mqtt5) {
            BOOL have_topic_alias = FALSE;
            // TODO check property
            // if(p->identifier == MQTT_PROP_TOPIC_ALIAS) {
            //     have_topic_alias = true;
            //     break;
            // }
            if(have_topic_alias == FALSE) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Mqtt publish: topic name empty and no alias",
                    NULL
                );
                KW_DECREF(kw)
                return -1;
            }
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt publish: topic name empty",
                NULL
            );
            KW_DECREF(kw)
            return -1;
        }
    } else {
        size_t tlen = strlen(topic);
        if(mosquitto_validate_utf8(topic, (int)tlen)<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt publish: topic malformed utf8",
                NULL
            );
            KW_DECREF(kw)
            return -1;
        }
        if(mosquitto_pub_topic_check(topic) != 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt publish: topic invalid",
                "topic",        "%s", topic,
                NULL
            );
            KW_DECREF(kw)
            return -1;
        }
    }

    size_t payloadlen = gbuffer_leftbytes(gbuf_payload);
    if(payloadlen < 0 || payloadlen > (int)MQTT_MAX_PAYLOAD) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt publish: payload > MQTT_MAX_PAYLOAD",
            "topic",        "%s", topic,
            "payloadlen",   "%ld", (long)payloadlen,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    if(priv->maximum_packet_size > 0 && payloadlen > priv->maximum_packet_size) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt publish: message too large",
            "topic",        "%s", topic,
            "payloadlen",   "%d", (int)payloadlen,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    int mid = mosquitto__mid_generate(gobj);

    if(qos == 0) {
        if(send__publish(
            gobj,
            mid,
            topic,
            gbuf_payload,
            (uint8_t)qos,
            retain,
            FALSE,          // dup
            properties,     // cmsg_props
            NULL,           // store_props
            expiry_interval // expiry_interval
        )==0) {
            /*
             *  Callback to user
             *  Only inform the client the message has been sent once, qos 0
             */
            json_t *kw_publish = json_pack("{s:i, s:i}",
                "mid", (int)mid,
                "qos", 0
            );
            json_t *kw_iev = iev_create(
                gobj,
                EV_MQTT_PUBLISH,
                kw_publish // owned
            );

            gobj_publish_event(gobj, EV_ON_IEV_MESSAGE, kw_iev);
        }
    } else {

        time_t t = mosquitto_time();
        user_flag_t user_flag;
        json_t *kw_mqtt_msg = new_json_message(
            gobj,
            mid,
            topic,
            gbuf_payload, // not owned
            qos,
            retain,
            FALSE,      // dup,
            properties, // not owned
            0, // TODO expiry_interval,
            0, // origin
            mosq_md_out,
            &user_flag,
            t
        );

        message__queue(
            gobj,
            kw_mqtt_msg,
            mosq_md_out,
            user_flag,
            t
        );

        // TODO this base64 to tr_queue.c
        // if(payloadlen) {
        //     gbuffer_t *gbuf_base64 = gbuffer_string_to_base64(payload, payloadlen);
        //     if(!gbuf_base64) {
        //         gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
        //             "function",     "%s", __FUNCTION__,
        //             "msgset",       "%s", MSGSET_INTERNAL_ERROR,
        //             "msg",          "%s", "Mqtt publish: No memory for gbuf_base64",
        //             "topic",        "%s", topic,
        //             "payloadlen",   "%d", (int)payloadlen,
        //             NULL
        //         );
        //         KW_DECREF(kw)
        //         return -1;
        //     }
        //     char *data = gbuffer_cur_rd_pointer(gbuf_base64);
        //     json_t *jn_bf = json_string(data);
        //     if(!jn_bf) {
        //         gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
        //             "function",     "%s", __FUNCTION__,
        //             "msgset",       "%s", MSGSET_INTERNAL_ERROR,
        //             "msg",          "%s", "Mqtt publish: Cannot jsonfy base64 data",
        //             "topic",        "%s", topic,
        //             "payloadlen",   "%d", (int)payloadlen,
        //             NULL
        //         );
        //         gbuffer_decref(gbuf_base64);
        //         KW_DECREF(kw)
        //         return -1;
        //     }
        //     json_object_set_new(kw_mqtt_msg, "data", jn_bf);
        //     gbuffer_decref(gbuf_base64);
        // } else {
        //     json_object_set_new(kw_mqtt_msg, "data", json_string(""));
        // }

        // mosquitto_message_all_t *message = GBMEM_MALLOC(sizeof(mosquitto_message_all_t));
        // message->timestamp = mosquitto_time();
        // message->msg.mid = mid;
        // message->expiry_interval = expiry_interval;
        //
        // if(topic) {
        //     message->msg.topic = gbmem_strdup(topic);
        //     if(!message->msg.topic) {
        //         message__cleanup(message);
        //         KW_DECREF(kw)
        //         return -1;
        //     }
        // }
        // if(payloadlen) {
        //     message->msg.payload = gbuffer_incref(gbuf_payload);
        //     if(!message->msg.payload) {
        //         message__cleanup(message);
        //         KW_DECREF(kw)
        //         return -1;
        //     }
        // } else {
        //     message->msg.payload = NULL;
        // }
        // message->msg.qos = (uint8_t)qos;
        // message->msg.retain = retain;
        // message->dup = FALSE;
        // message->properties = json_incref(properties);
        //
        // message->state = mosq_ms_invalid;
        // message__queue(gobj, message, mosq_md_out);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Entry of mqtt subscribing for clients
 ***************************************************************************/
PRIVATE int ac_mqtt_client_send_subscribe(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *subs = kw_get_list(gobj, kw, "subs", 0, 0);
    int mid = (int)kw_get_int(gobj, kw, "mid", 0, 0);
    int qos = (int)kw_get_int(gobj, kw, "qos", 0, 0);
    int options = (int)kw_get_int(gobj, kw, "options", 0, 0);
    json_t *properties = kw_get_dict(gobj, kw, "properties", 0, 0);

    if(priv->iamServer) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt publish: not client",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    if(!json_is_array(subs) || !json_size(subs)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt subscribe: subs must be a list of sub patterns",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    if(priv->protocol_version != mosq_p_mqtt5 && json_size(properties)>0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt subscribe: properties not supported",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }
    if(qos < 0 || qos > 2) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt subscribe: qos invalid",
            "qos",          "%d", qos,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }
    if((options & 0x30) == 0x30 || (options & 0xC0) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt subscribe: options invalid",
            "options",      "%d", options,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    if(json_size(properties)>0) {
        if(mqtt_property_check_all(gobj, CMD_SUBSCRIBE, properties)<0) {
            // Error already logged
            KW_DECREF(kw)
            return -1;
        }
    }

    uint32_t remaining_length = 0;
    int idx; json_t *jn_sub;
    json_array_foreach(subs, idx, jn_sub) {
        const char *sub = json_string_value(jn_sub);
        if(empty_string(sub)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt subscribe: subscription empty",
                "subs",         "%j", subs,
                NULL
            );
            KW_DECREF(kw)
            return -1;
        }
        int slen = (int)strlen(sub);
        if(mosquitto_validate_utf8(sub, slen)<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt subscribe: bad utf8",
                "subs",         "%j", subs,
                NULL
            );
            KW_DECREF(kw)
            return -1;
        }

        remaining_length += 2+(uint32_t)slen + 1;
    }

    if(priv->maximum_packet_size > 0) {
        remaining_length += 2 + property__get_length_all(properties);
        // TODO
        // if(packet__check_oversize(mosq, remaining_length)) {
        //     gobj_log_error(gobj, 0,
        //         "function",     "%s", __FUNCTION__,
        //         "msgset",       "%s", MSGSET_MQTT_ERROR,
        //         "msg",          "%s", "Mqtt subscribe: message too big",
        //         NULL
        //     );
        //     KW_DECREF(kw)
        //     return -1;
        // }
    }
    if(priv->protocol_version != mosq_p_mqtt5) {
        options = 0;
    }

    if(!mid) {
        mid = mosquitto__mid_generate(gobj);
        json_object_set_new(kw, "mid", json_integer(mid));
    }

    send__subscribe(gobj, mid, subs, qos|options, properties);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Entry of mqtt unsubscribing for clients
 ***************************************************************************/
PRIVATE int ac_mqtt_client_send_unsubscribe(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *subs = kw_get_list(gobj, kw, "subs", 0, 0);
    int mid = (int)kw_get_int(gobj, kw, "mid", 0, 0);
    json_t *properties = kw_get_dict(gobj, kw, "properties", 0, 0);

    if(priv->iamServer) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt publish: not client",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    if(!json_is_array(subs) || !json_size(subs)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt unsubscribe: subs must be a list of sub patterns",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    if(priv->protocol_version != mosq_p_mqtt5 && json_size(properties)>0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt unsubscribe: properties not supported",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    if(json_size(properties)>0) {
        if(mqtt_property_check_all(gobj, CMD_UNSUBSCRIBE, properties)<0) {
            // Error already logged
            KW_DECREF(kw)
            return -1;
        }
    }

    uint32_t remaining_length = 0;
    int idx; json_t *jn_sub;
    json_array_foreach(subs, idx, jn_sub) {
        const char *sub = json_string_value(jn_sub);
        if(empty_string(sub)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt subscribe: subscription empty",
                "subs",         "%j", subs,
                NULL
            );
            KW_DECREF(kw)
            return -1;
        }
        int slen = (int)strlen(sub);
        if(mosquitto_validate_utf8(sub, slen)<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt subscribe: bad utf8",
                "subs",         "%j", subs,
                NULL
            );
            KW_DECREF(kw)
            return -1;
        }

        remaining_length += 2U + (uint32_t)slen;
    }

    if(priv->maximum_packet_size > 0) {
        remaining_length += 2U + property__get_length_all(properties);
        // TODO
        // if(packet__check_oversize(mosq, remaining_length)) {
        //     gobj_log_error(gobj, 0,
        //         "function",     "%s", __FUNCTION__,
        //         "msgset",       "%s", MSGSET_MQTT_ERROR,
        //         "msg",          "%s", "Mqtt subscribe: message too big",
        //         NULL
        //     );
        //     KW_DECREF(kw)
        //     return -1;
        // }
    }

    if(!mid) {
        mid = mosquitto__mid_generate(gobj);
        json_object_set_new(kw, "mid", json_integer(mid));
    }

    send__unsubscribe(gobj, mid, subs, properties);

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
            "username",     "%s", gobj_read_str_attr(gobj, "__username__"),
            "client_id",    "%s", gobj_read_str_attr(gobj, "client_id"),
            "session_id",   "%s", gobj_read_str_attr(gobj, "__session_id__"),
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
            "username",     "%s", gobj_read_str_attr(gobj, "__username__"),
            "client_id",    "%s", gobj_read_str_attr(gobj, "client_id"),
            "session_id",   "%s", gobj_read_str_attr(gobj, "__session_id__"),
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
            "username",     "%s", gobj_read_str_attr(gobj, "__username__"),
            "client_id",    "%s", gobj_read_str_attr(gobj, "client_id"),
            "session_id",   "%s", gobj_read_str_attr(gobj, "__session_id__"),
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
            "username",     "%s", gobj_read_str_attr(gobj, "__username__"),
            "client_id",    "%s", gobj_read_str_attr(gobj, "client_id"),
            "session_id",   "%s", gobj_read_str_attr(gobj, "__session_id__"),
            NULL
        );
        KW_DECREF(kw)
        return MOSQ_ERR_INVAL;
    }

    if(priv->maximum_packet_size > 0) {
        remaining_length = 1 + 2+(uint32_t)tlen + (uint32_t)payloadlen +
            property__get_length_all(outgoing_properties);
        if(qos > 0) {
            remaining_length++;
        }
        if(packet_check_oversize(gobj, remaining_length)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt oversize packet",
                "username",     "%s", gobj_read_str_attr(gobj, "__username__"),
                "client_id",    "%s", gobj_read_str_attr(gobj, "client_id"),
                "session_id",   "%s", gobj_read_str_attr(gobj, "__session_id__"),
                NULL
            );
            KW_DECREF(kw)
            return MOSQ_ERR_OVERSIZE_PACKET;
        }
    }

    uint16_t mid = mosquitto__mid_generate(gobj);
    json_object_set_new(kw, "mid", json_integer(mid));

    send__publish(
        gobj,
        mid,
        topic_name,
        gbuf,
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
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_PROT_MQTT2);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_MQTT_PUBLISH);
GOBJ_DEFINE_EVENT(EV_MQTT_SUBSCRIBE);
GOBJ_DEFINE_EVENT(EV_MQTT_UNSUBSCRIBE);
GOBJ_DEFINE_EVENT(EV_MQTT_MESSAGE);

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
        {EV_CONNECTED,          ac_connected,                       ST_WAIT_HANDSHAKE},
        {EV_DISCONNECTED,       ac_disconnected,                    0},
        {EV_TIMEOUT,            ac_timeout_waiting_disconnected,    0},
        {EV_STOPPED,            ac_stopped,                         0},
        {EV_TX_READY,           0,                                  0},
        {0,0,0}
    };
    ev_action_t st_wait_handshake[] = {
        {EV_RX_DATA,            ac_process_handshake,               0},
        {EV_DISCONNECTED,       ac_disconnected,                    ST_DISCONNECTED},
        {EV_TIMEOUT,            ac_timeout_wait_handshake,          0},
        {EV_DROP,               ac_drop,                            0},
        {EV_TX_READY,           0,                                  0},
        {0,0,0}
    };
    ev_action_t st_wait_frame_header[] = {
        {EV_RX_DATA,            ac_process_frame_header,            0},
        // {EV_MQTT_MESSAGE,       ac_send_message,                    0},
        {EV_MQTT_PUBLISH,       ac_mqtt_client_send_publish,        0},
        {EV_MQTT_SUBSCRIBE,     ac_mqtt_client_send_subscribe,      0},
        {EV_MQTT_UNSUBSCRIBE,   ac_mqtt_client_send_unsubscribe,    0},

        {EV_DISCONNECTED,       ac_disconnected,                    ST_DISCONNECTED},
        {EV_TIMEOUT,            ac_timeout_waiting_frame_header,    0},
        {EV_DROP,               ac_drop,                            0},
        {EV_TX_READY,           0,                                  0},
        {0,0,0}
    };
    ev_action_t st_wait_payload[] = {
        {EV_RX_DATA,            ac_process_payload_data,            0},
        // {EV_MQTT_MESSAGE,       ac_send_message,                    0},
        {EV_MQTT_PUBLISH,       ac_mqtt_client_send_publish,        0},
        {EV_MQTT_SUBSCRIBE,     ac_mqtt_client_send_subscribe,      0},
        {EV_MQTT_UNSUBSCRIBE,   ac_mqtt_client_send_unsubscribe,    0},

        {EV_DISCONNECTED,       ac_disconnected,                    ST_DISCONNECTED},
        {EV_TIMEOUT,            ac_timeout_waiting_payload_data,    0},
        {EV_DROP,               ac_drop,                            0},
        {EV_TX_READY,           0,                                  0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_DISCONNECTED,           st_disconnected},
        {ST_WAIT_HANDSHAKE,         st_wait_handshake},
        {ST_WAIT_FRAME_HEADER,      st_wait_frame_header},
        {ST_WAIT_PAYLOAD,           st_wait_payload},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_RX_DATA,            0},
        {EV_MQTT_PUBLISH,       0},
        {EV_MQTT_SUBSCRIBE,     0},
        {EV_MQTT_UNSUBSCRIBE,   0},
        {EV_ON_IEV_MESSAGE,     EVF_OUTPUT_EVENT},
        // {EV_MQTT_MESSAGE,       EVF_OUTPUT_EVENT},
        {EV_TIMEOUT,            0},
        {EV_TX_READY,           0},
        {EV_CONNECTED,          0},
        {EV_DISCONNECTED,       0},
        {EV_STOPPED,            0},
        {EV_DROP,               0},

        {EV_ON_OPEN,            EVF_OUTPUT_EVENT},
        {EV_ON_CLOSE,           EVF_OUTPUT_EVENT},
        // {EV_ON_MESSAGE,         EVF_OUTPUT_EVENT},
        // {EV_ON_ID,              EVF_OUTPUT_EVENT},
        // {EV_ON_ID_NAK,          EVF_OUTPUT_EVENT},
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
PUBLIC int register_c_prot_mqtt2(void)
{
    return create_gclass(C_PROT_MQTT2);
}
