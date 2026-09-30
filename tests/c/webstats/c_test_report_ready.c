/***********************************************************************
 *          c_test_report_ready.c
 *
 *          Drives C_WEBSTATS through two runs of the same day and checks
 *          the report each run publishes (EV_REPORT_READY).
 *
 *          1) The access log holds REQUESTS lines of DAY: report-day DAY
 *             stores the day and publishes it (REQUESTS requests).
 *          2) The access log is emptied (as when the day has rotated
 *             away) and report-day DAY runs again: it reads nothing, keeps
 *             the stored day, and must publish THAT one. Up to 7.25.20 it
 *             mailed the stored report and published the empty one.
 *
 *          The second run is posted to ourselves: EV_REPORT_READY is
 *          published from inside the run, before webstats is idle again.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <string.h>

#include <c_webstats.h>
#include "c_test_report_ready.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BASE        "/tmp/test_webstats_report_ready"
#define DAY         "2026-09-01"
#define REQUESTS    3

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int write_file(hgobj gobj, const char *name, const char *content);
PRIVATE int ask_report_day(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
GOBJ_DEFINE_EVENT(EV_SECOND_RUN);

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
    int runs;
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
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char line[512];
    char content[4*512] = "";
    for(int i = 0; i < REQUESTS; i++) {
        snprintf(line, sizeof(line),
            "203.0.113.%d - - [01/Sep/2026:10:00:0%d +0200] \"GET / HTTP/1.1\" 200 123 \"-\" \"Mozilla/5.0\"\n",
            i + 1, i
        );
        strncat(content, line, sizeof(content) - strlen(content) - 1);
    }
    write_file(gobj, "access.log", content);

    priv->webstats = gobj_find_service("webstats", TRUE);
    gobj_subscribe_event(priv->webstats, EV_REPORT_READY, 0, gobj);

    ask_report_day(gobj);

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




/***************************************************************************
 *  Write a log file of the test
 ***************************************************************************/
PRIVATE int write_file(hgobj gobj, const char *name, const char *content)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), BASE, "logs", name, NULL);

    FILE *file = fopen(path, "w");
    if(!file) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot write a log file of the test",
            "path",         "%s", path,
            NULL
        );
        return -1;
    }
    fputs(content, file);
    fclose(file);
    return 0;
}

/***************************************************************************
 *  report-day DAY, not mailed
 ***************************************************************************/
PRIVATE int ask_report_day(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_resp = gobj_command(priv->webstats, "report-day report_date=" DAY " send=0", 0, gobj);
    int result = (int)kw_get_int(gobj, jn_resp, "result", -1, 0);
    JSON_DECREF(jn_resp)
    if(result < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "report-day refused",
            NULL
        );
        set_yuno_must_die();
        return -1;
    }
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  A run ended: the report it publishes
 ***************************************************************************/
PRIVATE int ac_report_ready(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t requests = kw_get_int(gobj, kw, "totals`requests", -1, 0);
    priv->runs++;

    if(priv->runs == 1) {
        if(requests == REQUESTS) {
            gobj_log_info(gobj, 0,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "First run published the day",
                NULL
            );
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "First run published a wrong report",
                "requests",     "%ld", (long)requests,
                NULL
            );
        }
        write_file(gobj, "access.log", "");
        gobj_post_event(gobj, EV_SECOND_RUN, 0, gobj);

    } else {
        if(requests == REQUESTS) {
            gobj_log_info(gobj, 0,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Second run published the stored day",
                NULL
            );
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Second run published the empty report, not the stored one",
                "requests",     "%ld", (long)requests,
                NULL
            );
        }
        set_yuno_must_die();
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The first run is over: the same day again, with nothing to read
 ***************************************************************************/
PRIVATE int ac_second_run(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    ask_report_day(gobj);

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
GOBJ_DEFINE_GCLASS(C_TEST_REPORT_READY);

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
        {EV_REPORT_READY,           ac_report_ready,            0},
        {EV_SECOND_RUN,             ac_second_run,              0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_REPORT_READY,   0},
        {EV_SECOND_RUN,     0},
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
PUBLIC int register_c_test_report_ready(void)
{
    return create_gclass(C_TEST_REPORT_READY);
}
