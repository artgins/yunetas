/****************************************************************************
 *          MQTT_UTIL.H
 *
 *          Mqtt utilities
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/

#pragma once

#include <helpers.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
#define PROTOCOL_NAME_v31 "MQIsdp"
#define PROTOCOL_VERSION_v31 3
#define PROTOCOL_NAME "MQTT"
#define PROTOCOL_VERSION_v311 4
#define PROTOCOL_VERSION_v5 5

/* Message types */
typedef enum { // string names in command_name[]
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

typedef enum mosquitto_protocol {
    mosq_p_invalid = 0,
    mosq_p_mqtt31 = PROTOCOL_VERSION_v31,
    mosq_p_mqtt311 = PROTOCOL_VERSION_v311,
    mosq_p_mqtt5 = PROTOCOL_VERSION_v5,
} mosquitto_protocol_t ;

/*
 * Enum: mqtt311_connack_codes
 * The CONNACK results for MQTT v3.1.1, and v3.1.
 */
typedef enum mqtt311_connack_codes { // string names in mqtt311_connack_codes_s[]
    CONNACK_ACCEPTED = 0,
    CONNACK_REFUSED_PROTOCOL_VERSION = 1,
    CONNACK_REFUSED_IDENTIFIER_REJECTED = 2,
    CONNACK_REFUSED_SERVER_UNAVAILABLE = 3,
    CONNACK_REFUSED_BAD_USERNAME_PASSWORD = 4,
    CONNACK_REFUSED_NOT_AUTHORIZED = 5,
} mqtt311_connack_codes_t;

/*
 * Enum: mqtt5_return_codes
 * The reason codes returned in various MQTT commands.
 */
typedef enum mqtt5_return_codes { // string names in mqtt5_return_codes_s[]
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
} mqtt5_return_codes_t;

/*
 * Enum: mqtt5_property
 * Options for use with MQTTv5 properties.
 */
typedef enum mqtt5_property {
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
} mqtt5_property_t;

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
typedef enum mqtt5_sub_options {
    MQTT_SUB_OPT_NO_LOCAL = 0x04,
    MQTT_SUB_OPT_RETAIN_AS_PUBLISHED = 0x08,
    MQTT_SUB_OPT_SEND_RETAIN_ALWAYS = 0x00,
    MQTT_SUB_OPT_SEND_RETAIN_NEW = 0x10,
    MQTT_SUB_OPT_SEND_RETAIN_NEVER = 0x20,
} mqtt5_sub_options_t;

/***************************************************************
 *              Prototypes
 ***************************************************************/

/*
 * Function: mosquitto_connack_string
 *
 * Call to obtain a const string description of an MQTT connection result.
 *
 * Parameters:
 *	connack_code - an MQTT connection result.
 *
 * Returns:
 *	A constant string describing the result.
 */
PUBLIC const char *mosquitto_connack_string(int connack_code);

/*
 * Function: mosquitto_reason_string
 *
 * Call to obtain a const string description of an MQTT reason code.
 *
 * Parameters:
 *	reason_code - an MQTT reason code.
 *
 * Returns:
 *	A constant string describing the reason.
 */
PUBLIC const char *mosquitto_reason_string(int reason_code);


/*
 * Function: mosquitto_sub_topic_tokenise
 *
 * Tokenise a topic or subscription string into an array of strings
 * representing the topic hierarchy.
 *
 * For example:
 *
 *    subtopic: "a/deep/topic/hierarchy"
 *
 *    Would result in:
 *
 *       topics[0] = "a"
 *       topics[1] = "deep"
 *       topics[2] = "topic"
 *       topics[3] = "hierarchy"
 *
 *    and:
 *
 *    subtopic: "/a/deep/topic/hierarchy/"
 *
 *    Would result in:
 *
 *       topics[0] = NULL
 *       topics[1] = "a"
 *       topics[2] = "deep"
 *       topics[3] = "topic"
 *       topics[4] = "hierarchy"
 *
 * Parameters:
 *	subtopic - the subscription/topic to tokenise
 *	topics -   a pointer to store the array of strings
 *	count -    an int pointer to store the number of items in the topics array.
 *
 * Returns:
 *	MOSQ_ERR_SUCCESS -        on success
 * 	MOSQ_ERR_NOMEM -          if an out of memory condition occurred.
 * 	MOSQ_ERR_MALFORMED_UTF8 - if the topic is not valid UTF-8
 *
 * Example:
 *
 * > char **topics;
 * > int topic_count;
 * > int i;
 * >
 * > mosquitto_sub_topic_tokenise("$SYS/broker/uptime", &topics, &topic_count);
 * >
 * > for(i=0; i<token_count; i++){
 * >     printf("%d: %s\n", i, topics[i]);
 * > }
 *
 * See Also:
 *	<mosquitto_sub_topic_tokens_free>
 */
PUBLIC int mosquitto_sub_topic_tokenise(const char *subtopic, char ***topics, int *count);

/*
 * Function: mosquitto_sub_topic_tokens_free
 *
 * Free memory that was allocated in <mosquitto_sub_topic_tokenise>.
 *
 * Parameters:
 *	topics - pointer to string array.
 *	count - count of items in string array.
 *
 * Returns:
 *	MOSQ_ERR_SUCCESS - on success
 * 	MOSQ_ERR_INVAL -   if the input parameters were invalid.
 *
 * See Also:
 *	<mosquitto_sub_topic_tokenise>
 */
int mosquitto_sub_topic_tokens_free(char ***topics, int count);

/*
 * Function: mosquitto_topic_matches_sub
 *
 * Check whether a topic matches a subscription.
 *
 * For example:
 *
 * foo/bar would match the subscription foo/# or +/bar
 * non/matching would not match the subscription non/+/+
 *
 * Parameters:
 *	sub - subscription string to check topic against.
 *	topic - topic to check.
 *	result - bool pointer to hold result. Will be set to true if the topic
 *	         matches the subscription.
 *
 * Returns:
 *	MOSQ_ERR_SUCCESS - on success
 * 	MOSQ_ERR_INVAL -   if the input parameters were invalid.
 * 	MOSQ_ERR_NOMEM -   if an out of memory condition occurred.
 */
PUBLIC int mosquitto_topic_matches_sub(const char *sub, const char *topic, BOOL *result);


/*
 * Function: mosquitto_topic_matches_sub2
 *
 * Check whether a topic matches a subscription.
 *
 * For example:
 *
 * foo/bar would match the subscription foo/# or +/bar
 * non/matching would not match the subscription non/+/+
 *
 * Parameters:
 *	sub - subscription string to check topic against.
 *	sublen - length in bytes of sub string
 *	topic - topic to check.
 *	topiclen - length in bytes of topic string
 *	result - bool pointer to hold result. Will be set to true if the topic
 *	         matches the subscription.
 *
 * Returns:
 *	MOSQ_ERR_SUCCESS - on success
 *	MOSQ_ERR_INVAL -   if the input parameters were invalid.
 *	MOSQ_ERR_NOMEM -   if an out of memory condition occurred.
 */
PUBLIC int mosquitto_topic_matches_sub2(const char *sub, size_t sublen, const char *topic, size_t topiclen, BOOL *result);

/*
 * Function: mosquitto_pub_topic_check
 *
 * Check whether a topic to be used for publishing is valid.
 *
 * This searches for + or # in a topic and checks its length.
 *
 * This check is already carried out in <mosquitto_publish> and
 * <mosquitto_will_set>, there is no need to call it directly before them. It
 * may be useful if you wish to check the validity of a topic in advance of
 * making a connection for example.
 *
 * Parameters:
 *   topic - the topic to check
 *
 * Returns:
 *   MOSQ_ERR_SUCCESS -        for a valid topic
 *   MOSQ_ERR_INVAL -          if the topic contains a + or a #, or if it is too long.
 *   MOSQ_ERR_MALFORMED_UTF8 - if topic is not valid UTF-8
 *
 * See Also:
 *   <mosquitto_sub_topic_check>
 */
PUBLIC int mosquitto_pub_topic_check(const char *topic);

/*
 * Function: mosquitto_pub_topic_check2
 *
 * Check whether a topic to be used for publishing is valid.
 *
 * This searches for + or # in a topic and checks its length.
 *
 * This check is already carried out in <mosquitto_publish> and
 * <mosquitto_will_set>, there is no need to call it directly before them. It
 * may be useful if you wish to check the validity of a topic in advance of
 * making a connection for example.
 *
 * Parameters:
 *   topic - the topic to check
 *   topiclen - length of the topic in bytes
 *
 * Returns:
 *   MOSQ_ERR_SUCCESS -        for a valid topic
 *   MOSQ_ERR_INVAL -          if the topic contains a + or a #, or if it is too long.
 *   MOSQ_ERR_MALFORMED_UTF8 - if topic is not valid UTF-8
 *
 * See Also:
 *   <mosquitto_sub_topic_check>
 */
PUBLIC int mosquitto_pub_topic_check2(const char *topic, size_t topiclen);

/*
 * Function: mosquitto_sub_topic_check
 *
 * Check whether a topic to be used for subscribing is valid.
 *
 * This searches for + or # in a topic and checks that they aren't in invalid
 * positions, such as with foo/#/bar, foo/+bar or foo/bar#, and checks its
 * length.
 *
 * This check is already carried out in <mosquitto_subscribe> and
 * <mosquitto_unsubscribe>, there is no need to call it directly before them.
 * It may be useful if you wish to check the validity of a topic in advance of
 * making a connection for example.
 *
 * Parameters:
 *   topic - the topic to check
 *
 * Returns:
 *   MOSQ_ERR_SUCCESS -        for a valid topic
 *   MOSQ_ERR_INVAL -          if the topic contains a + or a # that is in an
 *                             invalid position, or if it is too long.
 *   MOSQ_ERR_MALFORMED_UTF8 - if topic is not valid UTF-8
 *
 * See Also:
 *   <mosquitto_sub_topic_check>
 */
PUBLIC int mosquitto_sub_topic_check(const char *topic);

/*
 * Function: mosquitto_sub_topic_check2
 *
 * Check whether a topic to be used for subscribing is valid.
 *
 * This searches for + or # in a topic and checks that they aren't in invalid
 * positions, such as with foo/#/bar, foo/+bar or foo/bar#, and checks its
 * length.
 *
 * This check is already carried out in <mosquitto_subscribe> and
 * <mosquitto_unsubscribe>, there is no need to call it directly before them.
 * It may be useful if you wish to check the validity of a topic in advance of
 * making a connection for example.
 *
 * Parameters:
 *   topic - the topic to check
 *   topiclen - the length in bytes of the topic
 *
 * Returns:
 *   MOSQ_ERR_SUCCESS -        for a valid topic
 *   MOSQ_ERR_INVAL -          if the topic contains a + or a # that is in an
 *                             invalid position, or if it is too long.
 *   MOSQ_ERR_MALFORMED_UTF8 - if topic is not valid UTF-8
 *
 * See Also:
 *   <mosquitto_sub_topic_check>
 */
PUBLIC int mosquitto_sub_topic_check2(const char *topic, size_t topiclen);


/*
 * Function: mosquitto_validate_utf8
 *
 * Helper function to validate whether a UTF-8 string is valid, according to
 * the UTF-8 spec and the MQTT additions.
 *
 * Parameters:
 *   str - a string to check
 *   len - the length of the string in bytes
 *
 * Returns:
 *   MOSQ_ERR_SUCCESS -        on success
 *   MOSQ_ERR_INVAL -          if str is NULL or len<0 or len>65536
 *   MOSQ_ERR_MALFORMED_UTF8 - if str is not valid UTF-8
 */
PUBLIC int mosquitto_validate_utf8(const char *str, int len);


#ifdef __cplusplus
}
#endif
