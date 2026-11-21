/***********************************************************************
 *          C_AGENT22.C
 *          Agent22 GClass.
 *
 *          Yuneta Agent22, a second remote entry point for controlcenter
 *
 *          Copyright (c) 2022 Niyamaka.
 *          Copyright (c) 2025-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <grp.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <c_pty.h>
#include <c_tcp.h>
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

/*
 *  The main agent, as the packages install it, and the tree that tells a
 *  node that builds from source (the test of the packages' preinst)
 */
#define AGENT_DIR           "/yuneta/agent"
#define AGENT_UPGRADE_DIR   "/yuneta/agent/upgrade"
/*
 *  install-package: agent22 writes the url and the sha256 in PACKAGE_DIR
 *  (yuneta's); PACKAGE_PROGRAM (root's, in the package) checks them, copies
 *  them to PACKAGE_STATE_DIR (root's) and installs from there, and writes
 *  its `result` there. An agent22 of 7.31.29 or before wrote the script and
 *  root wrote the result in PACKAGE_DIR: PACKAGE_LEGACY_SCRIPT and its result.
 */
#define PACKAGE_DIR         "/yuneta/agent/install-package"
#define PACKAGE_PROGRAM     "/usr/libexec/yuneta/install-package"
#define PACKAGE_STATE_DIR   "/var/lib/yuneta/install-package"
#define PACKAGE_LEGACY_SCRIPT "install-package.sh"
#define PACKAGE_UNIT_PREFIX "yuneta-install-package-"
#define MAIN_AGENT          "yuneta_agent"
#define MAIN_AGENT_UNIT     "yuneta_agent.service"
#define YUNETAS_SOURCE_DIR  "/yuneta/development/yunetas"


/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int is_yuneta_agent22(pid_t pid);
PRIVATE void remove_pid_file(void);
PRIVATE hgobj console_route_gate(hgobj gobj, json_t *jn_route);
PRIVATE void console_release_pauses_of(hgobj gobj, hgobj transport);

GOBJ_DECLARE_EVENT(EV_CLOSE_CONSOLE);
PRIVATE int add_console_in_input_gate(hgobj gobj, const char *name, hgobj src);
PRIVATE int add_console_route(
    hgobj gobj,
    const char *name,
    json_t *jn_console,
    hgobj src,
    json_t *kw
);
PRIVATE int delete_console(hgobj gobj, const char *name);
PRIVATE BOOL node_builds_from_source(void);
PRIVATE BOOL version_names_a_file(const char *version);
PRIVATE json_t *binary_role_info(hgobj gobj, const char *path);
PRIVATE int restart_main_agent(hgobj gobj);
PRIVATE long main_agent_unit_pid(void);
PRIVATE int copy_running_main_agent(hgobj gobj, char *path, size_t path_size);
PRIVATE const char *package_file_name(const char *url, char *bf, size_t bfsize);
PRIVATE int write_small_file(hgobj gobj, const char *dir, const char *name, const char *text, int mode);
PRIVATE int read_small_file(hgobj gobj, const char *dir, const char *name, char *bf, size_t bfsize);
PRIVATE json_t *command_output_lines(const char *command);

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
PRIVATE json_t *cmd_upgrade_agent(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_install_package(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_install_package_status(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

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
SDATAPM (DTP_INTEGER,   "cx",           0,              "80",       "Columns"),
SDATAPM (DTP_INTEGER,   "cy",           0,              "24",       "Rows"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_close_console[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "name",         0,              "",         "Name of console"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_write_tty[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
    SDATAPM (DTP_STRING,    "name",         0,              0,          "Name of console"),
    SDATAPM (DTP_STRING,    "content64",    SDF_SECRET,     0,          "Content64 data to write to tty: keystrokes, a typed password among them"),
    SDATA_END()
};
PRIVATE sdata_desc_t pm_upgrade_agent[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "content64",    0,              0,          "The new yuneta_agent binary in base64. Use content64=$$(/yuneta/agent/yuneta_agent)"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_install_package[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "url",          0,              0,          "https url of the .deb or .rpm of yuneta-agent (the asset of a GitHub release)"),
SDATAPM (DTP_STRING,    "sha256",       0,              0,          "sha256 of the package, 64 hex digits: it is installed only if the download has it"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_install_package_status[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_INTEGER,   "lines",        0,              "40",       "Lines of the journal of the installs to show"),
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
SDATACM2 (DTP_SCHEMA,    "install-package", 0,                  0,                  pm_install_package,cmd_install_package,"Download a yuneta-agent package (.deb or .rpm) from an https url, check its sha256 and its name and install it, as root in a transient systemd unit started by " PACKAGE_PROGRAM " (in the package): its postinst restarts the main agent and then this one. Only on a node that runs from a package. The answer comes at once: install-package-status says how it went"),
SDATACM2 (DTP_SCHEMA,    "install-package-status",0,            0,                  pm_install_package_status,cmd_install_package_status,"How the last install-package went: its result, its units, the end of its journal, and the version of yuneta-agent installed"),
SDATACM2 (DTP_SCHEMA,    "upgrade-agent",   0,                  0,                  pm_upgrade_agent,cmd_upgrade_agent,"Replace the binary of the main agent (yuneta_agent) and restart it in its systemd unit. Only on a node that builds from source: on a package node the agent comes in the package"),

SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default---------description---------- */
SDATA (DTP_STRING,      "__username__",     SDF_RD,             "",             "Username 'yuneta', permission for all"),
SDATA (DTP_INTEGER,     "max_consoles",     SDF_WR,             "30",           "Maximum consoles opened"),
SDATA (DTP_INTEGER,     "console_pause_tx_queue",SDF_WR,        "256",          "Flow control of a console: when the link to the control center has this many messages or more waiting to be sent, the console stops reading its terminal (its process waits) until the link has sent them all, or drops its connection. 0: no flow control"),
SDATA (DTP_INTEGER,     "console_max_tx_queue",SDF_WR,          "8192",         "Most messages the link may have waiting to be sent when a console writes, whatever paused it: over it the console is closed, with a warning. A message carries up to 1 KB of console output. 0: no bound"),
SDATA (DTP_INTEGER,     "console_pause_tx_bytes",SDF_WR,        "1048576",      "Flow control of a console, in bytes: when the link has this many bytes or more waiting to be sent (its transport's cur_tx_bytes), the console pauses as with console_pause_tx_queue. A message of a console is about 2 KB, so 256 of them (console_pause_tx_queue) come first. 0: none"),
SDATA (DTP_INTEGER,     "console_max_tx_bytes",SDF_WR,          "33554432",     "Most bytes the link may have waiting to be sent when a console writes, whatever paused it: over it the console is closed as with console_max_tx_queue. 0: no bound"),
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
SDATAAUTHZ (DTP_SCHEMA, "install-package",  0,      0,      0,      "Permission to install a yuneta-agent package, and to see how it went. Equivalent to root: the package is the one of the url and sha256 given, and its scripts run as root"),
SDATAAUTHZ (DTP_SCHEMA, "upgrade-agent",    0,      0,      0,      "Permission to replace the binary of the main agent and restart it. Equivalent to root where yuneta has sudo (the default of the package): the main agent runs the binary given, as yuneta"),
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
    json_t *console_transports; // the transports (C_TCP) agent22 takes EV_TX_READY of: {"<pointer>": true}

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

    /*----------------------------------------*
     *  Check node_owner
     *----------------------------------------*/
    const char *node_owner = gobj_yuno_node_owner();
    if(empty_string(node_owner)) {
        node_owner = "none";
        gobj_write_str_attr(gobj_yuno(), "node_owner", node_owner);

        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "yuneta_agent22: node_owner EMPTY, exiting",
            NULL
        );
        print_error(0, "yuneta_agent22: node_owner EMPTY, exiting");
        exit(0);
    }

    /*----------------------------------------*
     *      Check user yuneta
     *----------------------------------------*/
    gobj_write_str_attr(
        gobj,
        "__username__",
        gobj_read_str_attr(gobj_yuno(), "__username__")
    );

    /*---------------------------------------*
     *      Check if already running
     *---------------------------------------*/
    {
        int pid = 0;

        FILE *file = fopen(pidfile, "r");
        if(file) {
            int x = fscanf(file, "%d", &pid);
            fclose(file);

            if(x != 1 || pid <= 0) {
                // pid 0 would be read as this very process (/proc/self)
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_OPERATIONAL,
                    "msg",          "%s", "pid file without a pid, removed",
                    "pid_file",     "%s", pidfile,
                    NULL
                );
                unlink(pidfile);
            } else if(pid != getpid()) {
                int ret = is_yuneta_agent22(pid);
                if(ret == 0) {
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INFO,
                        "msg",          "%s", "yuneta_agent22 already running, exiting",
                        "pid_in_file",  "%d", pid,
                        NULL
                    );
                    exit(0);
                } else if(ret == 1) {
                    unlink(pidfile);
                } else if(ret == 2) {
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_OPERATIONAL,
                        "msg",          "%s", "pid file left by an agent that did not end in order: its pid is not an agent now (killed and not yet reaped, or taken by another program), removed",
                        "pid_in_file",  "%d", pid,
                        NULL
                    );
                    unlink(pidfile);
                } else {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SYSTEM,
                        "msg",          "%s", "cannot check pid",
                        "pid_in_file",  "%d", pid,
                        "errno",        "%d", errno,
                        "serrno",       "%s", strerror(errno),
                        NULL
                    );
                    unlink(pidfile);
                }
            }
        }
        file = fopen(pidfile, "w");
        if(file) {
            fprintf(file, "%d\n", getpid());
            fclose(file);
        }

        if (!atexit_registered) {
            atexit(remove_pid_file);
            atexit_registered = 1;
        }
    }

    /*-----------------------------*
     *      Consoles
     *-----------------------------*/
    priv->list_consoles = json_object();
    priv->console_transports = json_object();

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
    JSON_DECREF(priv->console_transports);

    remove_pid_file();
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    /*----------------------------------------*
     *  Start controlcenter if it has owner
     *----------------------------------------*/
    const char *node_owner = gobj_yuno_node_owner();
    if(strcasecmp(node_owner, "none")!=0) {
        hgobj gobj_controlcenter = gobj_find_service("controlcenter", TRUE);
        /*
         *  The card the control center sees says the os of this node
         *  (list-agents, list-nodes)
         */
        json_t *jn_extra_info = json_deep_copy(gobj_read_json_attr(gobj_controlcenter, "extra_info"));
        if(!json_is_object(jn_extra_info)) {
            JSON_DECREF(jn_extra_info)
            jn_extra_info = json_object();
        }
        json_object_set_new(jn_extra_info, "os", get_os_info());
        gobj_write_new_json_attr(gobj_controlcenter, "extra_info", jn_extra_info);
        gobj_start_tree(gobj_controlcenter);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Close the live consoles (volatil C_PTY services): left running
     *  they reach gobj_end still started ("Destroying a RUNNING gobj").
     *  HACK copy the key: delete_console KW_EXTRACTs it from
     *  list_consoles and the original string dies mid-call.
     */
    const char *name; json_t *jn_console; void *n;
    json_object_foreach_safe(priv->list_consoles, n, name, jn_console) {
        char *name_ = gbmem_strdup(name);   // any length: a gobj name can exceed NAME_MAX - 1
        delete_console(gobj, name_);
        GBMEM_FREE(name_);
    }

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
            json_sprintf("%s: No permission to '%s' in service '%s'",
                gobj_yuno_role_plus_name(), permission, gobj_name(gobj)),
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
                        "msgset",       "%s", MSGSET_INTERNAL,
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
        json_sprintf("%s: ==> List consoles of agent22: '%s'",
            gobj_yuno_role_plus_name(), node_uuid()),
        0,
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *  The dict named `key` in `dict`. A console (or a route) is named by the
 *  user: the name is a KEY, as it is written (json_object_set), never a
 *  path -- read as one, a name holding the delimiter was not found, or
 *  found a field of another console.
 ***************************************************************************/
PRIVATE json_t *named_dict(hgobj gobj, json_t *dict, const char *key, BOOL required)
{
    json_t *jn = json_object_get(dict, key);
    if(json_is_object(jn)) {
        return jn;
    }
    if(required) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "named dict not found",
            "key",          "%s", key,
            NULL
        );
    }
    return NULL;
}

/***************************************************************************
 *  Take the dict named `key` out of `dict`: yours, NULL if not there.
 ***************************************************************************/
PRIVATE json_t *named_dict_take(json_t *dict, const char *key)
{
    json_t *jn = json_object_get(dict, key);
    if(!json_is_object(jn)) {
        return NULL;
    }
    json_incref(jn);
    json_object_del(dict, key);
    return jn;
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
            json_sprintf("%s: No permission to '%s' in service '%s'",
                gobj_yuno_role_plus_name(), permission, gobj_name(gobj)),
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
    int cx = kw_get_int(gobj, kw, "cx", 80, KW_WILD_NUMBER);
    int cy = kw_get_int(gobj, kw, "cy", 24, KW_WILD_NUMBER);

    if(empty_string(name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: What console name?", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(process)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: What process?", gobj_yuno_role_plus_name()),
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
        if(kw_size(priv->list_consoles) >= gobj_read_integer_attr(gobj, "max_consoles")) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf(
                    "%s: Too much opened consoles: %d",
                    gobj_yuno_role_plus_name(),
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
                json_sprintf("%s: Cannot open console: '%s'", gobj_yuno_role_plus_name(), name),
                0,
                0,
                kw  // owned
            );
        }
        gobj_set_volatil(gobj_console, TRUE);

        /*
         *  Save console
         */
        json_t *jn_console = json_pack("{s:s, s:{}}",
            "process", process,
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
        json_t *jn_console = named_dict(gobj, priv->list_consoles, name, TRUE);
        gobj_console = gobj_find_service(name, FALSE);
        if(!gobj_console) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("%s: Console gobj not found: '%s'",
                    gobj_yuno_role_plus_name(), name),
                0,
                0,
                kw  // owned
            );
        }
        int ret = add_console_route(gobj, name, jn_console, src, kw);
        if(ret < 0 && ret != -2) {
            // -2 is not an error: same channel re-opening its console
            // (browser refresh), the route metadata was refreshed.
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("%s: Error opening console: '%s'",
                    gobj_yuno_role_plus_name(), name),
                0,
                0,
                kw  // owned
            );
        }

        /*
         *  Re-attach: the live PTY published EV_TTY_OPEN at start only,
         *  replay it to the requester so its console leaves "Connecting…".
         */
        char route_name[NAME_MAX];
        snprintf(route_name, sizeof(route_name), "%s.%s",
            gobj_name(gobj_nearest_top_service(src)),
            gobj_name(src)
        );
        json_t *jn_routes = kw_get_dict(gobj, jn_console, "routes", 0, KW_REQUIRED);
        json_t *jn_route = named_dict(gobj, jn_routes, route_name, TRUE);
        if(jn_route) {
            // Same shape as C_PTY's EV_TTY_OPEN publish (minus fd/slave_name)
            json_t *jn_data = json_pack("{s:s, s:s, s:s, s:s, s:i, s:i}",
                "name", name,
                "process", kw_get_str(gobj, jn_console, "process", "", KW_REQUIRED),
                "uuid", node_uuid(),
                "cwd", gobj_read_str_attr(gobj_console, "cwd"),
                "rows", (int)gobj_read_integer_attr(gobj_console, "rows"),
                "cols", (int)gobj_read_integer_attr(gobj_console, "cols")
            );
            gobj_send_event(
                src,
                EV_TTY_OPEN,
                msg_iev_build_response(gobj,
                    0,  // result
                    0,  // comment
                    0,  // schema
                    jn_data,  // owned
                    json_incref(jn_route)  // owned
                ),
                gobj
            );
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
        json_sprintf("%s: Console opened: '%s'", gobj_yuno_role_plus_name(), name),  // owned
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
            json_sprintf("%s: No permission to '%s' in service '%s'",
                gobj_yuno_role_plus_name(), permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }


    /*----------------------------------------*
     *  Close console
     *----------------------------------------*/
    const char *name = kw_get_str(gobj, kw, "name", "", 0);

    if(empty_string(name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: What console name?", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    json_t *jn_console = named_dict(gobj, priv->list_consoles, name, FALSE);
    if(!jn_console) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: Console not found: '%s'", gobj_yuno_role_plus_name(), name),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Delete console or route
     */
    int ret = 0;
    hgobj gobj_console = gobj_find_service(name, TRUE);
    if(gobj_console) {
        ret = gobj_stop(gobj_console); // volatil, auto-destroy
    } else {
        ret = -1;
    }

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        ret,
        json_sprintf("%s: Console closed: '%s'", gobj_yuno_role_plus_name(), name),
        0,
        0, // owned
        kw  // owned
    );
}

/***************************************************************************
 *  Replace the binary of the main agent and restart it: the twin of the
 *  rule that each agent maintains the other. The binary is written as
 *  `/yuneta/agent/upgrade/yuneta_agent` (a yuno refuses to run under a
 *  name that is not its role, so it keeps its name, and the directory is
 *  on the same filesystem as the agent's, for the rename), must say it is
 *  a yuneta_agent (--print-role). The binary that RUNS is kept as
 *  `yuneta_agent.bak-pre-<new version>`, and its version is the one the
 *  answer gives: it is copied from /proc/<MainPID>/exe of the unit (to
 *  `upgrade/running/yuneta_agent`, the name of its role, for --print-role).
 *  A build that ran before has replaced the file on disk already, so the
 *  file is not the binary that runs. With no unit, or /proc not readable,
 *  the file on disk is kept (a hard link), and the answer says so. A
 *  backup already there is left as it is. The new binary takes its name
 *  with one rename(). Then the agent is
 *  restarted in its systemd unit (`sudo -n systemctl restart --no-block`),
 *  so this answers before the main agent is back.
 *
 *  Only on a node that builds from source: on a package node what runs
 *  is what its package installed, and the agent comes in the package.
 *  An agent that runs outside its unit is not restarted: the answer says
 *  so, and the binary on disk is the new one.
 ***************************************************************************/
PRIVATE json_t *cmd_upgrade_agent(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "upgrade-agent";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: No permission to '%s' in service '%s'",
                gobj_yuno_role_plus_name(), permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    if(!node_builds_from_source()) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "%s: this node runs from a package (no sources in %s): "
                "its agent comes in the package",
                gobj_yuno_role_plus_name(), YUNETAS_SOURCE_DIR
            ),
            0,
            0,
            kw  // owned
        );
    }

    const char *content64 = kw_get_str(gobj, kw, "content64", "", 0);
    if(empty_string(content64)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: No data in content64", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    char agent_path[PATH_MAX];
    char new_path[PATH_MAX];
    if(!build_path(agent_path, sizeof(agent_path), AGENT_DIR, MAIN_AGENT, NULL) ||
            !build_path(new_path, sizeof(new_path), AGENT_UPGRADE_DIR, MAIN_AGENT, NULL)) {
        // Error already logged
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot build the paths of the agent", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    if(!is_directory(AGENT_UPGRADE_DIR) && mkrdir(AGENT_UPGRADE_DIR, yuneta_xpermission()) < 0) {
        // Error already logged
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot create '%s'", gobj_yuno_role_plus_name(), AGENT_UPGRADE_DIR),
            0,
            0,
            kw  // owned
        );
    }

    gbuffer_t *gbuf_content = gbuffer_base64_to_binary_from_peer(gobj, content64, strlen(content64));
    if(!gbuf_content) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: Bad data in content64", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  The new binary takes the mode of the one it replaces
     */
    mode_t mode = 0775;
    struct stat st;
    if(stat(agent_path, &st) == 0) {
        mode = st.st_mode & 07777;
    }
    if(gbuf2file(gobj, gbuf_content, new_path, (int)mode, TRUE) < 0 ||
            chmod(new_path, mode) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot write the new binary of the main agent",
            "path",         "%s", new_path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        unlink(new_path);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot write '%s'", gobj_yuno_role_plus_name(), new_path),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  Is it a yuneta_agent?
     *----------------------------------------*/
    json_t *new_info = binary_role_info(gobj, new_path);
    const char *new_role = new_info? kw_get_str(gobj, new_info, "role", "", 0) : "";
    if(strcmp(new_role, MAIN_AGENT) != 0) {
        json_t *comment = json_sprintf(
            "%s: not a %s binary (it says role '%s')",
            gobj_yuno_role_plus_name(), MAIN_AGENT, new_role
        );
        JSON_DECREF(new_info)
        unlink(new_path);
        return msg_iev_build_response(
            gobj,
            -1,
            comment,
            0,
            0,
            kw  // owned
        );
    }

    char new_version[NAME_MAX];
    snprintf(new_version, sizeof(new_version), "%s", kw_get_str(gobj, new_info, "version", "", 0));
    JSON_DECREF(new_info)

    if(!version_names_a_file(new_version)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "The new agent binary gives a version that cannot name its backup",
            "version",      "%s", new_version,
            NULL
        );
        unlink(new_path);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "%s: the binary says version '%s': only [A-Za-z0-9._-] allowed, "
                "not starting with '.', no '..'",
                gobj_yuno_role_plus_name(), new_version
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  The binary that runs: its version
     *----------------------------------------*/
    char running_path[PATH_MAX] = "";
    BOOL from_running = (copy_running_main_agent(gobj, running_path, sizeof(running_path)) == 0)?
        TRUE : FALSE;
    if(!from_running) {
        running_path[0] = 0;
    }
    char old_version[NAME_MAX] = "";
    json_t *old_info = binary_role_info(gobj, from_running? running_path : agent_path);
    if(old_info) {
        snprintf(old_version, sizeof(old_version), "%s", kw_get_str(gobj, old_info, "version", "", 0));
        JSON_DECREF(old_info)
    }

    /*----------------------------------------*
     *  Keep the one that runs, then replace it
     *----------------------------------------*/
    char backup_name[NAME_MAX];
    char backup_path[PATH_MAX];
    int backup_len = snprintf(backup_name, sizeof(backup_name), "%s.bak-pre-%s", MAIN_AGENT, new_version);
    if(backup_len < 0 || backup_len >= (int)sizeof(backup_name)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "The version of the new agent is too long to name its backup",
            "version",      "%s", new_version,
            NULL
        );
        unlink(new_path);
        if(from_running) {
            unlink(running_path);
        }
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: the version '%s' is too long to name the backup",
                gobj_yuno_role_plus_name(), new_version),
            0,
            0,
            kw  // owned
        );
    }
    if(!build_path(backup_path, sizeof(backup_path), AGENT_DIR, backup_name, NULL)) {
        // Error already logged
        unlink(new_path);
        if(from_running) {
            unlink(running_path);
        }
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot build the path of the backup", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    BOOL backup_was_there = (access(backup_path, F_OK) == 0)? TRUE : FALSE;
    if(!backup_was_there) {
        const char *kept = from_running? running_path : agent_path;
        int ret = from_running? rename(running_path, backup_path) : link(agent_path, backup_path);
        if(ret < 0 && !(errno == ENOENT && !from_running)) {   // no file on disk: nothing to keep
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot keep the binary of the main agent: not replaced",
                "path",         "%s", kept,
                "backup",       "%s", backup_path,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            unlink(new_path);
            if(from_running) {
                unlink(running_path);
            }
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("%s: cannot keep '%s' as '%s': not replaced",
                    gobj_yuno_role_plus_name(), kept, backup_path),
                0,
                0,
                kw  // owned
            );
        }
    } else if(from_running) {
        unlink(running_path);   // the backup there is left as it is
    }

    if(rename(new_path, agent_path) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "rename() FAILED: the main agent binary not replaced",
            "path",         "%s", new_path,
            "destination",  "%s", agent_path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        unlink(new_path);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot rename '%s' to '%s'",
                gobj_yuno_role_plus_name(), new_path, agent_path),
            0,
            0,
            kw  // owned
        );
    }

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_OPERATIONAL,
        "msg",          "%s", "Main agent binary replaced",
        "old_version",  "%s", old_version,
        "old_from",     "%s", from_running? "running" : "disk",
        "new_version",  "%s", new_version,
        "backup",       "%s", backup_path,
        "backup_was_there", "%d", backup_was_there,
        NULL
    );

    /*----------------------------------------*
     *  Restart it in its unit
     *----------------------------------------*/
    int restarted = restart_main_agent(gobj);
    const char *restart;
    switch(restarted) {
        case 0:
            restart = "restart of " MAIN_AGENT_UNIT " queued";
            break;
        case 1:
            restart = "it runs outside its systemd unit: NOT restarted, restart it by hand";
            break;
        default:
            restart = "the restart of " MAIN_AGENT_UNIT " FAILED: the binary on disk is the new one";
            break;
    }

    json_t *jn_data = json_pack("{s:s, s:s, s:s, s:s, s:b, s:s}",
        "old_version", old_version,
        "old_from", from_running? "running" : "disk",
        "new_version", new_version,
        "backup", backup_path,
        "backup_was_there", backup_was_there,
        "restart", restart
    );
    return msg_iev_build_response(
        gobj,
        (restarted < 0)? -1 : 0,
        json_sprintf("%s: %s %s -> %s, the %s kept as %s%s; %s",
            gobj_yuno_role_plus_name(),
            MAIN_AGENT,
            empty_string(old_version)? "?" : old_version,
            new_version,
            from_running? "one that runs" : "one on disk (NOT the one that runs: see the log)",
            backup_path,
            backup_was_there? " (a backup already there, left as it was)" : "",
            restart
        ),
        0,
        jn_data, // owned
        kw  // owned
    );
}




/***************************************************************************
 *  Install a yuneta-agent package from an https url: the way a package
 *  node changes its agent (on such a node what runs is what the package
 *  installed). It runs as root in a transient systemd unit
 *  (`yuneta-install-package-<time>`), out of this agent's cgroup: the
 *  postinst restarts the main agent and then this one, and the install
 *  goes on. So this answers at once, and install-package-status says how
 *  it went.
 *
 *  agent22 writes only data: the url and the sha256, in PACKAGE_DIR. The
 *  root part is PACKAGE_PROGRAM, root's, in the package (`sudo -n
 *  PACKAGE_PROGRAM start`): it checks the data again, refuses a link,
 *  copies the data to PACKAGE_STATE_DIR (root's) and starts the unit, which
 *  downloads there, checks the sha256 and that the package is yuneta-agent,
 *  and installs it. Nothing of the operator's goes through a command line
 *  (systemd expands '%' and '$' there). Refused on a node that builds from
 *  source (the preinst refuses it too), on a package with no
 *  PACKAGE_PROGRAM (7.31.29 or before), and by the program while another
 *  install runs, or when the node lacks curl or the installer.
 ***************************************************************************/
PRIVATE json_t *cmd_install_package(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *permission = "install-package";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: No permission to '%s' in service '%s'",
                gobj_yuno_role_plus_name(), permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    if(node_builds_from_source()) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "%s: this node builds from source (%s): it does not take a package, "
                "its agent is upgraded with upgrade-agent",
                gobj_yuno_role_plus_name(), YUNETAS_SOURCE_DIR
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  The url and the sha256
     *----------------------------------------*/
    const char *url = kw_get_str(gobj, kw, "url", "", 0);
    const char *sha256 = kw_get_str(gobj, kw, "sha256", "", 0);
    char name[NAME_MAX];
    if(!package_file_name(url, name, sizeof(name))) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "%s: url must be https://, with no blank, quote, backslash nor control or "
                "non-ASCII character, its last part the name of a .deb or a .rpm "
                "([A-Za-z0-9._+~-]): '%s'",
                gobj_yuno_role_plus_name(), url
            ),
            0,
            0,
            kw  // owned
        );
    }

    char sha_lower[65];
    BOOL sha_ok = (strlen(sha256) == 64)? TRUE : FALSE;
    for(int i = 0; sha_ok && i < 64; i++) {
        char c = sha256[i];
        if(c >= 'A' && c <= 'F') {
            c = (char)(c - 'A' + 'a');
        }
        if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            sha_ok = FALSE;
        }
        sha_lower[i] = c;
    }
    sha_lower[64] = 0;
    if(!sha_ok) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: sha256 must be 64 hex digits", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  The root part comes in the package
     *----------------------------------------*/
    if(access(PACKAGE_PROGRAM, X_OK) != 0) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "install-package refused: the package of this node has no " PACKAGE_PROGRAM,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "%s: the package of this node has no %s: nothing done. "
                "Upgrade the package by hand once (install.sh, or apt-get/dnf install as root)",
                gobj_yuno_role_plus_name(), PACKAGE_PROGRAM
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  The data the program reads
     *----------------------------------------*/
    if(!is_directory(PACKAGE_DIR) && mkrdir(PACKAGE_DIR, yuneta_xpermission()) < 0) {
        // Error already logged
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot create '%s'", gobj_yuno_role_plus_name(), PACKAGE_DIR),
            0,
            0,
            kw  // owned
        );
    }
    const char *legacy[] = {PACKAGE_LEGACY_SCRIPT, "result", "name", NULL};
    for(int i = 0; legacy[i]; i++) {
        char legacy_path[PATH_MAX];
        if(build_path(legacy_path, sizeof(legacy_path), PACKAGE_DIR, legacy[i], NULL)) {
            unlink(legacy_path);    // what an agent22 of 7.31.29 or before left, if any
        }
    }
    if(write_small_file(gobj, PACKAGE_DIR, "url", url, 0644) < 0 ||
            write_small_file(gobj, PACKAGE_DIR, "sha256", sha_lower, 0644) < 0) {
        // Error already logged
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot write the data of the install in '%s'",
                gobj_yuno_role_plus_name(), PACKAGE_DIR),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  Start it: "started <unit> <file>"
     *----------------------------------------*/
    json_t *output = command_output_lines("/usr/bin/sudo -n " PACKAGE_PROGRAM " start 2>&1");
    char unit[NAME_MAX] = "";
    size_t idx;
    json_t *jn_line;
    json_array_foreach(output, idx, jn_line) {
        const char *line = json_string_value(jn_line);
        if(line && strncmp(line, "started ", 8) == 0) {
            const char *p = line + 8;
            size_t len = strcspn(p, " ");
            if(len > 0 && len < sizeof(unit)) {
                memcpy(unit, p, len);
                unit[len] = 0;
            }
        }
    }
    if(empty_string(unit)) {
        size_t n = json_array_size(output);
        const char *last = n > 0? json_string_value(json_array_get(output, n - 1)) : NULL;
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "install-package not started: package not installed",
            "output",       "%s", last? last : "",
            NULL
        );
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: install-package not started: %s",
                gobj_yuno_role_plus_name(), last? last : "the program said nothing"),
            0,
            output, // owned
            kw  // owned
        );
    }
    JSON_DECREF(output)

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_OPERATIONAL,
        "msg",          "%s", "yuneta-agent package install started",
        "unit",         "%s", unit,
        "url",          "%s", url,
        "sha256",       "%s", sha_lower,
        NULL
    );

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf(
            "%s: installing %s in the unit %s. Its postinst restarts the main agent "
            "and then this one: ask install-package-status when this agent is back",
            gobj_yuno_role_plus_name(), name, unit
        ),
        0,
        json_pack("{s:s, s:s, s:s}", "unit", unit, "name", name, "sha256", sha_lower),
        kw  // owned
    );
}

/***************************************************************************
 *  How the last install-package went: what the program wrote in `result`
 *  ("done <rc>", "download-failed <rc>", "sha256-mismatch",
 *  "not-yuneta-agent <file>", "refused <why>", "" while it runs or when
 *  none ran), its units, the end of its journal (they read it from the
 *  journal: this agent was restarted by the install it started), and the
 *  version of yuneta-agent installed. The result of an install started by
 *  an agent22 of 7.31.29 or before is in PACKAGE_DIR: it is read when
 *  PACKAGE_STATE_DIR has none.
 ***************************************************************************/
PRIVATE json_t *cmd_install_package_status(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *permission = "install-package";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: No permission to '%s' in service '%s'",
                gobj_yuno_role_plus_name(), permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    int lines = (int)kw_get_int(gobj, kw, "lines", 40, 0);
    if(lines <= 0 || lines > 1000) {
        lines = 40;
    }

    char result[256] = "";
    if(read_small_file(gobj, PACKAGE_STATE_DIR, "result", result, sizeof(result)) < 0) {
        read_small_file(gobj, PACKAGE_DIR, "result", result, sizeof(result));
    }

    char journal_cmd[256];
    if(access(PACKAGE_PROGRAM, X_OK) == 0) {
        snprintf(journal_cmd, sizeof(journal_cmd),
            "/usr/bin/sudo -n " PACKAGE_PROGRAM " journal %d 2>&1",
            lines
        );
    } else {
        snprintf(journal_cmd, sizeof(journal_cmd),
            "sudo -n journalctl -u '" PACKAGE_UNIT_PREFIX "*' -n %d --no-pager -o short-iso 2>&1",
            lines
        );
    }

    json_t *installed = command_output_lines(
        "dpkg-query -W -f='${Version}\\n' yuneta-agent 2>/dev/null || rpm -q yuneta-agent 2>/dev/null"
    );

    json_t *jn_data = json_pack("{s:s, s:o, s:o, s:o}",
        "result", result,
        "installed", json_string(json_string_value(json_array_get(installed, 0))?
            json_string_value(json_array_get(installed, 0)) : ""),
        "units", command_output_lines(
            "systemctl list-units '" PACKAGE_UNIT_PREFIX "*' --all --no-legend --plain 2>/dev/null"
        ),
        "journal", command_output_lines(journal_cmd)
    );
    JSON_DECREF(installed)

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: install-package: %s",
            gobj_yuno_role_plus_name(), empty_string(result)? "no result (running, or none ran)" : result),
        0,
        jn_data, // owned
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
    /*
     *  0 an agent22 runs with the pid, 1 nothing does, 2 something that is
     *  not an agent22 does: a zombie (its cmdline is empty) or another
     *  program; -1 cannot tell (errno). EPERM is an agent22 of another user
     *  as much as a stranger: its cmdline is readable, so it is read.
     */
    if(kill(pid, 0) < 0 && errno != EPERM) {
        return (errno == ESRCH)? 1 : -1;
    }
    char cmdline[1024];
    if(read_process_cmdline(cmdline, sizeof(cmdline), pid) < 0) {
        return (errno == ENOENT || errno == ESRCH)? 1 : -1;
    }
    if(strstr(cmdline, "yuneta_agent22 ")) {
        return 0;
    }
    return 2;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void remove_pid_file(void)
{
    unlink(pidfile);
}

/***************************************************************************
 *  A node that builds from source: the test of the packages' preinst
 ***************************************************************************/
PRIVATE BOOL node_builds_from_source(void)
{
    char kernel_dir[PATH_MAX];
    char git_path[PATH_MAX];
    if(!build_path(kernel_dir, sizeof(kernel_dir), YUNETAS_SOURCE_DIR, "kernel", NULL) ||
            !build_path(git_path, sizeof(git_path), YUNETAS_SOURCE_DIR, ".git", NULL)) {
        return FALSE;   // Error already logged
    }
    return is_directory(kernel_dir) && access(git_path, F_OK) == 0;
}

/***************************************************************************
 *  A version a file can be named after: [A-Za-z0-9._-], not starting
 *  with '.', no ".."
 ***************************************************************************/
PRIVATE BOOL version_names_a_file(const char *version)
{
    if(empty_string(version) || version[0] == '.' || strstr(version, "..")) {
        return FALSE;
    }
    for(const char *c = version; *c; c++) {
        BOOL ok = (*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'z') ||
            (*c >= 'A' && *c <= 'Z') || *c == '.' || *c == '_' || *c == '-';
        if(!ok) {
            return FALSE;
        }
    }
    return TRUE;
}

/***************************************************************************
 *  What a binary says of itself (`<path> --print-role`): a dict with its
 *  role and version, yours, or NULL (logged). `path` is one of the fixed
 *  paths of the agent, so it needs no quoting for the shell.
 ***************************************************************************/
PRIVATE json_t *binary_role_info(hgobj gobj, const char *path)
{
    char command[PATH_MAX + 32];
    if(snprintf(command, sizeof(command), "%s --print-role", path) >= (int)sizeof(command)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Path of the binary too long",
            "path",         "%s", path,
            NULL
        );
        return NULL;
    }

    gbuffer_t *gbuf = run_command(command);
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "The binary did not run",
            "command",      "%s", command,
            NULL
        );
        return NULL;
    }
    char *output = gbuffer_cur_rd_pointer(gbuf);
    char *p = output? strchr(output, '{') : NULL;
    if(!p) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "The binary does not say its role",
            "command",      "%s", command,
            "output",       "%s", output? output : "",
            NULL
        );
        gbuffer_decref(gbuf);
        return NULL;
    }
    json_t *jn_info = legalstring2json(p, TRUE);
    gbuffer_decref(gbuf);
    return jn_info;
}

/***************************************************************************
 *  Restart the main agent in its systemd unit, without waiting for it:
 *  0 queued, 1 it runs outside its unit (or there is no systemd), so it
 *  is not restarted, -1 the restart failed (logged).
 ***************************************************************************/
PRIVATE int restart_main_agent(hgobj gobj)
{
    long main_pid = main_agent_unit_pid();
    if(main_pid <= 0) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL,
            "msg",          "%s", "The main agent runs outside its systemd unit: not restarted",
            "unit",         "%s", MAIN_AGENT_UNIT,
            NULL
        );
        return 1;
    }

    char *const argv[] = {
        "sudo", "-n", "systemctl", "restart", "--no-block", MAIN_AGENT_UNIT, NULL
    };
    int status = run_process2("/usr/bin/sudo", argv);
    if(status < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "sudo -n systemctl restart FAILED",
            "unit",         "%s", MAIN_AGENT_UNIT,
            "status",       "%d", status,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  The MainPID of the unit of the main agent (its watcher): 0 when the
 *  unit does not run it, or there is no systemctl
 ***************************************************************************/
PRIVATE long main_agent_unit_pid(void)
{
    gbuffer_t *gbuf = run_command("systemctl show -p MainPID --value " MAIN_AGENT_UNIT " 2>/dev/null");
    if(!gbuf) {
        return 0;   // Error already logged
    }
    char *output = gbuffer_cur_rd_pointer(gbuf);
    long main_pid = output? strtol(output, NULL, 10) : 0;
    gbuffer_decref(gbuf);
    return main_pid;
}

/***************************************************************************
 *  Copy the binary that the main agent runs (/proc/<MainPID>/exe of its
 *  unit, the watcher: it runs the image the worker runs) to
 *  `upgrade/running/yuneta_agent`, the name of its role. 0 with its path
 *  in `path`, or -1: no unit (a WARNING), or it cannot be copied (logged).
 *  The file on disk may be another binary already: a build replaces it
 *  while the agent runs.
 ***************************************************************************/
PRIVATE int copy_running_main_agent(hgobj gobj, char *path, size_t path_size)
{
    long main_pid = main_agent_unit_pid();
    if(main_pid <= 0) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL,
            "msg",          "%s", "The main agent runs outside its systemd unit: the file on disk is kept, not the binary that runs",
            "unit",         "%s", MAIN_AGENT_UNIT,
            NULL
        );
        return -1;
    }

    char exe[64];
    snprintf(exe, sizeof(exe), "/proc/%ld/exe", main_pid);
    char running_dir[PATH_MAX];
    if(!build_path(running_dir, sizeof(running_dir), AGENT_UPGRADE_DIR, "running", NULL) ||
            !build_path(path, path_size, running_dir, MAIN_AGENT, NULL)) {
        return -1;  // Error already logged
    }
    if(!is_directory(running_dir) && mkrdir(running_dir, yuneta_xpermission()) < 0) {
        return -1;  // Error already logged
    }
    if(copyfile(exe, path, 0775, TRUE) < 0) {
        return -1;  // Error already logged
    }
    if(chmod(path, 0775) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "chmod() FAILED",
            "path",         "%s", path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        unlink(path);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  The name of the package of `url`, in bf: its last part. NULL if the url
 *  is not https://, holds a blank, a quote, a backslash, a control or a
 *  non-ASCII character (PACKAGE_PROGRAM refuses the same), or its
 *  last part is not the name of a .deb or a .rpm made of [A-Za-z0-9._+~-]
 *  (the script names the downloaded file so).
 ***************************************************************************/
PRIVATE const char *package_file_name(const char *url, char *bf, size_t bfsize)
{
    const char *https = "https://";
    if(empty_string(url) || strncmp(url, https, strlen(https)) != 0 || strlen(url) > 2048) {
        return NULL;
    }
    for(const char *c = url; *c; c++) {
        if((unsigned char)*c <= ' ' || (unsigned char)*c >= 0x7f ||
                *c == '\'' || *c == '"' || *c == '`' || *c == '\\') {
            return NULL;
        }
    }
    const char *name = strrchr(url, '/');
    if(!name || !*(name + 1)) {
        return NULL;
    }
    name++;
    size_t len = strlen(name);
    if(len < 5 || len >= bfsize || name[0] == '.') {
        return NULL;
    }
    for(const char *c = name; *c; c++) {
        BOOL ok = (*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'z') ||
            (*c >= 'A' && *c <= 'Z') || *c == '.' || *c == '_' || *c == '+' ||
            *c == '~' || *c == '-';
        if(!ok) {
            return NULL;
        }
    }
    if(strcmp(name + len - 4, ".deb") != 0 && strcmp(name + len - 4, ".rpm") != 0) {
        return NULL;
    }
    memcpy(bf, name, len + 1);
    return bf;
}

/***************************************************************************
 *  Write `text` as the file `name` in `dir`, with `mode`: 0, or -1 logged
 ***************************************************************************/
PRIVATE int write_small_file(hgobj gobj, const char *dir, const char *name, const char *text, int mode)
{
    char path[PATH_MAX];
    if(!build_path(path, sizeof(path), dir, name, NULL)) {
        return -1;  // Error already logged
    }
    size_t len = strlen(text);
    gbuffer_t *gbuf = gbuffer_create(len + 1, len + 1);
    if(!gbuf) {
        return -1;  // Error already logged
    }
    gbuffer_append(gbuf, (void *)text, len);
    if(gbuf2file(gobj, gbuf, path, mode, TRUE) < 0) {
        return -1;  // Error already logged
    }
    if(chmod(path, (mode_t)mode) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "chmod() FAILED",
            "path",         "%s", path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  The first line of the file `name` in `dir`, in bf (without its end of
 *  line): its length, or -1 (bf empty). A file that is not there is no
 *  error: no install wrote it yet. Any other failure is logged.
 ***************************************************************************/
PRIVATE int read_small_file(hgobj gobj, const char *dir, const char *name, char *bf, size_t bfsize)
{
    bf[0] = 0;
    char path[PATH_MAX];
    if(!build_path(path, sizeof(path), dir, name, NULL)) {
        return -1;  // Error already logged
    }
    FILE *file = fopen(path, "r");
    if(!file) {
        if(errno != ENOENT) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "fopen() FAILED",
                "path",         "%s", path,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
        }
        return -1;
    }
    if(!fgets(bf, (int)bfsize, file)) {
        bf[0] = 0;
    }
    fclose(file);
    size_t len = strlen(bf);
    while(len > 0 && (bf[len-1] == '\n' || bf[len-1] == '\r')) {
        bf[--len] = 0;
    }
    return (int)len;
}

/***************************************************************************
 *  The lines of what `command` writes (run by sh -c), as a list of strings:
 *  yours, never NULL (empty when it wrote nothing or did not run).
 ***************************************************************************/
PRIVATE json_t *command_output_lines(const char *command)
{
    json_t *jn_lines = json_array();
    gbuffer_t *gbuf = run_command(command);
    if(!gbuf) {
        return jn_lines;    // Error already logged
    }
    char *output = gbuffer_cur_rd_pointer(gbuf);
    char *save = NULL;
    for(char *line = output? strtok_r(output, "\n", &save) : NULL; line; line = strtok_r(NULL, "\n", &save)) {
        json_t *jn_line = json_string(line);
        if(jn_line) {
            json_array_append_new(jn_lines, jn_line);
        }
    }
    gbuffer_decref(gbuf);
    return jn_lines;
}

/***************************************************************************
 *  What a route of a console keeps of the `__md_iev__` of the request
 *  that opened it: what routes its events back, the stack of the hops
 *  (`ievent_gate_stack`) and the `__msg_type__`, and nothing else. Every
 *  event of the console carries it, copied: up to 7.30.26 the whole
 *  `__md_iev__` was kept, and a request padded with 1 MB made each chunk
 *  of 1 KB a message of 1 MB. Return yours.
 ***************************************************************************/
PRIVATE json_t *console_route_md(json_t *kw)
{
    json_t *md_iev = json_object_get(kw, "__md_iev__");
    json_t *md_route = json_object();
    json_t *jn_stack = json_object_get(md_iev, "ievent_gate_stack");
    if(jn_stack) {
        json_object_set_new(md_route, "ievent_gate_stack", json_deep_copy(jn_stack));
    }
    json_t *jn_msg_type = json_object_get(md_iev, "__msg_type__");
    if(json_is_string(jn_msg_type)) {
        json_object_set(md_route, "__msg_type__", jn_msg_type);
    }
    return md_route;
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
        /*
         *  Re-open through the same channel (e.g. a browser refresh behind
         *  the controlcenter): the stored __md_iev__ belongs to the previous
         *  requester, refresh it so replies route to the new one.
         */
        json_t *jn_route = json_object_get(jn_routes, route_name);
        json_object_set_new(jn_route, "__md_iev__", console_route_md(kw));
        return -2;
    }

    /*
     *  add in local list
     */
    json_t *jn_route = json_pack("{s:s, s:s, s:o}",
        "route_service", route_service,
        "route_child", route_child,
        "__md_iev__", console_route_md(kw)
    );
    if(!jn_route) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
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
 *
 ***************************************************************************/
PRIVATE int add_console_in_input_gate(hgobj gobj, const char *name, hgobj src)
{
    /*  The console name is the user's: a KEY of "consoles", as the close
     *  removes it, not a segment of a path.  */
    json_t *consoles = gobj_kw_get_user_data( // save in input gate
        src,
        "consoles",
        json_object(), // owned
        KW_CREATE
    );
    if(!json_is_object(consoles) || json_object_set_new(consoles, name, json_true()) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "cannot save the console in the input gate",
            "console",      "%s", name,
            NULL
        );
        return -1;
    }

    return 0;
}

/***************************************************************************
 *  The gate of a route of a console: the link to the control center
 *  (C_IEVENT_CLI), the only route of agent22's consoles. NULL when gone.
 ***************************************************************************/
PRIVATE hgobj console_route_gate(hgobj gobj, json_t *jn_route)
{
    const char *route_service = kw_get_str(gobj, jn_route, "route_service", "", KW_REQUIRED);
    const char *route_child = kw_get_str(gobj, jn_route,  "route_child", "", KW_REQUIRED);
    hgobj gobj_route_service = gobj_find_service(route_service, TRUE);
    if(!gobj_route_service) {
        return NULL;    // Error already logged
    }
    hgobj gobj_input_gate = gobj_child_by_name(gobj_route_service, route_child);
    if(!gobj_input_gate) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "no route child found",
            "service",      "%s", route_service,
            "child",        "%s", route_child,
            NULL
        );
    }
    return gobj_input_gate;
}

/***************************************************************************
 *  The transport (C_TCP) under the gate of a route: the queue the flow
 *  control of a console looks at. NULL: no flow control for that route.
 ***************************************************************************/
PRIVATE hgobj console_route_transport(hgobj gate)
{
    hgobj transport = gobj_last_bottom_gobj(gate);
    if(!transport || !gobj_typeof_gclass(transport, C_TCP)) {
        return NULL;
    }
    return transport;
}

/***************************************************************************
 *  The routes over `transport` hold no pause now (it sent all it had, or
 *  dropped its connection and what it had with it), and their consoles
 *  read their terminal again.
 ***************************************************************************/
PRIVATE void console_release_pauses_of(hgobj gobj, hgobj transport)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *name; json_t *jn_console;
    json_object_foreach(priv->list_consoles, name, jn_console) {
        BOOL paused = FALSE;
        json_t *jn_routes = kw_get_dict(gobj, jn_console, "routes", 0, KW_REQUIRED);
        const char *route_name; json_t *jn_route;
        json_object_foreach(jn_routes, route_name, jn_route) {
            if(!json_object_get(jn_route, "pause")) {
                continue;
            }
            hgobj gate = console_route_gate(gobj, jn_route);
            if(!gate || console_route_transport(gate) == transport) {
                json_object_del(jn_route, "pause");
            } else {
                paused = TRUE;
            }
        }
        hgobj gobj_console = gobj_find_service(name, FALSE);
        if(gobj_console && gobj_typeof_gclass(gobj_console, C_PTY) &&
                gobj_read_bool_attr(gobj_console, "rx_paused") != paused) {
            gobj_write_bool_attr(gobj_console, "rx_paused", paused);
        }
    }
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
    json_t *jn_console = named_dict_take(priv->list_consoles, name);

    hgobj gobj_console = gobj_find_service(name, FALSE);

    if(!jn_console) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
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
                    "msgset",       "%s", MSGSET_INTERNAL,
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
    hgobj gobj_channel = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    json_t *consoles = gobj_kw_get_user_data(gobj_channel, "consoles", 0, 0);
    if(!consoles) {
        return 0;
    }

    /*
     *  Copy the key: `name` IS the key of `consoles`, and delete_console()
     *  deletes it from there (the route of this very channel) while it
     *  goes on reading it -- with two or more routes, freed memory. As the
     *  stop of the agent does it.
     */
    const char *name; json_t *jn_; void *n;
    json_object_foreach_safe(consoles, n, name, jn_) {
        char *name_ = gbmem_strdup(name);   // any length: a gobj name can exceed NAME_MAX - 1
        delete_console(gobj, name_);
        GBMEM_FREE(name_);
    }

    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    delete_consoles_on_disconnection(gobj, kw, src);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tty_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_console = named_dict(gobj, priv->list_consoles, gobj_name(src), TRUE);
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
                    "msgset",       "%s", MSGSET_INTERNAL,
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
PRIVATE int ac_tty_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_is_shutdowning()) {
        KW_DECREF(kw);
        return 0;
    }

    json_t *jn_console = named_dict_take(priv->list_consoles, gobj_name(src));
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
                    "msgset",       "%s", MSGSET_INTERNAL,
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
 *  The output of a console, with flow control, as the agent's: when the
 *  link has `console_pause_tx_queue` messages or more waiting in its
 *  transport, or `console_pause_tx_bytes` bytes or more, the console stops
 *  reading its terminal (C_PTY's rx_paused:
 *  its process waits) until the transport has sent them all (its
 *  EV_TX_READY), or dropped its connection (EV_DISCONNECTED). The link is
 *  the only route of agent22's consoles and the one that opened them, so it
 *  freezes nobody else: a console waits for it as long as the control
 *  center is away, with no deadline. And a hard bound whatever paused it,
 *  `console_max_tx_queue` (and `console_max_tx_bytes`): over it the
 *  console is closed, ONE WARNING
 *  (posted: this runs inside the publication of the PTY). Up to 7.30.26
 *  there was neither: a control center that did not read its link grew
 *  agent22 until it aborted.
 ***************************************************************************/
PRIVATE int ac_tty_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *name = gobj_name(src);
    json_t *jn_console = named_dict(gobj, priv->list_consoles, name, TRUE);
    if(!jn_console) {
        KW_DECREF(kw);  // Error already logged
        return 0;
    }

    json_int_t max_tx_queue = gobj_read_integer_attr(gobj, "console_max_tx_queue");
    json_int_t pause_tx_queue = gobj_read_integer_attr(gobj, "console_pause_tx_queue");
    json_int_t max_tx_bytes = gobj_read_integer_attr(gobj, "console_max_tx_bytes");
    json_int_t pause_tx_bytes = gobj_read_integer_attr(gobj, "console_pause_tx_bytes");
    BOOL close_it = FALSE;
    json_t *jn_routes = kw_get_dict(gobj, jn_console, "routes", 0, KW_REQUIRED);

    const char *route_name; json_t *jn_route;
    json_object_foreach(jn_routes, route_name, jn_route) {
        hgobj gobj_input_gate = console_route_gate(gobj, jn_route);
        if(!gobj_input_gate) {
            continue;   // Error already logged, or its service is gone
        }
        if((max_tx_queue > 0 || pause_tx_queue > 0 || max_tx_bytes > 0 || pause_tx_bytes > 0) &&
                gobj_has_bottom_attr(gobj_input_gate, "cur_tx_queue")) {
            json_int_t tx_queue = gobj_read_integer_attr(gobj_input_gate, "cur_tx_queue");
            json_int_t tx_bytes = gobj_has_bottom_attr(gobj_input_gate, "cur_tx_bytes")?
                gobj_read_integer_attr(gobj_input_gate, "cur_tx_bytes") : 0;
            BOOL to_pause = (pause_tx_queue > 0 && tx_queue >= pause_tx_queue) ||
                (pause_tx_bytes > 0 && tx_bytes >= pause_tx_bytes);
            BOOL over = (max_tx_queue > 0 && tx_queue >= max_tx_queue) ||
                (max_tx_bytes > 0 && tx_bytes >= max_tx_bytes);
            if(to_pause && !json_object_get(jn_route, "pause")) {
                hgobj transport = console_route_transport(gobj_input_gate);
                if(transport) {
                    char key[32];
                    snprintf(key, sizeof(key), "%p", transport);
                    if(!json_object_get(priv->console_transports, key)) {
                        gobj_subscribe_event(transport, EV_TX_READY, NULL, gobj);
                        gobj_subscribe_event(transport, EV_DISCONNECTED, NULL, gobj);
                        json_object_set_new(priv->console_transports, key, json_true());
                    }
                    json_object_set_new(jn_route, "pause", json_true());
                    if(!gobj_read_bool_attr(src, "rx_paused")) {
                        gobj_write_bool_attr(src, "rx_paused", TRUE);
                    }
                }
            }
            if(over) {
                if(!json_object_get(jn_console, "closing")) {
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PROTOCOL,
                        "msg",          "%s", "console output not taken by its session, console closed for it",
                        "console",      "%s", name,
                        "route",        "%s", route_name,
                        "opener",       "%s", "yes, the console is closed",
                        "tx_queue",     "%ld", (long)tx_queue,
                        "console_max_tx_queue", "%ld", (long)max_tx_queue,
                        "tx_bytes",     "%ld", (long)tx_bytes,
                        "console_max_tx_bytes", "%ld", (long)max_tx_bytes,
                        NULL
                    );
                    json_object_set_new(jn_console, "closing", json_true());
                    close_it = TRUE;
                }
                continue;
            }
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

    if(close_it) {
        gobj_post_event(gobj, EV_CLOSE_CONSOLE, json_pack("{s:s}", "name", name), gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  A transport agent22 waits on (ac_tty_data()) sent all it had, or
 *  dropped its connection (its queue is gone with it): the consoles it
 *  paused read again. While the link is down their output goes to it as
 *  it did before.
 ***************************************************************************/
PRIVATE int ac_console_tx_ready(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    console_release_pauses_of(gobj, src);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  A console over the hard bound (ac_tty_data()): closed, telling the link
 *  (the PTY's EV_TTY_CLOSE, ac_tty_close()). Posted, never done inside the
 *  publication of the PTY. Nothing to do when the console is gone already,
 *  or is another console of that name.
 ***************************************************************************/
PRIVATE int ac_close_console(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *name = kw_get_str(gobj, kw, "name", "", KW_REQUIRED);
    json_t *jn_console = named_dict(gobj, priv->list_consoles, name, FALSE);
    if(jn_console && json_object_get(jn_console, "closing")) {
        hgobj gobj_console = gobj_find_service(name, FALSE);
        if(gobj_console) {
            gobj_stop(gobj_console); // volatil, auto-destroy: its EV_TTY_CLOSE tells the link
        } else {
            char *name_ = gbmem_strdup(name);
            delete_console(gobj, name_);
            GBMEM_FREE(name_);
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_write_tty(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    const char *name= kw_get_str(gobj, kw, "name", 0, 0);
    const char *content64 = kw_get_str(gobj, kw, "content64", 0, 0);
    if(empty_string(content64)) {
        /*
         *  Malformed write-tty frame, the peer's doing: a WARNING, and drop
         *  THIS message, not the shared controlcenter_cli link, as the agent
         *  does (up to 7.30.26 an ERROR, and EV_DROP of the link).
         */
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "content64 required",
            "name",         "%s", name,
            NULL
        );
        KW_DECREF(kw);
        return -1;
    }

    hgobj gobj_console = gobj_find_service(name, FALSE);
    if(!gobj_console || !gobj_typeof_gclass(gobj_console, C_PTY)) {
        /*
         *  A write-tty for a console that isn't here, usually a key typed
         *  right after the console closed, or a service that is no console:
         *  as the agent does, a WARNING and EV_TTY_CLOSE to the requester,
         *  never EV_DROP of the shared controlcenter_cli link, which took
         *  down the whole control channel for one stray key (7.30.26).
         *  src is the gate this write-tty came through.
         */
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", gobj_console? "not a console" : "console not found",
            "name",         "%s", name,
            NULL
        );
        gobj_send_event(
            src,
            EV_TTY_CLOSE,
            msg_iev_build_response(gobj,
                0,  // result
                0,  // comment
                0,  // schema
                json_pack("{s:s}", "name", name?name:""),   // owned
                kw_incref(kw)  // owned, echoes __md_iev__ back
            ),
            gobj
        );
        KW_DECREF(kw);
        return gobj_console? -1 : 0;
    }

    gbuffer_t *gbuf = gbuffer_base64_to_binary_from_peer(gobj, content64, strlen(content64));
    if(!gbuf) {
        /*  Undecodable base64: a WARNING already logged (the sender's
         *  bytes); drop THIS message, not the shared controlcenter_cli link,
         *  as the agent does. */
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
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_CLOSE_CONSOLE);

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
        {EV_ON_OPEN,         ac_on_open,     0},
        {EV_ON_CLOSE,        ac_on_close,    0},
        {EV_TTY_DATA,        ac_tty_data,    0},
        {EV_TTY_OPEN,        ac_tty_open,    0},
        {EV_TTY_CLOSE,       ac_tty_close,   0},
        {EV_WRITE_TTY,       ac_write_tty,   0},
        {EV_TX_READY,        ac_console_tx_ready, 0},
        {EV_DISCONNECTED,    ac_console_tx_ready, 0},
        {EV_CLOSE_CONSOLE,   ac_close_console, 0},
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
        {EV_TX_READY,            0},
        {EV_DISCONNECTED,        0},
        {EV_CLOSE_CONSOLE,       0},
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
        attrs_table,
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
