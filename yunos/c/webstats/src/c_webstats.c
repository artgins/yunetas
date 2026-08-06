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
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <strings.h>    /* strcasestr() */
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

#define MAX_SERVER_ERRORS   500     // 5xx lines kept whole
#define MAX_SAMPLE_LEN      512     // of an error line kept as the sample
#define MAX_QUOTES          16      // more than the widest generation needs

/*
 *  The bucket edges of the latency histogram, in seconds. A histogram and
 *  not a list of samples: the memory is the same whatever the traffic, and
 *  the buckets of two days add up, so a weekly view costs nothing.
 */
PRIVATE const double latency_edges[] = {
    0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0
};
#define LATENCY_BUCKETS (sizeof(latency_edges)/sizeof(latency_edges[0]) + 1)

/*
 *  What counts as a probe when the configuration says nothing. The same set
 *  as the packaged fail2ban filter: no node serves PHP, and none of these
 *  paths exists anywhere.
 *
 *  Deliberately NOT keyed on the status code. A SPA vhost answers 200 to any
 *  path (try_files ... /index.html), which is what made the first version of
 *  that filter blind on both console vhosts.
 */
PRIVATE const char *default_probe_patterns[] = {
    ".php", ".env", "/.git", "/.aws", "/.ssh", "/.svn", "/.hg", "/wp-", 0
};

/***************************************************************************
 *              Structures
 ***************************************************************************/
/*
 *  One access line, split. The pointers are into the line itself, so the
 *  struct lives only as long as the line does.
 */
typedef struct {
    const char *client;     size_t client_len;
    const char *path;       size_t path_len;        // the request without its query
    const char *referer;    size_t referer_len;
    const char *agent;      size_t agent_len;
    const char *host;       size_t host_len;        // empty before the $host generation
    json_int_t status;
    json_int_t bytes;
    double request_time;
    BOOL has_request_time;                          // false before the times generation
    int hour;
} ACCESS_LINE;

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
PRIVATE int parse_access_line(const char *line, ACCESS_LINE *al);
PRIVATE int count_key(hgobj gobj, json_t *jn_map, const char *key, const char *map_name);
PRIVATE int count_keyn(hgobj gobj, json_t *jn_map, const char *key, size_t len, const char *map_name);
PRIVATE json_t *top_of(json_t *jn_map, int top_n);
PRIVATE json_t *sorted_strings(json_t *jn_list);
PRIVATE void note_truncated(hgobj gobj, const char *map_name);
PRIVATE json_t *new_latency(void);
PRIVATE void add_latency(json_t *jn_latency, double seconds);
PRIVATE json_t *latency_percentiles(json_t *jn_latency);
PRIVATE int error_signature(const char *line, char *bf, size_t bfsize);
PRIVATE void close_report(hgobj gobj);
PRIVATE int open_store(hgobj gobj);
PRIVATE int close_store(hgobj gobj);
PRIVATE int store_report(hgobj gobj);
PRIVATE json_t *load_report(hgobj gobj, const char *date);
PRIVATE int prune_store(hgobj gobj);
PRIVATE void compare_with_history(hgobj gobj);
PRIVATE json_t *headline_of(hgobj gobj, json_t *record);

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
/*
 *  The day parameter is NOT called "date".
 *
 *  `command-yuno` hands its WHOLE kw to gobj_list_nodes() as the filter that
 *  picks the yuno, so any parameter named like a field of the yuno record
 *  becomes a filter on that field. The yuno record has a "date" (when it was
 *  created), so `command-yuno id=91 command=report-day date=2026-08-05`
 *  matches no yuno and answers "Yuno not found" -- naming the yuno, never
 *  the parameter. It is the same trap as the documented `id=`, and it is not
 *  limited to `id`.
 *
 *  The handler still reads a plain "date" as a fallback, for a caller that
 *  reaches the gclass directly.
 */
PRIVATE sdata_desc_t pm_report_day[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "report_date",  0,              0,          "Day to report, YYYY-MM-DD"),
SDATAPM (DTP_BOOLEAN,   "send",         0,              0,          "Send the report by email too"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_get_report[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "report_date",  0,              0,          "Day to get, YYYY-MM-DD"),
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
SDATA (DTP_STRING,  "tranger_path",     SDF_RD,             "/yuneta/store/webstats", "Where the daily records live"),
SDATA (DTP_STRING,  "tranger_database", SDF_RD,             "webstats", "TimeRanger2 database"),
SDATA (DTP_STRING,  "topic_daily",      SDF_RD,             "daily_stats", "Topic of the daily records"),
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

    /*
     *  Counter maps of the run: key -> count. They are kept apart from the
     *  record because the record holds only the top of each one, and the
     *  full map of a day under attack does not belong in a mail.
     */
    json_t *jn_clients;
    json_t *jn_paths;
    json_t *jn_not_found;
    json_t *jn_referrers;
    json_t *jn_agents;
    json_t *jn_probe_clients;
    json_t *jn_probe_patterns;
    json_t *jn_signatures;          // signature -> {count, first, last, sample}
    json_t *jn_vhosts;              // host -> {requests, bytes, status, latency}
    json_t *jn_probe_list;          // the patterns in use

    hgobj gobj_tranger;             // the C_TRANGER service
    json_t *tranger;                // its handle, NOT ours
    const char *topic_daily;

    char target_date[DATE_SIZE];    // the day of this run
    BOOL send_when_done;
    BOOL reader_done;

    uint64_t cur_kept;              // lines of the target day in the file being read
    uint64_t cur_unparsed;          // lines of the file the parser did not understand

    int32_t report_hour;
    int32_t report_minute;
    int32_t top_n;
    int32_t max_distinct_keys;
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
    SET_PRIV(max_distinct_keys,     gobj_read_integer_attr)
    SET_PRIV(topic_daily,           gobj_read_str_attr)
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
    JSON_DECREF(priv->jn_clients)
    JSON_DECREF(priv->jn_paths)
    JSON_DECREF(priv->jn_not_found)
    JSON_DECREF(priv->jn_referrers)
    JSON_DECREF(priv->jn_agents)
    JSON_DECREF(priv->jn_probe_clients)
    JSON_DECREF(priv->jn_probe_patterns)
    JSON_DECREF(priv->jn_signatures)
    JSON_DECREF(priv->jn_vhosts)
    JSON_DECREF(priv->jn_probe_list)
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
    if(open_store(gobj) < 0) {
        /*
         *  Error already logged. The run still goes: a node that cannot
         *  write the history must still send today's mail, and saying so is
         *  more use than staying quiet until somebody fixes the disk.
         */
    }

    prune_store(gobj);      // Error already logged

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

    close_store(gobj);

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
    const char *date = kw_get_str(gobj, kw, "report_date",
        kw_get_str(gobj, kw, "date", "", 0), 0
    );
    BOOL send = kw_get_bool(gobj, kw, "send", 0, KW_WILD_NUMBER);

    if(strlen(date) != DATE_SIZE-1) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: 'report_date' must be YYYY-MM-DD",
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
    const char *date = kw_get_str(gobj, kw, "report_date",
        kw_get_str(gobj, kw, "date", "", 0), 0
    );

    json_t *record = load_report(gobj, date);
    if(!record) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: no report stored for '%s'",
                gobj_yuno_role_plus_name(), date
            ),
            0,
            0,
            kw  // owned
        );
    }

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        record,     // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_reports(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->tranger) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: the store is not open",
                gobj_yuno_role_plus_name()
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  The keys ARE the dates, and an ISO date sorts the same as it reads.
     */
    json_t *jn_keys = sorted_strings(tranger2_list_keys(priv->tranger, priv->topic_daily));

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_keys,    // owned
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
 *  Note that a counter map hit its cap, once per map and per run.
 ***************************************************************************/
PRIVATE void note_truncated(hgobj gobj, const char *map_name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_truncated = kw_get_list(gobj, priv->jn_report, "truncated", 0, KW_REQUIRED);

    size_t idx;
    json_t *jn_entry;
    json_array_foreach(jn_truncated, idx, jn_entry) {
        if(strcmp(kw_get_str(gobj, jn_entry, "counter", "", 0), map_name)==0) {
            json_int_t dropped = kw_get_int(gobj, jn_entry, "dropped", 0, 0);
            json_object_set_new(jn_entry, "dropped", json_integer(dropped+1));
            return;
        }
    }

    json_array_append_new(jn_truncated, json_pack("{s:s, s:I}",
        "counter", map_name,
        "dropped", (json_int_t)1
    ));
}

/***************************************************************************
 *  Add one to key. A new key is refused once the map is full.
 *
 *  A day under attack makes a very large number of distinct paths, and an
 *  unbounded map is how a report generator becomes the incident. Reaching
 *  the cap is written into the record: a cap nobody is told about reads as
 *  full coverage.
 ***************************************************************************/
PRIVATE int count_key(hgobj gobj, json_t *jn_map, const char *key, const char *map_name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(empty_string(key)) {
        key = "-";
    }

    json_t *jn_count = json_object_get(jn_map, key);
    if(jn_count) {
        json_object_set_new(jn_map, key, json_integer(json_integer_value(jn_count)+1));
        return 0;
    }

    if((int32_t)json_object_size(jn_map) >= priv->max_distinct_keys) {
        note_truncated(gobj, map_name);
        return -1;
    }

    json_object_set_new(jn_map, key, json_integer(1));

    return 0;
}

/***************************************************************************
 *  Same, for a key that is a slice of the line and has no terminator.
 ***************************************************************************/
PRIVATE int count_keyn(hgobj gobj, json_t *jn_map, const char *key, size_t len, const char *map_name)
{
    char bf[PATH_MAX];

    if(len >= sizeof(bf)) {
        len = sizeof(bf)-1;
    }
    memcpy(bf, key, len);
    bf[len] = 0;

    return count_key(gobj, jn_map, bf, map_name);
}

/***************************************************************************
 *  The top rows of a counter map, most seen first.
 ***************************************************************************/
PRIVATE int cmp_by_count(const void *a, const void *b)
{
    json_int_t ca = json_integer_value(json_object_get(*(json_t * const *)a, "count"));
    json_int_t cb = json_integer_value(json_object_get(*(json_t * const *)b, "count"));

    if(ca < cb) {
        return 1;
    }
    if(ca > cb) {
        return -1;
    }

    return 0;
}

PRIVATE json_t *top_of(json_t *jn_map, int top_n)
{
    json_t *jn_top = json_array();

    size_t size = json_object_size(jn_map);
    if(size == 0) {
        return jn_top;
    }

    json_t **rows = gbmem_malloc(size * sizeof(json_t *));
    if(!rows) {
        // Error already logged
        return jn_top;
    }

    size_t n = 0;
    const char *key;
    json_t *jn_value;
    json_object_foreach(jn_map, key, jn_value) {
        rows[n++] = json_pack("{s:s, s:I}",
            "key", key,
            "count", (json_int_t)json_integer_value(jn_value)
        );
    }

    qsort(rows, n, sizeof(json_t *), cmp_by_count);

    for(size_t i=0; i<n; i++) {
        if(i < (size_t)top_n) {
            json_array_append_new(jn_top, rows[i]);
        } else {
            json_decref(rows[i]);
        }
    }

    GBMEM_FREE(rows)

    return jn_top;
}

/***************************************************************************
 *  A json array of strings, sorted. jansson has no sort of its own.
 ***************************************************************************/
PRIVATE int cmp_strings(const void *a, const void *b)
{
    const char *sa = json_string_value(*(json_t * const *)a);
    const char *sb = json_string_value(*(json_t * const *)b);

    return strcmp(sa?sa:"", sb?sb:"");
}

PRIVATE json_t *sorted_strings(json_t *jn_list)     // owned
{
    size_t size = json_array_size(jn_list);
    if(size < 2) {
        return jn_list;
    }

    json_t **rows = gbmem_malloc(size * sizeof(json_t *));
    if(!rows) {
        // Error already logged
        return jn_list;
    }

    for(size_t i=0; i<size; i++) {
        rows[i] = json_incref(json_array_get(jn_list, i));
    }

    qsort(rows, size, sizeof(json_t *), cmp_strings);

    json_t *jn_sorted = json_array();
    for(size_t i=0; i<size; i++) {
        json_array_append_new(jn_sorted, rows[i]);
    }

    GBMEM_FREE(rows)
    JSON_DECREF(jn_list)

    return jn_sorted;
}

/***************************************************************************
 *  A latency histogram, empty.
 ***************************************************************************/
PRIVATE json_t *new_latency(void)
{
    json_t *jn_buckets = json_array();
    for(size_t i=0; i<LATENCY_BUCKETS; i++) {
        json_array_append_new(jn_buckets, json_integer(0));
    }

    return json_pack("{s:I, s:f, s:f, s:o}",
        "count", (json_int_t)0,
        "sum", 0.0,
        "max", 0.0,
        "buckets", jn_buckets
    );
}

/***************************************************************************
 *  Add one measure to a histogram.
 ***************************************************************************/
PRIVATE void add_latency(json_t *jn_latency, double seconds)
{
    if(!jn_latency) {
        return;
    }

    size_t bucket = LATENCY_BUCKETS-1;
    for(size_t i=0; i<LATENCY_BUCKETS-1; i++) {
        if(seconds <= latency_edges[i]) {
            bucket = i;
            break;
        }
    }

    json_t *jn_buckets = json_object_get(jn_latency, "buckets");
    json_t *jn_bucket = json_array_get(jn_buckets, bucket);
    json_array_set_new(jn_buckets, bucket, json_integer(json_integer_value(jn_bucket)+1));

    json_int_t count = json_integer_value(json_object_get(jn_latency, "count"));
    json_object_set_new(jn_latency, "count", json_integer(count+1));

    double sum = json_number_value(json_object_get(jn_latency, "sum"));
    json_object_set_new(jn_latency, "sum", json_real(sum + seconds));

    double max = json_number_value(json_object_get(jn_latency, "max"));
    if(seconds > max) {
        json_object_set_new(jn_latency, "max", json_real(seconds));
    }
}

/***************************************************************************
 *  p50/p95/p99 of a histogram, read off the bucket edges.
 *
 *  The value is the edge of the bucket the percentile falls in, so it is an
 *  upper bound and not an interpolation: saying "p95 is at most 0.5 s" is
 *  true, and a number invented between two edges is not.
 ***************************************************************************/
PRIVATE double percentile_of(json_t *jn_buckets, json_int_t count, int pct)
{
    /*
     *  Ceiling division, and never below one sample. Truncating puts the
     *  p95 of two measures on the first bucket, which reads as a fast day
     *  when one of the two took half a second.
     */
    json_int_t target = (count * pct + 99) / 100;
    if(target < 1) {
        target = 1;
    }

    json_int_t seen = 0;
    for(size_t i=0; i<LATENCY_BUCKETS; i++) {
        seen += json_integer_value(json_array_get(jn_buckets, i));
        if(seen >= target) {
            if(i >= LATENCY_BUCKETS-1) {
                return -1;      // above the last edge, unbounded
            }
            return latency_edges[i];
        }
    }

    return -1;
}

PRIVATE json_t *latency_percentiles(json_t *jn_latency)
{
    json_int_t count = json_integer_value(json_object_get(jn_latency, "count"));
    if(count == 0) {
        return json_object();
    }

    json_t *jn_buckets = json_object_get(jn_latency, "buckets");
    double sum = json_number_value(json_object_get(jn_latency, "sum"));

    return json_pack("{s:f, s:f, s:f, s:f}",
        "avg", sum/(double)count,
        "p50", percentile_of(jn_buckets, count, 50),
        "p95", percentile_of(jn_buckets, count, 95),
        "p99", percentile_of(jn_buckets, count, 99)
    );
}

/***************************************************************************
 *  Split an access line.
 *
 *  The parser is tolerant on purpose: the same file holds lines of three
 *  generations of log_format, and a parser that demands the newest drops
 *  every older line without a word. Each generation only appends, so the
 *  number of quotes says which one a line is:
 *
 *      6  = combined
 *      8  = combined + "$host"
 *      10 = combined + "$host" + $request_time "$upstream_response_time"
 *
 *  Anything else is counted as unparsed and reported per file.
 ***************************************************************************/
PRIVATE int parse_access_line(const char *line, ACCESS_LINE *al)
{
    memset(al, 0, sizeof(*al));
    al->hour = -1;

    /*
     *  The quotes. nginx escapes a quote inside a field as \x22, so a raw
     *  one never appears in the middle of a value.
     */
    const char *q[MAX_QUOTES];
    size_t nq = 0;
    for(const char *p = line; *p; p++) {
        if(*p == '"') {
            if(nq >= MAX_QUOTES) {
                return -1;
            }
            q[nq++] = p;
        }
    }
    if(nq != 6 && nq != 8 && nq != 10) {
        return -1;
    }

    /*
     *  Client: up to the first space.
     */
    const char *sp = strchr(line, ' ');
    if(!sp) {
        return -1;
    }
    al->client = line;
    al->client_len = (size_t)(sp - line);

    /*
     *  Hour, from the stamp the line carries: "[05/Aug/2026:00:12:34 +0200]"
     */
    const char *br = strchr(line, '[');
    if(br && strlen(br) > 14 && br[12] == ':') {
        al->hour = (br[13]-'0')*10 + (br[14]-'0');
        if(al->hour < 0 || al->hour > 23) {
            al->hour = -1;
        }
    }

    /*
     *  Request: "GET /path?a=b HTTP/1.1". Only the path is kept, cut at the
     *  query, the same way the fail2ban filter bounds it.
     */
    const char *req = q[0]+1;
    size_t req_len = (size_t)(q[1] - req);
    const char *m = memchr(req, ' ', req_len);
    if(m) {
        const char *path = m+1;
        size_t rest = req_len - (size_t)(path - req);
        const char *e = memchr(path, ' ', rest);
        size_t path_len = e? (size_t)(e - path) : rest;
        const char *qm = memchr(path, '?', path_len);
        if(qm) {
            path_len = (size_t)(qm - path);
        }
        al->path = path;
        al->path_len = path_len;
    } else {
        al->path = req;             // a request with no method, keep it whole
        al->path_len = req_len;
    }

    /*
     *  Status and bytes, between the request and the referer.
     */
    const char *p = q[1]+1;
    while(*p == ' ') {
        p++;
    }
    al->status = strtoll(p, (char **)&p, 10);
    while(*p == ' ') {
        p++;
    }
    al->bytes = strtoll(p, (char **)&p, 10);

    al->referer = q[2]+1;
    al->referer_len = (size_t)(q[3] - al->referer);
    al->agent = q[4]+1;
    al->agent_len = (size_t)(q[5] - al->agent);

    if(nq >= 8) {
        al->host = q[6]+1;
        al->host_len = (size_t)(q[7] - al->host);
    }

    if(nq >= 10) {
        const char *t = q[7]+1;
        while(*t == ' ') {
            t++;
        }
        char *end = 0;
        double v = strtod(t, &end);
        if(end != t) {
            al->request_time = v;
            al->has_request_time = TRUE;
        }
    }

    return 0;
}

/***************************************************************************
 *  Add one access line of the target day to the counters.
 ***************************************************************************/
PRIVATE int accumulate_access_line(hgobj gobj, const char *line)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    ACCESS_LINE al;
    if(parse_access_line(line, &al) < 0) {
        priv->cur_unparsed++;
        if(gobj_trace_level(gobj) & TRACE_PARSE) {
            gobj_trace_msg(gobj, "webstats: unparsed access line: %s", line);
        }
        return -1;
    }

    char host[NAME_MAX];
    snprintf(host, sizeof(host), "%.*s", (int)al.host_len, al.host? al.host:"");
    if(empty_string(host)) {
        snprintf(host, sizeof(host), "%s", "-");    // before the $host generation
    }

    /*
     *  Totals
     */
    json_t *jn_totals = kw_get_dict(gobj, priv->jn_report, "totals", 0, KW_REQUIRED);
    json_object_set_new(jn_totals, "requests",
        json_integer(kw_get_int(gobj, jn_totals, "requests", 0, 0) + 1)
    );
    json_object_set_new(jn_totals, "bytes",
        json_integer(kw_get_int(gobj, jn_totals, "bytes", 0, 0) + al.bytes)
    );

    char klass[8];
    snprintf(klass, sizeof(klass), "%dxx", (int)(al.status/100));
    json_t *jn_status = kw_get_dict(gobj, jn_totals, "status", 0, KW_REQUIRED);
    json_object_set_new(jn_status, klass,
        json_integer(kw_get_int(gobj, jn_status, klass, 0, 0) + 1)
    );

    /*
     *  By hour
     */
    if(al.hour >= 0) {
        json_t *jn_by_hour = kw_get_list(gobj, priv->jn_report, "by_hour", 0, KW_REQUIRED);
        json_t *jn_h = json_array_get(jn_by_hour, (size_t)al.hour);
        json_array_set_new(jn_by_hour, (size_t)al.hour,
            json_integer(json_integer_value(jn_h) + 1)
        );
    }

    /*
     *  By vhost. This is what $host was added to the format for.
     */
    json_t *jn_vhost = json_object_get(priv->jn_vhosts, host);
    if(!jn_vhost) {
        if((int32_t)json_object_size(priv->jn_vhosts) >= priv->max_distinct_keys) {
            note_truncated(gobj, "by_vhost");
        } else {
            jn_vhost = json_pack("{s:I, s:I, s:{}, s:o}",
                "requests", (json_int_t)0,
                "bytes", (json_int_t)0,
                "status",
                "latency", new_latency()
            );
            json_object_set_new(priv->jn_vhosts, host, jn_vhost);
        }
    }
    if(jn_vhost) {
        json_object_set_new(jn_vhost, "requests",
            json_integer(kw_get_int(gobj, jn_vhost, "requests", 0, 0) + 1)
        );
        json_object_set_new(jn_vhost, "bytes",
            json_integer(kw_get_int(gobj, jn_vhost, "bytes", 0, 0) + al.bytes)
        );
        json_t *jn_vstatus = kw_get_dict(gobj, jn_vhost, "status", 0, KW_REQUIRED);
        json_object_set_new(jn_vstatus, klass,
            json_integer(kw_get_int(gobj, jn_vstatus, klass, 0, 0) + 1)
        );
        if(al.has_request_time) {
            add_latency(json_object_get(jn_vhost, "latency"), al.request_time);
        }
    }

    if(al.has_request_time) {
        add_latency(kw_get_dict(gobj, priv->jn_report, "latency", 0, KW_REQUIRED),
            al.request_time
        );
    }

    /*
     *  Clients. Ours are left out on purpose: without this the top clients
     *  of a node are the suite talking to itself.
     */
    char client[NAME_MAX];
    snprintf(client, sizeof(client), "%.*s", (int)al.client_len, al.client);

    BOOL internal = FALSE;
    size_t idx;
    json_t *jn_prefix;
    json_array_foreach(gobj_read_json_attr(gobj, "internal_networks"), idx, jn_prefix) {
        const char *prefix = json_string_value(jn_prefix);
        if(!empty_string(prefix) && strncmp(client, prefix, strlen(prefix))==0) {
            internal = TRUE;
            break;
        }
    }
    if(!internal) {
        count_key(gobj, priv->jn_clients, client, "clients");
    }

    /*
     *  Tops
     */
    count_keyn(gobj, priv->jn_paths, al.path, al.path_len, "paths");
    if(al.status == 404) {
        count_keyn(gobj, priv->jn_not_found, al.path, al.path_len, "not_found");
    }
    if(al.referer_len > 0 && !(al.referer_len == 1 && al.referer[0] == '-')) {
        count_keyn(gobj, priv->jn_referrers, al.referer, al.referer_len, "referrers");
    }
    count_keyn(gobj, priv->jn_agents, al.agent, al.agent_len, "agents");

    /*
     *  Every 5xx whole, not a counter: the line is what somebody has to read.
     */
    if(al.status >= 500 && al.status < 600) {
        json_t *jn_server_errors = kw_get_list(gobj, priv->jn_report, "server_errors", 0, KW_REQUIRED);
        if(json_array_size(jn_server_errors) < MAX_SERVER_ERRORS) {
            json_array_append_new(jn_server_errors, json_pack("{s:s, s:s, s:I, s:s, s:i}",
                "host", host,
                "client", client,
                "status", al.status,
                "path", "",
                "hour", al.hour
            ));
            json_t *jn_last = json_array_get(jn_server_errors, json_array_size(jn_server_errors)-1);
            json_object_set_new(jn_last, "path", json_stringn(al.path, al.path_len));
        } else {
            note_truncated(gobj, "server_errors");
        }
    }

    /*
     *  Probes, by what is asked and never by the status code.
     */
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%.*s", (int)al.path_len, al.path);

    json_t *jn_pattern;
    json_array_foreach(priv->jn_probe_list, idx, jn_pattern) {
        const char *pattern = json_string_value(jn_pattern);
        if(empty_string(pattern)) {
            continue;
        }
        if(strcasestr(path, pattern)) {
            json_t *jn_probes = kw_get_dict(gobj, priv->jn_report, "probes", 0, KW_REQUIRED);
            json_object_set_new(jn_probes, "requests",
                json_integer(kw_get_int(gobj, jn_probes, "requests", 0, 0) + 1)
            );
            count_key(gobj, priv->jn_probe_patterns, pattern, "probe_patterns");
            count_key(gobj, priv->jn_probe_clients, client, "probe_clients");
            break;
        }
    }

    return 0;
}

/***************************************************************************
 *  The signature of an error line: the message with the parts that change
 *  from one occurrence to the next taken out.
 *
 *  "2026/08/05 14:23:01 [error] 1234#0: *567 connect() failed (111: Connection
 *   refused) while connecting to upstream, client: 1.2.3.4, server: ..."
 *
 *  becomes
 *
 *  "[error] connect() failed (N: Connection refused) while connecting to
 *   upstream"
 ***************************************************************************/
PRIVATE int error_signature(const char *line, char *bf, size_t bfsize)
{
    /*
     *  Drop the stamp: "YYYY/MM/DD HH:MM:SS "
     */
    const char *p = line;
    const char *lb = strchr(p, '[');
    if(lb) {
        p = lb;
    }

    /*
     *  Drop "1234#0: *567 " after the level.
     */
    const char *rb = strchr(p, ']');
    const char *body = rb? rb+1 : p;
    while(*body == ' ') {
        body++;
    }
    const char *star = strstr(body, ": *");
    if(star) {
        body = star + 3;
        while(*body && *body != ' ') {
            body++;         // the connection number
        }
        while(*body == ' ') {
            body++;
        }
    }

    /*
     *  Cut at the variable context. Everything from here names one client,
     *  one request, one upstream.
     */
    size_t body_len = strlen(body);
    const char *cut = strstr(body, ", client: ");
    if(cut) {
        body_len = (size_t)(cut - body);
    }

    /*
     *  The level, then the message with runs of digits folded into N.
     */
    size_t out = 0;
    if(lb && rb && rb > lb) {
        size_t level_len = (size_t)(rb - lb) + 1;
        if(level_len + 2 < bfsize) {
            memcpy(bf, lb, level_len);
            out = level_len;
            bf[out++] = ' ';
        }
    }

    BOOL in_number = FALSE;
    for(size_t i=0; i<body_len && out < bfsize-1; i++) {
        if(body[i] >= '0' && body[i] <= '9') {
            if(!in_number) {
                bf[out++] = 'N';
                in_number = TRUE;
            }
            continue;
        }
        in_number = FALSE;
        bf[out++] = body[i];
    }
    bf[out] = 0;

    if(out == 0) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *  Add one error line of the target day, grouped by signature.
 *
 *  This half of the report is where the findings were: the Keycloak refusals,
 *  the client_max_body_size of real users, the bind() of an automation that
 *  believes it starts nginx. None of it is in the access log.
 ***************************************************************************/
PRIVATE int accumulate_error_line(hgobj gobj, const char *line)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_errors = kw_get_dict(gobj, priv->jn_report, "errors", 0, KW_REQUIRED);
    json_object_set_new(jn_errors, "total",
        json_integer(kw_get_int(gobj, jn_errors, "total", 0, 0) + 1)
    );

    char signature[MAX_SAMPLE_LEN];
    if(error_signature(line, signature, sizeof(signature)) < 0) {
        priv->cur_unparsed++;
        if(gobj_trace_level(gobj) & TRACE_PARSE) {
            gobj_trace_msg(gobj, "webstats: unparsed error line: %s", line);
        }
        return -1;
    }

    /*
     *  The time of the line: "YYYY/MM/DD HH:MM:SS"
     */
    char at[9] = "";
    if(strlen(line) > 19 && line[13] == ':') {
        snprintf(at, sizeof(at), "%.8s", line+11);
    }

    json_t *jn_signature = json_object_get(priv->jn_signatures, signature);
    if(!jn_signature) {
        if((int32_t)json_object_size(priv->jn_signatures) >= priv->max_distinct_keys) {
            note_truncated(gobj, "error_signatures");
            return 0;
        }
        jn_signature = json_pack("{s:I, s:s, s:s, s:s}",
            "count", (json_int_t)0,
            "first", at,
            "last", at,
            "sample", ""
        );
        json_object_set_new(jn_signature, "sample",
            json_stringn(line, strnlen(line, MAX_SAMPLE_LEN))
        );
        json_object_set_new(priv->jn_signatures, signature, jn_signature);
    }

    json_object_set_new(jn_signature, "count",
        json_integer(kw_get_int(gobj, jn_signature, "count", 0, 0) + 1)
    );
    if(!empty_string(at)) {
        json_object_set_new(jn_signature, "last", json_string(at));
    }

    return 0;
}

/***************************************************************************
 *  Open the store: one topic, one record per day, keyed by the date.
 *
 *  The raw lines never come here. The rotated .gz files are the archive for
 *  30 days; what this keeps is the handful of numbers that let a report say
 *  what CHANGED, which is the only thing that makes a daily mail worth
 *  opening.
 ***************************************************************************/
PRIVATE int open_store(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gobj_tranger) {
        return 0;                   // already open
    }

    const char *path = gobj_read_str_attr(gobj, "tranger_path");
    const char *database = gobj_read_str_attr(gobj, "tranger_database");
    if(empty_string(path) || empty_string(database) || empty_string(priv->topic_daily)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONFIGURATION,
            "msg",          "%s", "tranger_path, tranger_database or topic_daily is empty",
            "path",         "%s", path,
            "database",     "%s", database,
            "topic",        "%s", priv->topic_daily,
            NULL
        );
        return -1;
    }

    json_t *kw_tranger = json_pack("{s:s, s:s, s:b, s:I}",
        "path", path,
        "database", database,
        "master", 1,
        "subscriber", (json_int_t)(uintptr_t)gobj
    );

    char name[NAME_MAX];
    snprintf(name, sizeof(name), "tranger_%s", gobj_name(gobj));
    priv->gobj_tranger = gobj_create_service(name, C_TRANGER, kw_tranger, gobj);
    if(!priv->gobj_tranger) {
        // Error already logged
        return -1;
    }
    gobj_start(priv->gobj_tranger);

    priv->tranger = gobj_read_pointer_attr(priv->gobj_tranger, "tranger");
    if(!priv->tranger) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TRANGER,
            "msg",          "%s", "C_TRANGER gave no tranger handle",
            NULL
        );
        return -1;
    }

    /*
     *  The primary key is the date, so a re-run of a day appends a second
     *  record under the same key and the newest one is the answer. That is
     *  what makes report-day repeatable without a delete first.
     *
     *  tranger2_create_topic is idempotent: with the topic already on disk
     *  it just opens it.
     */
    json_t *topic = tranger2_create_topic(
        priv->tranger,
        priv->topic_daily,
        "date",                 // pkey
        "",                     // tkey, the append time is enough
        0,                      // jn_topic_ext
        sf_string_key,
        0,                      // jn_cols
        0                       // jn_var
    );
    if(!topic) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *  Close the store.
 ***************************************************************************/
PRIVATE int close_store(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->gobj_tranger) {
        return 0;
    }

    gobj_stop(priv->gobj_tranger);
    EXEC_AND_RESET(gobj_destroy, priv->gobj_tranger);
    priv->tranger = 0;

    return 0;
}

/***************************************************************************
 *  Append the record of the run to the topic.
 ***************************************************************************/
PRIVATE int store_report(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->tranger || !priv->jn_report) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TRANGER,
            "msg",          "%s", "No store or no report, the day was not saved",
            "date",         "%s", priv->target_date,
            NULL
        );
        return -1;
    }

    md2_record_ex_t md_record_ex;
    int ret = tranger2_append_record(
        priv->tranger,
        priv->topic_daily,
        0,                      // __t__, now
        0,                      // user_flag
        &md_record_ex,
        json_incref(priv->jn_report)    // owned
    );
    if(ret < 0) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *  The stored record of a day, or NULL.
 *
 *  Read backward and take one: a day reported more than once has more than
 *  one record under its key, and the newest is the answer.
 ***************************************************************************/
PRIVATE json_t *load_report(hgobj gobj, const char *date)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->tranger || empty_string(date)) {
        return NULL;
    }

    json_t *iterator = tranger2_open_iterator(
        priv->tranger,
        priv->topic_daily,
        date,
        0,          // match_cond
        NULL,       // load_record_callback
        "",         // iterator_id
        gobj_name(gobj),
        NULL,       // data
        0           // extra
    );
    if(!iterator) {
        return NULL;            // the key does not exist
    }

    /*
     *  Take the LAST row by asking for its rowid, not by asking for the
     *  first row backward: from_rowid is a position among the rows the
     *  iterator returns and `backward` does not turn it into a position
     *  from the end. (1, 1, TRUE) hands back row 1 -- the OLDEST -- so a
     *  day reported twice answered for ever with its first version, and a
     *  re-run to correct a day changed nothing that anybody could read.
     */
    size_t rows = tranger2_iterator_size(iterator);
    json_t *record = NULL;

    if(rows > 0) {
        json_t *page = tranger2_iterator_get_page(
            priv->tranger, iterator, (json_int_t)rows, 1, FALSE
        );
        record = json_incref(json_array_get(json_object_get(page, "data"), 0));
        JSON_DECREF(page)
    }

    tranger2_close_iterator(priv->tranger, iterator);

    return record;
}

/***************************************************************************
 *  Drop the days older than keep_days.
 *
 *  Compared as strings: an ISO date sorts the same as it reads, which is
 *  the reason the key has that shape.
 ***************************************************************************/
PRIVATE int prune_store(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->tranger) {
        return -1;              // Error already logged where it was opened
    }

    json_int_t keep_days = gobj_read_integer_attr(gobj, "keep_days");
    if(keep_days <= 0) {
        return 0;               // keeping everything is a valid choice
    }

    char oldest[DATE_SIZE];
    if(date_of(gobj, time(NULL) - keep_days*24*60*60, oldest, sizeof(oldest)) < 0) {
        // Error already logged
        return -1;
    }

    json_t *jn_keys = tranger2_list_keys(priv->tranger, priv->topic_daily);

    int dropped = 0;
    size_t idx;
    json_t *jn_key;
    json_array_foreach(jn_keys, idx, jn_key) {
        const char *key = json_string_value(jn_key);
        if(empty_string(key)) {
            continue;
        }
        if(strcmp(key, oldest) < 0) {
            if(tranger2_delete_key(priv->tranger, priv->topic_daily, key) == 0) {
                dropped++;
            }
            // Error already logged
        }
    }
    JSON_DECREF(jn_keys)

    if(dropped > 0) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Dropped days older than keep_days",
            "dropped",      "%d", dropped,
            "oldest",       "%s", oldest,
            NULL
        );
    }

    return 0;
}

/***************************************************************************
 *  The handful of numbers a reader compares from one day to the next.
 ***************************************************************************/
PRIVATE json_t *headline_of(hgobj gobj, json_t *record)
{
    if(!record) {
        return NULL;
    }

    json_t *totals = json_object_get(record, "totals");
    json_t *status = totals? json_object_get(totals, "status") : NULL;
    json_t *probes = json_object_get(record, "probes");
    json_t *errors = json_object_get(record, "errors");
    json_t *summary = json_object_get(record, "latency_summary");

    return json_pack("{s:I, s:I, s:I, s:I, s:I, s:I, s:I, s:f}",
        "requests", kw_get_int(gobj, totals, "requests", 0, 0),
        "bytes", kw_get_int(gobj, totals, "bytes", 0, 0),
        "clients", kw_get_int(gobj, totals, "clients", 0, 0),
        "4xx", kw_get_int(gobj, status, "4xx", 0, 0),
        "5xx", kw_get_int(gobj, status, "5xx", 0, 0),
        "probes", kw_get_int(gobj, probes, "requests", 0, 0),
        "errors", kw_get_int(gobj, errors, "total", 0, 0),
        "p95", summary? json_number_value(json_object_get(summary, "p95")) : 0.0
    );
}

/***************************************************************************
 *  The median of a list of numbers.
 ***************************************************************************/
PRIVATE int cmp_doubles(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;

    if(da < db) {
        return -1;
    }
    if(da > db) {
        return 1;
    }

    return 0;
}

PRIVATE double median_of(double *v, size_t n)
{
    if(n == 0) {
        return 0;
    }

    qsort(v, n, sizeof(double), cmp_doubles);

    if(n % 2) {
        return v[n/2];
    }

    return (v[n/2 - 1] + v[n/2]) / 2.0;
}

/***************************************************************************
 *  Put the previous day and the median of the week beside today's numbers.
 *
 *  This is what the store is FOR. A number with nothing to compare it to
 *  carries no information, and a daily mail of bare totals is one nobody
 *  opens twice.
 *
 *  A day with no history says so instead of comparing against zero, which
 *  would read as "everything doubled" on the first morning.
 ***************************************************************************/
PRIVATE void compare_with_history(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    if(!strptime(priv->target_date, "%Y-%m-%d", &tm)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Cannot read back the target date",
            "date",         "%s", priv->target_date,
            NULL
        );
        return;
    }
    tm.tm_hour = 12;                // noon, so a DST day cannot shift the date
    tm.tm_isdst = -1;
    time_t target = mktime(&tm);
    if(target == (time_t)-1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "mktime() FAILED on the target date",
            "date",         "%s", priv->target_date,
            NULL
        );
        return;
    }

    json_t *jn_changed = json_object();

    double requests[7], errors[7], p95[7];
    size_t n = 0;

    for(int back=1; back<=7; back++) {
        char date[DATE_SIZE];
        if(date_of(gobj, target - back*24*60*60, date, sizeof(date)) < 0) {
            continue;               // Error already logged
        }

        json_t *record = load_report(gobj, date);
        if(!record) {
            continue;               // a day with no report is not an error
        }

        json_t *headline = headline_of(gobj, record);
        if(back == 1) {
            json_object_set(jn_changed, "previous", headline);
        }

        requests[n] = (double)kw_get_int(gobj, headline, "requests", 0, 0);
        errors[n] = (double)kw_get_int(gobj, headline, "errors", 0, 0);
        p95[n] = json_number_value(json_object_get(headline, "p95"));
        n++;

        JSON_DECREF(headline)
        JSON_DECREF(record)
    }

    json_object_set_new(jn_changed, "days_of_history", json_integer((json_int_t)n));

    if(n > 0) {
        json_object_set_new(jn_changed, "median_7d", json_pack("{s:I, s:I, s:f}",
            "requests", (json_int_t)median_of(requests, n),
            "errors", (json_int_t)median_of(errors, n),
            "p95", median_of(p95, n)
        ));
    }

    json_object_set_new(jn_changed, "today", headline_of(gobj, priv->jn_report));

    json_object_set_new(priv->jn_report, "changed", jn_changed);
}

/***************************************************************************
 *  Fold the counter maps of the run into the record.
 *
 *  Only the top of each map goes in. The maps themselves are dropped with
 *  the next run: what a day of traffic holds does not belong in a mail, and
 *  the rotated files are still there for anyone who wants the detail.
 ***************************************************************************/
PRIVATE void close_report(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->jn_report) {
        return;
    }

    json_t *jn_totals = kw_get_dict(gobj, priv->jn_report, "totals", 0, KW_REQUIRED);
    json_object_set_new(jn_totals, "clients",
        json_integer((json_int_t)json_object_size(priv->jn_clients))
    );

    json_t *jn_top = kw_get_dict(gobj, priv->jn_report, "top", 0, KW_REQUIRED);
    json_object_set_new(jn_top, "paths", top_of(priv->jn_paths, priv->top_n));
    json_object_set_new(jn_top, "not_found", top_of(priv->jn_not_found, priv->top_n));
    json_object_set_new(jn_top, "referrers", top_of(priv->jn_referrers, priv->top_n));
    json_object_set_new(jn_top, "agents", top_of(priv->jn_agents, priv->top_n));
    json_object_set_new(jn_top, "clients", top_of(priv->jn_clients, priv->top_n));

    json_t *jn_probes = kw_get_dict(gobj, priv->jn_report, "probes", 0, KW_REQUIRED);
    json_object_set_new(jn_probes, "clients",
        json_integer((json_int_t)json_object_size(priv->jn_probe_clients))
    );
    json_object_set_new(jn_probes, "top_patterns",
        top_of(priv->jn_probe_patterns, priv->top_n)
    );
    json_object_set_new(jn_probes, "top_clients",
        top_of(priv->jn_probe_clients, priv->top_n)
    );

    /*
     *  Per vhost, with the percentiles read off its own histogram.
     */
    json_t *jn_by_vhost = json_object();
    const char *host;
    json_t *jn_vhost;
    json_object_foreach(priv->jn_vhosts, host, jn_vhost) {
        json_t *jn_latency = json_object_get(jn_vhost, "latency");
        json_object_set_new(jn_vhost, "latency_summary", latency_percentiles(jn_latency));
        json_object_set(jn_by_vhost, host, jn_vhost);
    }
    json_object_set_new(priv->jn_report, "by_vhost", jn_by_vhost);

    json_object_set_new(priv->jn_report, "latency_summary",
        latency_percentiles(kw_get_dict(gobj, priv->jn_report, "latency", 0, KW_REQUIRED))
    );

    /*
     *  Error signatures, most seen first. The sample is one whole line, so
     *  the reader sees the real thing and not only the shape of it.
     */
    json_t *jn_counts = json_object();
    const char *signature;
    json_t *jn_signature;
    json_object_foreach(priv->jn_signatures, signature, jn_signature) {
        json_object_set(jn_counts, signature, json_object_get(jn_signature, "count"));
    }

    json_t *jn_ranked = top_of(jn_counts, priv->top_n);
    JSON_DECREF(jn_counts)

    json_t *jn_by_signature = json_array();
    size_t idx;
    json_t *jn_row;
    json_array_foreach(jn_ranked, idx, jn_row) {
        const char *key = kw_get_str(gobj, jn_row, "key", "", 0);
        json_t *jn_detail = json_object_get(priv->jn_signatures, key);
        if(!jn_detail) {
            continue;
        }
        json_array_append_new(jn_by_signature, json_pack("{s:s, s:I, s:s, s:s, s:s}",
            "signature", key,
            "count", kw_get_int(gobj, jn_detail, "count", 0, 0),
            "first", kw_get_str(gobj, jn_detail, "first", "", 0),
            "last", kw_get_str(gobj, jn_detail, "last", "", 0),
            "sample", kw_get_str(gobj, jn_detail, "sample", "", 0)
        ));
    }
    JSON_DECREF(jn_ranked)

    json_t *jn_errors = kw_get_dict(gobj, priv->jn_report, "errors", 0, KW_REQUIRED);
    json_object_set_new(jn_errors, "distinct",
        json_integer((json_int_t)json_object_size(priv->jn_signatures))
    );
    json_object_set_new(jn_errors, "by_signature", jn_by_signature);
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
    priv->jn_report = json_pack("{s:s, s:i, s:s, s:I, s:[], s:[], s:[], s:{}}",
        "date", date,
        "version", 1,
        "node", get_hostname(),
        "generated_at", (json_int_t)time(NULL),
        "sources",
        "truncated",
        "server_errors",
        "top"
    );
    json_object_set_new(priv->jn_report, "totals", json_pack("{s:I, s:I, s:I, s:{}}",
        "requests", (json_int_t)0,
        "bytes", (json_int_t)0,
        "clients", (json_int_t)0,
        "status"
    ));
    json_object_set_new(priv->jn_report, "errors", json_pack("{s:I, s:[]}",
        "total", (json_int_t)0,
        "by_signature"
    ));
    json_object_set_new(priv->jn_report, "probes", json_pack("{s:I, s:I, s:[], s:[]}",
        "requests", (json_int_t)0,
        "clients", (json_int_t)0,
        "top_patterns",
        "top_clients"
    ));
    json_object_set_new(priv->jn_report, "latency", new_latency());
    json_object_set_new(priv->jn_report, "by_vhost", json_object());

    json_t *jn_by_hour = json_array();
    for(int h=0; h<24; h++) {
        json_array_append_new(jn_by_hour, json_integer(0));
    }
    json_object_set_new(priv->jn_report, "by_hour", jn_by_hour);

    /*
     *  The counter maps of the run, empty.
     */
    JSON_DECREF(priv->jn_clients)
    JSON_DECREF(priv->jn_paths)
    JSON_DECREF(priv->jn_not_found)
    JSON_DECREF(priv->jn_referrers)
    JSON_DECREF(priv->jn_agents)
    JSON_DECREF(priv->jn_probe_clients)
    JSON_DECREF(priv->jn_probe_patterns)
    JSON_DECREF(priv->jn_signatures)
    JSON_DECREF(priv->jn_vhosts)
    JSON_DECREF(priv->jn_probe_list)

    priv->jn_clients = json_object();
    priv->jn_paths = json_object();
    priv->jn_not_found = json_object();
    priv->jn_referrers = json_object();
    priv->jn_agents = json_object();
    priv->jn_probe_clients = json_object();
    priv->jn_probe_patterns = json_object();
    priv->jn_signatures = json_object();
    priv->jn_vhosts = json_object();

    priv->jn_probe_list = json_array();
    json_t *jn_configured = gobj_read_json_attr(gobj, "probe_patterns");
    if(json_array_size(jn_configured) > 0) {
        json_array_extend(priv->jn_probe_list, jn_configured);
    } else {
        for(int i=0; default_probe_patterns[i]; i++) {
            json_array_append_new(priv->jn_probe_list,
                json_string(default_probe_patterns[i])
            );
        }
    }

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
    priv->cur_kept = 0;
    priv->cur_unparsed = 0;
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

    close_report(gobj);
    compare_with_history(gobj);

    if(gobj_trace_level(gobj) & TRACE_REPORT) {
        gobj_trace_json(gobj, priv->jn_report, "webstats: report of %s", priv->target_date);
    }

    store_report(gobj);     // Error already logged
    prune_store(gobj);      // Error already logged

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
                priv->cur_kept++;
                accumulate_access_line(gobj, line);      // counts its own failures
            }
        } else {
            if(error_line_is_of_day(line, priv->target_date)) {
                priv->cur_kept++;
                accumulate_error_line(gobj, line);       // counts its own failures
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
    json_array_append_new(jn_sources, json_pack("{s:s, s:I, s:I, s:I, s:I, s:I}",
        "file", kw_get_str(gobj, kw, "path", "", 0),
        "lines", kw_get_int(gobj, kw, "lines", 0, 0),
        "kept", (json_int_t)priv->cur_kept,
        "unparsed", (json_int_t)priv->cur_unparsed,
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
