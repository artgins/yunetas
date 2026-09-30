/***********************************************************************
 *          c_test_schedule_slot.c
 *
 *          The day a scheduled run reports is the day before its SLOT, not
 *          the day before the moment its timer fires.
 *
 *          report_hour:report_minute is set to one minute ago, so the slot
 *          armed is tomorrow at that time, and the day it reports is today.
 *          Then EV_TIMEOUT is delivered to webstats at once: the schedule
 *          timer firing BEFORE its slot, as a timer may (a second early
 *          across midnight is enough for a report_hour of 0). Up to 7.25.20
 *          ac_schedule() took yesterday of time(NULL), and reported the day
 *          before.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <time.h>

#include <c_webstats.h>
#include "c_test_schedule_slot.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
GOBJ_DEFINE_EVENT(EV_FIRE_EARLY);

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
PRIVATE const trace_level_t s_user_trace_level[16] = {
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj webstats;
    char expected_date[16];     // the day the slot reports
} PRIVATE_DATA;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method play
 *
 *  The schedule is set to one minute ago: the next slot is tomorrow at that
 *  time, and the day it reports is the calendar day of one minute ago.
 *  The early fire is posted: webstats arms its schedule in its own play.
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    time_t a_minute_ago = time(NULL) - 60;
    struct tm tm;
    localtime_r(&a_minute_ago, &tm);
    strftime(priv->expected_date, sizeof(priv->expected_date), "%Y-%m-%d", &tm);

    priv->webstats = gobj_find_service("webstats", TRUE);
    gobj_subscribe_event(priv->webstats, EV_REPORT_READY, 0, gobj);
    gobj_write_integer_attr(priv->webstats, "report_hour", tm.tm_hour);
    gobj_write_integer_attr(priv->webstats, "report_minute", tm.tm_min);

    gobj_post_event(gobj, EV_FIRE_EARLY, 0, gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_unsubscribe_event(priv->webstats, EV_REPORT_READY, 0, gobj);

    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The schedule timer of webstats fires now, a day before its slot
 ***************************************************************************/
PRIVATE int ac_fire_early(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_send_event(priv->webstats, EV_TIMEOUT, 0, gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The run is over: which day did it report?
 ***************************************************************************/
PRIVATE int ac_report_ready(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *date = kw_get_str(gobj, kw, "date", "", 0);
    if(strcmp(date, priv->expected_date) == 0) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Scheduled run reported the day of its slot",
            NULL
        );
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Scheduled run reported a day that is not its slot's",
            "date",         "%s", date,
            "expected",     "%s", priv->expected_date,
            NULL
        );
    }
    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_SCHEDULE_SLOT);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_idle[] = {
        {EV_FIRE_EARLY,             ac_fire_early,              0},
        {EV_REPORT_READY,           ac_report_ready,            0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_FIRE_EARLY,     0},
        {EV_REPORT_READY,   0},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,
        0   // gcflag_t
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_test_schedule_slot(void)
{
    return create_gclass(C_TEST_SCHEDULE_SLOT);
}
