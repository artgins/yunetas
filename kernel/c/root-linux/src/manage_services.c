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
PRIVATE int autostart_services(void);
PRIVATE int autoplay_services(void);
PRIVATE int stop_autostart_services(void);
PRIVATE int pause_autoplay_services(void);



/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
 *  Start/Play services and yuno
 ***************************************************************************/
PUBLIC void run_services(void)
{
    hgobj yuno = gobj_yuno();

    /*------------------------*
     *      Start main
     *------------------------*/
    gobj_start(yuno);

    /*---------------------------------*
     *      Auto services
     *---------------------------------*/
    autostart_services();
    if(gobj_read_bool_attr(yuno, "autoplay")) {
        gobj_play(yuno);    /* will play default_service */
    }
    autoplay_services();
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

    hgobj yuno = gobj_yuno();
    if(yuno) { // TODO && !(yuno->obflag & obflag_destroying)) {
        if(gobj_is_playing(yuno)) {
            // It will pause default_service, WARNING too slow for big configurations!!
            gobj_pause(yuno);
        }

        pause_autoplay_services();
        stop_autostart_services();
        gobj_stop(yuno);
    }

    yev_loop_run_once(yuno_event_loop());  // Give an opportunity to close
    yev_loop_stop(yuno_event_loop());
    yev_loop_run_once(yuno_event_loop());  // Give an opportunity to close

    // yev_loop_run_once(yuno_event_loop());  // Give an opportunity to close
    // yev_loop_stop(yuno_event_loop());
    // yev_loop_run_once(yuno_event_loop());  // Give an opportunity to close


}

/***************************************************************************
 *  If service has mt_play then start only the service gobj.
 *      (Let mt_play be responsible to start their tree)
 *  If service has not mt_play then start the tree with gobj_start_tree().
 ***************************************************************************/
PRIVATE int autostart_services(void)
{
    const char *key; json_t *jn_service;
//     json_object_foreach(__jn_services__, key, jn_service) {
//         gobj_t *gobj = (gobj_t *)(uintptr_t)json_integer_value(jn_service);
//         if(gobj->gobj_flag & gobj_flag_yuno) {
//             continue;
//         }
//
//         if(gobj->gobj_flag & gobj_flag_autostart) {
// printf("\n=============> START AUTOSTART jn_services forward %s\n\n", gobj_short_name(gobj));// TODO TEST
//
//             if(gobj->gclass->gmt->mt_play) { // HACK checking mt_play because if it exists he has the power on!
//                 if(!gobj_is_running(gobj)) {
//                     gobj_start(gobj);
//                 }
//             } else {
//                 gobj_start_tree(gobj);
//             }
//         }
//     }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int autoplay_services(void)
{
    const char *key; json_t *jn_service;
//     json_object_foreach(__jn_services__, key, jn_service) {
//         gobj_t *gobj = (gobj_t *)(uintptr_t)json_integer_value(jn_service);
//         if(gobj->gobj_flag & gobj_flag_yuno) {
//             continue;
//         }
//
//         if(gobj->gobj_flag & gobj_flag_autoplay) {
// printf("\n=============> PLAY AUTOPLAY jn_services forward %s\n\n", gobj_short_name(gobj));// TODO TEST
//
//             if(!gobj_is_playing(gobj)) {
//                 gobj_play(gobj);
//             }
//         }
//     }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int pause_autoplay_services(void)
{
    /*
     *  Pause backward order
     */
//     json_t *jn_services = json_array();
//     const char *key; json_t *jn_service;
//     json_object_foreach(__jn_services__, key, jn_service) {
//         json_array_append(jn_services, jn_service);
//     }
//
//     int idx;
//     json_array_backward(jn_services, idx, jn_service) {
//         gobj_t *gobj = (gobj_t *)(uintptr_t)json_integer_value(jn_service);
//         if(gobj->gobj_flag & gobj_flag_yuno) {
//             continue;
//         }
//
//         if((gobj->gobj_flag & gobj_flag_autoplay) && gobj_is_playing(gobj)) {
// printf("\n=============> PAUSE AUTOPLAY jn_services backward %s\n\n", gobj_short_name(gobj));// TODO TEST
//
//             if(gobj_is_level_tracing(0, TRACE_START_STOP)) {
//                 gobj_log_debug(0,0,
//                     "function",     "%s", __FUNCTION__,
//                     "msgset",       "%s", MSGSET_STARTUP,
//                     "msg",          "%s", "PAUSE service",
//                     "service",      "%s", gobj_short_name(gobj),
//                     NULL
//                 );
//             }
//             gobj_pause(gobj);
//         }
//     }
//
//     json_decref(jn_services);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int stop_autostart_services(void)
{
    /*
     *  Pause backward order
     */
//     json_t *jn_services = json_array();
//     const char *key; json_t *jn_service;
//     json_object_foreach(__jn_services__, key, jn_service) {
//         json_array_append(jn_services, jn_service);
//     }
//
//     int idx;
//     json_array_backward(jn_services, idx, jn_service) {
//         gobj_t *gobj = (gobj_t *)(uintptr_t)json_integer_value(jn_service);
//         if(gobj->gobj_flag & gobj_flag_yuno) {
//             continue;
//         }
//
//         if((gobj->gobj_flag & gobj_flag_autostart) && gobj_is_running(gobj)) {
// printf("\n=============> STOP AUTOSTART jn_services backward %s\n\n", gobj_short_name(gobj));// TODO TEST
//
//
//             if(gobj_is_level_tracing(0, TRACE_START_STOP)) {
//                 gobj_log_debug(0,0,
//                     "function",     "%s", __FUNCTION__,
//                     "msgset",       "%s", MSGSET_STARTUP,
//                     "msg",          "%s", "STOP service",
//                     "service",      "%s", gobj_short_name(gobj),
//                     NULL
//                 );
//             }
//
//             if(gobj->gclass->gmt->mt_stop) { // HACK checking mt_stop because if exists he has the power on!
//                 gobj_stop(gobj);
//             } else {
//                 gobj_stop_tree(gobj); // WARNING old versions all doing stop_tree
//             }
//         }
//     }
//
//     json_decref(jn_services);

    return 0;
}
