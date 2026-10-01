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
 *                the client loops connect/inject/leave.
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

    int phase;              // the tests run in the loop: each phase a timeout
    int result;
    hgobj client3;
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
 *  Phase 2: the timer said them (the expected logs), and with nothing
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
 *  Phase 3: the 3 were said by the timer (dropped=3, the expected logs,
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

    switch(priv->phase++) {
        case 0:
            priv->result += run_tests(gobj);
            set_timeout(priv->timer, 400);  // past every window opened above
            KW_DECREF(kw)
            return 0;
        case 1:
            priv->result += test_drops_window_shortened(gobj);
            set_timeout(priv->timer, 1000); // the timer fires meanwhile
            KW_DECREF(kw)
            return 0;
        case 2:
            priv->result += test_drops_counted_again(gobj);
            set_timeout(priv->timer, 1000); // past the 300 ms window
            KW_DECREF(kw)
            return 0;
        default:
            priv->result += test_drops_timer_said(gobj);
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
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_STOPPED,                0},
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
