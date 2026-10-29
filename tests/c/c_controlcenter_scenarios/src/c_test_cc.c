/***********************************************************************
 *          C_TEST_CC.C
 *
 *          GClass to test the scenarios of the control center, and the
 *          routing of what its agents send back, on the control center's
 *          own source (yunos/c/controlcenter/src/c_controlcenter.c).
 *
 *          The control center runs as in production, between two sides
 *          played by C_TEST_CC_PEER: `__top_side__`, whose channels are web
 *          clients, and `__input_side__`, with one agent connected -- a
 *          real C_IEVENT_SRV in session, whose frames are decoded by the
 *          peer below it, and answered by this gobj as the agent would
 *          (msg_iev_build_response() on the request).
 *
 *          Verified:
 *              - save-scenario with the scenario as a string answers with
 *                its id: the id was read from the parsed json after it
 *                was freed;
 *              - a step parameter named like a framework key (`__md_iev__`,
 *                `__username__`) is refused by save-scenario;
 *              - a scenario saved before those checks (written straight into
 *                the treedb here) is checked again at run-scenario, and the
 *                run refused naming the step;
 *              - a step answer is taken only from the agent the step went
 *                to, and only as this control center marked it: a client
 *                cannot pass one through command-agent, nor inject one;
 *              - what an agent sends back for a web client (a command's or
 *                a stats' answer, the PTY mirror) reaches its channel only
 *                while it is the connection that asked: a closed channel's
 *                name is taken by the next client; and the agent's close
 *                drops the client of its console mirror, and only it;
 *              - what only an agent sends, injected by a web client and
 *                routed to another client, reaches nobody;
 *              - several consoles mirrored through one agent's channel:
 *                the agent's close drops the client of each, once;
 *              - the drops timer says a capped count when its window
 *                (drops_warning_window) ends, not at the stop, and is not
 *                left armed with nothing counted;
 *              - the agent is told the client takes EV_YUNO_STATS only when
 *                the client said so in its own __relays__;
 *              - what an agent sends back for no web client goes only to
 *                the C_IEVENT_CLI link the request came in by, never to a
 *                local service a hop below names, nor to a link that never
 *                asked that agent; a flood of it, or of agent events
 *                injected by a client, is said once a minute, also when
 *                the client loops connect/inject/leave;
 *              - a user who may read one scenario lists that one only,
 *                watches it with watch-scenario (the line the agent gets
 *                is built here) and is told so by my-permissions;
 *              - a change of a role, or of a link from a role, in the users
 *                store is told as EV_PERMISSIONS_CHANGED; a user's own
 *                update is not;
 *              - a stop, a pause or a report sends every step at once and
 *                waits for them all, naming each step that failed or was
 *                not answered; a start ends at its first failed step; and
 *                `scenarios` says the run in flight.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test_cc_peer.h"
#include "c_test_cc.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define AGENT_HOST  "node1"
#define GHOST_HOST  "ghost"     // case 19: a node whose agent connects late
#define DROPS_WAIT_MS   5000    // the most a phase waits for the drops timer

GOBJ_DECLARE_EVENT(EV_PERMISSIONS_CHANGED);     // c_controlcenter.c
GOBJ_DECLARE_EVENT(EV_SCENARIO_RUN_ENDED);      // c_controlcenter.c

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "level of help"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-------json_fn---------description--*/
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,    cmd_help,       "Command's help"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES  = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name--------------------flag----alias---items---description--*/
SDATA_END()
};

/*---------------------------------------------*
 *      Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;

    hgobj cc;
    hgobj top_side;
    hgobj input_side;

    hgobj agent_channel;    // the agent's channel in __input_side__
    hgobj agent_wire;       // below its C_IEVENT_SRV: what is sent to the agent
    hgobj ghost_channel;    // case 19: the agent of GHOST_HOST, connected late
    hgobj ghost_wire;

    int phase;              // the tests run in the loop: each phase a timeout
    uint64_t t_wait;        // msectimer: the deadline of what a phase waits for
    int result;
    hgobj client1;
    hgobj client3;
    json_int_t rates_tx0;
    json_int_t rates_rx0;
    int permissions_changed;    // EV_PERMISSIONS_CHANGED published by the control center
    int runs_ended;             // EV_SCENARIO_RUN_ENDED published by the control center
    json_int_t max_run_started; // when the start of case 16 ended (its stop is due 1 s after)
} PRIVATE_DATA;





            /******************************
             *      Framework Methods
             ******************************/




/***************************************************************************
 *              Framework Method Create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  The control center's treedb, wiped on every run
     */
    char path[PATH_MAX];
    yuneta_realm_store_dir(
        path,
        sizeof(path),
        gobj_yuno_role(),
        gobj_yuno_realm_owner(),
        gobj_yuno_realm_id(),
        "",
        "",
        FALSE
    );
    if(!empty_string(path)) {
        rmrdir(path);
    }

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

/***************************************************************************
 *              Framework Method Start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    return 0;
}

/***************************************************************************
 *              Framework Method Stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    return 0;
}

/***************************************************************************
 *              Framework Method Play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Once the control center plays: its treedb is open then
     */
    set_timeout(priv->timer, 100);
    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  A failed check: logged (the FIFO of expected logs fails too), -1
 ***************************************************************************/
PRIVATE int fail(hgobj gobj, const char *what, const char *detail)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "TEST FAIL",
        "what",         "%s", what,
        "detail",       "%s", detail? detail : "",
        NULL
    );
    return -1;
}

/***************************************************************************
 *  A request of a web client on `channel_name`, as the C_IEVENT_SRV of
 *  __top_side__ hands it to the control center: its hop on the stack.
 ***************************************************************************/
PRIVATE json_t *client_kw(const char *channel_name)
{
    return json_pack("{s:s, s:{s:[{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}]}}",
        "__username__", "alice",
        "__md_iev__",
            "ievent_gate_stack",
                "dst_yuno", "",
                "dst_role", "",
                "dst_service", "controlcenter",
                "src_yuno", "browser",
                "src_role", "gui_agent",
                "src_service", "gui",
                "user", "alice",
                "host", "browser",
                "input_service", "__top_side__",
                "input_channel", channel_name
    );
}

/***************************************************************************
 *  A web client connects on `channel`
 ***************************************************************************/
PRIVATE void client_opens(hgobj gobj, hgobj channel)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_bool_attr(channel, "opened", TRUE);
    gobj_send_event(priv->cc, EV_ON_OPEN,
        json_pack("{s:{s:I}}", "__temp__", "channel_gobj", (json_int_t)(uintptr_t)channel),
        priv->top_side
    );
}

/***************************************************************************
 *  The web client of `channel` leaves
 ***************************************************************************/
PRIVATE void client_closes(hgobj gobj, hgobj channel)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_bool_attr(channel, "opened", FALSE);
    gobj_send_event(priv->cc, EV_ON_CLOSE,
        json_pack("{s:{s:I}}", "__temp__", "channel_gobj", (json_int_t)(uintptr_t)channel),
        priv->top_side
    );
}

/***************************************************************************
 *  The agent's channel closes, as __input_side__ says it (the agent's
 *  C_IEVENT_SRV stays in session: only the control center is told)
 ***************************************************************************/
PRIVATE void agent_closes(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_send_event(priv->cc, EV_ON_CLOSE,
        json_pack("{s:{s:I}}", "__temp__", "channel_gobj", (json_int_t)(uintptr_t)priv->agent_channel),
        priv->input_side
    );
}

/***************************************************************************
 *  What reached a peer since the last time (yours)
 ***************************************************************************/
PRIVATE json_t *take_received(hgobj peer)
{
    json_t *received = json_incref(gobj_read_user_data(peer, "received"));
    gobj_write_user_data(peer, "received", json_array());
    return received;
}

/***************************************************************************
 *  The last request the agent got (yours), or NULL (a fail)
 ***************************************************************************/
PRIVATE json_t *agent_request(hgobj gobj, const char *what)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *received = take_received(priv->agent_wire);
    size_t n = json_array_size(received);
    json_t *kw = n? json_incref(json_object_get(json_array_get(received, n-1), "kw")) : NULL;
    JSON_DECREF(received)
    if(!kw) {
        fail(gobj, what, "the agent got no request");
    }
    return kw;
}

/***************************************************************************
 *  The agent sends `event` with `jn_data` (owned, the PTY's {name, ...} of
 *  a console) back along the route of `request` (not owned)
 ***************************************************************************/
PRIVATE void agent_sends_data(
    hgobj gobj,
    gobj_event_t event,
    json_t *request,
    int result,
    const char *comment,
    json_t *jn_data
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = msg_iev_build_response(
        gobj,
        result,
        json_string(comment),
        0,
        jn_data,
        json_deep_copy(request)
    );
    kw_set_subdict_value(gobj, kw, "__temp__", "channel_gobj",
        json_integer((json_int_t)(uintptr_t)priv->agent_channel));
    gobj_send_event(priv->cc, event, kw, priv->input_side);
}

/***************************************************************************
 *  The agent sends `event` back along the route of `request` (not owned),
 *  as C_AGENT does: msg_iev_build_response() on the request, arriving
 *  from its channel of __input_side__.
 ***************************************************************************/
PRIVATE void agent_sends(hgobj gobj, gobj_event_t event, json_t *request, int result, const char *comment)
{
    agent_sends_data(gobj, event, request, result, comment, NULL);
}

/***************************************************************************
 *  How many of `event` reached `peer`; the received are taken
 ***************************************************************************/
PRIVATE int count_received(hgobj peer, const char *event, const char *comment_part)
{
    json_t *received = take_received(peer);
    int n = 0;
    size_t idx; json_t *jn;
    json_array_foreach(received, idx, jn) {
        const char *ev = json_string_value(json_object_get(jn, "event"));
        if(!ev || strcmp(ev, event)!=0) {
            continue;
        }
        if(comment_part) {
            const char *comment = json_string_value(
                json_object_get(json_object_get(jn, "kw"), "comment")
            );
            if(!comment || !strstr(comment, comment_part)) {
                continue;
            }
        }
        n++;
    }
    JSON_DECREF(received)
    return n;
}

/***************************************************************************
 *  A command's answer (owned): the result and a part of the comment
 ***************************************************************************/
PRIVATE int check_response(
    hgobj gobj,
    json_t *response,   // owned
    int expected_result,
    const char *comment_part,
    const char *what
)
{
    int ret = 0;
    if(!response) {
        return fail(gobj, what, "no answer");
    }
    int result = (int)kw_get_int(gobj, response, "result", -99, KW_WILD_NUMBER);
    const char *comment = kw_get_str(gobj, response, "comment", "", 0);
    if(result != expected_result) {
        ret = fail(gobj, what, comment);
    } else if(comment_part && !strstr(comment, comment_part)) {
        ret = fail(gobj, what, comment);
    }
    JSON_DECREF(response)
    return ret;
}

/***************************************************************************
 *  A channel of a side
 ***************************************************************************/
PRIVATE hgobj create_peer(const char *name, hgobj parent)
{
    hgobj peer = gobj_create(name, C_TEST_CC_PEER, 0, parent);
    gobj_start(peer);
    return peer;
}

/***************************************************************************
 *  The agent of AGENT_HOST, connected: a C_IEVENT_SRV in session in a
 *  channel of __input_side__, over a peer that decodes what it is sent.
 ***************************************************************************/
PRIVATE int connect_agent(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->agent_channel = create_peer("agent-1", priv->input_side);
    priv->agent_wire = create_peer("agent-1-wire", priv->agent_channel);

    json_t *kw_srv = json_pack("{s:s, s:s, s:s, s:{s:s, s:{s:[{s:s}]}}}",
        "client_yuno_role", "yuneta_agent",
        "client_yuno_name", AGENT_HOST,
        "client_yuno_service", "agent",
        "identity_card",
            "id", "uuid-" AGENT_HOST,
            "__md_iev__",
                "ievent_gate_stack",
                    "host", AGENT_HOST
    );
    hgobj srv = gobj_create("agent-1", C_IEVENT_SRV, kw_srv, priv->agent_channel);
    if(!srv) {
        return fail(gobj, "connect_agent", "cannot create the C_IEVENT_SRV");
    }
    gobj_set_bottom_gobj(srv, priv->agent_wire);
    gobj_start(srv);
    gobj_change_state(srv, ST_SESSION);
    return 0;
}

/***************************************************************************
 *  1. save-scenario, the scenario as a string: answered with its id.
 *
 *  The command parser already parses a DTP_JSON parameter that arrives as
 *  a string, so the handler sees a string only when the json itself is a
 *  string holding the scenario -- encoded twice, as a client that
 *  stringifies what it already stringified sends it.
 ***************************************************************************/
PRIVATE int test_save_scenario_as_string(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_text = json_string(
        "{\"id\": \"string-form-scenario-0001\", \"node\": \"" AGENT_HOST "\","
        " \"yunos\": [{\"id\": \"1234\"}],"
        " \"actions\": {\"start\": [{\"yuno\": \"1234\", \"command\": \"resume-generation\"}]}}"
    );
    char *encoded = json_dumps(jn_text, JSON_ENCODE_ANY);
    JSON_DECREF(jn_text)
    if(!encoded) {
        return fail(gobj, "save-scenario as a string", "cannot encode it");
    }

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", json_string(encoded));
    gbmem_free(encoded);
    return check_response(gobj,
        gobj_command(priv->cc, "save-scenario", kw, client),
        0,
        "scenario created: string-form-scenario-0001",
        "save-scenario as a string names the scenario"
    );
}

/***************************************************************************
 *  A scenario whose start is one step of `command`, to save (yours)
 ***************************************************************************/
PRIVATE json_t *scenario_with_step(const char *id, const char *command)
{
    return json_pack("{s:s, s:s, s:[{s:s}], s:{s:[{s:s, s:s}]}}",
        "id", id,
        "node", AGENT_HOST,
        "yunos", "id", "1234",
        "actions", "start", "yuno", "1234", "command", command
    );
}

/***************************************************************************
 *  2. A step parameter named like a framework key is refused
 ***************************************************************************/
PRIVATE int test_save_refuses_framework_keys(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    const char *commands[] = {
        "resume-generation __md_iev__=x",
        "resume-generation __username__=root",
        "resume-generation __md_command__=x",
        0
    };
    for(int i=0; commands[i]; i++) {
        json_t *kw = client_kw(gobj_name(client));
        json_object_set_new(kw, "scenario", scenario_with_step("framework-keys", commands[i]));
        ret += check_response(gobj,
            gobj_command(priv->cc, "save-scenario", kw, client),
            -1,
            "actions.start[0]: command",
            commands[i]
        );
    }

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", scenario_with_step("scn", "resume-generation rate=10"));
    ret += check_response(gobj,
        gobj_command(priv->cc, "save-scenario", kw, client),
        0,
        "scenario created: scn",
        "a plain step is saved"
    );
    return ret;
}

/***************************************************************************
 *  3. A scenario saved before the step checks is checked at the run
 ***************************************************************************/
PRIVATE int test_run_checks_old_scenarios(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    hgobj treedb = gobj_find_service("treedb_controlcenter", TRUE);
    const char *commands[] = {
        "resume-generation __username__=root",
        "resume-generation id=4321",
        0
    };
    for(int i=0; commands[i]; i++) {
        char id[64];
        snprintf(id, sizeof(id), "saved-before-checks-%d", i);
        json_t *record = scenario_with_step(id, commands[i]);
        json_object_set_new(record, "links", json_array());
        json_object_set_new(record, "view", json_object());
        json_t *node = gobj_update_node(treedb, "scenarios", record,
            json_pack("{s:b}", "create", 1), gobj);
        if(!node) {
            ret += fail(gobj, "write an old scenario", id);
            continue;
        }
        JSON_DECREF(node)

        json_t *kw = client_kw(gobj_name(client));
        json_object_set_new(kw, "scenario_id", json_string(id));
        json_object_set_new(kw, "action", json_string("start"));
        ret += check_response(gobj,
            gobj_command(priv->cc, "run-scenario", kw, client),
            -1,
            "step 0",
            commands[i]
        );
    }
    json_t *sent = take_received(priv->agent_wire);
    if(json_array_size(sent) != 0) {
        ret += fail(gobj, "a refused run sends nothing", "");
    }
    JSON_DECREF(sent)
    return ret;
}

/***************************************************************************
 *  4. A step answer counts only from the step's agent, as marked here
 ***************************************************************************/
PRIVATE int test_forged_step_answers(hgobj gobj, hgobj requester, hgobj other)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_t *kw = client_kw(gobj_name(requester));
    json_object_set_new(kw, "scenario_id", json_string("scn"));
    json_object_set_new(kw, "action", json_string("start"));
    json_t *response = gobj_command(priv->cc, "run-scenario", kw, requester);
    if(response) {
        ret += fail(gobj, "run-scenario answers when the run is over",
            kw_get_str(gobj, response, "comment", "", 0));
        JSON_DECREF(response)
        return ret;
    }
    json_t *step = agent_request(gobj, "the step goes to the agent");
    if(!step) {
        return -1;
    }
    const char *run_id = kw_get_str(gobj, step, "__md_iev__`cc_run", "", 0);

    /*
     *  Another client, through command-agent, with the marks of the step
     */
    kw = client_kw(gobj_name(other));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("list-yunos"));
    kw_set_subdict_value(gobj, kw, "__md_iev__", "cc_run", json_string(run_id));
    kw_set_subdict_value(gobj, kw, "__md_iev__", "cc_step", json_integer(0));
    ret += check_response(gobj,
        gobj_command(priv->cc, "command-agent", kw, other),
        0, 0, "command-agent with the marks of a step"
    );
    json_t *forwarded = agent_request(gobj, "command-agent reaches the agent");
    if(forwarded) {
        if(kw_has_key(kw_get_dict(gobj, forwarded, "__md_iev__", 0, 0), "cc_run") ||
                kw_has_key(kw_get_dict(gobj, forwarded, "__md_iev__", 0, 0), "cc_step")) {
            ret += fail(gobj, "command-agent forwards the marks of a step", run_id);
        }
        agent_sends(gobj, EV_MT_COMMAND_ANSWER, forwarded, 0, "forged through command-agent");
        JSON_DECREF(forwarded)
    } else {
        ret += -1;
    }
    if(count_received(requester, EV_MT_COMMAND_ANSWER, 0) != 0) {
        ret += fail(gobj, "an answer through command-agent ended the run", run_id);
    }
    if(count_received(other, EV_MT_COMMAND_ANSWER, "forged through command-agent") != 1) {
        ret += fail(gobj, "the answer of command-agent reaches its client", "");
    }

    /*
     *  A client injects the answer
     */
    json_t *forged = msg_iev_build_response(gobj, 0, json_string("forged by a client"), 0, 0,
        json_deep_copy(step));
    kw_set_subdict_value(gobj, forged, "__temp__", "channel_gobj",
        json_integer((json_int_t)(uintptr_t)other));
    gobj_send_event(priv->cc, EV_MT_COMMAND_ANSWER, forged, priv->top_side);
    if(count_received(requester, EV_MT_COMMAND_ANSWER, 0) != 0) {
        ret += fail(gobj, "an answer injected by a client ended the run", run_id);
    }

    /*
     *  The agent's
     */
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, 0, "resumed");
    if(count_received(requester, EV_MT_COMMAND_ANSWER, "done, 1 steps") != 1) {
        ret += fail(gobj, "the step's answer ends the run", run_id);
    }
    JSON_DECREF(step)
    return ret;
}

/***************************************************************************
 *  5. What the agent sends back reaches a channel only while it is the
 *     connection that asked
 ***************************************************************************/
PRIVATE int test_late_answers_to_another_client(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("list-yunos"));
    ret += check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
        0, 0, "command-agent");
    json_t *command = agent_request(gobj, "command-agent reaches the agent");

    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "stats2agent", json_string("stats-yuno"));
    ret += check_response(gobj, gobj_command(priv->cc, "stats-agent", kw, client),
        0, 0, "stats-agent");
    json_t *stats = agent_request(gobj, "stats-agent reaches the agent");

    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("open-console"));
    ret += check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
        0, 0, "command-agent open-console");
    json_t *console = agent_request(gobj, "open-console reaches the agent");

    if(!command || !stats || !console) {
        JSON_DECREF(command)
        JSON_DECREF(stats)
        JSON_DECREF(console)
        return -1;
    }

    agent_sends(gobj, EV_TTY_OPEN, console, 0, "tty open");
    if(count_received(client, EV_TTY_OPEN, "tty open") != 1) {
        ret += fail(gobj, "the mirror opens on the client that asked", "");
    }

    /*
     *  The agent goes: the client of its mirror is dropped. The agent's
     *  channel names it as the mirror opened it (read from a freed frame
     *  up to 7.25.20).
     */
    agent_closes(gobj);
    if(count_received(client, EV_DROP, 0) != 1) {
        ret += fail(gobj, "the agent's close drops the client of its mirror", "");
    }
    agent_sends(gobj, EV_TTY_OPEN, console, 0, "tty open again");
    if(count_received(client, EV_TTY_OPEN, "tty open again") != 1) {
        ret += fail(gobj, "the mirror opens again on the client that asked", "");
    }

    /*
     *  The asker goes, another client takes its channel
     */
    client_closes(gobj, client);
    client_opens(gobj, client);

    agent_sends(gobj, EV_MT_COMMAND_ANSWER, command, 0, "late command answer");
    agent_sends(gobj, EV_MT_STATS_ANSWER, stats, 0, "late stats answer");
    agent_sends(gobj, EV_TTY_DATA, console, 0, "late tty data");
    agent_sends(gobj, EV_TTY_DATA, console, 0, "late tty data");
    agent_sends(gobj, EV_TTY_OPEN, console, 0, "late tty open");

    json_t *received = take_received(client);
    if(json_array_size(received) != 0) {
        char *s = json_dumps(received, JSON_COMPACT);
        ret += fail(gobj, "what the agent sent back for a connection that is gone reached the next one", s);
        gbmem_free(s);
    }
    JSON_DECREF(received)

    JSON_DECREF(command)
    JSON_DECREF(stats)

    /*
     *  The new connection's own answer does reach it
     */
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("list-yunos"));
    ret += check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
        0, 0, "command-agent of the new connection");
    command = agent_request(gobj, "command-agent of the new connection reaches the agent");
    if(command) {
        agent_sends(gobj, EV_MT_COMMAND_ANSWER, command, 0, "own answer");
        JSON_DECREF(command)
    }
    if(count_received(client, EV_MT_COMMAND_ANSWER, "own answer") != 1) {
        ret += fail(gobj, "the new connection gets its own answer", "");
    }

    /*
     *  The agent goes: the mirror's client, gone, is not the new one
     */
    agent_closes(gobj);
    if(count_received(client, EV_DROP, 0) != 0) {
        ret += fail(gobj, "the agent's close dropped the connection that took the mirror's channel", "");
    }

    agent_sends(gobj, EV_TTY_CLOSE, console, 0, "late tty close");
    JSON_DECREF(console)
    received = take_received(client);
    if(json_array_size(received) != 0) {
        char *s = json_dumps(received, JSON_COMPACT);
        ret += fail(gobj, "the mirror of a connection that is gone reached the next one", s);
        gbmem_free(s);
    }
    JSON_DECREF(received)
    return ret;
}

/***************************************************************************
 *  6. A web client cannot send what only an agent sends: an answer or a
 *     stream injected from __top_side__, routed by the client to another
 *     client's channel and connection, reaches nobody
 ***************************************************************************/
PRIVATE int test_injected_agent_events(hgobj gobj, hgobj victim, hgobj attacker)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    gobj_event_t events[] = {
        EV_MT_COMMAND_ANSWER,
        EV_MT_STATS_ANSWER,
        EV_TTY_OPEN,
        EV_TTY_DATA,
        EV_TTY_CLOSE,
        EV_YUNO_STATS,
        0
    };
    json_int_t victim_connection = json_integer_value(
        gobj_read_user_data(victim, "cc_connection")
    );
    for(int i=0; events[i]; i++) {
        json_t *kw = json_pack("{s:i, s:s, s:{s:[{s:s}, {s:s, s:I}]}, s:{s:I}}",
            "result", 0,
            "comment", "injected by a client",
            "__md_iev__",
                "ievent_gate_stack",
                    "dst_service", gobj_name(victim),
                    "dst_service", "gui",
                    "cc_connection", victim_connection,
            "__temp__",
                "channel_gobj", (json_int_t)(uintptr_t)attacker
        );
        gobj_send_event(priv->cc, events[i], kw, priv->top_side);
    }

    json_t *received = take_received(victim);
    if(json_array_size(received) != 0) {
        char *s = json_dumps(received, JSON_COMPACT);
        ret += fail(gobj, "what a client injected as an agent reached another client", s);
        gbmem_free(s);
    }
    JSON_DECREF(received)
    json_t *mirrors = gobj_read_user_data(attacker, "tty_mirrors");
    if(json_object_size(mirrors) != 0) {
        ret += fail(gobj, "a client's channel took a mirror it injected", "");
    }
    return ret;
}

/***************************************************************************
 *  `client` opens `console` through the agent (yours: the route the agent
 *  keeps), and the agent says it open
 ***************************************************************************/
PRIVATE json_t *open_console(hgobj gobj, hgobj client, const char *console)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("open-console"));
    json_object_set_new(kw, "name", json_string(console));
    check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
        0, 0, "command-agent open-console");
    json_t *route = agent_request(gobj, "open-console reaches the agent");
    if(route) {
        agent_sends_data(gobj, EV_TTY_OPEN, route, 0, "tty open",
            json_pack("{s:s}", "name", console));
    }
    if(count_received(client, EV_TTY_OPEN, "tty open") != 1) {
        fail(gobj, "a console opens on its client", console);
    }
    return route;
}

/***************************************************************************
 *  7. Several consoles mirrored through one agent's channel: when the
 *     agent goes, the client of each is dropped, once; a console opened
 *     again by another client is that client's, as the agent routes it; a
 *     console closed drops nobody
 ***************************************************************************/
PRIVATE int test_several_mirrors(hgobj gobj, hgobj client_a, hgobj client_b)
{
    int ret = 0;

    /*  Two clients, two consoles  */
    json_t *route_a = open_console(gobj, client_a, "console-a");
    json_t *route_b = open_console(gobj, client_b, "console-b");
    agent_closes(gobj);
    if(count_received(client_a, EV_DROP, 0) != 1 || count_received(client_b, EV_DROP, 0) != 1) {
        ret += fail(gobj, "the agent's close drops the client of each console", "");
    }
    JSON_DECREF(route_a)
    JSON_DECREF(route_b)

    /*  One console, opened again by another client  */
    route_a = open_console(gobj, client_a, "console-a");
    route_b = open_console(gobj, client_b, "console-a");
    agent_closes(gobj);
    if(count_received(client_a, EV_DROP, 0) != 0 || count_received(client_b, EV_DROP, 0) != 1) {
        ret += fail(gobj, "a console opened again is its last client's", "");
    }
    JSON_DECREF(route_a)
    JSON_DECREF(route_b)

    /*  One client, two consoles, one closed  */
    route_a = open_console(gobj, client_a, "console-a");
    route_b = open_console(gobj, client_a, "console-b");
    agent_sends_data(gobj, EV_TTY_CLOSE, route_b, 0, "tty close",
        json_pack("{s:s}", "name", "console-b"));
    if(count_received(client_a, EV_TTY_CLOSE, "tty close") != 1) {
        ret += fail(gobj, "a console closes on its client", "");
    }
    agent_closes(gobj);
    if(count_received(client_a, EV_DROP, 0) != 1) {
        ret += fail(gobj, "the agent's close drops a client of two consoles once", "");
    }
    JSON_DECREF(route_a)
    JSON_DECREF(route_b)
    return ret;
}

/***************************************************************************
 *  What command-agent tells the agent in `__relays__` (yours: the list, or
 *  NULL when it writes none) for a request of `client` carrying `relays`
 *  (owned, or NULL for none)
 ***************************************************************************/
PRIVATE json_t *forwarded_relays(hgobj gobj, hgobj client, json_t *relays)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("watch-yuno-stats ids=1234"));
    if(relays) {
        json_object_set_new(kw, "__relays__", relays);
    }
    check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
        0, 0, "command-agent watch-yuno-stats");
    json_t *forwarded = agent_request(gobj, "watch-yuno-stats reaches the agent");
    json_t *jn_relays = json_incref(json_object_get(forwarded, "__relays__"));
    JSON_DECREF(forwarded)
    return jn_relays;
}

/***************************************************************************
 *  8. The control center says it relays EV_YUNO_STATS only for a client
 *     that said it takes it: a ycommand through the control center is not
 *     sent readings it cannot handle
 ***************************************************************************/
PRIVATE int test_relays_only_for_who_asks(hgobj gobj, hgobj client)
{
    int ret = 0;

    json_t *jn_relays = forwarded_relays(gobj, client, NULL);
    if(jn_relays) {
        ret += fail(gobj, "a client without __relays__ is said to take EV_YUNO_STATS", "");
    }
    JSON_DECREF(jn_relays)

    jn_relays = forwarded_relays(gobj, client, json_pack("[s]", "EV_SOMETHING_ELSE"));
    if(jn_relays) {
        ret += fail(gobj, "a client whose __relays__ does not name EV_YUNO_STATS is said to take it", "");
    }
    JSON_DECREF(jn_relays)

    jn_relays = forwarded_relays(gobj, client, json_string(EV_YUNO_STATS));
    if(jn_relays) {
        ret += fail(gobj, "a __relays__ that is not a list is taken", "");
    }
    JSON_DECREF(jn_relays)

    jn_relays = forwarded_relays(gobj, client,
        json_pack("[s, s]", "EV_SOMETHING_ELSE", EV_YUNO_STATS));
    json_t *expected = json_pack("[s]", EV_YUNO_STATS);
    if(!jn_relays || !json_equal(jn_relays, expected)) {
        char *s = jn_relays? json_dumps(jn_relays, JSON_COMPACT) : 0;
        ret += fail(gobj, "a client that takes EV_YUNO_STATS: the agent is told only that", s? s : "none");
        if(s) {
            gbmem_free(s);
        }
    }
    JSON_DECREF(expected)
    JSON_DECREF(jn_relays)
    return ret;
}

/***************************************************************************
 *  A request of `client` as the user "viewer", who may read the scenario
 *  "scn" and nothing else (test_authz_checker() of main.c)
 ***************************************************************************/
PRIVATE json_t *viewer_kw(hgobj client)
{
    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "__username__", json_string("viewer"));
    return kw;
}

/*  The line the agent got, compared; the request is taken  */
PRIVATE int check_agent_line(hgobj gobj, const char *expected, const char *what)
{
    json_t *forwarded = agent_request(gobj, what);
    if(!forwarded) {
        return -1;
    }
    int ret = 0;
    const char *line = kw_get_str(gobj, forwarded, "__command__", "", 0);
    if(strcmp(line, expected)!=0) {
        ret = fail(gobj, what, line);
    }
    const char *own[] = {"scenario_id", "node", "keys", "period", "stop", 0};
    for(int i=0; own[i]; i++) {
        if(json_object_get(forwarded, own[i])) {
            ret += fail(gobj, "a parameter of watch-scenario reaches the agent", own[i]);
        }
    }
    JSON_DECREF(forwarded)
    return ret;
}

/***************************************************************************
 *  14. A user who may read ONE scenario: the list has that one only, the
 *      others are refused by id, `command-agent` is refused, and
 *      `watch-scenario` builds the watch of its yunos on a node -- the
 *      line the agent gets is the control center's, not the client's;
 *      `my-permissions` says what the user may do
 ***************************************************************************/
PRIVATE int test_scoped_viewer(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_t *response = gobj_command(priv->cc, "scenarios", viewer_kw(client), client);
    json_t *data = kw_get_list(gobj, response, "data", 0, 0);
    if(kw_get_int(gobj, response, "result", -99, KW_WILD_NUMBER) != 0 ||
            json_array_size(data) != 1 ||
            strcmp(kw_get_str(gobj, json_array_get(data, 0), "id", "", 0), "scn")!=0) {
        char *s = json_dumps(data, JSON_COMPACT);
        ret += fail(gobj, "the viewer's list holds 'scn' only", s? s : "");
        if(s) {
            gbmem_free(s);
        }
    }
    JSON_DECREF(response)

    json_t *kw = viewer_kw(client);
    json_object_set_new(kw, "scenario_id", json_string("string-form-scenario-0001"));
    ret += check_response(gobj, gobj_command(priv->cc, "scenarios", kw, client),
        -1, "no permission to 'read-scenarios'", "another scenario is refused by id");

    kw = viewer_kw(client);
    json_object_set_new(kw, "scenario_id", json_string("scn"));
    ret += check_response(gobj, gobj_command(priv->cc, "scenario-runs", kw, client),
        0, 0, "the viewer reads the runs of 'scn'");

    kw = viewer_kw(client);
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("watch-yuno-stats ids=1234"));
    ret += check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
        -1, "No permission to 'command-agent'", "the viewer may not command an agent");

    kw = viewer_kw(client);
    json_object_set_new(kw, "scenario", scenario_with_step("scn", "resume-generation rate=10"));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        -1, "no permission to 'write-scenarios'", "the viewer may not save 'scn'");

    /*
     *  Who may write "scn" only cannot save another scenario by naming
     *  "scn" beside it: the permission is asked of the id saved, never of
     *  the client's kw
     */
    kw = viewer_kw(client);
    json_object_set_new(kw, "__username__", json_string("writer"));
    json_object_set_new(kw, "scenario_id", json_string("scn"));
    json_object_set_new(kw, "scenario", scenario_with_step("not-scn", "resume-generation rate=10"));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        -1, "no permission to 'write-scenarios'", "a scenario_id beside another scenario is no permission");
    kw = viewer_kw(client);
    json_object_set_new(kw, "__username__", json_string("writer"));
    json_object_set_new(kw, "scenario", scenario_with_step("wr", "resume-generation rate=10"));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        0, "scenario created: wr", "the writer of 'wr' saves it");
    /*  "scn" runs (case 4 started it): writing it would be running it  */
    kw = viewer_kw(client);
    json_object_set_new(kw, "__username__", json_string("writer"));
    json_object_set_new(kw, "scenario", scenario_with_step("scn", "resume-generation rate=10"));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        -1, "scn' runs: saving it needs 'run-scenarios' too", "the writer may not save a running scn");

    /*  Nor read another by naming "scn" in the kw of the whole list  */
    kw = viewer_kw(client);
    json_object_set_new(kw, "scenario_id", json_string(""));
    json_object_set_new(kw, "other", json_string("scn"));
    response = gobj_command(priv->cc, "scenarios", kw, client);
    if(json_array_size(kw_get_list(gobj, response, "data", 0, 0)) != 1) {
        ret += fail(gobj, "the viewer's list, asked with keys of its own, holds 'scn' only", "");
    }
    JSON_DECREF(response)

    kw = viewer_kw(client);
    json_object_set_new(kw, "scenario_id", json_string("scn"));
    json_object_set_new(kw, "node", json_string(AGENT_HOST));
    json_object_set_new(kw, "period", json_integer(1000));
    json_object_set_new(kw, "__relays__", json_pack("[s]", EV_YUNO_STATS));
    ret += check_response(gobj, gobj_command(priv->cc, "watch-scenario", kw, client),
        0, "Command sent to 1 nodes", "the viewer watches 'scn'");
    ret += check_agent_line(gobj, "watch-yuno-stats ids=1234 period=1000",
        "watch-scenario builds the watch of the node's yunos");

    /*
     *  A yuno whose queue is another service's (a gate's __output_side__)
     *  is watched on that service too; a queue_service that is no name is
     *  refused
     */
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", json_pack("{s:s, s:s, s:[{s:s, s:s}]}",
        "id", "qs", "node", AGENT_HOST, "yunos", "id", "1234", "queue_service", "a b"));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        -1, "queue_service: letters", "a queue_service that is no name is refused");
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", json_pack("{s:s, s:s, s:[{s:s, s:s}]}",
        "id", "qs", "node", AGENT_HOST, "yunos", "id", "1234", "queue_service", "__output_side__"));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        0, "scenario created: qs", "a scenario with a queue_service");
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("qs"));
    json_object_set_new(kw, "node", json_string(AGENT_HOST));
    ret += check_response(gobj, gobj_command(priv->cc, "watch-scenario", kw, client),
        0, 0, "watch-scenario of qs");
    ret += check_agent_line(gobj, "watch-yuno-stats ids=1234,1234:__output_side__ period=2000",
        "watch-scenario watches the queue_service too");

    kw = viewer_kw(client);
    json_object_set_new(kw, "scenario_id", json_string("scn"));
    json_object_set_new(kw, "node", json_string(AGENT_HOST));
    json_object_set_new(kw, "keys", json_string("1234"));
    ret += check_response(gobj, gobj_command(priv->cc, "watch-scenario", kw, client),
        0, 0, "the viewer watches some keys of 'scn'");
    ret += check_agent_line(gobj, "watch-yuno-stats ids=1234 period=2000",
        "watch-scenario keys= keeps the keys asked");

    kw = viewer_kw(client);
    json_object_set_new(kw, "scenario_id", json_string("scn"));
    json_object_set_new(kw, "node", json_string(AGENT_HOST));
    json_object_set_new(kw, "keys", json_string("12,123,12345"));
    ret += check_response(gobj, gobj_command(priv->cc, "watch-scenario", kw, client),
        -1, "has no yuno to watch on node", "keys of no yuno of the node");

    kw = viewer_kw(client);
    json_object_set_new(kw, "scenario_id", json_string("scn"));
    json_object_set_new(kw, "node", json_string("another-node"));
    ret += check_response(gobj, gobj_command(priv->cc, "watch-scenario", kw, client),
        -1, "has no yuno to watch on node", "a node of no yuno of the scenario");

    kw = viewer_kw(client);
    json_object_set_new(kw, "scenario_id", json_string("scn"));
    json_object_set_new(kw, "node", json_string(AGENT_HOST));
    json_object_set_new(kw, "stop", json_true());
    ret += check_response(gobj, gobj_command(priv->cc, "watch-scenario", kw, client),
        0, 0, "the viewer stops its watch");
    ret += check_agent_line(gobj, "watch-yuno-stats stop=1", "watch-scenario stop=1");

    kw = viewer_kw(client);
    json_object_set_new(kw, "scenario_id", json_string("string-form-scenario-0001"));
    json_object_set_new(kw, "node", json_string(AGENT_HOST));
    ret += check_response(gobj, gobj_command(priv->cc, "watch-scenario", kw, client),
        -1, "no permission to 'read-scenarios'", "the viewer may not watch another scenario");

    response = gobj_command(priv->cc, "my-permissions", viewer_kw(client), client);
    json_t *expected = json_pack("{s:b, s:b, s:b, s:b, s:b, s:[s], s:b, s:b}",
        "list-agents", 0, "command-agent", 0, "drop-agent", 0, "logout-user", 0,
        "write-tty", 0, "read-scenarios", "scn", "write-scenarios", 0, "run-scenarios", 0
    );
    data = kw_get_dict(gobj, response, "data", 0, 0);
    if(!data || !json_equal(data, expected)) {
        char *s = data? json_dumps(data, JSON_COMPACT) : 0;
        ret += fail(gobj, "my-permissions of the viewer", s? s : "no data");
        if(s) {
            gbmem_free(s);
        }
    }
    JSON_DECREF(expected)
    JSON_DECREF(response)

    response = gobj_command(priv->cc, "my-permissions", client_kw(gobj_name(client)), client);
    data = kw_get_dict(gobj, response, "data", 0, 0);
    const char *key; json_t *v;
    json_object_foreach(data, key, v) {
        if(!json_is_true(v)) {
            ret += fail(gobj, "my-permissions of a user who may do everything", key);
        }
    }
    JSON_DECREF(response)
    return ret;
}

/***************************************************************************
 *  15. What a user may do changed in the users store: a role created,
 *      changed or deleted, a link from a role. The control center tells
 *      its subscribers (EV_PERMISSIONS_CHANGED), which ask my-permissions
 *      again; a user's own record (it changes at every login) and a link
 *      of another topic are not a change of permissions. The store's
 *      events are sent here as C_NODE publishes them.
 ***************************************************************************/
PRIVATE int test_permissions_changed(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    gobj_subscribe_event(priv->cc, EV_PERMISSIONS_CHANGED, 0, gobj);
    priv->permissions_changed = 0;

    struct {
        gobj_event_t event;
        const char *key;
        const char *topic;
        int told;
    } cases[] = {
        {EV_TREEDB_NODE_CREATED,  "topic_name",        "roles", 1},
        {EV_TREEDB_NODE_UPDATED,  "topic_name",        "roles", 1},
        {EV_TREEDB_NODE_DELETED,  "topic_name",        "roles", 1},
        {EV_TREEDB_NODE_LINKED,   "parent_topic_name", "roles", 1},
        {EV_TREEDB_NODE_UNLINKED, "parent_topic_name", "roles", 1},
        {EV_TREEDB_NODE_UPDATED,  "topic_name",        "users", 0},
        {EV_TREEDB_NODE_LINKED,   "parent_topic_name", "users", 0},
        {0}
    };
    for(int i=0; cases[i].event; i++) {
        int before = priv->permissions_changed;
        gobj_send_event(priv->cc, cases[i].event,
            json_pack("{s:s, s:s}", cases[i].key, cases[i].topic, "treedb_name", "treedb_authzs"),
            gobj
        );
        if(priv->permissions_changed - before != cases[i].told) {
            char what[NAME_MAX];
            snprintf(what, sizeof(what), "%s of %s", cases[i].event, cases[i].topic);
            ret += fail(gobj, "EV_PERMISSIONS_CHANGED told for a change of the users store", what);
        }
    }
    gobj_unsubscribe_event(priv->cc, EV_PERMISSIONS_CHANGED, 0, gobj);
    return ret;
}

/***************************************************************************
 *  16. A scenario's max_run: its start schedules its stop, which the
 *      control center runs itself when it is due. A: the scenario "mr"
 *      (max_run 1 s) is saved and started; its stop is due ~1 s after,
 *      `scenarios` says when (`stop_at`); a max_run out of 0..a week is
 *      refused, and while it runs a user who may write it but not run it
 *      cannot save it. B (the next phase, past it): the control center sent the
 *      stop step by itself, answered here as the agent does; the run is
 *      written as the control center's with the reason, and the stop is
 *      no longer scheduled. Every run that ends is told
 *      (EV_SCENARIO_RUN_ENDED).
 ***************************************************************************/
PRIVATE json_t *max_run_scenario(json_int_t max_run)
{
    return json_pack("{s:s, s:s, s:I, s:[{s:s}], s:{s:[{s:s, s:s}], s:[{s:s, s:s}]}}",
        "id", "mr",
        "node", AGENT_HOST,
        "max_run", max_run,
        "yunos", "id", "1234",
        "actions",
            "start", "yuno", "1234", "command", "resume-generation",
            "stop", "yuno", "1234", "command", "pause-generation"
    );
}

PRIVATE json_int_t stop_at_of(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("mr"));
    json_t *response = gobj_command(priv->cc, "scenarios", kw, client);
    json_int_t stop_at = kw_get_int(gobj,
        json_array_get(kw_get_list(gobj, response, "data", 0, 0), 0), "stop_at", 0, KW_WILD_NUMBER);
    JSON_DECREF(response)
    return stop_at;
}

PRIVATE int test_max_run_start(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    gobj_subscribe_event(priv->cc, EV_SCENARIO_RUN_ENDED, 0, gobj);
    priv->runs_ended = 0;

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", max_run_scenario(-1));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        -1, "max_run: seconds", "a negative max_run is refused");
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", max_run_scenario(1));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        0, "scenario created: mr", "a scenario with a max_run");
    if(stop_at_of(gobj, client) != 0) {
        ret += fail(gobj, "a scenario not started has a stop scheduled", "");
    }

    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("mr"));
    json_object_set_new(kw, "action", json_string("start"));
    json_t *response = gobj_command(priv->cc, "run-scenario", kw, client);
    JSON_DECREF(response)
    json_t *step = agent_request(gobj, "the start step of mr goes to the agent");
    if(!step) {
        return ret - 1;
    }
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, 0, "generating");
    JSON_DECREF(step)
    count_received(client, EV_MT_COMMAND_ANSWER, 0);

    /*
     *  It runs, and its max_run will stop it: who may write it but not run
     *  it cannot save it now -- the control center would run the stop steps
     *  that save writes
     */
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "__username__", json_string("writer"));
    json_object_set_new(kw, "scenario", max_run_scenario(1));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        -1, "saving it needs 'run-scenarios' too", "a writer who may not run cannot save a running mr");

    /*
     *  Nor delete it: its delete would take its stop steps, and the stop
     *  its max_run schedules
     */
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "__username__", json_string("writer"));
    json_object_set_new(kw, "scenario_id", json_string("mr"));
    ret += check_response(gobj, gobj_command(priv->cc, "delete-scenario", kw, client),
        -1, "deleting it needs 'run-scenarios' too", "a writer who may not run cannot delete a running mr");

    /*
     *  Nor the scenario "scn", which runs with no max_run (case 4 started
     *  it): a max_run saved now would schedule the stop steps saved now
     */
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "__username__", json_string("writer"));
    json_t *scn = scenario_with_step("scn", "resume-generation rate=10");
    json_object_set_new(scn, "max_run", json_integer(1));
    json_object_set_new(kw, "scenario", scn);
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        -1, "saving it needs 'run-scenarios' too", "a writer cannot give a running scn a max_run");

    priv->max_run_started = (json_int_t)time_in_seconds();
    json_int_t stop_at = stop_at_of(gobj, client);
    if(stop_at < priv->max_run_started || stop_at > priv->max_run_started + 1) {
        char detail[64];
        snprintf(detail, sizeof(detail), "%lld, started %lld", (long long)stop_at,
            (long long)priv->max_run_started);
        ret += fail(gobj, "a start schedules its stop at the end of the start + max_run", detail);
    }
    if(priv->runs_ended != 1) {
        ret += fail(gobj, "the start that ended is told (EV_SCENARIO_RUN_ENDED)", "");
    }
    return ret;
}

PRIVATE int test_max_run_stopped(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_t *step = agent_request(gobj, "the control center sends the stop of mr by itself");
    if(!step) {
        return -1;
    }
    if(!strstr(kw_get_str(gobj, step, "__command__", "", 0), "pause-generation")) {
        ret += fail(gobj, "the stop step of mr", kw_get_str(gobj, step, "__command__", "", 0));
    }
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, 0, "paused");
    JSON_DECREF(step)

    json_t *kw = client_kw(gobj_name(priv->client1));
    json_object_set_new(kw, "scenario_id", json_string("mr"));
    json_t *response = gobj_command(priv->cc, "scenario-runs", kw, priv->client1);
    json_t *newest = json_array_get(kw_get_list(gobj, response, "data", 0, 0), 0);
    if(strcmp(kw_get_str(gobj, newest, "action", "", 0), "stop")!=0 ||
            kw_get_int(gobj, newest, "result", -1, KW_WILD_NUMBER) != 0 ||
            !strstr(kw_get_str(gobj, newest, "comment", "", 0), "max_run of 1 s reached (started by alice)") ||
            strcmp(kw_get_str(gobj, newest, "username", "", 0), gobj_yuno_role_plus_name())!=0) {
        ret += fail(gobj, "the stop of the max_run is a run of the control center, with its reason",
            kw_get_str(gobj, newest, "comment", "", 0));
    }
    JSON_DECREF(response)
    if(stop_at_of(gobj, priv->client1) != 0) {
        ret += fail(gobj, "a stopped scenario has no stop scheduled", "");
    }
    if(priv->runs_ended != 2) {
        ret += fail(gobj, "the stop that ended is told (EV_SCENARIO_RUN_ENDED)", "");
    }
    gobj_unsubscribe_event(priv->cc, EV_SCENARIO_RUN_ENDED, 0, gobj);
    return ret;
}

/***************************************************************************
 *  19. The stop of a max_run whose node is not connected. The scenario
 *      "mrx" (max_run 1 s) starts a yuno on AGENT_HOST and stops one on
 *      GHOST_HOST, whose agent is not connected. A (phase 3): started.
 *      B (phase 4, past the stop): the control center ran the stop, which
 *      did nothing (one run, failed, and a warning), and the stop waits for
 *      a node of it (`stop_waits_node` in `scenarios`), not for a minute:
 *      up to 7.29.1 it was tried every minute, a run written each time, for
 *      as long as the node was down. Saved again as it is, it still waits
 *      (in 7.29.2 the save dropped the wait: due at once, one more failed
 *      run). The agent of GHOST_HOST connects.
 *      C (phase 5): the stop went to it at once; answered, the run is the
 *      second stop, done, and nothing is scheduled any more.
 ***************************************************************************/
PRIVATE int ghost_connects(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->ghost_channel = create_peer("agent-2", priv->input_side);
    priv->ghost_wire = create_peer("agent-2-wire", priv->ghost_channel);

    json_t *kw_srv = json_pack("{s:s, s:s, s:s, s:{s:s, s:{s:[{s:s}]}}}",
        "client_yuno_role", "yuneta_agent",
        "client_yuno_name", GHOST_HOST,
        "client_yuno_service", "agent",
        "identity_card",
            "id", "uuid-" GHOST_HOST,
            "__md_iev__",
                "ievent_gate_stack",
                    "host", GHOST_HOST
    );
    hgobj srv = gobj_create("agent-2", C_IEVENT_SRV, kw_srv, priv->ghost_channel);
    if(!srv) {
        return fail(gobj, "ghost_connects", "cannot create the C_IEVENT_SRV");
    }
    gobj_set_bottom_gobj(srv, priv->ghost_wire);
    gobj_start(srv);
    gobj_change_state(srv, ST_SESSION);

    gobj_send_event(priv->cc, EV_ON_OPEN,
        json_pack("{s:{s:I}}", "__temp__", "channel_gobj", (json_int_t)(uintptr_t)priv->ghost_channel),
        priv->input_side
    );
    return 0;
}

/*  The runs of "mrx" that are a stop, and the newest of them (yours)  */
PRIVATE int mrx_stops(hgobj gobj, json_t **newest_stop)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = client_kw(gobj_name(priv->client1));
    json_object_set_new(kw, "scenario_id", json_string("mrx"));
    json_t *response = gobj_command(priv->cc, "scenario-runs", kw, priv->client1);
    int n = 0;
    *newest_stop = NULL;
    size_t idx; json_t *run;
    json_array_foreach(kw_get_list(gobj, response, "data", 0, 0), idx, run) {
        if(strcmp(kw_get_str(gobj, run, "action", "", 0), "stop")!=0) {
            continue;
        }
        if(!*newest_stop) {
            *newest_stop = json_incref(run);
        }
        n++;
    }
    JSON_DECREF(response)
    return n;
}

PRIVATE json_t *mrx_scenario(void)
{
    return json_pack(
        "{s:s, s:s, s:I, s:[{s:s}, {s:s, s:s}], s:{s:[{s:s, s:s}], s:[{s:s, s:s}]}}",
        "id", "mrx",
        "node", AGENT_HOST,
        "max_run", (json_int_t)1,
        "yunos", "id", "1234", "id", "9999", "node", GHOST_HOST,
        "actions",
            "start", "yuno", "1234", "command", "resume-generation",
            "stop", "yuno", "9999", "command", "pause-generation"
    );
}

PRIVATE int test_max_run_no_node_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj client = priv->client1;
    int ret = 0;

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", mrx_scenario());
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        0, "scenario created: mrx", "a scenario whose stop is on a node not connected");

    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("mrx"));
    json_object_set_new(kw, "action", json_string("start"));
    json_t *response = gobj_command(priv->cc, "run-scenario", kw, client);
    JSON_DECREF(response)
    json_t *step = agent_request(gobj, "the start step of mrx goes to the agent");
    if(!step) {
        return ret - 1;
    }
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, 0, "generating");
    JSON_DECREF(step)
    count_received(client, EV_MT_COMMAND_ANSWER, 0);
    return ret;
}

PRIVATE int test_max_run_no_node_waits(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_t *newest = NULL;
    int n = mrx_stops(gobj, &newest);
    if(n != 1 || kw_get_int(gobj, newest, "result", 0, KW_WILD_NUMBER) >= 0) {
        ret += fail(gobj, "the stop of mrx ran once, and did nothing", "");
    }
    JSON_DECREF(newest)

    json_t *kw = client_kw(gobj_name(priv->client1));
    json_object_set_new(kw, "scenario_id", json_string("mrx"));
    json_t *response = gobj_command(priv->cc, "scenarios", kw, priv->client1);
    json_t *mrx = json_array_get(kw_get_list(gobj, response, "data", 0, 0), 0);
    if(!json_is_true(json_object_get(mrx, "stop_waits_node"))) {
        ret += fail(gobj, "the stop of mrx waits for a node of it (stop_waits_node)", "");
    }
    JSON_DECREF(response)

    /*
     *  Saved again as it is, it goes on waiting: up to 7.29.2 the save
     *  rebuilt its stop from the runs without the wait, due at once, and
     *  one more failed run was written
     */
    kw = client_kw(gobj_name(priv->client1));
    json_object_set_new(kw, "scenario", mrx_scenario());
    json_t *saved = gobj_command(priv->cc, "save-scenario", kw, priv->client1);
    mrx = json_array_get(kw_get_list(gobj, saved, "data", 0, 0), 0);
    if(kw_get_int(gobj, saved, "result", -1, KW_WILD_NUMBER) < 0 ||
            !json_is_true(json_object_get(mrx, "stop_waits_node"))) {
        ret += fail(gobj, "a save of mrx keeps its stop waiting for a node",
            kw_get_str(gobj, saved, "comment", "", 0));
    }
    JSON_DECREF(saved)

    ret += ghost_connects(gobj);
    return ret;
}

PRIVATE int test_max_run_no_node_reached(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_t *received = take_received(priv->ghost_wire);
    size_t nr = json_array_size(received);
    json_t *step = nr? json_incref(json_object_get(json_array_get(received, nr-1), "kw")) : NULL;
    JSON_DECREF(received)
    if(!step) {
        return fail(gobj, "the stop of mrx goes to the agent of its node when it connects",
            "the agent got no request");
    }
    if(!strstr(kw_get_str(gobj, step, "__command__", "", 0), "pause-generation")) {
        ret += fail(gobj, "the stop step of mrx", kw_get_str(gobj, step, "__command__", "", 0));
    }
    json_t *kw = msg_iev_build_response(gobj, 0, json_string("paused"), 0, 0, json_deep_copy(step));
    kw_set_subdict_value(gobj, kw, "__temp__", "channel_gobj",
        json_integer((json_int_t)(uintptr_t)priv->ghost_channel));
    gobj_send_event(priv->cc, EV_MT_COMMAND_ANSWER, kw, priv->input_side);
    JSON_DECREF(step)

    json_t *newest = NULL;
    int n = mrx_stops(gobj, &newest);
    if(n != 2 || kw_get_int(gobj, newest, "result", -1, KW_WILD_NUMBER) != 0) {
        ret += fail(gobj, "the stop of mrx, tried again, is done", "");
    }
    JSON_DECREF(newest)

    kw = client_kw(gobj_name(priv->client1));
    json_object_set_new(kw, "scenario_id", json_string("mrx"));
    json_t *response = gobj_command(priv->cc, "scenarios", kw, priv->client1);
    json_t *mrx = json_array_get(kw_get_list(gobj, response, "data", 0, 0), 0);
    if(json_object_get(mrx, "stop_at") || json_object_get(mrx, "stop_waits_node")) {
        ret += fail(gobj, "a stopped mrx has no stop scheduled", "");
    }
    JSON_DECREF(response)
    return ret;
}

/***************************************************************************
 *  20. The stop of a max_run whose node IS connected, and refuses it (its
 *      yuno restarting): it is tried again in a minute. It does not wait for
 *      an agent to connect: its own is connected, and no other may ever
 *      come. The scenario "mrn" (max_run 1 s) starts and stops a yuno on
 *      AGENT_HOST. A (phase 5): started. B (phase 6, past the stop): the
 *      control center sent the stop step by itself, answered -1 here (one
 *      failed run, and the warning "... tried again in a minute");
 *      `scenarios` says no `stop_waits_node` and a `stop_at` a minute away.
 *      In 7.29.2 it waited for an agent to connect, so the safety stop was
 *      never tried again. A stop by the user then stops it: nothing is
 *      scheduled any more.
 ***************************************************************************/
PRIVATE json_t *mrn_scenario_of(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = client_kw(gobj_name(priv->client1));
    json_object_set_new(kw, "scenario_id", json_string("mrn"));
    json_t *response = gobj_command(priv->cc, "scenarios", kw, priv->client1);
    json_t *mrn = json_incref(json_array_get(kw_get_list(gobj, response, "data", 0, 0), 0));
    JSON_DECREF(response)
    return mrn;
}

PRIVATE int test_max_run_node_refuses_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj client = priv->client1;
    int ret = 0;

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", json_pack(
        "{s:s, s:s, s:I, s:[{s:s}], s:{s:[{s:s, s:s}], s:[{s:s, s:s}]}}",
        "id", "mrn",
        "node", AGENT_HOST,
        "max_run", (json_int_t)1,
        "yunos", "id", "1234",
        "actions",
            "start", "yuno", "1234", "command", "resume-generation",
            "stop", "yuno", "1234", "command", "pause-generation"
    ));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        0, "scenario created: mrn", "a scenario whose stop is on a node connected");

    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("mrn"));
    json_object_set_new(kw, "action", json_string("start"));
    json_t *response = gobj_command(priv->cc, "run-scenario", kw, client);
    JSON_DECREF(response)
    json_t *step = agent_request(gobj, "the start step of mrn goes to the agent");
    if(!step) {
        return ret - 1;
    }
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, 0, "generating");
    JSON_DECREF(step)
    count_received(client, EV_MT_COMMAND_ANSWER, 0);
    return ret;
}

PRIVATE int test_max_run_node_refuses_retried(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj client = priv->client1;
    int ret = 0;

    json_t *step = agent_request(gobj, "the control center sends the stop of mrn by itself");
    if(!step) {
        return -1;
    }
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, -1, "the yuno is restarting");
    JSON_DECREF(step)

    json_int_t now = (json_int_t)time_in_seconds();
    json_t *mrn = mrn_scenario_of(gobj);
    json_int_t stop_at = kw_get_int(gobj, mrn, "stop_at", 0, KW_WILD_NUMBER);
    if(json_object_get(mrn, "stop_waits_node")) {
        ret += fail(gobj, "a stop refused on a node connected does not wait for a node", "");
    }
    if(stop_at < now + 59 || stop_at > now + 60) {
        char detail[64];
        snprintf(detail, sizeof(detail), "%lld, now %lld", (long long)stop_at, (long long)now);
        ret += fail(gobj, "a stop refused on a node connected is tried again in a minute", detail);
    }
    JSON_DECREF(mrn)

    /*  Stopped by the user: nothing is scheduled any more  */
    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("mrn"));
    json_object_set_new(kw, "action", json_string("stop"));
    json_t *response = gobj_command(priv->cc, "run-scenario", kw, client);
    JSON_DECREF(response)
    step = agent_request(gobj, "the stop step of mrn, by the user, goes to the agent");
    if(!step) {
        return ret - 1;
    }
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, 0, "paused");
    JSON_DECREF(step)
    count_received(client, EV_MT_COMMAND_ANSWER, 0);

    mrn = mrn_scenario_of(gobj);
    if(json_object_get(mrn, "stop_at") || json_object_get(mrn, "stop_waits_node")) {
        ret += fail(gobj, "a stopped mrn has no stop scheduled", "");
    }
    JSON_DECREF(mrn)
    return ret;
}

/***************************************************************************
 *  17. A stop, a pause or a report asks every yuno at once, and waits for
 *      them all; a start or a resume goes in order and ends at the first
 *      step that fails. The scenario "par" has three yunos on the agent,
 *      one step each in its start, stop and pause.
 *      A (case 0): the start, its second step failing: the third is not
 *      sent, the run fails there. The stop: the three steps are sent at
 *      once, `scenarios` says the run in flight (`run_in_flight`), the
 *      steps are answered out of order and the second fails; the run waits
 *      for the third, and ends naming only the one that failed, the others
 *      recorded with their result 0. B (the next phase, after case 16's):
 *      the pause, with `run_step_timeout` set to PAR_STEP_TIMEOUT_MS and
 *      its second step never answered: in flight while it waits; C (the
 *      phase after, once it is not in flight): the step not answered is
 *      named, and said once (a warning, in the expected logs).
 ***************************************************************************/
#define PAR_SCENARIO            "par"
#define PAR_STEP_TIMEOUT_MS     300
#define PAR_DEFAULT_TIMEOUT_MS  30000   // run_step_timeout's default, put back
#define PAR_WAIT_MS             5000    // the most phase C waits for the run to end

PRIVATE json_t *parallel_scenario(void)
{
    const char *yunos[] = {"1234", "1235", "1236", 0};
    struct {
        const char *action;
        const char *command;
    } defs[] = {
        {"start",   "resume-generation"},
        {"stop",    "pause-generation"},
        {"pause",   "pause-generation"},
        {0, 0}
    };

    json_t *jn_yunos = json_array();
    for(int y=0; yunos[y]; y++) {
        json_array_append_new(jn_yunos, json_pack("{s:s}", "id", yunos[y]));
    }
    json_t *actions = json_object();
    for(int a=0; defs[a].action; a++) {
        json_t *steps = json_array();
        for(int y=0; yunos[y]; y++) {
            json_array_append_new(steps, json_pack("{s:s, s:s}",
                "yuno", yunos[y],
                "command", defs[a].command
            ));
        }
        json_object_set_new(actions, defs[a].action, steps);
    }
    return json_pack("{s:s, s:s, s:o, s:o}",
        "id", PAR_SCENARIO,
        "node", AGENT_HOST,
        "yunos", jn_yunos,
        "actions", actions
    );
}

/*  The kw of every request the agent got since the last time (yours)  */
PRIVATE json_t *agent_requests(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *received = take_received(priv->agent_wire);
    json_t *requests = json_array();
    size_t idx; json_t *jn;
    json_array_foreach(received, idx, jn) {
        json_array_append(requests, json_object_get(jn, "kw"));
    }
    JSON_DECREF(received)
    return requests;
}

/*  What the agent got since the last time, left there (not yours)  */
PRIVATE json_t *agent_requests_peek(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    return gobj_read_user_data(priv->agent_wire, "received");
}

/*  The `run_in_flight` `scenarios` says of "par" (yours), or NULL  */
PRIVATE json_t *par_run_in_flight(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string(PAR_SCENARIO));
    json_t *response = gobj_command(priv->cc, "scenarios", kw, client);
    json_t *in_flight = json_object_get(
        json_array_get(kw_get_list(gobj, response, "data", 0, 0), 0), "run_in_flight");
    json_incref(in_flight);
    JSON_DECREF(response)
    return in_flight;
}

/*  The newest run of "par", as `scenario-runs` says it (yours), or NULL  */
PRIVATE json_t *par_newest_run(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string(PAR_SCENARIO));
    json_t *response = gobj_command(priv->cc, "scenario-runs", kw, client);
    json_t *newest = json_incref(json_array_get(kw_get_list(gobj, response, "data", 0, 0), 0));
    JSON_DECREF(response)
    return newest;
}

/*
 *  The requests of a run in parallel: one per step, all at once, each
 *  marked with its step and with the line of its yuno
 */
PRIVATE int check_parallel_requests(hgobj gobj, json_t *requests, const char *command, const char *what)
{
    int ret = 0;
    const char *yunos[] = {"id=1234", "id=1235", "id=1236", 0};

    if(json_array_size(requests) != 3) {
        char detail[64];
        snprintf(detail, sizeof(detail), "%d requests, expected 3", (int)json_array_size(requests));
        return fail(gobj, what, detail);
    }
    for(int i=0; yunos[i]; i++) {
        json_t *request = json_array_get(requests, (size_t)i);
        const char *line = kw_get_str(gobj, request, "__command__", "", 0);
        if(!strstr(line, yunos[i]) || !strstr(line, command) ||
                kw_get_int(gobj, request, "__md_iev__`cc_step", -1, KW_WILD_NUMBER) != i) {
            ret += fail(gobj, what, line);
        }
    }
    return ret;
}

/*  The result of each step of a run record: `results` -1, 0, or 1 (no result)  */
PRIVATE int check_step_results(hgobj gobj, json_t *run, const int results[3], const char *what)
{
    int ret = 0;
    json_t *steps = kw_get_list(gobj, run, "steps", 0, 0);
    if(json_array_size(steps) != 3) {
        return fail(gobj, what, "the run record has not its 3 steps");
    }
    for(int i=0; i<3; i++) {
        json_t *step = json_array_get(steps, (size_t)i);
        char detail[64];
        snprintf(detail, sizeof(detail), "step %d", i);
        if(results[i] > 0) {
            if(kw_has_key(step, "result")) {
                ret += fail(gobj, what, detail);
            }
        } else if(!kw_has_key(step, "result") ||
                kw_get_int(gobj, step, "result", -99, KW_WILD_NUMBER) != results[i]) {
            ret += fail(gobj, what, detail);
        }
    }
    return ret;
}

/*  A: the start stays in order; the stop goes in parallel  */
PRIVATE int test_parallel_stop(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", parallel_scenario());
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        0, "scenario created: " PAR_SCENARIO, "a scenario of three yunos");
    count_received(client, EV_MT_COMMAND_ANSWER, 0);

    /*
     *  The start: its second step fails, the third is not sent
     */
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string(PAR_SCENARIO));
    json_object_set_new(kw, "action", json_string("start"));
    json_t *response = gobj_command(priv->cc, "run-scenario", kw, client);
    if(response) {
        ret += fail(gobj, "the start of par answers when the run is over",
            kw_get_str(gobj, response, "comment", "", 0));
        JSON_DECREF(response)
        return ret;
    }
    const char *start_yunos[] = {"id=1234", "id=1235", 0};
    const int start_results[] = {0, -1};
    for(int i=0; start_yunos[i]; i++) {
        json_t *requests = agent_requests(gobj);
        json_t *step = json_array_get(requests, 0);
        if(json_array_size(requests) != 1 ||
                !strstr(kw_get_str(gobj, step, "__command__", "", 0), start_yunos[i])) {
            ret += fail(gobj, "a start sends its steps one after the other", start_yunos[i]);
            JSON_DECREF(requests)
            return ret;
        }
        agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, start_results[i],
            start_results[i] < 0? "the yuno refused" : "resumed");
        JSON_DECREF(requests)
    }
    json_t *requests = agent_requests(gobj);
    if(json_array_size(requests) != 0) {
        ret += fail(gobj, "a start sends no step after the one that failed",
            kw_get_str(gobj, json_array_get(requests, 0), "__command__", "", 0));
    }
    JSON_DECREF(requests)
    if(count_received(client, EV_MT_COMMAND_ANSWER, "start failed, 1 of 3 steps done: a step failed") != 1) {
        ret += fail(gobj, "a start ends at the step that fails", "");
    }
    json_t *run = par_newest_run(gobj, client);
    const int start_recorded[3] = {0, -1, 1};
    if(strcmp(kw_get_str(gobj, run, "action", "", 0), "start")!=0 ||
            kw_get_int(gobj, run, "result", 0, KW_WILD_NUMBER) != -1) {
        ret += fail(gobj, "the start that failed is written, failed",
            kw_get_str(gobj, run, "comment", "", 0));
    }
    ret += check_step_results(gobj, run, start_recorded, "the steps of the start: done, failed, not sent");
    JSON_DECREF(run)

    /*
     *  The stop: every step at once
     */
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string(PAR_SCENARIO));
    json_object_set_new(kw, "action", json_string("stop"));
    response = gobj_command(priv->cc, "run-scenario", kw, client);
    if(response) {
        ret += fail(gobj, "the stop of par answers when the run is over",
            kw_get_str(gobj, response, "comment", "", 0));
        JSON_DECREF(response)
        return ret;
    }
    requests = agent_requests(gobj);
    if(check_parallel_requests(gobj, requests, "pause-generation",
            "a stop sends every step at once") < 0) {
        JSON_DECREF(requests)
        return ret - 1;
    }
    char run_id[NAME_MAX];
    snprintf(run_id, sizeof(run_id), "%s",
        kw_get_str(gobj, json_array_get(requests, 0), "__md_iev__`cc_run", "", 0));

    json_t *in_flight = par_run_in_flight(gobj, client);
    if(!in_flight ||
            strcmp(kw_get_str(gobj, in_flight, "id", "", 0), run_id)!=0 ||
            strcmp(kw_get_str(gobj, in_flight, "action", "", 0), "stop")!=0 ||
            strcmp(kw_get_str(gobj, in_flight, "username", "", 0), "alice")!=0 ||
            kw_get_int(gobj, in_flight, "started_at", 0, KW_WILD_NUMBER) <= 0) {
        ret += fail(gobj, "scenarios says the run in flight", run_id);
    }
    JSON_DECREF(in_flight)

    /*
     *  Answered out of order; the one that fails does not end the run
     */
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, json_array_get(requests, 2), 0, "paused");
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, json_array_get(requests, 1), -1, "the yuno refused");
    if(count_received(client, EV_MT_COMMAND_ANSWER, 0) != 0) {
        ret += fail(gobj, "a stop waits for every step, also after one failed", run_id);
    }
    in_flight = par_run_in_flight(gobj, client);
    if(!in_flight) {
        ret += fail(gobj, "a stop with a step waiting is in flight", run_id);
    }
    JSON_DECREF(in_flight)
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, json_array_get(requests, 0), 0, "paused");
    JSON_DECREF(requests)

    const char *failure =
        "stop failed, 2 of 3 steps done: 1 of 3 steps failed; "
        "step 1 (command-yuno id=1235 command=pause-generation): the yuno refused";
    if(count_received(client, EV_MT_COMMAND_ANSWER, failure) != 1) {
        ret += fail(gobj, "a stop ends when every step answered, naming the one that failed", failure);
    }
    in_flight = par_run_in_flight(gobj, client);
    if(in_flight) {
        ret += fail(gobj, "scenarios says no run in flight once it ended",
            kw_get_str(gobj, in_flight, "id", "", 0));
    }
    JSON_DECREF(in_flight)

    run = par_newest_run(gobj, client);
    const char *comment = kw_get_str(gobj, run, "comment", "", 0);
    const int stop_recorded[3] = {0, -1, 0};
    if(strcmp(kw_get_str(gobj, run, "id", "", 0), run_id)!=0 ||
            strcmp(kw_get_str(gobj, run, "action", "", 0), "stop")!=0 ||
            kw_get_int(gobj, run, "result", 0, KW_WILD_NUMBER) != -1 ||
            !strstr(comment, failure) ||
            strstr(comment, "step 0 (") || strstr(comment, "step 2 (")) {
        ret += fail(gobj, "the stop is written failed, naming only the step that failed", comment);
    }
    ret += check_step_results(gobj, run, stop_recorded, "the steps of the stop: each its own answer");
    JSON_DECREF(run)
    return ret;
}

/*  B: a pause, its second step never answered: in flight while it waits  */
PRIVATE int test_parallel_unanswered_sent(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;
    hgobj client = priv->client1;

    gobj_write_integer_attr(priv->cc, "run_step_timeout", PAR_STEP_TIMEOUT_MS);
    count_received(client, EV_MT_COMMAND_ANSWER, 0);

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string(PAR_SCENARIO));
    json_object_set_new(kw, "action", json_string("pause"));
    json_t *response = gobj_command(priv->cc, "run-scenario", kw, client);
    if(response) {
        ret += fail(gobj, "the pause of par answers when the run is over",
            kw_get_str(gobj, response, "comment", "", 0));
        JSON_DECREF(response)
        return ret;
    }
    json_t *requests = agent_requests(gobj);
    if(check_parallel_requests(gobj, requests, "pause-generation",
            "a pause sends every step at once") < 0) {
        JSON_DECREF(requests)
        return ret - 1;
    }
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, json_array_get(requests, 0), 0, "paused");
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, json_array_get(requests, 2), 0, "paused");

    json_t *in_flight = par_run_in_flight(gobj, client);
    if(!in_flight ||
            strcmp(kw_get_str(gobj, in_flight, "id", "", 0),
                kw_get_str(gobj, json_array_get(requests, 0), "__md_iev__`cc_run", "", 0))!=0 ||
            strcmp(kw_get_str(gobj, in_flight, "action", "", 0), "pause")!=0) {
        ret += fail(gobj, "scenarios says the run in flight while a step is not answered", "");
    }
    JSON_DECREF(in_flight)
    JSON_DECREF(requests)
    if(count_received(client, EV_MT_COMMAND_ANSWER, 0) != 0) {
        ret += fail(gobj, "a pause waits for the step not answered yet", "");
    }
    return ret;
}

/*  C: past the deadline, the step not answered is named  */
PRIVATE int test_parallel_unanswered_ended(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;
    hgobj client = priv->client1;

    gobj_write_integer_attr(priv->cc, "run_step_timeout", PAR_DEFAULT_TIMEOUT_MS);

    char failure[256];
    snprintf(failure, sizeof(failure),
        "pause failed, 2 of 3 steps done: 1 of 3 steps failed; "
        "step 1 (command-yuno id=1235 command=pause-generation): not answered in %d ms",
        PAR_STEP_TIMEOUT_MS);
    if(count_received(client, EV_MT_COMMAND_ANSWER, failure) != 1) {
        ret += fail(gobj, "a pause ends at its deadline, naming the step not answered", failure);
    }
    json_t *run = par_newest_run(gobj, client);
    const int pause_recorded[3] = {0, -1, 0};
    if(strcmp(kw_get_str(gobj, run, "action", "", 0), "pause")!=0 ||
            kw_get_int(gobj, run, "result", 0, KW_WILD_NUMBER) != -1 ||
            !strstr(kw_get_str(gobj, run, "comment", "", 0), failure)) {
        ret += fail(gobj, "the pause is written failed, naming the step not answered",
            kw_get_str(gobj, run, "comment", "", 0));
    }
    ret += check_step_results(gobj, run, pause_recorded, "the steps of the pause: answered, not answered");
    JSON_DECREF(run)
    return ret;
}

/***************************************************************************
 *  18. The switch of a yuno: `run-scenario action=on|off scenario_yuno=`.
 *      The scenario "sw": a generator "sim" with a switch (on: two steps,
 *      the second naming no yuno, so it goes to "sim"; off: two steps) and
 *      a gate, whose start and stop are one step on the gate. A switch
 *      without its off is refused by save-scenario, and so is a step of it
 *      naming no yuno of the scenario. On while the scenario is stopped is
 *      refused, and so are on without a scenario_yuno and a scenario_yuno
 *      with start. Started: on sends its steps in order (to "sim", the
 *      line of its yuno and its service), on of a yuno with no switch is
 *      refused, its answer names the yuno, and
 *      the scenario still runs (on again is not refused for the state);
 *      off sends both at once. Stopped: on is refused again.
 ***************************************************************************/
#define SW_SCENARIO "sw"

PRIVATE json_t *switch_scenario(BOOL with_off, const char *on_yuno)
{
    json_t *jn_switch = json_pack("{s:[{s:s, s:s}, {s:s}]}",
        "on",
            "yuno", on_yuno, "command", "set-controllers controllers=5",
            "command", "resume-generation"
    );
    if(with_off) {
        json_object_set_new(jn_switch, "off", json_pack("[{s:s}, {s:s}]",
            "command", "pause-generation",
            "command", "set-controllers controllers=0"
        ));
    }
    return json_pack("{s:s, s:s, s:[{s:s, s:s, s:s, s:o}, {s:s, s:s}], s:{s:[{s:s, s:s}], s:[{s:s, s:s}]}}",
        "id", SW_SCENARIO,
        "node", AGENT_HOST,
        "yunos",
            "key", "sim", "id", "pp1", "service", "sim_controllers", "switch", jn_switch,
            "key", "gate", "id", "2120",
        "actions",
            "start", "yuno", "gate", "command", "play-yuno",
            "stop", "yuno", "gate", "command", "pause-yuno"
    );
}

PRIVATE json_t *run_kw(hgobj client, const char *action, const char *scenario_yuno)
{
    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string(SW_SCENARIO));
    json_object_set_new(kw, "action", json_string(action));
    if(scenario_yuno) {
        json_object_set_new(kw, "scenario_yuno", json_string(scenario_yuno));
    }
    return kw;
}

/*  Run `action` of "sw" whose steps are answered one by one, each 0  */
PRIVATE int run_serial(hgobj gobj, hgobj client, const char *action, const char *scenario_yuno,
    const char **lines, const char *what)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_t *response = gobj_command(priv->cc, "run-scenario", run_kw(client, action, scenario_yuno), client);
    if(response) {
        ret += fail(gobj, what, kw_get_str(gobj, response, "comment", "", 0));
        JSON_DECREF(response)
        return ret;
    }
    for(int i=0; lines[i]; i++) {
        json_t *requests = agent_requests(gobj);
        json_t *step = json_array_get(requests, 0);
        if(json_array_size(requests) != 1 ||
                strcmp(kw_get_str(gobj, step, "__command__", "", 0), lines[i])!=0) {
            ret += fail(gobj, what, step? kw_get_str(gobj, step, "__command__", "", 0) : "no step");
            JSON_DECREF(requests)
            return ret;
        }
        agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, 0, "done");
        JSON_DECREF(requests)
    }
    return ret;
}

PRIVATE int test_switch(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_t *left = agent_requests(gobj);    // what the cases before left
    JSON_DECREF(left)
    count_received(client, EV_MT_COMMAND_ANSWER, 0);

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", switch_scenario(FALSE, "sim"));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        -1, "yunos[0]: switch: {on: [steps], off: [steps]}", "a switch without its off is refused");
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", switch_scenario(TRUE, "nobody"));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        -1, "yunos[0].switch.on[0]: yuno: a key of its yunos", "a switch step of no yuno is refused");
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", switch_scenario(TRUE, "sim"));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        0, "scenario created: " SW_SCENARIO, "a scenario with a switch");

    ret += check_response(gobj, gobj_command(priv->cc, "run-scenario", run_kw(client, "on", "sim"), client),
        -1, "a switch is turned on only while its scenario runs", "on while stopped is refused");
    ret += check_response(gobj, gobj_command(priv->cc, "run-scenario", run_kw(client, "on", NULL), client),
        -1, "scenario_yuno goes with on and off", "on without a scenario_yuno is refused");
    ret += check_response(gobj, gobj_command(priv->cc, "run-scenario", run_kw(client, "start", "sim"), client),
        -1, "scenario_yuno goes with on and off", "a scenario_yuno with start is refused");
    if(json_array_size(agent_requests_peek(gobj)) != 0) {
        ret += fail(gobj, "nothing refused reaches the agent", "");
    }

    const char *start_lines[] = {"command-yuno id=2120 command=play-yuno", 0};
    ret += run_serial(gobj, client, "start", NULL, start_lines, "the start of sw");
    count_received(client, EV_MT_COMMAND_ANSWER, 0);
    ret += check_response(gobj, gobj_command(priv->cc, "run-scenario", run_kw(client, "on", "gate"), client),
        -1, "yuno 'gate' has no switch", "on of a yuno with no switch is refused");

    const char *on_lines[] = {
        "command-yuno id=pp1 service=sim_controllers command=set-controllers controllers=5",
        "command-yuno id=pp1 service=sim_controllers command=resume-generation",
        0
    };
    ret += run_serial(gobj, client, "on", "sim", on_lines, "on sends its steps in order, to its yuno");
    if(count_received(client, EV_MT_COMMAND_ANSWER, ": on sim done, 2 steps") != 1) {
        ret += fail(gobj, "the answer of on names its yuno", "");
    }

    /*  The scenario still runs: on is refused for no state, only for the run in flight  */
    json_t *response = gobj_command(priv->cc, "run-scenario", run_kw(client, "off", "sim"), client);
    if(response) {
        ret += fail(gobj, "off of a running scenario runs", kw_get_str(gobj, response, "comment", "", 0));
        JSON_DECREF(response)
        return ret;
    }
    json_t *requests = agent_requests(gobj);
    if(json_array_size(requests) != 2 ||
            !strstr(kw_get_str(gobj, json_array_get(requests, 0), "__command__", "", 0), "pause-generation") ||
            !strstr(kw_get_str(gobj, json_array_get(requests, 1), "__command__", "", 0), "controllers=0")) {
        ret += fail(gobj, "off sends its steps at once", "");
    }
    ret += check_response(gobj, gobj_command(priv->cc, "run-scenario", run_kw(client, "on", "sim"), client),
        -1, "a run is in flight", "one run at a time, a switch too");
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, json_array_get(requests, 1), 0, "done");
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, json_array_get(requests, 0), 0, "done");
    JSON_DECREF(requests)
    if(count_received(client, EV_MT_COMMAND_ANSWER, ": off sim done, 2 steps") != 1) {
        ret += fail(gobj, "off ends when both steps answered", "");
    }

    /*  The switch runs left the scenario running: on again goes  */
    ret += run_serial(gobj, client, "on", "sim", on_lines, "a switch does not change the state");
    count_received(client, EV_MT_COMMAND_ANSWER, 0);

    const char *stop_lines[] = {"command-yuno id=2120 command=pause-yuno", 0};
    response = gobj_command(priv->cc, "run-scenario", run_kw(client, "stop", NULL), client);
    JSON_DECREF(response)
    requests = agent_requests(gobj);
    if(json_array_size(requests) != 1 ||
            strcmp(kw_get_str(gobj, json_array_get(requests, 0), "__command__", "", 0), stop_lines[0])!=0) {
        ret += fail(gobj, "the stop of sw", "");
    } else {
        agent_sends(gobj, EV_MT_COMMAND_ANSWER, json_array_get(requests, 0), 0, "done");
    }
    JSON_DECREF(requests)
    count_received(client, EV_MT_COMMAND_ANSWER, 0);
    ret += check_response(gobj, gobj_command(priv->cc, "run-scenario", run_kw(client, "on", "sim"), client),
        -1, "a switch is turned on only while its scenario runs", "on once stopped is refused");
    return ret;
}

/***************************************************************************
 *  9. Who gets what the agent sends back when it names no web client:
 *     only one of this yuno's own ievent links (C_IEVENT_CLI), the one the
 *     request came in by -- never a local service named in a hop below
 *     this control center's, nor one that is not a link
 ***************************************************************************/
PRIVATE int test_answer_to_local_requesters(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    hgobj local = gobj_find_service("authz", TRUE);

    /*
     *  The agent's route: this control center's hop names nobody, the
     *  next one (the client's) a local service that listens
     */
    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("list-yunos"));
    check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
        0, 0, "command-agent");
    json_t *request = agent_request(gobj, "command-agent reaches the agent");
    if(!request) {
        return -1;
    }
    json_t *jn_stack = kw_get_list(gobj, request, "__md_iev__`ievent_gate_stack", 0, 0);
    json_object_set_new(json_array_get(jn_stack, 0), "src_service", json_string("nobody-here"));
    json_object_set_new(json_array_get(jn_stack, 1), "dst_service", json_string("authz"));
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, request, 0, "routed to the next hop");
    agent_sends(gobj, EV_TTY_DATA, request, 0, "tty routed to nobody");
    agent_sends(gobj, EV_TTY_DATA, request, 0, "tty routed to nobody");

    /*
     *  This control center's hop names a local service that is not a link
     */
    json_object_set_new(json_array_get(jn_stack, 0), "src_service", json_string("authz"));
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, request, 0, "routed to a local service");
    JSON_DECREF(request)

    json_t *received = take_received(local);
    if(json_array_size(received) != 0) {
        char *s = json_dumps(received, JSON_COMPACT);
        ret += fail(gobj, "an answer of the agent reached a local service it named", s);
        gbmem_free(s);
    }
    JSON_DECREF(received)
    if(count_received(client, EV_MT_COMMAND_ANSWER, 0) != 0) {
        ret += fail(gobj, "a rerouted answer reached the client", "");
    }

    /*
     *  This yuno's link to its own agent (a C_IEVENT_CLI gives itself as
     *  the src of a command it dispatches), in session
     */
    hgobj uplink = gobj_create_service("cc_uplink", C_IEVENT_CLI, 0, gobj);
    hgobj uplink_wire = create_peer("cc_uplink_wire", uplink);
    gobj_set_bottom_gobj(uplink, uplink_wire);
    gobj_change_state(uplink, ST_SESSION);

    /*
     *  The agent's route names that link, but the link never asked this
     *  agent anything: dropped (a C_IEVENT_CLI is not believed by name)
     */
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("list-yunos"));
    check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
        0, 0, "command-agent");
    request = agent_request(gobj, "command-agent reaches the agent");
    if(request) {
        jn_stack = kw_get_list(gobj, request, "__md_iev__`ievent_gate_stack", 0, 0);
        json_object_set_new(json_array_get(jn_stack, 0), "src_service", json_string("cc_uplink"));
        agent_sends(gobj, EV_MT_COMMAND_ANSWER, request, 0, "routed to a link that did not ask");
        JSON_DECREF(request)
    }
    if(count_received(uplink_wire, EV_MT_COMMAND_ANSWER, 0) != 0) {
        ret += fail(gobj, "an answer reached a link that never asked the agent", "");
    }

    /*
     *  A request that came in by that link: the answer goes back by it
     */

    kw = json_pack("{s:s, s:s, s:s, s:{s:[{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}]}}",
        "__username__", "yuneta",
        "agent_id", AGENT_HOST,
        "cmd2agent", "list-yunos",
        "__md_iev__",
            "ievent_gate_stack",
                "dst_yuno", "",
                "dst_role", "controlcenter",
                "dst_service", "controlcenter",
                "src_yuno", "",
                "src_role", "yuneta_agent",
                "src_service", "input-1",
                "user", "yuneta",
                "host", "cc-node"
    );
    check_response(gobj, gobj_command(priv->cc, "command-agent", kw, uplink),
        0, 0, "command-agent by the link to the local agent");
    request = agent_request(gobj, "command-agent by the link reaches the agent");
    if(request) {
        agent_sends(gobj, EV_MT_COMMAND_ANSWER, request, 0, "via the local agent");
    }
    if(count_received(uplink_wire, EV_MT_COMMAND_ANSWER, "via the local agent") != 1) {
        ret += fail(gobj, "the answer goes back by the link the request came in by", "");
    }

    /*
     *  The agent's connection closes: what it asked for the link is
     *  forgotten with it
     */
    agent_closes(gobj);
    if(request) {
        agent_sends(gobj, EV_MT_COMMAND_ANSWER, request, 0, "after the agent's close");
        JSON_DECREF(request)
    }
    if(count_received(uplink_wire, EV_MT_COMMAND_ANSWER, 0) != 0) {
        ret += fail(gobj, "an answer for the link reached it after the agent's close", "");
    }
    gobj_stop(uplink_wire);
    gobj_destroy(uplink);
    return ret;
}

/***************************************************************************
 *  10. A client that connects, sends one event only an agent sends, and
 *      leaves, in a loop: the capped warning is not said once per loop
 *      (the counts are said when their minute ends, or at the stop)
 ***************************************************************************/
PRIVATE int test_inject_in_a_loop(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int i=0; i<5; i++) {
        client_opens(gobj, client);
        json_t *kw = json_pack("{s:i, s:s, s:{s:I}}",
            "result", 0,
            "comment", "injected in a loop",
            "__temp__",
                "channel_gobj", (json_int_t)(uintptr_t)client
        );
        gobj_send_event(priv->cc, EV_MT_COMMAND_ANSWER, kw, priv->top_side);
        client_closes(gobj, client);
    }
    client_opens(gobj, client);
    return 0;   // the expected logs say it
}

/***************************************************************************
 *  13. The message counters and their rates. The rate tick is set to
 *      RATES_TICK_MS (it restarts now), the stats reset, and 100
 *      command-agent round trips sent at once (a request in from a client
 *      and out to the agent; the answer in from the agent and out to the
 *      client): +200 each way, all inside the first tick.
 ***************************************************************************/
#define RATES_ROUND_TRIPS   100
#define RATES_TICK_MS       1500    // not whole seconds: a rate divided by them fails

PRIVATE int test_rates_traffic(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    if(gobj_read_integer_attr(priv->cc, "txMsgs") <= 0 ||
            gobj_read_integer_attr(priv->cc, "rxMsgs") <= 0) {
        ret += fail(gobj, "the message counters do not count the relayed traffic", "");
    }

    gobj_write_integer_attr(priv->cc, "timeout", RATES_TICK_MS);
    json_t *jn_stats = gobj_stats(priv->cc, "__reset__", json_object(), gobj);
    JSON_DECREF(jn_stats)

    hgobj client = priv->client1;
    for(int i=0; i<RATES_ROUND_TRIPS; i++) {
        json_t *kw = client_kw(gobj_name(client));
        json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
        json_object_set_new(kw, "cmd2agent", json_string("list-yunos"));
        json_t *response = gobj_command(priv->cc, "command-agent", kw, client);
        JSON_DECREF(response)
        json_t *request = agent_request(gobj, "command-agent reaches the agent");
        if(request) {
            agent_sends(gobj, EV_MT_COMMAND_ANSWER, request, 0, "rates");
            JSON_DECREF(request)
        }
    }
    if(count_received(client, EV_MT_COMMAND_ANSWER, "rates") != RATES_ROUND_TRIPS) {
        ret += fail(gobj, "the round trips of the rates test", "");
    }
    json_int_t tx = gobj_read_integer_attr(priv->cc, "txMsgs");
    json_int_t rx = gobj_read_integer_attr(priv->cc, "rxMsgs");
    if(tx != 2*RATES_ROUND_TRIPS || rx != 2*RATES_ROUND_TRIPS) {
        char detail[80];
        snprintf(detail, sizeof(detail), "tx %lld rx %lld, expected %d each",
            (long long)tx, (long long)rx, 2*RATES_ROUND_TRIPS);
        ret += fail(gobj, "the counters count two messages per round trip each way", detail);
    }
    return ret;
}

/***************************************************************************
 *  13. More than two ticks later. The tick after the burst measured its
 *      exact interval, at least RATES_TICK_MS (a C_TIMER never fires
 *      early), so its rate is at most 200*1000/RATES_TICK_MS, and at least
 *      200*1000/(1.5*RATES_TICK_MS) unless the loop stalled more than half a
 *      tick. The maxima hold it, whoever read or not. A rate computed by
 *      the reading over whole seconds gives 200/3 = 66 here (read 3.5 s
 *      later), under the bound. Then stats=__reset__ zeroes counters, rates
 *      and maxima.
 ***************************************************************************/
PRIVATE int test_rates_read(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_int_t high = (2*RATES_ROUND_TRIPS)*1000/RATES_TICK_MS;
    json_int_t low = (2*RATES_ROUND_TRIPS)*1000/(RATES_TICK_MS*3/2);
    json_int_t maxtx = gobj_read_integer_attr(priv->cc, "maxtxMsgsec");
    json_int_t maxrx = gobj_read_integer_attr(priv->cc, "maxrxMsgsec");
    if(maxtx > high || maxtx < low || maxrx > high || maxrx < low) {
        char detail[120];
        snprintf(detail, sizeof(detail), "maxtxMsgsec %lld maxrxMsgsec %lld, expected %lld..%lld",
            (long long)maxtx, (long long)maxrx, (long long)low, (long long)high);
        ret += fail(gobj, "the rate of the burst, over the exact interval of its tick", detail);
    }

    json_t *jn_stats = gobj_stats(priv->cc, "__reset__", json_object(), gobj);
    JSON_DECREF(jn_stats)
    const char *names[] = {"txMsgs", "rxMsgs", "txMsgsec", "rxMsgsec", "maxtxMsgsec", "maxrxMsgsec", 0};
    for(int i=0; names[i]; i++) {
        if(gobj_read_integer_attr(priv->cc, names[i]) != 0) {
            ret += fail(gobj, "stats=__reset__ zeroes the counters, the rates and the maxima", names[i]);
        }
    }

    /*
     *  Every relay is counted, once each way: a write-tty (in from the
     *  client, out to the agent) and a run (the run-scenario in, its step
     *  out, the step's answer in, the run's answer out)
     */
    hgobj client = priv->client1;
    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "name", json_string("console-a"));
    json_object_set_new(kw, "content64", json_string("bHM="));
    gobj_send_event(priv->cc, EV_WRITE_TTY, kw, client);
    json_t *written = agent_request(gobj, "write-tty reaches the agent");
    JSON_DECREF(written)

    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("scn"));
    json_object_set_new(kw, "action", json_string("start"));
    json_t *response = gobj_command(priv->cc, "run-scenario", kw, client);
    JSON_DECREF(response)
    json_t *step = agent_request(gobj, "the step of the run goes to the agent");
    if(step) {
        agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, 0, "resumed");
        JSON_DECREF(step)
    }
    if(count_received(client, EV_MT_COMMAND_ANSWER, "done, 1 steps") != 1) {
        ret += fail(gobj, "the run of the rates test ends", "");
    }
    json_int_t rx = gobj_read_integer_attr(priv->cc, "rxMsgs");
    json_int_t tx = gobj_read_integer_attr(priv->cc, "txMsgs");
    if(rx != 3 || tx != 3) {
        char detail[80];
        snprintf(detail, sizeof(detail), "rx %lld tx %lld, expected 3 each",
            (long long)rx, (long long)tx);
        ret += fail(gobj, "write-tty and a run are counted, once each way", detail);
    }
    return ret;
}

/***************************************************************************
 *  12. The control center paused and stopped while the yuno runs (not at
 *      the shutdown, where a stop of a gobj that is not running is
 *      quiet): its timers, already stopped by clear_timeout(), are not
 *      stopped again ("GObj NOT RUNNING", an ERROR with a stack, before).
 *      The expected logs say it. A tick has measured the 3 messages of
 *      test 13, so the rates are not 0; paused, they read 0: up to 7.25.21
 *      they kept the value of the last tick.
 ***************************************************************************/
PRIVATE int test_stop_outside_shutdown(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    if(gobj_read_integer_attr(priv->cc, "txMsgsec") == 0 ||
            gobj_read_integer_attr(priv->cc, "rxMsgsec") == 0) {
        ret += fail(gobj, "a tick measured the messages before the pause", "");
    }
    gobj_pause(priv->cc);
    if(gobj_read_integer_attr(priv->cc, "txMsgsec") != 0 ||
            gobj_read_integer_attr(priv->cc, "rxMsgsec") != 0) {
        ret += fail(gobj, "the rates read 0 while paused", "");
    }
    gobj_stop(priv->cc);
    return ret;
}

/***************************************************************************
 *  All the tests
 ***************************************************************************/
PRIVATE int run_tests(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    priv->cc = gobj_find_service("controlcenter", TRUE);
    priv->top_side = gobj_find_service("__top_side__", TRUE);
    priv->input_side = gobj_find_service("__input_side__", TRUE);
    if(!priv->cc || !priv->top_side || !priv->input_side) {
        return fail(gobj, "services", "");
    }

    hgobj client1 = create_peer("client-1", priv->top_side);
    hgobj client2 = create_peer("client-2", priv->top_side);
    hgobj client3 = create_peer("client-3", priv->top_side);
    client_opens(gobj, client1);
    client_opens(gobj, client2);
    client_opens(gobj, client3);
    if(connect_agent(gobj) < 0) {
        return -1;
    }

    result += test_save_scenario_as_string(gobj, client1);
    result += test_save_refuses_framework_keys(gobj, client1);
    result += test_run_checks_old_scenarios(gobj, client1);
    result += test_forged_step_answers(gobj, client1, client2);
    result += test_injected_agent_events(gobj, client1, client2);
    result += test_late_answers_to_another_client(gobj, client3);
    result += test_several_mirrors(gobj, client1, client2);
    result += test_relays_only_for_who_asks(gobj, client1);
    result += test_answer_to_local_requesters(gobj, client1);
    result += test_inject_in_a_loop(gobj, client2);
    result += test_scoped_viewer(gobj, client1);
    result += test_permissions_changed(gobj);
    result += test_switch(gobj, client1);
    result += test_parallel_stop(gobj, client1);    // before 16: it counts the runs ended
    result += test_max_run_start(gobj, client1);

    priv->client1 = client1;
    priv->client3 = client3;
    return result;
}

/***************************************************************************
 *  11. The drops timer says a count when its window ends, not at the stop,
 *      and is not left armed with nothing counted. One phase per timeout,
 *      the event loop running in between.
 *
 *  Phase 1, every window opened by the cases above over 400 ms old:
 *  drops_warning_window = 300 ms. Their counts (case 5's frame, case 9's
 *  PTY frame, the 11 injected of cases 6 and 10) are due now.
 ***************************************************************************/
PRIVATE int test_drops_window_shortened(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_integer_attr(priv->cc, "drops_warning_window", 300);
    if(!gobj_read_bool_attr(priv->cc, "drops_timer_armed")) {
        return fail(gobj, "the drops timer is armed while something is counted", "");
    }
    return 0;
}

/***************************************************************************
 *  Phase 2, once the drops timer fired: it said them (the expected logs), and with nothing
 *  counted it is not armed. Then 4 readings for a client that is gone:
 *  the first is said now, 3 are counted, and it is armed again.
 ***************************************************************************/
PRIVATE int test_drops_counted_again(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;
    hgobj client = priv->client3;

    if(gobj_read_bool_attr(priv->cc, "drops_timer_armed")) {
        ret += fail(gobj, "the drops timer is left armed with nothing counted", "phase 2");
    }

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("watch-yuno-stats ids=1234"));
    json_object_set_new(kw, "__relays__", json_pack("[s]", EV_YUNO_STATS));
    check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
        0, 0, "command-agent watch-yuno-stats");
    json_t *watch = agent_request(gobj, "watch-yuno-stats reaches the agent");
    if(!watch) {
        return -1;
    }
    client_closes(gobj, client);
    for(int i=0; i<4; i++) {
        agent_sends(gobj, EV_YUNO_STATS, watch, 0, "a reading for nobody");
    }
    JSON_DECREF(watch)

    if(!gobj_read_bool_attr(priv->cc, "drops_timer_armed")) {
        ret += fail(gobj, "the drops timer is armed while something is counted", "phase 2");
    }
    return ret;
}

/***************************************************************************
 *  Phase 3, once the drops timer fired again: the 3 were said by it (dropped=3, the expected logs,
 *  before the PASSED), and it is not left armed.
 ***************************************************************************/
PRIVATE int test_drops_timer_said(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_read_bool_attr(priv->cc, "drops_timer_armed")) {
        return fail(gobj, "the drops timer is left armed with nothing counted", "phase 3");
    }
    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  EV_TIMEOUT: runs the tests inside the event loop, then exits
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Phase 2 waits for the run of case 17 B to end (no run in flight),
     *  and phases 4 and 5 for the control center's drops timer to have
     *  fired (drops_timer_armed back to false), checked at each 100 ms
     *  tick, at most PAR_WAIT_MS / DROPS_WAIT_MS: a stall of the loop
     *  delays them, it does not reorder them; a timer that never fires
     *  fails, loudly.
     */
    if(priv->phase == 2) {
        json_t *in_flight = par_run_in_flight(gobj, priv->client1);
        BOOL waiting = in_flight? TRUE : FALSE;
        JSON_DECREF(in_flight)
        if(waiting) {
            if(!test_msectimer(priv->t_wait)) {
                set_timeout(priv->timer, 100);
                KW_DECREF(kw)
                return 0;
            }
            priv->result += fail(gobj, "the run with a step not answered did not end in time",
                "phase 2");
            priv->phase = 11;
        }
    }
    if(priv->phase == 7 || priv->phase == 8) {
        if(gobj_read_bool_attr(priv->cc, "drops_timer_armed")) {
            if(!test_msectimer(priv->t_wait)) {
                set_timeout(priv->timer, 100);
                KW_DECREF(kw)
                return 0;
            }
            priv->result += fail(gobj, "the drops timer did not fire in time",
                priv->phase == 7? "phase 7" : "phase 8");
            priv->phase = 11;
        }
    }

    switch(priv->phase++) {
        case 0:
            priv->result += run_tests(gobj);
            /*  past every window opened above, and past the stop the
             *  max_run of case 16 schedules (1 s, on a clock of seconds)  */
            set_timeout(priv->timer, 2500);
            KW_DECREF(kw)
            return 0;
        case 1:
            priv->result += test_max_run_stopped(gobj);
            priv->result += test_parallel_unanswered_sent(gobj);
            priv->t_wait = start_msectimer(PAR_WAIT_MS);
            set_timeout(priv->timer, 100);
            KW_DECREF(kw)
            return 0;
        case 2:
            priv->result += test_parallel_unanswered_ended(gobj);
            set_timeout(priv->timer, 100);
            KW_DECREF(kw)
            return 0;
        case 3:
            priv->result += test_max_run_no_node_start(gobj);
            set_timeout(priv->timer, 2500);     // past the stop its max_run schedules
            KW_DECREF(kw)
            return 0;
        case 4:
            priv->result += test_max_run_no_node_waits(gobj);
            set_timeout(priv->timer, 1500);     // the stop timer, at least 1 s
            KW_DECREF(kw)
            return 0;
        case 5:
            priv->result += test_max_run_no_node_reached(gobj);
            priv->result += test_max_run_node_refuses_start(gobj);
            set_timeout(priv->timer, 2500);     // past the stop its max_run schedules
            KW_DECREF(kw)
            return 0;
        case 6:
            priv->result += test_max_run_node_refuses_retried(gobj);
            priv->result += test_drops_window_shortened(gobj);
            priv->t_wait = start_msectimer(DROPS_WAIT_MS);
            set_timeout(priv->timer, 100);
            KW_DECREF(kw)
            return 0;
        case 7:
            priv->result += test_drops_counted_again(gobj);
            priv->t_wait = start_msectimer(DROPS_WAIT_MS);
            set_timeout(priv->timer, 100);
            KW_DECREF(kw)
            return 0;
        case 8:
            priv->result += test_drops_timer_said(gobj);
            priv->result += test_rates_traffic(gobj);
            set_timeout(priv->timer, 2*RATES_TICK_MS + 500);    // past two ticks
            KW_DECREF(kw)
            return 0;
        case 9:
            priv->result += test_rates_read(gobj);
            set_timeout(priv->timer, RATES_TICK_MS + 500);  // a tick with the 3 messages
            KW_DECREF(kw)
            return 0;
        case 10:
            priv->result += test_stop_outside_shutdown(gobj);
            break;
        default:
            break;
    }

    if(priv->result == 0) {
        gobj_log_info(gobj, 0,
            "msgset", "%s", MSGSET_INFO,
            "msg", "%s", "All controlcenter scenarios tests PASSED",
            NULL
        );
    }

    gobj_log_info(gobj, 0,
        "msgset", "%s", MSGSET_INFO,
        "msg", "%s", "Exit to die",
        NULL
    );
    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_PERMISSIONS_CHANGED of the control center (case 15)
 ***************************************************************************/
PRIVATE int ac_permissions_changed(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->permissions_changed++;
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_SCENARIO_RUN_ENDED of the control center (case 16)
 ***************************************************************************/
PRIVATE int ac_run_ended(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->runs_ended++;
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_STOPPED
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}




            /***************************
             *      Commands
             ***************************/




/***************************************************************************
 *              Command: help
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw)
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
 *                          FSM
 ***************************************************************************/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
};

GOBJ_DEFINE_GCLASS(C_TEST_CC);

PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    ev_action_t st_idle[] = {
        {EV_TIMEOUT,                ac_timeout,         0},
        {EV_STOPPED,                ac_stopped,         0},
        {EV_PERMISSIONS_CHANGED,    ac_permissions_changed, 0},
        {EV_SCENARIO_RUN_ENDED,     ac_run_ended,       0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_STOPPED,                0},
        {EV_PERMISSIONS_CHANGED,    0},
        {EV_SCENARIO_RUN_ENDED,     0},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
        command_table,
        s_user_trace_level,
        0   // gcflag_t
    );
    return __gclass__ ? 0 : -1;
}

/***************************************************************************
 *              Registration
 ***************************************************************************/
PUBLIC int register_c_test_cc(void)
{
    return create_gclass(C_TEST_CC);
}
