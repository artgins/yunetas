/****************************************************************************
 *          g_st_kernel.h
 *
 *          Global States of Kernel
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "gobj.h"

#ifdef __cplusplus
extern "C"{
#endif

GOBJ_DECLARE_STATE(ST_DISCONNECTED);
GOBJ_DECLARE_STATE(ST_WAIT_CONNECTED);
GOBJ_DECLARE_STATE(ST_CONNECTED);
GOBJ_DECLARE_STATE(ST_WAIT_TXED);
GOBJ_DECLARE_STATE(ST_WAIT_DISCONNECTED);
GOBJ_DECLARE_STATE(ST_WAIT_STOPPED);
GOBJ_DECLARE_STATE(ST_WAIT_HANDSHAKE);
GOBJ_DECLARE_STATE(ST_WAIT_FRAME_HEADER);
GOBJ_DECLARE_STATE(ST_WAIT_PAYLOAD);
GOBJ_DECLARE_STATE(ST_WAIT_IMEI);
GOBJ_DECLARE_STATE(ST_WAIT_ID);
GOBJ_DECLARE_STATE(ST_STOPPED);
GOBJ_DECLARE_STATE(ST_SESSION);
GOBJ_DECLARE_STATE(ST_IDLE);
GOBJ_DECLARE_STATE(ST_WAIT_RESPONSE);
GOBJ_DECLARE_STATE(ST_OPENED);
GOBJ_DECLARE_STATE(ST_CLOSED);
GOBJ_DECLARE_STATE(ST_DISABLED);


#ifdef __cplusplus
}
#endif
