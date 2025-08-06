/****************************************************************************
 *          g_events.c
 *
 *          Global Events
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "g_events.h"

// Timer Events, defined here to easy filtering in trace
GOBJ_DEFINE_EVENT(EV_TIMEOUT);
GOBJ_DEFINE_EVENT(EV_TIMEOUT_PERIODIC);

// System Events
GOBJ_DEFINE_EVENT(EV_STATE_CHANGED);

// Channel Messages: Input Events (Requests)
GOBJ_DEFINE_EVENT(EV_SEND_MESSAGE);
GOBJ_DEFINE_EVENT(EV_SEND_IEV);
GOBJ_DEFINE_EVENT(EV_SEND_EMAIL);
GOBJ_DEFINE_EVENT(EV_DROP);
GOBJ_DEFINE_EVENT(EV_REMOTE_LOG);

// Channel Messages: Output Events (Publications)
GOBJ_DEFINE_EVENT(EV_ON_OPEN);
GOBJ_DEFINE_EVENT(EV_ON_CLOSE);
GOBJ_DEFINE_EVENT(EV_ON_MESSAGE);      // with GBuffer
GOBJ_DEFINE_EVENT(EV_ON_COMMAND);
GOBJ_DEFINE_EVENT(EV_ON_IEV_MESSAGE);  // with IEvent, old EV_IEV_MESSAGE
GOBJ_DEFINE_EVENT(EV_ON_ID);
GOBJ_DEFINE_EVENT(EV_ON_ID_NAK);
GOBJ_DEFINE_EVENT(EV_ON_HEADER);

GOBJ_DEFINE_EVENT(EV_HTTP_MESSAGE);

// Tube Messages
GOBJ_DEFINE_EVENT(EV_CONNECT);
GOBJ_DEFINE_EVENT(EV_CONNECTED);
GOBJ_DEFINE_EVENT(EV_DISCONNECT);
GOBJ_DEFINE_EVENT(EV_DISCONNECTED);
GOBJ_DEFINE_EVENT(EV_RX_DATA);
GOBJ_DEFINE_EVENT(EV_TX_DATA);
GOBJ_DEFINE_EVENT(EV_TX_READY);
GOBJ_DEFINE_EVENT(EV_STOPPED);

// CLI Messages
GOBJ_DEFINE_EVENT(EV_COMMAND);
