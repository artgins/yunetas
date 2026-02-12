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
 *          Copyright (c) 2025,2026 ArtGins.
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
#include "mqtt_util.h"
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
typedef enum mosq_err_s {
    MOSQ_ERR_SUCCESS = 0,
    MOSQ_ERR_NOMEM = -1,
    MOSQ_ERR_PROTOCOL = -2,
    MOSQ_ERR_CONN_REFUSED = -3,
    MOSQ_ERR_NOT_FOUND = -4,
    MOSQ_ERR_UNKNOWN = -5,
    MOSQ_ERR_MALFORMED_PACKET = -6,
    MOSQ_ERR_DUPLICATE_PROPERTY = -7,
    MOSQ_ERR_QOS_NOT_SUPPORTED = -8,
    MOSQ_ERR_OVERSIZE_PACKET = -9,
    MOSQ_ERR_RETAIN_NOT_SUPPORTED = -10,
    MOSQ_ERR_TOPIC_ALIAS_INVALID = -11,
    MOSQ_ERR_ADMINISTRATIVE_ACTION = -12,
} mosq_err_t;

/* MQTT specification restricts client ids to a maximum of 23 characters */
#define MOSQ_MQTT_ID_MAX_LENGTH 23

/***************************************************************************
 *              Structures
 ***************************************************************************/
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
PRIVATE int send__publish(
    hgobj gobj,
    uint16_t mid,
    const char *topic,
    gbuffer_t *payload,
    uint8_t qos,
    BOOL retain,
    BOOL dup,
    json_t *properties, // not owned
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
PRIVATE void send_drop(hgobj gobj, int reason);
PRIVATE uint16_t mqtt_mid_generate(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
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
SDATA (DTP_POINTER,     "tranger_queues",   0,          0,      "Queues TimeRanger for mqtt messages with qos > 0. If null then no persistence queues are used and max qos is limited to 0"),
SDATA (DTP_STRING,      "alert_message",    SDF_RD,     "ALERT Queuing", "Alert message"),
SDATA (DTP_INTEGER,     "max_pending_acks", SDF_RD,     "10000","Maximum messages pending of ack, mid is an uint16_t, max_pending_acks cannot be greater than 65535"), // TODO this is really like max_inflight_messages, TODO unify
SDATA (DTP_INTEGER,     "backup_queue_size",SDF_RD,     "60000","Do backup at this size, using rowid as mid therefore backup_queue_size cannot be greater than 65535"),
SDATA (DTP_INTEGER,     "alert_queue_size", SDF_RD,     "2000", "Limit alert queue size"),
SDATA (DTP_INTEGER,     "timeout_ack",      SDF_RD,     "60",   "Timeout ack in seconds"),

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

SDATA (DTP_STRING,      "mqtt_keepalive",   0,         "10",    "MQTT keepalive. The number of seconds between sending PING commands to the broker for the purposes of informing it we are still connected and functioning. Defaults to 60 seconds."), // TODO repon 60

SDATA (DTP_STRING,      "mqtt_will_topic",  0,          "",     "MQTT will topic"),
SDATA (DTP_STRING,      "mqtt_will_payload",0,          "",     "MQTT will payload"),
SDATA (DTP_STRING,      "mqtt_will_qos",    0,          "",     "MQTT will qos"),
SDATA (DTP_STRING,      "mqtt_will_retain", 0,          "",     "MQTT will retain"),

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
SDATA (DTP_INTEGER,     "timeout_periodic", SDF_RD,     "1000",  "Timeout periodic"),
SDATA (DTP_INTEGER,     "timeout_backup",   SDF_RD,     "1",     "Timeout to check backup, in seconds"),

/*
 *  Configuration
 */

SDATA (DTP_INTEGER,     "max_inflight_bytes",SDF_WR,        0,      "Outgoing QoS 1 and 2 messages will be allowed in flight until this byte limit is reached. This allows control of outgoing message rate based on message size rather than message count. If the limit is set to 100, messages of over 100 bytes are still allowed, but only a single message can be in flight at once. Defaults to 0. (No limit)."),

SDATA (DTP_INTEGER,     "max_inflight_messages",SDF_WR,     "20",   "The maximum number of outgoing QoS 1 or 2 messages that can be in the process of being transmitted simultaneously. This includes messages currently going through handshakes and messages that are being retried. Defaults to 20. Set to 0 for no maximum. If set to 1, this will guarantee in-order delivery of messages. mid is an uint16_t, max_inflight_messages (max_pending_acks) cannot be greater than 65535"),

SDATA (DTP_INTEGER,     "max_queued_bytes", SDF_WR,         0,      "The number of outgoing QoS 1 and 2 messages above those currently in-flight will be queued (per client) by the broker. Once this limit has been reached, subsequent messages will be silently dropped. This is an important option if you are sending messages at a high rate and/or have clients who are slow to respond or may be offline for extended periods of time. Defaults to 0. (No maximum).See also the max_queued_messages option. If both max_queued_messages and max_queued_bytes are specified, packets will be queued until the first limit is reached."),

SDATA (DTP_INTEGER,     "max_queued_messages",SDF_WR,       "1000",   "The maximum number of QoS 1 or 2 messages to hold in the queue (per client) above those messages that are currently in flight. Defaults to 1000. Set to 0 for no maximum (not recommended). See also the queue_qos0_messages and max_queued_bytes options."),

SDATA (DTP_INTEGER,     "message_size_limit",SDF_WR,        0,      "This option sets the maximum publish payload size that the broker will allow. Received messages that exceed this size will not be accepted by the broker. This means that the message will not be forwarded on to subscribing clients, but the QoS flow will be completed for QoS 1 or QoS 2 messages. MQTT v5 clients using QoS 1 or QoS 2 will receive a PUBACK or PUBREC with the 'implementation specific error' reason code. The default value is 0, which means that all valid MQTT messages are accepted. MQTT imposes a maximum payload size of 268435455 bytes."),

SDATA (DTP_INTEGER,     "max_packet_size",  SDF_WR,         0,      "For MQTT v5 clients, it is possible to have the server send a 'maximum packet size' value that will instruct the client it will not accept MQTT packets with size greater than value bytes. This applies to the full MQTT packet, not just the payload. Setting this option to a positive value will set the maximum packet size to that number of bytes. If a client sends a packet which is larger than this value, it will be disconnected. This applies to all clients regardless of the protocol version they are using, but v3.1.1 and earlier clients will of course not have received the maximum packet size information. Defaults to no limit. This option applies to all clients, not just those using MQTT v5, but it is not possible to notify clients using MQTT v3.1.1 or MQTT v3.1 of the limit. Setting below 20 bytes is forbidden because it is likely to interfere with normal client operation even with small payloads."),

SDATA (DTP_BOOLEAN,     "retain_available", SDF_WR,         "1",   "If set to FALSE, then retained messages are not supported. Clients that send a message with the retain bit will be disconnected if this option is set to FALSE. Defaults to TRUE."),

SDATA (DTP_INTEGER,     "max_qos",          SDF_WR,         "2",      "Limit the QoS value allowed for clients connecting to this listener. Defaults to 2, which means any QoS can be used. Set to 0 or 1 to limit to those QoS values. This makes use of an MQTT v5 feature to notify clients of the limitation. MQTT v3.1.1 clients will not be aware of the limitation. Clients publishing to this listener with a too-high QoS will be disconnected."),

SDATA (DTP_BOOLEAN,     "allow_zero_length_clientid",SDF_WR, "1",   "MQTT 3.1.1 and MQTT 5 allow clients to connect with a zero length client id and have the broker generate a client id for them. Use this option to allow/disallow this behaviour. Defaults to TRUE."),

SDATA (DTP_INTEGER,     "max_topic_alias",  SDF_WR,         "10",     "This option sets the maximum number topic aliases that an MQTT v5 client is allowed to create. This option applies per listener. Defaults to 10. Set to 0 to disallow topic aliases. The maximum value possible is 65535."),

/*
 *  Dynamic Data
 */
SDATA (DTP_BOOLEAN,     "in_session",       SDF_VOLATIL|SDF_STATS,0,    "CONNECT mqtt done"),
SDATA (DTP_BOOLEAN,     "send_disconnect",  SDF_VOLATIL,        0,      "send DISCONNECT"),
SDATA (DTP_INTEGER,     "last_mid",         SDF_VOLATIL,        0,      "Last mid"),

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
SDATA (DTP_STRING,      "will_topic",       SDF_VOLATIL,        0,      "Will topic"),
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
    hgobj gobj_timer;
    hgobj gobj_timer_periodic;
    BOOL iamServer;             // What side? server or client
    time_t timer_ping;          // Timer to send ping (client) or check keepalive timeout (server)
    time_t timer_check_ping;    // Timer to check ping response (client only)

    json_int_t timeout_handshake;
    json_int_t timeout_payload;
    json_int_t timeout_close;
    json_int_t timeout_periodic;
    json_int_t timeout_backup;
    time_t t_backup;

    FRAME_HEAD frame_head;
    istream_h istream_frame;
    istream_h istream_payload;

    FRAME_HEAD message_head;

    BOOL inform_on_close;
    json_t *jn_alias_list;

    json_t *tranger_queues;
    tr2_queue_t *trq_in_msgs;
    tr2_queue_t *trq_out_msgs;

    /*
     *  Config
     */
    int max_inflight_bytes;
    int max_inflight_messages;
    int max_queued_bytes;
    int max_queued_messages;
    int max_packet_size;
    int message_size_limit;
    BOOL retain_available;
    BOOL allow_zero_length_clientid;
    int max_topic_alias;

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
    uint16_t last_mid;
    uint32_t max_qos;

    BOOL will;
    const char *will_topic;
    BOOL will_retain;
    uint32_t will_qos;
    uint32_t will_delay_interval;
    uint32_t will_expiry_interval;
    gbuffer_t *gbuf_will_payload;
    int out_packet_count;

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
    priv->gobj_timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    char timer_name[NAME_MAX];
    snprintf(timer_name, sizeof(timer_name), "%s-PERIODIC", gobj_name(gobj));
    priv->gobj_timer_periodic = gobj_create_pure_child(timer_name, C_TIMER, 0, gobj);

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
    SET_PRIV(last_mid,                  gobj_read_integer_attr)
    SET_PRIV(timeout_handshake,         gobj_read_integer_attr)
    SET_PRIV(timeout_payload,           gobj_read_integer_attr)
    SET_PRIV(timeout_close,             gobj_read_integer_attr)
    SET_PRIV(timeout_periodic,          gobj_read_integer_attr)
    SET_PRIV(timeout_backup,            gobj_read_integer_attr)
    SET_PRIV(in_session,                gobj_read_bool_attr)
    SET_PRIV(send_disconnect,           gobj_read_bool_attr)

    SET_PRIV(max_inflight_bytes,        gobj_read_integer_attr)
    SET_PRIV(max_inflight_messages,     gobj_read_integer_attr)
    SET_PRIV(max_queued_bytes,          gobj_read_integer_attr)
    SET_PRIV(max_queued_messages,       gobj_read_integer_attr)
    SET_PRIV(max_packet_size,           gobj_read_integer_attr)
    SET_PRIV(message_size_limit,        gobj_read_integer_attr)
    SET_PRIV(retain_available,          gobj_read_bool_attr)
    SET_PRIV(max_qos,                   gobj_read_integer_attr)
    SET_PRIV(allow_zero_length_clientid,gobj_read_bool_attr)
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

    IF_EQ_SET_PRIV(last_mid,                    gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_handshake,         gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_payload,           gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_close,             gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_periodic,          gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_backup,            gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(in_session,                gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(send_disconnect,           gobj_read_bool_attr)

    ELIF_EQ_SET_PRIV(max_inflight_bytes,        gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(max_inflight_messages,     gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(max_queued_bytes,          gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(max_queued_messages,       gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(max_packet_size,           gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(message_size_limit,        gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(retain_available,          gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(max_qos,                   gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(allow_zero_length_clientid,gobj_read_bool_attr)
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

    clear_timeout(priv->gobj_timer);
    clear_timeout(priv->gobj_timer_periodic);

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
PRIVATE void restore_client_attributes(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->iamServer) {
        return;
    }
    gobj_write_str_attr(gobj, "client_id", gobj_read_str_attr(gobj, "mqtt_client_id"));
}

/***************************************************************************
 *  HACK input queues will use only inflight messages
 *       output queues will use both inflight and queued messages
 ***************************************************************************/
PRIVATE int open_queues(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char queue_name[NAME_MAX];

    /*
     *  Input messages
     */
    build_queue_name(
        queue_name,
        sizeof(queue_name),
        priv->client_id,
        mosq_md_in
    );

    printf("CLIENT_ID QUEUE OPEN %s %s %s\n", priv->client_id, queue_name, gobj_full_name(gobj)); // TODO TEST

    priv->trq_in_msgs = tr2q_open(
        priv->tranger_queues,
        queue_name,
        "tm",
        0,  // system_flag
        priv->max_inflight_messages,
        gobj_read_integer_attr(gobj, "backup_queue_size")
    );

    tr2q_load(priv->trq_in_msgs);

    /*
     *  Output messages
     */
    build_queue_name(
        queue_name,
        sizeof(queue_name),
        priv->client_id,
        mosq_md_out
    );

    priv->trq_out_msgs = tr2q_open(
        priv->tranger_queues,
        queue_name,
        "tm",
        0,  // system_flag
        priv->max_inflight_messages,
        gobj_read_integer_attr(gobj, "backup_queue_size")
    );

    tr2q_load(priv->trq_out_msgs);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void close_queues(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char queue_name[NAME_MAX];

    if(priv->trq_in_msgs) {
        printf("CLIENT_ID QUEUE CLOSE %s %s %s\n", priv->client_id, queue_name, gobj_full_name(gobj)); // TODO TEST

        tr2q_close(priv->trq_in_msgs);

        if(priv->clean_start) {
            /*
             *  Delete the queues if it's not a persistent session
             */

            /*
             *  Input messages
             */
            build_queue_name(
                queue_name,
                sizeof(queue_name),
                priv->client_id,
                mosq_md_in
            );

            tranger2_delete_topic(
                priv->tranger_queues,
                queue_name
            );
        }

        priv->trq_in_msgs = NULL;
    }

    if(priv->trq_out_msgs) {
        tr2q_close(priv->trq_out_msgs);

        if(priv->clean_start) {
            /*
             *  Delete the queues if it's not a persistent session
             */

            /*
             *  Output messages
             */
            build_queue_name(
                queue_name,
                sizeof(queue_name),
                priv->client_id,
                mosq_md_out
            );
            tranger2_delete_topic(
                priv->tranger_queues,
                queue_name
            );
        }

        priv->trq_out_msgs = NULL;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int message__out_update(
    hgobj gobj,
    uint16_t mid,
    enum mqtt_msg_state state,
    int qos
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    tr2_queue_t *trq = priv->trq_out_msgs;

    q2_msg_t *qmsg = tr2q_get_by_mid(trq, mid);
    if(qmsg) {
        int msg_qos = msg_flag_get_qos_level(qmsg);
        if(msg_qos != qos) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "QoS mismatch",
                "mid",          "%d", (int)mid,
                "msg_qos",      "%d", msg_qos,
                "expected_qos", "%d", qos,
                NULL
            );
            return MOSQ_ERR_PROTOCOL;
        }
        msg_flag_set_state(qmsg, state);
        tr2q_save_hard_mark(qmsg, qmsg->md_record.user_flag);
        return MOSQ_ERR_SUCCESS;
    } else {
        // Trace by now, see use cases
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Message not found",
            "mid",          "%d", (int)mid,
            "qos",          "%d", qos,
            NULL
        );
        return MOSQ_ERR_NOT_FOUND;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int message__release_to_inflight(hgobj gobj, enum mqtt_msg_direction dir, BOOL redeliver)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(dir == mosq_md_out) {
        tr2_queue_t *trq = priv->trq_out_msgs;

        q2_msg_t *qmsg, *next;
        Q2MSG_FOREACH_FORWARD_INFLIGHT_SAFE(trq, qmsg, next) {
            int qos = msg_flag_get_qos_level(qmsg);
            mqtt_msg_state_t state = msg_flag_get_state(qmsg);

            if(qos > 0 && state == mosq_ms_invalid) {
                /*
                 *  Assign mid and update state
                 */
                uint16_t mid = mqtt_mid_generate(gobj);
                qmsg->mid = mid;

                /*
                 *  Get message content and send PUBLISH
                 */
                json_t *kw_msg = tr2q_msg_json(qmsg);
                if(!kw_msg) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                        "msg",          "%s", "No message content in queue entry",
                        "mid",          "%d", (int)mid,
                        NULL
                    );
                    continue;
                }
                const char *topic = kw_get_str(gobj, kw_msg, "topic", "", 0);
                gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(
                    gobj, kw_msg, "gbuffer", 0, 0
                );
                BOOL retain = kw_get_bool(gobj, kw_msg, "retain", 0, 0);
                BOOL dup = msg_flag_get_dup(qmsg);
                json_t *properties = kw_get_dict(gobj, kw_msg, "properties", 0, 0);
                uint32_t expiry_interval = (uint32_t)kw_get_int(
                    gobj, kw_msg, "expiry_interval", 0, 0
                );

                /*
                 *  What is first? send the message or save his state in disk
                 */
                if(send__publish(
                    gobj,
                    mid,
                    topic,
                    gbuf,       // notowned
                    (uint8_t)qos,
                    retain,
                    dup,
                    properties, // not owned
                    expiry_interval
                ) == 0) {
                    // Message sent, save state
                    mqtt_msg_state_t new_state;
                    if(qos == 1) {
                        new_state = mosq_ms_wait_for_puback;
                    } else {
                        new_state = mosq_ms_wait_for_pubrec;
                    }
                    msg_flag_set_state(qmsg, new_state);
                    tr2q_save_hard_mark(qmsg, qmsg->md_record.user_flag);
                }

            } else if(redeliver && (state == mosq_ms_wait_for_puback || state == mosq_ms_wait_for_pubrec)) {
                /*
                 *  [MQTT-4.4.0-1] Redeliver unacknowledged PUBLISH on reconnect
                 *  Assign new mid (original was not persisted) and resend with DUP=1
                 */
                uint16_t mid = mqtt_mid_generate(gobj);
                qmsg->mid = mid;

                json_t *kw_msg = tr2q_msg_json(qmsg);
                if(!kw_msg) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                        "msg",          "%s", "No message content for redelivery",
                        "mid",          "%d", (int)mid,
                        NULL
                    );
                    continue;
                }
                const char *topic = kw_get_str(gobj, kw_msg, "topic", "", 0);
                gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(
                    gobj, kw_msg, "gbuffer", 0, 0
                );
                BOOL retain = kw_get_bool(gobj, kw_msg, "retain", 0, 0);
                json_t *properties = kw_get_dict(gobj, kw_msg, "properties", 0, 0);
                uint32_t expiry_interval = (uint32_t)kw_get_int(
                    gobj, kw_msg, "expiry_interval", 0, 0
                );

                send__publish(
                    gobj,
                    mid,
                    topic,
                    gbuf,       // notowned
                    (uint8_t)qos,
                    retain,
                    TRUE,       // DUP=1 for redelivery
                    properties, // not owned
                    expiry_interval
                );
                tr2q_save_hard_mark(qmsg, qmsg->md_record.user_flag);

            } else if(redeliver && state == mosq_ms_wait_for_pubcomp) {
                /*
                 *  [MQTT-4.4.0-1] Redeliver PUBREL on reconnect
                 */
                if(qmsg->mid == 0) {
                    qmsg->mid = mqtt_mid_generate(gobj);
                }
                send__pubrel(gobj, qmsg->mid, NULL);
                tr2q_save_hard_mark(qmsg, qmsg->md_record.user_flag);
            }
        }
    }

    return 0;
}

/***************************************************************************
 *  Enqueue a message in the input or output queue
 ***************************************************************************/
PRIVATE int message__queue(
    hgobj gobj,
    json_t *kw_mqtt_msg, // owned
    mqtt_msg_direction_t dir,
    uint16_t user_flag
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(dir == mosq_md_out) {
        tr2q_append(
            priv->trq_out_msgs,
            0,              // __t__ if 0 then the time will be set by TimeRanger with now time
            kw_mqtt_msg,    // owned
            user_flag       // extra flags in addition to TRQ_MSG_PENDING
        );

    } else {
        tr2q_append(
            priv->trq_in_msgs,
            0,              // __t__ if 0 then the time will be set by TimeRanger with now time
            kw_mqtt_msg,    // owned
            user_flag       // extra flags in addition to TRQ_MSG_PENDING
        );
    }
int todo_xxx;
// TODO esto está en db__message_insert(), similar a message__queue?
// if(dir == mosq_md_out && update){
//     rc = db__message_write_inflight_out_latest(context);
//     if(rc) return rc;
//     rc = db__message_write_queued_out(context);
//     if(rc) return rc;
// }


    return message__release_to_inflight(gobj, dir, FALSE);
}

/***************************************************************************
 *  Find the message with 'mid',
 *      dequeue
 *      return the message
 ***************************************************************************/
PRIVATE int message__remove(
    hgobj gobj,
    uint16_t mid,
    mqtt_msg_direction_t dir,
    int qos, // TODO what checks mosquitto does with qos? review
    json_t **pmsg
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(pmsg) {
        *pmsg = NULL;
    }

    q2_msg_t *msg;
    if(dir == mosq_md_out) {
        msg = tr2q_get_by_mid(priv->trq_out_msgs, mid);

    } else {
        msg = tr2q_get_by_mid(priv->trq_in_msgs, mid);
    }

    if(msg) {
        json_t *kw = tr2q_msg_json(msg);
        if(pmsg) {
            *pmsg = kw_incref(kw);
        }
        tr2q_unload_msg(msg, 0);
        return MOSQ_ERR_SUCCESS;
    } else {
        return MOSQ_ERR_NOT_FOUND;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int message__delete(
    hgobj gobj,
    uint16_t mid,
    enum mqtt_msg_direction dir,
    int qos
) {
    json_t *kw_mqtt_msg;

    int rc = message__remove(gobj, mid, dir, qos, &kw_mqtt_msg);
    if(rc == MOSQ_ERR_SUCCESS) {
        KW_DECREF(kw_mqtt_msg)
    }
    return rc;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void db__message_remove_from_inflight(
    hgobj gobj,
    tr2_queue_t *trq,
    q2_msg_t *qmsg
) {
    tr2q_unload_msg(qmsg, 0); // TODO must check that message is in inflight list, not in queued list
}

/***************************************************************************
 * Is this context ready to take more in flight messages right now?
 *  'qos' qos for the packet of interest
 *  Return true if more in flight are allowed.
 ***************************************************************************/
PRIVATE BOOL db__ready_for_flight(hgobj gobj, enum mqtt_msg_direction dir, int qos)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    tr2_queue_t *trq;
    if(dir == mosq_md_out) {
        trq = priv->trq_out_msgs;
    } else {
        trq = priv->trq_in_msgs;
    }

    if(priv->max_inflight_messages == 0 && priv->max_inflight_bytes == 0) {
        return TRUE;
    }

    if(qos == 0) {
        // TODO check bytes or msgs?
        return TRUE;
    } else {
        if(trq->max_inflight_messages == 0) {
            return TRUE;
        }
        if(trq->max_inflight_messages > 0 &&
            tr2q_inflight_size(trq) < trq->max_inflight_messages
        ) {
            return TRUE;
        }

        if(priv->max_inflight_bytes > 0) {
            // TODO check bytes
        }
    }

    return FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int db__message_dequeue_first(
    hgobj gobj,
    tr2_queue_t *trq
) {
    q2_msg_t *msg = tr2q_first_queued_msg(trq);
    if(!msg) {
        // Silence please
        return -1;
    }
    return tr2q_move_from_queued_to_inflight(msg);
}

/***************************************************************************
 *
 ***************************************************************************/
// PRIVATE int db__message_write_inflight_out_single(
//     hgobj gobj,
//     struct mosquitto_client_msg *msg
// ) {
//     PRIVATE_DATA *priv = gobj_priv_data(gobj);
//     json_t *cmsg_props = NULL, *store_props = NULL;
//     int rc;
//     uint16_t mid;
//     int retries;
//     int retain;
//     const char *topic;
//     uint8_t qos;
//     uint32_t payloadlen;
//     const void *payload;
//     uint32_t expiry_interval;
//
//     expiry_interval = 0;
//     if(msg->store->message_expiry_time) {
//         if(priv->db_now_real_s > msg->store->message_expiry_time) {
//             /* Message is expired, must not send. */
//             if(msg->direction == mosq_md_out && msg->qos > 0) {
//                 // TODO util__increment_send_quota(context);
//             }
//             db__message_remove_from_inflight(gobj, &priv->msgs_out, msg);
//             return MOSQ_ERR_SUCCESS;
//         } else {
//             expiry_interval = (uint32_t)(msg->store->message_expiry_time - priv->db_now_real_s);
//         }
//     }
//     mid = msg->mid;
//     retries = msg->dup;
//     retain = msg->retain;
//     topic = msg->store->topic;
//     qos = (uint8_t)msg->qos;
//     cmsg_props = msg->properties;
//     store_props = msg->store->properties;
//
//     switch(msg->state) {
//         case mosq_ms_publish_qos0:
//             rc = send__publish(gobj, mid, topic, msg->store->payload, qos, retain, retries, cmsg_props, store_props, expiry_interval);
//             if(rc == MOSQ_ERR_SUCCESS || rc == MOSQ_ERR_OVERSIZE_PACKET) {
//                 db__message_remove_from_inflight(gobj, &priv->msgs_out, msg);
//             } else {
//                 return rc;
//             }
//             break;
//
//         case mosq_ms_publish_qos1:
//             rc = send__publish(gobj, mid, topic, msg->store->payload, qos, retain, retries, cmsg_props, store_props, expiry_interval);
//             if(rc == MOSQ_ERR_SUCCESS) {
//                 msg->timestamp = priv->db_now_s;
//                 msg->dup = 1; /* Any retry attempts are a duplicate. */
//                 msg->state = mosq_ms_wait_for_puback;
//             } else if(rc == MOSQ_ERR_OVERSIZE_PACKET) {
//                 db__message_remove_from_inflight(gobj, &priv->msgs_out, msg);
//             } else {
//                 return rc;
//             }
//             break;
//
//         case mosq_ms_publish_qos2:
//             rc = send__publish(gobj, mid, topic, msg->store->payload, qos, retain, retries, cmsg_props, store_props, expiry_interval);
//             if(rc == MOSQ_ERR_SUCCESS) {
//                 msg->timestamp = priv->db_now_s;
//                 msg->dup = 1; /* Any retry attempts are a duplicate. */
//                 msg->state = mosq_ms_wait_for_pubrec;
//             } else if(rc == MOSQ_ERR_OVERSIZE_PACKET) {
//                 db__message_remove_from_inflight(gobj, &priv->msgs_out, msg);
//             } else {
//                 return rc;
//             }
//             break;
//
//         case mosq_ms_resend_pubrel:
//             rc = send__pubrel(gobj, mid, NULL);
//             if(!rc) {
//                 msg->state = mosq_ms_wait_for_pubcomp;
//             } else {
//                 return rc;
//             }
//             break;
//
//         case mosq_ms_invalid:
//         case mosq_ms_send_pubrec:
//         case mosq_ms_resend_pubcomp:
//         case mosq_ms_wait_for_puback:
//         case mosq_ms_wait_for_pubrec:
//         case mosq_ms_wait_for_pubrel:
//         case mosq_ms_wait_for_pubcomp:
//         case mosq_ms_queued:
//             break;
//     }
//     return MOSQ_ERR_SUCCESS;
// }

/***************************************************************************
 *  Using in handle__subscribe() and websockets
 ***************************************************************************/
// PRIVATE int db__message_write_inflight_out_latest(hgobj gobj)
// {
    // struct mosquitto_client_msg *tail, *next;
    // int rc;
    //
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
//     return MOSQ_ERR_SUCCESS;
// }

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
    tr2_queue_t *trq = priv->trq_out_msgs;
    if(!trq) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "No output queue",
            NULL
        );
        return MOSQ_ERR_NOT_FOUND;
    }

    q2_msg_t *qmsg = tr2q_get_by_mid(trq, mid);
    if(qmsg) {
        int msg_qos = msg_flag_get_qos_level(qmsg);
        if(msg_qos != qos) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "QoS mismatch",
                "mid",          "%d", (int)mid,
                "msg_qos",      "%d", msg_qos,
                "expected_qos", "%d", qos,
                NULL
            );
            return MOSQ_ERR_PROTOCOL;
        }
        // tail->timestamp = db.now_s; TODO
        msg_flag_set_state(qmsg, state);
        tr2q_save_hard_mark(qmsg, qmsg->md_record.user_flag);
        return MOSQ_ERR_SUCCESS;
    }

    // Silence please
    return MOSQ_ERR_NOT_FOUND;
}

/***************************************************************************
 *  Using in handle__pubrec() and handle__pubackcomp()
 ***************************************************************************/
PRIVATE int db__message_delete_outgoing(
    hgobj gobj,
    uint16_t mid,
    enum mqtt_msg_state expect_state,
    int qos
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    tr2_queue_t *trq = priv->trq_out_msgs;

    q2_msg_t *qmsg = tr2q_get_by_mid(trq, mid);
    if(qmsg) {
        int msg_qos = msg_flag_get_qos_level(qmsg);
        if(msg_qos != qos) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "QoS mismatch",
                "mid",          "%d", (int)mid,
                "msg_qos",      "%d", msg_qos,
                "expected_qos", "%d", qos,
                NULL
            );
            //return MOSQ_ERR_PROTOCOL;
        }
        if(qos == 2) {
            mqtt_msg_state_t msg_state = msg_flag_get_state(qmsg);
            if(msg_state != expect_state) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Unexpected message state for QoS 2",
                    "mid",          "%d", (int)mid,
                    "state",        "%s", msg_flag_state_to_str(msg_state),
                    "expected",     "%s", msg_flag_state_to_str(expect_state),
                    NULL
                );
                //return MOSQ_ERR_PROTOCOL;
            }
        }

        db__message_remove_from_inflight(gobj, trq, qmsg);

    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Message not found in trq_out_msgs",
            "client_id",    "%s", priv->client_id,
            "mid",          "%d", mid,
            NULL
        );
    }

    /*
     *  Move queued messages to inflight while ready for flight
     */
    q2_msg_t *tail, *tmp;
    Q2MSG_FOREACH_FORWARD_QUEUED_SAFE(trq, tail, tmp) {
        int tail_qos = msg_flag_get_qos_level(tail);
        if(!db__ready_for_flight(gobj, mosq_md_out, tail_qos)) {
            break;
        }
        db__message_dequeue_first(gobj, trq);
    }

    /*
     *  Send inflight messages
     */
    message__release_to_inflight(gobj, mosq_md_out, FALSE);

    return MOSQ_ERR_SUCCESS;
}

/***************************************************************************
 *  Used by handle__pubrel()
 ***************************************************************************/
PRIVATE int db__message_release_incoming(hgobj gobj, uint16_t mid)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL deleted = FALSE;

    q2_msg_t *qmsg = tr2q_get_by_mid(priv->trq_in_msgs, mid);
    if(qmsg) {
        int msg_qos = msg_flag_get_qos_level(qmsg);
        if(msg_qos != 2) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Expected QoS 2 message",
                "mid",          "%d", (int)mid,
                "msg_qos",      "%d", msg_qos,
                NULL
            );
            return MOSQ_ERR_PROTOCOL;
        }

        json_t *kw_mqtt_msg = tr2q_msg_json(qmsg);
        const char *topic = kw_get_str(gobj, kw_mqtt_msg, "topic", "", 0);

        if(empty_string(topic)) {
            /*
             *  TODO review, I don't know if this a real use case
             *  topic==NULL/empty: QoS 2 message that was denied/dropped,
             *  being processed so the client doesn't keep resending it.
             *  Don't send it to other clients.
             */
        } else {
            /*
             *  Dispatch the message to the broker (subscribers)
             *  Here a qos 2 message is released to their subscribers
             */
            json_t *kw_iev = iev_create(
                gobj,
                EV_MQTT_MESSAGE,
                kw_incref(kw_mqtt_msg) // owned
            );
            gobj_publish_event( // To broker, sub__messages_queue, return # subscribers
                gobj,
                EV_ON_IEV_MESSAGE,
                kw_iev
            );
        }

        db__message_remove_from_inflight(
            gobj,
            priv->trq_in_msgs,
            qmsg
        );
        deleted = TRUE;
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Message not found",
            "mid",          "%d", (int)mid,
            NULL
        );
    }

    int todo_pop_queued;
    // DL_FOREACH_SAFE(context->msgs_in.queued, tail, tmp){
    //     if(db__ready_for_flight(context, mosq_md_in, tail->qos)){
    //         break;
    //     }
    //
    //     tail->timestamp = db.now_s;
    //
    //     if(tail->qos == 2){
    //         send__pubrec(context, tail->mid, 0, NULL);
    //         tail->state = mosq_ms_wait_for_pubrel;
    //         db__message_dequeue_first(context, &context->msgs_in);
    //     }
    // }

    if(deleted) {
        return MOSQ_ERR_SUCCESS;
    } else {
        // Silence please
        return MOSQ_ERR_NOT_FOUND;
    }
}

/***************************************************************************
 *  Using in handle__publish_s()
 *  Check if a mid already exists in inflight queue and remove it
 ***************************************************************************/
PRIVATE int db__message_remove_incoming_dup(hgobj gobj, uint16_t mid)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    q2_msg_t *qmsg, *tmp;

    DL_FOREACH_SAFE(&priv->trq_in_msgs->dl_inflight, qmsg, tmp) {
        if(qmsg->rowid == (uint64_t)mid) {
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
                priv->trq_in_msgs,
                qmsg
            );
            return MOSQ_ERR_SUCCESS;
        }
    }

    // Silence please
    return MOSQ_ERR_NOT_FOUND;
}

/***************************************************************************
 *  Move input qos-2 messages from queued list to inflight queue
 *      and send its pubrec
 ***************************************************************************/
PRIVATE int db__message_write_queued_in(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->trq_in_msgs) {
        register q2_msg_t *qmsg;
        Q2MSG_FOREACH_FORWARD_QUEUED(priv->trq_in_msgs, qmsg) {
            // if(context->msgs_in.inflight_maximum != 0 && context->msgs_in.inflight_quota == 0){
            if(priv->trq_in_msgs->max_inflight_messages > 0 && tr2q_inflight_size(priv->trq_in_msgs)==0) {
                break;
            }

            if(tr2q_move_from_queued_to_inflight(qmsg)<0) {
                break;
            }
            msg_flag_set_state(qmsg, mosq_ms_wait_for_pubrel);
            uint16_t mid = mqtt_mid_generate(gobj);
            qmsg->mid = mid;
            send__pubrec(gobj, mid, 0, NULL);
            tr2q_save_hard_mark(qmsg, qmsg->md_record.user_flag);
        }
    }
    return MOSQ_ERR_SUCCESS;
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
            gbuf_correlation_data = gbuffer_binary_to_base64(b64, strlen(b64));
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
// PRIVATE int mosquitto_property_add_varint(hgobj gobj, json_t *proplist, int identifier, uint32_t value)
// {
//     if(!proplist || value > 268435455) {
//         gobj_log_error(gobj, 0,
//             "function",     "%s", __FUNCTION__,
//             "msgset",       "%s", MSGSET_MQTT_ERROR,
//             "msg",          "%s", "Mqtt proplist NULL or value too big",
//             "identifier",   "%d", identifier,
//             NULL
//         );
//         return -1;
//     }
//     if(identifier != MQTT_PROP_SUBSCRIPTION_IDENTIFIER) {
//         gobj_log_error(gobj, 0,
//             "function",     "%s", __FUNCTION__,
//             "msgset",       "%s", MSGSET_MQTT_ERROR,
//             "msg",          "%s", "No MQTT_PROP_SUBSCRIPTION_IDENTIFIER",
//             "identifier",   "%d", identifier,
//             NULL
//         );
//         return -1;
//     }
//
//     const char *property_name = mqtt_property_identifier_to_string(identifier);
//     json_object_set_new(proplist, property_name, json_integer(value));
//
//     return MOSQ_ERR_SUCCESS;
// }

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
        if(mqtt_validate_utf8(value, (int)slen)<0) {
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
                gbuffer_t *gbuf_binary_data = gbuffer_base64_to_binary(b64, strlen(b64));
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

    if(mqtt_validate_utf8(*str, (int)(*length))<0) {
        *str = NULL;
        *length = 0;
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "malformed utf8",
            NULL
        );
        return -1;
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
                    "command",      "%d", mqtt_command_string(command),
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
                    "command",      "%d", mqtt_command_string(command),
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
                    "command",      "%d", mqtt_command_string(command),
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
                    "command",      "%d", mqtt_command_string(command),
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
                    "command",      "%d", mqtt_command_string(command),
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
                    "command",      "%d", mqtt_command_string(command),
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
                    "command",      "%d", mqtt_command_string(command),
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
                    "command",      "%d", mqtt_command_string(command),
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
                    "command",      "%d", mqtt_command_string(command),
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
                    "command",      "%d", mqtt_command_string(command),
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
                    "command",      "%d", mqtt_command_string(command),
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
                "command",      "%d", mqtt_command_string(command),
                "identifier",   "%d", identifier,
                NULL
            );
            return MOSQ_ERR_MALFORMED_PACKET;
    }
    return 0;
}

/***************************************************************************
 *  Structure of a property:

    "property-name": {
        "identifier": 17,
        "name": "property-name",
        "type": 3,
        "value": 4294967295     // can be integer or string or base64-string
    },

    "user-property" is a special case, it's saved with the user-property-name

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

    /* Check for duplicates */
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
            json_object_set_new(property, "value", json_stringn(str1, slen1));
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
            gbuffer_t *gbuf_b64 = gbuffer_binary_to_base64(str1, slen1);
            json_object_set_new(property, "value", json_string(gbuffer_cur_rd_pointer(gbuf_b64)));
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

            json_object_set_new(property, "name", json_stringn(str1, slen1));
            json_object_set_new(property, "value", json_stringn(str2, slen2));
            // using original MQTT_PROP_USER_PROPERTY implies save only one user property
            property_name = kw_get_str(gobj, property, "name", NULL, KW_REQUIRED);
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
        int identifier = (int)kw_get_int(gobj, property, "identifier", 0, KW_REQUIRED);
        if(identifier == MQTT_PROP_REQUEST_PROBLEM_INFORMATION
                || identifier == MQTT_PROP_PAYLOAD_FORMAT_INDICATOR
                || identifier == MQTT_PROP_REQUEST_RESPONSE_INFORMATION
                || identifier == MQTT_PROP_MAXIMUM_QOS
                || identifier == MQTT_PROP_RETAIN_AVAILABLE
                || identifier == MQTT_PROP_WILDCARD_SUB_AVAILABLE
                || identifier == MQTT_PROP_SUBSCRIPTION_ID_AVAILABLE
                || identifier == MQTT_PROP_SHARED_SUB_AVAILABLE) {

            int value = (int)kw_get_int(gobj, property, "value", 0, KW_REQUIRED);
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
            int value = (int)kw_get_int(gobj, property, "value", 0, KW_REQUIRED);
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

            int value = (int)kw_get_int(gobj, property, "value", 0, KW_REQUIRED);
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
            "all_properties, command %s", mqtt_command_string(command)
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
// PRIVATE json_int_t property_read_byte(json_t *properties, int identifier)
// {
//     hgobj gobj = 0;
//
//     if(identifier != MQTT_PROP_PAYLOAD_FORMAT_INDICATOR
//             && identifier != MQTT_PROP_REQUEST_PROBLEM_INFORMATION
//             && identifier != MQTT_PROP_REQUEST_RESPONSE_INFORMATION
//             && identifier != MQTT_PROP_MAXIMUM_QOS
//             && identifier != MQTT_PROP_RETAIN_AVAILABLE
//             && identifier != MQTT_PROP_WILDCARD_SUB_AVAILABLE
//             && identifier != MQTT_PROP_SUBSCRIPTION_ID_AVAILABLE
//             && identifier != MQTT_PROP_SHARED_SUB_AVAILABLE
//     ) {
//         gobj_log_error(gobj, 0,
//             "function",     "%s", __FUNCTION__,
//             "msgset",       "%s", MSGSET_MQTT_ERROR,
//             "msg",          "%s", "Bad byte property identifier",
//             "identifier",   "%d", identifier,
//             NULL
//         );
//     }
//
//     json_t *property = property_get_property(properties, identifier);
//     if(!property) {
//         return -1;
//     }
//     return kw_get_int(gobj, property, "value", -1, KW_REQUIRED);
// }

/***************************************************************************
 *  Return json_int_t, -1 is property not available
 *  return value is an uint16_t
 ***************************************************************************/
// PRIVATE json_int_t property_read_int16(json_t *properties, int identifier)
// {
//     hgobj gobj = 0;
//
//     if(identifier != MQTT_PROP_SERVER_KEEP_ALIVE
//             && identifier != MQTT_PROP_RECEIVE_MAXIMUM
//             && identifier != MQTT_PROP_TOPIC_ALIAS_MAXIMUM
//             && identifier != MQTT_PROP_TOPIC_ALIAS
//     ) {
//         gobj_log_error(gobj, 0,
//             "function",     "%s", __FUNCTION__,
//             "msgset",       "%s", MSGSET_MQTT_ERROR,
//             "msg",          "%s", "Bad int16 property identifier",
//             "identifier",   "%d", identifier,
//             NULL
//         );
//     }
//
//     json_t *property = property_get_property(properties, identifier);
//     if(!property) {
//         return -1;
//     }
//     return kw_get_int(gobj, property, "value", -1, KW_REQUIRED);
// }

/***************************************************************************
 *  Return json_int_t, -1 is property not available
 *  return value is an uint32_t
 ***************************************************************************/
// PRIVATE json_int_t property_read_int32(json_t *properties, int identifier)
// {
//     hgobj gobj = 0;
//
//     if(identifier != MQTT_PROP_MESSAGE_EXPIRY_INTERVAL
//             && identifier != MQTT_PROP_SESSION_EXPIRY_INTERVAL
//             && identifier != MQTT_PROP_WILL_DELAY_INTERVAL
//             && identifier != MQTT_PROP_MAXIMUM_PACKET_SIZE
//     ) {
//         gobj_log_error(gobj, 0,
//             "function",     "%s", __FUNCTION__,
//             "msgset",       "%s", MSGSET_MQTT_ERROR,
//             "msg",          "%s", "Bad int32 property identifier",
//             "identifier",   "%d", identifier,
//             NULL
//         );
//     }
//
//     json_t *property = property_get_property(properties, identifier);
//     if(!property) {
//         return -1;
//     }
//     return kw_get_int(gobj, property, "value", -1, KW_REQUIRED);
// }

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
// PRIVATE json_int_t property_read_binary(json_t *properties, int identifier)
// {
//     hgobj gobj = 0;
//
//     if(identifier != MQTT_PROP_CORRELATION_DATA
//             && identifier != MQTT_PROP_AUTHENTICATION_DATA
//     ) {
//         gobj_log_error(gobj, 0,
//             "function",     "%s", __FUNCTION__,
//             "msgset",       "%s", MSGSET_MQTT_ERROR,
//             "msg",          "%s", "Bad binary property identifier",
//             "identifier",   "%d", identifier,
//             NULL
//         );
//     }
//
//     json_t *property = property_get_property(properties, identifier);
//     if(!property) {
//         return -1;
//     }
//     return kw_get_int(gobj, property, "value", -1, KW_REQUIRED);
// }

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
// PRIVATE const char *property_read_string_pair(json_t *properties, int identifier)
// {
//     hgobj gobj = 0;
//
//     // TODO what must to return?
//     if(identifier != MQTT_PROP_CONTENT_TYPE
//             && identifier != MQTT_PROP_RESPONSE_TOPIC
//             && identifier != MQTT_PROP_ASSIGNED_CLIENT_IDENTIFIER
//             && identifier != MQTT_PROP_AUTHENTICATION_METHOD
//             && identifier != MQTT_PROP_RESPONSE_INFORMATION
//             && identifier != MQTT_PROP_SERVER_REFERENCE
//             && identifier != MQTT_PROP_REASON_STRING
//     ) {
//         gobj_log_error(gobj, 0,
//             "function",     "%s", __FUNCTION__,
//             "msgset",       "%s", MSGSET_MQTT_ERROR,
//             "msg",          "%s", "Bad string property identifier",
//             "identifier",   "%d", identifier,
//             NULL
//         );
//     }
//
//     json_t *property = property_get_property(properties, identifier);
//     if(!property) {
//         return NULL;
//     }
//     return kw_get_str(gobj, property, "value", "", 0);
// }

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
PRIVATE int packet__check_oversize(hgobj gobj, uint32_t remaining_length)
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
        trace_msg0("👉👉 Sending simple command as %s %s to '%s' %s",
            priv->iamServer?"broker":"client",
            mqtt_command_string(command),
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
    json_t *properties // owned
) {
    uint32_t payloadlen;
    uint8_t byte;
    uint8_t version;
    uint32_t headerlen;
    uint32_t proplen = 0;
    uint32_t varbytes = 0;
    json_t *local_props = NULL;
    // uint16_t receive_maximum; TODO

    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *mqtt_client_id = gobj_read_str_attr(gobj, "mqtt_client_id");
    const char *mqtt_protocol = gobj_read_str_attr(gobj, "mqtt_protocol");
    uint32_t mqtt_session_expiry_interval = (uint32_t)atoi(
        gobj_read_str_attr(gobj, "mqtt_session_expiry_interval")
    );
    uint16_t keepalive = atoi(gobj_read_str_attr(gobj, "mqtt_keepalive"));
    BOOL clean_session = (atoi(gobj_read_str_attr(gobj, "mqtt_clean_session")))?1:0;

    const char *mqtt_will_topic = gobj_read_str_attr(gobj, "mqtt_will_topic");
    const char *mqtt_will_payload = gobj_read_str_attr(gobj, "mqtt_will_payload");
    int mqtt_will_qos = atoi(gobj_read_str_attr(gobj, "mqtt_will_qos"));
    BOOL mqtt_will_retain = (atoi(gobj_read_str_attr(gobj, "mqtt_will_retain")))?1:0;
    BOOL will = !empty_string(mqtt_will_topic);

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
        return -1;
    }

    if(!empty_string(mqtt_client_id)) {
        payloadlen = (uint32_t)(2U+strlen(mqtt_client_id));
    } else {
        payloadlen = 2U;
    }

    if(will) {
        payloadlen += (uint32_t)(2+strlen(mqtt_will_topic) + 2+(uint32_t)strlen(mqtt_will_payload));
        if(protocol == mosq_p_mqtt5) {
            // TODO payloadlen += property__get_remaining_length(mosq->will->properties);
        }
    }

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
            return -1;
        }
    }

    if(!empty_string(username)) {
        payloadlen += (uint32_t)(2+strlen(username));
    }
    if(!empty_string(password)) {
        payloadlen += (uint32_t)(2+strlen(password));
    }

    int remaining_length = (int)headerlen + (int)payloadlen;
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
        byte = byte | (uint8_t)(((mqtt_will_qos&0x3)<<3) | ((will&0x1)<<2));
        if(priv->retain_available) {
            byte |= (uint8_t)((mqtt_will_retain&0x1)<<5);
        }
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
        if(protocol == mosq_p_mqtt5) {
            /* Write will properties */
            // TODO property__write_all(gobj, gbuf, mosq->will->properties, true);
        }
        mqtt_write_string(gbuf, mqtt_will_topic);
        mqtt_write_string(gbuf, mqtt_will_payload);
    }

    if(!empty_string(username)) {
        mqtt_write_string(gbuf, username);
    }
    if(!empty_string(password)) {
        mqtt_write_string(gbuf, password);
    }

    gobj_write_integer_attr(gobj, "keepalive", keepalive);

    const char *url = gobj_read_str_attr(gobj, "url");
    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0(
        "👉👉 client Sending CONNECT to \n"
        "   url '%s' \n"
        "   client '%s' \n"
        "   username '%s' \n"
        "   protocol_version '%s' \n"
        "   clean_start %d, session_expiry_interval %d \n"
        "   will-topic %s, will-payload %s, will_retain %d, will_qos %d \n"
        "   keepalive %d \n",
            (char *)url,
            (char *)mqtt_client_id,
            (char *)username,
            (char *)mqtt_protocol,
            (int)clean_session,
            (int)mqtt_session_expiry_interval,
            mqtt_will_topic,
            mqtt_will_payload,
            (int)mqtt_will_retain,
            (int)mqtt_will_qos,
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
        trace_msg0("👉👉 Sending CONNACK as %s to '%s' %s (ack %d, reason: %d '%s')",
            priv->iamServer?"broker":"client",
            priv->client_id,
            gobj_short_name(gobj_bottom_gobj(gobj)),
            ack,
            reason_code,
            mqtt_reason_string(reason_code)
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
            mqtt_property_add_int16(
                gobj, connack_props, MQTT_PROP_MAXIMUM_QOS, priv->max_qos
            );
        }

        remaining_length += property__get_remaining_length(connack_props);
    }

    if(packet__check_oversize(gobj, remaining_length)<0) {
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
            trace_msg0("👉👉 Sending DISCONNECT as %s to '%s' (%d '%s')",
                priv->iamServer?"broker":"client",
                priv->client_id,
                reason_code,
                mqtt_reason_string(reason_code)
            );
        } else {
            trace_msg0("👉👉 Sending client DISCONNECT as %s to '%s'",
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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->iamServer) {
        return send_simple_command(gobj, CMD_PINGRESP);
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt CMD_PINGREQ and not broker",
            NULL
        );
        return MOSQ_ERR_PROTOCOL;
    }
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

    if(priv->iamServer) {
        if(!priv->is_bridge) {
            // It seems the broker shouldn't receive pingresp, only if it's bridged.
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt CMD_PINGREQ: not bridge",
                NULL
            );
            return MOSQ_ERR_PROTOCOL;
        }
    } else {
        /*
         *  Client
         *  Reset ping check timer
         */
        priv->timer_check_ping = 0;
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
        trace_msg0("👉👉 Sending as %s, %s to '%s' (mid %ld, reason: %d '%s')",
            priv->iamServer?"broker":"client",
            mqtt_command_string(command & 0xF0),
            priv->client_id,
            (long)mid,
            reason_code,
            mqtt_reason_string(reason_code)
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
        property__write_all(gobj, gbuf, properties, TRUE);
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
    json_t *properties, // not owned
    uint32_t expiry_interval
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->retain_available) {
        retain = FALSE;
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("👉👉 Sending PUBLISH as %s to '%s', topic '%s' (dup %d, qos %d, retain %d, mid %d)",
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
        proplen += property__get_length_all(properties);
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
            properties = NULL;
            expiry_interval = 0;
        } else {
            packetlen += proplen + varbytes;
        }
    }
    if(packet__check_oversize(gobj, packetlen)<0) {
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
        property__write_all(gobj, gbuf, properties, FALSE);
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
        trace_msg0("👉👉 Sending SUBSCRIBE as %s client id '%s', topic '%s' (qos %d, mid %d)",
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
        trace_msg0("👉👉 Sending UNSUBSCRIBE as %s client id '%s', topic '%s' (mid %d)",
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

    if((ret=mqtt_pub_topic_check2(will_topic, tlen))<0) {
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
 *  Server
 ***************************************************************************/
PRIVATE int handle__connect(hgobj gobj, gbuffer_t *gbuf, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->in_session) {
        gobj_log_warning(gobj, 0,
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
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt CMD_CONNECT: MQTT protocol name length",
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
        // In properties is getting session_expiry_interval
        session_expiry_interval = priv->session_expiry_interval;
    }

    if(will && will_qos > priv->max_qos){
        if(protocol_version == mosq_p_mqtt5){
            send__connack(gobj, 0, MQTT_RC_QOS_NOT_SUPPORTED, NULL);
        }
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MQTT_ERROR,
            "msg",          "%s", "Mqtt: Unsupporred Will QoS in CONNECT",
            "connect_flags","%d", (int)connect_flags,
            NULL
        );
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
    json_t *kw_auth = json_pack("{s:s, s:s, s:s, s:s}",
        "client_id", SAFE_PRINT(priv->client_id),
        "jwt", SAFE_PRINT(jwt),
        "peername", SAFE_PRINT(peername),
        "dst_service", SAFE_PRINT(dst_service)
    );
    if(username_flag) {
        json_object_set_new(kw_auth, "username", json_stringn(username, username_len));
    }
    if(password_flag) {
        json_object_set_new(kw_auth, "password", json_stringn(password, password_len));
    }

    json_t *auth = gobj_authenticate(gobj, kw_auth, gobj);
    authorization = COMMAND_RESULT(gobj, auth);
    //print_json("XXX authenticated", auth); // TODO TEST

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

    /*--------------------------------------------------*
     *      Here is connect__on_authorised
     *  This must be done *after* any security checks
     *--------------------------------------------------*/
    json_t *services_roles = kw_get_dict(gobj, auth, "services_roles", NULL, KW_REQUIRED);

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
            (int)priv->session_expiry_interval,
            (int)will,
            (int)will_retain,
            (int)will_qos,
            (int)username_flag,
            (int)password_flag,
            (int)keepalive
        );
        if(connect_properties) {
            print_json("CONNECT_PROPERTIES", connect_properties);
        }

        if(priv->gbuf_will_payload) {
            gobj_trace_dump_gbuf(gobj, priv->gbuf_will_payload, "gbuf_will_payload");
        }
    }

    priv->timer_ping = 0;
    priv->timer_check_ping = 0;

    /*---------------------------------------------*
     *      Prepare the response
     *---------------------------------------------*/
    json_t *connack_props = json_object();

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
     *  Open/Create a client, done in upper level.
     *  This must be done *after* any security checks.
     *  With assigned_id == TRUE the id is random!, not a persistent id
     *  (HACK client_id is really a device_id)
     *--------------------------------------------------------------*/
    json_t *client = json_pack("{s:s, s:b, s:b, s:s, s:O, s:s, s:s, s:i, s:i, s:i, s:b}",
        "id",                       priv->client_id,
        "assigned_id",              priv->assigned_id,
        "clean_start",              priv->clean_start,
        "username",                 gobj_read_str_attr(gobj, "__username__"),
        "services_roles",           services_roles,
        "session_id",               gobj_read_str_attr(gobj, "__session_id__"),
        "peername",                 peername,
        "protocol_version",         (int)priv->protocol_version,
        "session_expiry_interval",  (int)priv->session_expiry_interval,
        "keep_alive",               (int)priv->keepalive,
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

    /*---------------------------*
     *      Open session
     *---------------------------*/
    priv->inform_on_close = TRUE;
    int ret = gobj_publish_event( // To broker
        gobj,
        EV_ON_OPEN,
        client  // owned
    );
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

    uint8_t connect_ack = 0;
    if(ret == 1) {
        connect_ack |= 0x01; // ack=1 Resume existing session
    }
    send__connack(
        gobj,
        connect_ack,
        CONNACK_ACCEPTED,
        connack_props // owned
    );

    gobj_write_bool_attr(gobj, "in_session", TRUE);
    gobj_write_bool_attr(gobj, "send_disconnect", TRUE);

    /*----------------*
     *  Open queues
     *----------------*/
    if(priv->tranger_queues) {
        open_queues(gobj);
    } else {
        gobj_write_integer_attr(gobj, "max_qos", 0);
    }

int todo_xxx;
    // TODO db__expire_all_messages(gobj);
    // db__message_write_queued_out(gobj);
    // db__message_write_inflight_out_all(gobj);

    /*
     *  Send queued messages to the reconnecting client
     */
    if(priv->tranger_queues) {
        message__release_to_inflight(gobj, mosq_md_out, TRUE);
    }

    set_timeout_periodic(priv->gobj_timer_periodic, priv->timeout_periodic);
    if(priv->tranger_queues) {
        if(priv->timeout_backup > 0) {
            priv->t_backup = start_sectimer(priv->timeout_backup);
        } else {
            priv->t_backup = 0;
        }
    }

    /*
     *  Start timer keepalive/ping
     */
    if(priv->keepalive > 0) {
        priv->timer_ping = start_sectimer((priv->keepalive*3)/2);
    }

    return 0;
}

/***************************************************************************
 *  Only for clients, not implemented for bridge
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
                "command",      "%s", mqtt_command_string(CMD_CONNACK),
                "reason",       "%s", mqtt_reason_string(MQTT_RC_UNSUPPORTED_PROTOCOL_VERSION),
                NULL
            );
            // TODO connack_callback(mosq, MQTT_RC_UNSUPPORTED_PROTOCOL_VERSION, connect_flags, NULL);
            return ret;

        } else if(ret<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Error in properties",
                "command",      "%s", mqtt_command_string(CMD_CONNACK),
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
                    "command",      "%s", mqtt_command_string(CMD_CONNACK),
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

    // priv->msgs_out.inflight_quota = priv->msgs_out.inflight_maximum;
    // message__reconnect_reset(mosq, true); TODO important?!

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("👈👈 COMMAND=%s, as %s, client %s, reason: %d '%s'",
            mqtt_command_string(CMD_CONNACK),
            priv->iamServer? "server":"client",
            SAFE_PRINT(priv->client_id),
            reason_code,
            priv->protocol_version == mosq_p_mqtt5?
                mqtt_reason_string(reason_code) :
                mqtt_connack_string(reason_code)
        );
        if(properties) {
            print_json("CONNACK properties", properties);
        }
    }

    switch(reason_code) {
        case 0:
            // TODO message__retry_check(mosq); important!
            /*
             *  Set in session
             */
            gobj_write_bool_attr(gobj, "in_session", TRUE);
            gobj_write_bool_attr(gobj, "send_disconnect", TRUE); // TODO necessary?

            /*
             *  Start the periodic timer and begin ping timer
             */
            set_timeout_periodic(priv->gobj_timer_periodic, priv->timeout_periodic);
            if(priv->timeout_backup > 0) {
                priv->t_backup = start_sectimer(priv->timeout_backup);
            } else {
                priv->t_backup = 0;
            }
            if(priv->keepalive > 0) {
                priv->timer_ping = start_sectimer(priv->keepalive);
            }

            return MOSQ_ERR_SUCCESS;

        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            {
                // TODO don't retry connections?
                const char *reason = priv->protocol_version != mosq_p_mqtt5?
                    mqtt_connack_string(reason_code):mqtt_reason_string(reason_code);
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
        trace_msg0("  👈 Received DISCONNECT, as %s, client '%s' (%d '%s')",
            priv->iamServer? "server":"client",
            SAFE_PRINT(priv->client_id),
            reason_code,
            mqtt_reason_string(reason_code)
        );
        if(properties) {
            gobj_trace_json(gobj, properties, "properties");
        }
    }

    if(priv->protocol_version == mosq_p_mqtt311 || priv->protocol_version == mosq_p_mqtt5) {
        if(priv->frame_head.flags != 0x00) {
            send_drop(gobj, MOSQ_ERR_PROTOCOL);
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

    send_drop(gobj, MOSQ_ERR_SUCCESS);
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
        trace_msg0("  👈 Received DISCONNECT, as %s, client '%s' (%d '%s')",
            priv->iamServer? "server":"client",
            SAFE_PRINT(priv->client_id),
            reason_code,
            mqtt_reason_string(reason_code)
        );
        if(properties) {
            gobj_trace_json(gobj, properties, "properties");
        }
    }

    send_drop(gobj, reason_code);
    return 0;
}

/***************************************************************************
 *  Server
 ***************************************************************************/
PRIVATE int handle__subscribe(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint16_t mid;
    json_int_t subscription_identifier = 0;
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
            // TODO review
            /* FIXME - it would be better if property__read_all() returned
             * MOSQ_ERR_MALFORMED_PACKET, but this is would change the library
             * return codes so needs doc changes as well. */
            if(rc == MOSQ_ERR_PROTOCOL){
                return MOSQ_ERR_MALFORMED_PACKET;
            } else {
                return rc;
            }
        }

        subscription_identifier = property_read_varint(properties, MQTT_PROP_SUBSCRIPTION_IDENTIFIER);
        if(subscription_identifier != -1) {
            /* If the identifier exists and was force set to 0, this is an error */
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
        } else {
            subscription_identifier = 0; // reset to 0 if not used
        }
    }

    json_t *jn_list = json_array();

    while(gbuffer_leftbytes(gbuf)>0) {
        uint16_t slen;
        char *sub = NULL;
        uint8_t qos;
        uint8_t subscription_options;
        uint8_t retain_handling = 0;

        if(mqtt_read_string(gobj, gbuf, &sub, &slen)<0) {
            // Error already logged
            JSON_DECREF(jn_list)
            JSON_DECREF(properties)
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
            JSON_DECREF(properties)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        if(mqtt_sub_topic_check2(sub, slen)<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt subscribe: Invalid subscription string",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            JSON_DECREF(jn_list)
            JSON_DECREF(properties)
            return MOSQ_ERR_MALFORMED_PACKET;
        }

        if(mqtt_read_byte(gobj, gbuf, &subscription_options)) {
            // Error already logged
            JSON_DECREF(jn_list)
            JSON_DECREF(properties)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        if(priv->protocol_version == mosq_p_mqtt31 ||
            priv->protocol_version == mosq_p_mqtt311
        ) {
            qos = subscription_options;
            subscription_options = 0;
            // if(priv->is_bridge) {
            //     subscription_options = MQTT_SUB_OPT_RETAIN_AS_PUBLISHED | MQTT_SUB_OPT_NO_LOCAL;
            // }
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
                JSON_DECREF(properties)
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
                JSON_DECREF(properties)
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
            JSON_DECREF(properties)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        if(qos > priv->max_qos) {
            qos = priv->max_qos;
        }

        json_t *jn_sub = json_pack("{s:s#, s:i, s:i, s:i, s:i}",
            "sub", sub, (int)slen,
            "subscription_identifier", (int)subscription_identifier,
            "qos", (int)qos,
            "subscription_options", (int)subscription_options,
            "retain_handling", retain_handling
        );
        json_array_append_new(jn_list, jn_sub);

        if(gobj_trace_level(gobj) & SHOW_DECODE) {
            trace_msg0("  👈 Received SUBSCRIBE, as %s, client '%s', topic '%s' (QoS %d)",
                priv->iamServer? "server":"client",
                SAFE_PRINT(priv->client_id),
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
            JSON_DECREF(properties)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
    }

    JSON_DECREF(properties); // Free subscribe command properties

    gbuffer_t *gbuf_payload = gbuffer_create(256, 12*1024);
    gbuffer_incref(gbuf_payload); // Avoid be destroyed, get it the response

    json_t *kw_subscribe = json_pack("{s:s, s:i, s:o, s:I}",
        "client_id", priv->client_id,
        "protocol_version", (int)priv->protocol_version,
        "subs", jn_list, // owned
        "gbuffer", (json_int_t)(uintptr_t)gbuf_payload
    );

    json_t *kw_iev = iev_create(
        gobj,
        EV_MQTT_SUBSCRIBE,
        kw_subscribe // owned
    );

    int rc = gobj_publish_event( // To broker
        gobj,
        EV_ON_IEV_MESSAGE,
        kw_iev // owned but gbuf_payload survives
    );
    if(rc < 0) {
        gbuffer_decref(gbuf_payload);
        return -1;
    }

    send__suback(
        gobj,
        mid,
        gbuf_payload, // owned
        NULL // owned (properties suback not used)
    );

    int todo_xxx; // TODO
    // if(priv->current_out_packet == NULL){
    //     db__message_write_queued_out(gobj);
    //     db__message_write_inflight_out_latest(gobj);
    // }

    return MOSQ_ERR_SUCCESS;
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
        remaining_length += property__get_remaining_length(properties);
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("👉👉 Sending SUBACK as %s to '%s' %s",
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
        property__write_all(gobj, gbuf, properties, TRUE);
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
 *  Server
 ***************************************************************************/
PRIVATE int handle__unsubscribe(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint16_t mid;
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
            JSON_DECREF(properties)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
    }

    json_t *jn_list = json_array();

    while(gbuffer_leftbytes(gbuf)>0) {
        uint16_t slen;
        char *sub = NULL;
        if(mqtt_read_string(gobj, gbuf, &sub, &slen)<0) {
            // Error already logged
            JSON_DECREF(jn_list)
            JSON_DECREF(properties)
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
            JSON_DECREF(properties)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        if(mqtt_sub_topic_check2(sub, slen)<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Invalid unsubscription string",
                "client_id",    "%s", priv->client_id,
                NULL
            );
            JSON_DECREF(jn_list)
            JSON_DECREF(properties)
            return MOSQ_ERR_MALFORMED_PACKET;
        }

        if(gobj_trace_level(gobj) & SHOW_DECODE) {
            trace_msg0("  👈 Received UNSUBSCRIBE, as %s, client '%s', topic '%s'",
                priv->iamServer? "server":"client",
                SAFE_PRINT(priv->client_id),
                sub
            );
        }

        json_t *jn_sub = json_pack("{s:s#}",
            "sub", sub, (int)slen
        );
        json_array_append_new(jn_list, jn_sub);
    }

    JSON_DECREF(properties); // Free unsubscribe command properties

    gbuffer_t *gbuf_payload = gbuffer_create(256, 12*1024);
    gbuffer_incref(gbuf_payload); // Avoid be destroyed, get it the response

    json_t *kw_unsubscribe = json_pack("{s:s, s:i, s:o, s:I}",
        "client_id", priv->client_id,
        "protocol_version", (int)priv->protocol_version,
        "subs", jn_list, // owned
        "gbuffer", (json_int_t)(uintptr_t)gbuf_payload
    );

    json_t *kw_iev = iev_create(
        gobj,
        EV_MQTT_UNSUBSCRIBE,
        kw_unsubscribe // owned
    );

    int rc = gobj_publish_event( // To broker
        gobj,
        EV_ON_IEV_MESSAGE,
        kw_iev // owned but gbuf_payload survives
    );
    if(rc < 0) {
        gbuffer_decref(gbuf_payload);
        return -1;
    }

    return send__unsuback(
        gobj,
        mid,
        gbuf_payload, // owned
        NULL // owned (properties unsuback not used)
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
        trace_msg0("👉👉 Sending UNSUBACK as %s to '%s' %s",
            priv->iamServer?"broker":"client",
            priv->client_id,
            gobj_short_name(gobj_bottom_gobj(gobj))
        );
    }

    uint32_t remaining_length = 2;
    uint32_t reason_code_count = gbuffer_leftbytes(gbuf_payload);

    if(priv->protocol_version == mosq_p_mqtt5) {
        remaining_length += property__get_remaining_length(properties);
        remaining_length += (uint32_t)reason_code_count;
    }

    gbuffer_t *gbuf = build_mqtt_packet(gobj, CMD_UNSUBACK, remaining_length);
    mqtt_write_uint16(gbuf, mid);

    if(priv->protocol_version == mosq_p_mqtt5) {
        property__write_all(gobj, gbuf, properties, TRUE);
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
        trace_msg0("  👈 Received SUBACK, as %s, client '%s' (mid: %d)",
            priv->iamServer? "server":"client",
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

    gobj_publish_event( // To client user
        gobj,
        EV_ON_IEV_MESSAGE,
        kw_iev
    );

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
        trace_msg0("  👈 Received UNSUBACK as %s, client '%s' (mid: %d)",
            priv->iamServer? "server":"client",
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

    gobj_publish_event( // To client user
        gobj,
        EV_ON_IEV_MESSAGE,
        kw_iev
    );

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
    uint8_t reason_code = 0;
    uint16_t slen;
    json_t *properties = NULL;
    uint32_t message_expiry_interval = 0;
    int topic_alias = -1;
    uint16_t mid = 0;
    gbuffer_t *payload = NULL;
    json_t *kw_mqtt_msg = NULL;

    /*-----------------------------------*
     *  Get and check the header
     *-----------------------------------*/
    uint8_t header = priv->frame_head.flags;
    BOOL dup = (header & 0x08)>>3;      // possible values of 0,1
    uint8_t qos = (header & 0x06)>>1;   // shift of two bites -> possible values of 0,1,2,3
    BOOL retain = (header & 0x01);      // possible values of 0,1

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
            JSON_DECREF(properties)
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

    if(mqtt_pub_topic_check2(topic, slen)<0) {
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

    /*-----------------------------------*
     *          Get the payload
     *-----------------------------------*/
    size_t payloadlen = gbuffer_leftbytes(gbuf);

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

        if(mqtt_read_bytes(gobj, gbuf, gbuffer_cur_wr_pointer(payload), (int)payloadlen)<0) {
            GBMEM_FREE(topic)
            JSON_DECREF(properties)
            GBUFFER_DECREF(payload)
            return MOSQ_ERR_MALFORMED_PACKET;
        }
        gbuffer_set_wr(payload, payloadlen);
    }

    /*-----------------------------------*
     *      Check permissions
     *-----------------------------------*/

    /* Check for topic access */
    // TODO
    rc = 0;
    // rc = mosquitto_acl_check(context, msg->topic, msg->payloadlen, msg->payload, msg->qos, msg->retain, MOSQ_ACL_WRITE);
    if(rc == -1) {
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
        GBUFFER_DECREF(payload)
        return rc;
    }

    /*-----------------------------------*
     *      Build our json message
     *-----------------------------------*/
    /*
     *  Create the MQTT message for published message received in broker
     */
    kw_mqtt_msg = new_mqtt_message( // broker, message from client
        gobj,
        priv->client_id,
        topic,
        payload, // owned
        qos,
        mid,
        retain,
        dup,
        properties, // owned
        message_expiry_interval,
        mosquitto_time()
    );
    payload = NULL; // owned by kw_mqtt_msg

    uint16_t user_flag = mosq_mo_client | mosq_md_in;
    if(qos == 1) {
        user_flag |= mosq_m_qos1;
    } else if(qos == 2) {
        user_flag |= mosq_m_qos2;
    }
    if(retain) {
        user_flag |= mosq_m_retain;
    }
    if(dup) {
        user_flag |= mosq_m_dup;
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("  👈 Received PUBLISH, as %s, client '%s', topic '%s' (dup %d, qos %d, retain %d, mid %d, len %ld)",
            priv->iamServer? "server":"client",
            SAFE_PRINT(priv->client_id),
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

    /*-----------------------------------*
     *      Process the message
     *-----------------------------------*/
    /*
     *  Here mosquitto checked the inflight input queue to search repeated mid's
     *  with db__message_store_find() and db__message_remove_incoming() functions.
     *  Not good for performance, better to have a periodic gobj_timer that cleans incomplete mid's
     *  and searching in the inflight queue ONLY if the message has the DUP flag.
     */
    if(qos == 2) {
        if(dup) {
            /*
             *  Delete possible msg with same mid
             */
            message__remove(gobj, mid, mosq_md_in, 2, NULL);
        }

        if(!db__ready_for_flight(gobj, mosq_md_in, qos)) {
            /* Client isn't allowed any more incoming messages, so fail early */
            reason_code = MQTT_RC_QUOTA_EXCEEDED;
            goto process_bad_message;
        }
    }

    switch(qos) {
        case 0:
            {
                /*
                 *  Broker
                 *  Dispatch the message to the subscribers
                 */
                json_t *kw_iev = iev_create(
                    gobj,
                    EV_MQTT_MESSAGE,
                    kw_incref(kw_mqtt_msg) // owned
                );
                rc = gobj_publish_event( // To broker, sub__messages_queue, return # subscribers
                    gobj,
                    EV_ON_IEV_MESSAGE,
                    kw_iev
                );
            }
            break;

        case 1:
            // util__decrement_receive_quota(context);
            {
                /*
                 *  Broker
                 *  Dispatch the message to subscribers
                 */
                json_t *kw_iev = iev_create(
                    gobj,
                    EV_MQTT_MESSAGE,
                    kw_incref(kw_mqtt_msg) // owned
                );
                rc = gobj_publish_event( // To broker, sub__messages_queue, return # subscribers
                    gobj,
                    EV_ON_IEV_MESSAGE,
                    kw_iev
                );

                /*
                 *  Response acknowledge
                 */
                if(rc > 0 || priv->protocol_version != mosq_p_mqtt5) {
                    send__puback(gobj, mid, 0, NULL);
                } else if(rc == 0) {
                    send__puback(gobj, mid, MQTT_RC_NO_MATCHING_SUBSCRIBERS, NULL);
                }
            }
            break;

        case 2:
            {
                /*
                 *  Append the message to the input queue
                 *  wait completion of the message
                 */
                user_flag |= mosq_ms_wait_for_pubrel;
                message__queue(
                    gobj,
                    kw_incref(kw_mqtt_msg), // owned
                    mosq_md_in,
                    user_flag
                );

                /*
                 *  Response acknowledge
                 */
                send__pubrec(gobj, mid, 0, NULL);
            }
            break;
    }

    kw_decref(kw_mqtt_msg);

    /*
     *  Pull from input queued list
     */
    db__message_write_queued_in(gobj); // TODO review
    return rc;

process_bad_message:
    rc = MOSQ_ERR_UNKNOWN;
    switch(qos) {
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

    // TODO review
    // if(priv->max_queued_messages > 0 && priv->out_packet_count >= priv->max_queued_messages) {
    //     rc = MQTT_RC_QUOTA_EXCEEDED;
    // }

    GBUFFER_DECREF(payload)
    GBMEM_FREE(topic)
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
    uint8_t reason_code = 0;
    uint16_t mid = 0;
    uint16_t slen;
    json_t *properties = NULL;
    uint32_t expiry_interval = 0; // TODO get from properties although be client?

    /*-----------------------------------*
     *      Get and check the header
     *-----------------------------------*/
    uint8_t header = priv->frame_head.flags;
    BOOL dup = (header & 0x08)>>3;      // possible values of 0,1
    uint8_t qos = (header & 0x06)>>1;   // shift of two bites -> possible values of 0,1,2,3
    BOOL retain = (header & 0x01);      // possible values of 0,1

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

    /*-----------------------------------*
     *          Get the payload
     *-----------------------------------*/
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

    /*-----------------------------------*
     *      Check permissions
     *-----------------------------------*/
    // TODO need it?

    /*-----------------------------------*
     *      Build our json message
     *-----------------------------------*/
    /*
     *  Create the MQTT message for published message received in client
     */
    json_t *kw_mqtt_msg = new_mqtt_message( // client, message from broker
        gobj,
        priv->client_id,
        topic,
        payload, // owned
        qos,
        mid,
        retain,
        dup,
        properties, // owned
        expiry_interval,
        mosquitto_time()
    );

    uint16_t user_flag = mosq_mo_broker | mosq_md_in;
    if(qos == 1) {
        user_flag |= mosq_m_qos1;
    } else if(qos == 2) {
        user_flag |= mosq_m_qos2;
    }
    if(retain) {
        user_flag |= mosq_m_retain;
    }
    if(dup) {
        user_flag |= mosq_m_dup;
    }

    if(gobj_trace_level(gobj) & SHOW_DECODE) {
        trace_msg0("  👈 Received PUBLISH, as '%s', client %s, topic '%s' (dup %d, qos %d, retain %d, mid %d, len %ld)",
            priv->iamServer? "server":"client",
            SAFE_PRINT(priv->client_id),
            topic,
            dup,
            qos,
            retain,
            mid,
            (long)payloadlen
        );
    }

    GBMEM_FREE(topic)

    /*-----------------------------------*
     *      Process the message
     *-----------------------------------*/

    /*
     *  QoS 2: Discard incoming message with the same mid
     */
    if(qos == 2) {
        if(dup) {
            db__message_remove_incoming_dup(gobj, mid); // Search and remove without warning
        }

        if(!db__ready_for_flight(gobj, mosq_md_in, qos)) {
            /* No more space for incoming messages, so fail early */
            reason_code = MQTT_RC_QUOTA_EXCEEDED;
            goto process_bad_message;
        }
    }

    if(retain) {
        /*-------------------------------------------*
         *  Incoming Retain
         *  Important thick to inform to user
         **-----------------------------------------*/
        // TODO
    }

    switch(qos) {
        case 0:
            {
                /*
                 *  Client
                 *  Callback the message to the user
                 */
                json_t *kw_iev = iev_create(
                    gobj,
                    EV_MQTT_MESSAGE,
                    kw_incref(kw_mqtt_msg) // owned
                );

                gobj_publish_event( // To client user
                    gobj,
                    EV_ON_IEV_MESSAGE,
                    kw_iev
                );
            }
            break;

        case 1:
            // util__decrement_receive_quota(mosq);

            {
                /*
                 *  Client
                 *  Callback the message to the user
                 */
                json_t *kw_iev = iev_create(
                    gobj,
                    EV_MQTT_MESSAGE,
                    kw_incref(kw_mqtt_msg) // owned
                );

                gobj_publish_event( // To client user
                    gobj,
                    EV_ON_IEV_MESSAGE,
                    kw_iev
                );

                /*
                 *  Response acknowledge
                 */
                send__puback(gobj, mid, 0, NULL);
            }
            break;

        case 2:
            // util__decrement_receive_quota(mosq);
            {
                /*
                 *  Append the message to the input queue
                 *  wait completion of the message
                 */
                user_flag |= mosq_ms_wait_for_pubrel;
                message__queue(
                    gobj,
                    kw_incref(kw_mqtt_msg), // owned
                    mosq_md_in,
                    user_flag
                );

                /*
                 *  Response acknowledge
                 */
                send__pubrec(gobj, mid, 0, NULL);
            }
            break;
    }

    KW_DECREF(kw_mqtt_msg)

    /*
     *  Pull from input queued list
     */
    db__message_write_queued_in(gobj);
    return rc;

process_bad_message:
    rc = MOSQ_ERR_UNKNOWN;
    switch(qos) {
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

    // TODO review
    // if(priv->max_queued_messages > 0 && priv->out_packet_count >= priv->max_queued_messages) {
    //     rc = MQTT_RC_QUOTA_EXCEEDED;
    // }

    GBMEM_FREE(topic)
    KW_DECREF(kw_mqtt_msg)
    return rc;
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
            trace_msg0("  👈 Received %s, as %s, client '%s' (mid: %d, reason: %d '%s')",
                type,
                priv->iamServer? "server":"client",
                SAFE_PRINT(priv->client_id),
                mid,
                reason_code,
                mqtt_reason_string(reason_code)
            );
        }

        /*
         *  Immediately free,
         *  we don't do anything with Reason String or User Property at the moment
         */
        JSON_DECREF(properties)

        db__message_delete_outgoing(gobj, mid, mosq_ms_wait_for_pubcomp, qos);
        return MOSQ_ERR_SUCCESS;

    } else {
        if(gobj_trace_level(gobj) & SHOW_DECODE) {
            trace_msg0("  👈 Received %s, as %s, client %s (mid: %d, reason: %d '%s')",
                type,
                priv->iamServer? "server":"client",
                SAFE_PRINT(priv->client_id),
                mid,
                reason_code,
                mqtt_reason_string(reason_code)
            );
        }

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

            gobj_publish_event( // To client user
                gobj,
                EV_ON_IEV_MESSAGE,
                kw_iev
            );

        } else if(rc != MOSQ_ERR_NOT_FOUND) {
            JSON_DECREF(properties)
            return rc;
        }

        message__release_to_inflight(gobj, mosq_md_out, FALSE);

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
            trace_msg0("  👈 Received PUBREC, as %s, client '%s' (mid: %d, reason: %d '%s')",
                priv->iamServer? "server":"client",
                SAFE_PRINT(priv->client_id),
                mid,
                reason_code,
                mqtt_reason_string(reason_code)
            );
        }

        if(reason_code < 0x80) {
            rc = db__message_update_outgoing(gobj, mid, mosq_ms_wait_for_pubcomp, 2);
        } else {
            return db__message_delete_outgoing(gobj, mid, mosq_ms_wait_for_pubrec, 2);
        }

    } else {
        if(gobj_trace_level(gobj) & SHOW_DECODE) {
            trace_msg0("  👈 Received PUBREC, as %s, client %s (mid: %d, reason: %d '%s')",
                priv->iamServer? "server":"client",
                SAFE_PRINT(priv->client_id),
                mid,
                reason_code,
                mqtt_reason_string(reason_code)
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

                gobj_publish_event( // To client user
                    gobj,
                    EV_ON_IEV_MESSAGE,
                    kw_iev
                );
            }

            message__release_to_inflight(gobj, mosq_md_out, FALSE);

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

    send__pubrel(gobj, mid, NULL);

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
        /*------------------------------------*
         *          Broker
         *------------------------------------*/
        if(gobj_trace_level(gobj) & SHOW_DECODE) {
            trace_msg0("  👈 Received PUBREL, as %s, client '%s' (mid: %d)",
                priv->iamServer? "server":"client",
                SAFE_PRINT(priv->client_id),
                mid
            );
        }

        rc = db__message_release_incoming(gobj, mid);
        if(rc == MOSQ_ERR_NOT_FOUND) {
            /* Message not found. Still send a PUBCOMP anyway because this could be
            * due to a repeated PUBREL after a client has reconnected. */
        } else if(rc != MOSQ_ERR_SUCCESS) {
            return rc;
        }

        /*
         *  Response acknowledge
         */
        send__pubcomp(gobj, mid, NULL);

    } else {
        /*------------------------------------*
         *          Client
         *------------------------------------*/
        if(gobj_trace_level(gobj) & SHOW_DECODE) {
            trace_msg0("  👈 Received PUBREL, as %s, client %s (mid: %d)",
                priv->iamServer? "server":"client",
                SAFE_PRINT(priv->client_id),
                mid
            );
        }

        /*
         *  Response acknowledge
         */
        send__pubcomp(gobj, mid, NULL);

        json_t *kw_mqtt_msg;
        message__remove(gobj, mid, mosq_md_in, 2, &kw_mqtt_msg);
        if(kw_mqtt_msg) {
            /* Only pass the message on if we have removed it from the queue - this
             * prevents multiple callbacks for the same message.
             */
            /*
             *  Client
             *  Callback the message to the user
             */
            json_t *kw_iev = iev_create(
                gobj,
                EV_MQTT_MESSAGE,
                kw_mqtt_msg // owned
            );

            gobj_publish_event( // To client user
                gobj,
                EV_ON_IEV_MESSAGE,
                kw_iev
            );
        }
    }

    JSON_DECREF(properties)
    return MOSQ_ERR_SUCCESS;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE uint16_t mqtt_mid_generate(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint16_t mid = priv->last_mid + 1;
    if(mid == 0) {
        mid = 1;
    }
    gobj_write_integer_attr(gobj, "last_mid", mid);

    return priv->last_mid;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void send_drop(hgobj gobj, int reason) // old do_disconnect()
{
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void ws_close(hgobj gobj, int reason)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_in_this_state(gobj, ST_DISCONNECTED)) {
        gobj_change_state(gobj, ST_DISCONNECTED);
        clear_timeout(priv->gobj_timer_periodic);

        /*
         *  WARNING: feedback, retroalimentación
         *  Set timer close before send drop, could be event disconnect come immediately
         */
        set_timeout(priv->gobj_timer, priv->timeout_close);

        if(priv->in_session) {
            if(priv->send_disconnect) {
                send__disconnect(gobj, reason, NULL);
            }
        }

        // sending event EV_DROP, WARNING case of event's feedback (EV_DISCONNECTED)
        send_drop(gobj, reason);
    }
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
    set_timeout(priv->gobj_timer, priv->timeout_handshake);
}

/***************************************************************************
 *  Start to wait frame header
 ***************************************************************************/
PRIVATE void start_wait_frame_header(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

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
    //                 "command",      "%s", mqtt_command_string(frame->command),
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
    //                 "command",      "%s", mqtt_command_string(frame->command),
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

    /*
     *  Server: reset keepalive timer on every received packet from client
     *  Per MQTT spec, the server must track all Control Packets, not just PINGREQ
     */
    if(priv->iamServer) {
        if(priv->keepalive > 0) {
            priv->timer_ping = start_sectimer((priv->keepalive * 3) / 2);
        }
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
                    gobj_publish_event( // To client user
                        gobj,
                        EV_ON_OPEN,
                        jn_data
                    );

                    /*----------------*
                     *  Open queues
                     *----------------*/
                    if(priv->tranger_queues) {
                        open_queues(gobj);
                    } else {
                        gobj_write_integer_attr(gobj, "max_qos", 0);
                    }

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
                "command",      "%s", mqtt_command_string(frame->command),
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

    if(gobj_is_running(gobj) && !gobj_in_this_state(gobj, ST_DISCONNECTED)) {
        start_wait_frame_header(gobj);
    }

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

    if(priv->istream_payload) {
        istream_destroy(priv->istream_payload);
        priv->istream_payload = 0;
    }

    /*----------------*
     *  Close queues
     *----------------*/
    if(priv->tranger_queues) {
        close_queues(gobj);
    }

    GBUFFER_DECREF(priv->gbuf_will_payload);

    clear_timeout(priv->gobj_timer);
    clear_timeout(priv->gobj_timer_periodic);
    priv->timer_ping = 0;
    priv->timer_check_ping = 0;

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

        gobj_publish_event( // To broker and client user?
            gobj,
            EV_ON_CLOSE,
            kw2
        );
    }

    if(gobj_is_volatil(src)) {
        gobj_set_bottom_gobj(gobj, 0);
    }

    JSON_DECREF(priv->jn_alias_list)
    gobj_reset_volatil_attrs(gobj);
    restore_client_attributes(gobj);

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
                    mqtt_command_string(frame->command),
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
                        "command",      "%s", mqtt_command_string(frame->command),
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
                        "command",      "%s", mqtt_command_string(frame->command),
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
                set_timeout(priv->gobj_timer, priv->timeout_payload);

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

    clear_timeout(priv->gobj_timer);

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
                    mqtt_command_string(frame->command),
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
                set_timeout(priv->gobj_timer, priv->timeout_payload);

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
    // We wait forever, if it has done the login.
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

    clear_timeout(priv->gobj_timer);

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
                trace_msg0("❌❌ Mqtt error: disconnecting: %s", priv->client_id);
                gobj_trace_dump_full_gbuf(gobj, gbuf, "Mqtt error: disconnecting");
            }
            ws_close(gobj, MQTT_RC_PROTOCOL_ERROR);
            KW_DECREF(kw)
            return -1;
        }
    } else {
        set_timeout(priv->gobj_timer, priv->timeout_payload);
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
 *  From broker, send message (publish) to client
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, const char *event, json_t *kw_mqtt_msg, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*------------------------*
     *   Get info of message
     *------------------------*/
    const char *topic = kw_get_str(gobj, kw_mqtt_msg, "topic", "", KW_REQUIRED);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(
        gobj, kw_mqtt_msg, "gbuffer", 0, KW_REQUIRED
    );
    if(gobj_trace_level(gobj) & TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "%s, topic %s", gobj_short_name(gobj), topic);
    }
    uint8_t qos = (uint8_t)kw_get_int(gobj, kw_mqtt_msg, "qos", 0, KW_REQUIRED);
    BOOL retain = kw_get_bool(gobj, kw_mqtt_msg, "retain", 0, KW_REQUIRED);
    json_t *properties = kw_get_dict(gobj, kw_mqtt_msg, "properties", 0, 0);
    int expiry_interval = (int)kw_get_int(gobj, kw_mqtt_msg, "expiry_interval", 0, 0);

    if(qos > priv->max_qos) {
        qos = priv->max_qos;
    }

    if(qos == 0) {
        uint16_t mid = mqtt_mid_generate(gobj);
        send__publish(
            gobj,
            mid,
            topic,
            gbuf,
            (uint8_t)qos,
            retain,
            FALSE,      // dup
            properties,
            expiry_interval
        );
    } else {
        uint16_t user_flag = mosq_mo_broker | mosq_md_out;
        if(qos == 1) {
            user_flag |= mosq_m_qos1;
        } else if(qos == 2) {
            user_flag |= mosq_m_qos2;
        }
        if(retain) {
            user_flag |= mosq_m_retain;
        }

        kw_delete_metadata_keys(kw_mqtt_msg);  // don't save __temp__

        message__queue(
            gobj,
            kw_incref(kw_mqtt_msg), // owned
            mosq_md_out,
            user_flag
        );
    }

    KW_DECREF(kw_mqtt_msg)
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
            "qos",          "%d", (int)qos,
            "max_qos",      "%d", (int)priv->max_qos,
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
        if(mqtt_validate_utf8(topic, (int)tlen)<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt publish: topic malformed utf8",
                NULL
            );
            KW_DECREF(kw)
            return -1;
        }
        if(mqtt_pub_topic_check2(topic, strlen(topic))<0) {
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

    /*
     *  Create the MQTT message for client publishing message to broker
     */
    json_t *kw_mqtt_msg = new_mqtt_message( // client sending message to broker
        gobj,
        priv->client_id,
        topic,
        gbuf_payload, // not owned
        qos,
        0,
        retain,
        FALSE,      // dup,
        properties, // not owned
        expiry_interval,
        mosquitto_time()
    );

    if(qos == 0) {
        int mid = mqtt_mid_generate(gobj);
        if(send__publish(
            gobj,
            mid,
            topic,
            gbuf_payload,
            (uint8_t)qos,
            retain,
            FALSE,          // dup
            properties,     // cmsg_props
            expiry_interval // expiry_interval
        )==0) {
            /*
             *  Callback to user
             *  Only inform the client the message has been sent once, qos 0
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

            gobj_publish_event( // To client user
                gobj,
                EV_ON_IEV_MESSAGE,
                kw_iev
            );
        }
    } else {

        uint16_t user_flag = mosq_mo_client | mosq_md_out;
        if(qos == 1) {
            user_flag |= mosq_m_qos1;
        } else if(qos == 2) {
            user_flag |= mosq_m_qos2;
        }
        if(retain) {
            user_flag |= mosq_m_retain;
        }

        message__queue(
            gobj,
            kw_incref(kw_mqtt_msg),
            mosq_md_out,
            user_flag
        );
    }

    KW_DECREF(kw_mqtt_msg)
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
        if(mqtt_validate_utf8(sub, slen)<0) {
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
        if(packet__check_oversize(gobj, remaining_length)<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt subscribe: message too big",
                NULL
            );
            KW_DECREF(kw)
            return -1;
        }
    }
    if(priv->protocol_version != mosq_p_mqtt5) {
        options = 0;
    }

    send__subscribe(
        gobj,
        mqtt_mid_generate(gobj),
        subs,
        qos|options,
        properties
    );

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
        if(mqtt_validate_utf8(sub, slen)<0) {
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
        if(packet__check_oversize(gobj, remaining_length)<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "Mqtt subscribe: message too big",
                NULL
            );
            KW_DECREF(kw)
            return -1;
        }
    }

    send__unsubscribe(
        gobj,
        mqtt_mid_generate(gobj),
        subs,
        properties
    );

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_periodic(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Check keepalive/ping timer
     */
    if(priv->timer_ping > 0) {
        if(test_sectimer(priv->timer_ping)) {
            if(priv->iamServer) {
                /*
                 *  Server: keepalive timeout, no packet received from client
                 *  within 1.5 * keepalive seconds. Close the connection.
                 */
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MQTT_ERROR,
                    "msg",          "%s", "Client keepalive timeout, closing connection",
                    "client_id",    "%s", SAFE_PRINT(priv->client_id),
                    "keepalive",    "%d", (int)priv->keepalive,
                    NULL
                );
                priv->timer_ping = 0;
                ws_close(gobj, MOSQ_ERR_PROTOCOL);
                KW_DECREF(kw)
                return 0;
            } else {
                /*
                 *  Client: time to send a PINGREQ
                 */
                send_simple_command(gobj, CMD_PINGREQ);

                /*
                 *  Start timer to check PINGRESP arrives
                 *  Give the client keepalive / 2 seconds to check
                 */
                time_t check_timeout = (priv->keepalive) / 2;
                if(check_timeout < 1) {
                    check_timeout = 1;
                }
                priv->timer_check_ping = start_sectimer(check_timeout);

                /*
                 *  Restart timer for next ping
                 */
                if(priv->keepalive > 0) {
                    priv->timer_ping = start_sectimer(priv->keepalive);
                }
            }
        }
    }

    /*
     *  Check ping response timer (client only)
     */
    if(priv->timer_check_ping > 0) {
        if(test_sectimer(priv->timer_check_ping)) {
            /*
             *  No PINGRESP received in time. Close the connection.
             */
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MQTT_ERROR,
                "msg",          "%s", "PINGRESP timeout, closing connection",
                NULL
            );
            priv->timer_check_ping = 0;
            ws_close(gobj, MOSQ_ERR_PROTOCOL);
            KW_DECREF(kw)
            return 0;
        }
    }

    if(priv->timeout_backup > 0 && test_sectimer(priv->t_backup)) {

        debug_json2(priv->trq_in_msgs->topic, "CLIENT_ID QUEUE TOPIC %s %s", priv->client_id, gobj_full_name(gobj)); // TODO TEST

        if(priv->trq_in_msgs) {
            if(tr2q_inflight_size(priv->trq_in_msgs)==0) { // && priv->pending_acks==0) {
                // Check and do backup only when no message
                tr2q_check_backup(priv->trq_in_msgs);
            }
        }
        if(priv->trq_out_msgs) {
            if(tr2q_inflight_size(priv->trq_out_msgs)==0) { // && priv->pending_acks==0) {
                // Check and do backup only when no message
                tr2q_check_backup(priv->trq_out_msgs);
            }
        }
        priv->t_backup = start_sectimer(priv->timeout_backup);
    }

    // TODO implement a cleaner of messages inflight with pending acks and timeout reached
    // TODO or remove all messages with same mid when receiving and ack

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Session taken over by another connection with the same client_id.
     *  WARNING The broker could have cleaned up the old session and created a new one.
     */
    BOOL session_take_over = kw_get_bool(gobj, kw, "session_take_over", 0, KW_REQUIRED);

    int reason_code = 0;
    if(session_take_over) {
        reason_code = (priv->protocol_version == mosq_p_mqtt5)?MQTT_RC_SESSION_TAKEN_OVER:0;
    } else {
        close_queues(gobj);
    }

    ws_close(gobj, reason_code); // this send drop

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
        {EV_SEND_MESSAGE,       ac_send_message,                    0},
        {EV_MQTT_PUBLISH,       ac_mqtt_client_send_publish,        0},
        {EV_MQTT_SUBSCRIBE,     ac_mqtt_client_send_subscribe,      0},
        {EV_MQTT_UNSUBSCRIBE,   ac_mqtt_client_send_unsubscribe,    0},

        {EV_DISCONNECTED,       ac_disconnected,                    ST_DISCONNECTED},
        {EV_TIMEOUT,            ac_timeout_waiting_frame_header,    0},
        {EV_TIMEOUT_PERIODIC,   ac_timeout_periodic,                0},
        {EV_DROP,               ac_drop,                            0},
        {EV_TX_READY,           0,                                  0},
        {0,0,0}
    };
    ev_action_t st_wait_payload[] = {
        {EV_RX_DATA,            ac_process_payload_data,            0},
        {EV_SEND_MESSAGE,       ac_send_message,                    0},
        {EV_MQTT_PUBLISH,       ac_mqtt_client_send_publish,        0},
        {EV_MQTT_SUBSCRIBE,     ac_mqtt_client_send_subscribe,      0},
        {EV_MQTT_UNSUBSCRIBE,   ac_mqtt_client_send_unsubscribe,    0},

        {EV_DISCONNECTED,       ac_disconnected,                    ST_DISCONNECTED},
        {EV_TIMEOUT,            ac_timeout_waiting_payload_data,    0},
        {EV_TIMEOUT_PERIODIC,   ac_timeout_periodic,                0},
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
        {EV_SEND_MESSAGE,       0},
        {EV_MQTT_PUBLISH,       0},
        {EV_MQTT_SUBSCRIBE,     0},
        {EV_MQTT_UNSUBSCRIBE,   0},
        {EV_ON_IEV_MESSAGE,     EVF_OUTPUT_EVENT},
        {EV_TIMEOUT,            0},
        {EV_TIMEOUT_PERIODIC,   0},
        {EV_TX_READY,           0},
        {EV_CONNECTED,          0},
        {EV_DISCONNECTED,       0},
        {EV_STOPPED,            0},
        {EV_DROP,               0},

        {EV_ON_OPEN,            EVF_OUTPUT_EVENT},
        {EV_ON_CLOSE,           EVF_OUTPUT_EVENT},
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
