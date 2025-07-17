/***********************************************************************
 *          C_WATCHFS.C
 *          Watchfs GClass.
 *
 *
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include "c_watchfs.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define PATTERNS_SIZE  100

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int exec_command(hgobj gobj, const char *path, const char *filename);


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

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
SDATA_END()
};


/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name--------------------flag--------default---------description---------- */
SDATA (ASN_OCTET_STR,   "path",                 0,          0,              "Path to watch"),
SDATA (ASN_BOOLEAN,     "recursive",            0,          0,              "Watch all directory tree"),
SDATA (ASN_OCTET_STR,   "patterns",             0,          0,              "File patterns to watch"),
SDATA (ASN_OCTET_STR,   "command",              0,          0,              "Command to execute when a fs event occurs"),
SDATA (ASN_BOOLEAN,     "use_parameter",        0,          0,              "Pass to command the filename as parameter"),
SDATA (ASN_BOOLEAN,     "ignore_changed_event", 0,          0,              "Ignore EV_CHANGED"),
SDATA (ASN_BOOLEAN,     "ignore_renamed_event", 0,          0,              "Ignore EV_RENAMED"),
SDATA (ASN_BOOLEAN,     "info",                 0,          0,              "Inform of found subdirectories"),
SDATA (ASN_POINTER,     "user_data",            0,          0,              "user data"),
SDATA (ASN_POINTER,     "user_data2",           0,          0,              "more user data"),
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
    hgobj gobj_fs;
    hgobj timer;
    char *patterns_list[PATTERNS_SIZE];
    regex_t regex[PATTERNS_SIZE];
    int npatterns;
    BOOL use_parameter;
    BOOL ignore_changed_event;
    BOOL ignore_renamed_event;
    uint64_t time2exec; // TODO this would be witch each changed file!
                        // (for app using the filename parameter)
    BOOL info;
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

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(use_parameter,                 gobj_read_bool_attr)
    SET_PRIV(ignore_changed_event,          gobj_read_bool_attr)
    SET_PRIV(ignore_renamed_event,          gobj_read_bool_attr)
    SET_PRIV(info,                          gobj_read_bool_attr)

    priv->timer = gobj_create("", GCLASS_TIMER, 0, gobj);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *path = gobj_read_str_attr(gobj, "path");
    if(!path || access(path, 0)!=0) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "path NOT EXIST",
            "path",         "%s", path,
            NULL
        );
        fprintf(stderr, "\nPath '%s' NO FOUND\n", path);
        exit(-1);
    }
    log_info(0,
        "gobj",         "%s", gobj_short_name(gobj),
        "msgset",       "%s", MSGSET_MONITORING,
        "msg",          "%s", "watching path",
        "path",         "%s", path,
        NULL
    );

    const char *patterns = gobj_read_str_attr(gobj, "patterns");
    priv->npatterns = split(
        patterns,
        ";",
        priv->patterns_list,
        PATTERNS_SIZE
    );

    for(int i=0; i<priv->npatterns; i++) {
        // WARNING changed 0 by REG_EXTENDED|REG_NOSUB: future side effect?
        int r = regcomp(&priv->regex[i], priv->patterns_list[i], REG_EXTENDED|REG_NOSUB);
        if(r!=0) {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "regcomp() FAILED",
                "re",           "%s", priv->patterns_list[i],
                NULL
            );
            exit(-1);
        }
        log_info(0,
            "gobj",         "%s", gobj_short_name(gobj),
            "msgset",       "%s", MSGSET_MONITORING,
            "msg",          "%s", "watching pattern",
            "re",           "%s", priv->patterns_list[i],
            NULL
        );
    }

    json_t *kw_fs = json_pack("{s:s, s:b, s:b}",
        "path", path,
        "info", 0,
        "recursive", gobj_read_bool_attr(gobj, "recursive")
    );
    priv->gobj_fs = gobj_create("", GCLASS_FS, kw_fs, gobj);
    gobj_subscribe_event(priv->gobj_fs, NULL, 0, gobj);

    gobj_start(priv->gobj_fs);
    gobj_start(priv->timer);
    set_timeout_periodic(priv->timer, 200);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->gobj_fs);
    clear_timeout(priv->timer);
    gobj_stop(priv->timer);

    split_free(priv->patterns_list, priv->npatterns);

    for(int i=0; i<priv->npatterns; i++) {
        regfree(&priv->regex[i]);
    }
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
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
    return msg_iev_build_webix(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *      Execute command
 ***************************************************************************/
PRIVATE int exec_command(hgobj gobj, const char *path, const char *filename)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *command = gobj_read_str_attr(gobj, "command");

    if(priv->use_parameter) {
        char temp[4*1014];

        snprintf(temp, sizeof(temp), "%s %s/%s", command, path, filename);
        return system(temp);
    } else {
        return system(command);
    }
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_renamed(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    // TODO would be a launcher for each modified filename
    //const char *path = kw_get_str(kw, "path", "");
    const char *filename = kw_get_str(kw, "filename", "", 0);
    const char *path = kw_get_str(kw, "path", "", 0);
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(strstr(path, "/_build/")) { // 2024-Mar-07 FIX to avoid sphinx re-make all time
        // TODO set a new parameter: ignore_path
        JSON_DECREF(kw);
        return 0;
    }

    BOOL matched = FALSE;
    for(int i=0; i<priv->npatterns; i++) {
        int reti = regexec(&priv->regex[i], filename, 0, NULL, 0);
        if(reti==0) {
            // Match
            matched = TRUE;
            break;
        } else if(reti == REG_NOMATCH) {
            // No match
        } else {
            //regerror(reti, &regex, msgbuf, sizeof(msgbuf));
            //fprintf(stderr, "Regex match failed: %s\n", msgbuf);
        }
    }

    if(matched) {
        if(priv->info) {
            log_info(0,
                "gobj",         "%s", gobj_short_name(gobj),
                "msgset",       "%s", MSGSET_MONITORING,
                "msg",          "%s", "matched",
                "filename",     "%s", filename,
                NULL
            );
        }
        //exec_command(gobj, path, filename);
        if(priv->time2exec == 0) {
            priv->time2exec = start_msectimer(500);
        } else {
            priv->time2exec = start_msectimer(200);
        }
    }

    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_changed(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    // TODO would be a launcher for each modified filename
    //const char *path = kw_get_str(kw, "path", "", FALSE);
    const char *filename = kw_get_str(kw, "filename", "", 0);
    const char *path = kw_get_str(kw, "path", "", 0);
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(strstr(path, "/_build/")) { // 2024-Mar-07 FIX to avoid sphinx re-make all time
        // TODO set a new parameter: ignore_path
        JSON_DECREF(kw);
        return 0;
    }

    if(priv->ignore_changed_event) {
        JSON_DECREF(kw);
        return 0;
    }


    BOOL matched = FALSE;
    for(int i=0; i<priv->npatterns; i++) {
        int reti = regexec(&priv->regex[i], filename, 0, NULL, 0);
        if(reti==0) {
            // Match
            matched = TRUE;
            break;
        } else if(reti == REG_NOMATCH) {
            // No match
        } else {
            //regerror(reti, &regex, msgbuf, sizeof(msgbuf));
            //fprintf(stderr, "Regex match failed: %s\n", msgbuf);
        }
    }

    if(matched) {
        if(priv->info) {
            log_info(0,
                "gobj",         "%s", gobj_short_name(gobj),
                "msgset",       "%s", MSGSET_MONITORING,
                "msg",          "%s", "matched",
                "filename",     "%s", filename,
                NULL
            );
        }
        //exec_command(gobj, path, filename);
        if(priv->time2exec == 0) {
            priv->time2exec = start_msectimer(500);
        } else {
            priv->time2exec = start_msectimer(200);
        }
    }
    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(test_msectimer(priv->time2exec)) {
        exec_command(gobj, "", ""); //TODO filename parameter not used.
        priv->time2exec = 0;
    }
    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = {
    // top input
    // bottom input
    {"EV_RENAMED",      0,  0,  0},
    {"EV_CHANGED",      0,  0,  0},
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
    {"EV_RENAMED",      ac_renamed,     0},
    {"EV_CHANGED",      ac_changed,     0},
    {"EV_TIMEOUT",      ac_timeout,     0},
    {"EV_STOPPED",      0,              0},
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
    GCLASS_WATCHFS_NAME,
    &fsm,
    {
        mt_create,
        0, //mt_create2,
        0, //mt_destroy,
        mt_start,
        mt_stop,
        mt_play,
        mt_pause,
        0, //mt_writing,
        0, //mt_reading,
        0, //mt_subscription_added,
        0, //mt_subscription_deleted,
        0, //mt_child_added,
        0, //mt_child_removed,
        0, //mt_stats,
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
PUBLIC GCLASS *gclass_watchfs(void)
{
    return &_gclass;
}
