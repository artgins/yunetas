/***********************************************************************
 *          C_CONTROLCENTER.C
 *          Controlcenter GClass.
 *
 *          Control Center of Yuneta Systems
 *
 *  The agents of the nodes dial out to it (`__input_side__`), the web
 *  consoles and `ycommand` connect to it (`__top_side__`), and it carries
 *  one to the other: `command-agent` / `stats-agent` forward a command to
 *  a node's agent and relay its answer, the PTY mirror and the pushed
 *  `EV_YUNO_STATS` travel back to the client that asked.
 *
 *  Its treedb, `treedb_controlcenter` (treedb_schema_controlcenter.c),
 *  keeps the SCENARIOS: a set of yunos on one or several nodes, how the
 *  messages flow between them, and the commands of each action (start,
 *  pause, resume, stop, report) -- a test to run and watch, or just a
 *  group of yunos to watch. A scenario is a document saved whole: its
 *  yunos have no identity outside it (their identity, node + id, lives in
 *  each agent).
 *      scenarios [scenario_id=]            list them, or one
 *      save-scenario scenario={} [revision=]  create or replace one (validated here)
 *      delete-scenario scenario_id=        delete one and its runs
 *      run-scenario scenario_id= action=   run the steps of an action, in order
 *      scenario-runs scenario_id=          the runs of one, newest first
 *  A RUN sends each step to the node's agent as `command-yuno`, carrying
 *  the user who asked (`__username__`, what the agent's audit records),
 *  one after the other: a step is sent only when the one before it
 *  answered, and a step that fails, does not answer in `run_step_timeout`,
 *  or whose agent disconnects meanwhile ends the run there. The agent
 *  runs it on the control center's session, so `write-scenarios` together
 *  with `run-scenarios` is as much as `command-agent`: grant them alike.
 *  Then the run is written in
 *  `scenario_runs` (linked to its scenario) and the requester answered
 *  with it -- the result and comment of every step, and for a `report`
 *  what each step answered (`data`). One run at a time.
 *
 *  A web client is told apart by its CONNECTION, not by the name of its
 *  channel in `__top_side__`: that name is taken by the next client once
 *  it closes. Each connection gets a number when it opens
 *  (`cc_connection`, in the channel's user data), stamped on what it sends
 *  to an agent; what comes back later -- an EV_YUNO_STATS still pushed by
 *  a watch that has not expired, the answer of a run -- reaches the channel
 *  only if it is still that connection.
 *
 *  Users are NOT kept here: who may use this yuno is its C_AUTHZ's store.
 *
 *          Copyright (c) 2020 Niyamaka.
 *          Copyright (c) 2025-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <grp.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <pwd.h>

#include "c_controlcenter.h"
#include "treedb_schema_controlcenter.c"

/***************************************************************************
 *              Constants
 ***************************************************************************/
/*  A run id is `<scenario id>.<ms>` and must fit in NAME_MAX.  */
#define SCENARIO_ID_MAX 200

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_logout_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_agents(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_command_agent(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_stats_agent(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_drop_agent(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_scenarios(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_save_scenario(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_delete_scenario(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_run_scenario(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_scenario_runs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *refuse_scenario_command(hgobj gobj, const char *permission, json_t *kw, hgobj src);
PRIVATE json_t *treedb_tranger(hgobj gobj);
PRIVATE const char *scenario_id_of(hgobj gobj, json_t *kw);
PRIVATE const char *username_of(hgobj gobj, json_t *kw, hgobj src);
PRIVATE BOOL is_scenario_action(const char *action);
PRIVATE int check_scenario(hgobj gobj, json_t *scenario, char *err, size_t errsz);
PRIVATE json_t *build_run_steps(hgobj gobj, json_t *scenario, const char *action, char *err, size_t errsz);
PRIVATE json_t *sort_runs_newest_first(json_t *runs);
PRIVATE BOOL requester_is_listening(hgobj gobj, hgobj requester, gobj_event_t event);
PRIVATE int run_send_step(hgobj gobj);
PRIVATE int run_step_answered(hgobj gobj, json_t *kw);
PRIVATE int run_step_timed_out(hgobj gobj);
PRIVATE int run_end(hgobj gobj, int result, const char *comment);
PRIVATE int run_agent_disconnected(hgobj gobj);
PRIVATE hgobj channel_of_side(hgobj side, hgobj g);
PRIVATE json_int_t connection_number(hgobj channel);
PRIVATE BOOL same_connection(hgobj gobj, json_t *kw, hgobj channel);

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
PRIVATE sdata_desc_t pm_list_agents[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_INTEGER,   "expand",        0,              0,          "Expand details"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_command_agent[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "agent_id",     0,              0,          "agent id (UUID or HOSTNAME)"),
SDATAPM (DTP_STRING,    "agent_service",0,              0,          "agent service"),
SDATAPM (DTP_STRING,    "cmd2agent",    0,              0,          "command to agent"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_stats_agent[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "agent_id",     0,              0,          "agent id (UUID or HOSTNAME)"),
SDATAPM (DTP_STRING,    "agent_service",0,              0,          "agent service"),
SDATAPM (DTP_STRING,    "stats2agent",  0,              0,          "stats to agent"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_drop_agent[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
    SDATAPM (DTP_STRING,    "agent_id",     0,              0,          "agent id (UUID or HOSTNAME)"),
    SDATA_END()
};
PRIVATE sdata_desc_t pm_write_tty[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "agent_id",     0,              0,          "agent id"),
SDATAPM (DTP_STRING,    "content64",    0,              0,          "Content64 data to write to tty"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_logout_user[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "username",     0,              0,          "Username"),
SDATAPM (DTP_BOOLEAN,   "disabled",     0,              0,          "Disable user"),
SDATA_END()
};
/*
 *  `scenario_id` and never `id`: through the agent's `command-yuno` the
 *  whole kw is the filter that picks the yuno, and there `id` IS the
 *  yuno's (`command-yuno id=1996 service=controlcenter command=scenarios`).
 */
PRIVATE sdata_desc_t pm_scenario[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "scenario_id",  0,              0,          "Scenario id"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_save_scenario[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_JSON,      "scenario",     0,              0,          "The scenario, whole: {id, description, group, node, agent_url, yunos, links, actions, view}"),
SDATAPM (DTP_INTEGER,   "revision",     0,              "0",        "The revision the scenario was read at (`__md_treedb__`g_rowid` in what `scenarios` answers): refused if it was saved since. 0: no check"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_run_scenario[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "scenario_id",  0,              0,          "Scenario id"),
SDATAPM (DTP_STRING,    "action",       0,              0,          "Action to run: start, pause, resume, stop or report"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};
PRIVATE const char *a_write_tty[] = {"EV_WRITE_TTY", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,       "Command's help"),
SDATACM2 (DTP_SCHEMA,   "authzs",           0,                  0,              pm_authzs,      cmd_authzs,     "Authorization's help"),
SDATACM (DTP_SCHEMA,    "logout-user",      0,                  pm_logout_user, cmd_logout_user,"Logout user"),
SDATACM (DTP_SCHEMA,    "list-agents",      0,                  pm_list_agents, cmd_list_agents, "List connected agents"),

SDATACM2 (DTP_SCHEMA,   "command-agent",    SDF_WILD_CMD,       0,              pm_command_agent,cmd_command_agent,"Command to agent (agent id = UUID or HOSTNAME)"),

SDATACM2 (DTP_SCHEMA,   "stats-agent",      SDF_WILD_CMD,       0,              pm_stats_agent, cmd_stats_agent, "Get statistics of agent"),

SDATACM2 (DTP_SCHEMA,   "drop-agent",       SDF_WILD_CMD,       0,              pm_drop_agent,cmd_drop_agent,"Drop connection to agent (agent id = UUID or HOSTNAME)"),

SDATACM2 (DTP_SCHEMA,   "write-tty",        0,                  a_write_tty,    pm_write_tty,   0,              "Write data to tty (internal use)"),

SDATACM2 (DTP_SCHEMA,   "scenarios",        0,                  0,              pm_scenario,    cmd_scenarios,  "List the scenarios, or one"),
SDATACM2 (DTP_SCHEMA,   "save-scenario",    0,                  0,              pm_save_scenario,cmd_save_scenario,"Create or replace a scenario"),
SDATACM2 (DTP_SCHEMA,   "delete-scenario",  0,                  0,              pm_scenario,    cmd_delete_scenario,"Delete a scenario and its runs"),
SDATACM2 (DTP_SCHEMA,   "run-scenario",     0,                  0,              pm_run_scenario,cmd_run_scenario,"Run the steps of an action of a scenario, in order"),
SDATACM2 (DTP_SCHEMA,   "scenario-runs",    0,                  0,              pm_scenario,    cmd_scenario_runs,"The runs of a scenario, newest first"),

SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description---------- */
SDATA (DTP_STRING,      "__username__",     SDF_RD,             "",         "Username 'yuneta', permission for all"),
SDATA (DTP_INTEGER,     "txMsgs",           SDF_RD|SDF_PSTATS,  0,          "Messages transmitted"),
SDATA (DTP_INTEGER,     "rxMsgs",           SDF_RD|SDF_RSTATS,  0,          "Messages received"),

SDATA (DTP_INTEGER,     "txMsgsec",         SDF_RD|SDF_RSTATS,  0,          "Messages by second"),
SDATA (DTP_INTEGER,     "rxMsgsec",         SDF_RD|SDF_RSTATS,  0,          "Messages by second"),
SDATA (DTP_INTEGER,     "maxtxMsgsec",      SDF_WR|SDF_RSTATS,  0,          "Max Tx Messages by second"),
SDATA (DTP_INTEGER,     "maxrxMsgsec",      SDF_WR|SDF_RSTATS,  0,          "Max Rx Messages by second"),

SDATA (DTP_INTEGER,     "run_step_timeout", SDF_WR|SDF_PERSIST, "30000",    "Milliseconds a step of a scenario run may take to be answered"),

SDATA (DTP_INTEGER,     "timeout",          SDF_RD,             "1000",     "Timeout"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
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
SDATAAUTHZ (DTP_SCHEMA, "list-agents",      0,      0,      0,      "Permission to list remote agents"),
SDATAAUTHZ (DTP_SCHEMA, "command-agent",    0,      0,      0,      "Permission to send command to remote agent"),
SDATAAUTHZ (DTP_SCHEMA, "drop-agent",       0,      0,      0,      "Permission to drop connection to remote agent"),
SDATAAUTHZ (DTP_SCHEMA, "logout-user",      0,      0,      0,      "Permission to logout users"),
SDATAAUTHZ (DTP_SCHEMA, "write-tty",        0,      0,      0,      "Internal use. Feed remote consola from local keyboard"),

SDATAAUTHZ (DTP_SCHEMA, "read-scenarios",   0,      0,      0,      "Permission to read the scenarios and their runs"),
SDATAAUTHZ (DTP_SCHEMA, "write-scenarios",  0,      0,      0,      "Permission to save and delete scenarios"),
SDATAAUTHZ (DTP_SCHEMA, "run-scenarios",    0,      0,      0,      "Permission to run the actions of a scenario"),

SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    int32_t timeout;

    hgobj gobj_top_side;
    hgobj gobj_input_side;

    hgobj gobj_treedbs;
    hgobj gobj_treedb_controlcenter;
    hgobj gobj_authz;

    uint64_t txMsgs;
    uint64_t rxMsgs;
    uint64_t txMsgsec;
    uint64_t rxMsgsec;

    uint64_t stats_dropped;         // EV_YUNO_STATS for a web client that is gone
    uint64_t t_stats_dropped_log;   // msectimer: next time they may be said

    hgobj run_timer;                // deadline of the step of the run in flight
    json_t *run;                    // the run in flight, or NULL (one at a time)
    json_t *run_kw_answer;          // the request of that run, to answer it
    hgobj run_agent_channel;        // channel (in __input_side__) of the step in flight

    json_int_t connections;         // number given to the last web client connection
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
     *      Check user yuneta
     *----------------------------------------*/
    gobj_write_str_attr(
        gobj,
        "__username__",
        gobj_read_str_attr(gobj_yuno(), "__username__")
    );

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    priv->run_timer = gobj_create_pure_child("run_timer", C_TIMER, 0, gobj);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,               gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout,             gobj_read_integer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(priv->run)
    KW_DECREF(priv->run_kw_answer)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-----------------------------*
     *      Get Authzs service
     *  Only to drop the sessions of a user (logout-user): the users
     *  are its business, nothing of them is kept here.
     *-----------------------------*/
    priv->gobj_authz =  gobj_find_service("authz", TRUE);

    gobj_start(priv->timer);
    gobj_start(priv->run_timer);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->timer);
    gobj_stop(priv->run_timer);
    return 0;
}

/***************************************************************************
 *      Framework Method play
 *  cccc rule:
 *  If service has mt_play then start only the service gobj.
 *      (Let mt_play be responsible to start their tree)
 *  If service has not mt_play then start the tree with gobj_start_tree().
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-------------------------------------------*
     *          Create Treedb System
     *-------------------------------------------*/
    char path[PATH_MAX];
    yuneta_realm_store_dir(
        path,
        sizeof(path),
        gobj_yuno_role(),
        gobj_yuno_realm_owner(),
        gobj_yuno_realm_id(),
        "",  // tenant
        "",  // gclass-treedb controls the directories
        TRUE
    );
    json_t *kw_treedbs = json_pack("{s:s, s:s, s:b, s:i, s:i, s:i}",
        "path", path,
        "filename_mask", "%Y",  // to management treedbs we don't need multifiles (per day)
        "master", 1,
        "xpermission", 02770,
        "rpermission", 0660,
        "exit_on_error", LOG_OPT_EXIT_ZERO
    );
    priv->gobj_treedbs = gobj_create_service(
        "treedbs",
        C_TREEDB,
        kw_treedbs,
        gobj
    );

    /*
     *  HACK pipe inheritance
     */
    gobj_set_bottom_gobj(gobj, priv->gobj_treedbs);

    /*
     *  Start treedbs
     */
    gobj_subscribe_event(priv->gobj_treedbs, 0, 0, gobj);
    gobj_start_tree(priv->gobj_treedbs);

    /*-------------------------------------------*
     *      Load schema
     *      Open treedb controlcenter service
     *-------------------------------------------*/
    helper_quote2doublequote(treedb_schema_controlcenter);
    json_t *jn_treedb_schema_controlcenter = legalstring2json(treedb_schema_controlcenter, TRUE);
    if(!jn_treedb_schema_controlcenter) {
        /*
         *  Exit if schema fails
         */
        exit(-1);
    }

    if(parse_schema(jn_treedb_schema_controlcenter)<0) {
        /*
         *  Exit if schema fails
         */
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "Parse schema fails",
            NULL
        );
        exit(-1);
    }


    const char *treedb_name = kw_get_str(gobj,
        jn_treedb_schema_controlcenter,
        "id",
        "treedb_controlcenter",
        KW_REQUIRED
    );

    json_t *kw_treedb = json_pack("{s:s, s:i, s:s, s:o, s:b}",
        "filename_mask", "%Y",
        "exit_on_error", 0,
        "treedb_name", treedb_name,
        "treedb_schema", jn_treedb_schema_controlcenter,
        "impose_c_schema", 1    // the binary imposes its schema, whatever the attribute says
    );
    json_t *jn_resp = gobj_command(priv->gobj_treedbs,
        "open-treedb",
        kw_treedb,
        gobj
    );
    int result = (int)kw_get_int(gobj, jn_resp, "result", -1, KW_REQUIRED);
    if(result < 0) {
        const char *comment = kw_get_str(gobj, jn_resp, "comment", "", KW_REQUIRED);
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", comment,
            NULL
        );
    }
    json_decref(jn_resp);

    /*
     *  Not subscribed: every write of this treedb is made by this gobj's
     *  own commands, which answer with what they wrote.
     */
    priv->gobj_treedb_controlcenter = gobj_find_service("treedb_controlcenter", TRUE);

    /*-------------------------*
     *      Start services
     *-------------------------*/
    priv->gobj_top_side = gobj_find_service("__top_side__", TRUE);
    gobj_subscribe_event(priv->gobj_top_side, 0, 0, gobj);
    gobj_start_tree(priv->gobj_top_side);

    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, 0, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*---------------------------------------*
     *  A run in flight ends here: written
     *  while the treedb is open, answered
     *  while its requester is connected.
     *---------------------------------------*/
    if(priv->run) {
        run_end(gobj, -1, "the control center was paused in the middle of the run");
    }

    /*---------------------------------------*
     *      Stop services
     *---------------------------------------*/
    if(priv->gobj_top_side) {
        if(gobj_is_playing(priv->gobj_top_side)) {
            gobj_pause(priv->gobj_top_side);
        }
        gobj_stop_tree(priv->gobj_top_side);
    }
    if(priv->gobj_input_side) {
        if(gobj_is_playing(priv->gobj_input_side)) {
            gobj_pause(priv->gobj_input_side);
        }
        gobj_stop_tree(priv->gobj_input_side);
    }

    /*---------------------------------------*
     *      Close treedb controlcenter
     *---------------------------------------*/
    json_decref(gobj_command(priv->gobj_treedbs,
        "close-treedb",
        json_pack("{s:s}",
            "treedb_name", "treedb_controlcenter"
        ),
        gobj
    ));
    priv->gobj_treedb_controlcenter = 0;

    /*-------------------------*
     *      Stop treedbs
     *-------------------------*/
    if(priv->gobj_treedbs) {
        gobj_unsubscribe_event(priv->gobj_treedbs, 0, 0, gobj);
        gobj_stop_tree(priv->gobj_treedbs);
        EXEC_AND_RESET(gobj_destroy, priv->gobj_treedbs)
    }

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
    KW_INCREF(kw)
    json_t *jn_resp = gobj_build_authzs_doc(gobj, cmd, kw);
    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_resp,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_logout_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *username = kw_get_str(gobj, kw, "username", "", 0);
    BOOL disabled = kw_get_bool(gobj, kw, "disabled", 0, KW_WILD_NUMBER);

    if(empty_string(username)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What realm owner?"),
            0,
            0,
            kw  // owned
        );
    }

    int result = gobj_send_event(
        priv->gobj_authz,
        EV_REJECT_USER,
        json_pack("{s:s, s:b}",
            "username", username,
            "disabled", disabled
        ),
        gobj
    );

    return msg_iev_build_response(
        gobj,
        0,
        result<0?
            json_sprintf("%s", gobj_log_last_message()):
            json_sprintf("%d sessions dropped", result),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_agents(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int expand = (int)kw_get_int(gobj, kw, "expand", 0, KW_WILD_NUMBER);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "list-agents";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  Job
     *----------------------------------------*/
    json_t *jn_data = json_array();

    json_t *jn_filter = json_pack("{s:s, s:s}",
        "__gclass_name__", C_IEVENT_SRV,
        "__state__", ST_SESSION
    );
    json_t *dl_children = gobj_match_children_tree(priv->gobj_input_side, jn_filter);

    int idx; json_t *jn_child;
    json_array_foreach(dl_children, idx, jn_child) {
        hgobj child = (hgobj)(size_t)json_integer_value(jn_child);
        json_t *jn_attrs = json_deep_copy(gobj_read_json_attr(child, "identity_card"));
        json_object_del(jn_attrs, "jwt");
        if(expand) {
            json_array_append_new(jn_data, jn_attrs);
        } else {
            json_array_append_new(jn_data,
                json_sprintf("UUID:%s (%s,%s),  HOSTNAME:'%s'",
                    kw_get_str(gobj, jn_attrs, "id", "", 0),
                    kw_get_str(gobj, jn_attrs, "yuno_role", "", 0),
                    kw_get_str(gobj, jn_attrs, "yuno_version", "", 0),
                    kw_get_str(gobj, jn_attrs, "__md_iev__`ievent_gate_stack`0`host", "", 0)
                )
            );
            json_decref(jn_attrs);
        }
    }
    gobj_free_iter(dl_children);

    return msg_iev_build_response(gobj,
        0,
        0,
        0,
        jn_data,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_command_agent(hgobj gobj, const char *cmd, json_t *kw_, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *kw = json_deep_copy(kw_);
    KW_DECREF(kw_);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "command-agent";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  Job
     *----------------------------------------*/
    const char *keys2delete[] = { // WARNING parameters of command-yuno command of agent
        "id",
        "command",
        "service",
        "realm_id",
        "yuno_role",
        "yuno_name",
        "yuno_release",
        "yuno_tag",
        "yuno_disabled",
        "yuno_running",
        "__relays__",   // only this control center says what it relays
        0
    };
    for(int i=0; keys2delete[i]!=0; i++) {
        json_object_del(kw, keys2delete[i]);
    }

    /*
     *  Only run_send_step() marks a request as a step of a run: its answer
     *  would be taken as the step's
     */
    json_t *jn_md_iev = kw_get_dict(gobj, kw, "__md_iev__", 0, 0);
    if(jn_md_iev) {
        json_object_del(jn_md_iev, "cc_run");
        json_object_del(jn_md_iev, "cc_step");
    }

    /*
     *  Tell the agent which of its pushed events this control center
     *  relays to the web client: an agent sends a watch-yuno-stats
     *  through a control center only if it says EV_YUNO_STATS here. A
     *  control center that does not know an event DROPS the agent's
     *  connection when it gets one, so an older one must never be sent
     *  it -- and an older one does not write this key.
     */
    json_object_set_new(kw, "__relays__", json_pack("[s]", EV_YUNO_STATS));

    /*
     *  Which connection of a web client asks: what the agent pushes back
     *  later reaches its channel only while it is still that connection.
     */
    hgobj client_channel = channel_of_side(priv->gobj_top_side, src);
    if(client_channel) {
        json_t *jn_client = json_array_get(
            kw_get_list(gobj, kw, "__md_iev__`ievent_gate_stack", 0, 0), 0
        );
        if(json_is_object(jn_client)) {
            json_object_set_new(jn_client, "cc_connection",
                json_integer(connection_number(client_channel)));
        }
    }

    const char *agent_id = kw_get_str(gobj, kw, "agent_id", "", 0);
    const char *cmd2agent = kw_get_str(gobj, kw, "cmd2agent", "", 0);
    const char *agent_service = kw_get_str(gobj, kw, "agent_service", "", 0);
    if(!empty_string(agent_service)) {
        json_object_set_new(kw, "service", json_string(agent_service));
    }

    if(empty_string(cmd2agent)) {
        return msg_iev_build_response(gobj,
            -1,
            json_string("What cmd2agent?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *jn_filter = json_pack("{s:s, s:s}",
        "__gclass_name__", C_IEVENT_SRV,
        "__state__", ST_SESSION
    );

    json_t *dl_children = gobj_match_children_tree(priv->gobj_input_side, jn_filter);

    int some = 0;
    int idx; json_t *jn_child;
    json_array_foreach(dl_children, idx, jn_child) {
        hgobj child = (hgobj)(size_t)json_integer_value(jn_child);
        json_t *jn_attrs = gobj_read_json_attr(child, "identity_card");
        if(!empty_string(agent_id)) {
            const char *id_ = kw_get_str(gobj, jn_attrs, "id", "", 0);
            const char *host_ = kw_get_str(gobj, jn_attrs, "__md_iev__`ievent_gate_stack`0`host", "", 0);
            if(strcmp(id_, agent_id)!=0 && strcmp(host_, agent_id)!=0) {
                continue;
            }
        }

        json_t *webix = gobj_command( // debe retornar siempre 0.
            child,
            cmd2agent,
            json_incref(kw),
            src
        );
        JSON_DECREF(webix);
        some++;
        break; // connect ONLY one
    }

    gobj_free_iter(dl_children);

    return msg_iev_build_response(gobj, // Asynchronous response too
        some?0:-1,
        json_sprintf("Command sent to %d nodes", some),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_stats_agent(hgobj gobj, const char *cmd, json_t *kw_, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *kw = json_deep_copy(kw_);
    KW_DECREF(kw_);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "command-agent";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  Job
     *----------------------------------------*/
    const char *keys2delete[] = { // WARNING parameters of command-yuno command of agent
        "id",
        "command",
        "service",
        "realm_id",
        "yuno_role",
        "yuno_name",
        "yuno_release",
        "yuno_tag",
        "yuno_disabled",
        "yuno_running",
        0
    };
    for(int i=0; keys2delete[i]!=0; i++) {
        json_object_del(kw, keys2delete[i]);
    }

    const char *agent_id = kw_get_str(gobj, kw, "agent_id", "", 0);
    const char *stats2agent = kw_get_str(gobj, kw, "stats2agent", "", 0);
    const char *agent_service = kw_get_str(gobj, kw, "agent_service", "", 0);
    if(!empty_string(agent_service)) {
        json_object_set_new(kw, "service", json_string(agent_service));
    }

    json_t *jn_filter = json_pack("{s:s, s:s}",
        "__gclass_name__", C_IEVENT_SRV,
        "__state__", ST_SESSION
    );
    json_t *dl_children = gobj_match_children_tree(priv->gobj_input_side, jn_filter);

    int some = 0;
    int idx; json_t *jn_child;
    json_array_foreach(dl_children, idx, jn_child) {
        hgobj child = (hgobj)(size_t)json_integer_value(jn_child);
        json_t *jn_attrs = gobj_read_json_attr(child, "identity_card");
        if(!empty_string(agent_id)) {
            const char *id_ = kw_get_str(gobj, jn_attrs, "id", "", 0);
            const char *host_ = kw_get_str(gobj, jn_attrs, "__md_iev__`ievent_gate_stack`0`host", "", 0);
            if(strcmp(id_, agent_id)!=0 && strcmp(host_, agent_id)!=0) {
                continue;
            }
        }

        json_t *webix = gobj_stats( // debe retornar siempre 0.
            child,
            stats2agent,
            json_incref(kw),
            src
        );
        JSON_DECREF(webix);
        some++;
        break; // connect ONLY one
    }

    gobj_free_iter(dl_children);

    return msg_iev_build_response(gobj, // Asynchronous response too
        some?0:-1,
        json_sprintf("Stats sent to %d nodes", some),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_drop_agent(hgobj gobj, const char *cmd, json_t *kw_, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *kw = json_deep_copy(kw_);
    KW_DECREF(kw_);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "drop-agent";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }


    const char *agent_id = kw_get_str(gobj, kw, "agent_id", "", 0);

    json_t *jn_filter = json_pack("{s:s, s:s}",
        "__gclass_name__", C_IEVENT_SRV,
        "__state__", ST_SESSION
    );
    json_t *dl_children = gobj_match_children_tree(priv->gobj_input_side, jn_filter);

    int some = 0;
    int idx; json_t *jn_child;
    json_array_foreach(dl_children, idx, jn_child) {
        hgobj child = (hgobj)(size_t)json_integer_value(jn_child);
        json_t *jn_attrs = gobj_read_json_attr(child, "identity_card");
        if(!empty_string(agent_id)) {
            const char *id_ = kw_get_str(gobj, jn_attrs, "id", "", 0);
            const char *host_ = kw_get_str(gobj, jn_attrs, "__md_iev__`ievent_gate_stack`0`host", "", 0);
            if(strcmp(id_, agent_id)!=0 && strcmp(host_, agent_id)!=0) {
                continue;
            }
        }

        gobj_send_event( // debe retornar siempre 0.
            child,
            EV_DROP,
            json_object(),
            src
        );
        some++;
    }

    gobj_free_iter(dl_children);

    return msg_iev_build_response(gobj, // Asynchronous response too
        some?0:-1,
        json_sprintf("Drop sent to %d nodes", some),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *  The scenarios, or the one named.
 ***************************************************************************/
PRIVATE json_t *cmd_scenarios(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *refused = refuse_scenario_command(gobj, "read-scenarios", kw, src);
    if(refused) {
        return refused;
    }

    const char *scenario_id = scenario_id_of(gobj, kw);
    json_t *jn_data;
    if(!empty_string(scenario_id)) {
        json_t *node = gobj_get_node(
            priv->gobj_treedb_controlcenter,
            "scenarios",
            json_pack("{s:s}", "id", scenario_id),
            json_pack("{s:b, s:b}", "hook_size", 1, "with_metadata", 1),
            gobj
        );
        if(!node) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("%s: scenario not found: '%s'", gobj_yuno_role_plus_name(), scenario_id),
                0,
                0,
                kw  // owned
            );
        }
        jn_data = json_array();
        json_array_append_new(jn_data, node);
    } else {
        jn_data = gobj_list_nodes(
            priv->gobj_treedb_controlcenter,
            "scenarios",
            0,
            json_pack("{s:b, s:b}", "hook_size", 1, "with_metadata", 1),
            gobj
        );
    }

    return msg_iev_build_response(
        gobj,
        0,
        0,
        tranger2_list_topic_desc_cols(treedb_tranger(gobj), "scenarios"),
        jn_data,
        kw  // owned
    );
}

/***************************************************************************
 *  Create a scenario, or replace it WHOLE: every column is written, so a
 *  field the new document leaves out is emptied, not kept. Who created it
 *  and when stays; who changed it and when is written each time.
 *
 *  `revision` is what makes two editors safe: the `g_rowid` of the
 *  scenario when it was read (every save moves it; `updated_at` counts
 *  seconds, too coarse to tell two saves apart). Given, a save over a
 *  scenario saved since -- or deleted since -- is refused and says by whom
 *  and when, instead of the last writer silently winning. 0 is no check:
 *  a new scenario, or an overwrite asked for on purpose.
 ***************************************************************************/
PRIVATE json_t *cmd_save_scenario(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *refused = refuse_scenario_command(gobj, "write-scenarios", kw, src);
    if(refused) {
        return refused;
    }

    /*
     *  A json from the command line arrives as a string
     */
    json_t *jn_parsed = 0;
    json_t *jn_scenario = kw_get_dict_value(gobj, kw, "scenario", 0, 0);
    if(json_is_string(jn_scenario)) {
        const char *s = json_string_value(jn_scenario);
        jn_parsed = anystring2json(s, strlen(s), FALSE);
        jn_scenario = jn_parsed;
    }

    char err[256];
    if(check_scenario(gobj, jn_scenario, err, sizeof(err)) < 0) {
        JSON_DECREF(jn_parsed)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: scenario not valid: %s", gobj_yuno_role_plus_name(), err),
            0,
            0,
            kw  // owned
        );
    }

    /*  Copied: a scenario parsed here is freed before the answers name it  */
    char scenario_id[SCENARIO_ID_MAX+1];
    snprintf(scenario_id, sizeof(scenario_id), "%s",
        json_string_value(json_object_get(jn_scenario, "id")));
    const char *username = username_of(gobj, kw, src);
    json_int_t now = (json_int_t)time_in_seconds();

    json_t *record = json_object();
    json_object_set_new(record, "id", json_string(scenario_id));
    const char *text_cols[] = {"description", "group", "node", "agent_url", 0};
    for(int i=0; text_cols[i]; i++) {
        json_t *v = json_object_get(jn_scenario, text_cols[i]);
        json_object_set_new(record, text_cols[i], json_string(json_is_string(v)? json_string_value(v) : ""));
    }
    const char *list_cols[] = {"yunos", "links", 0};
    for(int i=0; list_cols[i]; i++) {
        json_t *v = json_object_get(jn_scenario, list_cols[i]);
        json_object_set_new(record, list_cols[i], json_is_array(v)? json_deep_copy(v) : json_array());
    }
    const char *dict_cols[] = {"actions", "view", 0};
    for(int i=0; dict_cols[i]; i++) {
        json_t *v = json_object_get(jn_scenario, dict_cols[i]);
        json_object_set_new(record, dict_cols[i], json_is_object(v)? json_deep_copy(v) : json_object());
    }
    json_object_set_new(record, "updated_by", json_string(username));
    json_object_set_new(record, "updated_at", json_integer(now));

    json_t *existing = gobj_get_node(
        priv->gobj_treedb_controlcenter,
        "scenarios",
        json_pack("{s:s}", "id", scenario_id),
        json_pack("{s:b}", "with_metadata", 1),
        gobj
    );
    BOOL is_new = existing? FALSE : TRUE;

    json_int_t revision = kw_get_int(gobj, kw, "revision", 0, KW_WILD_NUMBER);
    json_int_t stored = existing?
        kw_get_int(gobj, existing, "__md_treedb__`g_rowid", 0, KW_REQUIRED) : 0;
    if(revision > 0 && revision != stored) {
        json_t *jn_comment = existing?
            json_sprintf("%s: scenario '%s' was saved by %s since you read it "
                "(revision %lld, yours %lld): read it again",
                gobj_yuno_role_plus_name(), scenario_id,
                kw_get_str(gobj, existing, "updated_by", "", 0),
                (long long)stored, (long long)revision) :
            json_sprintf("%s: scenario '%s' was deleted since you read it",
                gobj_yuno_role_plus_name(), scenario_id);
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "save-scenario refused: the scenario changed since it was read",
            "scenario_id",  "%s", scenario_id,
            "revision",     "%lld", (long long)revision,
            "stored",       "%lld", (long long)stored,
            "username",     "%s", username,
            NULL
        );
        JSON_DECREF(existing)
        JSON_DECREF(jn_parsed)
        JSON_DECREF(record)
        return msg_iev_build_response(gobj, -1, jn_comment, 0, 0, kw);
    }
    JSON_DECREF(existing)
    if(is_new) {
        json_object_set_new(record, "created_by", json_string(username));
        json_object_set_new(record, "created_at", json_integer(now));
    }
    JSON_DECREF(jn_parsed)

    json_t *node = gobj_update_node(
        priv->gobj_treedb_controlcenter,
        "scenarios",
        record, // owned
        json_pack("{s:b, s:b, s:b}", "create", 1, "hook_size", 1, "with_metadata", 1),
        gobj
    );
    if(!node) {
        return msg_iev_build_response(
            gobj,
            -1,
            // Error already logged
            json_sprintf("%s: cannot save the scenario '%s' (see the log)",
                gobj_yuno_role_plus_name(), scenario_id),
            0,
            0,
            kw  // owned
        );
    }

    json_t *jn_data = json_array();
    json_array_append_new(jn_data, node);
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: scenario %s: %s", gobj_yuno_role_plus_name(),
            is_new? "created" : "saved", scenario_id),
        tranger2_list_topic_desc_cols(treedb_tranger(gobj), "scenarios"),
        jn_data,
        kw  // owned
    );
}

/***************************************************************************
 *  Delete a scenario, and its runs with it: a run is the history of ONE
 *  scenario, and says nothing without it.
 ***************************************************************************/
PRIVATE json_t *cmd_delete_scenario(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *refused = refuse_scenario_command(gobj, "write-scenarios", kw, src);
    if(refused) {
        return refused;
    }

    const char *scenario_id = scenario_id_of(gobj, kw);
    if(empty_string(scenario_id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: what scenario? scenario_id=<id>", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    if(priv->run && strcmp(kw_get_str(gobj, priv->run, "scenario_id", "", 0), scenario_id)==0) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: scenario '%s' is running: %s", gobj_yuno_role_plus_name(),
                scenario_id, kw_get_str(gobj, priv->run, "id", "", 0)),
            0,
            0,
            kw  // owned
        );
    }

    json_t *node = gobj_get_node(
        priv->gobj_treedb_controlcenter,
        "scenarios",
        json_pack("{s:s}", "id", scenario_id),
        0,
        gobj
    );
    if(!node) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: scenario not found: '%s'", gobj_yuno_role_plus_name(), scenario_id),
            0,
            0,
            kw  // owned
        );
    }

    json_t *runs = gobj_node_children(
        priv->gobj_treedb_controlcenter,
        "scenarios",
        json_pack("{s:s}", "id", scenario_id),
        "runs",
        0,
        json_pack("{s:b}", "only_id", 1),
        gobj
    );
    int deleted_runs = 0;
    size_t idx; json_t *jn_run;
    json_array_foreach(runs, idx, jn_run) {
        const char *run_id = json_is_string(jn_run)?
            json_string_value(jn_run) : kw_get_str(gobj, jn_run, "id", "", 0);
        if(gobj_delete_node(
                priv->gobj_treedb_controlcenter,
                "scenario_runs",
                json_pack("{s:s}", "id", run_id),
                json_pack("{s:b}", "force", 1),
                gobj
            )==0) {
            deleted_runs++;
        }
        // else Error already logged
    }
    JSON_DECREF(runs)

    int ret = gobj_delete_node(
        priv->gobj_treedb_controlcenter,
        "scenarios",
        node, // owned
        json_pack("{s:b}", "force", 1),
        gobj
    );
    if(ret < 0) {
        return msg_iev_build_response(
            gobj,
            -1,
            // Error already logged
            json_sprintf("%s: cannot delete the scenario '%s' (see the log)",
                gobj_yuno_role_plus_name(), scenario_id),
            0,
            0,
            kw  // owned
        );
    }

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: scenario deleted: %s (and %d runs)",
            gobj_yuno_role_plus_name(), scenario_id, deleted_runs),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *  Run the steps of an action of a scenario, in order, each on its node's
 *  agent as `command-yuno`. Answered when the run is over (run_end()).
 ***************************************************************************/
PRIVATE json_t *cmd_run_scenario(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *refused = refuse_scenario_command(gobj, "run-scenarios", kw, src);
    if(refused) {
        return refused;
    }

    if(priv->run) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: a run is in flight, one at a time: %s",
                gobj_yuno_role_plus_name(), kw_get_str(gobj, priv->run, "id", "", 0)),
            0,
            0,
            kw  // owned
        );
    }

    const char *scenario_id = scenario_id_of(gobj, kw);
    const char *action = kw_get_str(gobj, kw, "action", "", 0);
    if(!is_scenario_action(action)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: what action? start, pause, resume, stop or report",
                gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    json_t *scenario = gobj_get_node(
        priv->gobj_treedb_controlcenter,
        "scenarios",
        json_pack("{s:s}", "id", scenario_id),
        0,
        gobj
    );
    if(!scenario) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: scenario not found: '%s'", gobj_yuno_role_plus_name(), scenario_id),
            0,
            0,
            kw  // owned
        );
    }

    char err[256];
    json_t *steps = build_run_steps(gobj, scenario, action, err, sizeof(err));
    JSON_DECREF(scenario)
    if(!steps) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot run '%s' of '%s': %s", gobj_yuno_role_plus_name(),
                action, scenario_id, err),
            0,
            0,
            kw  // owned
        );
    }

    const char *requester = kw_get_str(
        gobj, kw, "__md_iev__`ievent_gate_stack`0`input_channel", "", 0
    );
    hgobj client_channel = channel_of_side(priv->gobj_top_side, src);
    char run_id[NAME_MAX];
    int len = snprintf(run_id, sizeof(run_id), "%s.%llu",
        scenario_id, (unsigned long long)time_in_milliseconds());
    if(len < 0 || (size_t)len >= sizeof(run_id)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "scenario id too long for a run id",
            "scenario_id",  "%s", scenario_id,
            NULL
        );
        JSON_DECREF(steps)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: scenario id too long to run it: '%s'", gobj_yuno_role_plus_name(),
                scenario_id),
            0,
            0,
            kw  // owned
        );
    }

    priv->run = json_pack("{s:s, s:s, s:s, s:s, s:s, s:I, s:I, s:o, s:i}",
        "id", run_id,
        "scenario_id", scenario_id,
        "action", action,
        "username", username_of(gobj, kw, src),
        "requester", requester,
        "requester_connection", client_channel? connection_number(client_channel) : (json_int_t)0,
        "started_at", (json_int_t)time_in_seconds(),
        "steps", steps, // owned
        "idx", 0
    );
    priv->run_kw_answer = kw;   // owned: answered by run_end()

    run_send_step(gobj);
    return 0;   /* Asynchronous response */
}

/***************************************************************************
 *  The runs of a scenario, newest first.
 ***************************************************************************/
PRIVATE json_t *cmd_scenario_runs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *refused = refuse_scenario_command(gobj, "read-scenarios", kw, src);
    if(refused) {
        return refused;
    }

    const char *scenario_id = scenario_id_of(gobj, kw);
    json_t *scenario = gobj_get_node(
        priv->gobj_treedb_controlcenter,
        "scenarios",
        json_pack("{s:s}", "id", scenario_id),
        0,
        gobj
    );
    if(!scenario) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: scenario not found: '%s'", gobj_yuno_role_plus_name(), scenario_id),
            0,
            0,
            kw  // owned
        );
    }
    JSON_DECREF(scenario)

    json_t *runs = gobj_node_children(
        priv->gobj_treedb_controlcenter,
        "scenarios",
        json_pack("{s:s}", "id", scenario_id),
        "runs",
        0,
        json_pack("{s:b}", "fkey_only_id", 1),
        gobj
    );

    return msg_iev_build_response(
        gobj,
        0,
        0,
        tranger2_list_topic_desc_cols(treedb_tranger(gobj), "scenario_runs"),
        sort_runs_newest_first(runs), // owned
        kw  // owned
    );
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  Is the requester still there to take its answer? A client that left
 *  before it (a ycommand that timed out, a closed tab) leaves its channel
 *  of __top_side__ CLOSED, and a link that dropped leaves its C_IEVENT_CLI
 *  out of session: neither takes EV_SEND_IEV. The answer has nowhere to go,
 *  and that is the client's leaving, not an error here.
 ***************************************************************************/
PRIVATE BOOL requester_is_listening(hgobj gobj, hgobj requester, gobj_event_t event)
{
    BOOL listening;
    if(gobj_has_attr(requester, "opened")) {
        listening = gobj_read_bool_attr(requester, "opened");
    } else {
        listening = gobj_in_this_state(requester, ST_SESSION);
    }
    if(!listening) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "requester gone before its answer, answer dropped",
            "requester",    "%s", gobj_short_name(requester),
            "event",        "%s", event,
            NULL
        );
    }
    return listening;
}

/***************************************************************************
 *  The answer of a scenario command that cannot run: the user has not
 *  the permission, or the treedb is not open. NULL when it can (kw
 *  untouched).
 ***************************************************************************/
PRIVATE json_t *refuse_scenario_command(hgobj gobj, const char *permission, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: no permission to '%s'", gobj_yuno_role_plus_name(), permission),
            0,
            0,
            kw  // owned
        );
    }
    if(!priv->gobj_treedb_controlcenter) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SERVICE,
            "msg",          "%s", "Treedb Controlcenter not open",
            "permission",   "%s", permission,
            NULL
        );
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: the scenarios are not open yet", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    return NULL;
}

PRIVATE json_t *treedb_tranger(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    return gobj_read_pointer_attr(priv->gobj_treedb_controlcenter, "tranger");
}

/***************************************************************************
 *  The scenario a command names (`scenario_id`, see pm_scenario).
 ***************************************************************************/
PRIVATE const char *scenario_id_of(hgobj gobj, json_t *kw)
{
    return kw_get_str(gobj, kw, "scenario_id", "", 0);
}

/***************************************************************************
 *  Who asks: the user C_IEVENT_SRV put in the kw, or the channel's.
 ***************************************************************************/
PRIVATE const char *username_of(hgobj gobj, json_t *kw, hgobj src)
{
    const char *username = kw_get_str(gobj, kw, "__username__", "", 0);
    if(empty_string(username) && src && gobj_has_attr(src, "__username__")) {
        username = gobj_read_str_attr(src, "__username__");
    }
    return username? username : "";
}

PRIVATE const char *scenario_actions[] = {"start", "pause", "resume", "stop", "report", 0};

PRIVATE BOOL is_scenario_action(const char *action)
{
    if(empty_string(action)) {
        return FALSE;
    }
    for(int i=0; scenario_actions[i]; i++) {
        if(strcmp(scenario_actions[i], action)==0) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  A name that travels in a command line: [A-Za-z0-9_.^-]+, the console's
 *  NAME_RE (monitor_helpers.js).
 ***************************************************************************/
PRIVATE BOOL is_plain_name(const char *s)
{
    if(empty_string(s) || strlen(s) >= NAME_MAX) {
        return FALSE;
    }
    for(const char *p = s; *p; p++) {
        if(!isalnum((unsigned char)*p) && !strchr("_.^-", *p)) {
            return FALSE;
        }
    }
    return TRUE;
}

/***************************************************************************
 *  A scenario id is a key of the treedb and the start of a run id:
 *  [A-Za-z0-9_.@-]+, not starting with a dot (timeranger2 refuses it as a
 *  key), SCENARIO_ID_MAX at most. The console's SCENARIO_ID_RE.
 ***************************************************************************/
PRIVATE BOOL is_scenario_id(const char *s)
{
    if(empty_string(s) || *s == '.' || strlen(s) > SCENARIO_ID_MAX) {
        return FALSE;
    }
    for(const char *p = s; *p; p++) {
        if(!isalnum((unsigned char)*p) && !strchr("_.@-", *p)) {
            return FALSE;
        }
    }
    return TRUE;
}

/***************************************************************************
 *  The parameters a step cannot carry: it travels as
 *  `command-yuno id= service= command=<the step>`, and the agent takes the
 *  whole kw of command-yuno as the filter that selects the yuno -- so a
 *  parameter named like a column of its `yunos` topic, or like one of
 *  command-yuno's own, would pick another yuno or none ("Yuno not found").
 *  The console refuses the same list (monitor_helpers.js).
 *  Nor a framework key (`__md_iev__`, `__username__`, ...: any name that
 *  starts with `__`): the agent's command parser sets the parameters of the
 *  line over the kw, so it would replace the routing of the answer, or the
 *  user the control center stamps.
 ***************************************************************************/
PRIVATE const char *reserved_step_params[] = {
    "id", "service", "command",
    "realm_id", "yuno_role", "yuno_name", "yuno_release", "yuno_tag",
    "yuno_running", "yuno_playing", "yuno_pid", "watcher_pid", "yuno_disabled",
    "must_play", "start_priority", "sched_priority", "cpu_core",
    "role_version", "name_version", "traced", "yuno_multiple", "global",
    "date", "yuno_startdate", "_channel_gobj", "_requester",
    "_requester_md_iev", "launch_id", "configurations", "binary", "_geometry",
    0
};

/***************************************************************************
 *  A step's command: `name key=value ...` on one line, values without
 *  blanks, no reserved parameter. The console's TEST_COMMAND_RE.
 *  -1 with the reason in `bad`.
 ***************************************************************************/
PRIVATE int check_step_command(const char *command, char *bad, size_t badsz)
{
    int n = 0;
    const char **words = split2(command, " \t", &n);
    if(!words || n == 0) {
        split_free2(words);
        snprintf(bad, badsz, "a command name and its key=value parameters");
        return -1;
    }
    int ret = 0;
    const char *name = words[0];
    if(!islower((unsigned char)name[0])) {
        snprintf(bad, badsz, "a command name first: '%.40s'", name);
        ret = -1;
    }
    for(const char *p = name; ret == 0 && *p; p++) {
        if(!isalnum((unsigned char)*p) && *p != '_' && *p != '-') {
            snprintf(bad, badsz, "a command name first: '%.40s'", name);
            ret = -1;
        }
    }
    for(int i=1; ret == 0 && i<n; i++) {
        const char *eq = strchr(words[i], '=');
        size_t klen = eq? (size_t)(eq - words[i]) : 0;
        if(!eq || klen == 0 || klen >= NAME_MAX) {
            snprintf(bad, badsz, "key=value parameters, without blanks: '%.40s'", words[i]);
            ret = -1;
            break;
        }
        char key[NAME_MAX];
        snprintf(key, sizeof(key), "%.*s", (int)klen, words[i]);
        if(!is_plain_name(key)) {
            snprintf(bad, badsz, "a parameter name of letters, digits and _ . ^ -: '%.40s'", key);
            ret = -1;
            break;
        }
        if(strncmp(key, "__", 2)==0) {
            snprintf(bad, badsz,
                "the parameter '%.40s' is a framework key: it would replace what the control center sets",
                key);
            ret = -1;
            break;
        }
        for(int j=0; reserved_step_params[j]; j++) {
            if(strcmp(key, reserved_step_params[j])==0) {
                snprintf(bad, badsz,
                    "the parameter '%.40s' is command-yuno's or a yuno's field: it would select the yuno",
                    key);
                ret = -1;
                break;
            }
        }
    }
    if(ret == 0 && (strchr(command, '\r') || strchr(command, '\n'))) {
        snprintf(bad, badsz, "one command line");
        ret = -1;
    }
    split_free2(words);
    return ret;
}

/***************************************************************************
 *  A string member, "" when absent; NULL when present and not a string.
 ***************************************************************************/
PRIVATE const char *str_member(json_t *jn, const char *key)
{
    json_t *v = json_object_get(jn, key);
    if(!v) {
        return "";
    }
    return json_is_string(v)? json_string_value(v) : NULL;
}

/***************************************************************************
 *  The key of a yuno of a scenario: its `key`, or its `id`.
 ***************************************************************************/
PRIVATE const char *yuno_key(json_t *yuno)
{
    const char *key = str_member(yuno, "key");
    if(empty_string(key)) {
        key = str_member(yuno, "id");
    }
    return key? key : "";
}

PRIVATE json_t *find_scenario_yuno(json_t *scenario, const char *key)
{
    size_t idx; json_t *yuno;
    json_array_foreach(json_object_get(scenario, "yunos"), idx, yuno) {
        if(strcmp(yuno_key(yuno), key)==0) {
            return yuno;
        }
    }
    return NULL;
}

/***************************************************************************
 *  Is this a scenario? The same rules the console's editor applies
 *  (monitor_helpers.js), checked here because any client can save one.
 *  -1 with the reason in `err`.
 ***************************************************************************/
PRIVATE int scenario_error(char *err, size_t errsz, const char *fmt, ...) JANSSON_ATTRS((format(printf, 3, 4)));
PRIVATE int scenario_error(char *err, size_t errsz, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, errsz, fmt, ap);
    va_end(ap);
    return -1;
}

PRIVATE int check_scenario(hgobj gobj, json_t *scenario, char *err, size_t errsz)
{
    if(!json_is_object(scenario)) {
        return scenario_error(err, errsz, "it must be a dict");
    }
    const char *scenario_id = str_member(scenario, "id");
    if(!scenario_id || !is_scenario_id(scenario_id)) {
        return scenario_error(err, errsz,
            "id: letters, digits and _ . @ -, not starting with a dot, %d at most", SCENARIO_ID_MAX);
    }
    const char *text_cols[] = {"description", "group", "node", "agent_url", 0};
    for(int i=0; text_cols[i]; i++) {
        if(!str_member(scenario, text_cols[i])) {
            return scenario_error(err, errsz, "%s: must be a string", text_cols[i]);
        }
    }

    json_t *yunos = json_object_get(scenario, "yunos");
    if(!json_is_array(yunos) || json_array_size(yunos)==0) {
        return scenario_error(err, errsz, "yunos: a list of one yuno at least");
    }
    BOOL every_yuno_placed = TRUE;
    json_t *keys = json_object();
    size_t idx; json_t *yuno;
    json_array_foreach(yunos, idx, yuno) {
        if(!json_is_object(yuno)) {
            JSON_DECREF(keys)
            return scenario_error(err, errsz, "yunos[%d]: must be a dict", (int)idx);
        }
        const char *yuno_id = str_member(yuno, "id");
        if(!yuno_id || !is_plain_name(yuno_id)) {
            JSON_DECREF(keys)
            return scenario_error(err, errsz, "yunos[%d]: id: the yuno id of its agent", (int)idx);
        }
        const char *key = str_member(yuno, "key");
        if(!key || (!empty_string(key) && !is_plain_name(key))) {
            JSON_DECREF(keys)
            return scenario_error(err, errsz, "yunos[%d]: key: letters, digits and _ . ^ -", (int)idx);
        }
        key = yuno_key(yuno);
        if(json_object_get(keys, key)) {
            JSON_DECREF(keys)
            return scenario_error(err, errsz, "yunos[%d]: key '%s' repeated: give each a `key`",
                (int)idx, key);
        }
        json_object_set_new(keys, key, json_true());
        const char *yuno_cols[] = {"node", "service", "label", "rate", 0};
        for(int i=0; yuno_cols[i]; i++) {
            if(!str_member(yuno, yuno_cols[i])) {
                JSON_DECREF(keys)
                return scenario_error(err, errsz, "yunos[%d]: %s: must be a string",
                    (int)idx, yuno_cols[i]);
            }
        }
        const char *yuno_service = str_member(yuno, "service");
        if(!empty_string(yuno_service) && !is_plain_name(yuno_service)) {
            JSON_DECREF(keys)
            return scenario_error(err, errsz, "yunos[%d]: service: letters, digits and _ . ^ -",
                (int)idx);
        }
        const char *rate = str_member(yuno, "rate");
        if(!empty_string(rate) && strcmp(rate, "rx")!=0 && strcmp(rate, "tx")!=0) {
            JSON_DECREF(keys)
            return scenario_error(err, errsz, "yunos[%d]: rate: rx or tx", (int)idx);
        }
        if(empty_string(str_member(yuno, "node"))) {
            every_yuno_placed = FALSE;
        }
    }
    if(empty_string(str_member(scenario, "node")) &&
            empty_string(str_member(scenario, "agent_url")) &&
            !every_yuno_placed) {
        JSON_DECREF(keys)
        return scenario_error(err, errsz,
            "say where the yunos are: a node (through the control center) or an agent_url (direct)");
    }

    json_t *links = json_object_get(scenario, "links");
    if(links && !json_is_array(links)) {
        JSON_DECREF(keys)
        return scenario_error(err, errsz, "links: a list of [from, to]");
    }
    json_t *link;
    json_array_foreach(links, idx, link) {
        if(!json_is_array(link) || json_array_size(link)!=2 ||
                !json_is_string(json_array_get(link, 0)) ||
                !json_is_string(json_array_get(link, 1)) ||
                !json_object_get(keys, json_string_value(json_array_get(link, 0))) ||
                !json_object_get(keys, json_string_value(json_array_get(link, 1)))) {
            JSON_DECREF(keys)
            return scenario_error(err, errsz, "links[%d]: [from, to], two keys of its yunos", (int)idx);
        }
    }

    json_t *actions = json_object_get(scenario, "actions");
    if(actions && !json_is_object(actions)) {
        JSON_DECREF(keys)
        return scenario_error(err, errsz, "actions: a dict of start, pause, resume, stop, report");
    }
    const char *action; json_t *steps;
    json_object_foreach(actions, action, steps) {
        if(!is_scenario_action(action)) {
            JSON_DECREF(keys)
            return scenario_error(err, errsz,
                "actions: '%s' is not an action (start, pause, resume, stop, report)", action);
        }
        if(!json_is_array(steps)) {
            JSON_DECREF(keys)
            return scenario_error(err, errsz, "actions.%s: a list of steps", action);
        }
        json_t *step;
        json_array_foreach(steps, idx, step) {
            const char *step_yuno = str_member(step, "yuno");
            const char *command = str_member(step, "command");
            if(!json_is_object(step) || !step_yuno || !json_object_get(keys, step_yuno)) {
                JSON_DECREF(keys)
                return scenario_error(err, errsz, "actions.%s[%d]: yuno: a key of its yunos",
                    action, (int)idx);
            }
            char bad[NAME_MAX];
            if(!command || check_step_command(command, bad, sizeof(bad)) < 0) {
                JSON_DECREF(keys)
                return scenario_error(err, errsz, "actions.%s[%d]: command: %s",
                    action, (int)idx, command? bad : "must be a string");
            }
            const char *step_service = str_member(step, "service");
            if(!step_service || (!empty_string(step_service) && !is_plain_name(step_service))) {
                JSON_DECREF(keys)
                return scenario_error(err, errsz, "actions.%s[%d]: service: letters, digits and _ . ^ -",
                    action, (int)idx);
            }
        }
    }
    JSON_DECREF(keys)

    json_t *view = json_object_get(scenario, "view");
    if(view && !json_is_object(view)) {
        return scenario_error(err, errsz, "view: a dict");
    }
    return 0;
}

/***************************************************************************
 *  The steps of an action, resolved: the node and the command line each
 *  one goes to. NULL with the reason in `err`: a step whose yuno has no
 *  node cannot be run from here (a direct scenario runs from the console).
 *  Each step is checked again as save-scenario checks it: a scenario saved
 *  by an older control center (or written into the treedb by other means)
 *  never went through those checks.
 ***************************************************************************/
PRIVATE json_t *build_run_steps(hgobj gobj, json_t *scenario, const char *action, char *err, size_t errsz)
{
    json_t *actions = json_object_get(scenario, "actions");
    json_t *defs = json_object_get(actions, action);
    if(!json_is_array(defs) || json_array_size(defs)==0) {
        scenario_error(err, errsz, "the scenario declares no step for '%s'", action);
        return NULL;
    }
    const char *default_node = str_member(scenario, "node");

    json_t *steps = json_array();
    size_t idx; json_t *def;
    json_array_foreach(defs, idx, def) {
        const char *key = str_member(def, "yuno");
        json_t *yuno = find_scenario_yuno(scenario, key? key : "");
        if(!yuno) {
            JSON_DECREF(steps)
            scenario_error(err, errsz, "step %d: no yuno '%s' in the scenario", (int)idx, key? key : "");
            return NULL;
        }
        const char *node = str_member(yuno, "node");
        if(empty_string(node)) {
            node = default_node;
        }
        if(empty_string(node)) {
            JSON_DECREF(steps)
            scenario_error(err, errsz, "step %d: yuno '%s' has no node: a direct scenario runs from the console",
                (int)idx, key);
            return NULL;
        }
        const char *service = str_member(def, "service");
        if(empty_string(service)) {
            service = str_member(yuno, "service");
        }
        const char *command = str_member(def, "command");
        const char *yuno_id = str_member(yuno, "id");

        char bad[NAME_MAX];
        if(!yuno_id || !is_plain_name(yuno_id)) {
            JSON_DECREF(steps)
            scenario_error(err, errsz, "step %d: yuno '%s': id: the yuno id of its agent; save the scenario again",
                (int)idx, key);
            return NULL;
        }
        if(!service || (!empty_string(service) && !is_plain_name(service))) {
            JSON_DECREF(steps)
            scenario_error(err, errsz, "step %d: service: letters, digits and _ . ^ -; save the scenario again",
                (int)idx);
            return NULL;
        }
        if(!command || check_step_command(command, bad, sizeof(bad)) < 0) {
            JSON_DECREF(steps)
            scenario_error(err, errsz, "step %d: command: %s; save the scenario again",
                (int)idx, command? bad : "must be a string");
            return NULL;
        }

        json_t *line = empty_string(service)?
            json_sprintf("command-yuno id=%s command=%s", yuno_id, command) :
            json_sprintf("command-yuno id=%s service=%s command=%s", yuno_id, service, command);
        json_array_append_new(steps, json_pack("{s:s, s:s, s:s, s:s, s:s, s:o}",
            "yuno", key,
            "node", node,
            "yuno_id", yuno_id,
            "service", service? service : "",
            "command", command,
            "line", line
        ));
    }
    return steps;
}

/***************************************************************************
 *  The runs, newest first (by `started_at`, then by id).
 ***************************************************************************/
PRIVATE int cmp_runs_newest_first(const void *a, const void *b)
{
    json_t *ra = *(json_t * const *)a;
    json_t *rb = *(json_t * const *)b;
    json_int_t ta = json_integer_value(json_object_get(ra, "started_at"));
    json_int_t tb = json_integer_value(json_object_get(rb, "started_at"));
    if(ta != tb) {
        return (ta < tb)? 1 : -1;
    }
    const char *ia = json_string_value(json_object_get(ra, "id"));
    const char *ib = json_string_value(json_object_get(rb, "id"));
    return -strcmp(ia? ia : "", ib? ib : "");
}

PRIVATE json_t *sort_runs_newest_first(json_t *runs) // owned, returned
{
    size_t n = json_array_size(runs);
    if(n < 2) {
        return runs? runs : json_array();
    }
    json_t **items = gbmem_malloc(n * sizeof(json_t *));
    if(!items) {
        // Error already logged
        return runs;
    }
    for(size_t i=0; i<n; i++) {
        items[i] = json_incref(json_array_get(runs, i));
    }
    qsort(items, n, sizeof(json_t *), cmp_runs_newest_first);
    json_t *sorted = json_array();
    for(size_t i=0; i<n; i++) {
        json_array_append_new(sorted, items[i]);
    }
    gbmem_free(items);
    JSON_DECREF(runs)
    return sorted;
}

/***************************************************************************
 *  The channel of a connected agent, by its UUID or its hostname -- the
 *  same match command-agent makes.
 ***************************************************************************/
PRIVATE hgobj find_agent_channel(hgobj gobj, const char *agent_id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_filter = json_pack("{s:s, s:s}",
        "__gclass_name__", C_IEVENT_SRV,
        "__state__", ST_SESSION
    );
    json_t *dl_children = gobj_match_children_tree(priv->gobj_input_side, jn_filter);

    hgobj found = 0;
    int idx; json_t *jn_child;
    json_array_foreach(dl_children, idx, jn_child) {
        hgobj child = (hgobj)(size_t)json_integer_value(jn_child);
        json_t *jn_attrs = gobj_read_json_attr(child, "identity_card");
        const char *id_ = kw_get_str(gobj, jn_attrs, "id", "", 0);
        const char *host_ = kw_get_str(gobj, jn_attrs, "__md_iev__`ievent_gate_stack`0`host", "", 0);
        if(strcmp(id_, agent_id)==0 || strcmp(host_, agent_id)==0) {
            found = child;
            break;
        }
    }
    gobj_free_iter(dl_children);
    return found;
}

/***************************************************************************
 *  Send the step the run is at to its node's agent, marked in __md_iev__
 *  with the run and the step: its answer comes back to this gobj
 *  (ac_command_yuno_answer -> run_step_answered), carrying the user who
 *  asked. If the agent's channel closes first, ac_on_close ends the run.
 ***************************************************************************/
PRIVATE int run_send_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int idx = (int)kw_get_int(gobj, priv->run, "idx", 0, KW_REQUIRED);
    json_t *step = json_array_get(kw_get_list(gobj, priv->run, "steps", 0, KW_REQUIRED), (size_t)idx);
    const char *node = kw_get_str(gobj, step, "node", "", KW_REQUIRED);
    const char *line = kw_get_str(gobj, step, "line", "", KW_REQUIRED);

    hgobj channel = find_agent_channel(gobj, node);
    if(!channel) {
        json_object_set_new(step, "result", json_integer(-1));
        json_object_set_new(step, "comment", json_sprintf("node '%s' is not connected", node));
        return run_end(gobj, -1, "a node of the run is not connected");
    }
    priv->run_agent_channel = channel_of_side(priv->gobj_input_side, channel);

    json_t *kw_step = json_pack("{s:s}",
        "__username__", kw_get_str(gobj, priv->run, "username", "", 0)
    );
    kw_set_subdict_value(gobj, kw_step, "__md_iev__", "cc_run",
        json_string(kw_get_str(gobj, priv->run, "id", "", 0)));
    kw_set_subdict_value(gobj, kw_step, "__md_iev__", "cc_step", json_integer(idx));

    json_t *webix = gobj_command(channel, line, kw_step, gobj);
    JSON_DECREF(webix)

    set_timeout(priv->run_timer, gobj_read_integer_attr(gobj, "run_step_timeout"));
    return 0;
}

/***************************************************************************
 *  The answer of a step (kw owned): the next one, or the end.
 ***************************************************************************/
PRIVATE int run_step_answered(hgobj gobj, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *run_id = kw_get_str(gobj, kw, "__md_iev__`cc_run", "", 0);
    int step_idx = (int)kw_get_int(gobj, kw, "__md_iev__`cc_step", -1, KW_WILD_NUMBER);
    int result = (int)kw_get_int(gobj, kw, "result", -1, KW_WILD_NUMBER);

    if(!priv->run ||
            strcmp(run_id, kw_get_str(gobj, priv->run, "id", "", 0))!=0 ||
            step_idx != (int)kw_get_int(gobj, priv->run, "idx", 0, 0)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "answer of a scenario run step that is over, dropped",
            "run",          "%s", run_id,
            "step",         "%d", step_idx,
            "result",       "%d", result,
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }
    clear_timeout(priv->run_timer);

    json_t *steps = kw_get_list(gobj, priv->run, "steps", 0, KW_REQUIRED);
    json_t *step = json_array_get(steps, (size_t)step_idx);
    json_object_set_new(step, "result", json_integer(result));
    json_object_set_new(step, "comment", json_string(kw_get_str(gobj, kw, "comment", "", 0)));
    /*
     *  What a report asks for is what the steps answer: kept with them
     */
    if(strcmp(kw_get_str(gobj, priv->run, "action", "", 0), "report")==0) {
        json_t *jn_data = kw_get_dict_value(gobj, kw, "data", 0, 0);
        if(jn_data) {
            json_object_set(step, "data", jn_data);
        }
    }
    KW_DECREF(kw)

    if(result < 0) {
        return run_end(gobj, -1, "a step failed");
    }
    step_idx++;
    json_object_set_new(priv->run, "idx", json_integer(step_idx));
    if((size_t)step_idx >= json_array_size(steps)) {
        return run_end(gobj, 0, "");
    }
    return run_send_step(gobj);
}

/***************************************************************************
 *  The step in flight did not answer in time: the run ends there.
 ***************************************************************************/
PRIVATE int run_step_timed_out(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->run) {
        return 0;
    }
    int idx = (int)kw_get_int(gobj, priv->run, "idx", 0, KW_REQUIRED);
    json_t *step = json_array_get(kw_get_list(gobj, priv->run, "steps", 0, KW_REQUIRED), (size_t)idx);
    json_object_set_new(step, "result", json_integer(-1));
    json_object_set_new(step, "comment", json_sprintf("not answered in %d ms",
        (int)gobj_read_integer_attr(gobj, "run_step_timeout")));
    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_APP,
        "msg",          "%s", "a step of a scenario run was not answered",
        "run",          "%s", kw_get_str(gobj, priv->run, "id", "", 0),
        "step",         "%d", idx,
        "line",         "%s", kw_get_str(gobj, step, "line", "", 0),
        NULL
    );
    return run_end(gobj, -1, "a step was not answered");
}

/***************************************************************************
 *  The agent of the step in flight disconnected: its answer will not come,
 *  the run ends now instead of at the step's deadline.
 ***************************************************************************/
PRIVATE int run_agent_disconnected(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->run) {
        return 0;
    }
    int idx = (int)kw_get_int(gobj, priv->run, "idx", 0, KW_REQUIRED);
    json_t *step = json_array_get(kw_get_list(gobj, priv->run, "steps", 0, KW_REQUIRED), (size_t)idx);
    json_object_set_new(step, "result", json_integer(-1));
    json_object_set_new(step, "comment", json_sprintf("the agent of '%s' disconnected",
        kw_get_str(gobj, step, "node", "", 0)));
    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_APP,
        "msg",          "%s", "the agent of a step of a scenario run disconnected",
        "run",          "%s", kw_get_str(gobj, priv->run, "id", "", 0),
        "step",         "%d", idx,
        "node",         "%s", kw_get_str(gobj, step, "node", "", 0),
        NULL
    );
    return run_end(gobj, -1, "the agent of a node of the run disconnected");
}

/***************************************************************************
 *  The channel of `side` (an iogate: its children are the channels) that
 *  `g` hangs from, or 0 when `g` is not under it.
 ***************************************************************************/
PRIVATE hgobj channel_of_side(hgobj side, hgobj g)
{
    if(!side) {
        return 0;
    }
    while(g && gobj_parent(g) != side) {
        g = gobj_parent(g);
    }
    return g;
}

/***************************************************************************
 *  The number of the web client connection a channel of __top_side__
 *  holds now (given by ac_on_open), 0 if none.
 ***************************************************************************/
PRIVATE json_int_t connection_number(hgobj channel)
{
    return json_integer_value(gobj_read_user_data(channel, "cc_connection"));
}

/***************************************************************************
 *  Is `channel` still the connection that asked what `kw` answers? The
 *  first frame of its ievent stack (the client's, once this control
 *  center's own is popped) carries the number stamped by command-agent.
 *  Without the stamp (sent before 7.25.15) there is nothing to tell.
 ***************************************************************************/
PRIVATE BOOL same_connection(hgobj gobj, json_t *kw, hgobj channel)
{
    json_t *jn_client = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, 0);
    if(!json_is_object(jn_client) || !kw_has_key(jn_client, "cc_connection")) {
        return TRUE;
    }
    return kw_get_int(gobj, jn_client, "cc_connection", 0, KW_WILD_NUMBER) ==
        connection_number(channel);
}

/***************************************************************************
 *  The run is over: write it (linked to its scenario) and answer the
 *  requester with it, if it is still there.
 ***************************************************************************/
PRIVATE int run_end(hgobj gobj, int result, const char *comment)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->run_timer);
    json_t *run = priv->run;
    json_t *kw_answer = priv->run_kw_answer;
    priv->run = 0;
    priv->run_kw_answer = 0;
    priv->run_agent_channel = 0;
    if(!run) {
        KW_DECREF(kw_answer)
        return 0;
    }

    const char *scenario_id = kw_get_str(gobj, run, "scenario_id", "", 0);
    json_t *steps = kw_get_list(gobj, run, "steps", 0, KW_REQUIRED);
    int done = 0;
    size_t idx; json_t *step;
    json_array_foreach(steps, idx, step) {
        if(kw_get_int(gobj, step, "result", -1, KW_WILD_NUMBER) >= 0 && kw_has_key(step, "result")) {
            done++;
        }
    }
    json_t *jn_text = result < 0?
        json_sprintf("run %s: %s failed, %d of %d steps done: %s",
            kw_get_str(gobj, run, "id", "", 0), kw_get_str(gobj, run, "action", "", 0),
            done, (int)json_array_size(steps), comment) :
        json_sprintf("run %s: %s done, %d steps",
            kw_get_str(gobj, run, "id", "", 0), kw_get_str(gobj, run, "action", "", 0),
            (int)json_array_size(steps));

    char parent_ref[NAME_MAX*2];
    snprintf(parent_ref, sizeof(parent_ref), "scenarios^%s^runs", scenario_id);
    json_t *record = json_pack("{s:s, s:s, s:s, s:s, s:I, s:I, s:i, s:s, s:O}",
        "id", kw_get_str(gobj, run, "id", "", 0),
        "scenario_id", parent_ref,
        "action", kw_get_str(gobj, run, "action", "", 0),
        "username", kw_get_str(gobj, run, "username", "", 0),
        "started_at", (json_int_t)kw_get_int(gobj, run, "started_at", 0, 0),
        "ended_at", (json_int_t)time_in_seconds(),
        "result", result,
        "comment", json_string_value(jn_text),
        "steps", steps
    );
    json_t *node = 0;
    if(priv->gobj_treedb_controlcenter) {
        node = gobj_update_node(
            priv->gobj_treedb_controlcenter,
            "scenario_runs",
            record, // owned
            json_pack("{s:b, s:b, s:b}", "create", 1, "autolink", 1, "fkey_only_id", 1),
            gobj
        );
        if(!node) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB,
                "msg",          "%s", "cannot write the scenario run",
                "run",          "%s", kw_get_str(gobj, run, "id", "", 0),
                NULL
            );
        }
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB,
            "msg",          "%s", "the scenario run ends with the treedb closed, not written",
            "run",          "%s", kw_get_str(gobj, run, "id", "", 0),
            NULL
        );
        JSON_DECREF(record)
    }

    /*
     *  Answer the requester, as ac_final_count() does in the agent
     */
    const char *requester = kw_get_str(gobj, run, "requester", "", 0);
    hgobj gobj_requester = empty_string(requester)? 0 :
        gobj_child_by_name(priv->gobj_top_side, requester);
    json_int_t requester_connection = kw_get_int(gobj, run, "requester_connection", 0, 0);
    if(gobj_requester && requester_connection &&
            requester_connection != connection_number(gobj_requester)) {
        gobj_requester = 0;     // its channel is another client's now
    } else if(!gobj_requester && !empty_string(requester)) {
        gobj_requester = gobj_find_service(requester, FALSE);
    }
    if(!kw_answer) {
        JSON_DECREF(jn_text)
        JSON_DECREF(node)
    } else if(!gobj_requester) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "requester of a scenario run not found, answer dropped",
            "requester",    "%s", requester,
            "run",          "%s", kw_get_str(gobj, run, "id", "", 0),
            NULL
        );
        JSON_DECREF(jn_text)
        JSON_DECREF(node)
        KW_DECREF(kw_answer)
    } else if(!requester_is_listening(gobj, gobj_requester, EV_MT_COMMAND_ANSWER)) {
        JSON_DECREF(jn_text)
        JSON_DECREF(node)
        KW_DECREF(kw_answer)
    } else {
        json_t *jn_data = json_array();
        if(node) {
            json_array_append_new(jn_data, node);
        }
        json_t *webix = msg_iev_build_response(
            gobj,
            result,
            json_sprintf("%s: %s", gobj_yuno_role_plus_name(), json_string_value(jn_text)),
            0,
            jn_data,
            kw_answer   // owned
        );
        JSON_DECREF(jn_text)
        gobj_send_event(
            gobj_requester,
            EV_SEND_IEV,
            iev_create(gobj, EV_MT_COMMAND_ANSWER, webix),
            gobj
        );
    }

    JSON_DECREF(run)
    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  Identity_card on from
 *      Web clients (__top_side__)
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src != priv->gobj_top_side && src != priv->gobj_input_side) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "on_open NOT from GOBJ_TOP_SIDE",
            "src",          "%s", gobj_full_name(src),
            NULL
        );
    }

    if(src == priv->gobj_top_side) {
        hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
        if(channel_gobj) {
            gobj_write_user_data(channel_gobj, "cc_connection", json_integer(++priv->connections));
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Identity_card off from
 *      Web clients (__top_side__)
 *      agent clients (__input_side__)
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);

    if(src == priv->gobj_input_side && priv->run && channel_gobj &&
            channel_gobj == priv->run_agent_channel) {
        run_agent_disconnected(gobj);
    }
    if(src == priv->gobj_top_side && channel_gobj) {
        gobj_write_user_data(channel_gobj, "cc_connection", json_integer(0));
    }

    const char *dst_service = json_string_value(
        gobj_read_user_data(channel_gobj, "tty_mirror_dst_service")
    );

    if(!empty_string(dst_service)) {
        hgobj gobj_requester = gobj_child_by_name(
            priv->gobj_top_side,
            dst_service
        );

        if(!gobj_requester) {
            // Debe venir del agent
        }

        if(gobj_requester) {
            gobj_write_user_data(channel_gobj, "tty_mirror_dst_service", json_string(""));
            gobj_send_event(gobj_requester, EV_DROP, 0, gobj);
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  HACK intermediate node
 ***************************************************************************/
PRIVATE int ac_stats_yuno_answer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_ievent_id = msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID);
    const char *dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);

    hgobj gobj_requester = gobj_child_by_name(
        priv->gobj_top_side,
        dst_service
    );
    JSON_DECREF(jn_ievent_id);

    if(!gobj_requester) {
        // Debe venir del agent
        jn_ievent_id = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, 0);
        JSON_INCREF(jn_ievent_id);
        dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
        gobj_requester = gobj_find_service(dst_service, TRUE);
    }

    if(!gobj_requester) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "service not found",
            "service",      "%s", dst_service,
            NULL
        );
        JSON_DECREF(jn_ievent_id);
        KW_DECREF(kw);
        return 0;
    }
    JSON_DECREF(jn_ievent_id);
    if(!requester_is_listening(gobj, gobj_requester, event)) {
        KW_DECREF(kw);
        return 0;
    }

    KW_INCREF(kw);
    json_t *kw_redirect = msg_iev_set_back_metadata(gobj, kw, kw, TRUE); // "__answer__"

    json_t *iev = iev_create(
        gobj,
        event,
        kw_redirect    // owned
    );

    return gobj_send_event(
        gobj_requester,
        EV_SEND_IEV,
        iev,
        gobj
    );
}

/***************************************************************************
 *  HACK intermediate node
 ***************************************************************************/
PRIVATE int ac_command_yuno_answer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  The answer of a step of a scenario run: this gobj asked it, and
     *  only the agent of the step answers it
     */
    if(kw_get_dict_value(gobj, kw, "__md_iev__`cc_run", 0, 0)) {
        hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, 0);
        if(priv->run && (src != priv->gobj_input_side || channel_gobj != priv->run_agent_channel)) {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL,
                "msg",          "%s", "answer of a scenario run step not from the agent of the step, dropped",
                "run",          "%s", kw_get_str(gobj, kw, "__md_iev__`cc_run", "", 0),
                "src",          "%s", gobj_short_name(src),
                "channel",      "%s", channel_gobj? gobj_short_name(channel_gobj) : "",
                NULL
            );
            KW_DECREF(kw);
            return 0;
        }
        return run_step_answered(gobj, kw);
    }

    json_t *jn_ievent_id = msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID);
    const char *dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);

    hgobj gobj_requester = gobj_child_by_name(
        priv->gobj_top_side,
        dst_service
    );
    JSON_DECREF(jn_ievent_id);

    if(!gobj_requester) {
        // Debe venir del agent
        jn_ievent_id = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, 0);
        JSON_INCREF(jn_ievent_id);
        dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
        gobj_requester = gobj_find_service(dst_service, TRUE);
    }

    if(!gobj_requester) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "service not found",
            "service",      "%s", dst_service,
            NULL
        );
        JSON_DECREF(jn_ievent_id);
        KW_DECREF(kw);
        return 0;
    }
    JSON_DECREF(jn_ievent_id);
    if(!requester_is_listening(gobj, gobj_requester, event)) {
        KW_DECREF(kw);
        return 0;
    }

    KW_INCREF(kw);
    json_t *kw_redirect = msg_iev_set_back_metadata(gobj, kw, kw, TRUE);

    json_t *iev = iev_create(
        gobj,
        event,
        kw_redirect    // owned
    );

    return gobj_send_event(
        gobj_requester,
        EV_SEND_IEV,
        iev,
        gobj
    );
}

/***************************************************************************
 *  HACK intermediate node
 ***************************************************************************/
PRIVATE int ac_tty_mirror_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_ievent_id = msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID);
    const char *dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);

    hgobj gobj_requester = gobj_child_by_name(
        priv->gobj_top_side,
        dst_service
    );
    JSON_DECREF(jn_ievent_id);

    if(!gobj_requester) {
        // Debe venir del agent
        jn_ievent_id = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, 0);
        JSON_INCREF(jn_ievent_id);
        dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
        gobj_requester = gobj_find_service(dst_service, TRUE);
    }

    if(!gobj_requester) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "service not found",
            "service",      "%s", dst_service,
            NULL
        );
        JSON_DECREF(jn_ievent_id);
        KW_DECREF(kw);
        return 0;
    }

    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    gobj_write_user_data(channel_gobj, "tty_mirror_dst_service", json_string(dst_service));

    JSON_DECREF(jn_ievent_id);
    if(!requester_is_listening(gobj, gobj_requester, event)) {
        KW_DECREF(kw);
        return 0;
    }

    KW_INCREF(kw);
    json_t *kw_redirect = msg_iev_set_back_metadata(gobj, kw, kw, TRUE);

    json_t *iev = iev_create(
        gobj,
        event,
        kw_redirect    // owned
    );

    return gobj_send_event(
        gobj_requester,
        EV_SEND_IEV,
        iev,
        gobj
    );
}

/***************************************************************************
 *  HACK intermediate node
 ***************************************************************************/
PRIVATE int ac_tty_mirror_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_ievent_id = msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID);
    const char *dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);

    hgobj gobj_requester = gobj_child_by_name(
        priv->gobj_top_side,
        dst_service
    );
    JSON_DECREF(jn_ievent_id);

    if(!gobj_requester) {
        // Debe venir del agent
        jn_ievent_id = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, 0);
        JSON_INCREF(jn_ievent_id);
        dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
        gobj_requester = gobj_find_service(dst_service, TRUE);
    }

    if(!gobj_requester) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "service not found",
            "service",      "%s", dst_service,
            NULL
        );
        JSON_DECREF(jn_ievent_id);
        KW_DECREF(kw);
        return 0;
    }

    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    gobj_write_user_data(channel_gobj, "tty_mirror_dst_service", json_string(""));

    JSON_DECREF(jn_ievent_id);
    if(!requester_is_listening(gobj, gobj_requester, event)) {
        KW_DECREF(kw);
        return 0;
    }

    KW_INCREF(kw);
    json_t *kw_redirect = msg_iev_set_back_metadata(gobj, kw, kw, TRUE);

    json_t *iev = iev_create(
        gobj,
        event,
        kw_redirect    // owned
    );

    return gobj_send_event(
        gobj_requester,
        EV_SEND_IEV,
        iev,
        gobj
    );
}

/***************************************************************************
 *  HACK intermediate node
 ***************************************************************************/
PRIVATE int ac_tty_mirror_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_ievent_id = msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID);
    const char *dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);

    hgobj gobj_requester = gobj_child_by_name(
        priv->gobj_top_side,
        dst_service
    );
    JSON_DECREF(jn_ievent_id);

    if(!gobj_requester) {
        // Debe venir del agent
        jn_ievent_id = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, 0);
        JSON_INCREF(jn_ievent_id);
        dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
        gobj_requester = gobj_find_service(dst_service, TRUE);
    }

    if(!gobj_requester) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "service not found",
            "service",      "%s", dst_service,
            NULL
        );
        JSON_DECREF(jn_ievent_id);
        KW_DECREF(kw);
        return 0;
    }
    JSON_DECREF(jn_ievent_id);
    if(!requester_is_listening(gobj, gobj_requester, event)) {
        KW_DECREF(kw);
        return 0;
    }

    KW_INCREF(kw);
    json_t *kw_redirect = msg_iev_set_back_metadata(gobj, kw, kw, TRUE);

    json_t *iev = iev_create(
        gobj,
        event,
        kw_redirect    // owned
    );

    return gobj_send_event(
        gobj_requester,
        EV_SEND_IEV,
        iev,
        gobj
    );
}

/***************************************************************************
 *  A reading of a watch-yuno-stats, sent by an agent along the route of the
 *  web client that asked for it: relay it to that client, the way
 *  ac_tty_mirror_data() relays the PTY. The client may be gone -- a tab
 *  closed: the agent only learns when its watch expires, not renewed -- so
 *  a reading for nobody is expected. It is counted, and said once a minute,
 *  not once per reading.
 ***************************************************************************/
PRIVATE int ac_yuno_stats_relay(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  The popped frame is freed here: its dst_service is copied first,
     *  the warning below still names it.
     */
    json_t *jn_ievent_id = msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID);
    char dst_service[NAME_MAX];
    snprintf(dst_service, sizeof(dst_service), "%s",
        kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0));
    JSON_DECREF(jn_ievent_id);

    hgobj gobj_requester = gobj_child_by_name(
        priv->gobj_top_side,
        dst_service
    );
    BOOL reconnected = FALSE;
    if(gobj_requester) {
        /*  Another connection holds the channel (a closed one holds 0 and
         *  is not listening: that one is only gone).  */
        reconnected = !same_connection(gobj, kw, gobj_requester) &&
            connection_number(gobj_requester) != 0;
    } else {
        json_t *jn_next = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, 0);
        snprintf(dst_service, sizeof(dst_service), "%s",
            kw_get_str(gobj, jn_next, "dst_service", "", 0));
        gobj_requester = gobj_find_service(dst_service, FALSE);
    }

    BOOL listening = FALSE;
    if(gobj_requester && !reconnected) {
        if(gobj_has_attr(gobj_requester, "opened")) {
            listening = gobj_read_bool_attr(gobj_requester, "opened");
        } else {
            listening = gobj_in_this_state(gobj_requester, ST_SESSION);
        }
    }
    if(!listening) {
        priv->stats_dropped++;
        if(!priv->t_stats_dropped_log || test_msectimer(priv->t_stats_dropped_log)) {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "yuno stats for a web client that is gone, dropped (the agent's watch expires)",
                "service",      "%s", dst_service,
                "reconnected",  "%s", reconnected? "its channel holds another connection now" : "",
                "dropped",      "%lu", (unsigned long)priv->stats_dropped,
                NULL
            );
            priv->stats_dropped = 0;
            priv->t_stats_dropped_log = start_msectimer(60*1000);
        }
        KW_DECREF(kw);
        return 0;
    }

    KW_INCREF(kw);
    json_t *kw_redirect = msg_iev_set_back_metadata(gobj, kw, kw, TRUE);

    json_t *iev = iev_create(
        gobj,
        event,
        kw_redirect    // owned
    );

    return gobj_send_event(
        gobj_requester,
        EV_SEND_IEV,
        iev,
        gobj
    );
}

/***************************************************************************
 *  HACK intermediate node, pero al revés(???)
 ***************************************************************************/
PRIVATE int ac_write_tty(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS TODO función interna para todos? marca con flag
     *----------------------------------------*/
    const char *permission = "write-tty";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        gobj_send_event(
            src,
            event,
                msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
        KW_DECREF(kw);
        return 0;
    }

    /*----------------------------------------*
     *  Job
     *----------------------------------------*/
    const char *agent_id = kw_get_str(gobj, kw, "agent_id", "", 0);

    json_t *jn_filter = json_pack("{s:s, s:s}",
        "__gclass_name__", C_IEVENT_SRV,
        "__state__", ST_SESSION
    );
    json_t *dl_children = gobj_match_children_tree(priv->gobj_input_side, jn_filter);

    int some = 0;
    int idx; json_t *jn_child;
    json_array_foreach(dl_children, idx, jn_child) {
        hgobj child = (hgobj)(size_t)json_integer_value(jn_child);
        json_t *jn_attrs = gobj_read_json_attr(child, "identity_card");
        if(!empty_string(agent_id)) {
            /*
             *  Match by UUID OR hostname, like command-agent (a caller such
             *  as the browser console addresses nodes by hostname). Matching
             *  only the UUID here silently missed every hostname-addressed
             *  write-tty.
             */
            const char *id_ = kw_get_str(gobj, jn_attrs, "id", "", 0);
            const char *host_ = kw_get_str(gobj, jn_attrs,
                "__md_iev__`ievent_gate_stack`0`host", "", 0);
            if(strcmp(id_, agent_id)!=0 && strcmp(host_, agent_id)!=0) {
                continue;
            }
        }

        json_t *webix = gobj_command( // debe retornar siempre 0.
            child,
            "write-tty",
            json_incref(kw),
            src
        );
        some++;
        JSON_DECREF(webix);
    }

    gobj_free_iter(dl_children);

    if(!some) {
        /*
         *  No connected agent matched agent_id. Do NOT drop src: it is a
         *  shared control channel (the browser console multiplexes every
         *  panel and several PTY consoles over one link), so a single stray
         *  write-tty must not tear the whole session down. Log and ignore;
         *  a truly gone console is cleaned up via EV_TTY_CLOSE / on_close.
         */
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "write-tty: no connected agent matched",
            "agent_id",     "%s", agent_id,
            NULL
        );
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

    if(src == priv->run_timer) {
        KW_DECREF(kw);
        return run_step_timed_out(gobj);
    }

    uint64_t maxtxMsgsec = gobj_read_integer_attr(gobj, "maxtxMsgsec");
    uint64_t maxrxMsgsec = gobj_read_integer_attr(gobj, "maxrxMsgsec");
    if(priv->txMsgsec > maxtxMsgsec) {
        gobj_write_integer_attr(gobj, "maxtxMsgsec", priv->txMsgsec);
    }
    if(priv->rxMsgsec > maxrxMsgsec) {
        gobj_write_integer_attr(gobj, "maxrxMsgsec", priv->rxMsgsec);
    }

    gobj_write_integer_attr(gobj, "txMsgsec", priv->txMsgsec);
    gobj_write_integer_attr(gobj, "rxMsgsec", priv->rxMsgsec);

    priv->rxMsgsec = 0;
    priv->txMsgsec = 0;

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
    .mt_create                  = mt_create,
    .mt_destroy                 = mt_destroy,
    .mt_start                   = mt_start,
    .mt_stop                    = mt_stop,
    .mt_play                    = mt_play,
    .mt_pause                   = mt_pause,
    .mt_writing                 = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_CONTROLCENTER);

/*------------------------*
 *      Events
 *------------------------*/

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
        {EV_MT_STATS_ANSWER,        ac_stats_yuno_answer,    0},
        {EV_MT_COMMAND_ANSWER,      ac_command_yuno_answer,  0},
        {EV_TTY_DATA,               ac_tty_mirror_data,      0},
        {EV_YUNO_STATS,             ac_yuno_stats_relay,     0},
        {EV_WRITE_TTY,              ac_write_tty,            0},

        {EV_ON_OPEN,                ac_on_open,              0},
        {EV_ON_CLOSE,               ac_on_close,             0},
        {EV_TTY_OPEN,               ac_tty_mirror_open,      0},
        {EV_TTY_CLOSE,              ac_tty_mirror_close,     0},

        {EV_TIMEOUT,                ac_timeout,              0},
        {EV_STOPPED,                0,                       0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_MT_COMMAND_ANSWER,      EVF_PUBLIC_EVENT},
        {EV_TTY_DATA,               EVF_PUBLIC_EVENT},
        {EV_YUNO_STATS,             EVF_PUBLIC_EVENT},
        {EV_MT_STATS_ANSWER,        EVF_PUBLIC_EVENT},
        {EV_WRITE_TTY,              0},

        {EV_TTY_OPEN,               EVF_PUBLIC_EVENT},
        {EV_TTY_CLOSE,              EVF_PUBLIC_EVENT},

        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_TIMEOUT,                0},
        {EV_STOPPED,                0},

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
PUBLIC int register_c_controlcenter(void)
{
    return create_gclass(C_CONTROLCENTER);
}
