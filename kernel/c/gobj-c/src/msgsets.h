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
 *  A MSGSET names the *topic* of a log entry. Severity is decided by the
 *  caller picking gobj_log_error / _warning / _info / _debug — never
 *  encoded in the msgset itself. logcenter counts by severity at the top
 *  level, then groups by msgset underneath, so a suffix like "_ERROR"
 *  would duplicate information already carried by the call site.
 */
#define MSGSET_PARAMETER                "Parameter"
#define MSGSET_MEMORY                   "No memory"
#define MSGSET_INTERNAL                 "Internal"
#define MSGSET_SYSTEM                   "OS"
#define MSGSET_JSON                     "Jansson"
#define MSGSET_LIBURING                 "Liburing"
#define MSGSET_AUTH                     "Auth"
#define MSGSET_TRANGER                  "Tranger"
#define MSGSET_TREEDB                   "TreeDb"
#define MSGSET_MSG2DB                   "Msg2Db"
#define MSGSET_QUEUE                    "Queue"
#define MSGSET_REGISTER                 "Register"
#define MSGSET_OPERATIONAL              "Operational"
#define MSGSET_SMACHINE                 "SMachine"
#define MSGSET_PROTOCOL                 "Protocol"
#define MSGSET_APP                      "App"
#define MSGSET_SERVICE                  "Service"
#define MSGSET_TASK                     "Task"
#define MSGSET_CONFIGURATION            "Configuration"
#define MSGSET_RUNTIME                  "Runtime"
#define MSGSET_MQTT                     "Mqtt"
#define MSGSET_POSTGRES                 "Postgres"
#define MSGSET_OPENSSL                  "OpenSSL"
#define MSGSET_MBEDTLS                  "MbedTLS"

#define MSGSET_STATISTICS               "Statistics"
#define MSGSET_STARTUP                  "Startup"
#define MSGSET_INFO                     "Info"
#define MSGSET_CONNECT_DISCONNECT       "Connect Disconnect"
#define MSGSET_DEBUG                    "Debug"
#define MSGSET_GBUFFERS                 "GBuffers"
#define MSGSET_YEV_LOOP                 "Yev_loop"
#define MSGSET_TRACK_MEM                "TrackMem"
#define MSGSET_MONITORING               "Monitoring"
#define MSGSET_PERSISTENT_IEVENTS       "Persistent IEvents"
#define MSGSET_OPEN_CLOSE               "Open Close"
#define MSGSET_DATABASE                 "Database"
#define MSGSET_CREATION_DELETION_GOBJS  "Creation Deletion GObjs"
#define MSGSET_SUBSCRIPTIONS            "Creation Deletion Subscriptions"
#define MSGSET_BAD_BEHAVIOUR            "Bad Behaviour"


#ifdef __cplusplus
}
#endif
