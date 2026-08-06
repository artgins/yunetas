/****************************************************************************
 *          c_log_reader.h
 *          Log_reader GClass.
 *
 *          Read a text file and publish its lines
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
GOBJ_DECLARE_GCLASS(C_LOG_READER);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_LOG_LINES);       // {path, lines:[]} a batch of complete lines
GOBJ_DECLARE_EVENT(EV_LOG_EOF);         // {path, lines, bytes} the file is done
GOBJ_DECLARE_EVENT(EV_LOG_ERROR);       // {path, error} the file was not read

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_log_reader(void);

#ifdef __cplusplus
}
#endif
