/***********************************************************************
 *          MQTT_UTIL.C
 *
 *          MQTT utilities
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
#include "mqtt_util.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
 *
 ***************************************************************************/


/***************************************************************************
 *  get_name_from_nn_table(mqtt311_connack_codes_s, connack_code) is slower
 ***************************************************************************/
const char *mosquitto_connack_string(int connack_code)
{
    switch(connack_code){
        case 0:
            return "Connection Accepted.";
        case 1:
            return "Connection Refused: unacceptable protocol version.";
        case 2:
            return "Connection Refused: identifier rejected.";
        case 3:
            return "Connection Refused: broker unavailable.";
        case 4:
            return "Connection Refused: bad user name or password.";
        case 5:
            return "Connection Refused: not authorised.";
        default:
            return "Connection Refused: unknown reason.";
    }
}

/***************************************************************************
 *  get_name_from_nn_table(mqtt5_return_codes_s) is a slower alternative
 ***************************************************************************/
PUBLIC const char *mosquitto_reason_string(int reason_code)
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
 * Check that a topic used for publishing is valid.
 * Search for + or # in a topic. Return -1 if found.
 * Also returns -1 if the topic string is too long.
 * Returns 0 if everything is fine.
 ***************************************************************************/
PUBLIC int mosquitto_pub_topic_check(const char *topic)
{
    int len = 0;
    int hier_count = 0;

    if(topic == NULL) {
        return -1;
    }
    const char *str = topic;
    while(str && *str) {
        if(*str == '+' || *str == '#') {
            return -1;
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
 * Return -1 if invalid position found.
 * Also returns -1 if the topic string is too long.
 * Returns 0 if everything is fine.
 ***************************************************************************/
PUBLIC int mosquitto_sub_topic_check(const char *str)
{
    char c = '\0';
    int len = 0;
    int hier_count = 0;

    if(str == NULL) {
        return -1;
    }

    while(str[0]) {
        if(str[0] == '+') {
            if((c != '\0' && c != '/') || (str[1] != '\0' && str[1] != '/')) {
                return -1;
            }
        } else if(str[0] == '#') {
            if((c != '\0' && c != '/')  || str[1] != '\0') {
                return -1;
            }
        } else if(str[0] == '/') {
            hier_count++;
        }
        len++;
        c = str[0];
        str = &str[1];
    }
    if(len > 65535) {
        return -1;
    }
    if(hier_count > TOPIC_HIERARCHY_LIMIT) {
        return -1;
    }

    return 0;
}
