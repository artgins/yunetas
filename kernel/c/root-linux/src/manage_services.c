/***********************************************************************
 *          MANAGE_SERVICES.C
 *
 *          Manage services
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <yev_loop.h>
#include "c_yuno.h"
#include "manage_services.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
 *  Start/Play services and yuno
 ***************************************************************************/
PUBLIC void gobj_run_services(void)
{
    hgobj yuno = gobj_yuno();

    /*------------------------*
     *      Start main
     *------------------------*/
    gobj_start(yuno);

    /*---------------------------------*
     *      Auto services
     *---------------------------------*/
    gobj_autostart_services();
    if(gobj_read_bool_attr(yuno, "autoplay")) {
        gobj_play(yuno);    /* will play default_service */
    }
    gobj_autoplay_services();
}

/***************************************************************************
 *  Pause/Stop services and yuno
 ***************************************************************************/
PUBLIC void gobj_stop_services(void)
{
    /*
     *  Shutdown
     */
    gobj_set_shutdown();

    hgobj yuno = gobj_yuno();
    if(yuno) { // TODO && !(yuno->obflag & obflag_destroying)) {
        if(gobj_is_playing(yuno)) {
            // It will pause default_service, WARNING too slow for big configurations!!
            gobj_pause(yuno);
        }

        gobj_pause_autoplay_services();
        gobj_stop_autostart_services();
        gobj_stop(yuno);
    }

    yev_loop_run_once(yuno_event_loop());  // Give an opportunity to close
    yev_loop_stop(yuno_event_loop());
    yev_loop_run_once(yuno_event_loop());  // Give an opportunity to close

    // yev_loop_run_once(yuno_event_loop());  // Give an opportunity to close
    // yev_loop_stop(yuno_event_loop());
    // yev_loop_run_once(yuno_event_loop());  // Give an opportunity to close


}
