/***********************************************************************
 *          c_webstats.c
 *          Webstats GClass.
 *
 *          Daily report of the node's web server logs
 *
 *          The run is: read the files of the day, count what they hold,
 *          write the record, send the mail. One file at a time, each one
 *          read by a C_LOG_READER child.
 *
 *          The day of a line comes from the timestamp the line carries,
 *          never from the file it sits in. So the yuno keeps no read
 *          offset, does not care when logrotate runs, and can rebuild any
 *          day that is still on disk. See README.md, section 4.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "c_log_reader.h"
#include "c_webstats.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define DEFER_MS            1       // continuation between files
#define DATE_SIZE           11      // "YYYY-MM-DD" and the zero

#define DEFAULT_ACCESS_LOG_1    "/yuneta/bin/nginx/logs/access.log"
#define DEFAULT_ACCESS_LOG_2    "/yuneta/bin/openresty/nginx/logs/access.log"
#define DEFAULT_ERROR_LOG_1     "/yuneta/bin/nginx/logs/error.log"
#define DEFAULT_ERROR_LOG_2     "/yuneta/bin/openresty/nginx/logs/error.log"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int start_run(hgobj gobj, const char *date, BOOL send);
PRIVATE int start_next_file(hgobj gobj);
PRIVATE int finish_run(hgobj gobj);
PRIVATE int arm_schedule(hgobj gobj);
PRIVATE json_t *build_file_list(hgobj gobj);
PRIVATE BOOL access_line_is_of_day(const char *line, const char *date);
PRIVATE BOOL error_line_is_of_day(const char *line, const char *date);
PRIVATE int accumulate_access_line(hgobj gobj, const char *line);
PRIVATE int accumulate_error_line(hgobj gobj, const char *line);
PRIVATE int send_report(hgobj gobj);
PRIVATE int date_of(hgobj gobj, time_t t, char *bf, size_t bfsize);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_analyze_now(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_report_day(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_get_report(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_reports(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_sources(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "authz",        0,              0,          "permission to search"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service where to search the permission. If empty print all service's permissions"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_report_day[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "date",         0,              0,          "Day to report, YYYY-MM-DD"),
SDATAPM (DTP_BOOLEAN,   "send",         0,              0,          "Send the report by email too"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_get_report[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "date",         0,              0,          "Day to get, YYYY-MM-DD"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,       "Command's help"),
SDATACM2 (DTP_SCHEMA,   "authzs",           0,                  0,              pm_authzs,      cmd_authzs,     "Authorization's help"),
SDATACM (DTP_SCHEMA,    "analyze-now",      0,                  0,              cmd_analyze_now, "Build the report of yesterday, now"),
SDATACM (DTP_SCHEMA,    "report-day",       0,                  pm_report_day,  cmd_report_day, "Rebuild the report of a day still on disk"),
SDATACM (DTP_SCHEMA,    "get-report",       0,                  pm_get_report,  cmd_get_report, "Get a stored report"),
SDATACM (DTP_SCHEMA,    "list-reports",     0,                  0,              cmd_list_reports, "List the days already reported"),
SDATACM (DTP_SCHEMA,    "list-sources",     0,                  0,              cmd_list_sources, "List the log files, and say if they can be read"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type--------name----------------flag----------------default-----description---------- */
SDATA (DTP_POINTER, "subscriber",       0,                  0,          "Subscriber of output-events"),
SDATA (DTP_LIST,    "access_log_paths", SDF_RD,             "[]",       "Access logs. The yuno also reads each '<path>.1'. Empty: both standard trees"),
SDATA (DTP_LIST,    "error_log_paths",  SDF_RD,             "[]",       "Error logs. The yuno also reads each '<path>.1'. Empty: both standard trees"),
SDATA (DTP_INTEGER, "report_hour",      SDF_WR|SDF_PERSIST, "6",        "Local hour of the daily run"),
SDATA (DTP_INTEGER, "report_minute",    SDF_WR|SDF_PERSIST, "0",        "Local minute of the daily run"),
SDATA (DTP_BOOLEAN, "send_email",       SDF_WR|SDF_PERSIST, "true",     "Send the daily report by email"),
SDATA (DTP_STRING,  "email_to",         SDF_WR|SDF_PERSIST, "",         "Destination of the report"),
SDATA (DTP_STRING,  "email_service",    SDF_RD,             "emailsender", "Service that sends the report"),
SDATA (DTP_INTEGER, "top_n",            SDF_WR|SDF_PERSIST, "20",       "Rows per top table"),
SDATA (DTP_INTEGER, "max_distinct_keys",SDF_RD,             "200000",   "Cap of keys per counter map"),
SDATA (DTP_LIST,    "probe_patterns",   SDF_RD,             "[]",       "What counts as a probe. Empty: the same set as the fail2ban filter"),
SDATA (DTP_LIST,    "internal_networks",SDF_RD,             "[]",       "Address prefixes not counted as clients"),
SDATA (DTP_INTEGER, "keep_days",        SDF_RD,             "400",      "Days of aggregates kept"),
SDATA (DTP_POINTER, "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER, "user_data2",       0,                  0,          "more user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_PARSE  = 0x0001,
    TRACE_REPORT = 0x0002,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"parse",           "Trace the lines the parser rejected"},
{"report",          "Trace the built report"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;                    // the daily schedule, seconds are accurate enough
    hgobj timer_defer;              // the continuation between files
    hgobj reader;                   // the file being read, or 0

    json_t *jn_files;               // files left in this run
    json_t *jn_report;              // the record being built

    char target_date[DATE_SIZE];    // the day of this run
    BOOL send_when_done;
    BOOL reader_done;

    int32_t report_hour;
    int32_t report_minute;
    int32_t top_n;
    BOOL send_email;
} PRIVATE_DATA;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    priv->timer_defer = gobj_create_pure_child("defer", C_TIMER0, 0, gobj);

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    } else if(gobj_is_pure_child(gobj)) {
        subscriber = gobj_parent(gobj);
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(report_hour,           gobj_read_integer_attr)
    SET_PRIV(report_minute,         gobj_read_integer_attr)
    SET_PRIV(top_n,                 gobj_read_integer_attr)
    SET_PRIV(send_email,            gobj_read_bool_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(report_hour,         gobj_read_integer_attr)
        arm_schedule(gobj);
    ELIF_EQ_SET_PRIV(report_minute,     gobj_read_integer_attr)
        arm_schedule(gobj);
    ELIF_EQ_SET_PRIV(top_n,             gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(send_email,        gobj_read_bool_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(priv->jn_files)
    JSON_DECREF(priv->jn_report)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  C_TIMER0 must be started by its owner: set_timeout0() only writes
     *  "msec". A timer0 that is armed without being running answers its
     *  own yev callback with -1, and that BREAKS THE EVENT LOOP of the
     *  whole yuno, which exits with no error of its own.
     */
    gobj_start(priv->timer_defer);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    clear_timeout0(priv->timer_defer);

    gobj_stop(priv->timer_defer);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 *
 *  The work starts here and not in mt_start: mt_create builds the children,
 *  mt_play opens what runs.
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    arm_schedule(gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    return 0;
}




                    /***************************
                     *      Commands
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw);
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    return gobj_build_authzs_doc(gobj, cmd, kw);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_analyze_now(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char date[DATE_SIZE];
    if(date_of(gobj, time(NULL) - 24*60*60, date, sizeof(date)) < 0) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot compute yesterday's date",
                gobj_yuno_role_plus_name()
            ),
            0,
            0,
            kw  // owned
        );
    }

    if(start_run(gobj, date, priv->send_email) < 0) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot start a run, one is already running",
                gobj_yuno_role_plus_name()
            ),
            0,
            0,
            kw  // owned
        );
    }

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: reading the logs of %s",
            gobj_yuno_role_plus_name(), date
        ),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_report_day(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *date = kw_get_str(gobj, kw, "date", "", 0);
    BOOL send = kw_get_bool(gobj, kw, "send", 0, KW_WILD_NUMBER);

    if(strlen(date) != DATE_SIZE-1) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: 'date' must be YYYY-MM-DD",
                gobj_yuno_role_plus_name()
            ),
            0,
            0,
            kw  // owned
        );
    }

    if(start_run(gobj, date, send) < 0) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot start a run, one is already running",
                gobj_yuno_role_plus_name()
            ),
            0,
            0,
            kw  // owned
        );
    }

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: reading the logs of %s",
            gobj_yuno_role_plus_name(), date
        ),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_get_report(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    /*
     *  TODO the store is not written yet. Until it is, only the report of
     *  the run held in memory can be answered, and the command says so
     *  instead of answering an empty record.
     */
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *date = kw_get_str(gobj, kw, "date", "", 0);

    if(priv->jn_report && strcmp(date, priv->target_date)==0) {
        return msg_iev_build_response(
            gobj,
            0,
            0,
            0,
            json_incref(priv->jn_report),
            kw  // owned
        );
    }

    return msg_iev_build_response(
        gobj,
        -1,
        json_sprintf("%s: no report of '%s' in memory, and the store is not written yet",
            gobj_yuno_role_plus_name(), date
        ),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_reports(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    /*
     *  TODO answer the dates of the 'daily_stats' topic once it exists.
     */
    return msg_iev_build_response(
        gobj,
        -1,
        json_sprintf("%s: the store is not written yet",
            gobj_yuno_role_plus_name()
        ),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *  Say which files will be read and whether each one can be read now.
 *
 *  A control that declares itself healthy is not verified: this command is
 *  what turns "the yuno is running" into "the yuno can see the logs".
 ***************************************************************************/
PRIVATE json_t *cmd_list_sources(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_files = build_file_list(gobj);
    json_t *jn_list = json_array();

    size_t idx;
    json_t *jn_file;
    json_array_foreach(jn_files, idx, jn_file) {
        const char *path = kw_get_str(gobj, jn_file, "path", "", 0);
        BOOL readable = is_regular_file(path) && access(path, R_OK)==0;

        json_array_append_new(jn_list, json_pack("{s:s, s:s, s:b}",
            "path", path,
            "kind", kw_get_str(gobj, jn_file, "kind", "", 0),
            "readable", readable
        ));
    }
    JSON_DECREF(jn_files)

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_list,
        kw  // owned
    );
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  "YYYY-MM-DD" of a time, in local time.
 *
 *  strftime() and not snprintf() of the tm fields: it is the call made for
 *  this, and it says whether the day fitted instead of leaving a silently
 *  cut date that would then be compared against every line of the file.
 ***************************************************************************/
PRIVATE int date_of(hgobj gobj, time_t t, char *bf, size_t bfsize)
{
    struct tm tm;

    if(!localtime_r(&t, &tm)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "localtime_r() FAILED",
            "t",            "%ld", (long)t,
            NULL
        );
        return -1;
    }

    if(strftime(bf, bfsize, "%Y-%m-%d", &tm) == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Date does not fit in the buffer",
            "bfsize",       "%d", (int)bfsize,
            NULL
        );
        return -1;
    }

    return 0;
}

/***************************************************************************
 *  Arm the schedule for the next report_hour:report_minute.
 *
 *  The wall clock is the right clock here: the report is defined by a local
 *  calendar day, so the run has to follow the calendar, DST included. This
 *  is not a timeout, which would use start_msectimer()/test_msectimer().
 ***************************************************************************/
PRIVATE int arm_schedule(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_is_playing(gobj)) {
        return 0;
    }

    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    tm.tm_hour = priv->report_hour;
    tm.tm_min = priv->report_minute;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;

    time_t next = mktime(&tm);
    if(next <= now) {
        /*
         *  Tomorrow. mktime() normalises the overflow, and it is the only
         *  way that stays right across a DST change: adding 86400 seconds
         *  lands one hour off twice a year.
         */
        tm.tm_mday++;
        tm.tm_isdst = -1;
        next = mktime(&tm);
    }

    if(next == (time_t)-1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Cannot compute the next run, check report_hour/report_minute",
            "report_hour",  "%d", priv->report_hour,
            "report_minute","%d", priv->report_minute,
            NULL
        );
        return -1;
    }

    set_timeout(priv->timer, (json_int_t)(next - now) * 1000);

    return 0;
}

/***************************************************************************
 *  The files of a run: what is configured, plus the '.1' of each one.
 *
 *  With delaycompress the file of the previous day is '<path>.1' and it is
 *  not compressed, so the daily run never needs gzip.
 ***************************************************************************/
PRIVATE json_t *build_file_list(hgobj gobj)
{
    json_t *jn_files = json_array();

    const char *kinds[] = {"access", "error", 0};
    const char *attrs[] = {"access_log_paths", "error_log_paths", 0};
    const char *defaults[2][2] = {
        {DEFAULT_ACCESS_LOG_1, DEFAULT_ACCESS_LOG_2},
        {DEFAULT_ERROR_LOG_1, DEFAULT_ERROR_LOG_2}
    };

    for(int i=0; kinds[i]; i++) {
        json_t *jn_paths = gobj_read_json_attr(gobj, attrs[i]);

        json_t *jn_use = json_array();
        if(json_array_size(jn_paths) > 0) {
            json_array_extend(jn_use, jn_paths);
        } else {
            /*
             *  A node runs nginx OR openresty. The tree of the other one can
             *  be installed and unused, so a missing file is not an error
             *  here: it is the tree that this node does not use.
             */
            json_array_append_new(jn_use, json_string(defaults[i][0]));
            json_array_append_new(jn_use, json_string(defaults[i][1]));
        }

        size_t idx;
        json_t *jn_path;
        json_array_foreach(jn_use, idx, jn_path) {
            const char *path = json_string_value(jn_path);
            if(empty_string(path)) {
                continue;
            }
            char rotated[PATH_MAX];
            snprintf(rotated, sizeof(rotated), "%s.1", path);

            json_array_append_new(jn_files, json_pack("{s:s, s:s}",
                "path", path, "kind", kinds[i]
            ));
            json_array_append_new(jn_files, json_pack("{s:s, s:s}",
                "path", rotated, "kind", kinds[i]
            ));
        }
        JSON_DECREF(jn_use)
    }

    return jn_files;
}

/***************************************************************************
 *  Is this access line of the wanted day?
 *
 *  The stamp is "[05/Aug/2026:00:12:34 +0200]", in the offset the line
 *  carries. Comparing the formatted day is enough and costs no mktime().
 ***************************************************************************/
PRIVATE BOOL access_line_is_of_day(const char *line, const char *date)
{
    static const char *months[] = {
        "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
    };
    static const char *month_num[] = {
        "01","02","03","04","05","06","07","08","09","10","11","12"
    };

    const char *p = strchr(line, '[');
    if(!p || strlen(p) < 21) {
        return FALSE;
    }
    p++;

    if(p[2] != '/' || p[6] != '/' || p[11] != ':') {
        return FALSE;
    }

    int mon = 0;
    for(; mon<12; mon++) {
        if(memcmp(p+3, months[mon], 3)==0) {
            break;
        }
    }
    if(mon >= 12) {
        return FALSE;
    }

    char stamp[DATE_SIZE];
    snprintf(stamp, sizeof(stamp), "%.4s-%s-%.2s", p+7, month_num[mon], p);

    return strcmp(stamp, date)==0;
}

/***************************************************************************
 *  Is this error line of the wanted day?
 *
 *  The stamp is "2026/08/05 14:23:01 [error] ...", already in order.
 ***************************************************************************/
PRIVATE BOOL error_line_is_of_day(const char *line, const char *date)
{
    if(strlen(line) < 10) {
        return FALSE;
    }
    if(line[4] != '/' || line[7] != '/') {
        return FALSE;
    }

    char stamp[DATE_SIZE];
    snprintf(stamp, sizeof(stamp), "%.4s-%.2s-%.2s", line, line+5, line+8);

    return strcmp(stamp, date)==0;
}

/***************************************************************************
 *  TODO Phase 1: split the line and add it to the counters.
 *
 *  The parser must be tolerant, not strict: the same file holds lines of
 *  three generations of log_format (6, 8 and 10 quotes), and a parser that
 *  demands the newest drops every older line without a word. Read what is
 *  there, leave the rest unset, and count what could not be read.
 ***************************************************************************/
PRIVATE int accumulate_access_line(hgobj gobj, const char *line)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_totals = kw_get_dict(gobj, priv->jn_report, "totals", 0, KW_REQUIRED);
    json_int_t requests = kw_get_int(gobj, jn_totals, "requests", 0, 0);
    json_object_set_new(jn_totals, "requests", json_integer(requests+1));

    return 0;
}

/***************************************************************************
 *  TODO Phase 1: group the line by signature and count it.
 *
 *  The signature is the message with its variable parts removed: pid#tid,
 *  *connection, client:, server:, request:, upstream:, host:, and any
 *  number. This half of the report is where the findings were.
 ***************************************************************************/
PRIVATE int accumulate_error_line(hgobj gobj, const char *line)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_errors = kw_get_dict(gobj, priv->jn_report, "errors", 0, KW_REQUIRED);
    json_int_t total = kw_get_int(gobj, jn_errors, "total", 0, 0);
    json_object_set_new(jn_errors, "total", json_integer(total+1));

    return 0;
}

/***************************************************************************
 *  Start a run for a day. Returns -1 if a run is already going.
 ***************************************************************************/
PRIVATE int start_run(hgobj gobj, const char *date, BOOL send)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_in_this_state(gobj, ST_IDLE)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL,
            "msg",          "%s", "A run is already going",
            "state",        "%s", gobj_current_state(gobj),
            "date",         "%s", date,
            NULL
        );
        return -1;
    }

    snprintf(priv->target_date, sizeof(priv->target_date), "%s", date);
    priv->send_when_done = send;

    /*
     *  The schedule is disarmed while a run goes: both timers deliver
     *  EV_TIMEOUT, and the state is what tells them apart. It is armed
     *  again when the run ends.
     */
    clear_timeout(priv->timer);

    JSON_DECREF(priv->jn_files)
    priv->jn_files = build_file_list(gobj);

    JSON_DECREF(priv->jn_report)
    priv->jn_report = json_pack("{s:s, s:i, s:s, s:I, s:[], s:{s:I}, s:{s:I}, s:[]}",
        "date", date,
        "version", 1,
        "node", get_hostname(),
        "generated_at", (json_int_t)time(NULL),
        "sources",
        "totals", "requests", (json_int_t)0,
        "errors", "total", (json_int_t)0,
        "truncated"
    );

    gobj_change_state(gobj, ST_READING);

    return start_next_file(gobj);
}

/***************************************************************************
 *  Read the next file of the run, or finish.
 ***************************************************************************/
PRIVATE int start_next_file(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(json_array_size(priv->jn_files) == 0) {
        return finish_run(gobj);
    }

    json_t *jn_file = json_array_get(priv->jn_files, 0);
    const char *path = kw_get_str(gobj, jn_file, "path", "", 0);

    priv->reader_done = FALSE;
    priv->reader = gobj_create("reader", C_LOG_READER, json_pack("{s:s}",
        "path", path
    ), gobj);
    if(!priv->reader) {
        // Error already logged
        json_array_remove(priv->jn_files, 0);
        set_timeout0(priv->timer_defer, DEFER_MS);
        return -1;
    }

    gobj_start(priv->reader);

    return 0;
}

/***************************************************************************
 *  The run is over: write the record, send the mail, arm the schedule.
 ***************************************************************************/
PRIVATE int finish_run(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_change_state(gobj, ST_REPORTING);

    if(gobj_trace_level(gobj) & TRACE_REPORT) {
        gobj_trace_json(gobj, priv->jn_report, "webstats: report of %s", priv->target_date);
    }

    /*
     *  TODO Phase 1: append the record to the 'daily_stats' topic, and drop
     *  the records older than keep_days.
     */

    if(priv->send_when_done) {
        send_report(gobj);      // Error already logged
    }

    gobj_publish_event(gobj, EV_REPORT_READY, json_incref(priv->jn_report));

    gobj_change_state(gobj, ST_IDLE);

    arm_schedule(gobj);

    return 0;
}

/***************************************************************************
 *  Hand the report to the email service.
 *
 *  The body travels as a string and not as a gbuffer: emailsender turns a
 *  gbuffer into the same string anyway, and a string does not touch the kw
 *  auto-decref of the serialized fields.
 ***************************************************************************/
PRIVATE int send_report(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *email_to = gobj_read_str_attr(gobj, "email_to");
    if(empty_string(email_to)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONFIGURATION,
            "msg",          "%s", "No 'email_to', the report was not sent",
            "date",         "%s", priv->target_date,
            NULL
        );
        return -1;
    }

    const char *email_service = gobj_read_str_attr(gobj, "email_service");
    hgobj gobj_emailsender = gobj_find_service(email_service, FALSE);
    if(!gobj_emailsender) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SERVICE,
            "msg",          "%s", "Email service not found, the report was not sent",
            "service",      "%s", email_service,
            "date",         "%s", priv->target_date,
            NULL
        );
        return -1;
    }

    /*
     *  TODO Phase 1: the HTML report, led by what needs a decision.
     *  Until then the mail carries the record, so a run that reads nothing
     *  still says so instead of being silent.
     */
    char *body = json_dumps(priv->jn_report, JSON_INDENT(4));
    if(!body) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON,
            "msg",          "%s", "Cannot dump the report, it was not sent",
            "date",         "%s", priv->target_date,
            NULL
        );
        return -1;
    }

    char subject[NAME_MAX];
    snprintf(subject, sizeof(subject), "%s: web report of %s",
        get_hostname(), priv->target_date
    );

    json_t *kw_email = json_pack("{s:s, s:s, s:s, s:b}",
        "to", email_to,
        "subject", subject,
        "body", body,
        "is_html", 0
    );
    GBMEM_FREE(body)

    return gobj_send_event(gobj_emailsender, EV_SEND_EMAIL, kw_email, gobj);
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The schedule fired: report the day that just ended.
 ***************************************************************************/
PRIVATE int ac_schedule(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char date[DATE_SIZE];
    if(date_of(gobj, time(NULL) - 24*60*60, date, sizeof(date)) < 0) {
        // Error already logged
        arm_schedule(gobj);     // do not lose the schedule over one bad day
        KW_DECREF(kw)
        return 0;
    }

    start_run(gobj, date, priv->send_email);    // Error already logged

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A batch of lines. Keep the ones of the target day.
 ***************************************************************************/
PRIVATE int ac_log_lines(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_file = json_array_get(priv->jn_files, 0);
    BOOL is_access = strcmp(kw_get_str(gobj, jn_file, "kind", "", 0), "access")==0;

    json_t *jn_lines = kw_get_list(gobj, kw, "lines", 0, 0);

    size_t idx;
    json_t *jn_line;
    json_array_foreach(jn_lines, idx, jn_line) {
        const char *line = json_string_value(jn_line);
        if(!line) {
            continue;
        }
        if(is_access) {
            if(access_line_is_of_day(line, priv->target_date)) {
                accumulate_access_line(gobj, line);
            }
        } else {
            if(error_line_is_of_day(line, priv->target_date)) {
                accumulate_error_line(gobj, line);
            }
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The file is done.
 *
 *  The reader is NOT destroyed here: this action runs inside the reader's
 *  own publish stack. It is destroyed from the deferred action.
 ***************************************************************************/
PRIVATE int ac_log_eof(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_sources = kw_get_list(gobj, priv->jn_report, "sources", 0, KW_REQUIRED);
    json_array_append_new(jn_sources, json_pack("{s:s, s:I, s:I, s:I}",
        "file", kw_get_str(gobj, kw, "path", "", 0),
        "lines", kw_get_int(gobj, kw, "lines", 0, 0),
        "bytes", kw_get_int(gobj, kw, "bytes", 0, 0),
        "too_long", kw_get_int(gobj, kw, "too_long", 0, 0)
    ));

    priv->reader_done = TRUE;
    set_timeout0(priv->timer_defer, DEFER_MS);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The file could not be read.
 *
 *  It is recorded and the run goes on: a node runs nginx or openresty, so
 *  the tree of the other one is missing by design. What must never happen
 *  is a report that does not say which of its sources it did not read.
 ***************************************************************************/
PRIVATE int ac_log_error(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_sources = kw_get_list(gobj, priv->jn_report, "sources", 0, KW_REQUIRED);
    json_array_append_new(jn_sources, json_pack("{s:s, s:s}",
        "file", kw_get_str(gobj, kw, "path", "", 0),
        "error", kw_get_str(gobj, kw, "error", "", 0)
    ));

    priv->reader_done = TRUE;
    set_timeout0(priv->timer_defer, DEFER_MS);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Out of the reader's stack: destroy it and take the next file.
 ***************************************************************************/
PRIVATE int ac_next_file(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->reader_done) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Deferred continuation with no reader done",
            "date",         "%s", priv->target_date,
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }

    if(priv->reader) {
        gobj_stop(priv->reader);
        gobj_destroy(priv->reader);
        priv->reader = 0;
    }

    if(json_array_size(priv->jn_files) > 0) {
        json_array_remove(priv->jn_files, 0);
    }

    start_next_file(gobj);      // Error already logged

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A timer was cancelled. Part of the C_TIMER0 contract: it tells its owner
 *  when the timeout it was holding will not arrive.
 ***************************************************************************/
PRIVATE int ac_timer_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
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
    .mt_create  = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_play    = mt_play,
    .mt_pause   = mt_pause,
    .mt_writing = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_WEBSTATS);

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DEFINE_STATE(ST_READING);
GOBJ_DEFINE_STATE(ST_REPORTING);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_REPORT_READY);

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_TIMEOUT,                ac_schedule,            0},
        {EV_STOPPED,                ac_timer_stopped,       0},
        {0,0,0}
    };
    ev_action_t st_reading[] = {
        {EV_LOG_LINES,              ac_log_lines,           0},
        {EV_LOG_EOF,                ac_log_eof,             0},
        {EV_LOG_ERROR,              ac_log_error,           0},
        {EV_TIMEOUT,                ac_next_file,           0},
        {EV_STOPPED,                ac_timer_stopped,       0},
        {0,0,0}
    };
    ev_action_t st_reporting[] = {
        {EV_STOPPED,                ac_timer_stopped,       0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,       st_idle},
        {ST_READING,    st_reading},
        {ST_REPORTING,  st_reporting},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_STOPPED,                0},
        {EV_LOG_LINES,              0},
        {EV_LOG_EOF,                0},
        {EV_LOG_ERROR,              0},
        {EV_REPORT_READY,           EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {NULL, 0}
    };

    /*----------------------------------------*
     *          Register GClass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0, // local methods
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
        command_table,
        s_user_trace_level,
        0 // gcflags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_webstats(void)
{
    return create_gclass(C_WEBSTATS);
}
