/****************************************************************************
 *          c_linux_yuno.c
 *
 *          GClass __yuno__
 *          Low level esp-idf
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <time.h>
#include <libintl.h>
#include <locale.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <gobj_environment.h>
#include "yuneta_ev_loop.h"
#include "yuneta_environment.h"
#include "c_timer.h"
#include "c_linux_yuno.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void remove_pid_file(void);
PRIVATE int save_pid_in_file(hgobj gobj);

PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int atexit_registered = 0; /* Register atexit just 1 time. */
PRIVATE char pidfile[PATH_MAX] = {0};

/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type---------name---------------flag------------default---------description---------- */
SDATA (DTP_STRING,  "url_udp_log",      SDF_PERSIST,    "",             "UDP Log url"),
SDATA (DTP_STRING,  "process",          SDF_RD,         "",             "Process name"),
SDATA (DTP_STRING,  "hostname",         SDF_RD,         "",             "Hostname"),
SDATA (DTP_INTEGER, "pid",              SDF_RD,         "",             "pid"),
SDATA (DTP_STRING,  "node_uuid",        SDF_RD,         "",             "uuid of node"),
SDATA (DTP_STRING,  "node_owner",       SDF_RD,         "",             "Owner of node"),
SDATA (DTP_STRING,  "realm_id",         SDF_RD,         "",             "Realm id where the yuno lives"),
SDATA (DTP_STRING,  "realm_owner",      SDF_RD,         "",             "Owner of realm"),
SDATA (DTP_STRING,  "realm_role",       SDF_RD,         "",             "Role of realm"),
SDATA (DTP_STRING,  "realm_name",       SDF_RD,         "",             "Name of realm"),
SDATA (DTP_STRING,  "realm_env",        SDF_RD,         "",             "Environment of realm"),

SDATA (DTP_STRING,  "appName",          SDF_RD,         "",             "App name, must match the role"),
SDATA (DTP_STRING,  "appDesc",          SDF_RD,         "",             "App Description"),
SDATA (DTP_STRING,  "appDate",          SDF_RD,         "",             "App date/time"),
SDATA (DTP_STRING,  "work_dir",         SDF_RD,         "",             "Work dir"),
SDATA (DTP_STRING,  "domain_dir",       SDF_RD,         "",             "Domain dir"),

SDATA (DTP_STRING,  "yuno_role",        SDF_RD,         "",             "Yuno Role"),
SDATA (DTP_STRING,  "yuno_id",          SDF_RD,         "",             "Yuno Id. Set by agent"),
SDATA (DTP_STRING,  "yuno_name",        SDF_RD,         "",             "Yuno name. Set by agent"),
SDATA (DTP_STRING,  "yuno_tag",         SDF_RD,         "",             "Tags of yuno. Set by agent"),
SDATA (DTP_STRING,  "yuno_release",     SDF_RD,         "",             "Yuno Release. Set by agent"),
SDATA (DTP_STRING,  "bind_ip",          SDF_RD,         "",             "Bind ip of yuno's realm. Set by agent"),
SDATA (DTP_BOOLEAN, "yuno_multiple",    SDF_RD,         "0",            "True when yuno can open shared ports. Set by agent"),
SDATA (DTP_INTEGER, "launch_id",        SDF_RD,         "0",            "Launch Id. Set by agent"),
SDATA (DTP_STRING,  "start_date",       SDF_RD|SDF_STATS, "",           "Yuno starting date"),
SDATA (DTP_INTEGER, "uptime",          SDF_RD|SDF_STATS, "0",           "Yuno living time"),

SDATA (DTP_JSON,    "tags",             SDF_RD,         "{}",           "tags"),
SDATA (DTP_JSON,    "required_services",SDF_RD,         "{}",           "Required services"),
SDATA (DTP_JSON,    "public_services",  SDF_RD,         "{}",           "Public services"),
SDATA (DTP_JSON,    "service_descriptor",SDF_RD,        "{}",           "Public service descriptor"),
SDATA (DTP_STRING,  "i18n_dirname",     SDF_RD,         "",             "dir_name parameter of bindtextdomain()"),
SDATA (DTP_STRING,  "i18n_domain",      SDF_RD,         "",             "domain_name parameter of bindtextdomain() and textdomain()"),
SDATA (DTP_STRING,  "i18n_codeset",     SDF_RD,         "UTF-8",        "codeset of textdomain"),
SDATA (DTP_INTEGER, "watcher_pid",      SDF_RD,         "0",            "Watcher pid"),
SDATA (DTP_STRING,  "info_msg",         SDF_RD,         "",             "Info msg, like errno"),
SDATA (DTP_JSON,    "allowed_ips",      SDF_PERSIST,    "{}",           "Allowed peer ip's if true, false not allowed"),
SDATA (DTP_JSON,    "denied_ips",       SDF_PERSIST,    "{}",           "Denied peer ip's if true, false not denied"),

SDATA (DTP_STRING,  "yuneta_version",   SDF_RD,         YUNETA_VERSION, "Yuneta version"),
SDATA (DTP_DICT,    "trace_levels",     SDF_PERSIST,    "{}",           "Trace levels"),
SDATA (DTP_DICT,    "no_trace_levels",  SDF_PERSIST,    "{}",           "No trace levels"),
SDATA (DTP_INTEGER, "periodic",         SDF_RD,         "1000",         "Timeout periodic, in miliseconds"),
SDATA (DTP_INTEGER, "timeout_stats",    SDF_RD,         "1",            "timeout (seconds) for publishing stats"),
SDATA (DTP_INTEGER, "timeout_flush",    SDF_RD,         "2",            "timeout (seconds) for rotatory flush"),
SDATA (DTP_INTEGER, "autokill",         SDF_RD,         "0",            "Timeout (>0) to autokill in seconds"),
SDATA (DTP_STRING,  "start_date",       SDF_STATS,      "",             "Yuno starting date"),
SDATA (DTP_INTEGER, "start_time",       SDF_STATS,      "0",            "Yuno starting time"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING, "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,  "level",        0,              0,          "command search level in childs"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,       "Command's help"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendant value!
 *  required paired correlative strings
 *  in s_user_trace_level
 *---------------------------------------------*/
enum {
    TRACE_XXXX          = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    //{"xxxx",             "Trace xxxx"},
    {0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
//SDATAAUTHZ (ASN_SCHEMA, "open-console",     0,      0,      0,      "Permission to open console"),
//SDATAAUTHZ (ASN_SCHEMA, "close-console",    0,      0,      0,      "Permission to close console"),
//SDATAAUTHZ (ASN_SCHEMA, "list-consoles",    0,      0,      0,      "Permission to list consoles"),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj gobj_timer;
    yev_loop_t yev_loop;

    size_t t_flush;
    size_t t_stats;
    size_t t_restart;
    int timeout_flush;
    int timeout_stats;
    int timeout_restart;
    int periodic;
    int autokill;
} PRIVATE_DATA;

PRIVATE hgclass gclass = 0;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if (!atexit_registered) {
        atexit(remove_pid_file);
        atexit_registered = 1;
    }

    time_t now;
    time(&now);
    gobj_write_integer_attr(gobj, "start_time", now);

    char bfdate[90];
    current_timestamp(bfdate, sizeof(bfdate));
    gobj_write_str_attr(gobj, "start_date", bfdate);

    node_uuid();

    gobj_log_warning(0, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "start_time",
        "t",            "%lld", (long long int)now,
        "start_date",   "%s", bfdate,
        NULL
    );

    const char *i18n_domain = gobj_read_str_attr(gobj, "i18n_domain");
    if(!empty_string(i18n_domain)) {
        const char *i18n_dirname = gobj_read_str_attr(gobj, "i18n_dirname");
        if(!empty_string(i18n_dirname)) {
            bindtextdomain(i18n_domain, i18n_dirname);
        }
        textdomain(i18n_domain);
        const char *i18n_codeset = gobj_read_str_attr(gobj, "i18n_codeset");
        if(!empty_string(i18n_codeset)) {
            bind_textdomain_codeset(i18n_domain, i18n_codeset);
        }
    }

// TODO   set_user_gclass_traces(gobj);
//    set_user_trace_filter(gobj);
//    set_user_gclass_no_traces(gobj);

    priv->gobj_timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    if(gobj_read_integer_attr(gobj, "launch_id")) {
        save_pid_in_file(gobj);
    }

    /*
     *  Create the event loop
     */
    yev_loop_init(gobj, &priv->yev_loop);

    SET_PRIV(periodic,              (int)gobj_read_integer_attr)
    SET_PRIV(autokill,              (int)gobj_read_integer_attr)
    SET_PRIV(timeout_stats,         (int)gobj_read_integer_attr)
    SET_PRIV(timeout_flush,         (int)gobj_read_integer_attr)
    SET_PRIV(timeout_restart,       (int)gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(periodic,            (int)gobj_read_integer_attr)
        if(gobj_is_running(gobj)) {
            set_timeout_periodic(priv->gobj_timer, priv->periodic);
        }
    ELIF_EQ_SET_PRIV(autokill,          (int)gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_stats,     (int)gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_flush,     (int)gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_restart,   (int)gobj_read_integer_attr)
        if(priv->timeout_restart) {
            priv->t_restart = start_sectimer(priv->timeout_restart);
        } else {
            priv->t_restart = 0;
        }
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Set user traces
     *  Here set gclass/gobj traces
     */
    // TODO set_user_gobj_traces(gobj);
    //set_user_gobj_no_traces(gobj);

    gobj_start(priv->gobj_timer);

    set_timeout_periodic(priv->gobj_timer, priv->periodic);

    if(priv->timeout_flush) {
        priv->t_flush = start_sectimer(priv->timeout_flush);
    }
    if(priv->timeout_stats) {
        priv->t_stats = start_sectimer(priv->timeout_stats);
    }
    if(priv->timeout_restart) {
        priv->t_restart = start_sectimer(priv->timeout_restart);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    /*
     *  When yuno stops, it's the death of the app
     */
    clear_timeout(priv->gobj_timer);
    gobj_stop(priv->gobj_timer);
    gobj_stop_childs(gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

     yev_loop_close(priv->yev_loop);
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    /*
     *  This play order can come from yuneta_agent or autoplay config option or programmatic sentence
     */
    gobj_play(gobj_default_service());

    /*
     *  Run event loop
     */
    gobj_log_debug(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_STARTUP,
        "msg",              "%s", "Running main",
        "yuno_id",          "%s", gobj_read_str_attr(gobj, "yuno_id"),
        "yuno_name",        "%s", gobj_read_str_attr(gobj, "yuno_name"),
        "yuno_role",        "%s", gobj_read_str_attr(gobj, "yuno_role"),
        NULL
    );

// TODO    uv_run(&priv->uv_loop, UV_RUN_DEFAULT);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    /*
     *  This play order can come from yuneta_agent or programmatic sentence
     */
    gobj_pause(gobj_default_service());
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
//     KW_INCREF(kw)
//     json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
//     return msg_iev_build_webix(
//         gobj,
//         0,
//         jn_resp,
//         0,
//         0,
//         kw  // owned
//     );
    return json_object();
}




                    /***************************
                     *      Local methods
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void remove_pid_file(void)
{
    if(!empty_string(pidfile)) {
        unlink(pidfile);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int save_pid_in_file(hgobj gobj)
{
    /*
     *  Let it create the bin_path. Can exist some zombi yuno.
     */
    unsigned int pid = getpid();
    yuneta_bin_file(pidfile, sizeof(pidfile), "yuno.pid", TRUE);
    FILE *file = fopen(pidfile, "w");
    fprintf(file, "%d\n", pid);
    fclose(file);
    return 0;
}





                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_periodic_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    static json_int_t i = 0;
    i++;

    if(priv->autokill > 0) {
        if(i >= priv->autokill) {
            gobj_trace_msg(gobj, "===========> SHUTDOWN <===========");
            gobj_shutdown();
            JSON_DECREF(kw)
            return -1;
        }
    }

    if(priv->timeout_flush && test_sectimer(priv->t_flush)) {
        priv->t_flush = start_sectimer(priv->timeout_flush);
        // TODO rotatory_flush(0);
    }
    if(gobj_get_yuno_must_die()) {
        JSON_DECREF(kw);
        gobj_shutdown(); // provoca gobj_pause y gobj_stop del gobj yuno
        return 0;
    }
    if(priv->timeout_restart && test_sectimer(priv->t_restart)) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_STARTUP,
            "msg",          "%s", "Exit to restart",
            NULL
        );
        gobj_set_exit_code(-1);
        JSON_DECREF(kw);
        gobj_shutdown(); // provoca gobj_pause y gobj_stop del gobj yuno
        return 0;
    }

    if(priv->timeout_stats && test_sectimer(priv->t_stats)) {
        priv->t_stats = start_sectimer(priv->timeout_stats);
        // TODO load_stats(gobj);
    }


    // Let others uses the periodic timer, save resources
    gobj_publish_event(gobj, EV_PERIODIC_TIMEOUT, 0);

    JSON_DECREF(kw)
    return 0;
}




                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_destroy = mt_destroy,
    .mt_writing = mt_writing,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*---------------------------------------------*
 *          Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {0, 0, 0}
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_YUNO);

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
    if(gclass) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
        {EV_PERIODIC_TIMEOUT,       ac_periodic_timeout,    0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_PERIODIC_TIMEOUT,     EVF_OUTPUT_EVENT},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    gclass = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        lmt,
        tattr_desc,
        sizeof(PRIVATE_DATA),
        authz_table,
        command_table,
        s_user_trace_level,
        0   // gclass_flag
    );
    if(!gclass) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_linux_yuno(void)
{
    return create_gclass(C_YUNO);
}
