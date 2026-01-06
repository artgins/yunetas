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
PUBLIC void gobj_run_services(void); /* Start/Play services and yuno, by priority */
PUBLIC void gobj_stop_services(void); /* Pause/Stop services and yuno, by priority */

PUBLIC int gobj_autostart_services(void);
PUBLIC int gobj_autoplay_services(void);
PUBLIC int gobj_stop_autostart_services(void);
PUBLIC int gobj_pause_autoplay_services(void);


#ifdef __cplusplus
}
#endif
