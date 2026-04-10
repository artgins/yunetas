/****************************************************************************
 *          C_AUTH_BFF_YUNO.H
 *          auth_bff yuno top-level GClass.
 *
 *          Top-level orchestrator for the auth_bff yuno.  It is the
 *          default service of the yuno; on play/pause from the agent
 *          it starts/stops the __bff_side__ tree provided by the
 *          batch configuration.
 *
 *          NOTE: this is *not* the kernel C_AUTH_BFF GClass — that
 *          one (kernel/c/root-linux/src/c_auth_bff.c) is meant to be
 *          a per-channel processor and lives as a child of C_CHANNEL
 *          inside __bff_side__.
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
GOBJ_DECLARE_GCLASS(C_AUTH_BFF_YUNO);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_auth_bff_yuno(void);

#ifdef __cplusplus
}
#endif
