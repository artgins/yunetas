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
PRIVATE int autostart_services(json_t *top_services);
PRIVATE int autoplay_services(json_t *top_services);
PRIVATE int stop_autostart_services(json_t *top_services);
PRIVATE int pause_autoplay_services(json_t *top_services);

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
 *  Start/Play services and yuno
 ***************************************************************************/
PUBLIC void run_services(void)
{
    hgobj yuno = gobj_yuno();

    json_t *top_services = gobj_top_services(); /* Get list of top services {gobj, priority} */

    /*------------------------*
     *      Start main
     *------------------------*/
    gobj_start(yuno);

    /*---------------------------------*
     *      Auto services
     *---------------------------------*/
    autostart_services(top_services);
    if(gobj_read_bool_attr(yuno, "autoplay")) {
        gobj_play(yuno);    /* will play default_service */
    }
    autoplay_services(top_services);

    json_decref(top_services);
}

/***************************************************************************
 *  Pause/Stop services and yuno
 ***************************************************************************/
PUBLIC void stop_services(void)
{
    /*
     *  Shutdown
     */
    gobj_set_shutdown();

    json_t *top_services = gobj_top_services(); /* Get list of top services {gobj, priority} */

    hgobj yuno = gobj_yuno();
    if(yuno && !gobj_is_destroying(yuno)) {
        if(gobj_is_playing(yuno)) {
            // It will pause default_service, WARNING too slow for big configurations!!
            gobj_pause(yuno);
        }

        pause_autoplay_services(top_services);
        stop_autostart_services(top_services);
        gobj_stop(yuno);
    }

    yev_loop_stop(yuno_event_loop());
    yev_loop_run_once(yuno_event_loop());  // Give an opportunity to close
}

/***************************************************************************
 *  If service has mt_play then start only the service gobj.
 *      (Let mt_play be responsible to start their tree)
 *  If service has not mt_play then start the tree with gobj_start_tree().
 ***************************************************************************/
PRIVATE int autostart_services(json_t *top_services)
{
    for(int priority=0; priority<9; priority++) {
        int idx; json_t *srv;
        json_array_foreach(top_services, idx, srv) {
            int priority_ = (int)kw_get_int(0, srv, "priority", 9, KW_REQUIRED);
            if(priority == priority_) {
                hgobj gobj = (hgobj)kw_get_int(0, srv, "gobj", 0, KW_REQUIRED);
                gobj_flag_t gobj_flag = kw_get_int(0, srv, "gobj_flag", 0, KW_REQUIRED);
                if(gobj_flag & gobj_flag_autostart) {
                    // HACK checking mt_play because if it exists he has the power on!
                    gobj_log_debug(0,0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_STARTUP,
                        "msg",          "%s", "START service",
                        "service",      "%s", gobj_short_name(gobj),
                        NULL
                    );
                    if(gclass_with_mt_play(gobj_gclass(gobj))) {
                        if(!gobj_is_running(gobj)) {
                            gobj_start(gobj);
                        }
                    } else {
                        gobj_start_tree(gobj);
                    }
                }
            }
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int autoplay_services(json_t *top_services)
{
    for(int priority=0; priority<9; priority++) {
        int idx; json_t *srv;
        json_array_foreach(top_services, idx, srv) {
            int priority_ = (int)kw_get_int(0, srv, "priority", 9, KW_REQUIRED);
            if(priority == priority_) {
                hgobj gobj = (hgobj)kw_get_int(0, srv, "gobj", 0, KW_REQUIRED);
                gobj_flag_t gobj_flag = kw_get_int(0, srv, "gobj_flag", 0, KW_REQUIRED);
                if(gobj_flag & gobj_flag_autoplay) {
                    if(!gobj_is_playing(gobj)) {
                        if(gobj_is_level_tracing(0, TRACE_START_STOP)) {
                            gobj_log_debug(0,0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_STARTUP,
                                "msg",          "%s", "PLAY service",
                                "service",      "%s", gobj_short_name(gobj),
                                NULL
                            );
                        }
                        gobj_play(gobj);
                    }
                }
            }
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int pause_autoplay_services(json_t *top_services)
{
    /*
     *  Pause backward priority order
     */
    for(int priority=9; priority>=0; priority--) {
        int idx; json_t *srv;
        json_array_foreach(top_services, idx, srv) {
            int priority_ = (int)kw_get_int(0, srv, "priority", 9, KW_REQUIRED);
            if(priority == priority_) {
                hgobj gobj = (hgobj)kw_get_int(0, srv, "gobj", 0, KW_REQUIRED);
                gobj_flag_t gobj_flag = kw_get_int(0, srv, "gobj_flag", 0, KW_REQUIRED);
                if(gobj_flag & gobj_flag_autoplay) {
                    if(gobj_is_playing(gobj)) {
                        if(gobj_is_level_tracing(0, TRACE_START_STOP)) {
                            gobj_log_debug(0,0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_STARTUP,
                                "msg",          "%s", "PAUSE service",
                                "service",      "%s", gobj_short_name(gobj),
                                NULL
                            );
                        }
                        gobj_pause(gobj);
                        yev_loop_run_once(yuno_event_loop());
                    }
                }
            }
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int stop_autostart_services(json_t *top_services)
{
    /*
     *  Stop backward priority order
     */
    for(int priority=9; priority>=0; priority--) {
        int idx; json_t *srv;
        json_array_foreach(top_services, idx, srv) {
            int priority_ = (int)kw_get_int(0, srv, "priority", 9, KW_REQUIRED);
            if(priority == priority_) {
                hgobj gobj = (hgobj)kw_get_int(0, srv, "gobj", 0, KW_REQUIRED);
                gobj_flag_t gobj_flag = kw_get_int(0, srv, "gobj_flag", 0, KW_REQUIRED);
                if(gobj_flag & gobj_flag_autostart) {
                    if(gobj_is_running(gobj)) {
                        if(gobj_is_level_tracing(0, TRACE_START_STOP)) {
                            gobj_log_debug(0,0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_STARTUP,
                                "msg",          "%s", "STOP service",
                                "service",      "%s", gobj_short_name(gobj),
                                NULL
                            );
                        }
                        gobj_stop(gobj);
                        yev_loop_run_once(yuno_event_loop());
                    }
                }
            }
        }
    }

    return 0;
}
