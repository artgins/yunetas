/***********************************************************************
 *          C_WATCHFS.C
 *          Watchfs GClass.
 *
 *
 *
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "c_watchfs.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE char *shell_escape(hgobj gobj, const char *str, char *dest, size_t dest_size);
PRIVATE int exec_command(hgobj gobj, const char *path, const char *filename);
PRIVATE void remember_matched_file(hgobj gobj, const char *path, const char *filename);


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
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name--------------------flag--------default---------description---------- */
SDATA (DTP_STRING,      "path",                 0,          0,              "Path to watch"),
SDATA (DTP_BOOLEAN,     "recursive",            0,          0,              "Watch all directory tree"),
SDATA (DTP_STRING,      "patterns",             0,          0,              "File patterns to watch"),
SDATA (DTP_STRING,      "command",              0,          0,              "Command to execute when a fs event occurs"),
SDATA (DTP_BOOLEAN,     "use_parameter",        0,          0,              "Pass to command the filename as parameter"),
SDATA (DTP_BOOLEAN,     "ignore_changed_event", 0,          0,              "Ignore EV_FS_CHANGED"),
SDATA (DTP_BOOLEAN,     "ignore_renamed_event", 0,          0,              "Ignore EV_FS_RENAMED"),
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
    const char **patterns_list;
    regex_t *regex;
    int npatterns;
    BOOL use_parameter;
    BOOL ignore_changed_event;
    BOOL ignore_renamed_event;
    uint64_t time2exec; // TODO this would be witch each changed file!
                        // (for app using the filename parameter)
    char last_path[PATH_MAX];       // the last file matched before time2exec: the parameter
    char last_filename[PATH_MAX];
    BOOL last_file_too_long;        // not kept whole: the command is not run with it
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

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
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
            "msgset",       "%s", MSGSET_PARAMETER,
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
    priv->patterns_list = split2(
        patterns,
        ";",
        &priv->npatterns
    );
    priv->regex = GBMEM_MALLOC(sizeof(regex_t) * priv->npatterns);

    for(int i=0; i<priv->npatterns; i++) {
        // WARNING changed 0 by REG_EXTENDED|REG_NOSUB: future side effect?
        int r = regcomp(&priv->regex[i], priv->patterns_list[i], REG_EXTENDED|REG_NOSUB);
        if(r!=0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
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
    priv->gobj_fs = gobj_create("", C_FS, kw_fs, gobj);    // it subscribes its parent

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

    split_free2(priv->patterns_list);

    for(int i=0; i<priv->npatterns; i++) {
        regfree(&priv->regex[i]);
    }
    GBMEM_FREE(priv->regex)
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
 *      Escape shell argument to prevent command injection
 *      Wraps the string in single quotes and escapes any single quotes.
 *      NULL (logged) when it does not fit in `dest`: up to 7.29.3 it was
 *      cut with nothing said, and the command ran with another name.
 ***************************************************************************/
PRIVATE char *shell_escape(hgobj gobj, const char *str, char *dest, size_t dest_size)
{
    size_t pos = 0;
    if(dest_size > 0) {
        dest[pos++] = '\''; // Opening single quote
    }
    for(const char *p = str; *p && pos < dest_size; p++) {
        if(*p == '\'') {
            // Replace ' with '\'' (close quote, escaped quote, open quote)
            if(pos + 4 > dest_size) {
                pos = dest_size;
                break;
            }
            dest[pos++] = '\'';
            dest[pos++] = '\\';
            dest[pos++] = '\'';
            dest[pos++] = '\'';
        } else {
            dest[pos++] = *p;
        }
    }
    if(pos + 2 > dest_size) {   // the closing quote and the NUL
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Argument too long to quote: command not run",
            "argument",     "%s", str,
            "size",         "%d", (int)dest_size,
            NULL
        );
        return NULL;
    }
    dest[pos++] = '\''; // Closing single quote
    dest[pos] = '\0';
    return dest;
}

/***************************************************************************
 *  The file the command is given with use_parameter: the last one matched
 *  before it runs (the changes of a burst run the command once). Up to
 *  7.29.3 it was given none, and use_parameter appended '/'.
 ***************************************************************************/
PRIVATE void remember_matched_file(hgobj gobj, const char *path, const char *filename)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int lp = snprintf(priv->last_path, sizeof(priv->last_path), "%s", path);
    int lf = snprintf(priv->last_filename, sizeof(priv->last_filename), "%s", filename);
    if(lp < 0 || lp >= (int)sizeof(priv->last_path) ||
            lf < 0 || lf >= (int)sizeof(priv->last_filename)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Path of the file too long: the command will not run with it",
            "path",         "%s", path,
            "filename",     "%s", filename,
            NULL
        );
        priv->last_file_too_long = TRUE;
        return;
    }
    priv->last_file_too_long = FALSE;
}

/***************************************************************************
 *      Execute command
 ***************************************************************************/
PRIVATE int exec_command(hgobj gobj, const char *path, const char *filename)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *command = gobj_read_str_attr(gobj, "command");
    if(empty_string(command)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONFIGURATION,
            "msg",          "%s", "No command configured: nothing run",
            NULL
        );
        return -1;
    }

    /*
     *  With the file as its parameter: path/filename, quoted. Nothing is
     *  run when it does not fit (said): up to 7.29.3 each step was cut with
     *  nothing said, and the command ran with the name cut, or with no
     *  closing quote.
     */
    char temp[4*PATH_MAX];
    if(priv->use_parameter) {
        if(priv->last_file_too_long) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Path of the file too long: command not run",
                NULL
            );
            return -1;
        }
        char fullpath[PATH_MAX];
        char escaped_fullpath[2*PATH_MAX];
        int len = snprintf(fullpath, sizeof(fullpath), "%s/%s", path, filename);
        if(len < 0 || len >= (int)sizeof(fullpath)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Path of the file too long: command not run",
                "path",         "%s", path,
                "filename",     "%s", filename,
                NULL
            );
            return -1;
        }
        if(!shell_escape(gobj, fullpath, escaped_fullpath, sizeof(escaped_fullpath))) {
            return -1;  // Error already logged
        }
        len = snprintf(temp, sizeof(temp), "%s %s", command, escaped_fullpath);
        if(len < 0 || len >= (int)sizeof(temp)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Command with the file too long: not run",
                "command",      "%s", command,
                "file",         "%s", fullpath,
                NULL
            );
            return -1;
        }
        command = temp;
    }

    /*
     *  run_process2() and not system(): the command starts with no signal
     *  blocked (system() gives it the yuno's mask, the signals the yuno
     *  keeps for its signalfd), and SIGQUIT is not set to SIG_IGN while it
     *  runs (that throws away a stop that is pending)
     */
    char *const argv[] = {"sh", "-c", (char *)command, NULL};
    return run_process2("/bin/sh", argv);
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_renamed(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    // TODO would be a launcher for each modified filename
    //const char *path = kw_get_str(gobj, kw, "path", "");
    const char *filename = kw_get_str(gobj, kw, "filename", "", 0);
    const char *path = kw_get_str(gobj, kw, "path", "", 0);
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(strstr(path, "/_build/")) { // 2024-Mar-07 FIX to avoid sphinx re-make all time
        // TODO set a new parameter: ignore_path
        KW_DECREF(kw);
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
        remember_matched_file(gobj, path, filename);
        if(priv->time2exec == 0) {
            priv->time2exec = start_msectimer(500);
        } else {
            priv->time2exec = start_msectimer(200);
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_changed(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    // TODO would be a launcher for each modified filename
    //const char *path = kw_get_str(gobj, kw, "path", "", FALSE);
    const char *filename = kw_get_str(gobj, kw, "filename", "", 0);
    const char *path = kw_get_str(gobj, kw, "path", "", 0);
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(strstr(path, "/_build/")) { // 2024-Mar-07 FIX to avoid sphinx re-make all time
        // TODO set a new parameter: ignore_path
        KW_DECREF(kw);
        return 0;
    }

    if(priv->ignore_changed_event) {
        KW_DECREF(kw);
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
        remember_matched_file(gobj, path, filename);
        if(priv->time2exec == 0) {
            priv->time2exec = start_msectimer(500);
        } else {
            priv->time2exec = start_msectimer(200);
        }
    }
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(test_msectimer(priv->time2exec)) {
        exec_command(gobj, priv->last_path, priv->last_filename);   // the last file of the burst
        priv->time2exec = 0;
    }
    KW_DECREF(kw);
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

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
// Events EV_FS_RENAMED and EV_FS_CHANGED are defined by C_FS

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
        {EV_FS_RENAMED,         ac_renamed,     0},
        {EV_FS_CHANGED,         ac_changed,     0},
        {EV_TIMEOUT_PERIODIC,   ac_timeout,     0},
        {EV_STOPPED,            0,              0},
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
        {EV_FS_RENAMED,         0},
        {EV_FS_CHANGED,         0},
        {EV_TIMEOUT_PERIODIC,   0},
        {EV_STOPPED,            0},
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
        attrs_table,
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
