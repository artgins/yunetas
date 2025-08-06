/****************************************************************************
 *          g_events.h
 *
 *          Global Events
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "gobj.h"

#ifdef __cplusplus
extern "C"{
#endif

// Timer Events, defined here to easy filtering in trace
GOBJ_DECLARE_EVENT(EV_TIMEOUT);
GOBJ_DECLARE_EVENT(EV_TIMEOUT_PERIODIC);

// System Events
GOBJ_DECLARE_EVENT(EV_STATE_CHANGED);

// Channel Messages: Input Events (Requests)
GOBJ_DECLARE_EVENT(EV_SEND_MESSAGE);
GOBJ_DECLARE_EVENT(EV_SEND_IEV);
GOBJ_DECLARE_EVENT(EV_SEND_EMAIL);
GOBJ_DECLARE_EVENT(EV_DROP);
GOBJ_DECLARE_EVENT(EV_REMOTE_LOG);

// Channel Messages: Output Events (Publications)
GOBJ_DECLARE_EVENT(EV_ON_OPEN);
GOBJ_DECLARE_EVENT(EV_ON_CLOSE);
GOBJ_DECLARE_EVENT(EV_ON_MESSAGE);      // with GBuffer
GOBJ_DECLARE_EVENT(EV_ON_COMMAND);
GOBJ_DECLARE_EVENT(EV_ON_IEV_MESSAGE);  // with IEvent
GOBJ_DECLARE_EVENT(EV_ON_ID);
GOBJ_DECLARE_EVENT(EV_ON_ID_NAK);
GOBJ_DECLARE_EVENT(EV_ON_HEADER);

GOBJ_DECLARE_EVENT(EV_HTTP_MESSAGE);

// Tube Messages
GOBJ_DECLARE_EVENT(EV_CONNECT);
GOBJ_DECLARE_EVENT(EV_CONNECTED);
GOBJ_DECLARE_EVENT(EV_DISCONNECT);
GOBJ_DECLARE_EVENT(EV_DISCONNECTED);
GOBJ_DECLARE_EVENT(EV_RX_DATA);
GOBJ_DECLARE_EVENT(EV_TX_DATA);
GOBJ_DECLARE_EVENT(EV_TX_READY);
GOBJ_DECLARE_EVENT(EV_STOPPED);

// CLI Messages
GOBJ_DECLARE_EVENT(EV_COMMAND);

#ifdef __cplusplus
}
#endif
