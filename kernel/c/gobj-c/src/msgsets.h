/****************************************************************************
 *              msgsets.h
 *
 *              Message sets for glogger
 *
 *              Copyright (c) 1996-2015 Niyamaka.
 *              Copyright (c) 2025, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#ifdef __cplusplus
extern "C"{
#endif

/*
 *  Msgid's
 */
#define MSGSET_PARAMETER_ERROR          "Parameter Error"
#define MSGSET_MEMORY_ERROR             "No memory"
#define MSGSET_INTERNAL_ERROR           "Internal Error"
#define MSGSET_SYSTEM_ERROR             "OS Error"
#define MSGSET_JSON_ERROR               "Jansson Error"
#define MSGSET_LIBURING_ERROR           "Liburing Error"
#define MSGSET_AUTH_ERROR               "Auth Error"
#define MSGSET_TRANGER_ERROR            "Tranger Error"
#define MSGSET_TREEDB_ERROR             "TreeDb Error"
#define MSGSET_MSG2DB_ERROR             "Msg2Db Error"
#define MSGSET_QUEUE_ALARM              "Queue Alarm"
#define MSGSET_REGISTER_ERROR           "Register error"
#define MSGSET_OPERATIONAL_ERROR        "Operational error"
#define MSGSET_SMACHINE_ERROR           "SMachine error"
#define MSGSET_PROTOCOL_ERROR           "Protocol error"
#define MSGSET_APP_ERROR                "App error"
#define MSGSET_SERVICE_ERROR            "Service error"
#define MSGSET_TASK_ERROR               "Task error"
#define MSGSET_CONFIGURATION_ERROR      "Configuration error"
#define MSGSET_RUNTIME_ERROR            "Runtime error"
#define MSGSET_MQTT_ERROR               "Mqtt error"
#define MSGSET_POSTGRES_ERROR           "Postgres error"

// Info/Debug MSGSETs
#define MSGSET_STATISTICS               "Statistics"
#define MSGSET_STARTUP                  "Startup"
#define MSGSET_INFO                     "Info"
#define MSGSET_CONNECT_DISCONNECT       "Connect Disconnect"
#define MSGSET_DEBUG                    "Debug"
#define MSGSET_PROTOCOL                 "Protocol"
#define MSGSET_GBUFFERS                 "GBuffers"
#define MSGSET_YEV_LOOP                 "Yev_loop"
#define MSGSET_TRACK_MEM                "TrackMem"
#define MSGSET_AUTH                     "Auth"
#define MSGSET_MONITORING               "Monitoring"

#define MSGSET_PERSISTENT_IEVENTS       "Persistent IEvents"
#define MSGSET_OPEN_CLOSE               "Open Close"
#define MSGSET_DATABASE                 "Database"
#define MSGSET_CREATION_DELETION_GOBJS  "Creation Deletion GObjs"
#define MSGSET_SUBSCRIPTIONS            "Creation Deletion Subscriptions"
#define MSGSET_BAD_BEHAVIOUR            "Bad Behaviour"
#define MSGSET_PROTOCOL_INFO            "Protocol info"
#define MSGSET_TASK_INFO                "Task info"
#define MSGSET_MQTT_INFO                "Mqtt info"


#ifdef __cplusplus
}
#endif
