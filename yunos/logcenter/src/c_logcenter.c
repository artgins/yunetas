/***********************************************************************
 *          C_LOGCENTER.C
 *          Logcenter GClass.
 *
 *          Log Center
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <syslog.h>
#include "c_logcenter.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define MAX_ROTATORYFILE_SIZE   600         /* multiply by 1024L*1024L */
#define ROTATORY_BUFFER_SIZE    10          /* multiply by 1024L*1024L */
#define MIN_FREE_DISK           20          /* % percent */
#define MIN_FREE_MEM            20          /* % percent */
#define PATHBUFLEN              512

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    DL_ITEM_FIELDS
    const char *host;
    const char *app;
    hrotatory_t hrot;
} rot_item;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int cb_newfile(void *user_data, const char *old_filename, const char *new_filename);
PRIVATE json_t *make_summary(hgobj gobj, BOOL show_internal_errors);
PRIVATE int send_summary(hgobj gobj, GBUFFER *gbuf);
PRIVATE int do_log_stats(hgobj gobj, int priority, json_t* kw);
PRIVATE int reset_counters(hgobj gobj);
PRIVATE int trunk_data_log_file(hgobj gobj);
PRIVATE json_t *search_log_message(hgobj gobj, const char *text, uint32_t maxcount);
PRIVATE json_t *tail_log_message(hgobj gobj, uint32_t lines);
PRIVATE const char *_get_hostname(void);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
#define MAX_PRIORITY_COUNTER 12
PRIVATE uint64_t priority_counter[MAX_PRIORITY_COUNTER];


PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_display_summary(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_send_summary(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_enable_send_summary(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_disable_send_summary(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_reset_counters(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_search(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_tail(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_restart_yuneta(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_search[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_OCTET_STR, "text",         0,              0,          "Text to search."),
SDATAPM (ASN_OCTET_STR, "maxcount",     0,              0,          "Max count of items to search. Default: -1."),
SDATA_END()
};
PRIVATE sdata_desc_t pm_tail[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_OCTET_STR, "lines",        0,              0,          "Lines to output. Default: 100."),
SDATA_END()
};
PRIVATE sdata_desc_t pm_restart_yuneta[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_BOOLEAN,   "enable",       0,              0,          "Set 1 to enable restart yuneta on queue alarm"),
SDATAPM (ASN_UNSIGNED,  "timeout_restart_yuneta",       0,          0, "Timeout between restarts in seconds"),
SDATAPM (ASN_UNSIGNED,  "queue_restart_limit",          0,          0, "Restart yuneta only if queue size is greater than 'queue_restart_limit'"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_OCTET_STR, "cmd",          0,              0,          "command about you want help."),
SDATAPM (ASN_UNSIGNED,  "level",        0,              0,          "command search level in childs"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (ASN_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,       "Command's help"),
SDATACM (ASN_SCHEMA,    "display-summary",  0,                  0,              cmd_display_summary, "Display the summary report."),
SDATACM (ASN_SCHEMA,    "send-summary",     0,                  0,              cmd_send_summary, "Send by email the summary report."),
SDATACM (ASN_SCHEMA,    "enable-send-summary", 0,               0,          cmd_enable_send_summary, "Enable send by email the summary report."),
SDATACM (ASN_SCHEMA,    "disable-send-summary", 0,              0,          cmd_disable_send_summary, "Disable send by email the summary report."),
SDATACM (ASN_SCHEMA,    "send-summary",     0,                  0,              cmd_send_summary, "Send by email the summary report."),
SDATACM (ASN_SCHEMA,    "reset-counters",   0,                  0,              cmd_reset_counters, "Reset counters."),
SDATACM (ASN_SCHEMA,    "search",           0,                  pm_search,      cmd_search,     "Search in log messages."),
SDATACM (ASN_SCHEMA,    "tail",             0,                  pm_tail,        cmd_tail,       "output the last part of log messages."),
SDATACM (ASN_SCHEMA,    "restart-yuneta-on-queue-alarm",        0,              pm_restart_yuneta, cmd_restart_yuneta,       "Enable or disable restart-yuneta on queue alarm"),
SDATA_END()
};


/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name--------------------flag------------------------default---------description---------- */
SDATA (ASN_OCTET_STR,   "url",                  SDF_RD|SDF_REQUIRED, "udp://127.0.0.1:1992", "url of udp server"),
SDATA (ASN_OCTET_STR,   "from",                 SDF_WR|SDF_PERSIST|SDF_REQUIRED, "", "from email field"),
SDATA (ASN_OCTET_STR,   "to",                   SDF_WR|SDF_PERSIST|SDF_REQUIRED, "", "to email field"),
SDATA (ASN_OCTET_STR,   "subject",              SDF_WR|SDF_PERSIST|SDF_REQUIRED, "Log Center Summary", "subject email field"),
SDATA (ASN_OCTET_STR,   "log_filename",         SDF_WR, "W.log", "Log filename. Available mask: DD/MM/CCYY-W-ZZZ"),
SDATA (ASN_UNSIGNED64,  "max_rotatoryfile_size",SDF_WR|SDF_PERSIST|SDF_REQUIRED, MAX_ROTATORYFILE_SIZE, "Maximum log files size (in Megas)"),
SDATA (ASN_UNSIGNED64,  "rotatory_bf_size",     SDF_WR|SDF_PERSIST|SDF_REQUIRED, ROTATORY_BUFFER_SIZE, "Buffer size of rotatory (in Megas)"),
SDATA (ASN_INTEGER,     "min_free_disk",        SDF_WR|SDF_PERSIST|SDF_REQUIRED, MIN_FREE_DISK, "Minimun free percent disk"),
SDATA (ASN_INTEGER,     "min_free_mem",         SDF_WR|SDF_PERSIST|SDF_REQUIRED, MIN_FREE_MEM, "Minimun free percent memory"),

SDATA (ASN_BOOLEAN,     "restart_on_alarm",     SDF_PERSIST|SDF_WR,             FALSE, "If true the logcenter will execute 'restart_yuneta_command' after receive a queue alarm. Next restart will not execute until 'timeout_restart_yuneta' has pass"),
SDATA (ASN_OCTET_STR,   "restart_yuneta_command", SDF_WR|SDF_PERSIST,           "/yuneta/bin/yshutdown -s; sleep 1; /yuneta/agent/yuneta_agent --start --config-file=/yuneta/agent/yuneta_agent.json", "Restart yuneta command"),
SDATA (ASN_UNSIGNED,    "timeout_restart_yuneta",SDF_PERSIST|SDF_WR,        1*60*60, "Timeout between restarts in seconds"),
SDATA (ASN_UNSIGNED,    "queue_restart_limit",  SDF_PERSIST|SDF_WR,        0, "Restart yuneta when queue size is greater"),

SDATA (ASN_BOOLEAN,     "send_summary_disabled",SDF_PERSIST|SDF_WR,         FALSE, "If true then disable send summary"),
SDATA (ASN_INTEGER,     "timeout",              SDF_RD,  1*1000, "Timeout"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_USER = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"trace_user",        "Trace user description"},
{0, 0},
};


/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int32_t timeout;
    hrotatory_t global_rotatory;
    const char *log_filename;

    hgobj timer;
    hgobj gobj_gss_udp_s;

    json_t *global_alerts;
    json_t *global_criticals;
    json_t *global_errors;
    json_t *global_warnings;
    json_t *global_infos;

    time_t warn_free_disk;
    time_t warn_free_mem;
    int last_disk_free_percent;
    int last_mem_free_percent;

    BOOL restart_on_alarm;
    uint32_t timeout_restart_yuneta;
    uint32_t queue_restart_limit;
    time_t t_restart;
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

    priv->timer = gobj_create("", GCLASS_TIMER, 0, gobj);
    json_t *kw_gss_udps = json_pack("{s:s}",
        "url", gobj_read_str_attr(gobj, "url")
    );
    priv->gobj_gss_udp_s = gobj_create("", GCLASS_GSS_UDP_S0, kw_gss_udps, gobj);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,                   gobj_read_int32_attr)
    SET_PRIV(restart_on_alarm,          gobj_read_bool_attr)
    SET_PRIV(log_filename,              gobj_read_str_attr)
    SET_PRIV(timeout_restart_yuneta,    gobj_read_uint32_attr)
    SET_PRIV(queue_restart_limit,       gobj_read_uint32_attr)

    priv->global_alerts = json_object();
    priv->global_criticals = json_object();
    priv->global_errors = json_object();
    priv->global_warnings = json_object();
    priv->global_infos = json_object();

    priv->warn_free_disk = start_sectimer(10);
    priv->warn_free_mem = start_sectimer(10);
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout,                     gobj_read_int32_attr)
    ELIF_EQ_SET_PRIV(timeout_restart_yuneta,    gobj_read_uint32_attr)
        if(priv->timeout_restart_yuneta == 0) {
            priv->t_restart = 0;
        }
    ELIF_EQ_SET_PRIV(queue_restart_limit,       gobj_read_uint32_attr)
    ELIF_EQ_SET_PRIV(restart_on_alarm,          gobj_read_bool_attr)
        if(priv->restart_on_alarm == 0) {
            priv->t_restart = 0;
        }
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_decref(priv->global_alerts);
    json_decref(priv->global_criticals);
    json_decref(priv->global_errors);
    json_decref(priv->global_warnings);
    json_decref(priv->global_infos);
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
 *  Yuneta rule:
 *  If service has mt_play then start only the service gobj.
 *      (Let mt_play be responsible to start their tree)
 *  If service has not mt_play then start the tree with gobj_start_tree().
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);

    if(!empty_string(priv->log_filename)) {
        char destination[PATHBUFLEN];
        yuneta_realm_file(
            destination,
            sizeof(destination),
            "logs",
            priv->log_filename,
            TRUE
        );
        priv->global_rotatory = rotatory_open(
            destination,
            gobj_read_uint64_attr(gobj, "rotatory_bf_size") * 1024L*1024L,
            gobj_read_uint64_attr(gobj, "max_rotatoryfile_size"),
            gobj_read_int32_attr(gobj, "min_free_disk"),
            yuneta_xpermission(),   // permission for directories and executable files. 0 = default 02775
            yuneta_rpermission(),   // permission for regular files. 0 = default 0664
            FALSE
        );
        if(priv->global_rotatory) {
            log_info(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Open global rotatory",
                "path",         "%s", destination,
                NULL
            );
            rotatory_subscribe2newfile(
                priv->global_rotatory,
                cb_newfile,
                gobj
            );
            log_add_handler("logcenter", "file", LOG_OPT_ALL, priv->global_rotatory);
        }
    }

    set_timeout_periodic(priv->timer, priv->timeout);
    if(priv->gobj_gss_udp_s) {
        gobj_start(priv->gobj_gss_udp_s);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    log_del_handler("logcenter");

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    if(priv->gobj_gss_udp_s) {
        gobj_stop(priv->gobj_gss_udp_s);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method stats
 ***************************************************************************/
PRIVATE json_t *mt_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src)
{
    json_t *jn_stats;

    if(stats && strcmp(stats, "__reset__")==0) {
        reset_counters(gobj);
        trunk_data_log_file(gobj);
    }

    if(stats && strstr(stats, "internal")) {
        jn_stats = json_pack("{s:I, s:I, s:I, s:I, s:I, s:I, s:I, s:I}",
            "Alert",    (json_int_t)(size_t)gobj_read_uint32_attr(gobj_yuno(), "log_alerts"),
            "Critical", (json_int_t)(size_t)gobj_read_uint32_attr(gobj_yuno(), "log_criticals"),
            "Error",    (json_int_t)(size_t)gobj_read_uint32_attr(gobj_yuno(), "log_errors"),
            "Warning",  (json_int_t)(size_t)gobj_read_uint32_attr(gobj_yuno(), "log_warnings"),
            "Info",     (json_int_t)(size_t)gobj_read_uint32_attr(gobj_yuno(), "log_infos"),
            "Debug",    (json_int_t)(size_t)gobj_read_uint32_attr(gobj_yuno(), "log_debugs"),
            "Audit",    (json_int_t)(size_t)gobj_read_uint32_attr(gobj_yuno(), "log_audits"),
            "Monitor",  (json_int_t)(size_t)gobj_read_uint32_attr(gobj_yuno(), "log_monitors")
        );
    } else {
        jn_stats = json_pack("{s:I, s:I, s:I, s:I, s:I, s:I, s:I, s:I}",
            "Alert",    (json_int_t)priority_counter[LOG_ALERT],
            "Critical", (json_int_t)priority_counter[LOG_CRIT],
            "Error",    (json_int_t)priority_counter[LOG_ERR],
            "Warning",  (json_int_t)priority_counter[LOG_WARNING],
            "Info",     (json_int_t)priority_counter[LOG_INFO],
            "Debug",    (json_int_t)priority_counter[LOG_DEBUG],
            "Audit",    (json_int_t)priority_counter[LOG_AUDIT],
            "Monitor",  (json_int_t)priority_counter[LOG_MONITOR]
        );
    }

    append_yuno_metadata(gobj, jn_stats, stats);

    return msg_iev_build_webix(
        gobj,
        0,
        0,
        0,
        jn_stats,
        kw  // owned
    );
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
    return msg_iev_build_webix(
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
PRIVATE json_t *cmd_display_summary(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_summary = make_summary(gobj, TRUE);
    return msg_iev_build_webix(
        gobj,
        0,
        0,
        0,
        jn_summary,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_send_summary(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    char fecha[90];
    current_timestamp(fecha, sizeof(fecha));

    json_t *jn_summary = make_summary(gobj, FALSE);
    GBUFFER *gbuf_summary = gbuf_create(32*1024, MIN(1*1024*1024L, gbmem_get_maximum_block()), 0, codec_utf_8);
    gbuf_printf(gbuf_summary, "From %s (%s, %s)\nat %s, \n\n",
        _get_hostname(),
        node_uuid(),
        __yuneta_version__,
        fecha
    );
    json2gbuf(gbuf_summary, jn_summary, JSON_INDENT(4));
    gbuf_printf(gbuf_summary, "\r\n");
    send_summary(gobj, gbuf_summary);

    return msg_iev_build_webix(
        gobj,
        0,
        json_sprintf("Summary report sent by email to %s", gobj_read_str_attr(gobj, "to")),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_enable_send_summary(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    gobj_write_bool_attr(gobj, "send_summary_disabled", FALSE);
    gobj_save_persistent_attrs(gobj, json_string("send_summary_disabled"));

    return msg_iev_build_webix(
        gobj,
        0,
        json_sprintf("Send summary enabled"),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_disable_send_summary(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    gobj_write_bool_attr(gobj, "send_summary_disabled", TRUE);
    gobj_save_persistent_attrs(gobj, json_string("send_summary_disabled"));

    return msg_iev_build_webix(
        gobj,
        0,
        json_sprintf("Send summary disabled"),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_reset_counters(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    reset_counters(gobj);
    trunk_data_log_file(gobj);
    return cmd_display_summary(gobj, "", kw, src);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_search(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *maxcount_ = kw_get_str(kw, "maxcount", "", 0);
    uint32_t maxcount = atoi(maxcount_);
    if(maxcount <= 0) {
        maxcount = -1;
    }
    const char *text = kw_get_str(kw, "text", 0, 0);
    if(empty_string(text)) {
        return msg_iev_build_webix(
            gobj,
            -1,
            json_sprintf("What text?"),
            0,
            0,
            kw  // owned
        );
    }
    json_t *jn_log_msg = search_log_message(gobj, text, maxcount);
    return msg_iev_build_webix(
        gobj,
        0,
        0,
        0,
        jn_log_msg,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_tail(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *lines_ = kw_get_str(kw, "lines", "", 0);
    uint32_t lines = atoi(lines_);
    if(lines <= 0) {
        lines = 100;
    }
    json_t *jn_log_msg = tail_log_message(gobj, lines);
    return msg_iev_build_webix(
        gobj,
        0,
        0,
        0,
        jn_log_msg,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_restart_yuneta(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int enable = kw_get_int(kw, "enable", -1, KW_WILD_NUMBER);
    int timeout_restart_yuneta = kw_get_int(kw, "timeout_restart_yuneta", -1, KW_WILD_NUMBER);
    int queue_restart_limit = kw_get_int(kw, "queue_restart_limit", -1, KW_WILD_NUMBER);

    if(enable == -1) {
        return msg_iev_build_webix(
            gobj,
            -1,
            json_sprintf("What enable?.\nNow enable restart_yuneta_on_queue_alarm: %s, timeout_restart_yuneta: %d seconds, queue_restart_limit: %d",
                priv->restart_on_alarm?"YES":"NO",
                priv->timeout_restart_yuneta,
                priv->queue_restart_limit
            ),
            0,
            0,
            kw  // owned
        );
    }

    if(timeout_restart_yuneta != -1) {
        if(timeout_restart_yuneta < 1*60*60) {
            return msg_iev_build_webix(
                gobj,
                -1,
                json_sprintf("timeout_restart_yuneta must be >= 3600 (1 hour).\nNow enable restart_yuneta_on_queue_alarm: %s, timeout_restart_yuneta: %d seconds, queue_restart_limit: %d",
                    priv->restart_on_alarm?"YES":"NO",
                    priv->timeout_restart_yuneta,
                    priv->queue_restart_limit
                ),
                0,
                0,
                kw  // owned
            );
        }
    }

    if(queue_restart_limit != -1) {
        if(queue_restart_limit < 10000) {
            return msg_iev_build_webix(
                gobj,
                -1,
                json_sprintf("queue_restart_limit must be >= 10000 (1 hour).\nNow enable restart_yuneta_on_queue_alarm: %s, timeout_restart_yuneta: %d seconds, queue_restart_limit: %d",
                    priv->restart_on_alarm?"YES":"NO",
                    priv->timeout_restart_yuneta,
                    priv->queue_restart_limit
                ),
                0,
                0,
                kw  // owned
            );
        }
    }

    gobj_write_bool_attr(gobj, "restart_on_alarm", enable?TRUE:FALSE);
    gobj_save_persistent_attrs(gobj, json_string("restart_on_alarm"));

    if(timeout_restart_yuneta != -1) {
        gobj_write_uint32_attr(gobj, "timeout_restart_yuneta", timeout_restart_yuneta);
        gobj_save_persistent_attrs(gobj, json_string("timeout_restart_yuneta"));
    }

    if(queue_restart_limit != -1) {
        gobj_write_uint32_attr(gobj, "queue_restart_limit", queue_restart_limit);
        gobj_save_persistent_attrs(gobj, json_string("queue_restart_limit"));
    }

    return msg_iev_build_webix(
        gobj,
        0,
        json_sprintf("Enable restart_yuneta_on_queue_alarm: %s, timeout_restart_yuneta: %d seconds, queue_restart_limit: %d",
             enable?"YES":"NO",
             priv->timeout_restart_yuneta,
             priv->queue_restart_limit
        ),
        0,
        0,
        kw  // owned
    );
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int trunk_data_log_file(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(priv->global_rotatory) {
        rotatory_trunk(priv->global_rotatory);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int reset_counters(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    memset(priority_counter, 0, sizeof(priority_counter));
    gobj_write_uint32_attr(gobj_yuno(), "log_alerts", 0);
    gobj_write_uint32_attr(gobj_yuno(), "log_criticals", 0);
    gobj_write_uint32_attr(gobj_yuno(), "log_errors", 0);
    gobj_write_uint32_attr(gobj_yuno(), "log_warnings", 0);
    gobj_write_uint32_attr(gobj_yuno(), "log_infos", 0);
    gobj_write_uint32_attr(gobj_yuno(), "log_debugs", 0);
    gobj_write_uint32_attr(gobj_yuno(), "log_audits", 0);
    gobj_write_uint32_attr(gobj_yuno(), "log_monitors", 0);

    json_object_clear(priv->global_alerts);
    json_object_clear(priv->global_criticals);
    json_object_clear(priv->global_errors);
    json_object_clear(priv->global_warnings);
    json_object_clear(priv->global_infos);

    return 0;
}

/***************************************************************************
 *  Envia correo a la direccion telegrafica configurada
 ***************************************************************************/
PRIVATE int send_email(
    hgobj gobj,
    const char *from,
    const char *to,
    const char *subject,
    GBUFFER *gbuf)
{
    hgobj gobj_emailsender = gobj_find_service("emailsender", FALSE);
    if(!gobj_emailsender) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SERVICE_ERROR,
            "msg",          "%s", "Service 'emailsender' not found",
            NULL
        );
        return -1;
    }
    json_t *kw_email = json_object();
    json_object_set_new(kw_email, "from", json_string(from));
    json_object_set_new(kw_email, "to", json_string(to));
    json_object_set_new(kw_email, "subject", json_string(subject));
    json_object_set_new(kw_email,
        "gbuffer",
        json_integer((json_int_t)(size_t)gbuf)
    );

    /*  TODO ??? commentario obsoleto
     *  Envia el mensaje al yuno emailsender
     *  Uso iev_send2 para persistencia.
     *  Si no lo hiciese tendría que recoger el retorno de iev_send()
     *  y si es negativo responder al host con ack negativo.
     */
    return gobj_send_event(gobj_emailsender, "EV_SEND_EMAIL", kw_email, gobj);
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE const char *_get_hostname(void)
{
    static char hostname[120 + 1] = {0};

    if(!*hostname)
        gethostname(hostname, sizeof(hostname)-1);
    return hostname;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send_summary(hgobj gobj, GBUFFER *gbuf)
{
    char subject[120];
    snprintf(subject, sizeof(subject), "%s: Log Center Summary", _get_hostname());
    return send_email(gobj,
        gobj_read_str_attr(gobj, "from"),
        gobj_read_str_attr(gobj, "to"),
        subject,
        gbuf
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *make_summary(hgobj gobj, BOOL show_internal_errors)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *jn_summary = json_object();

    if(show_internal_errors) {
        json_t *jn_internal_stats = json_pack("{s:I, s:I, s:I, s:I, s:I, s:I, s:I, s:I}",
            "Alert",    (json_int_t)(size_t)gobj_read_uint32_attr(gobj_yuno(), "log_alerts"),
            "Critical", (json_int_t)(size_t)gobj_read_uint32_attr(gobj_yuno(), "log_criticals"),
            "Error",    (json_int_t)(size_t)gobj_read_uint32_attr(gobj_yuno(), "log_errors"),
            "Warning",  (json_int_t)(size_t)gobj_read_uint32_attr(gobj_yuno(), "log_warnings"),
            "Info",     (json_int_t)(size_t)gobj_read_uint32_attr(gobj_yuno(), "log_infos"),
            "Debug",    (json_int_t)(size_t)gobj_read_uint32_attr(gobj_yuno(), "log_debugs"),
            "Audit",    (json_int_t)(size_t)gobj_read_uint32_attr(gobj_yuno(), "log_audits"),
            "Monitor",  (json_int_t)(size_t)gobj_read_uint32_attr(gobj_yuno(), "log_monitors")
        );
        json_object_set_new(jn_summary, "Internal Counters", jn_internal_stats);
    }

    json_t *jn_global_stats = json_pack("{s:I, s:I, s:I, s:I, s:I, s:I, s:I, s:I}",
        "Alert",    (json_int_t)priority_counter[LOG_ALERT],
        "Critical", (json_int_t)priority_counter[LOG_CRIT],
        "Error",    (json_int_t)priority_counter[LOG_ERR],
        "Warning",  (json_int_t)priority_counter[LOG_WARNING],
        "Info",     (json_int_t)priority_counter[LOG_INFO],
        "Debug",    (json_int_t)priority_counter[LOG_DEBUG],
        "Audit",    (json_int_t)priority_counter[LOG_AUDIT],
        "Monitor",  (json_int_t)priority_counter[LOG_MONITOR]
    );
    json_object_set_new(jn_summary, "Global Counters", jn_global_stats);
    json_object_set_new(jn_summary, "Node Owner", json_string(gobj_node_owner()));

    if(show_internal_errors) { // THE same but in different order
        if(priority_counter[LOG_INFO]) {
            json_object_set(jn_summary, "Global Infos", priv->global_infos);
        }
        if(priority_counter[LOG_WARNING]) {
            json_object_set(jn_summary, "Global Warnings", priv->global_warnings);
        }
        if(priority_counter[LOG_ERR]) {
            json_object_set(jn_summary, "Global Errors", priv->global_errors);
        }
        if(priority_counter[LOG_ALERT]) {
            json_object_set(jn_summary, "Global Alerts", priv->global_alerts);
        }
        if(priority_counter[LOG_CRIT]) {
            json_object_set(jn_summary, "Global Criticals", priv->global_criticals);
        }
    } else {
        if(priority_counter[LOG_CRIT]) {
            json_object_set(jn_summary, "Global Criticals", priv->global_criticals);
        }
        if(priority_counter[LOG_ALERT]) {
            json_object_set(jn_summary, "Global Alerts", priv->global_alerts);
        }
        if(priority_counter[LOG_ERR]) {
            json_object_set(jn_summary, "Global Errors", priv->global_errors);
        }
        if(priority_counter[LOG_WARNING]) {
            json_object_set(jn_summary, "Global Warnings", priv->global_warnings);
        }
        if(priority_counter[LOG_INFO]) {
            json_object_set(jn_summary, "Global Infos", priv->global_infos);
        }
    }


    return jn_summary;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send_report_email(hgobj gobj, BOOL reset)
{
    if(gobj_read_bool_attr(gobj, "send_summary_disabled")) {
        return 0;
    }

    /*
     *  Day changed, report errors
     */
    char fecha[90];
    current_timestamp(fecha, sizeof(fecha));

    json_t *jn_summary = make_summary(gobj, FALSE);
    GBUFFER *gbuf_summary = gbuf_create(32*1024, MIN(1*1024*1024L, gbmem_get_maximum_block()), 0, codec_utf_8);
    gbuf_printf(gbuf_summary, "From %s (%s, %s)\nat %s, Logcenter Summary:\n\n",
        _get_hostname(),
        node_uuid(),
        __yuneta_version__,
        fecha
    );
    json2gbuf(gbuf_summary, jn_summary, JSON_INDENT(4));
    gbuf_printf(gbuf_summary, "\n\n");

    /*
     *  Reset counters
     */
    if(reset) {
        reset_counters(gobj);
    }

    /*
     *  Send summary
     */
    send_summary(gobj, gbuf_summary);

    return 0;
}

/***************************************************************************
 *  old_filename can be null
 ***************************************************************************/
PRIVATE int cb_newfile(void *user_data, const char *old_filename, const char *new_filename)
{
    hgobj gobj = user_data;
    if(!empty_string(old_filename)) {
        send_report_email(gobj, TRUE);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int write2logs(hgobj gobj, int priority, GBUFFER *gbuf)
{
    char *bf = gbuf_cur_rd_pointer(gbuf);
    _log_bf(priority, 0, bf, strlen(bf));
    return 0;
}

/***************************************************************************
 *  Save log
 ***************************************************************************/
PRIVATE int do_log_stats(hgobj gobj, int priority, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *jn_dict = 0;

    switch(priority) {
        case LOG_ALERT:
            jn_dict = priv->global_alerts;
            break;
        case LOG_CRIT:
            jn_dict = priv->global_criticals;
            break;
        case LOG_ERR:
            jn_dict = priv->global_errors;
            break;
        case LOG_WARNING:
            jn_dict = priv->global_warnings;
            break;
        case LOG_INFO:
            jn_dict = priv->global_infos;
            break;
        default:
            return -1;
    }
    const char *msgset = kw_get_str(kw, "msgset",0, 0);
    const char *msg = kw_get_str(kw, "msg", 0, 0);
    if(!msgset || !msg) {
        return -1;
    }

    json_t *jn_set = kw_get_dict(jn_dict, msgset, json_object(), KW_CREATE);

/*
 *  TODO en vez estar harcoded que esté en config.
    "msg",          "%s", "path NOT FOUND, default value returned",
    "path",         "%s", path,

    "msg",          "%s", "GClass Attribute NOT FOUND",
    "gclass",       "%s", gobj_gclass_name(gobj),
    "attr",         "%s", attr,

    "msg",          "%s", "Publish event WITHOUT subscribers",
    "event",        "%s", event,
 */
    if(strncmp(msg, "path NOT FOUND", strlen("path NOT FOUND"))==0 ||
        strncmp(msg, "path MUST BE", strlen("path MUST BE"))==0
    ) {
        const char *path = kw_get_str(kw, "path", 0, 0);
        if(!empty_string(path)) {
            json_t *jn_level1 = kw_get_dict(jn_set, msg, json_object(), KW_CREATE);
            json_int_t counter = kw_get_int(jn_level1, path, 0, KW_CREATE);
            counter++;
            json_object_set_new(jn_level1, path, json_integer(counter));
        } else {
            json_int_t counter = kw_get_int(jn_set, msg, 0, KW_CREATE);
            counter++;
            json_object_set_new(jn_set, msg, json_integer(counter));
        }

    } else if(strcmp(msg, "GClass Attribute NOT FOUND")==0) {
        const char *attr = kw_get_str(kw, "attr", 0, 0);
        if(!empty_string(attr)) {
            json_t *jn_level1 = kw_get_dict(jn_set, msg, json_object(), KW_CREATE);
            json_int_t counter = kw_get_int(jn_level1, attr, 0, KW_CREATE);
            counter++;
            json_object_set_new(jn_level1, attr, json_integer(counter));
        } else {
            json_int_t counter = kw_get_int(jn_set, msg, 0, KW_CREATE);
            counter++;
            json_object_set_new(jn_set, msg, json_integer(counter));
        }

    } else if(strcmp(msg, "Publish event WITHOUT subscribers")==0) {
        const char *event = kw_get_str(kw, "event", 0, 0);
        if(!empty_string(event)) {
            json_t *jn_level1 = kw_get_dict(jn_set, msg, json_object(), KW_CREATE);
            json_int_t counter = kw_get_int(jn_level1, event, 0, KW_CREATE);
            counter++;
            json_object_set_new(jn_level1, event, json_integer(counter));
        } else {
            json_int_t counter = kw_get_int(jn_set, msg, 0, KW_CREATE);
            counter++;
            json_object_set_new(jn_set, msg, json_integer(counter));
        }

    } else {
        json_int_t counter = kw_get_int(jn_set, msg, 0, KW_CREATE);
        counter++;
        json_object_set_new(jn_set, msg, json_integer(counter));
    }

    if(strcmp(msgset, MSGSET_QUEUE_ALARM)==0) {
        json_int_t queue_size = kw_get_int(kw, "queue_size", 10000, 0);
        if(priv->restart_on_alarm && priv->queue_restart_limit > 0 && queue_size >= priv->queue_restart_limit) {
            const char *restart_yuneta_command = gobj_read_str_attr(gobj, "restart_yuneta_command");
            if(priv->timeout_restart_yuneta) {
                if(priv->t_restart == 0) {
                    priv->t_restart = start_sectimer(priv->timeout_restart_yuneta);
                    int ret = system(restart_yuneta_command);
                    if(ret < 0) {
                        log_error(0,
                            "gobj",         "%s", gobj_full_name(gobj),
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                            "msg",          "%s", "system() FAILED",
                            NULL
                        );
                    }
                } else {
                    if(test_sectimer(priv->t_restart)) {
                        priv->t_restart = start_sectimer(priv->timeout_restart_yuneta);
                        int ret = system(restart_yuneta_command);
                        if(ret < 0) {
                            log_error(0,
                                "gobj",         "%s", gobj_full_name(gobj),
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                                "msg",          "%s", "system() FAILED",
                                NULL
                            );
                        }
                    }
                }
            }
        }
    }

    return 0;
}

/***************************************************************************
 *  Search text in some value of dict
 ***************************************************************************/
BOOL text_in_dict(json_t *jn_dict, const char *text)
{
    json_t *jn_value;
    const char *key;
    json_object_foreach(jn_dict, key, jn_value) {
        if(json_is_string(jn_value)) {
            const char *text_ = json_string_value(jn_value);
            if(!empty_string(text_)) {
                if(strstr(text_, text)) {
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

/***************************************************************************
 *  Extrae el json del fichero, fill a list of dicts.
 ***************************************************************************/
PRIVATE json_t *extrae_json(hgobj gobj, FILE *file, uint32_t maxcount, json_t *jn_list, const char *text)
{
    int currcount = 0;
    /*
     *  Load commands
     */
    #define WAIT_BEGIN_DICT 0
    #define WAIT_END_DICT   1
    int c;
    int st = WAIT_BEGIN_DICT;
    int brace_indent = 0;
    GBUFFER *gbuf = gbuf_create(4*1024, gbmem_get_maximum_block(), 0, 0);
    BOOL fin = FALSE;
    while(!fin && (c=fgetc(file))!=EOF) {
        switch(st) {
        case WAIT_BEGIN_DICT:
            if(c != '{') {
                continue;
            }
            gbuf_reset_wr(gbuf);
            gbuf_reset_rd(gbuf);
            if(gbuf_append(gbuf, &c, 1)<=0) {
                abort();
            }
            brace_indent = 1;
            st = WAIT_END_DICT;
            break;
        case WAIT_END_DICT:
            if(c == '{') {
                brace_indent++;
            } else if(c == '}') {
                brace_indent--;
            }
            if(gbuf_append(gbuf, &c, 1)<=0) {
                abort();
            }
            if(brace_indent == 0) {
                json_t *jn_dict = legalstring2json(gbuf_cur_rd_pointer(gbuf), FALSE);
                if(jn_dict) {
                    if(!text) {
                        // Tail
                        if(json_array_size(jn_list) >= maxcount) {
                            json_array_remove(jn_list, 0);
                        }
                        json_array_append_new(jn_list, jn_dict);
                    } else if(text_in_dict(jn_dict, text)) {
                        // Search
                        json_array_append_new(jn_list, jn_dict);
                        currcount++;
                        if(currcount >= maxcount) {
                            fin = TRUE;
                            break;
                        }
                    } else {
                        json_decref(jn_dict);
                    }
                } else {
                    //log_debug_gbuf("FAILED", gbuf);
                }
                st = WAIT_BEGIN_DICT;
            }
            break;
        }
    }
    gbuf_decref(gbuf);

    return jn_list;
}

/***************************************************************************
 *  Search text in log file
 ***************************************************************************/
PRIVATE json_t *search_log_message(hgobj gobj, const char *text, uint32_t maxcount)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_logmsgs = json_array();

    const char *path = rotatory_path(priv->global_rotatory);
    FILE *file = fopen(path, "r");
    if(!file) {
        json_object_set_new(jn_logmsgs, "msg", json_string("Cannot open log filename"));
        json_object_set_new(jn_logmsgs, "path", json_string(path));
        return jn_logmsgs;
    }

    extrae_json(gobj, file, maxcount, jn_logmsgs, text);

    fclose(file);
    return jn_logmsgs;
}

/***************************************************************************
 *  Tail last lines of log file
 ***************************************************************************/
PRIVATE json_t *tail_log_message(hgobj gobj, uint32_t lines)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_logmsgs = json_array();

    const char *path = rotatory_path(priv->global_rotatory);
    FILE *file = fopen(path, "r");
    if(!file) {
        json_object_set_new(jn_logmsgs, "msg", json_string("Cannot open log filename"));
        json_object_set_new(jn_logmsgs, "path", json_string(path));
        return jn_logmsgs;
    }

    extrae_json(gobj, file, lines, jn_logmsgs, 0);

    fclose(file);
    return jn_logmsgs;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send_warn_free_disk(hgobj gobj, int percent, int minimun)
{
    char bf[256];

    snprintf(bf, sizeof(bf), "Free disk (~%d%%) is TOO LOW (<%d%%)", percent, minimun);

    json_t *jn_value = json_pack("{s:s, s:s, s:s, s:s, s:i}",
        "gobj",         gobj_full_name(gobj),
        "function",     __FUNCTION__,
        "msgset",       MSGSET_SYSTEM_ERROR,
        "msg",          bf,
        "len",          percent
    );

    int priority = LOG_ALERT;
    priority_counter[priority]++;
    do_log_stats(gobj, priority, jn_value);
    json_decref(jn_value);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send_warn_free_mem(hgobj gobj, int percent)
{
    char bf[256];

    snprintf(bf, sizeof(bf), "Free mem (~%d%%) is TOO LOW", percent);

    json_t *jn_value = json_pack("{s:s, s:s, s:s, s:s, s:i}",
        "gobj",         gobj_full_name(gobj),
        "function",     __FUNCTION__,
        "msgset",       MSGSET_SYSTEM_ERROR,
        "msg",          bf,
        "len",          percent
    );

    int priority = LOG_ALERT;
    priority_counter[priority]++;
    do_log_stats(gobj, priority, jn_value);
    json_decref(jn_value);
    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    GBUFFER *gbuf = (GBUFFER *)(size_t)kw_get_int(kw, "gbuffer", 0, 0);
    static uint32_t last_sequence = 0;

//     log_debug_gbuf(LOG_DUMP_INPUT, "monitor-input", gbuf);

    /*---------------------------------------*
     *  Get priority, sequence and crc
     *---------------------------------------*/
    char ssequence[20]={0}, scrc[20]={0};

    unsigned char *bf = gbuf_cur_rd_pointer(gbuf);
    int len = (int)gbuf_leftbytes(gbuf);
    if(len < 17) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "gbuffer too small",
            "len",          "%d", len,
            NULL
        );
        KW_DECREF(kw);
        return -1;
    }

    uint32_t crc = 0;
    for(int i=0; i<len-8; i++) {
        unsigned char xx = bf[i];
        uint32_t x = (uint32_t)xx;
        crc  += x;
    }
    snprintf(
        scrc,
        sizeof(scrc),
        "%08"PRIX32,
        crc
    );
    char *pcrc = (char *)bf + len - 8;
    if(strcmp(pcrc, scrc)!=0) {
//        log_error(0,
//            "gobj",         "%s", gobj_full_name(gobj),
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_JSON_ERROR,
//            "msg",          "%s", "BAD crc",
//            "my-crc",       "%s", scrc,
//            "his-crc",      "%s", pcrc,
//            "bf",           "%s", bf,
//            "len",          "%d", len,
//            NULL
//        );
//        log_debug_full_gbuf(0, gbuf, "crc bad");
        //KW_DECREF(kw);
        //return -1;
    }

    gbuf_set_wr(gbuf, len-8);

    char *spriority = gbuf_get(gbuf, 1);
    char *p = gbuf_get(gbuf, 8);
    if(p) {
        memmove(ssequence, p, 8);
        uint32_t sequence = strtol(ssequence, NULL, 16);
        if(sequence != last_sequence +1) {
            // Cuando vengan de diferentes fuentes vendrán lógicamente con diferente secuencia
    //         log_warning(1,
    //             "gobj",         "%s", gobj_full_name(gobj),
    //             "function",     "%s", __FUNCTION__,
    //             "msgset",       "%s", MSGSET_JSON_ERROR,
    //             "msg",          "%s", "BAD sequence",
    //             "last",         "%d", last_sequence,
    //             "curr",         "%d", sequence,
    //             NULL
    //         );
        }
        last_sequence = sequence;
    }

    /*---------------------------------------*
     *  Save msg in log
     *---------------------------------------*/
    int priority = *spriority - '0';
    if(priority < MAX_PRIORITY_COUNTER) {
        priority_counter[priority]++;
    }
    write2logs(
        gobj,
        priority,
        gbuf // not owned
    );

    /*---------------------------------------*
     *  Convert gbuf msg in json summary
     *---------------------------------------*/
    bf = gbuf_cur_rd_pointer(gbuf);
    if(*bf == '{') {
        gbuf_incref(gbuf);
        json_t *jn_value = gbuf2json(gbuf, 2); // gbuf stolen
        if(jn_value) {
            do_log_stats(gobj, priority, jn_value);
            json_decref(jn_value);
        }
    }

    KW_DECREF(kw);  // gbuf is owned here
    return 0;

}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL send_report = FALSE;

    const char *work_dir = yuneta_work_dir();
    if(test_sectimer(priv->warn_free_disk)) {
        if(!empty_string(work_dir)) {
            size_t min_free_disk = gobj_read_int32_attr(gobj, "min_free_disk");
            struct statvfs64 fiData;
            if(statvfs64(work_dir, &fiData) == 0) {
                int disk_free_percent = (fiData.f_bavail * 100)/fiData.f_blocks;
                if(disk_free_percent <= min_free_disk) {
                    if(priv->last_disk_free_percent != disk_free_percent || 1) {
                        send_warn_free_disk(gobj, disk_free_percent, min_free_disk);
                        priv->last_disk_free_percent = disk_free_percent;
                        send_report = TRUE;
                    }
                }
            }
        }
        priv->warn_free_disk = start_sectimer(60*60*24);
    }

    if(test_sectimer(priv->warn_free_mem)) {
        size_t min_free_mem = gobj_read_int32_attr(gobj, "min_free_mem");
        uint64_t total_memory = uv_get_total_memory()/1024;
        unsigned long free_memory = free_ram_in_kb();
        int mem_free_percent = (free_memory * 100)/total_memory;
        if(mem_free_percent <= min_free_mem) {
            send_warn_free_mem(gobj, mem_free_percent);
            priv->last_mem_free_percent = mem_free_percent;
            send_report = TRUE;
        }
        priv->warn_free_mem = start_sectimer(60*60*24);
    }

    if(send_report) {
        send_report_email(gobj, FALSE);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Child stopped
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    KW_DECREF(kw);
    return 0;
}


/***************************************************************************
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = {
    // top input
    // bottom input
    {"EV_ON_MESSAGE",   0,  0,  0},
    {"EV_ON_OPEN",      0,  0,  0},
    {"EV_ON_CLOSE",     0,  0,  0},
    {"EV_TIMEOUT",      0,  0,  0},
    {"EV_STOPPED",      0,  0,  0},
    // internal
    {NULL, 0, 0, 0}
};
PRIVATE const EVENT output_events[] = {
    {NULL, 0, 0, 0}
};
PRIVATE const char *state_names[] = {
    "ST_IDLE",
    NULL
};

PRIVATE EV_ACTION ST_IDLE[] = {
    {"EV_ON_MESSAGE",       ac_on_message,          0},
    {"EV_ON_OPEN",          ac_on_open,             0},
    {"EV_ON_CLOSE",         ac_on_close,            0},
    {"EV_TIMEOUT",          ac_timeout,             0},
    {"EV_STOPPED",          ac_stopped,             0},
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_IDLE,
    NULL
};

PRIVATE FSM fsm = {
    input_events,
    output_events,
    state_names,
    states,
};

/***************************************************************************
 *              GClass
 ***************************************************************************/
/*---------------------------------------------*
 *              Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {0, 0, 0}
};

/*---------------------------------------------*
 *              GClass
 *---------------------------------------------*/
PRIVATE GCLASS _gclass = {
    0,  // base
    GCLASS_LOGCENTER_NAME,
    &fsm,
    {
        mt_create,
        0, //mt_create2,
        mt_destroy,
        mt_start,
        mt_stop,
        mt_play,
        mt_pause,
        mt_writing,
        0, //mt_reading,
        0, //mt_subscription_added,
        0, //mt_subscription_deleted,
        0, //mt_child_added,
        0, //mt_child_removed,
        mt_stats,
        0, //mt_command_parser,
        0, //mt_inject_event,
        0, //mt_create_resource,
        0, //mt_list_resource,
        0, //mt_save_resource,
        0, //mt_delete_resource,
        0, //mt_future21
        0, //mt_future22
        0, //mt_get_resource
        0, //mt_state_changed,
        0, //mt_authenticate,
        0, //mt_list_childs,
        0, //mt_stats_updated,
        0, //mt_disable,
        0, //mt_enable,
        0, //mt_trace_on,
        0, //mt_trace_off,
        0, //mt_gobj_created,
        0, //mt_future33,
        0, //mt_future34,
        0, //mt_publish_event,
        0, //mt_publication_pre_filter,
        0, //mt_publication_filter,
        0, //mt_authz_checker,
        0, //mt_future39,
        0, //mt_create_node,
        0, //mt_update_node,
        0, //mt_delete_node,
        0, //mt_link_nodes,
        0, //mt_future44,
        0, //mt_unlink_nodes,
        0, //mt_topic_jtree,
        0, //mt_get_node,
        0, //mt_list_nodes,
        0, //mt_shoot_snap,
        0, //mt_activate_snap,
        0, //mt_list_snaps,
        0, //mt_treedbs,
        0, //mt_treedb_topics,
        0, //mt_topic_desc,
        0, //mt_topic_links,
        0, //mt_topic_hooks,
        0, //mt_node_parents,
        0, //mt_node_childs,
        0, //mt_list_instances,
        0, //mt_node_tree,
        0, //mt_topic_size,
        0, //mt_future62,
        0, //mt_future63,
        0, //mt_future64
    },
    lmt,
    tattr_desc,
    sizeof(PRIVATE_DATA),
    0,  // acl
    s_user_trace_level,
    command_table,  // command_table
    0,  // gcflag
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC GCLASS *gclass_logcenter(void)
{
    return &_gclass;
}
