/****************************************************************************
 *          TR2Q_MQTT.C
 *
 *          Defines and functions for tr2_queue md2_record_ex_t.user_flag
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "tr2q_mqtt.h"

/********************************************************************
 *  Debug/String Functions
 ********************************************************************/

const char *user_flag_state_to_str(mosquitto_msg_state_t state)
{
    switch (state) {
        case mosq_ms_invalid:
            return "INVALID";
        case mosq_ms_publish_qos0:
            return "PUBLISH_QOS0";
        case mosq_ms_publish_qos1:
            return "PUBLISH_QOS1";
        case mosq_ms_wait_for_puback:
            return "WAIT_PUBACK";
        case mosq_ms_publish_qos2:
            return "PUBLISH_QOS2";
        case mosq_ms_wait_for_pubrec:
            return "WAIT_PUBREC";
        case mosq_ms_resend_pubrel:
            return "RESEND_PUBREL";
        case mosq_ms_wait_for_pubrel:
            return "WAIT_PUBREL";
        case mosq_ms_resend_pubcomp:
            return "RESEND_PUBCOMP";
        case mosq_ms_wait_for_pubcomp:
            return "WAIT_PUBCOMP";
        case mosq_ms_send_pubrec:
            return "SEND_PUBREC";
        case mosq_ms_queued:
            return "QUEUED";
        default:
            return "UNKNOWN";
    }
}

const char *user_flag_direction_to_str(mosquitto_msg_direction_t dir)
{
    switch (dir) {
        case mosq_md_in:
            return "IN";
        case mosq_md_out:
            return "OUT";
        default:
            return "NONE";
    }
}

const char *user_flag_origin_to_str(mosquitto_msg_origin_t orig)
{
    switch (orig) {
        case mosq_mo_client:
            return "CLIENT";
        case mosq_mo_broker:
            return "BROKER";
        default:
            return "NONE";
    }
}
