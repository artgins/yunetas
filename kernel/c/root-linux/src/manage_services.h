/****************************************************************************
 *          MANAGE_SERVICES.H
 *
 *          Manage services
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/

#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void run_services(void); /* Start/Play services and yuno, by priority */
PUBLIC void stop_services(void); /* Pause/Stop services and yuno, by priority */
PUBLIC void yuno_shutdown(void); /* Shutdown yuno, mark gobj as shutdown and exit of event loop */

#ifdef __cplusplus
}
#endif
