/****************************************************************************
 *          c_archive_sender.h
 *          Archive_sender GClass.
 *
 *          Ship the batches of a timeranger2 archive to a backup, and
 *          remove them
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_ARCHIVE_SENDER);

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DECLARE_STATE(ST_SHIPPING);
GOBJ_DECLARE_STATE(ST_RETRY_WAIT);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_RSYNC_EXITED);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_archive_sender(void);

#ifdef __cplusplus
}
#endif
