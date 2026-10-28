/****************************************************************************
 *          c_archive_keeper.h
 *          Archive_keeper GClass.
 *
 *          Keep the batches of timeranger2 archives that archive_senders
 *          ship
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
GOBJ_DECLARE_GCLASS(C_ARCHIVE_KEEPER);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_archive_keeper(void);

#ifdef __cplusplus
}
#endif
