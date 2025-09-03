/***********************************************************************
 *          C_AGENT22.C
 *          Agent22 GClass.
 *
 *          Yuneta Agent22, a second remote entry point for controlcenter
 *
 *          Copyright (c) 2022 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <grp.h>
#include <errno.h>
#include <regex.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>

#include <c_pty.h>
#include "c_agent22.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define SDATA_GET_ID(hs)  kw_get_str(gobj, (hs), "id", "", KW_REQUIRED)
#define SDATA_GET_STR(hs, field)  kw_get_str(gobj, (hs), (field), "", KW_REQUIRED)
#define SDATA_GET_INT(hs, field)  kw_get_int(gobj, (hs), (field), 0, KW_REQUIRED)
#define SDATA_GET_BOOL(hs, field)  kw_get_bool(gobj, (hs), (field), 0, KW_REQUIRED)
#define SDATA_GET_ITER(hs, field)  kw_get_list(gobj, (hs), (field), 0, KW_REQUIRED)
#define SDATA_GET_JSON(hs, field)  kw_get_dict_value(gobj, (hs), (field), 0, KW_REQUIRED)

#define SDATA_SET_STR(hs, key, value) json_object_set_new((hs), (key), json_string(value))
#define SDATA_SET_INT(hs, key, value) json_object_set_new((hs), (key), json_integer(value))
#define SDATA_SET_BOOL(hs, key, value) json_object_set_new((hs), (key), value?json_true():json_false())
#define SDATA_SET_JSON(hs, key, value) json_object_set((hs), (key), value)
#define SDATA_SET_JSON_NEW(hs, key, value) json_object_set_new((hs), (key), value)


/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int is_yuneta_agent22(pid_t pid);
PRIVATE void remove_pid_file(void);
PRIVATE int add_console_in_input_gate(hgobj gobj, const char *name, hgobj src);
PRIVATE int add_console_route(
    hgobj gobj,
    const char *name,
    json_t *jn_console,
    hgobj src,
    json_t *kw
);
PRIVATE int remove_console_route(
    hgobj gobj,
    const char *name,
    const char *route_service,
    const char *route_child
);

/***************************************************************************
 *              Resources
 ***************************************************************************/


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE int atexit_registered = 0; /* Register atexit just 1 time. */
PRIVATE const char *pidfile = "/yuneta/realms/agent/yuneta_agent22.pid";

PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_consoles(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_open_console(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_close_console(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help"),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_open_console[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "name",         0,              "",         "Name of console"),
SDATAPM (DTP_STRING,    "process",      0,              "bash",     "Process to execute"),
SDATAPM (DTP_STRING,    "cwd",          0,              0,          "Current work directory"),
SDATAPM (DTP_BOOLEAN,   "hold_open",    0,              0,          "True to not close pty on client disconnection"),
SDATAPM (DTP_INTEGER,   "cx",           0,              "80",       "Columns"),
SDATAPM (DTP_INTEGER,   "cy",           0,              "24",       "Rows"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_close_console[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "name",         0,              "",         "Name of console"),
SDATAPM (DTP_BOOLEAN,   "force",        0,              0,          "Force to close although hold_open TRUE"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_write_tty[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
    SDATAPM (DTP_STRING,    "name",         0,              0,          "Name of console"),
    SDATAPM (DTP_STRING,    "content64",    0,              0,          "Content64 data to write to tty"),
    SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};
PRIVATE const char *a_write_tty[] = {"EV_WRITE_TTY", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD2--type-----------name----------------flag----------------alias---------------items-----------json_fn---------description---------- */
SDATACM2 (DTP_SCHEMA,   "help",             0,                  a_help,             pm_help,        cmd_help,       "Command's help"),
SDATACM2 (DTP_SCHEMA,   "write-tty",        0,                  a_write_tty,        pm_write_tty,   0,              "Write data to tty"),
// HACK DANGER backdoor, use Yuneta only in private networks, or public but encrypted and assured connections.
SDATACM2 (DTP_SCHEMA,    "list-consoles",   0,                  0,                  0,              cmd_list_consoles, "List consoles"),
SDATACM2 (DTP_SCHEMA,    "open-console",    0,                  0,                  pm_open_console,cmd_open_console, "Open console"),
SDATACM2 (DTP_SCHEMA,    "close-console",   0,                  0,                  pm_close_console,cmd_close_console,"Close console"),

SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag----------------default---------description---------- */
SDATA (DTP_STRING,      "__username__",     SDF_RD,             "",             "Username"),
SDATA (DTP_INTEGER,     "max_consoles",     SDF_WR,             "10",           "Maximum consoles opened"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,              "User data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,              "More user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,              "Subscriber of output-events. Not a child gobj"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/

PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
SDATAAUTHZ (DTP_SCHEMA, "open-console",     0,      0,      0,      "Permission to open console"),
SDATAAUTHZ (DTP_SCHEMA, "close-console",    0,      0,      0,      "Permission to close console"),
SDATAAUTHZ (DTP_SCHEMA, "list-consoles",    0,      0,      0,      "Permission to list consoles"),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    BOOL enabled_yunos_running;

    hgobj gobj_tranger;
    json_t *tranger;

    json_t *list_consoles; // Dictionary of console names

    hgobj resource;
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

    if (!atexit_registered) {
        atexit(remove_pid_file);
        atexit_registered = 1;
    }

    /*----------------------------------------*
     *  Check node_owner
     *----------------------------------------*/
    const char *node_owner = gobj_yuno_node_owner();
    if(empty_string(node_owner)) {
        node_owner = "none";
        gobj_write_str_attr(gobj_yuno(), "node_owner", node_owner);

        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "node_owner EMPTY, setting none",
            NULL
        );
    }

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    BOOL is_yuneta = FALSE;
    struct passwd *pw = getpwuid(getuid());
    if(strcmp(pw->pw_name, "yuneta")==0) {
        gobj_write_str_attr(gobj, "__username__", "yuneta");
        is_yuneta = TRUE;
    } else {
        static gid_t groups[30]; // HACK to use outside
        int ngroups = sizeof(groups)/sizeof(groups[0]);

        getgrouplist(pw->pw_name, 0, groups, &ngroups);
        for(int i=0; i<ngroups; i++) {
            struct group *gr = getgrgid(groups[i]);
            if(strcmp(gr->gr_name, "yuneta")==0) {
                gobj_write_str_attr(gobj, "__username__", "yuneta");
                is_yuneta = TRUE;
                break;
            }
        }
    }
    if(!is_yuneta) {
        gobj_trace_msg(gobj, "User or group 'yuneta' is needed to run %s", gobj_yuno_role());
        printf("User or group 'yuneta' is needed to run %s\n", gobj_yuno_role());
        exit(0);
    }

    /*---------------------------------------*
     *      Check if already running
     *---------------------------------------*/
    {
        int pid = 0;

        FILE *file = fopen(pidfile, "r");
        if(file) {
            int x = fscanf(file, "%d", &pid);
            if(x) {} // avoid warning

            fclose(file);

            int ret = is_yuneta_agent22(pid);
            if(ret == 0) {
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INFO,
                    "msg",          "%s", "yuneta_agent22 already running, exiting",
                    "pid",          "%d", pid,
                    NULL
                );
                exit(0);
            } else if(errno == ESRCH) {
                unlink(pidfile);
            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                    "msg",          "%s", "cannot check pid",
                    "pid",          "%d", pid,
                    "errno",        "%d", errno,
                    "serrno",       "%s", strerror(errno),
                    NULL
                );
                unlink(pidfile);
            }

        }
        file = fopen(pidfile, "w");
        if(file) {
            fprintf(file, "%d\n", getpid());
            fclose(file);
        }
    }

    /*-----------------------------*
     *      Consoles
     *-----------------------------*/
    priv->list_consoles = json_object();

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
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(priv->list_consoles);

    remove_pid_file();
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
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_trace_on(hgobj gobj, const char *level, json_t *kw)
{
    treedb_set_trace(TRUE);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_trace_off(hgobj gobj, const char *level, json_t *kw)
{
    treedb_set_trace(FALSE);

    KW_DECREF(kw);
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
PRIVATE json_t *cmd_list_consoles(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "list-consoles";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  List consoles
     *----------------------------------------*/
    int result = 0;
    json_t *jn_data = json_object();

    const char *name; json_t *jn_console;
    json_object_foreach(priv->list_consoles, name, jn_console) {
        json_t *jn_dup_console = json_deep_copy(jn_console);
        json_object_set_new(jn_data, name, jn_dup_console);

        json_t *jn_routes = kw_get_dict(gobj, jn_dup_console, "routes", 0, KW_REQUIRED);
        json_t *jn_gobjs = kw_get_dict(gobj, jn_dup_console, "gobjs", json_object(), KW_CREATE);

        const char *route_name; json_t *jn_route;
        json_object_foreach(jn_routes, route_name, jn_route) {
            const char *route_service = kw_get_str(gobj, jn_route, "route_service", "", KW_REQUIRED);
            const char *route_child = kw_get_str(gobj, jn_route,  "route_child", "", KW_REQUIRED);
            hgobj gobj_route_service = gobj_find_service(route_service, TRUE);
            if(gobj_route_service) {
                hgobj gobj_input_gate = gobj_child_by_name(gobj_route_service, route_child);
                if(!gobj_input_gate) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                        "msg",          "%s", "no route child found",
                        "service",      "%s", route_service,
                        "child",        "%s", route_child,
                        NULL
                    );
                    json_object_set_new(jn_gobjs, route_name, json_string("ERROR route_child not found"));
                    result = -1;
                    continue;
                }
                json_t *jn_consoles = gobj_kw_get_user_data(gobj_input_gate, "consoles", 0, 0);
                json_object_set_new(jn_gobjs, route_name, json_deep_copy(jn_consoles));
            } else {
                json_object_set_new(jn_gobjs, route_name, json_string("ERROR route_service not found"));
                result = -1;
            }
        }
    }

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        result,
        json_sprintf("==> List consoles of agent22: '%s'", node_uuid()),
        0,
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_open_console(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "open-console";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  Open console
     *----------------------------------------*/
    const char *name = kw_get_str(gobj, kw, "name", "", 0);
    const char *process = kw_get_str(gobj, kw, "process", "bash", 0);
    const char *cwd = kw_get_str(gobj, kw, "cwd", "/home/yuneta", 0);
    BOOL hold_open = kw_get_bool(gobj, kw, "hold_open", 0, KW_WILD_NUMBER);
    int cx = kw_get_int(gobj, kw, "cx", 80, KW_WILD_NUMBER);
    int cy = kw_get_int(gobj, kw, "cy", 24, KW_WILD_NUMBER);

    if(empty_string(name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What console name?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(process)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What process?"),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Get a iter of matched resources
     */
    hgobj gobj_console = 0;

    if(!kw_has_key(priv->list_consoles, name)) {
        /*
         *  New console
         */
        if(kw_size(priv->list_consoles) > gobj_read_integer_attr(gobj, "max_consoles")) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf(
                    "Too much opened consoles: %d",
                    (int)kw_size(priv->list_consoles)
                ),
                0,
                0,
                kw  // owned
            );
        }

        /*
         *  Create pseudoterminal
         */
        json_t *kw_pty = json_pack("{s:s, s:s, s:i, s:i}",
            "process", process,
            "cwd", cwd,
            "cols", cx,
            "rows", cy
        );
        gobj_console = gobj_create_service(name, C_PTY, kw_pty, gobj);
        if(!gobj_console) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Cannot open console: '%s'", name),
                0,
                0,
                kw  // owned
            );
        }
        gobj_set_volatil(gobj_console, TRUE);

        /*
         *  Save console
         */
        json_t *jn_console = json_pack("{s:s, s:b, s:{}}",
            "process", process,
            "hold_open", hold_open,
            "routes"
        );

        add_console_route(gobj, name, jn_console, src, kw);

        json_object_set(priv->list_consoles, name, jn_console); // save in local list

        json_decref(jn_console);

        gobj_start(gobj_console);

    } else {
        /*
         *  Console already exists
         */
        json_t *jn_console = kw_get_dict(gobj, priv->list_consoles, name, 0, KW_REQUIRED);
        gobj_console = gobj_find_service(name, FALSE);
        if(!gobj_console) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Console gobj not found: '%s'", name),
                0,
                0,
                kw  // owned
            );
        }
        int ret = add_console_route(gobj, name, jn_console, src, kw);
        if(ret < 0) {
            if(ret == -2) {
                return msg_iev_build_response(
                    gobj,
                    -1,
                    json_sprintf("Console already open: '%s'", name),
                    0,
                    0,
                    kw  // owned
                );
            } else {
                return msg_iev_build_response(
                    gobj,
                    -1,
                    json_sprintf("Error opening console: '%s'", name),
                    0,
                    0,
                    kw  // owned
                );
            }
        }
    }

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        json_sprintf("Console opened: '%s'", name),  // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_close_console(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "close-console";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }


    /*----------------------------------------*
     *  Close console
     *----------------------------------------*/
    const char *name = kw_get_str(gobj, kw, "name", "", 0);
    BOOL force = kw_get_bool(gobj, kw, "force", 0, KW_WILD_NUMBER);

    if(empty_string(name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What console name?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *jn_console = kw_get_dict(gobj, priv->list_consoles, name, 0, 0);
    if(!jn_console) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Console not found: '%s'", name),
            0,
            0,
            kw  // owned
        );
    }

    BOOL hold_open = kw_get_bool(gobj, jn_console, "hold_open", 0, KW_REQUIRED);
    if(force) {
        hold_open = FALSE;
    }

    /*
     *  Delete console or route
     */
    int ret = 0;
    if(hold_open) {
        const char *route_service = gobj_name(gobj_nearest_top_service(src));
        const char *route_child = gobj_name(src);
        ret = remove_console_route(gobj, name, route_service, route_child);
    } else {
        hgobj gobj_console = gobj_find_service(name, TRUE);
        gobj_stop(gobj_console); // volatil, auto-destroy
    }

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        ret,
        json_sprintf("Console closed: '%s'", name),
        0,
        0, // owned
        kw  // owned
    );
}




            /***************************
             *      Local Methods
             ***************************/




/*****************************************************************
 *
 *****************************************************************/
PRIVATE int is_yuneta_agent22(pid_t pid)
{
    int ret = kill(pid, 0);
    if(ret == 0) {
        char cmdline[1024];
        read_process_cmdline(cmdline, sizeof(cmdline), pid);
        if(strstr(cmdline, " yuneta_agent22 ")) {
            return 0;
        }
        return -1;
    }
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void remove_pid_file(void)
{
    unlink(pidfile);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int add_console_route(
    hgobj gobj,
    const char *name,
    json_t *jn_console_,
    hgobj src,
    json_t *kw
)
{
    json_t *jn_routes = kw_get_dict(gobj, jn_console_, "routes", 0, KW_REQUIRED);

    const char *route_service = gobj_name(gobj_nearest_top_service(src));
    const char *route_child = gobj_name(src);

    char route_name[NAME_MAX];
    snprintf(route_name, sizeof(route_name), "%s.%s", route_service, route_child);

    if(kw_has_key(jn_routes, route_name)) {
        return -2;
    }

    /*
     *  add in local list
     */
    json_t *jn_route = json_pack("{s:s, s:s, s:O}",
        "route_service", route_service,
        "route_child", route_child,
        "__md_iev__", kw_get_dict(gobj, kw, "__md_iev__", 0, KW_REQUIRED)
    );
    if(!jn_route) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "cannot create route",
            "route_service","%s", route_service,
            "route_child",  "%s", route_child,
            "kw",           "%j", kw,
            NULL
        );
        return -1;
    }


    json_object_set_new(jn_routes, route_name, jn_route);

    /*
     *  add in input gate
     */
    return add_console_in_input_gate(gobj, name, src);
}

/***************************************************************************
 *  Delete route in local list and input gate
 ***************************************************************************/
PRIVATE int remove_console_route(
    hgobj gobj,
    const char *name,
    const char *route_service,
    const char *route_child
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_console_ = kw_get_dict(gobj, priv->list_consoles, name, 0, 0);
    json_t *jn_routes = kw_get_dict(gobj, jn_console_, "routes", 0, KW_REQUIRED);

    char route_name[NAME_MAX];
    snprintf(route_name, sizeof(route_name), "%s.%s", route_service, route_child);

    /*
     *  delete in local list
     */
    if(kw_has_key(jn_routes, route_name)) {
        json_object_del(jn_routes, route_name);
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "route not exist in local list",
            "name",         "%s", route_name,
            NULL
        );
    }

    /*
     *  delete in input gate
     */
    hgobj gobj_route_service = gobj_find_service(route_service, TRUE);
    if(gobj_route_service) {
        hgobj gobj_input_gate = gobj_child_by_name(gobj_route_service, route_child);
        if(gobj_input_gate) {
            json_t *consoles = gobj_kw_get_user_data(gobj_input_gate, "consoles", 0, 0);
            if(consoles) {
                json_object_del(consoles, route_name);
            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "no route found in child gobj",
                    "route_name",   "%s", route_name,
                    "service",      "%s", route_service,
                    "child",        "%s", route_child,
                    NULL
                );
            }
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "no route child gobj found",
                "service",      "%s", route_service,
                "child",        "%s", route_child,
                NULL
            );
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int add_console_in_input_gate(hgobj gobj, const char *name, hgobj src)
{
    char name_[NAME_MAX];
    snprintf(name_, sizeof(name_), "consoles`%s", name);
    gobj_kw_get_user_data( // save in input gate
        src,
        name_,
        json_true(), // owned
        KW_CREATE
    );

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int delete_console(hgobj gobj, const char *name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  delete in local list
     */
    json_t *jn_console = kw_get_dict(gobj, priv->list_consoles, name, 0, KW_EXTRACT);

    hgobj gobj_console = gobj_find_service(name, FALSE);

    if(!jn_console) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "no console conf found",
            "name",         "%s", name,
            NULL
        );
        if(gobj_console) {
            gobj_stop(gobj_console); // volatil, auto-destroy
        }
        return -1;
    }

    /*
     *  delete routes in input gates
     */
    json_t *jn_routes = kw_get_dict(gobj, jn_console, "routes", 0, KW_REQUIRED);

    const char *route; json_t *jn_route; void *n;
    json_object_foreach_safe(jn_routes, n, route, jn_route) {
        const char *route_service = kw_get_str(gobj, jn_route, "route_service", "", KW_REQUIRED);
        const char *route_child = kw_get_str(gobj, jn_route,  "route_child", "", KW_REQUIRED);
        hgobj gobj_route_service = gobj_find_service(route_service, TRUE);
        if(gobj_route_service) {
            hgobj gobj_input_gate = gobj_child_by_name(gobj_route_service, route_child);
            if(!gobj_input_gate) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "no route child found",
                    "service",      "%s", route_service,
                    "child",        "%s", route_child,
                    NULL
                );
                continue;
            }
            json_t *consoles = gobj_kw_get_user_data(gobj_input_gate, "consoles", 0, 0);
            if(consoles) {
                json_object_del(consoles, name);
            }

        }
    }

    if(gobj_console) {
        gobj_stop(gobj_console); // volatil, auto-destroy
    }
    json_decref(jn_console);

    return 0;
}

/***************************************************************************
 *  From input gate
 ***************************************************************************/
PRIVATE int delete_consoles_on_disconnection(hgobj gobj, json_t *kw, hgobj src_)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj gobj_channel = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    json_t *consoles = gobj_kw_get_user_data(gobj_channel, "consoles", 0, 0);
    if(!consoles) {
        return 0;
    }

    const char *route_service = gobj_name(gobj_nearest_top_service(gobj_channel));
    const char *route_child = gobj_name(gobj_channel);

    const char *name; json_t *jn_; void *n;
    json_object_foreach_safe(consoles, n, name, jn_) {
        json_t *jn_console = kw_get_dict(gobj, priv->list_consoles, name, 0, 0);

        BOOL hold_open = kw_get_bool(gobj, jn_console, "hold_open", 0, 0);
        if(hold_open) {
            remove_console_route(gobj, name, route_service, route_child);
        } else {
            delete_console(gobj, name);
        }
    }

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
    delete_consoles_on_disconnection(gobj, kw, src);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tty_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_console = kw_get_dict(gobj, priv->list_consoles, gobj_name(src), 0, KW_REQUIRED);
    if(jn_console) {
        json_t *jn_routes = kw_get_dict(gobj, jn_console, "routes", 0, KW_REQUIRED);

        const char *route_name; json_t *jn_route;
        json_object_foreach(jn_routes, route_name, jn_route) {
            const char *route_service = kw_get_str(gobj, jn_route, "route_service", "", KW_REQUIRED);
            const char *route_child = kw_get_str(gobj, jn_route,  "route_child", "", KW_REQUIRED);
            hgobj gobj_route_service = gobj_find_service(route_service, TRUE);
            if(!gobj_route_service) {
                continue;
            }
            hgobj gobj_input_gate = gobj_child_by_name(gobj_route_service, route_child);
            if(!gobj_input_gate) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "no route child found",
                    "service",      "%s", route_service,
                    "child",        "%s", route_child,
                    NULL
                );
                continue;
            }

            gobj_send_event(
                gobj_input_gate,
                EV_TTY_OPEN,
                msg_iev_build_response(gobj,
                    0,  // result
                    0,  // comment
                    0,  // schema
                    json_incref(kw), // owned
                    json_incref(jn_route)  // owned
                ),
                gobj
            );
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tty_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_is_shutdowning()) {
        KW_DECREF(kw);
        return 0;
    }

    json_t *jn_console = kw_get_dict(gobj, priv->list_consoles, gobj_name(src), 0, KW_EXTRACT);
    if(jn_console) {
        json_t *jn_routes = kw_get_dict(gobj, jn_console, "routes", 0, KW_REQUIRED);

        const char *route_name; json_t *jn_route; void *n;
        json_object_foreach_safe(jn_routes, n, route_name, jn_route) {
            const char *route_service = kw_get_str(gobj, jn_route, "route_service", "", KW_REQUIRED);
            const char *route_child = kw_get_str(gobj, jn_route,  "route_child", "", KW_REQUIRED);
            hgobj gobj_route_service = gobj_find_service(route_service, TRUE);
            if(!gobj_route_service) {
                continue;
            }
            hgobj gobj_input_gate = gobj_child_by_name(gobj_route_service, route_child);
            if(!gobj_input_gate) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "no route child found",
                    "service",      "%s", route_service,
                    "child",        "%s", route_child,
                    NULL
                );
                continue;
            }

            json_t *consoles = gobj_kw_get_user_data(gobj_input_gate, "consoles", 0, 0);

            if(consoles) {
                json_object_del(consoles, gobj_name(src));
            }

            gobj_send_event(
                gobj_input_gate,
                EV_TTY_CLOSE,
                msg_iev_build_response(gobj,
                    0,  // result
                    0,  // comment
                    0,  // schema
                    json_incref(kw), // owned
                    json_incref(jn_route)  // owned
                ),
                gobj
            );
        }

        json_decref(jn_console);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tty_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_console = kw_get_dict(gobj, priv->list_consoles, gobj_name(src), 0, KW_REQUIRED);
    if(jn_console) {
        json_t *jn_routes = kw_get_dict(gobj, jn_console, "routes", 0, KW_REQUIRED);

        const char *route_name; json_t *jn_route;
        json_object_foreach(jn_routes, route_name, jn_route) {
            const char *route_service = kw_get_str(gobj, jn_route, "route_service", "", KW_REQUIRED);
            const char *route_child = kw_get_str(gobj, jn_route,  "route_child", "", KW_REQUIRED);
            hgobj gobj_route_service = gobj_find_service(route_service, TRUE);
            if(!gobj_route_service) {
                continue;
            }
            hgobj gobj_input_gate = gobj_child_by_name(gobj_route_service, route_child);
            if(!gobj_input_gate) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "no route child found",
                    "service",      "%s", route_service,
                    "child",        "%s", route_child,
                    NULL
                );
                continue;
            }

            gobj_send_event(
                gobj_input_gate,
                EV_TTY_DATA,
                msg_iev_build_response(gobj,
                    0,  // result
                    0,  // comment
                    0,  // schema
                    json_incref(kw), // owned
                    json_incref(jn_route)  // owned
                ),
                gobj
            );
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_write_tty(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    const char *name= kw_get_str(gobj, kw, "name", 0, 0);
    const char *content64 = kw_get_str(gobj, kw, "content64", 0, 0);
    if(empty_string(content64)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "content64 required",
            "name",         "%s", name,
            NULL
        );
        gobj_send_event(src, EV_DROP, 0, gobj);
        KW_DECREF(kw);
        return 0;
    }

    hgobj gobj_console = gobj_find_service(name, FALSE);
    if(!gobj_console) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "console not found",
            "name",         "%s", name,
            NULL
        );
        gobj_send_event(src, EV_DROP, 0, gobj);
        KW_DECREF(kw);
        return 0;
    }

    gbuffer_t *gbuf = gbuffer_base64_to_string(content64, strlen(content64));
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "Bad data",
            "name",         "%s", name,
            NULL
        );
        gobj_send_event(src, EV_DROP, 0, gobj);
        KW_DECREF(kw);
        return 0;
    }

    json_t *kw_tty = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    gobj_send_event(gobj_console, EV_WRITE_TTY, kw_tty, gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
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
    .mt_destroy     = mt_destroy,
    .mt_start       = mt_start,
    .mt_stop        = mt_stop,
    .mt_writing     = mt_writing,
    .mt_trace_on    = mt_trace_on,
    .mt_trace_off   = mt_trace_off,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_AGENT22);

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
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
        {EV_ON_OPEN,         ac_on_open,     0},
        {EV_ON_CLOSE,        ac_on_close,    0},
        {EV_TTY_DATA,        ac_tty_data,    0},
        {EV_TTY_OPEN,        ac_tty_open,    0},
        {EV_TTY_CLOSE,       ac_tty_close,   0},
        {EV_WRITE_TTY,       ac_write_tty,   0},
        {EV_TIMEOUT,         ac_timeout,     0},
        {EV_STOPPED,         0,              0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,            st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_PLAY_YUNO_ACK,       EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {EV_PAUSE_YUNO_ACK,      EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {EV_MT_STATS_ANSWER,     EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT},
        {EV_MT_COMMAND_ANSWER,   EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT},

        {EV_ON_OPEN,             0},
        {EV_ON_CLOSE,            0},
        {EV_TTY_DATA,            0},
        {EV_TTY_OPEN,            0},
        {EV_TTY_CLOSE,           0},
        {EV_WRITE_TTY,           0},
        {EV_TIMEOUT,             0},
        {EV_STOPPED,             0},
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
        authz_table,
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
PUBLIC int register_c_agent22(void)
{
    return create_gclass(C_AGENT22);
}
