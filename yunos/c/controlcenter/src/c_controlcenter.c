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
 *          [scenario_yuno=]                (on/off: the SWITCH of that yuno)
 *      scenario-runs scenario_id=          the runs of one, newest first
 *      watch-scenario scenario_id= node=   watch its yunos on that node (the
 *                                          agent's watch-yuno-stats, built here)
 *      my-permissions                      what the user who asks may do here
 *  A scenario permission is checked with the `scenario_id` the command acts
 *  on, so a role whose `parameters` scope it (C_AUTHZ) grants it for some
 *  scenarios only: the list keeps them, save checks the id it saves, and
 *  such a user watches them with watch-scenario, never with command-agent.
 *  A scenario's `max_run` (seconds, 0 = none) is a SAFEGUARD against a test
 *  left running: a `start` that did any step schedules its `stop` max_run
 *  seconds after it; a `stop` that did any step cancels it; pause and resume
 *  leave the clock as it is. When it is due the control center runs `stop`
 *  itself, as one more run (its user, the reason in its comment), a few
 *  seconds later if another run is in flight. The schedule is rebuilt from
 *  the runs when the control center starts, so a restart does not lose it;
 *  `scenarios` answers it as `stop_at` (epoch seconds). Every run that ends
 *  publishes EV_SCENARIO_RUN_ENDED (public, no data): a console that watches
 *  a scenario reads its state again, and sees it stopped by somebody else.
 *  A yuno of a scenario may carry a SWITCH, `switch: {on: [steps], off:
 *  [steps]}` (a step as those of an action; its `yuno` defaults to the
 *  switch's own): the operator turns one generator on or off by hand,
 *  while the others keep going -- to climb the load one fleet at a time.
 *  `run-scenario action=on|off scenario_yuno=<key>` runs them, under
 *  `run-scenarios`, as a run like any other (one at a time, written, told).
 *  `on` runs only while the scenario RUNS (its last run that did anything
 *  is a start or a resume): the max_run stop of a stopped scenario would
 *  not be there to stop it. `off` runs in any state, all its steps at once
 *  as a stop does. A switch run changes neither the state of the scenario
 *  nor its max_run clock: its `stop` is what turns every switch off.
 *  When what a user may do changes in the `authz` store -- a role created,
 *  changed or deleted, a role given or taken -- it publishes
 *  EV_PERMISSIONS_CHANGED (public, no data) to the clients that subscribed,
 *  which ask `my-permissions` again: a console shows the new controls
 *  without a reload. A user's own record is not watched: it changes at every
 *  login (its sessions), and disabling or deleting a user drops its sessions.
 *  A RUN sends each step to the node's agent as `command-yuno`, carrying
 *  the user who asked (`__username__`, what the agent's audit records).
 *  A start or a resume goes one step after the other: a step is sent only
 *  when the one before it answered, and a step that fails, does not answer
 *  in `run_step_timeout`, or whose agent disconnects meanwhile ends the run
 *  there. A stop, a pause or a report sends every step at once, within one
 *  `run_step_timeout`: each result is recorded, and the run ends with -1
 *  naming the steps that failed (up to 7.28.3 these were serial too, and a
 *  stop ended at the first yuno that did not answer). The agent
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
 *  to an agent (command-agent, stats-agent); what comes back later -- the
 *  answer of a command or of a stats, the PTY of an open-console, an
 *  EV_YUNO_STATS still pushed by a watch that has not expired, the answer
 *  of a run -- reaches the channel only if it is still that connection.
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

/*  The longest max_run of a scenario: a week, in seconds  */
#define MAX_RUN_LIMIT   (7*24*3600)

/*  What the users store publishes when something in it changes  */
#define AUTHZ_STORE_EVENTS {                \
    EV_TREEDB_NODE_CREATED,                 \
    EV_TREEDB_NODE_UPDATED,                 \
    EV_TREEDB_NODE_DELETED,                 \
    EV_TREEDB_NODE_LINKED,                  \
    EV_TREEDB_NODE_UNLINKED,                \
    0                                       \
}

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

PRIVATE json_t *cmd_watch_scenario(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_my_permissions(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *forward_to_agent(hgobj gobj, json_t *kw, hgobj src);
PRIVATE int ac_authz_store_changed(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src);
PRIVATE int start_run(
    hgobj gobj,
    const char *scenario_id,
    const char *action,
    const char *username,
    const char *requester,
    json_int_t requester_connection,
    json_t *kw_answer,
    const char *reason,
    const char *scenario_yuno,
    char *err,
    size_t errsz
);
PRIVATE void schedule_from_runs(hgobj gobj, const char *scenario_id);
PRIVATE int run_steps_done(hgobj gobj, json_t *run);
PRIVATE BOOL run_changes_state(hgobj gobj, json_t *run);
PRIVATE BOOL scenario_runs_now(hgobj gobj, const char *scenario_id);
PRIVATE void retry_stops_waiting_node(hgobj gobj);
PRIVATE BOOL steps_reach_a_node(hgobj gobj, json_t *steps);
PRIVATE BOOL stop_reaches_a_node(hgobj gobj, const char *scenario_id);
PRIVATE BOOL scenario_is_running(hgobj gobj, const char *scenario_id);
PRIVATE void add_stop_at(hgobj gobj, json_t *scenario);
PRIVATE void arm_stop_timer(hgobj gobj);
PRIVATE int stop_due_scenario(hgobj gobj);
GOBJ_DECLARE_EVENT(EV_SCENARIO_RUN_ENDED);      // defined with the gclass, below
GOBJ_DECLARE_EVENT(EV_PERMISSIONS_CHANGED);     // defined with the gclass, below
PRIVATE json_t *refuse_scenario_command(
    hgobj gobj,
    const char *permission,
    const char *scenario_id,
    json_t *kw,
    hgobj src
);
PRIVATE BOOL user_may_scenario(hgobj gobj, const char *permission, const char *scenario_id, json_t *kw, hgobj src);
PRIVATE json_t *treedb_tranger(hgobj gobj);
PRIVATE const char *scenario_id_of(hgobj gobj, json_t *kw);
PRIVATE const char *username_of(hgobj gobj, json_t *kw, hgobj src);
PRIVATE BOOL is_scenario_action(const char *action);
PRIVATE BOOL is_switch_action(const char *action);
PRIVATE BOOL in_comma_list(const char *list, const char *name);
PRIVATE int check_scenario(hgobj gobj, json_t *scenario, char *err, size_t errsz);
PRIVATE json_t *build_run_steps(
    hgobj gobj,
    json_t *scenario,
    const char *action,
    const char *scenario_yuno,
    char *err,
    size_t errsz
);
PRIVATE json_t *sort_runs_newest_first(json_t *runs);
PRIVATE BOOL requester_listens(hgobj requester);
PRIVATE BOOL requester_is_listening(hgobj gobj, hgobj requester, gobj_event_t event);
PRIVATE int run_send_step(hgobj gobj);
PRIVATE int run_send_all_steps(hgobj gobj);
PRIVATE int run_parallel_step_done(hgobj gobj);
PRIVATE hgobj run_channel_of_step(hgobj gobj, int idx);
PRIVATE BOOL run_uses_channel(hgobj gobj, hgobj channel);
PRIVATE int run_step_answered(hgobj gobj, json_t *kw);
PRIVATE int run_step_timed_out(hgobj gobj);
PRIVATE int run_end(hgobj gobj, int result, const char *comment);
PRIVATE int run_agent_disconnected(hgobj gobj, hgobj channel);
PRIVATE hgobj channel_of_side(hgobj side, hgobj g);
PRIVATE json_int_t connection_number(hgobj channel);
PRIVATE BOOL same_connection(hgobj gobj, json_t *kw, hgobj channel);
PRIVATE void stamp_client_connection(hgobj gobj, json_t *kw, hgobj src);
PRIVATE BOOL client_relays(json_t *kw, gobj_event_t event);
PRIVATE void record_link_request(hgobj gobj, hgobj src, hgobj agent_srv);
PRIVATE hgobj requester_of_route(
    hgobj gobj,
    json_t *kw,
    char *dst_service,
    size_t dst_service_size,
    BOOL *reconnected
);
PRIVATE hgobj requester_of_answer(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,
    char *dst_service,
    size_t dst_service_size
);
PRIVATE int relay_to_requester(hgobj gobj, gobj_event_t event, json_t *kw, hgobj requester);
PRIVATE BOOL is_from_agent_side(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src);
PRIVATE void say_pending_drops(hgobj gobj, const char *when, BOOL only_ended);
PRIVATE void arm_drops_timer(hgobj gobj);
PRIVATE void update_rates(hgobj gobj);
PRIVATE void start_rates_tick(hgobj gobj);
PRIVATE int check_drops_warning_window(hgobj gobj);
PRIVATE BOOL drops_window_ended(hgobj gobj, uint64_t window_start);
PRIVATE void count_dropped_stream(
    hgobj gobj,
    gobj_event_t event,
    const char *dst_service,
    BOOL reconnected,
    uint64_t *dropped,
    uint64_t *t_next_log
);

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
SDATAPM (DTP_STRING,    "content64",    SDF_SECRET,     0,          "Content64 data to write to tty: keystrokes, a typed password among them"),
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
PRIVATE sdata_desc_t pm_watch_scenario[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "scenario_id",  0,              0,          "Scenario id"),
SDATAPM (DTP_STRING,    "node",         0,              0,          "Node of the scenario whose yunos to watch (its agent id)"),
SDATAPM (DTP_STRING,    "keys",         0,              "",         "Keys of the yunos of that node to watch, comma separated. Empty: all of them"),
SDATAPM (DTP_INTEGER,   "period",       0,              "2000",     "Milliseconds between two readings"),
SDATAPM (DTP_BOOLEAN,   "stop",         0,              "0",        "Stop the watch of this node"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_run_scenario[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "scenario_id",  0,              0,          "Scenario id"),
SDATAPM (DTP_STRING,    "action",       0,              0,          "Action to run: start, pause, resume, stop or report; on or off, the switch of a yuno"),
SDATAPM (DTP_STRING,    "scenario_yuno",0,              0,          "Key of the yuno whose switch to turn (action on or off)"),
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
SDATACM2 (DTP_SCHEMA,   "watch-scenario",   0,                  0,              pm_watch_scenario,cmd_watch_scenario,"Watch the yunos of a scenario on one node (watch-yuno-stats to its agent, built here)"),
SDATACM2 (DTP_SCHEMA,   "my-permissions",   0,                  0,              0,              cmd_my_permissions,"The permissions of the user who asks, in this service"),

SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description---------- */
SDATA (DTP_STRING,      "__username__",     SDF_RD,             "",         "Username 'yuneta', permission for all"),
SDATA (DTP_INTEGER,     "txMsgs",           SDF_RD|SDF_RSTATS,  0,          "Messages sent: command-agent/stats-agent/write-tty requests and run steps to the agents, answers and streams of the agents and run answers to the clients"),
SDATA (DTP_INTEGER,     "rxMsgs",           SDF_RD|SDF_RSTATS,  0,          "Messages received: command-agent/stats-agent/write-tty/run-scenario requests of the clients, answers and streams of the agents"),

SDATA (DTP_INTEGER,     "txMsgsec",         SDF_RD|SDF_RSTATS,  0,          "txMsgs by second over the last tick (`timeout`), exact interval"),
SDATA (DTP_INTEGER,     "rxMsgsec",         SDF_RD|SDF_RSTATS,  0,          "rxMsgs by second over the last tick (`timeout`), exact interval"),
SDATA (DTP_INTEGER,     "maxtxMsgsec",      SDF_WR|SDF_RSTATS,  0,          "Max Tx Messages by second (write 0 to start again)"),
SDATA (DTP_INTEGER,     "maxrxMsgsec",      SDF_WR|SDF_RSTATS,  0,          "Max Rx Messages by second (write 0 to start again)"),

SDATA (DTP_INTEGER,     "run_step_timeout", SDF_WR|SDF_PERSIST, "30000",    "Milliseconds a step of a scenario run may take to be answered"),
SDATA (DTP_INTEGER,     "drops_warning_window",SDF_WR,          "60000",    "Milliseconds of a capped warning's window (dropped streams, unrouted PTY, injected agent events): one warning per window, the rest counted and said when it ends"),
SDATA (DTP_BOOLEAN,     "drops_timer_armed",SDF_RD,             "0",        "A capped warning has a count to say when its window ends"),

SDATA (DTP_INTEGER,     "timeout",          SDF_WR,             "1000",     "Period of the rate tick, ms: txMsgsec/rxMsgsec and their maxima are computed on it"),
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

/*
 *  The key of the kw a scenario permission is asked with: built here from
 *  the scenario the command acts on (user_may_scenario()), so a role may
 *  scope these permissions on it (C_AUTHZ `parameters`).
 */
PRIVATE sdata_desc_t pm_authz_scenario[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "scenario_id",  0,              0,          "The scenario the permission is asked for"),
SDATA_END()
};

PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
SDATAAUTHZ (DTP_SCHEMA, "list-agents",      0,      0,      0,      "Permission to list remote agents"),
SDATAAUTHZ (DTP_SCHEMA, "command-agent",    0,      0,      0,      "Permission to send command to remote agent"),
SDATAAUTHZ (DTP_SCHEMA, "drop-agent",       0,      0,      0,      "Permission to drop connection to remote agent"),
SDATAAUTHZ (DTP_SCHEMA, "logout-user",      0,      0,      0,      "Permission to logout users"),
SDATAAUTHZ (DTP_SCHEMA, "write-tty",        0,      0,      0,      "Internal use. Feed remote consola from local keyboard"),

SDATAAUTHZ (DTP_SCHEMA, "read-scenarios",   0,      0,      pm_authz_scenario, "Permission to read the scenarios and their runs"),
SDATAAUTHZ (DTP_SCHEMA, "write-scenarios",  0,      0,      pm_authz_scenario, "Permission to save and delete scenarios"),
SDATAAUTHZ (DTP_SCHEMA, "run-scenarios",    0,      0,      pm_authz_scenario, "Permission to run the actions of a scenario"),

SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int32_t timeout;
    json_int_t drops_warning_window;

    hgobj gobj_top_side;
    hgobj gobj_input_side;

    hgobj gobj_treedbs;
    hgobj gobj_treedb_controlcenter;
    hgobj gobj_treedb_authzs;       // the users store of C_AUTHZ: watched for permission changes
    hgobj gobj_authz;


    json_int_t txMsgs;              // relayed: requests to agents, answers and streams to clients
    json_int_t rxMsgs;              // to relay: requests of clients, answers and streams of agents
    json_int_t last_txMsgs;         // at the last rate computation
    json_int_t last_rxMsgs;
    json_int_t txMsgsec;
    json_int_t rxMsgsec;
    uint64_t t_rates;               // msectimer of the last rate tick (0: none)
    hgobj rates_timer;              // the rate tick, every `timeout` ms while playing

    uint64_t stats_dropped;         // EV_YUNO_STATS for a web client that is gone
    uint64_t t_stats_dropped_log;   // msectimer: start of its window (0: none)
    uint64_t tty_dropped;           // EV_TTY_DATA for a web client that is gone
    uint64_t t_tty_dropped_log;     // msectimer: start of its window (0: none)
    uint64_t tty_unrouted;          // EV_TTY_DATA routed to no requester
    uint64_t t_tty_unrouted_log;    // msectimer: start of its window (0: none)
    uint64_t injected;              // events of an agent sent from __top_side__
    uint64_t t_injected_log;        // msectimer: start of its window (0: none)
    hgobj drops_timer;              // says the counts above when their window ends
    BOOL drops_timer_armed;
    uint64_t drops_timer_end;       // msectimer the drops timer is armed for

    hgobj run_timer;                // deadline of the step of the run in flight
    hgobj stop_timer;               // the next stop a scenario's max_run is due
    json_t *auto_stops;             // scenario id -> {stop_at, scheduled_at: epoch seconds, started_by} (max_run)
    uint64_t last_run_ms;           // the millisecond of the last run id: never twice the same
    json_t *run;                    // the run in flight, or NULL (one at a time)
    json_t *run_kw_answer;          // the request of that run, to answer it
    json_t *run_channels;           // "<step idx>" -> channel (in __input_side__) of each step in flight

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

    priv->run_timer = gobj_create_pure_child("run_timer", C_TIMER, 0, gobj);
    priv->stop_timer = gobj_create_pure_child("stop_timer", C_TIMER, 0, gobj);
    priv->auto_stops = json_object();
    priv->rates_timer = gobj_create_pure_child("rates_timer", C_TIMER0, 0, gobj);
    priv->drops_timer = gobj_create_pure_child("drops_timer", C_TIMER, 0, gobj);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,               gobj_read_integer_attr)
    if(priv->timeout < 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "timeout (the rate tick) must be 1 ms or more, 1000 put back",
            "timeout",      "%d", (int)priv->timeout,
            NULL
        );
        gobj_write_integer_attr(gobj, "timeout", 1000);
        priv->timeout = 1000;
    }
    SET_PRIV(drops_warning_window,  gobj_read_integer_attr)
    check_drops_warning_window(gobj);
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout,             gobj_read_integer_attr)
        if(priv->timeout < 1) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "timeout (the rate tick) must be 1 ms or more, 1000 put back",
                "timeout",      "%d", (int)priv->timeout,
                NULL
            );
            gobj_write_integer_attr(gobj, "timeout", 1000);    // mt_writing() takes it
        } else if(gobj_is_playing(gobj)) {
            start_rates_tick(gobj);
        }
    ELIF_EQ_SET_PRIV(drops_warning_window, gobj_read_integer_attr)
        if(check_drops_warning_window(gobj) == 0) {
            arm_drops_timer(gobj);  // the windows that are open now end with the new value
        }
    END_EQ_SET_PRIV()

    /*
     *  `stats=__reset__` writes the defaults of the SDF_RSTATS attrs: the
     *  counters live in priv (mt_reading), so they are zeroed here, with
     *  their rates
     */
    if(strcmp(path, "txMsgs")==0) {
        priv->txMsgs = 0;
        priv->last_txMsgs = 0;
        priv->txMsgsec = 0;
    } else if(strcmp(path, "rxMsgs")==0) {
        priv->rxMsgs = 0;
        priv->last_rxMsgs = 0;
        priv->rxMsgsec = 0;
    }
}

/***************************************************************************
 *      Framework Method reading
 *  The message counters and their rates (SDF_RSTATS, backed by priv), as
 *  the rate tick left them: a reading computes nothing, so whoever reads,
 *  and however often, reads the same.
 ***************************************************************************/
PRIVATE SData_Value_t mt_reading(hgobj gobj, const char *name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    SData_Value_t v = {0,{0}};
    if(strcmp(name, "txMsgs")==0) {
        v.found = 1;
        v.v.i = priv->txMsgs;
    } else if(strcmp(name, "rxMsgs")==0) {
        v.found = 1;
        v.v.i = priv->rxMsgs;
    } else if(strcmp(name, "txMsgsec")==0) {
        v.found = 1;
        v.v.i = priv->txMsgsec;
    } else if(strcmp(name, "rxMsgsec")==0) {
        v.found = 1;
        v.v.i = priv->rxMsgsec;
    }
    return v;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(priv->run)
    JSON_DECREF(priv->run_channels)
    KW_DECREF(priv->run_kw_answer)
    JSON_DECREF(priv->auto_stops)
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

    return 0;   // the timers start when they are set (C_TIMER's msec)
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    say_pending_drops(gobj, "the control center stops", FALSE);
    /*
     *  clear_timeout() stops a C_TIMER that runs and leaves alone one that
     *  does not: a gobj_stop() after it logged "GObj NOT RUNNING"
     */
    clear_timeout(priv->drops_timer);
    priv->drops_timer_armed = FALSE;
    gobj_write_bool_attr(gobj, "drops_timer_armed", FALSE);
    clear_timeout(priv->run_timer);
    clear_timeout(priv->stop_timer);
    clear_timeout0(priv->rates_timer);
    if(gobj_is_running(priv->rates_timer)) {
        gobj_stop(priv->rates_timer);
    }
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
    if(!yuneta_realm_store_dir(
        path,
        sizeof(path),
        gobj_yuno_role(),
        gobj_yuno_realm_owner(),
        gobj_yuno_realm_id(),
        "",  // tenant
        "",  // gclass-treedb controls the directories
        TRUE
    )) {
        gobj_log_critical(gobj, LOG_OPT_EXIT_ZERO,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot build or create the store directory",
            NULL
        );
        return -1;
    }
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

    /*
     *  The stops a max_run had scheduled before a restart, from the runs
     */
    json_object_clear(priv->auto_stops);
    if(priv->gobj_treedb_controlcenter) {
        json_t *scenarios = gobj_list_nodes(
            priv->gobj_treedb_controlcenter, "scenarios", 0, json_pack("{s:b}", "only_id", 1), gobj
        );
        size_t idx; json_t *jn_sc;
        json_array_foreach(scenarios, idx, jn_sc) {
            schedule_from_runs(gobj, json_is_string(jn_sc)?
                json_string_value(jn_sc) : kw_get_str(gobj, jn_sc, "id", "", 0));
        }
        JSON_DECREF(scenarios)

        /*
         *  A stop already due waits for an agent of its nodes: none is
         *  connected yet (__input_side__ starts below), and run now it
         *  could only write a failed run (retry_stops_waiting_node()).
         */
        json_int_t now = (json_int_t)time_in_seconds();
        const char *id; json_t *jn_at;
        json_object_foreach(priv->auto_stops, id, jn_at) {
            if(kw_get_int(gobj, jn_at, "stop_at", 0, KW_REQUIRED) <= now) {
                json_object_set_new(jn_at, "waits_node", json_true());
            }
        }
    }
    arm_stop_timer(gobj);

    /*-------------------------*
     *      Start services
     *-------------------------*/
    priv->gobj_top_side = gobj_find_service("__top_side__", TRUE);
    gobj_subscribe_event(priv->gobj_top_side, 0, 0, gobj);
    gobj_start_tree(priv->gobj_top_side);

    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, 0, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

    /*
     *  What a user may do lives in the users store of C_AUTHZ. A control
     *  center with no store (a test yuno) has no permissions to announce.
     */
    priv->gobj_treedb_authzs = gobj_find_service("treedb_authzs", FALSE);
    if(priv->gobj_treedb_authzs) {
        gobj_event_t authz_store_events[] = AUTHZ_STORE_EVENTS;
        for(int i=0; authz_store_events[i]; i++) {
            gobj_subscribe_event(priv->gobj_treedb_authzs, authz_store_events[i], 0, gobj);
        }
    }

    start_rates_tick(gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gobj_treedb_authzs) {
        gobj_event_t authz_store_events[] = AUTHZ_STORE_EVENTS;
        for(int i=0; authz_store_events[i]; i++) {
            gobj_unsubscribe_event(priv->gobj_treedb_authzs, authz_store_events[i], 0, gobj);
        }
        priv->gobj_treedb_authzs = 0;
    }

    /*---------------------------------------*
     *  A run in flight ends here: written
     *  while the treedb is open, answered
     *  while its requester is connected.
     *---------------------------------------*/
    if(priv->run) {
        run_end(gobj, -1, "the control center was paused in the middle of the run");
    }
    clear_timeout(priv->stop_timer);
    json_object_clear(priv->auto_stops);  // rebuilt from the runs at play

    clear_timeout0(priv->rates_timer);
    priv->txMsgsec = 0;
    priv->rxMsgsec = 0;

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
    priv->rxMsgs++;
    return forward_to_agent(gobj, kw, src);
}

/***************************************************************************
 *  Send `cmd2agent` to the agent `agent_id`, along the route of the
 *  client's request: what the agent answers, or pushes, goes back to it.
 *  The kw is the client's, cleaned of what is not the agent's business.
 *  Answers at once with the dispatch ("Command sent to N nodes").
 ***************************************************************************/
PRIVATE json_t *forward_to_agent(hgobj gobj, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL client_takes_yuno_stats = client_relays(kw, EV_YUNO_STATS);

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
     *  Tell the agent which of its pushed events reach the client: an
     *  agent sends a watch-yuno-stats through a control center only if it
     *  says EV_YUNO_STATS here. It is said only when BOTH take it: this
     *  control center relays it (an older one, which does not know the
     *  event, drops the agent's connection when it gets one, and does not
     *  write this key), and the client said so in its own __relays__ (a
     *  ycommand does not: it logs "Event NOT DEFINED" on each one). The
     *  client's other entries are not the agent's business.
     */
    if(client_takes_yuno_stats) {
        json_object_set_new(kw, "__relays__", json_pack("[s]", EV_YUNO_STATS));
    }

    stamp_client_connection(gobj, kw, src);

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

        record_link_request(gobj, src, child);
        priv->txMsgs++;
        json_t *webix = gobj_command( // debe retornar siempre 0.
            child,
            cmd2agent,
            kw_incref(kw),
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
    priv->rxMsgs++;
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

    stamp_client_connection(gobj, kw, src);

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

        record_link_request(gobj, src, child);
        priv->txMsgs++;
        json_t *webix = gobj_stats( // debe retornar siempre 0.
            child,
            stats2agent,
            kw_incref(kw),
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

    /*
     *  One scenario is asked of its id; the list keeps the ones the user
     *  may read, each asked of ITS id (all of them for an unscoped grant,
     *  some for a role scoped by `scenario_id`).
     */
    const char *scenario_id = scenario_id_of(gobj, kw);
    BOOL whole_list = empty_string(scenario_id);
    json_t *refused = refuse_scenario_command(gobj,
        whole_list? NULL : "read-scenarios", scenario_id, kw, src);
    if(refused) {
        return refused;
    }

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

    size_t i_sc; json_t *jn_sc;
    json_array_foreach(jn_data, i_sc, jn_sc) {
        add_stop_at(gobj, jn_sc);
    }

    if(whole_list) {
        json_t *mine = json_array();
        int idx; json_t *scenario;
        json_array_foreach(jn_data, idx, scenario) {
            if(user_may_scenario(gobj, "read-scenarios",
                    kw_get_str(gobj, scenario, "id", "", 0), kw, src)) {
                json_array_append(mine, scenario);
            }
        }
        JSON_DECREF(jn_data)
        jn_data = mine;
        /*  An empty store is an empty list for who may read it  */
        if(json_array_size(jn_data)==0 &&
                !user_may_scenario(gobj, "read-scenarios", NULL, kw, src)) {
            JSON_DECREF(jn_data)
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("%s: no permission to '%s'", gobj_yuno_role_plus_name(), "read-scenarios"),
                0,
                0,
                kw  // owned
            );
        }
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

    json_t *refused = refuse_scenario_command(gobj, NULL, NULL, kw, src);
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

    /*
     *  The permission is asked of the id SAVED, never of the client's kw:
     *  a `scenario_id` beside the scenario would answer for another one.
     *  A scenario that RUNS (its last run that did anything is not a stop,
     *  or a run of it is in flight) needs run-scenarios too, whatever its
     *  max_run says before or after: a max_run saved now schedules a stop
     *  the control center runs with the stop steps saved now, and writing
     *  them would be running them.
     */
    {
        json_t *jn_id = json_is_object(jn_scenario)? json_object_get(jn_scenario, "id") : NULL;
        const char *saved_id = json_is_string(jn_id)? json_string_value(jn_id) : NULL;
        if(!user_may_scenario(gobj, "write-scenarios", saved_id, kw, src)) {
            JSON_DECREF(jn_parsed)
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("%s: no permission to '%s'", gobj_yuno_role_plus_name(), "write-scenarios"),
                0,
                0,
                kw  // owned
            );
        }
        if(saved_id && scenario_runs_now(gobj, saved_id) &&
                !user_may_scenario(gobj, "run-scenarios", saved_id, kw, src)) {
            JSON_DECREF(jn_parsed)
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("%s: scenario '%s' runs: saving it needs 'run-scenarios' too",
                    gobj_yuno_role_plus_name(), saved_id),
                0,
                0,
                kw  // owned
            );
        }
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
    json_t *jn_max_run = json_object_get(jn_scenario, "max_run");
    json_object_set_new(record, "max_run",
        json_integer(json_is_integer(jn_max_run)? json_integer_value(jn_max_run) : 0));
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

    /*  A max_run changed while it runs moves its stop  */
    schedule_from_runs(gobj, scenario_id);
    arm_stop_timer(gobj);
    add_stop_at(gobj, node);

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
 *  scenario, and says nothing without it. One that RUNS (scenario_runs_now())
 *  needs run-scenarios too, as saving it does: its delete takes its stop
 *  steps and the stop its max_run schedules, so nobody stops its yunos
 *  from the control center any more -- that is a decision about running
 *  them. Up to 7.29.1 write-scenarios was enough.
 ***************************************************************************/
PRIVATE json_t *cmd_delete_scenario(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *refused = refuse_scenario_command(gobj, "write-scenarios", scenario_id_of(gobj, kw), kw, src);
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
    BOOL runs_now = scenario_runs_now(gobj, scenario_id);
    if(runs_now && !user_may_scenario(gobj, "run-scenarios", scenario_id, kw, src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: scenario '%s' runs: deleting it needs 'run-scenarios' too",
                gobj_yuno_role_plus_name(), scenario_id),
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

    if(runs_now) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "a scenario deleted while it runs: its yunos are left as they are",
            "scenario_id",  "%s", scenario_id,
            "username",     "%s", username_of(gobj, kw, src),
            "max_run_stop_dropped", "%s", json_object_get(priv->auto_stops, scenario_id)? "yes" : "no",
            NULL
        );
    }
    json_object_del(priv->auto_stops, scenario_id);
    arm_stop_timer(gobj);

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: scenario deleted: %s (and %d runs)%s",
            gobj_yuno_role_plus_name(), scenario_id, deleted_runs,
            runs_now? "; it was running: its yunos are left as they are" : ""),
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

    json_t *refused = refuse_scenario_command(gobj, "run-scenarios", scenario_id_of(gobj, kw), kw, src);
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
    const char *scenario_yuno = kw_get_str(gobj, kw, "scenario_yuno", "", 0);
    if(!is_scenario_action(action) && !is_switch_action(action)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: what action? start, pause, resume, stop or report; on or off with a scenario_yuno",
                gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    if(is_switch_action(action) == empty_string(scenario_yuno)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: scenario_yuno goes with on and off, and only with them",
                gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    if(strcmp(action, "on")==0 && !scenario_is_running(gobj, scenario_id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: '%s' does not run: a switch is turned on only while its scenario runs",
                gobj_yuno_role_plus_name(), scenario_id),
            0,
            0,
            kw  // owned
        );
    }

    char err[512];
    hgobj client_channel = channel_of_side(priv->gobj_top_side, src);
    if(start_run(gobj,
            scenario_id,
            action,
            username_of(gobj, kw, src),
            kw_get_str(gobj, kw, "__md_iev__`ievent_gate_stack`0`input_channel", "", 0),
            client_channel? connection_number(client_channel) : (json_int_t)0,
            kw,     // owned by the run if it starts: answered by run_end()
            "",
            scenario_yuno,
            err, sizeof(err))<0) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: %s", gobj_yuno_role_plus_name(), err),
            0,
            0,
            kw  // owned
        );
    }
    priv->rxMsgs++;
    return 0;   /* Asynchronous response */
}

/***************************************************************************
 *  Start a run of `action` of a scenario: its steps one after the other
 *  (run_send_step()), answered by run_end() to `kw_answer` (owned when it
 *  starts; NULL for a run of the control center itself, a max_run stop).
 *  `scenario_yuno` names the yuno whose switch an on/off turns ("" for an
 *  action of the scenario).
 *  -1 with the reason in `err` when it cannot start, and kw_answer is the
 *  caller's still.
 ***************************************************************************/
PRIVATE int start_run(
    hgobj gobj,
    const char *scenario_id,
    const char *action,
    const char *username,
    const char *requester,
    json_int_t requester_connection,
    json_t *kw_answer,
    const char *reason,
    const char *scenario_yuno,
    char *err,
    size_t errsz
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->run) {
        snprintf(err, errsz, "a run is in flight, one at a time: %s",
            kw_get_str(gobj, priv->run, "id", "", 0));
        return -1;
    }
    if(!priv->gobj_treedb_controlcenter) {
        snprintf(err, errsz, "the scenarios are not open yet");
        return -1;
    }
    json_t *scenario = gobj_get_node(
        priv->gobj_treedb_controlcenter,
        "scenarios",
        json_pack("{s:s}", "id", scenario_id),
        0,
        gobj
    );
    if(!scenario) {
        snprintf(err, errsz, "scenario not found: '%s'", scenario_id);
        return -1;
    }

    char why[256];
    json_t *steps = build_run_steps(gobj, scenario, action, scenario_yuno, why, sizeof(why));
    JSON_DECREF(scenario)
    if(!steps) {
        snprintf(err, errsz, "cannot run '%s' of '%s': %s", action, scenario_id, why);
        return -1;
    }

    /*  Two runs in one millisecond would share the id, and the second
     *  record would replace the first  */
    uint64_t run_ms = time_in_milliseconds();
    if(run_ms <= priv->last_run_ms) {
        run_ms = priv->last_run_ms + 1;
    }
    priv->last_run_ms = run_ms;

    char run_id[NAME_MAX];
    int len = snprintf(run_id, sizeof(run_id), "%s.%llu",
        scenario_id, (unsigned long long)run_ms);
    if(len < 0 || (size_t)len >= sizeof(run_id)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "scenario id too long for a run id",
            "scenario_id",  "%s", scenario_id,
            NULL
        );
        JSON_DECREF(steps)
        snprintf(err, errsz, "scenario id too long to run it: '%s'", scenario_id);
        return -1;
    }

    priv->run = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:I, s:I, s:o, s:i, s:s}",
        "id", run_id,
        "scenario_id", scenario_id,
        "action", action,
        "scenario_yuno", scenario_yuno? scenario_yuno : "",
        "username", username,
        "requester", requester? requester : "",
        "requester_connection", requester_connection,
        "started_at", (json_int_t)time_in_seconds(),
        "steps", steps, // owned
        "idx", 0,
        "reason", reason? reason : ""
    );
    priv->run_kw_answer = kw_answer;
    JSON_DECREF(priv->run_channels)
    priv->run_channels = json_object();

    /*
     *  A stop, a pause or a report asks every yuno at once, and waits for
     *  them all: one yuno that does not answer must not leave the others
     *  running (a stop that ended at its first unanswered step left the
     *  simulators of a node generating, and the max_run stop failed every
     *  minute on the same dead one). A start or a resume goes in order,
     *  and ends at the first step that fails: what follows depends on it.
     *  A switch alike: off is a stop of one yuno, on a start of one.
     */
    if(strcmp(action, "stop")==0 || strcmp(action, "pause")==0 || strcmp(action, "report")==0 ||
            strcmp(action, "off")==0) {
        json_object_set_new(priv->run, "parallel", json_true());
        return run_send_all_steps(gobj);
    }
    run_send_step(gobj);
    return 0;
}

/***************************************************************************
 *  The runs of a scenario, newest first.
 ***************************************************************************/
PRIVATE json_t *cmd_scenario_runs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *refused = refuse_scenario_command(gobj, "read-scenarios", scenario_id_of(gobj, kw), kw, src);
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

/***************************************************************************
 *  Watch the yunos of a scenario on one of its nodes: the watch-yuno-stats
 *  of that node's agent, BUILT HERE from the scenario, so a user who may
 *  read the scenario sees its yunos live without `command-agent` (which
 *  sends anything to any node). `keys` narrows it to some of the yunos of
 *  that node (a console pauses the readings of a yuno); `stop=1` ends it.
 *  A yuno whose queue is another service's (`queue_service`, a gate's
 *  __output_side__) is watched on that service too.
 *  The readings come back as from `command-agent` (EV_YUNO_STATS, when the
 *  client's __relays__ says it takes them).
 ***************************************************************************/
PRIVATE json_t *cmd_watch_scenario(hgobj gobj, const char *cmd, json_t *kw_, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *kw = json_deep_copy(kw_);
    KW_DECREF(kw_);

    json_t *refused = refuse_scenario_command(gobj, "read-scenarios", scenario_id_of(gobj, kw), kw, src);
    if(refused) {
        return refused;
    }

    const char *scenario_id = scenario_id_of(gobj, kw);
    const char *node = kw_get_str(gobj, kw, "node", "", 0);
    if(empty_string(scenario_id) || empty_string(node)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: what scenario and node? scenario_id=<id> node=<node>",
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
    if(!empty_string(kw_get_str(gobj, scenario, "agent_url", "", 0))) {
        JSON_DECREF(scenario)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: scenario '%s' is watched through its agent_url, not here",
                gobj_yuno_role_plus_name(), scenario_id),
            0,
            0,
            kw  // owned
        );
    }

    const char *keys = kw_get_str(gobj, kw, "keys", "", 0);
    BOOL stop = kw_get_bool(gobj, kw, "stop", 0, 0);

    gbuffer_t *gbuf = gbuffer_create(256, 4*1024);
    int n = 0;
    if(stop) {
        gbuffer_printf(gbuf, "watch-yuno-stats stop=1");
        n = 1;
    } else {
        const char *scenario_node = kw_get_str(gobj, scenario, "node", "", 0);
        json_t *yunos = kw_get_list(gobj, scenario, "yunos", 0, 0);
        int idx; json_t *yuno;
        json_array_foreach(yunos, idx, yuno) {
            const char *y_node = kw_get_str(gobj, yuno, "node", "", 0);
            if(strcmp(empty_string(y_node)? scenario_node : y_node, node)!=0) {
                continue;
            }
            const char *y_id = kw_get_str(gobj, yuno, "id", "", 0);
            const char *y_key = kw_get_str(gobj, yuno, "key", "", 0);
            if(empty_string(y_key)) {
                y_key = y_id;
            }
            if(!empty_string(keys) && !in_comma_list(keys, y_key)) {
                continue;
            }
            const char *y_service = kw_get_str(gobj, yuno, "service", "", 0);
            gbuffer_printf(gbuf, "%s%s%s%s",
                n? "," : "watch-yuno-stats ids=",
                y_id,
                empty_string(y_service)? "" : ":",
                y_service
            );
            n++;
            const char *q_service = kw_get_str(gobj, yuno, "queue_service", "", 0);
            if(!empty_string(q_service) && strcmp(q_service, y_service)!=0) {
                gbuffer_printf(gbuf, ",%s:%s", y_id, q_service);
            }
        }
        if(n) {
            gbuffer_printf(gbuf, " period=%lld",
                (long long)kw_get_int(gobj, kw, "period", 2000, 0));
        }
    }
    JSON_DECREF(scenario)

    if(!n) {
        GBUFFER_DECREF(gbuf)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: scenario '%s' has no yuno to watch on node '%s'",
                gobj_yuno_role_plus_name(), scenario_id, node),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  The agent gets the line built here and nothing of this command:
     *  `period` and `stop` are parameters of its watch-yuno-stats too.
     */
    json_object_set_new(kw, "agent_id", json_string(node));
    json_object_set_new(kw, "cmd2agent",
        json_stringn(gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf)));
    GBUFFER_DECREF(gbuf)
    const char *own_params[] = {"scenario_id", "node", "keys", "period", "stop", "agent_service", 0};
    for(int i=0; own_params[i]; i++) {
        json_object_del(kw, own_params[i]);
    }

    priv->rxMsgs++;
    return forward_to_agent(gobj, kw, src);
}

/***************************************************************************
 *  What the user who asks may do here: every permission of this service,
 *  true or false; a scenario permission that a role grants only for some
 *  scenarios (C_AUTHZ `parameters`) is the list of those scenarios. A
 *  console shows what it can use, instead of controls that are refused.
 ***************************************************************************/
PRIVATE json_t *cmd_my_permissions(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_data = json_object();
    json_t *scenarios = NULL;
    for(const sdata_desc_t *it = authz_table; it->name; it++) {
        if(user_may_scenario(gobj, it->name, NULL, kw, src)) {
            json_object_set_new(jn_data, it->name, json_true());
            continue;
        }
        if(strstr(it->name, "-scenarios") && priv->gobj_treedb_controlcenter) {
            if(!scenarios) {
                scenarios = gobj_list_nodes(
                    priv->gobj_treedb_controlcenter, "scenarios", 0, 0, gobj
                );
            }
            json_t *ids = json_array();
            int idx; json_t *scenario;
            json_array_foreach(scenarios, idx, scenario) {
                const char *id = kw_get_str(gobj, scenario, "id", "", 0);
                if(user_may_scenario(gobj, it->name, id, kw, src)) {
                    json_array_append_new(ids, json_string(id));
                }
            }
            if(json_array_size(ids)) {
                json_object_set_new(jn_data, it->name, ids);
                continue;
            }
            JSON_DECREF(ids)
        }
        json_object_set_new(jn_data, it->name, json_false());
    }
    JSON_DECREF(scenarios)

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_data,
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
PRIVATE BOOL requester_listens(hgobj requester)
{
    if(gobj_has_attr(requester, "opened")) {
        return gobj_read_bool_attr(requester, "opened");
    }
    return gobj_in_this_state(requester, ST_SESSION);
}

PRIVATE BOOL requester_is_listening(hgobj gobj, hgobj requester, gobj_event_t event)
{
    BOOL listening = requester_listens(requester);
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
 *  the permission on the scenario it acts on, or the treedb is not open.
 *  NULL when it can (kw untouched). A NULL permission checks only the
 *  treedb: the caller checks the permission per scenario.
 ***************************************************************************/
PRIVATE json_t *refuse_scenario_command(
    hgobj gobj,
    const char *permission,
    const char *scenario_id,
    json_t *kw,
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(permission && !user_may_scenario(gobj, permission, scenario_id, kw, src)) {
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
            "permission",   "%s", permission? permission : "",
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

/***************************************************************************
 *  Has the user `permission` on THIS scenario? A role may grant a
 *  permission for some scenarios only (C_AUTHZ `parameters`, e.g.
 *  {"scenario_id": ["load-20k"]}): the check carries the id the
 *  command acts on and who asks (the user C_IEVENT_SRV authenticated), and
 *  NOTHING of the client's kw -- a key of it could answer for a scope. A
 *  NULL id asks the permission with no scenario: only an unscoped grant
 *  has it.
 ***************************************************************************/
PRIVATE BOOL user_may_scenario(
    hgobj gobj,
    const char *permission,
    const char *scenario_id,
    json_t *kw, // not owned
    hgobj src
)
{
    json_t *kw_check = json_object();
    if(scenario_id) {
        json_object_set_new(kw_check, "scenario_id", json_string(scenario_id));
    }
    const char *username = username_of(gobj, kw, src);
    if(!empty_string(username)) {
        json_object_set_new(kw_check, "__username__", json_string(username));
    }
    return gobj_user_has_authz(gobj, permission, kw_check, src);
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
PRIVATE const char *switch_actions[] = {"on", "off", 0};

/***************************************************************************
 *  Is `name` one of the items of "a,b,c"?
 ***************************************************************************/
PRIVATE BOOL in_comma_list(const char *list, const char *name)
{
    size_t ln = strlen(name);
    const char *p = list;
    while(p && *p) {
        const char *comma = strchr(p, ',');
        size_t lt = comma? (size_t)(comma - p) : strlen(p);
        if(lt == ln && strncmp(p, name, ln)==0) {
            return TRUE;
        }
        p = comma? comma + 1 : NULL;
    }
    return FALSE;
}

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

PRIVATE BOOL is_switch_action(const char *action)
{
    if(empty_string(action)) {
        return FALSE;
    }
    for(int i=0; switch_actions[i]; i++) {
        if(strcmp(switch_actions[i], action)==0) {
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
        snprintf(bad, badsz, "a command name first: '%.*s'", utf8_prefix_len(name, 40), name);
        ret = -1;
    }
    for(const char *p = name; ret == 0 && *p; p++) {
        if(!isalnum((unsigned char)*p) && *p != '_' && *p != '-') {
            snprintf(bad, badsz, "a command name first: '%.*s'", utf8_prefix_len(name, 40), name);
            ret = -1;
        }
    }
    for(int i=1; ret == 0 && i<n; i++) {
        const char *eq = strchr(words[i], '=');
        size_t klen = eq? (size_t)(eq - words[i]) : 0;
        if(!eq || klen == 0 || klen >= NAME_MAX) {
            snprintf(bad, badsz, "key=value parameters, without blanks: '%.*s'",
                utf8_prefix_len(words[i], 40), words[i]);
            ret = -1;
            break;
        }
        char key[NAME_MAX];
        snprintf(key, sizeof(key), "%.*s", (int)klen, words[i]);
        if(!is_plain_name(key)) {
            snprintf(bad, badsz, "a parameter name of letters, digits and _ . ^ -: '%.*s'",
                utf8_prefix_len(key, 40), key);
            ret = -1;
            break;
        }
        if(strncmp(key, "__", 2)==0) {
            snprintf(bad, badsz,
                "the parameter '%.*s' is a framework key: it would replace what the control center sets",
                utf8_prefix_len(key, 40), key);
            ret = -1;
            break;
        }
        for(int j=0; reserved_step_params[j]; j++) {
            if(strcmp(key, reserved_step_params[j])==0) {
                snprintf(bad, badsz,
                    "the parameter '%.*s' is command-yuno's or a yuno's field: it would select the yuno",
                    utf8_prefix_len(key, 40), key);
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

/*
 *  The steps of an action or of a switch: each a yuno (a key of `keys`;
 *  `own_yuno`, the switch's, when it names none), a command and an
 *  optional service. `where` names the list in the error.
 */
PRIVATE int check_steps(
    json_t *steps,
    const char *where,
    json_t *keys,
    const char *own_yuno,
    char *err,
    size_t errsz
)
{
    if(!json_is_array(steps)) {
        return scenario_error(err, errsz, "%s: a list of steps", where);
    }
    size_t idx; json_t *step;
    json_array_foreach(steps, idx, step) {
        const char *step_yuno = json_is_object(step)? str_member(step, "yuno") : NULL;
        if(own_yuno && json_is_object(step) && !json_object_get(step, "yuno")) {
            step_yuno = own_yuno;
        }
        const char *command = json_is_object(step)? str_member(step, "command") : NULL;
        if(!json_is_object(step) || !step_yuno || !json_object_get(keys, step_yuno)) {
            return scenario_error(err, errsz, "%s[%d]: yuno: a key of its yunos", where, (int)idx);
        }
        char bad[NAME_MAX];
        if(!command || check_step_command(command, bad, sizeof(bad)) < 0) {
            return scenario_error(err, errsz, "%s[%d]: command: %s",
                where, (int)idx, command? bad : "must be a string");
        }
        const char *step_service = str_member(step, "service");
        if(!step_service || (!empty_string(step_service) && !is_plain_name(step_service))) {
            return scenario_error(err, errsz, "%s[%d]: service: letters, digits and _ . ^ -",
                where, (int)idx);
        }
    }
    return 0;
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
        const char *yuno_cols[] = {"node", "service", "label", "rate", "queue_service", 0};
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
        /*  The service whose stats say the yuno's queue, when it is not
         *  `service` (a gate's queue is its __output_side__'s)  */
        const char *queue_service = str_member(yuno, "queue_service");
        if(!empty_string(queue_service) && !is_plain_name(queue_service)) {
            JSON_DECREF(keys)
            return scenario_error(err, errsz, "yunos[%d]: queue_service: letters, digits and _ . ^ -",
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
        char where[NAME_MAX];
        snprintf(where, sizeof(where), "actions.%s", action);
        if(check_steps(steps, where, keys, NULL, err, errsz) < 0) {
            JSON_DECREF(keys)
            return -1;
        }
    }

    /*  The switch of a yuno: its on and off, both, never empty  */
    json_array_foreach(yunos, idx, yuno) {
        json_t *jn_switch = json_object_get(yuno, "switch");
        if(!jn_switch) {
            continue;
        }
        const char *key = yuno_key(yuno);
        if(!json_is_object(jn_switch) || json_object_size(jn_switch) != 2 ||
                json_array_size(json_object_get(jn_switch, "on"))==0 ||
                json_array_size(json_object_get(jn_switch, "off"))==0) {
            JSON_DECREF(keys)
            return scenario_error(err, errsz, "yunos[%d]: switch: {on: [steps], off: [steps]}", (int)idx);
        }
        for(int i=0; switch_actions[i]; i++) {
            char where[NAME_MAX];
            snprintf(where, sizeof(where), "yunos[%d].switch.%s", (int)idx, switch_actions[i]);
            if(check_steps(json_object_get(jn_switch, switch_actions[i]), where, keys, key,
                    err, errsz) < 0) {
                JSON_DECREF(keys)
                return -1;
            }
        }
    }
    JSON_DECREF(keys)

    json_t *view = json_object_get(scenario, "view");
    if(view && !json_is_object(view)) {
        return scenario_error(err, errsz, "view: a dict");
    }

    /*  Seconds a start may run before the control center stops it  */
    json_t *max_run = json_object_get(scenario, "max_run");
    if(max_run && (!json_is_integer(max_run) || json_integer_value(max_run) < 0 ||
            json_integer_value(max_run) > MAX_RUN_LIMIT)) {
        return scenario_error(err, errsz, "max_run: seconds, 0 (none) to %d", MAX_RUN_LIMIT);
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
PRIVATE json_t *build_run_steps(
    hgobj gobj,
    json_t *scenario,
    const char *action,
    const char *scenario_yuno,
    char *err,
    size_t errsz
)
{
    json_t *defs = NULL;
    if(is_switch_action(action)) {
        json_t *own = find_scenario_yuno(scenario, scenario_yuno? scenario_yuno : "");
        if(!own) {
            scenario_error(err, errsz, "no yuno '%s' in the scenario", scenario_yuno? scenario_yuno : "");
            return NULL;
        }
        defs = json_object_get(json_object_get(own, "switch"), action);
        if(!json_is_array(defs) || json_array_size(defs)==0) {
            scenario_error(err, errsz, "yuno '%s' has no switch", scenario_yuno);
            return NULL;
        }
    } else {
        defs = json_object_get(json_object_get(scenario, "actions"), action);
        if(!json_is_array(defs) || json_array_size(defs)==0) {
            scenario_error(err, errsz, "the scenario declares no step for '%s'", action);
            return NULL;
        }
        scenario_yuno = NULL;
    }
    const char *default_node = str_member(scenario, "node");

    json_t *steps = json_array();
    size_t idx; json_t *def;
    json_array_foreach(defs, idx, def) {
        const char *key = str_member(def, "yuno");
        if(scenario_yuno && json_is_object(def) && !json_object_get(def, "yuno")) {
            key = scenario_yuno;
        }
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
    json_object_clear(priv->run_channels);
    char key[32];
    snprintf(key, sizeof(key), "%d", idx);
    json_object_set_new(priv->run_channels, key,
        json_integer((json_int_t)(size_t)channel_of_side(priv->gobj_input_side, channel)));

    json_t *kw_step = json_pack("{s:s}",
        "__username__", kw_get_str(gobj, priv->run, "username", "", 0)
    );
    kw_set_subdict_value(gobj, kw_step, "__md_iev__", "cc_run",
        json_string(kw_get_str(gobj, priv->run, "id", "", 0)));
    kw_set_subdict_value(gobj, kw_step, "__md_iev__", "cc_step", json_integer(idx));

    priv->txMsgs++;
    json_t *webix = gobj_command(channel, line, kw_step, gobj);
    JSON_DECREF(webix)

    set_timeout(priv->run_timer, gobj_read_integer_attr(gobj, "run_step_timeout"));
    return 0;
}

/***************************************************************************
 *  A run in parallel (stop, pause, report): every step at once, under one
 *  deadline. A step whose node is not connected fails at once; the run ends
 *  when no step waits (run_parallel_step_done()).
 ***************************************************************************/
PRIVATE int run_send_all_steps(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Every step is registered before the first is sent: an answer that
     *  came at once must not find the run with nothing left to wait for
     */
    json_t *steps = kw_get_list(gobj, priv->run, "steps", 0, KW_REQUIRED);
    size_t idx; json_t *step;
    json_array_foreach(steps, idx, step) {
        const char *node = kw_get_str(gobj, step, "node", "", KW_REQUIRED);
        hgobj channel = find_agent_channel(gobj, node);
        if(!channel) {
            json_object_set_new(step, "result", json_integer(-1));
            json_object_set_new(step, "comment", json_sprintf("node '%s' is not connected", node));
            continue;
        }
        char key[32];
        snprintf(key, sizeof(key), "%d", (int)idx);
        json_object_set_new(priv->run_channels, key,
            json_integer((json_int_t)(size_t)channel_of_side(priv->gobj_input_side, channel)));
    }
    if(json_object_size(priv->run_channels) == 0) {
        return run_parallel_step_done(gobj);    // no node connected: nothing to wait for
    }
    set_timeout(priv->run_timer, gobj_read_integer_attr(gobj, "run_step_timeout"));

    json_t *run = json_incref(priv->run);   // an answer at once may end it under the loop
    json_array_foreach(steps, idx, step) {
        if(priv->run != run || !run_channel_of_step(gobj, (int)idx)) {
            if(priv->run != run) {
                break;
            }
            continue;
        }
        hgobj channel = find_agent_channel(gobj, kw_get_str(gobj, step, "node", "", KW_REQUIRED));
        if(!channel) {
            continue;   // registered just above: it cannot be gone in between
        }
        json_t *kw_step = json_pack("{s:s}",
            "__username__", kw_get_str(gobj, run, "username", "", 0)
        );
        kw_set_subdict_value(gobj, kw_step, "__md_iev__", "cc_run",
            json_string(kw_get_str(gobj, run, "id", "", 0)));
        kw_set_subdict_value(gobj, kw_step, "__md_iev__", "cc_step", json_integer((json_int_t)idx));

        priv->txMsgs++;
        json_t *webix = gobj_command(channel, kw_get_str(gobj, step, "line", "", KW_REQUIRED), kw_step, gobj);
        JSON_DECREF(webix)
    }
    JSON_DECREF(run)
    return 0;
}

/***************************************************************************
 *  A step of a run in parallel is over (answered, failed, not answered):
 *  the end of the run when none waits. -1 when any step failed, its
 *  comment naming each one.
 ***************************************************************************/
PRIVATE int run_parallel_step_done(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(json_object_size(priv->run_channels) > 0) {
        return 0;
    }
    json_t *steps = kw_get_list(gobj, priv->run, "steps", 0, KW_REQUIRED);
    int failed = 0;
    size_t idx; json_t *step;
    json_array_foreach(steps, idx, step) {
        if(kw_get_int(gobj, step, "result", -1, KW_WILD_NUMBER) < 0) {
            failed++;
        }
    }
    if(failed == 0) {
        return run_end(gobj, 0, "");
    }

    gbuffer_t *gbuf = gbuffer_create(256, 64 * 1024);
    if(!gbuf) {
        // Error already logged
        return run_end(gobj, -1, "steps failed");
    }
    gbuffer_printf(gbuf, "%d of %d steps failed", failed, (int)json_array_size(steps));
    json_array_foreach(steps, idx, step) {
        if(kw_get_int(gobj, step, "result", -1, KW_WILD_NUMBER) < 0) {
            gbuffer_printf(gbuf, "; step %d (%s): %s",
                (int)idx,
                kw_get_str(gobj, step, "line", "", 0),
                kw_get_str(gobj, step, "comment", "", 0)
            );
        }
    }
    gbuffer_append_char(gbuf, 0);
    int ret = run_end(gobj, -1, (const char *)gbuffer_cur_rd_pointer(gbuf));
    GBUFFER_DECREF(gbuf)
    return ret;
}

/***************************************************************************
 *  The channel a step in flight was sent through, 0 when it waits for none
 ***************************************************************************/
PRIVATE hgobj run_channel_of_step(hgobj gobj, int idx)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char key[32];
    snprintf(key, sizeof(key), "%d", idx);
    return (hgobj)(size_t)json_integer_value(json_object_get(priv->run_channels, key));
}

/***************************************************************************
 *  Does a step in flight wait for an answer through `channel`?
 ***************************************************************************/
PRIVATE BOOL run_uses_channel(hgobj gobj, hgobj channel)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *key; json_t *jn_channel;
    json_object_foreach(priv->run_channels, key, jn_channel) {
        if((hgobj)(size_t)json_integer_value(jn_channel) == channel) {
            return TRUE;
        }
    }
    return FALSE;
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

    BOOL parallel = priv->run? kw_get_bool(gobj, priv->run, "parallel", 0, 0) : FALSE;
    if(!priv->run ||
            strcmp(run_id, kw_get_str(gobj, priv->run, "id", "", 0))!=0 ||
            (parallel? !run_channel_of_step(gobj, step_idx) :
                step_idx != (int)kw_get_int(gobj, priv->run, "idx", 0, 0))) {
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
    if(!parallel) {
        clear_timeout(priv->run_timer);
    }

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

    if(parallel) {
        char key[32];
        snprintf(key, sizeof(key), "%d", step_idx);
        json_object_del(priv->run_channels, key);
        return run_parallel_step_done(gobj);
    }
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
    if(kw_get_bool(gobj, priv->run, "parallel", 0, 0)) {
        /*
         *  Every step still waiting is not answered; the run ends with them
         */
        json_t *steps = kw_get_list(gobj, priv->run, "steps", 0, KW_REQUIRED);
        const char *key; json_t *jn_channel; void *n;
        json_object_foreach_safe(priv->run_channels, n, key, jn_channel) {
            json_t *step = json_array_get(steps, (size_t)atoi(key));
            json_object_set_new(step, "result", json_integer(-1));
            json_object_set_new(step, "comment", json_sprintf("not answered in %d ms",
                (int)gobj_read_integer_attr(gobj, "run_step_timeout")));
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", "a step of a scenario run was not answered",
                "run",          "%s", kw_get_str(gobj, priv->run, "id", "", 0),
                "step",         "%d", atoi(key),
                "line",         "%s", kw_get_str(gobj, step, "line", "", 0),
                NULL
            );
            json_object_del(priv->run_channels, key);
        }
        return run_parallel_step_done(gobj);
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
PRIVATE int run_agent_disconnected(hgobj gobj, hgobj channel)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->run) {
        return 0;
    }
    if(kw_get_bool(gobj, priv->run, "parallel", 0, 0)) {
        /*
         *  Only the steps of that agent fail: the others are still answered
         */
        json_t *steps = kw_get_list(gobj, priv->run, "steps", 0, KW_REQUIRED);
        const char *key; json_t *jn_channel; void *n;
        json_object_foreach_safe(priv->run_channels, n, key, jn_channel) {
            if((hgobj)(size_t)json_integer_value(jn_channel) != channel) {
                continue;
            }
            json_t *step = json_array_get(steps, (size_t)atoi(key));
            json_object_set_new(step, "result", json_integer(-1));
            json_object_set_new(step, "comment", json_sprintf("the agent of '%s' disconnected",
                kw_get_str(gobj, step, "node", "", 0)));
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", "the agent of a step of a scenario run disconnected",
                "run",          "%s", kw_get_str(gobj, priv->run, "id", "", 0),
                "step",         "%d", atoi(key),
                "node",         "%s", kw_get_str(gobj, step, "node", "", 0),
                NULL
            );
            json_object_del(priv->run_channels, key);
        }
        return run_parallel_step_done(gobj);
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
 *  Which connection of a web client asks (kw not owned, the request to an
 *  agent): what the agent sends back later, an answer or a stream, reaches
 *  its channel only while it is still that connection.
 ***************************************************************************/
PRIVATE void stamp_client_connection(hgobj gobj, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj client_channel = channel_of_side(priv->gobj_top_side, src);
    if(!client_channel) {
        return;     // not a web client: a local service, found by its name
    }
    json_t *jn_client = json_array_get(
        kw_get_list(gobj, kw, "__md_iev__`ievent_gate_stack", 0, 0), 0
    );
    if(json_is_object(jn_client)) {
        json_object_set_new(jn_client, "cc_connection",
            json_integer(connection_number(client_channel)));
    }
}

/***************************************************************************
 *  Does the client of a request (kw not owned) say it takes `event`, in
 *  its `__relays__` (a list naming it)?
 ***************************************************************************/
PRIVATE BOOL client_relays(json_t *kw, gobj_event_t event)
{
    json_t *jn_relays = json_object_get(kw, "__relays__");
    size_t idx; json_t *jn_relay;
    json_array_foreach(jn_relays, idx, jn_relay) {
        if(json_is_string(jn_relay) && strcmp(json_string_value(jn_relay), event)==0) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  A request that came in by one of this yuno's own ievent links (a
 *  C_IEVENT_CLI service, its agent's `agent_client`) goes to the agent of
 *  `agent_srv`: the agent's channel remembers that link, the only one whose
 *  name it may route an answer back to (requester_of_route()). Forgotten
 *  when the agent's channel closes.
 ***************************************************************************/
PRIVATE void record_link_request(hgobj gobj, hgobj src, hgobj agent_srv)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!src || !gobj_is_service(src) || !gobj_typeof_gclass(src, C_IEVENT_CLI)) {
        return;
    }
    hgobj agent_channel = channel_of_side(priv->gobj_input_side, agent_srv);
    if(!agent_channel) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "agent not under __input_side__, the link's answers will be dropped",
            "link",         "%s", gobj_name(src),
            NULL
        );
        return;
    }
    json_t *links = gobj_read_user_data(agent_channel, "link_requesters");
    if(!json_is_object(links)) {
        links = json_object();
        gobj_write_user_data(agent_channel, "link_requesters", links);
    }
    json_object_set_new(links, gobj_name(src), json_true());
}

/***************************************************************************
 *  Who gets what an agent sends back along the route of a request (kw not
 *  owned): pops this control center's hop off the stack, whose
 *  `dst_service` is who asked -- the channel of __top_side__ of a web
 *  client, or, for a request that came in by one of this yuno's own
 *  ievent links (its agent's `agent_client`: a C_IEVENT_CLI dispatches a
 *  command as its own src), that C_IEVENT_CLI service -- and only if a
 *  request of that link went to the agent this comes from
 *  (record_link_request()). Its name is copied in `dst_service` (the
 *  popped frame is freed here). Nothing else is a requester: the hops
 *  below were written by the client, and a name the agent echoed that
 *  names another local service, or a link that never asked it, is not
 *  believed.
 *  NULL when there is nobody; NULL and `reconnected` when the channel is
 *  held by another connection now: its name was taken by the next client.
 *  A closed channel is returned, to be found not listening.
 ***************************************************************************/
PRIVATE hgobj requester_of_route(
    hgobj gobj,
    json_t *kw,
    char *dst_service,
    size_t dst_service_size,
    BOOL *reconnected
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    *reconnected = FALSE;
    json_t *jn_ievent_id = msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID);
    snprintf(dst_service, dst_service_size, "%s",
        kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0));
    JSON_DECREF(jn_ievent_id);

    hgobj gobj_requester = gobj_child_by_name(priv->gobj_top_side, dst_service);
    if(gobj_requester) {
        if(!same_connection(gobj, kw, gobj_requester) &&
                connection_number(gobj_requester) != 0) {
            *reconnected = TRUE;
            return NULL;
        }
        return gobj_requester;
    }

    hgobj gobj_link = gobj_find_service(dst_service, FALSE);
    if(!gobj_link || !gobj_typeof_gclass(gobj_link, C_IEVENT_CLI)) {
        return NULL;
    }
    hgobj agent_channel = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, 0);
    json_t *links = agent_channel? gobj_read_user_data(agent_channel, "link_requesters") : NULL;
    if(!json_is_true(json_object_get(links, dst_service))) {
        return NULL;
    }
    return gobj_link;
}

/***************************************************************************
 *  The requester of an answer (kw not owned), as requester_of_route()
 *  finds it; NULL, logged, when there is none, or when its channel is
 *  held by another connection now.
 ***************************************************************************/
PRIVATE hgobj requester_of_answer(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,
    char *dst_service,
    size_t dst_service_size
)
{
    BOOL reconnected;
    hgobj gobj_requester = requester_of_route(
        gobj, kw, dst_service, dst_service_size, &reconnected
    );
    if(reconnected) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "answer for a web client that is gone, dropped: its channel holds another connection now",
            "service",      "%s", dst_service,
            "event",        "%s", event,
            NULL
        );
        return NULL;
    }
    if(!gobj_requester) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "answer of an agent for no requester of this control center, dropped",
            "service",      "%s", dst_service,
            "event",        "%s", event,
            NULL
        );
    }
    return gobj_requester;
}

/***************************************************************************
 *  What an agent sends back (an answer, a stream) is public for the
 *  agents' sake, so a web client can send it too: it would be relayed by
 *  the routing IT wrote, to another client. Taken only from __input_side__.
 *  An authenticated client can send them as fast as it likes: the one that
 *  opens a window (drops_warning_window) is said, with the ones counted since, the others counted.
 ***************************************************************************/
PRIVATE BOOL is_from_agent_side(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        priv->rxMsgs++;
        return TRUE;
    }
    priv->injected++;
    if(drops_window_ended(gobj, priv->t_injected_log)) {
        hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, 0);
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "event of an agent not from the agents' side, dropped",
            "event",        "%s", event,
            "src",          "%s", gobj_short_name(src),
            "channel",      "%s", channel_gobj? gobj_short_name(channel_gobj) : "",
            "dropped",      "%lu", (unsigned long)priv->injected,
            NULL
        );
        priv->injected = 0;
        priv->t_injected_log = start_msectimer(0);
    }
    arm_drops_timer(gobj);
    return FALSE;
}

/***************************************************************************
 *  Send `event` (kw owned) back to its requester, if it is listening
 ***************************************************************************/
PRIVATE int relay_to_requester(hgobj gobj, gobj_event_t event, json_t *kw, hgobj requester)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!requester_is_listening(gobj, requester, event)) {
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

    priv->txMsgs++;
    return gobj_send_event(
        requester,
        EV_SEND_IEV,
        iev,
        gobj
    );
}

/***************************************************************************
 *  A frame of a stream (a PTY, a watch) for a web client that is gone.
 *  Expected -- the agent learns it only when the stream ends -- and many:
 *  counted, and said once a window (drops_warning_window), not once per frame.
 ***************************************************************************/
PRIVATE void count_dropped_stream(
    hgobj gobj,
    gobj_event_t event,
    const char *dst_service,
    BOOL reconnected,
    uint64_t *dropped,
    uint64_t *t_next_log
)
{
    (*dropped)++;
    if(drops_window_ended(gobj, *t_next_log)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", event == EV_YUNO_STATS?
                "yuno stats for a web client that is gone, dropped (the agent's watch expires)" :
                "stream for a web client that is gone, dropped",
            "event",        "%s", event,
            "service",      "%s", dst_service,
            "reconnected",  "%s", reconnected? "its channel holds another connection now" : "",
            "dropped",      "%lu", (unsigned long)*dropped,
            NULL
        );
        *dropped = 0;
        *t_next_log = start_msectimer(0);
    }
    arm_drops_timer(gobj);
}

/***************************************************************************
 *  The rate tick (re)starts: a periodic timer of `timeout` ms, the rates
 *  measured from now. A deliberate timer: the rates are SAMPLED each
 *  period over the exact interval, not computed by whoever reads them (two
 *  readers would share one window), and the maxima catch a burst nobody
 *  read. It re-issues no query: it is not polling.
 *
 *  A C_TIMER0 (io_uring, a real period), not a C_TIMER: a C_TIMER checks
 *  its deadline on the yuno's periodic grain (1000 ms) and re-arms from
 *  when it was served, so a tick of 1000 came every 1 or 2 s, and a burst
 *  of 1 s in a tick of 2 s was halved in the maxima (up to 7.25.21).
 ***************************************************************************/
PRIVATE void start_rates_tick(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->t_rates = start_msectimer(0);
    priv->last_txMsgs = priv->txMsgs;
    priv->last_rxMsgs = priv->rxMsgs;
    if(!gobj_is_running(priv->rates_timer)) {
        gobj_start(priv->rates_timer);
    }
    set_timeout_periodic0(priv->rates_timer, priv->timeout);
}

/***************************************************************************
 *  A rate tick: messages by second since the previous one, over the exact
 *  milliseconds between them, and the maxima written into their attrs
 *  when passed.
 ***************************************************************************/
PRIVATE void update_rates(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint64_t now = start_msectimer(0);
    uint64_t elapsed = priv->t_rates? now - priv->t_rates : 0;
    if(elapsed > 0) {
        priv->txMsgsec = (json_int_t)((uint64_t)(priv->txMsgs - priv->last_txMsgs)*1000/elapsed);
        priv->rxMsgsec = (json_int_t)((uint64_t)(priv->rxMsgs - priv->last_rxMsgs)*1000/elapsed);
    }
    priv->t_rates = now;
    priv->last_txMsgs = priv->txMsgs;
    priv->last_rxMsgs = priv->rxMsgs;

    if(priv->txMsgsec > gobj_read_integer_attr(gobj, "maxtxMsgsec")) {
        gobj_write_integer_attr(gobj, "maxtxMsgsec", priv->txMsgsec);
    }
    if(priv->rxMsgsec > gobj_read_integer_attr(gobj, "maxrxMsgsec")) {
        gobj_write_integer_attr(gobj, "maxrxMsgsec", priv->rxMsgsec);
    }
}

/***************************************************************************
 *  The capped warnings: what each counted since it last spoke, and when its
 *  window started.
 ***************************************************************************/
typedef struct {
    uint64_t *count;
    uint64_t *t_window;
    const char *msgset;
    const char *msg;
} drop_cap_t;

#define DROP_CAPS 4

PRIVATE void drop_caps(PRIVATE_DATA *priv, drop_cap_t caps[DROP_CAPS])
{
    caps[0] = (drop_cap_t){&priv->stats_dropped, &priv->t_stats_dropped_log, MSGSET_INFO,
        "yuno stats for a web client that is gone, dropped (the agent's watch expires)"};
    caps[1] = (drop_cap_t){&priv->tty_dropped, &priv->t_tty_dropped_log, MSGSET_INFO,
        "stream for a web client that is gone, dropped"};
    caps[2] = (drop_cap_t){&priv->tty_unrouted, &priv->t_tty_unrouted_log, MSGSET_PROTOCOL,
        "PTY output of an agent for no requester of this control center, dropped"};
    caps[3] = (drop_cap_t){&priv->injected, &priv->t_injected_log, MSGSET_PROTOCOL,
        "event of an agent not from the agents' side, dropped"};
}

/***************************************************************************
 *  A capped warning says what it counted when it speaks NEXT, and there may
 *  be no next one: what is still counted is said when its window
 *  (drops_warning_window) ends (`only_ended`, the drops timer) or when the
 *  control center stops. Never on a connection's close: a client that
 *  connects, sends one, and leaves, in a loop, would be said once per loop.
 ***************************************************************************/
PRIVATE void say_pending_drops(hgobj gobj, const char *when, BOOL only_ended)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    drop_cap_t caps[DROP_CAPS];
    drop_caps(priv, caps);
    for(int i=0; i<DROP_CAPS; i++) {
        if(*caps[i].count == 0) {
            continue;
        }
        if(only_ended && !drops_window_ended(gobj, *caps[i].t_window)) {
            continue;
        }
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", caps[i].msgset,
            "msg",          "%s", caps[i].msg,
            "when",         "%s", when,
            "dropped",      "%lu", (unsigned long)*caps[i].count,
            NULL
        );
        *caps[i].count = 0;
        *caps[i].t_window = start_msectimer(0);
    }
}

/***************************************************************************
 *  Has the window that started at `window_start` (0: none) ended?
 ***************************************************************************/
PRIVATE BOOL drops_window_ended(hgobj gobj, uint64_t window_start)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!window_start) {
        return TRUE;
    }
    return test_msectimer(window_start + (uint64_t)priv->drops_warning_window);
}

/***************************************************************************
 *  A drops_warning_window under 1 ms would say every drop: refused, the
 *  default put back. Returns 0 if it was good, -1 (logged) if not.
 ***************************************************************************/
PRIVATE int check_drops_warning_window(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->drops_warning_window >= 1) {
        return 0;
    }
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER,
        "msg",          "%s", "drops_warning_window must be 1 ms or more, 60000 put back",
        "drops_warning_window", "%lld", (long long)priv->drops_warning_window,
        NULL
    );
    gobj_write_integer_attr(gobj, "drops_warning_window", 60000);  // mt_writing() takes it
    return -1;
}

/***************************************************************************
 *  The drops timer fires when the first window with a count ends (a real
 *  time, not a deferral): armed while something is counted and not said,
 *  moved when that end moves, cleared when nothing is counted.
 ***************************************************************************/
PRIVATE void arm_drops_timer(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    drop_cap_t caps[DROP_CAPS];
    drop_caps(priv, caps);
    uint64_t now = start_msectimer(0);
    uint64_t first_end = 0;
    for(int i=0; i<DROP_CAPS; i++) {
        if(*caps[i].count == 0) {
            continue;
        }
        uint64_t end = *caps[i].t_window?
            *caps[i].t_window + (uint64_t)priv->drops_warning_window : now;
        if(end <= now) {
            end = now + 1;
        }
        if(!first_end || end < first_end) {
            first_end = end;
        }
    }
    if(!first_end) {
        if(priv->drops_timer_armed) {
            clear_timeout(priv->drops_timer);
            priv->drops_timer_armed = FALSE;
            gobj_write_bool_attr(gobj, "drops_timer_armed", FALSE);
        }
        return;
    }
    if(priv->drops_timer_armed && priv->drops_timer_end == first_end) {
        return;
    }
    set_timeout(priv->drops_timer, (json_int_t)(first_end - now));
    priv->drops_timer_armed = TRUE;
    priv->drops_timer_end = first_end;
    gobj_write_bool_attr(gobj, "drops_timer_armed", TRUE);
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
    if(priv->run_channels) {
        json_object_clear(priv->run_channels);
    }
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
    const char *reason = kw_get_str(gobj, run, "reason", "", 0);
    const char *scenario_yuno = kw_get_str(gobj, run, "scenario_yuno", "", 0);
    json_t *jn_text = result < 0?
        json_sprintf("%s%srun %s: %s%s%s failed, %d of %d steps done: %s",
            reason, empty_string(reason)? "" : "; ",
            kw_get_str(gobj, run, "id", "", 0), kw_get_str(gobj, run, "action", "", 0),
            empty_string(scenario_yuno)? "" : " ", scenario_yuno,
            done, (int)json_array_size(steps), comment) :
        json_sprintf("%s%srun %s: %s%s%s done, %d steps",
            reason, empty_string(reason)? "" : "; ",
            kw_get_str(gobj, run, "id", "", 0), kw_get_str(gobj, run, "action", "", 0),
            empty_string(scenario_yuno)? "" : " ", scenario_yuno,
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
        priv->txMsgs++;
        gobj_send_event(
            gobj_requester,
            EV_SEND_IEV,
            iev_create(gobj, EV_MT_COMMAND_ANSWER, webix),
            gobj
        );
    }

    /*
     *  What the run did to the stop a max_run schedules: a start sets it,
     *  a stop cancels it. A stop of the max_run that did no step is tried
     *  again: in a minute when a node of it is connected (its yuno refused
     *  or did not answer, restarting perhaps); else when an agent of one of
     *  its nodes connects (retry_stops_waiting_node()), and not every
     *  minute, writing a run (and a warning) each time, for as long as the
     *  node is down. 7.29.2 made every one wait for an agent: one whose
     *  node was connected was never tried again.
     */
    if(priv->gobj_treedb_controlcenter) {
        schedule_from_runs(gobj, scenario_id);
        json_t *pending = json_object_get(priv->auto_stops, scenario_id);
        if(!empty_string(reason) && done == 0 && pending) {
            BOOL reaches = steps_reach_a_node(gobj, steps);
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", reaches?
                    "the stop of a scenario's max_run did nothing, tried again in a minute" :
                    "the stop of a scenario's max_run did nothing, tried again when a node of it connects",
                "scenario_id",  "%s", scenario_id,
                "comment",      "%s", comment? comment : "",
                NULL
            );
            if(reaches) {
                json_object_del(pending, "waits_node");
                json_object_set_new(pending, "stop_at", json_integer((json_int_t)time_in_seconds() + 60));
            } else {
                json_object_set_new(pending, "waits_node", json_true());
            }
        }
        arm_stop_timer(gobj);
    }

    /*  Whoever watches a scenario reads its state again  */
    gobj_publish_event(gobj, EV_SCENARIO_RUN_ENDED, json_object());

    JSON_DECREF(run)
    return 0;
}

/***************************************************************************
 *  How many steps of a run did their work: a step not sent has no result.
 ***************************************************************************/
PRIVATE int run_steps_done(hgobj gobj, json_t *run)
{
    int done = 0;
    size_t idx; json_t *step;
    json_array_foreach(kw_get_list(gobj, run, "steps", 0, 0), idx, step) {
        if(kw_has_key(step, "result") && kw_get_int(gobj, step, "result", -1, KW_WILD_NUMBER) >= 0) {
            done++;
        }
    }
    return done;
}

/***************************************************************************
 *  The stop a scenario's max_run schedules, from its runs: none when it
 *  has no max_run or its last run that did anything (a report changes
 *  nothing) is a stop; else the end of its last start, plus max_run.
 ***************************************************************************/
PRIVATE void schedule_from_runs(hgobj gobj, const char *scenario_id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *old = json_object_get(priv->auto_stops, scenario_id);
    BOOL waited = old && json_is_true(json_object_get(old, "waits_node"));
    json_int_t old_stop_at = old? kw_get_int(gobj, old, "stop_at", 0, KW_REQUIRED) : 0;
    json_int_t old_scheduled_at = old? kw_get_int(gobj, old, "scheduled_at", 0, KW_REQUIRED) : 0;
    json_object_del(priv->auto_stops, scenario_id);
    json_t *scenario = gobj_get_node(
        priv->gobj_treedb_controlcenter,
        "scenarios",
        json_pack("{s:s}", "id", scenario_id),
        0,
        gobj
    );
    json_int_t max_run = kw_get_int(gobj, scenario, "max_run", 0, KW_WILD_NUMBER);
    JSON_DECREF(scenario)
    if(max_run <= 0) {
        return;
    }

    json_t *runs = sort_runs_newest_first(gobj_node_children(
        priv->gobj_treedb_controlcenter,
        "scenarios",
        json_pack("{s:s}", "id", scenario_id),
        "runs",
        0,
        json_pack("{s:b}", "fkey_only_id", 1),
        gobj
    ));
    BOOL running = FALSE;
    size_t idx; json_t *run;
    json_array_foreach(runs, idx, run) {
        const char *action = kw_get_str(gobj, run, "action", "", 0);
        if(!run_changes_state(gobj, run)) {
            continue;
        }
        if(!running) {
            if(strcmp(action, "stop")==0) {
                break;
            }
            running = TRUE;
        }
        if(strcmp(action, "start")==0) {
            json_int_t scheduled_at = kw_get_int(gobj, run, "ended_at", 0, KW_WILD_NUMBER) + max_run;
            json_object_set_new(priv->auto_stops, scenario_id, json_pack("{s:I, s:I, s:s}",
                "stop_at", scheduled_at,
                "scheduled_at", scheduled_at,
                "started_by", kw_get_str(gobj, run, "username", "", 0)
            ));
            break;
        }
    }
    JSON_DECREF(runs)

    /*
     *  The same stop (same start, same max_run) keeps what its failures
     *  made of it: rebuilt bare it was due at once, and wrote one more
     *  failed run. One that waited for a node goes on waiting, unless a node
     *  of it is connected now (a save may have moved its steps) (7.29.2);
     *  one tried again in a minute keeps its minute (7.29.3).
     */
    json_t *stop = json_object_get(priv->auto_stops, scenario_id);
    if(old && stop && kw_get_int(gobj, stop, "scheduled_at", 0, KW_REQUIRED) == old_scheduled_at) {
        if(waited) {
            if(!stop_reaches_a_node(gobj, scenario_id)) {
                json_object_set_new(stop, "waits_node", json_true());
            }
        } else if(old_stop_at > old_scheduled_at) {
            json_object_set_new(stop, "stop_at", json_integer(old_stop_at));
        }
    }
}

/***************************************************************************
 *  Does a run change the state of its scenario? Not a report, not a
 *  switch (on/off of one yuno), not one that did no step.
 ***************************************************************************/
PRIVATE BOOL run_changes_state(hgobj gobj, json_t *run)
{
    const char *action = kw_get_str(gobj, run, "action", "", 0);
    if(strcmp(action, "report")==0 || is_switch_action(action)) {
        return FALSE;
    }
    return run_steps_done(gobj, run) > 0;
}

/***************************************************************************
 *  The action of the last run of a scenario that changed its state, in
 *  `bf` ("" when there is none: never started, or a new scenario).
 ***************************************************************************/
PRIVATE void last_state_action(hgobj gobj, const char *scenario_id, char *bf, size_t bfsize)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    bf[0] = 0;
    if(!priv->gobj_treedb_controlcenter) {
        return;
    }
    /*  A new scenario has no runs  */
    json_t *scenario = gobj_get_node(
        priv->gobj_treedb_controlcenter,
        "scenarios",
        json_pack("{s:s}", "id", scenario_id),
        0,
        gobj
    );
    if(!scenario) {
        return;
    }
    JSON_DECREF(scenario)
    json_t *runs = sort_runs_newest_first(gobj_node_children(
        priv->gobj_treedb_controlcenter,
        "scenarios",
        json_pack("{s:s}", "id", scenario_id),
        "runs",
        0,
        json_pack("{s:b}", "fkey_only_id", 1),
        gobj
    ));
    size_t idx; json_t *run;
    json_array_foreach(runs, idx, run) {
        if(!run_changes_state(gobj, run)) {
            continue;
        }
        snprintf(bf, bfsize, "%s", kw_get_str(gobj, run, "action", "", 0));
        break;
    }
    JSON_DECREF(runs)
}

/***************************************************************************
 *  Does the scenario run? A run of it in flight, or its last run that
 *  changed its state is not a stop.
 ***************************************************************************/
PRIVATE BOOL scenario_runs_now(hgobj gobj, const char *scenario_id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->run && strcmp(kw_get_str(gobj, priv->run, "scenario_id", "", 0), scenario_id)==0) {
        return TRUE;
    }
    char action[NAME_MAX];
    last_state_action(gobj, scenario_id, action, sizeof(action));
    return !empty_string(action) && strcmp(action, "stop")!=0;
}

/***************************************************************************
 *  Is it RUNNING, not paused nor stopped: its last run that changed its
 *  state is a start or a resume. What a switch is turned on in.
 ***************************************************************************/
PRIVATE BOOL scenario_is_running(hgobj gobj, const char *scenario_id)
{
    char action[NAME_MAX];
    last_state_action(gobj, scenario_id, action, sizeof(action));
    return strcmp(action, "start")==0 || strcmp(action, "resume")==0;
}

/*  `stop_at` of a scenario answered: when its max_run stops it  */
PRIVATE void add_stop_at(hgobj gobj, json_t *scenario)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *stop = json_object_get(priv->auto_stops, kw_get_str(gobj, scenario, "id", "", 0));
    if(stop) {
        json_object_set(scenario, "stop_at", json_object_get(stop, "stop_at"));
        if(json_is_true(json_object_get(stop, "waits_node"))) {
            json_object_set_new(scenario, "stop_waits_node", json_true());
        }
    }

    /*
     *  The run of it in flight, if any: a run is written only when it ends,
     *  and a client that read only the runs took a start still running for
     *  the stop before it
     */
    if(priv->run && strcmp(kw_get_str(gobj, priv->run, "scenario_id", "", 0),
            kw_get_str(gobj, scenario, "id", "", 0))==0) {
        json_object_set_new(scenario, "run_in_flight", json_pack("{s:s, s:s, s:s, s:s, s:I}",
            "id", kw_get_str(gobj, priv->run, "id", "", 0),
            "action", kw_get_str(gobj, priv->run, "action", "", 0),
            "scenario_yuno", kw_get_str(gobj, priv->run, "scenario_yuno", "", 0),
            "username", kw_get_str(gobj, priv->run, "username", "", 0),
            "started_at", (json_int_t)kw_get_int(gobj, priv->run, "started_at", 0, 0)
        ));
    }
}

/***************************************************************************
 *  The stop timer, at the next stop due (at most an hour away: the wall
 *  clock the schedule is written in may have moved meanwhile).
 ***************************************************************************/
PRIVATE void arm_stop_timer(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t next = 0;
    const char *id; json_t *jn_at;
    json_object_foreach(priv->auto_stops, id, jn_at) {
        if(json_is_true(json_object_get(jn_at, "waits_node"))) {
            continue;   // retry_stops_waiting_node()
        }
        json_int_t at = kw_get_int(gobj, jn_at, "stop_at", 0, KW_REQUIRED);
        if(!next || at < next) {
            next = at;
        }
    }
    clear_timeout(priv->stop_timer);
    if(!next) {
        return;
    }
    json_int_t ms = (next - (json_int_t)time_in_seconds()) * 1000;
    if(ms < 1000) {
        ms = 1000;
    }
    if(ms > 3600*1000) {
        ms = 3600*1000;
    }
    set_timeout(priv->stop_timer, ms);
}

/***************************************************************************
 *  The stop timer fired: the first scenario whose max_run is due is
 *  stopped by the control center itself, as a run of its own. Another
 *  run in flight: a few seconds later.
 ***************************************************************************/
PRIVATE int stop_due_scenario(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t now = (json_int_t)time_in_seconds();
    char scenario_id[SCENARIO_ID_MAX+1] = "";
    char started_by[NAME_MAX] = "";
    const char *id; json_t *jn_at;
    json_object_foreach(priv->auto_stops, id, jn_at) {
        if(json_is_true(json_object_get(jn_at, "waits_node"))) {
            continue;   // retry_stops_waiting_node()
        }
        if(kw_get_int(gobj, jn_at, "stop_at", 0, KW_REQUIRED) <= now) {
            snprintf(scenario_id, sizeof(scenario_id), "%s", id);
            snprintf(started_by, sizeof(started_by), "%s",
                kw_get_str(gobj, jn_at, "started_by", "", 0));
            break;
        }
    }
    if(empty_string(scenario_id)) {
        arm_stop_timer(gobj);   // the clock moved: not due yet
        return 0;
    }
    if(priv->run) {
        set_timeout(priv->stop_timer, 5000);
        return 0;
    }

    json_t *scenario = gobj_get_node(
        priv->gobj_treedb_controlcenter,
        "scenarios",
        json_pack("{s:s}", "id", scenario_id),
        0,
        gobj
    );
    /*  Who started what is stopped: the audit of the run says it  */
    char reason[NAME_MAX+64];
    snprintf(reason, sizeof(reason), "max_run of %lld s reached (started by %s)",
        (long long)kw_get_int(gobj, scenario, "max_run", 0, KW_WILD_NUMBER), started_by);
    JSON_DECREF(scenario)

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_APP,
        "msg",          "%s", "stopping a scenario: its max_run is reached",
        "scenario_id",  "%s", scenario_id,
        NULL
    );
    json_int_t scheduled_at = kw_get_int(gobj,
        json_object_get(priv->auto_stops, scenario_id), "scheduled_at", 0, KW_REQUIRED);
    json_object_del(priv->auto_stops, scenario_id);
    char err[512];
    if(start_run(gobj, scenario_id, "stop", gobj_yuno_role_plus_name(), "", 0, NULL,
            reason, "", err, sizeof(err)) < 0) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "the stop of a scenario's max_run cannot run, tried again in a minute",
            "scenario_id",  "%s", scenario_id,
            "comment",      "%s", err,
            NULL
        );
        json_object_set_new(priv->auto_stops, scenario_id, json_pack("{s:I, s:I, s:s}",
            "stop_at", now + 60, "scheduled_at", scheduled_at, "started_by", started_by));
        arm_stop_timer(gobj);
    }
    return 0;
}

/***************************************************************************
 *  An agent connected: the stops of a max_run that did nothing and wait
 *  for a node (run_end()) are due again, those with a node of their stop
 *  connected now. One whose nodes are all still away goes on waiting,
 *  and writes no run.
 ***************************************************************************/
PRIVATE void retry_stops_waiting_node(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->gobj_treedb_controlcenter) {
        return;
    }
    BOOL due = FALSE;
    const char *id; json_t *jn_at;
    json_object_foreach(priv->auto_stops, id, jn_at) {
        if(!json_is_true(json_object_get(jn_at, "waits_node"))) {
            continue;
        }
        if(stop_reaches_a_node(gobj, id)) {
            json_object_del(jn_at, "waits_node");
            json_object_set_new(jn_at, "stop_at", json_integer((json_int_t)time_in_seconds()));
            due = TRUE;
        }
    }
    if(due) {
        arm_stop_timer(gobj);
    }
}

/***************************************************************************
 *  Is the agent of a node of these steps connected?
 ***************************************************************************/
PRIVATE BOOL steps_reach_a_node(hgobj gobj, json_t *steps)
{
    size_t idx; json_t *step;
    json_array_foreach(steps, idx, step) {
        if(find_agent_channel(gobj, kw_get_str(gobj, step, "node", "", 0))) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  Is the agent of a node of a scenario's stop connected? FALSE, with a
 *  warning, when its stop cannot be built (the scenario gone, a step
 *  with no node).
 ***************************************************************************/
PRIVATE BOOL stop_reaches_a_node(hgobj gobj, const char *scenario_id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *scenario = gobj_get_node(
        priv->gobj_treedb_controlcenter,
        "scenarios",
        json_pack("{s:s}", "id", scenario_id),
        0,
        gobj
    );
    char why[256];
    snprintf(why, sizeof(why), "scenario not found");
    json_t *steps = scenario?
        build_run_steps(gobj, scenario, "stop", "", why, sizeof(why)) : NULL;
    JSON_DECREF(scenario)
    if(!steps) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "the stop of a scenario's max_run that waits for a node cannot run",
            "scenario_id",  "%s", scenario_id,
            "comment",      "%s", why,
            NULL
        );
        return FALSE;
    }
    BOOL reaches = steps_reach_a_node(gobj, steps);
    JSON_DECREF(steps)
    return reaches;
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
    if(src == priv->gobj_input_side) {
        retry_stops_waiting_node(gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Something changed in the users store: a change of what a user may do
 *  -- a role, or a link from a role (a user given or taken it, a child
 *  role) -- is told to the clients, which ask `my-permissions` again. A
 *  user's own record changes at every login and is not one.
 ***************************************************************************/
PRIVATE int ac_authz_store_changed(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    const char *topic = (event == EV_TREEDB_NODE_LINKED || event == EV_TREEDB_NODE_UNLINKED)?
        kw_get_str(gobj, kw, "parent_topic_name", "", 0) :
        kw_get_str(gobj, kw, "topic_name", "", 0);
    if(strcmp(topic, "roles")==0) {
        gobj_publish_event(gobj, EV_PERMISSIONS_CHANGED, json_object());
    }
    KW_DECREF(kw)
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
            run_uses_channel(gobj, channel_gobj)) {
        run_agent_disconnected(gobj, channel_gobj);
    }
    if(src == priv->gobj_top_side && channel_gobj) {
        gobj_write_user_data(channel_gobj, "cc_connection", json_integer(0));
    }
    if(src == priv->gobj_input_side && channel_gobj) {
        gobj_write_user_data(channel_gobj, "link_requesters", json_object());
    }

    /*
     *  The consoles mirrored through an agent's channel end with it: their
     *  clients are dropped, each only if it is still the connection that
     *  opened its console.
     */
    json_t *mirrors = gobj_read_user_data(channel_gobj, "tty_mirrors");
    if(json_object_size(mirrors) > 0) {
        json_t *dropped = json_object();
        const char *console; json_t *jn_mirror;
        json_object_foreach(mirrors, console, jn_mirror) {
            const char *dst_service = kw_get_str(gobj, jn_mirror, "dst_service", "", 0);
            hgobj gobj_requester = gobj_child_by_name(priv->gobj_top_side, dst_service);
            json_int_t mirror_connection = kw_get_int(gobj, jn_mirror, "connection", 0, 0);
            if(!gobj_requester || json_object_get(dropped, dst_service)) {
                continue;
            }
            if(mirror_connection && mirror_connection != connection_number(gobj_requester)) {
                continue;   // its channel is another client's now
            }
            json_object_set_new(dropped, dst_service, json_true());
            gobj_send_event(gobj_requester, EV_DROP, 0, gobj);
        }
        JSON_DECREF(dropped)
        gobj_write_user_data(channel_gobj, "tty_mirrors", json_object());
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  HACK intermediate node
 *  An answer reaches the web client only while its channel is still the
 *  connection that asked (the name of a closed channel is the next
 *  client's).
 ***************************************************************************/
PRIVATE int ac_stats_yuno_answer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(!is_from_agent_side(gobj, event, kw, src)) {
        KW_DECREF(kw);
        return 0;
    }

    char dst_service[NAME_MAX];
    hgobj gobj_requester = requester_of_answer(gobj, event, kw, dst_service, sizeof(dst_service));
    if(!gobj_requester) {
        // Error already logged
        KW_DECREF(kw);
        return 0;
    }

    return relay_to_requester(gobj, event, kw, gobj_requester);
}

/***************************************************************************
 *  HACK intermediate node
 *  As ac_stats_yuno_answer(), and the answers of the steps of a run.
 ***************************************************************************/
PRIVATE int ac_command_yuno_answer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!is_from_agent_side(gobj, event, kw, src)) {
        KW_DECREF(kw);
        return 0;
    }

    /*
     *  The answer of a step of a scenario run: this gobj asked it, and
     *  only the agent of the step answers it
     */
    if(kw_get_dict_value(gobj, kw, "__md_iev__`cc_run", 0, 0)) {
        hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, 0);
        int step_idx = (int)kw_get_int(gobj, kw, "__md_iev__`cc_step", -1, KW_WILD_NUMBER);
        if(priv->run && run_channel_of_step(gobj, step_idx) && channel_gobj != run_channel_of_step(gobj, step_idx)) {
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

    char dst_service[NAME_MAX];
    hgobj gobj_requester = requester_of_answer(gobj, event, kw, dst_service, sizeof(dst_service));
    if(!gobj_requester) {
        // Error already logged
        KW_DECREF(kw);
        return 0;
    }

    return relay_to_requester(gobj, event, kw, gobj_requester);
}

/***************************************************************************
 *  HACK intermediate node
 *  The PTY of an open-console opens: the agent's channel remembers, per
 *  console, the client of its mirror and its connection, to drop it if the
 *  agent goes. Several consoles go through one agent's channel; a console
 *  opened again by another client is that client's now, as the agent
 *  routes it (add_console_route() refreshes the route of the channel).
 ***************************************************************************/
PRIVATE int ac_tty_mirror_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!is_from_agent_side(gobj, event, kw, src)) {
        KW_DECREF(kw);
        return 0;
    }

    char dst_service[NAME_MAX];
    hgobj gobj_requester = requester_of_answer(gobj, event, kw, dst_service, sizeof(dst_service));
    if(!gobj_requester) {
        // Error already logged
        KW_DECREF(kw);
        return 0;
    }

    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    json_t *mirrors = gobj_read_user_data(channel_gobj, "tty_mirrors");
    if(!json_is_object(mirrors)) {
        mirrors = json_object();
        gobj_write_user_data(channel_gobj, "tty_mirrors", mirrors);
    }
    json_object_set_new(mirrors, kw_get_str(gobj, kw, "data`name", "", 0), json_pack("{s:s, s:I}",
        "dst_service", dst_service,
        "connection", channel_of_side(priv->gobj_top_side, gobj_requester)?
            connection_number(gobj_requester) : (json_int_t)0
    ));

    return relay_to_requester(gobj, event, kw, gobj_requester);
}

/***************************************************************************
 *  HACK intermediate node
 *  The PTY closes: the agent's channel forgets that console's mirror,
 *  whoever holds the client's channel now.
 ***************************************************************************/
PRIVATE int ac_tty_mirror_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(!is_from_agent_side(gobj, event, kw, src)) {
        KW_DECREF(kw);
        return 0;
    }

    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    json_t *mirrors = gobj_read_user_data(channel_gobj, "tty_mirrors");
    if(json_is_object(mirrors)) {
        json_object_del(mirrors, kw_get_str(gobj, kw, "data`name", "", 0));
    }

    char dst_service[NAME_MAX];
    hgobj gobj_requester = requester_of_answer(gobj, event, kw, dst_service, sizeof(dst_service));
    if(!gobj_requester) {
        // Error already logged
        KW_DECREF(kw);
        return 0;
    }

    return relay_to_requester(gobj, event, kw, gobj_requester);
}

/***************************************************************************
 *  HACK intermediate node
 *  The PTY's output, relayed as ac_yuno_stats_relay() relays a watch: a
 *  frame for a client that is gone is counted, not logged one by one.
 ***************************************************************************/
PRIVATE int ac_tty_mirror_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!is_from_agent_side(gobj, event, kw, src)) {
        KW_DECREF(kw);
        return 0;
    }

    char dst_service[NAME_MAX];
    BOOL reconnected;
    hgobj gobj_requester = requester_of_route(
        gobj, kw, dst_service, sizeof(dst_service), &reconnected
    );
    if(reconnected || (gobj_requester && !requester_listens(gobj_requester))) {
        count_dropped_stream(gobj, event, dst_service, reconnected,
            &priv->tty_dropped, &priv->t_tty_dropped_log);
        KW_DECREF(kw);
        return 0;
    }
    if(!gobj_requester) {
        /*
         *  A route of the agent that names nobody here: said once a
         *  window, as the frames of a client that is gone
         */
        priv->tty_unrouted++;
        if(drops_window_ended(gobj, priv->t_tty_unrouted_log)) {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL,
                "msg",          "%s", "PTY output of an agent for no requester of this control center, dropped",
                "service",      "%s", dst_service,
                "dropped",      "%lu", (unsigned long)priv->tty_unrouted,
                NULL
            );
            priv->tty_unrouted = 0;
            priv->t_tty_unrouted_log = start_msectimer(0);
        }
        arm_drops_timer(gobj);
        KW_DECREF(kw);
        return 0;
    }
    return relay_to_requester(gobj, event, kw, gobj_requester);
}

/***************************************************************************
 *  A reading of a watch-yuno-stats, sent by an agent along the route of the
 *  web client that asked for it: relay it to that client, the way
 *  ac_tty_mirror_data() relays the PTY. The client may be gone -- a tab
 *  closed: the agent only learns when its watch expires, not renewed -- so
 *  a reading for nobody is expected. It is counted, and said once a window,
 *  not once per reading.
 ***************************************************************************/
PRIVATE int ac_yuno_stats_relay(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!is_from_agent_side(gobj, event, kw, src)) {
        KW_DECREF(kw);
        return 0;
    }

    char dst_service[NAME_MAX];
    BOOL reconnected;
    hgobj gobj_requester = requester_of_route(
        gobj, kw, dst_service, sizeof(dst_service), &reconnected
    );

    if(!gobj_requester || !requester_listens(gobj_requester)) {
        count_dropped_stream(gobj, event, dst_service, reconnected,
            &priv->stats_dropped, &priv->t_stats_dropped_log);
        KW_DECREF(kw);
        return 0;
    }
    return relay_to_requester(gobj, event, kw, gobj_requester);
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
    priv->rxMsgs++;
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

        priv->txMsgs++;
        json_t *webix = gobj_command( // debe retornar siempre 0.
            child,
            "write-tty",
            kw_incref(kw),
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
    if(src == priv->stop_timer) {
        KW_DECREF(kw);
        return stop_due_scenario(gobj);
    }
    if(src == priv->drops_timer) {
        priv->drops_timer_armed = FALSE;    // it fired: a one-shot is not armed
        gobj_write_bool_attr(gobj, "drops_timer_armed", FALSE);
        say_pending_drops(gobj, "its window ended", TRUE);
        arm_drops_timer(gobj);
        KW_DECREF(kw);
        return 0;
    }

    if(src == priv->rates_timer) {
        update_rates(gobj);
        KW_DECREF(kw);
        return 0;
    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "a timeout of a timer this gobj does not have",
        "event",        "%s", event,
        "src",          "%s", gobj_short_name(src),
        NULL
    );
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
    .mt_reading                 = mt_reading,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_CONTROLCENTER);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_PERMISSIONS_CHANGED);
GOBJ_DEFINE_EVENT(EV_SCENARIO_RUN_ENDED);

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
        {EV_TIMEOUT_PERIODIC,       ac_timeout,              0},
        {EV_STOPPED,                0,                       0},

        {EV_TREEDB_NODE_CREATED,    ac_authz_store_changed,  0},
        {EV_TREEDB_NODE_UPDATED,    ac_authz_store_changed,  0},
        {EV_TREEDB_NODE_DELETED,    ac_authz_store_changed,  0},
        {EV_TREEDB_NODE_LINKED,     ac_authz_store_changed,  0},
        {EV_TREEDB_NODE_UNLINKED,   ac_authz_store_changed,  0},
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
        {EV_TIMEOUT_PERIODIC,       0},
        {EV_STOPPED,                0},

        {EV_TREEDB_NODE_CREATED,    0},
        {EV_TREEDB_NODE_UPDATED,    0},
        {EV_TREEDB_NODE_DELETED,    0},
        {EV_TREEDB_NODE_LINKED,     0},
        {EV_TREEDB_NODE_UNLINKED,   0},

        /*  What a user may do changed: a client asks my-permissions again  */
        {EV_PERMISSIONS_CHANGED,    EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        /*  A scenario run ended: a client watching it reads its state again  */
        {EV_SCENARIO_RUN_ENDED,     EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},

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
