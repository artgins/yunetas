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
#define WITH_BROKER 1

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
PUBLIC const char *mosquitto_connack_string(int connack_code)
{
    switch(connack_code) {
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
 *
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
 *
 ***************************************************************************/
PUBLIC int mosquitto_sub_topic_tokenise(
    const char *subtopic,
    char ***topics,
    int *count
) {
    size_t len;
    size_t hier_count = 1;
    size_t start, stop;
    size_t hier;
    size_t tlen;
    size_t i, j;

    if(!subtopic || !topics || !count) {
        return -1;
    }

    len = strlen(subtopic);

    for(i=0; i<len; i++) {
        if(subtopic[i] == '/') {
            if(i > len-1) {
                /* Separator at end of line */
            }else{
                hier_count++;
            }
        }
    }

    (*topics) = gbmem_calloc(hier_count, sizeof(char *));
    if(!(*topics)) {
        return -1;
    }

    start = 0;
    hier = 0;

    for(i=0; i<len+1; i++) {
        if(subtopic[i] == '/' || subtopic[i] == '\0') {
            stop = i;
            if(start != stop) {
                tlen = stop-start + 1;
                (*topics)[hier] = gbmem_calloc(tlen, sizeof(char));
                if(!(*topics)[hier]) {
                    for(j=0; j<hier; j++) {
                        gbmem_free((*topics)[j]);
                    }
                    gbmem_free((*topics));
                    return -1;
                }
                for(j=start; j<stop; j++) {
                    (*topics)[hier][j-start] = subtopic[j];
                }
            }
            start = i+1;
            hier++;
        }
    }

    *count = (int)hier_count;

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int mosquitto_sub_topic_tokens_free(char ***topics, int count)
{
    int i;

    if(!topics || !(*topics) || count<1) {
        return -1;
    }

    for(i=0; i<count; i++) {
        gbmem_free((*topics)[i]);
    }
    gbmem_free(*topics);

    return 0;
}

/***************************************************************************
 * Check that a topic used for publishing is valid.
 * Search for + or # in a topic. Return -1 if found.
 * Also returns -1 if the topic string is too long.
 * Returns 0 if everything is fine.
 ***************************************************************************/
PUBLIC int mosquitto_pub_topic_check(const char *str)
{
    int len = 0;
#ifdef WITH_BROKER
    int hier_count = 0;
#endif

    if(str == NULL) {
		return -1;
    }

    while(str && str[0]) {
		if(str[0] == '+' || str[0] == '#') {
			return -1;
		}
#ifdef WITH_BROKER
		else if(str[0] == '/') {
			hier_count++;
		}
#endif
		len++;
		str = &str[1];
    }
    if(len == 0 || len > 65535) return -1;
#ifdef WITH_BROKER
    if(hier_count > TOPIC_HIERARCHY_LIMIT) return -1;
#endif

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int mosquitto_pub_topic_check2(const char *str, size_t len)
{
    size_t i;
#ifdef WITH_BROKER
    int hier_count = 0;
#endif

    if(str == NULL || len == 0 || len > 65535) {
		return -1;
    }

    for(i=0; i<len; i++) {
		if(str[i] == '+' || str[i] == '#') {
			return -1;
		}
#ifdef WITH_BROKER
		else if(str[i] == '/') {
			hier_count++;
		}
#endif
    }
#ifdef WITH_BROKER
    if(hier_count > TOPIC_HIERARCHY_LIMIT) return -1;
#endif

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
#ifdef WITH_BROKER
    int hier_count = 0;
#endif

    if(str == NULL) {
		return -1;
    }

    while(str[0]) {
		if(str[0] == '+') {
			if((c != '\0' && c != '/') || (str[1] != '\0' && str[1] != '/')){
				return -1;
			}
		}else if(str[0] == '#') {
			if((c != '\0' && c != '/')  || str[1] != '\0') {
				return -1;
			}
		}
#ifdef WITH_BROKER
		else if(str[0] == '/') {
			hier_count++;
		}
#endif
		len++;
		c = str[0];
		str = &str[1];
    }
    if(len == 0 || len > 65535) return -1;
#ifdef WITH_BROKER
    if(hier_count > TOPIC_HIERARCHY_LIMIT) return -1;
#endif

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int mosquitto_sub_topic_check2(const char *str, size_t len)
{
    char c = '\0';
    size_t i;
#ifdef WITH_BROKER
    int hier_count = 0;
#endif

    if(str == NULL || len == 0 || len > 65535) {
		return -1;
    }

    for(i=0; i<len; i++) {
		if(str[i] == '+') {
			if((c != '\0' && c != '/') || (i<len-1 && str[i+1] != '/')){
				return -1;
			}
		}else if(str[i] == '#') {
			if((c != '\0' && c != '/')  || i<len-1) {
				return -1;
			}
		}
#ifdef WITH_BROKER
		else if(str[i] == '/') {
			hier_count++;
		}
#endif
		c = str[i];
    }
#ifdef WITH_BROKER
    if(hier_count > TOPIC_HIERARCHY_LIMIT) return -1;
#endif

    return 0;
}

/***************************************************************************
 *  Does a topic match a subscription?
 ***************************************************************************/
PUBLIC int mosquitto_topic_matches_sub(const char *sub, const char *topic, BOOL *result)
{
    size_t spos;

    if(!result) return -1;
    *result = FALSE;

    if(!sub || !topic || sub[0] == 0 || topic[0] == 0) {
		return -1;
    }

    if((sub[0] == '$' && topic[0] != '$')
			|| (topic[0] == '$' && sub[0] != '$')){

		return 0;
    }

    spos = 0;

    while(sub[0] != 0) {
		if(topic[0] == '+' || topic[0] == '#') {
			return -1;
		}
		if(sub[0] != topic[0] || topic[0] == 0) { /* Check for wildcard matches */
			if(sub[0] == '+') {
				/* Check for bad "+foo" or "a/+foo" subscription */
				if(spos > 0 && sub[-1] != '/') {
					return -1;
				}
				/* Check for bad "foo+" or "foo+/a" subscription */
				if(sub[1] != 0 && sub[1] != '/') {
					return -1;
				}
				spos++;
				sub++;
				while(topic[0] != 0 && topic[0] != '/') {
					if(topic[0] == '+' || topic[0] == '#') {
						return -1;
					}
					topic++;
				}
				if(topic[0] == 0 && sub[0] == 0) {
					*result = TRUE;
					return 0;
				}
			}else if(sub[0] == '#') {
				/* Check for bad "foo#" subscription */
				if(spos > 0 && sub[-1] != '/') {
					return -1;
				}
				/* Check for # not the final character of the sub, e.g. "#foo" */
				if(sub[1] != 0) {
					return -1;
				}else{
					while(topic[0] != 0) {
						if(topic[0] == '+' || topic[0] == '#') {
							return -1;
						}
						topic++;
					}
					*result = TRUE;
					return 0;
				}
			}else{
				/* Check for e.g. foo/bar matching foo/+/# */
				if(topic[0] == 0
						&& spos > 0
						&& sub[-1] == '+'
						&& sub[0] == '/'
						&& sub[1] == '#')
				{
					*result = TRUE;
					return 0;
				}

				/* There is no match at this point, but is the sub invalid? */
				while(sub[0] != 0) {
					if(sub[0] == '#' && sub[1] != 0) {
						return -1;
					}
					spos++;
					sub++;
				}

				/* Valid input, but no match */
				return 0;
			}
		}else{
			/* sub[spos] == topic[tpos] */
			if(topic[1] == 0) {
				/* Check for e.g. foo matching foo/# */
				if(sub[1] == '/'
						&& sub[2] == '#'
						&& sub[3] == 0) {
					*result = TRUE;
					return 0;
				}
			}
			spos++;
			sub++;
			topic++;
			if(sub[0] == 0 && topic[0] == 0) {
				*result = TRUE;
				return 0;
			}else if(topic[0] == 0 && sub[0] == '+' && sub[1] == 0) {
				if(spos > 0 && sub[-1] != '/') {
					return -1;
				}
				spos++;
				sub++;
				*result = TRUE;
				return 0;
			}
		}
    }
    if((topic[0] != 0 || sub[0] != 0)){
		*result = FALSE;
    }
    while(topic[0] != 0) {
		if(topic[0] == '+' || topic[0] == '#') {
			return -1;
		}
		topic++;
    }

    return 0;
}

/***************************************************************************
 *  Does a topic match a subscription?
 ***************************************************************************/
PUBLIC int mosquitto_topic_matches_sub2(const char *sub, size_t sublen, const char *topic, size_t topiclen, BOOL *result)
{
    size_t spos, tpos;

    if(!result) return -1;
    *result = FALSE;

    if(!sub || !topic || !sublen || !topiclen) {
		return -1;
    }

    if((sub[0] == '$' && topic[0] != '$')
			|| (topic[0] == '$' && sub[0] != '$')){

		return 0;
    }

    spos = 0;
    tpos = 0;

    while(spos < sublen) {
		if(tpos < topiclen && (topic[tpos] == '+' || topic[tpos] == '#')){
			return -1;
		}
		if(tpos == topiclen || sub[spos] != topic[tpos]) {
			if(sub[spos] == '+') {
				/* Check for bad "+foo" or "a/+foo" subscription */
				if(spos > 0 && sub[spos-1] != '/') {
					return -1;
				}
				/* Check for bad "foo+" or "foo+/a" subscription */
				if(spos+1 < sublen && sub[spos+1] != '/') {
					return -1;
				}
				spos++;
				while(tpos < topiclen && topic[tpos] != '/') {
					if(topic[tpos] == '+' || topic[tpos] == '#') {
						return -1;
					}
					tpos++;
				}
				if(tpos == topiclen && spos == sublen) {
					*result = TRUE;
					return 0;
				}
			}else if(sub[spos] == '#') {
				/* Check for bad "foo#" subscription */
				if(spos > 0 && sub[spos-1] != '/') {
					return -1;
				}
				/* Check for # not the final character of the sub, e.g. "#foo" */
				if(spos+1 < sublen) {
					return -1;
				}else{
					while(tpos < topiclen) {
						if(topic[tpos] == '+' || topic[tpos] == '#') {
							return -1;
						}
						tpos++;
					}
					*result = TRUE;
					return 0;
				}
			}else{
				/* Check for e.g. foo/bar matching foo/+/# */
				if(tpos == topiclen
						&& spos > 0
						&& sub[spos-1] == '+'
						&& sub[spos] == '/'
						&& spos+1 < sublen
						&& sub[spos+1] == '#')
				{
					*result = TRUE;
					return 0;
				}

				/* There is no match at this point, but is the sub invalid? */
				while(spos < sublen) {
					if(sub[spos] == '#' && spos+1 < sublen) {
						return -1;
					}
					spos++;
				}

				/* Valid input, but no match */
				return 0;
			}
		}else{
			/* sub[spos] == topic[tpos] */
			if(tpos+1 == topiclen) {
				/* Check for e.g. foo matching foo/# */
				if(spos+3 == sublen
						&& sub[spos+1] == '/'
						&& sub[spos+2] == '#') {
					*result = TRUE;
					return 0;
				}
			}
			spos++;
			tpos++;
			if(spos == sublen && tpos == topiclen) {
				*result = TRUE;
				return 0;
			}else if(tpos == topiclen && sub[spos] == '+' && spos+1 == sublen) {
				if(spos > 0 && sub[spos-1] != '/') {
					return -1;
				}
				spos++;
				*result = TRUE;
				return 0;
			}
		}
    }
    if(tpos < topiclen || spos < sublen) {
		*result = FALSE;
    }
    while(tpos < topiclen) {
		if(topic[tpos] == '+' || topic[tpos] == '#') {
			return -1;
		}
		tpos++;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int mosquitto_validate_utf8(const char *str, int len)
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
