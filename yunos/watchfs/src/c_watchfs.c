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
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,       "Command's help"),
SDATA_END()
};


/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name--------------------flag--------default---------description---------- */
SDATA (DTP_STRING,      "path",                 0,          0,              "Path to watch"),
SDATA (DTP_BOOLEAN,     "recursive",            0,          0,              "Watch all directory tree"),
SDATA (DTP_STRING,      "patterns",             0,          0,              "File patterns to watch"),
SDATA (DTP_STRING,      "command",              0,          0,              "Command to execute when a fs event occurs"),
SDATA (DTP_BOOLEAN,     "use_parameter",        0,          0,              "Pass to command the filename as parameter"),
SDATA (DTP_BOOLEAN,     "ignore_changed_event", 0,          0,              "Ignore EV_CHANGED"),
SDATA (DTP_BOOLEAN,     "ignore_renamed_event", 0,          0,              "Ignore EV_RENAMED"),
SDATA (DTP_BOOLEAN,     "info",                 0,          0,              "Inform of found subdirectories"),
SDATA (DTP_POINTER,     "user_data",            0,          0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",           0,          0,              "more user data"),
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

PRIVATE hgclass __gclass__ = 0;




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

    priv->timer = gobj_create("", C_TIMER, 0, gobj);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *path = gobj_read_str_attr(gobj, "path");
    if(!path || access(path, 0)!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "path NOT EXIST",
            "path",         "%s", path,
            NULL
        );
        fprintf(stderr, "\nPath '%s' NO FOUND\n", path);
        exit(-1);
    }
    gobj_log_info(gobj, 0,
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
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "regcomp() FAILED",
                "re",           "%s", priv->patterns_list[i],
                NULL
            );
            exit(-1);
        }
        gobj_log_info(gobj, 0,
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
    return msg_iev_build_response(
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
    //const char *path = kw_get_str(gobj, kw, "path", "");
    const char *filename = kw_get_str(gobj, kw, "filename", "", 0);
    const char *path = kw_get_str(gobj, kw, "path", "", 0);
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
            gobj_log_info(gobj, 0,
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
    //const char *path = kw_get_str(gobj, kw, "path", "", FALSE);
    const char *filename = kw_get_str(gobj, kw, "filename", "", 0);
    const char *path = kw_get_str(gobj, kw, "path", "", 0);
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
            gobj_log_info(gobj, 0,
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

/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create      = mt_create,
    .mt_start       = mt_start,
    .mt_stop        = mt_stop,
    .mt_play        = mt_play,
    .mt_pause       = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_WATCHFS);

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
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
        {EV_RENAMED,    ac_renamed,     0},
        {EV_CHANGED,    ac_changed,     0},
        {EV_TIMEOUT,    ac_timeout,     0},
        {EV_STOPPED,    0,              0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_RENAMED,    0},
        {EV_CHANGED,    0},
        {EV_TIMEOUT,    0},
        {EV_STOPPED,    0},
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
        0,                  // LMT
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,                  // ACL
        command_table,
        s_user_trace_level,
        0                   // GClass flags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_watchfs(void)
{
    return create_gclass(C_WATCHFS);
}
