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
 *                `scenarios` says the run in flight;
 *              - the key of a node (NODE KEYS of c_controlcenter.c): a
 *                node never seen is recorded pending; accept-node pins its
 *                key; with enforce_node_keys another key, a pending or a
 *                refused node is dropped, an agent without a key is let
 *                in; refuse-node drops the agent in session; an agent with
 *                the accepted key is authenticated by it (mt_authenticate),
 *                as agent:<uuid>, with no token; another key or a node not
 *                accepted, with no token, is refused;
 *              - each request for the agent (it says
 *                `knows_operator_assertion`) carries an operator assertion
 *                (operator_assertion_key): signed with the key, for that
 *                node (aud = its uuid), naming who asks (sub), living 60 s;
 *                and none when nobody asks. Verified with the public key,
 *                as the agent's C_AUTHZ does;
 *              - a node is reached by its uuid, and by a hostname only when
 *                one node has it: an agent that gives another's hostname,
 *                or its uuid as hostname, takes nothing, and write-tty goes
 *                to one agent; a hostname is written only by a key login
 *                of an accepted node, so it cannot be planted, and a card
 *                id that is another node's hostname takes nothing;
 *                drop-agent names a node as command-agent does; one key
 *                is for one node; the card of an
 *                agent not authenticated is checked; a decision about a
 *                node drops its sessions it does not let in now; an open
 *                with nothing changed (its port aside) writes nothing;
 *                logout-user asks its permission and passes `disabled`
 *                only when given;
 *              - a node without a key is named only by the card of its
 *                agent in session, and a refused open writes nothing, so
 *                no session of a token takes its name nor leaves a name
 *                behind; the session that logged in by the key gets what
 *                goes to its uuid; the card of a session of a token is
 *                checked; a scenario on a name of more than one node does
 *                not start, its stop does what it reaches, and its max_run
 *                stop is tried again in a minute;
 *              - an agent whose link is closing refuses an order at once:
 *                command-agent, stats-agent and write-tty are answered with
 *                that refusal, and a step of a run fails at once;
 *              - what the agent answers to its own requests (read-file,
 *                view-config...) reaches the client that asked them.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <jwt.h>

#include "c_test_cc_peer.h"
#include "c_test_cc.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define AGENT_HOST  "node1"
#define GHOST_HOST  "ghost"     // case 19: a node whose agent connects late
#define DROPS_WAIT_MS   5000    // the most a phase waits for the drops timer

/*
 *  The public half of the key of main.c's operator_assertion_key
 */
PRIVATE const char *ASSERTION_PUBLIC_JWK = "{\"kid\":\"controlcenter\",\"kty\":\"EC\",\"crv\":\"P-256\",\"alg\":\"ES256\",\"use\":\"sig\",\"x\":\"nXhUxejW5sVCIMMt5VPATGJS_nsEIvIHbw_xYLdpa1Q\",\"y\":\"EmyPj6BtdnzvQTYMrLgqtRxTZig5KqIS1hh2YEB2kW8\"}";

GOBJ_DECLARE_EVENT(EV_PERMISSIONS_CHANGED);     // c_controlcenter.c
GOBJ_DECLARE_EVENT(EV_SCENARIO_RUN_ENDED);      // c_controlcenter.c

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int write_failed_tries(hgobj gobj, const char *scenario_id);
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
    hgobj mra_1;                // case 36: the two nodes that give the name of "mra"
    hgobj mra_2;
    hgobj mrp_node;             // case 38: the node of the second yuno of "mrp"
    hgobj mrw_node;             // case 41: the node of the second yuno of "mrw"
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

    /*
     *  libjwt allocates with gbmem, as C_AUTHZ makes it in a real control
     *  center (this yuno has none): the control center signs its operator
     *  assertions only then. jwt_set_alloc() hands jansson libjwt's
     *  wrappers too: back to gbmem's own pair, as C_AUTHZ does.
     */
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;
    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    jwt_set_alloc(malloc_func, free_func);
    sys_malloc_fn_t json_malloc_func;
    sys_free_fn_t json_free_func;
    gbmem_get_json_allocators(&json_malloc_func, &json_free_func);
    json_set_alloc_funcs(json_malloc_func, json_free_func);
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
 *  The agent of AGENT_HOST says its session is open (EV_ON_OPEN of
 *  __input_side__): the control center writes its node's record, whose
 *  hostname is what names the node in command-agent
 ***************************************************************************/
PRIVATE void agent_opens(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj srv = gobj_child_by_name(priv->agent_channel, "agent-1");
    gobj_send_event(priv->cc, EV_ON_OPEN,
        json_pack("{s:{s:I}, s:O}",
            "__temp__", "channel_gobj", (json_int_t)(uintptr_t)priv->agent_channel,
            "identity_card", gobj_read_json_attr(srv, "identity_card")
        ),
        priv->input_side
    );
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

    json_t *kw_srv = json_pack("{s:s, s:s, s:s, s:{s:s, s:b, s:{s:[{s:s}]}}}",
        "client_yuno_role", "yuneta_agent",
        "client_yuno_name", AGENT_HOST,
        "client_yuno_service", "agent",
        "identity_card",
            "id", "uuid-" AGENT_HOST,
            "knows_operator_assertion", 1,
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
    agent_opens(gobj);
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
        " \"actions\": {\"start\": [{\"yuno\": \"1234\", \"command\": \"resume-load\"}]}}"
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
        "resume-load __md_iev__=x",
        "resume-load __username__=root",
        "resume-load __md_command__=x",
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
    json_object_set_new(kw, "scenario", scenario_with_step("scn", "resume-load rate=10"));
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
        "resume-load __username__=root",
        "resume-load id=4321",
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
     *  A client injects the answer, a frame with a "__temp__" of its own
     *  whose channel is an address it chose: decoded as __top_side__'s
     *  C_IEVENT_SRV decodes it, and given its channel as C_IOGATE gives it.
     *  It is the first of the drops window, so its channel is read for the
     *  warning: up to 7.30.26 the peer's list was left in place (C_IOGATE
     *  could not write into it) and the address was read, a SIGSEGV.
     *  The bytes are written by hand, as a peer writes them:
     *  iev_create_to_gbuffer() leaves "__temp__" out of every frame.
     */
    json_t *forged = msg_iev_build_response(gobj, 0, json_string("forged by a client"), 0, 0,
        json_deep_copy(step));
    char *forged_text = json2uglystr(forged);
    KW_DECREF(forged)
    gbuffer_t *frame = gbuffer_create(256, 256*1024);
    if(frame && forged_text && forged_text[0] == '{') {
        gbuffer_printf(frame,
            "{\"event\": \"%s\", \"kw\": {\"__temp__\": [%lld], %s}",
            EV_MT_COMMAND_ANSWER,
            (long long)0x41414141,
            forged_text + 1     // the members of the kw, after its '{'
        );
    }
    GBMEM_FREE(forged_text)
    gobj_event_t frame_event = NULL;
    forged = frame? iev_create_from_gbuffer(gobj, &frame_event, frame, 1) : NULL;
    if(!forged || frame_event != EV_MT_COMMAND_ANSWER) {
        KW_DECREF(forged)
        return ret + fail(gobj, "the frame of a client is decoded", "");
    }
    kw_set_subdict_value(gobj, forged, "__temp__", "channel",
        json_string(gobj_name(other)));
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
    /*
     *  The client opens it again (the agent's new channel relays only to
     *  a client that asked it, case 49): the agent opens it again
     */
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("open-console"));
    ret += check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
        0, 0, "command-agent open-console again");
    json_decref(agent_request(gobj, "open-console again reaches the agent"));
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
        EV_READ_FILE,
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
 *  48. What the agent answers to its own requests (read-file, edit-config,
 *      view-yuno-config...) reaches the client that asked by command-agent,
 *      each with its own event. The agent's channel decodes a frame only
 *      if its event is public in this yuno: up to 7.30.26 none of the seven
 *      was ("Frame of an event that is not public, refused"), the client
 *      was told "Command sent to 1 nodes" and nothing more.
 ***************************************************************************/
PRIVATE int test_agent_request_answers(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    struct {
        const char *cmd2agent;
        gobj_event_t event;
    } requests[] = {
        {"read-file filename=/yuneta/agent/yuneta_agent.json",  EV_READ_FILE},
        {"read-json filename=/yuneta/agent/yuneta_agent.json",  EV_READ_JSON},
        {"read-binary-file filename=/yuneta/agent/yuneta_agent",EV_READ_BINARY_FILE},
        {"edit-config id=x",                                    EV_EDIT_CONFIG},
        {"view-config id=x",                                    EV_VIEW_CONFIG},
        {"edit-yuno-config id=1234",                            EV_EDIT_YUNO_CONFIG},
        {"view-yuno-config id=1234",                            EV_VIEW_YUNO_CONFIG},
        {0, 0}
    };

    for(int i=0; requests[i].cmd2agent; i++) {
        const char *event = requests[i].event;
        if(!gclass_find_public_event(event, FALSE)) {
            ret += fail(gobj, "the agent's channel refuses the frame of an answer of the agent", event);
        }

        json_t *kw = client_kw(gobj_name(client));
        json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
        json_object_set_new(kw, "cmd2agent", json_string(requests[i].cmd2agent));
        ret += check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
            0, "Command sent to 1 nodes", requests[i].cmd2agent);
        json_t *request = agent_request(gobj, requests[i].cmd2agent);
        if(!request) {
            ret += -1;
            continue;
        }
        agent_sends_data(gobj, event, request, 0, "the agent's answer",
            json_pack("{s:s}", "name", "yuneta_agent.json"));
        JSON_DECREF(request)
        if(count_received(client, event, "the agent's answer") != 1) {
            ret += fail(gobj, "the answer of the agent's request reaches the client that asked", event);
        }
    }
    return ret;
}

/***************************************************************************
 *  `request` (not owned, what the agent got) with its route rewritten to
 *  the channel `to` (NULL: kept) and the connection stamp of the client
 *  hop set to `connection` (-1: taken out). Yours.
 ***************************************************************************/
PRIVATE json_t *rerouted(json_t *request, const char *to, json_int_t connection)
{
    json_t *forged = json_deep_copy(request);
    json_t *stack = json_object_get(json_object_get(forged, "__md_iev__"), "ievent_gate_stack");
    json_t *top = json_array_get(stack, 0);
    json_t *client = json_array_get(stack, json_array_size(stack) - 1);
    if(to) {
        json_object_set_new(top, "src_service", json_string(to));
    }
    if(connection < 0) {
        json_object_del(client, "cc_connection");
    } else {
        json_object_set_new(client, "cc_connection", json_integer(connection));
    }
    return forged;
}

/***************************************************************************
 *  49. What an agent answers (a command, a stats, its own requests) and
 *      streams (a console, the readings of a watch) reaches only a client
 *      that asked THAT agent, in the connection it holds now. The agent writes the route of its answers: up to 7.30.26
 *      an agent could send them to any web client of the control center,
 *      naming its channel, with the connection stamp of that client or
 *      with none. The answers that do not go: ONE WARNING (a window), the
 *      rest counted.
 ***************************************************************************/
PRIVATE int test_agent_request_answers_unasked(hgobj gobj, hgobj client, hgobj other)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    /*  `other` connects again: this connection asked the agent nothing  */
    client_closes(gobj, other);
    client_opens(gobj, other);
    json_decref(take_received(other));
    json_int_t other_connection = json_integer_value(gobj_read_user_data(other, "cc_connection"));
    json_int_t client_connection = json_integer_value(gobj_read_user_data(client, "cc_connection"));

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("read-file filename=/yuneta/agent/yuneta_agent.json"));
    ret += check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
        0, "Command sent to 1 nodes", "read-file of a client");
    json_t *request = agent_request(gobj, "read-file of a client");
    if(!request) {
        return ret - 1;
    }

    gobj_event_t events[] = {
        EV_READ_FILE, EV_READ_JSON, EV_READ_BINARY_FILE, EV_EDIT_CONFIG,
        EV_VIEW_CONFIG, EV_EDIT_YUNO_CONFIG, EV_VIEW_YUNO_CONFIG,
        EV_MT_COMMAND_ANSWER, EV_MT_STATS_ANSWER,
        /*  A console and a watch: up to 7.30.26 relayed to any client  */
        EV_TTY_OPEN, EV_TTY_DATA, EV_TTY_CLOSE, EV_YUNO_STATS, 0
    };
    for(int i=0; events[i]; i++) {
        /*  To the client that asked nothing, with its own stamp  */
        json_t *forged = rerouted(request, gobj_name(other), other_connection);
        agent_sends_data(gobj, events[i], forged, 0, "forged for a client that did not ask",
            json_pack("{s:s}", "name", "evil"));
        JSON_DECREF(forged)
        /*  To it with no stamp  */
        forged = rerouted(request, gobj_name(other), -1);
        agent_sends_data(gobj, events[i], forged, 0, "forged with no stamp",
            json_pack("{s:s}", "name", "evil"));
        JSON_DECREF(forged)
        /*  To the client that asked, with no stamp  */
        forged = rerouted(request, NULL, -1);
        agent_sends_data(gobj, events[i], forged, 0, "forged with no stamp",
            json_pack("{s:s}", "name", "evil"));
        JSON_DECREF(forged)
    }
    json_t *received = take_received(other);
    if(json_array_size(received) != 0) {
        char *s = json_dumps(received, JSON_COMPACT);
        ret += fail(gobj, "an answer of an agent reached a client that did not ask it", s);
        gbmem_free(s);
    }
    JSON_DECREF(received)
    received = take_received(client);
    if(json_array_size(received) != 0) {
        char *s_ = json_dumps(received, JSON_COMPACT);
        ret += fail(gobj, "an answer of an agent with no connection stamp reached the client", s_);
        gbmem_free(s_);
    }
    JSON_DECREF(received)

    /*  What must still go: the agent's answer, to the client that asked  */
    json_t *legit = rerouted(request, NULL, client_connection);
    agent_sends_data(gobj, EV_READ_FILE, legit, 0, "the agent's answer",
        json_pack("{s:s}", "name", "yuneta_agent.json"));
    JSON_DECREF(legit)
    if(count_received(client, EV_READ_FILE, "the agent's answer") != 1) {
        ret += fail(gobj, "the answer of the agent's request reaches the client that asked", "");
    }
    JSON_DECREF(request)
    return ret;
}

/***************************************************************************
 *  50. A web client that does not take what is relayed to it: its queue
 *      holds client_max_tx_queue messages (the test writes the attribute of
 *      its channel, as C_TCP's). A console's output for it drops the
 *      client, ONE WARNING, and nothing more is relayed to that connection.
 *      Up to 7.30.26 its queue grew with all of it.
 ***************************************************************************/
PRIVATE int test_client_that_does_not_take(hgobj gobj, hgobj client)
{
    int ret = 0;

    json_t *route = open_console(gobj, client, "full");
    if(!route) {
        return -1;
    }
    gobj_write_integer_attr(client, "cur_tx_queue", 8192);
    for(int i=0; i<3; i++) {
        agent_sends_data(gobj, EV_TTY_DATA, route, 0, "output",
            json_pack("{s:s, s:s}", "name", "full", "content64", "eHg="));
    }
    json_t *received = take_received(client);
    int drops = 0, data = 0;
    size_t idx; json_t *jn;
    json_array_foreach(received, idx, jn) {
        const char *ev = json_string_value(json_object_get(jn, "event"));
        if(ev && strcmp(ev, EV_DROP)==0) {
            drops++;
        } else if(ev && strcmp(ev, EV_TTY_DATA)==0) {
            data++;
        }
    }
    JSON_DECREF(received)
    if(drops != 1 || data != 0) {
        char d[64];
        snprintf(d, sizeof(d), "drops %d, data %d", drops, data);
        ret += fail(gobj, "a client that does not take is dropped once, and gets nothing", d);
    }

    /*  It connects again, and takes: what is relayed reaches it  */
    gobj_write_integer_attr(client, "cur_tx_queue", 0);
    client_closes(gobj, client);
    client_opens(gobj, client);
    JSON_DECREF(route)
    route = open_console(gobj, client, "full");
    if(route) {
        agent_sends_data(gobj, EV_TTY_DATA, route, 0, "output",
            json_pack("{s:s, s:s}", "name", "full", "content64", "eHg="));
        if(count_received(client, EV_TTY_DATA, 0) != 1) {
            ret += fail(gobj, "a client that takes gets the console's output", "");
        }
        agent_sends_data(gobj, EV_TTY_CLOSE, route, 0, "tty close",
            json_pack("{s:s}", "name", "full"));
        json_decref(take_received(client));
    }
    JSON_DECREF(route)
    return ret;
}

/***************************************************************************
 *  51. A web client that does not take the answers of an agent: a message
 *      is a frame of any size (an answer can be 200 MB), so what it holds
 *      is bounded in bytes too, client_max_tx_bytes (the client's channel
 *      counts, as C_TCP's cur_tx_bytes, what is sent to it while it holds).
 *      Sixteen answers of 1 MB with a bound of 4 MB: the client gets what
 *      fits, is dropped ONCE (the WARNING of case 50), and nothing more is
 *      relayed to that connection. Up to the fix the bound counted
 *      messages only, and sixteen answers of 24 MB took the control center
 *      from 15 to 408 MB with no warning.
 ***************************************************************************/
PRIVATE int test_client_that_holds_bytes(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_int_t bound0 = gobj_read_integer_attr(priv->cc, "client_max_tx_bytes");
    if(bound0 != 32*1024*1024) {
        char d[64];
        snprintf(d, sizeof(d), "%ld", (long)bound0);
        ret += fail(gobj, "client_max_tx_bytes is 32 MB by default", d);
    }
    const json_int_t bound = 4*1024*1024;
    gobj_write_integer_attr(priv->cc, "client_max_tx_bytes", bound);

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("read-file filename=/yuneta/agent/big"));
    ret += check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
        0, "Command sent to 1 nodes", "read-file of a client that holds");
    json_t *request = agent_request(gobj, "read-file of a client that holds");
    if(!request) {
        gobj_write_integer_attr(priv->cc, "client_max_tx_bytes", bound0);
        return ret - 1;
    }

    size_t big = 1024*1024;
    char *content = gbmem_malloc(big + 1);
    memset(content, 'A', big);
    content[big] = 0;
    gobj_write_bool_attr(client, "holding", TRUE);
    for(int i=0; i<16; i++) {
        agent_sends_data(gobj, EV_READ_FILE, request, 0, "a big answer",
            json_pack("{s:s, s:s}", "name", "big", "content64", content));
    }
    gbmem_free(content);
    JSON_DECREF(request)

    json_int_t held = gobj_read_integer_attr(client, "cur_tx_bytes");
    json_t *received = take_received(client);
    int drops = 0, answers = 0, after_drop = 0;
    size_t idx; json_t *jn;
    json_array_foreach(received, idx, jn) {
        const char *ev = json_string_value(json_object_get(jn, "event"));
        if(ev && strcmp(ev, EV_DROP)==0) {
            drops++;
        } else if(ev && strcmp(ev, EV_READ_FILE)==0) {
            answers++;
            if(drops) {
                after_drop++;
            }
        }
    }
    JSON_DECREF(received)
    if(drops != 1 || answers < 4 || after_drop != 0 ||
            held < bound || held > bound + (json_int_t)(2*big)) {
        char d[128];
        snprintf(d, sizeof(d), "drops %d, answers %d (%d after the drop), held %ld bytes",
            drops, answers, after_drop, (long)held);
        ret += fail(gobj, "a client that holds what it is sent is dropped once at the bound in bytes", d);
    }

    gobj_write_bool_attr(client, "holding", FALSE);
    gobj_write_integer_attr(client, "cur_tx_bytes", 0);
    client_closes(gobj, client);
    client_opens(gobj, client);
    json_decref(take_received(client));
    gobj_write_integer_attr(priv->cc, "client_max_tx_bytes", bound0);
    return ret;
}

/***************************************************************************
 *  52. A client asks an agent and leaves before the answer: the answer is
 *      dropped as "requester gone" (an INFO), not as an answer for a client
 *      that did not ask (the WARNING of case 49, which names the agent as a
 *      forger, and whose count it joined). Up to the fix the closed channel
 *      had no connection, so the check of the stamp ran first and failed.
 ***************************************************************************/
PRIVATE int test_answer_for_a_client_gone(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("read-file filename=/yuneta/agent/yuneta_agent.json"));
    ret += check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
        0, "Command sent to 1 nodes", "read-file of a client that leaves");
    json_t *request = agent_request(gobj, "read-file of a client that leaves");
    if(!request) {
        return ret - 1;
    }
    client_closes(gobj, client);
    agent_sends_data(gobj, EV_READ_FILE, request, 0, "the agent's answer",
        json_pack("{s:s}", "name", "yuneta_agent.json"));
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, request, 0, "the agent's answer");
    JSON_DECREF(request)
    json_t *received = take_received(client);
    if(json_array_size(received) != 0) {
        ret += fail(gobj, "an answer reached a client that is gone", "");
    }
    JSON_DECREF(received)
    client_opens(gobj, client);
    json_decref(take_received(client));
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
    json_object_set_new(kw, "scenario", scenario_with_step("scn", "resume-load rate=10"));
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
    json_object_set_new(kw, "scenario", scenario_with_step("not-scn", "resume-load rate=10"));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        -1, "no permission to 'write-scenarios'", "a scenario_id beside another scenario is no permission");
    kw = viewer_kw(client);
    json_object_set_new(kw, "__username__", json_string("writer"));
    json_object_set_new(kw, "scenario", scenario_with_step("wr", "resume-load rate=10"));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        0, "scenario created: wr", "the writer of 'wr' saves it");
    /*  "scn" runs (case 4 started it): writing it would be running it  */
    kw = viewer_kw(client);
    json_object_set_new(kw, "__username__", json_string("writer"));
    json_object_set_new(kw, "scenario", scenario_with_step("scn", "resume-load rate=10"));
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
    json_t *expected = json_pack("{s:b, s:b, s:b, s:b, s:b, s:b, s:[s], s:b, s:b}",
        "list-agents", 0, "command-agent", 0, "drop-agent", 0, "logout-user", 0,
        "write-tty", 0, "manage-nodes", 0, "read-scenarios", "scn", "write-scenarios", 0,
        "run-scenarios", 0
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
 *      events are published here by the store (the service treedb_authzs)
 *      as C_NODE publishes them. The same event sent by anybody else (a
 *      peer's frame past the frontier) is ignored, with ONE warning for
 *      three: no EV_PERMISSIONS_CHANGED.
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
    hgobj store = gobj_find_service("treedb_authzs", TRUE);
    for(int i=0; cases[i].event; i++) {
        int before = priv->permissions_changed;
        gobj_publish_event(store, cases[i].event,
            json_pack("{s:s, s:s}", cases[i].key, cases[i].topic, "treedb_name", "treedb_authzs")
        );
        if(priv->permissions_changed - before != cases[i].told) {
            char what[NAME_MAX];
            snprintf(what, sizeof(what), "%s of %s", cases[i].event, cases[i].topic);
            ret += fail(gobj, "EV_PERMISSIONS_CHANGED told for a change of the users store", what);
        }
    }

    int before = priv->permissions_changed;
    for(int i=0; i<3; i++) {
        gobj_send_event(priv->cc, EV_TREEDB_NODE_UPDATED,
            json_pack("{s:s, s:s}", "topic_name", "roles", "treedb_name", "treedb_authzs"),
            gobj
        );
    }
    if(priv->permissions_changed != before) {
        ret += fail(gobj, "a store event not from the store tells EV_PERMISSIONS_CHANGED", "");
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
            "start", "yuno", "1234", "command", "resume-load",
            "stop", "yuno", "1234", "command", "pause-load"
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
    json_t *scn = scenario_with_step("scn", "resume-load rate=10");
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
    if(!strstr(kw_get_str(gobj, step, "__command__", "", 0), "pause-load")) {
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
    json_t *identity_card = json_incref(json_object_get(kw_srv, "identity_card"));
    hgobj srv = gobj_create("agent-2", C_IEVENT_SRV, kw_srv, priv->ghost_channel);
    if(!srv) {
        JSON_DECREF(identity_card)
        return fail(gobj, "ghost_connects", "cannot create the C_IEVENT_SRV");
    }
    gobj_set_bottom_gobj(srv, priv->ghost_wire);
    gobj_start(srv);
    gobj_change_state(srv, ST_SESSION);

    gobj_send_event(priv->cc, EV_ON_OPEN,
        json_pack("{s:{s:I}, s:o}",
            "__temp__", "channel_gobj", (json_int_t)(uintptr_t)priv->ghost_channel,
            "identity_card", identity_card
        ),
        priv->input_side
    );
    return 0;
}

/***************************************************************************
 *  0. Two agents with one uuid. A copy of the agent of AGENT_HOST, on
 *      another host, opens its session: a WARNING (the expected logs). The
 *      same uuid from the same host (a link that came back before its old
 *      session closed) says nothing. Both are gone before the cases below,
 *      and the agent of AGENT_HOST opens again: the copy renamed its record.
 ***************************************************************************/
PRIVATE hgobj open_agent_session(hgobj gobj, const char *name, const char *uuid, const char *host)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char wire_name[NAME_MAX];
    snprintf(wire_name, sizeof(wire_name), "%s-wire", name);
    hgobj channel = create_peer(name, priv->input_side);
    hgobj wire = create_peer(wire_name, channel);

    json_t *kw_srv = json_pack("{s:s, s:s, s:s, s:{s:s, s:{s:[{s:s}]}}}",
        "client_yuno_role", "yuneta_agent",
        "client_yuno_name", host,
        "client_yuno_service", "agent",
        "identity_card",
            "id", uuid,
            "__md_iev__",
                "ievent_gate_stack",
                    "host", host
    );
    json_t *identity_card = json_incref(json_object_get(kw_srv, "identity_card"));
    hgobj srv = gobj_create(name, C_IEVENT_SRV, kw_srv, channel);
    if(!srv) {
        JSON_DECREF(identity_card)
        fail(gobj, "open_agent_session", "cannot create the C_IEVENT_SRV");
        return 0;
    }
    gobj_set_bottom_gobj(srv, wire);
    gobj_start(srv);
    gobj_change_state(srv, ST_SESSION);

    gobj_send_event(priv->cc, EV_ON_OPEN,
        json_pack("{s:{s:I}, s:o}",
            "__temp__", "channel_gobj", (json_int_t)(uintptr_t)channel,
            "identity_card", identity_card
        ),
        priv->input_side
    );
    return channel;
}

/***************************************************************************
 *  An agent session that presents a key (peer_cert.spki_sha256, as the
 *  C_IEVENT_SRV of a TLS link with a client certificate holds it and gives
 *  it in its EV_ON_OPEN), or none, with the identity card `card` (owned).
 *  Its channel has a peername, as C_TCP's: `peer_ip` and a new port at
 *  every session, as a link that connects again.
 ***************************************************************************/
PRIVATE const char *peer_ip = "10.0.0.1";
PRIVATE unsigned peer_port = 40000;

PRIVATE hgobj open_session_card(
    hgobj gobj,
    const char *name,
    json_t *card,           // owned
    const char *spki,       // NULL: no certificate
    const char *username    // NULL: a session of a token (no __username__ said)
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char wire_name[NAME_MAX];
    snprintf(wire_name, sizeof(wire_name), "%s-wire", name);
    hgobj channel = create_peer(name, priv->input_side);
    hgobj wire = create_peer(wire_name, channel);
    char peername[NAME_MAX];
    snprintf(peername, sizeof(peername), "%s:%u", peer_ip, peer_port++);
    gobj_write_str_attr(channel, "peername", peername);

    json_t *kw_srv = json_pack("{s:s, s:s, s:s, s:O}",
        "client_yuno_role", "yuneta_agent",
        "client_yuno_name", name,
        "client_yuno_service", "agent",
        "identity_card", card
    );
    hgobj srv = gobj_create(name, C_IEVENT_SRV, kw_srv, channel);
    if(!srv) {
        JSON_DECREF(card)
        fail(gobj, "open_session_card", "cannot create the C_IEVENT_SRV");
        return 0;
    }
    if(spki) {
        gobj_write_new_json_attr(srv, "peer_cert", json_pack("{s:s, s:b}",
            "spki_sha256", spki,
            "verified", 0
        ));
    }
    if(username) {
        gobj_write_str_attr(srv, "__username__", username);
    }
    gobj_set_bottom_gobj(srv, wire);
    gobj_start(srv);
    gobj_change_state(srv, ST_SESSION);

    json_t *kw_open = json_pack("{s:{s:I}, s:o}",
        "__temp__", "channel_gobj", (json_int_t)(uintptr_t)channel,
        "identity_card", card
    );
    if(username) {
        json_object_set_new(kw_open, "__username__", json_string(username));
    }
    if(spki) {
        json_object_set_new(kw_open, "peer_cert", json_pack("{s:s, s:b}",
            "spki_sha256", spki,
            "verified", 0
        ));
    }
    gobj_send_event(priv->cc, EV_ON_OPEN, kw_open, priv->input_side);
    return channel;
}

/*  The same, with the card of an agent: its uuid, its hostname  */
PRIVATE hgobj open_session_as(
    hgobj gobj,
    const char *name,
    const char *uuid,
    const char *host,
    const char *spki,       // NULL: no certificate
    const char *username    // NULL: a session of a token (no __username__ said)
)
{
    json_t *card = json_pack("{s:s, s:{s:[{s:s}]}}",
        "id", uuid,
        "__md_iev__",
            "ievent_gate_stack",
                "host", host
    );
    return open_session_card(gobj, name, card, spki, username);
}

PRIVATE hgobj open_keyed_session(
    hgobj gobj,
    const char *name,
    const char *uuid,
    const char *host,
    const char *spki    // NULL: no certificate
)
{
    return open_session_as(gobj, name, uuid, host, spki, NULL);
}

/*  A session that logged in by the accepted key of its node: agent:<uuid>  */
PRIVATE hgobj open_key_login(
    hgobj gobj,
    const char *name,
    const char *uuid,
    const char *host,
    const char *spki
)
{
    char username[NAME_MAX];
    snprintf(username, sizeof(username), "agent:%s", uuid);
    return open_session_as(gobj, name, uuid, host, spki, username);
}

/*  How many EV_DROP `peer` got (its record is taken)  */
PRIVATE int drops_of(hgobj peer)
{
    json_t *received = take_received(peer);
    int n = 0;
    size_t idx; json_t *jn;
    json_array_foreach(received, idx, jn) {
        if(strcmp(kw_get_str(0, jn, "event", "", 0), EV_DROP)==0) {
            n++;
        }
    }
    JSON_DECREF(received)
    return n;
}

PRIVATE void close_session(hgobj channel)
{
    if(channel) {
        gobj_stop_tree(channel);
        gobj_destroy(channel);
    }
}

/*  The `nodes` record of `uuid` as list-nodes says it (yours), or NULL  */
PRIVATE json_t *node_record(hgobj gobj, hgobj client, const char *uuid)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *response = gobj_command(priv->cc, "list-nodes", client_kw(gobj_name(client)), client);
    json_t *found = NULL;
    size_t idx; json_t *jn;
    json_array_foreach(kw_get_list(gobj, response, "data", 0, 0), idx, jn) {
        if(strcmp(kw_get_str(gobj, jn, "id", "", 0), uuid)==0) {
            found = json_incref(jn);
            break;
        }
    }
    JSON_DECREF(response)
    return found;
}

/*  A field of the record of `uuid`, compared  */
PRIVATE int check_node_field(hgobj gobj, hgobj client, const char *uuid,
    const char *field, const char *expected, const char *what)
{
    json_t *node = node_record(gobj, client, uuid);
    const char *got = node? kw_get_str(gobj, node, field, "", 0) : "(no record)";
    int ret = strcmp(got, expected)==0? 0 : fail(gobj, what, got);
    JSON_DECREF(node)
    return ret;
}

/***************************************************************************
 *  The control center authenticates the identity card `card` (owned) of an
 *  agent whose C_IEVENT_SRV holds `spki` as its transport's peer_cert (no
 *  token): the answer (yours), and in `username` the __username__ the
 *  session got
 ***************************************************************************/
PRIVATE json_t *authenticate_card(
    hgobj gobj,
    const char *name,
    json_t *card,
    const char *spki,
    char *username,
    size_t username_size
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj channel = create_peer(name, priv->input_side);
    hgobj srv = gobj_create(name, C_IEVENT_SRV, 0, channel);
    if(!srv) {
        fail(gobj, "authenticate_card", "cannot create the C_IEVENT_SRV");
        JSON_DECREF(card)
        close_session(channel);
        return NULL;
    }
    gobj_write_new_json_attr(srv, "peer_cert", json_pack("{s:s, s:b}", "spki_sha256", spki, "verified", 0));
    json_t *response = gobj_authenticate(priv->cc, card, srv);
    snprintf(username, username_size, "%s", gobj_read_str_attr(srv, "__username__"));
    close_session(channel);
    return response;
}

/*  The card of an agent, as C_IEVENT_CLI writes it, authenticated by `spki`  */
PRIVATE json_t *authenticate_by_key(
    hgobj gobj,
    const char *name,
    const char *uuid,
    const char *host,
    const char *spki,
    char *username,
    size_t username_size
)
{
    json_t *card = json_pack("{s:s, s:{s:[{s:s, s:s}]}}",
        "id", uuid,
        "__md_iev__",
            "ievent_gate_stack",
                "dst_service", "controlcenter",
                "host", host
    );
    return authenticate_card(gobj, name, card, spki, username, username_size);
}

/***************************************************************************
 *  22. The key of a node (NODE KEYS of c_controlcenter.c)
 ***************************************************************************/
#define KEY_A   "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define KEY_B   "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
#define KEY_C   "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
#define KEY_D   "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"
#define KEY_E   "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
#define KEY_F   "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
#define KEY_G   "0000000000000000000000000000000000000000000000000000000000000000"
#define KEY_H   "1111111111111111111111111111111111111111111111111111111111111111"
#define KEY_I   "2222222222222222222222222222222222222222222222222222222222222222"
#define KEY_J   "3333333333333333333333333333333333333333333333333333333333333333"

PRIVATE int test_node_keys(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    /*
     *  Only recorded (enforce_node_keys off): a node never seen is
     *  pending, let in
     */
    hgobj ch = open_keyed_session(gobj, "keyed-1", "uuid-keyed", "keyed", KEY_A);
    if(!ch) {
        return -1;
    }
    if(drops_of(ch) != 0) {
        ret += fail(gobj, "a node never seen is dropped while only recording", "");
    }
    ret += check_node_field(gobj, client, "uuid-keyed", "state", "pending",
        "a node never seen is recorded pending");
    ret += check_node_field(gobj, client, "uuid-keyed", "presented_spki", KEY_A,
        "a node never seen is recorded with the key it presented");

    /*
     *  accept-node pins the key it presented
     */
    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "node_id", json_string("uuid-keyed"));
    ret += check_response(gobj, gobj_command(priv->cc, "accept-node", kw, client),
        -1, "what key", "accept-node without the key read on the node");
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "node_id", json_string("uuid-keyed"));
    json_object_set_new(kw, "spki_sha256", json_string(KEY_A));
    ret += check_response(gobj, gobj_command(priv->cc, "accept-node", kw, client),
        0, "node 'uuid-keyed' accepted", "accept-node");
    ret += check_node_field(gobj, client, "uuid-keyed", "spki_sha256", KEY_A,
        "accept-node pins the key given");
    close_session(ch);

    /*
     *  Another key, only recording: let in, said
     */
    ch = open_keyed_session(gobj, "keyed-2", "uuid-keyed", "keyed", KEY_B);
    if(drops_of(ch) != 0) {
        ret += fail(gobj, "another key is dropped while only recording", "");
    }
    close_session(ch);

    /*
     *  enforce_node_keys
     */
    gobj_write_bool_attr(priv->cc, "enforce_node_keys", TRUE);

    ch = open_keyed_session(gobj, "keyed-3", "uuid-keyed", "keyed", KEY_B);
    if(drops_of(ch) != 1) {
        ret += fail(gobj, "an accepted node with another key is not dropped", "");
    }
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string("uuid-keyed"));
    json_object_set_new(kw, "cmd2agent", json_string("list-yunos"));
    ret += check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
        -1, 0, "a refused agent gets no command");
    close_session(ch);

    hgobj good = open_keyed_session(gobj, "keyed-4", "uuid-keyed", "keyed", KEY_A);
    if(drops_of(good) != 0) {
        ret += fail(gobj, "an accepted node with its key is dropped", "");
    }

    ch = open_keyed_session(gobj, "stranger-1", "uuid-stranger", "stranger", KEY_B);
    if(drops_of(ch) != 1) {
        ret += fail(gobj, "a node never seen is not dropped when enforcing", "");
    }
    close_session(ch);

    hgobj old = open_keyed_session(gobj, "old-1", "uuid-old", "old", NULL);
    if(drops_of(old) != 0) {
        ret += fail(gobj, "an agent of before (no key) is dropped while allow_nodes_without_key", "");
    }

    /*
     *  refuse-node drops the agent in session (its C_IEVENT_SRV, which
     *  drops its transport: the wire)
     */
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "node_id", json_string("uuid-keyed"));
    ret += check_response(gobj, gobj_command(priv->cc, "refuse-node", kw, client),
        0, "node 'uuid-keyed' refused", "refuse-node");
    hgobj good_wire = gobj_child_by_name(good, "keyed-4-wire");
    if(!good_wire || drops_of(good_wire) != 1) {
        ret += fail(gobj, "refuse-node does not drop the agent in session", "");
    }

    /*
     *  No key to accept; and a user without manage-nodes
     */
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "node_id", json_string("uuid-old"));
    json_object_set_new(kw, "spki_sha256", json_string("not-a-key"));
    ret += check_response(gobj, gobj_command(priv->cc, "accept-node", kw, client),
        -1, "what key", "accept-node with a key that is no sha256");
    kw = viewer_kw(client);
    json_object_set_new(kw, "node_id", json_string("uuid-keyed"));
    ret += check_response(gobj, gobj_command(priv->cc, "accept-node", kw, client),
        -1, "No permission to 'manage-nodes'", "accept-node needs manage-nodes");

    /*
     *  Authenticated by the key, no token: a node accepted with its key; and
     *  refused with another key, and as a node never seen
     */
    ch = open_keyed_session(gobj, "auth-1", "uuid-auth", "authhost", KEY_A);
    close_session(ch);
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "node_id", json_string("uuid-auth"));
    json_object_set_new(kw, "spki_sha256", json_string(KEY_A));
    ret += check_response(gobj, gobj_command(priv->cc, "accept-node", kw, client),
        0, "node 'uuid-auth' accepted", "accept-node of uuid-auth");

    char username[NAME_MAX];
    json_t *auth = authenticate_by_key(gobj, "auth-2", "uuid-auth", "authhost", KEY_A,
        username, sizeof(username));
    if(kw_get_int(gobj, auth, "result", -1, 0) != 0 || strcmp(username, "agent:uuid-auth")!=0 ||
            !kw_get_list(gobj, auth, "services_roles`controlcenter", 0, 0)) {
        char *dump = auth? json_dumps(auth, JSON_COMPACT) : 0;
        ret += fail(gobj, "an agent with the accepted key is authenticated by it", dump? dump : username);
        if(dump) {
            gbmem_free(dump);
        }
    }
    JSON_DECREF(auth)

    auth = authenticate_by_key(gobj, "auth-3", "uuid-auth", "authhost", KEY_B,
        username, sizeof(username));
    if(kw_get_int(gobj, auth, "result", 0, 0) >= 0) {
        ret += fail(gobj, "an agent with another key and no token is authenticated", username);
    }
    JSON_DECREF(auth)

    auth = authenticate_by_key(gobj, "auth-4", "uuid-new", "newhost", KEY_B,
        username, sizeof(username));
    if(kw_get_int(gobj, auth, "result", 0, 0) >= 0) {
        ret += fail(gobj, "a node never seen, with no token, is authenticated", username);
    }
    JSON_DECREF(auth)
    ret += check_node_field(gobj, client, "uuid-new", "state", "knocking",
        "a node never seen at its authentication is knocking, not recorded");

    /*
     *  Another key for the same uuid does not replace the first one kept;
     *  the operator accepts it with the key read on the node, and it logs in
     */
    auth = authenticate_by_key(gobj, "auth-5", "uuid-new", "newhost", KEY_G,
        username, sizeof(username));
    JSON_DECREF(auth)
    ret += check_node_field(gobj, client, "uuid-new", "spki_sha256", KEY_B,
        "a second key of a knocking node does not replace the first");
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "node_id", json_string("uuid-new"));
    json_object_set_new(kw, "spki_sha256", json_string(KEY_G));
    ret += check_response(gobj, gobj_command(priv->cc, "accept-node", kw, client),
        0, "node 'uuid-new' accepted", "accept-node of a knocking node");
    ret += check_node_field(gobj, client, "uuid-new", "state", "accepted",
        "accept-node of a knocking node creates its record");
    ret += check_node_field(gobj, client, "uuid-new", "hostname", "",
        "accept-node takes the hostname a node knocked with under another key");
    auth = authenticate_by_key(gobj, "auth-6", "uuid-new", "newhost", KEY_G,
        username, sizeof(username));
    if(kw_get_int(gobj, auth, "result", -1, 0) != 0) {
        ret += fail(gobj, "a knocking node accepted logs in by its key", username);
    }
    JSON_DECREF(auth)

    /*
     *  accept-node of a node never seen: the operator's uuid and key. And a
     *  full knocking table evicts the one knocked least recently
     */
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "node_id", json_string("uuid-unseen"));
    json_object_set_new(kw, "spki_sha256", json_string(KEY_B));
    ret += check_response(gobj, gobj_command(priv->cc, "accept-node", kw, client),
        0, "node 'uuid-unseen' accepted", "accept-node of a node never seen");
    ret += check_node_field(gobj, client, "uuid-unseen", "spki_sha256", KEY_B,
        "accept-node of a node never seen pins the key given");

    gobj_write_integer_attr(priv->cc, "max_knocking_nodes", 2);
    const char *flood[] = {"uuid-k1", "uuid-k2", "uuid-k3", 0};
    for(int i=0; flood[i]; i++) {
        char name[NAME_MAX];
        snprintf(name, sizeof(name), "knock-%d", i);
        auth = authenticate_by_key(gobj, name, flood[i], "flood", KEY_B, username, sizeof(username));
        JSON_DECREF(auth)
    }
    json_t *first = node_record(gobj, client, "uuid-k1");
    json_t *last = node_record(gobj, client, "uuid-k3");
    if(first || !last) {
        ret += fail(gobj, "a full knocking table evicts the one knocked least recently", "");
    }
    JSON_DECREF(first)
    JSON_DECREF(last)
    gobj_write_integer_attr(priv->cc, "max_knocking_nodes", 256);

    gobj_write_bool_attr(priv->cc, "enforce_node_keys", FALSE);
    close_session(good);
    close_session(old);
    return ret;
}

/*  The wire below the C_IEVENT_SRV of an agent's `channel`  */
PRIVATE hgobj wire_of(hgobj channel)
{
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "%s-wire", gobj_name(channel));
    return gobj_child_by_name(channel, name);
}

/*  How many frames reached the agent of `channel` (they are taken)  */
PRIVATE int frames_to(hgobj channel)
{
    json_t *received = take_received(wire_of(channel));
    int n = (int)json_array_size(received);
    JSON_DECREF(received)
    return n;
}

/*  Two counts of frames are not the ones expected (both are taken first)  */
PRIVATE BOOL frames_differ(int got1, int got2, int expected1, int expected2)
{
    return got1 != expected1 || got2 != expected2;
}

/*  command-agent or stats-agent for `agent_id`: the answer (yours)  */
PRIVATE json_t *to_agent(hgobj gobj, hgobj client, const char *command, const char *agent_id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(agent_id));
    json_object_set_new(kw, "cmd2agent", json_string("list-yunos"));
    json_object_set_new(kw, "stats2agent", json_string("yuno"));
    return gobj_command(priv->cc, command, kw, client);
}

/*  A key typed in the console of `agent_id`  */
PRIVATE void write_tty_to(hgobj gobj, hgobj client, const char *agent_id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(agent_id));
    json_object_set_new(kw, "name", json_string("console-a"));
    json_object_set_new(kw, "content64", json_string("bHM="));
    gobj_send_event(priv->cc, EV_WRITE_TTY, kw, client);
}

/*  accept-node of `uuid` with `spki`, its answer compared  */
PRIVATE int accept_node_with(hgobj gobj, hgobj client, const char *uuid, const char *spki,
    int expected_result, const char *comment_part, const char *what)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "node_id", json_string(uuid));
    json_object_set_new(kw, "spki_sha256", json_string(spki));
    return check_response(gobj, gobj_command(priv->cc, "accept-node", kw, client),
        expected_result, comment_part, what);
}

/***************************************************************************
 *  23. A node is reached by its uuid, the identity its session was
 *      accepted for, and by a hostname only when one node has it: the
 *      hostname is what the agent writes in its card. Nodes A and B,
 *      accepted with their keys, enforce_node_keys on:
 *      - A gives B's hostname, its channel first: two nodes have that
 *        name (a WARNING when the second opens), so command-agent,
 *        stats-agent and write-tty by it are refused and reach neither;
 *        write-tty without agent_id reaches no agent; B's uuid reaches B;
 *      - A gives B's uuid as its hostname, its channel first: B's uuid
 *        reaches B, for command-agent and stats-agent;
 *      - A back with its own name: B's hostname reaches B;
 *      - a session with no key (allow_nodes_without_key) that gives B's
 *        hostname, its channel first, takes nothing either.
 *      Up to 7.30.14 the first channel whose hostname or uuid matched took
 *      it, and write-tty went to every match (to all agents without
 *      agent_id).
 ***************************************************************************/
PRIVATE int test_node_names(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    hgobj ch = open_keyed_session(gobj, "na-0", "uuid-na", "host-a", KEY_C);
    close_session(ch);
    ret += accept_node_with(gobj, client, "uuid-na", KEY_C, 0, "accepted", "accept-node of A");
    ch = open_keyed_session(gobj, "nb-0", "uuid-nb", "host-b", KEY_D);
    close_session(ch);
    ret += accept_node_with(gobj, client, "uuid-nb", KEY_D, 0, "accepted", "accept-node of B");
    gobj_write_bool_attr(priv->cc, "enforce_node_keys", TRUE);

    /*
     *  A gives B's hostname
     */
    hgobj a = open_key_login(gobj, "na-1", "uuid-na", "host-b", KEY_C);
    hgobj b = open_key_login(gobj, "nb-1", "uuid-nb", "host-b", KEY_D);
    if(!a || !b) {
        return -1;  // Error already logged
    }
    if(frames_differ(drops_of(a), drops_of(b), 0, 0)) {
        ret += fail(gobj, "an accepted node with its key is dropped", "");
    }
    ret += check_response(gobj, to_agent(gobj, client, "command-agent", "host-b"),
        -1, "more than one node", "command-agent by a hostname two nodes give is refused");
    ret += check_response(gobj, to_agent(gobj, client, "stats-agent", "host-b"),
        -1, "more than one node", "stats-agent by a hostname two nodes give is refused");
    if(frames_differ(frames_to(a), frames_to(b), 0, 0)) {
        ret += fail(gobj, "a command by a hostname two nodes give reaches an agent", "");
    }
    write_tty_to(gobj, client, "host-b");
    if(frames_differ(frames_to(a), frames_to(b), 0, 0)) {
        ret += fail(gobj, "a write-tty by a hostname two nodes give reaches an agent", "");
    }
    write_tty_to(gobj, client, "");
    if(frames_differ(frames_to(a), frames_to(b), 0, 0)) {
        ret += fail(gobj, "a write-tty without agent_id reaches an agent", "");
    }
    json_t *received = take_received(priv->agent_wire);
    if(json_array_size(received) != 0) {
        ret += fail(gobj, "a write-tty without agent_id reaches the agent of " AGENT_HOST, "");
    }
    JSON_DECREF(received)
    ret += check_response(gobj, to_agent(gobj, client, "command-agent", "uuid-nb"),
        0, "sent to 1 nodes", "command-agent by B's uuid");
    if(frames_differ(frames_to(a), frames_to(b), 0, 1)) {
        ret += fail(gobj, "B's uuid reaches B, and B only", "");
    }
    close_session(a);
    close_session(b);

    /*
     *  A gives B's uuid as its hostname
     */
    a = open_key_login(gobj, "na-2", "uuid-na", "uuid-nb", KEY_C);
    b = open_key_login(gobj, "nb-2", "uuid-nb", "host-b", KEY_D);
    ret += check_response(gobj, to_agent(gobj, client, "command-agent", "uuid-nb"),
        0, "sent to 1 nodes", "command-agent by B's uuid, A giving it as its hostname");
    if(frames_differ(frames_to(a), frames_to(b), 0, 1)) {
        ret += fail(gobj, "command-agent: B's uuid given as A's hostname reaches A", "");
    }
    ret += check_response(gobj, to_agent(gobj, client, "stats-agent", "uuid-nb"),
        0, "sent to 1 nodes", "stats-agent by B's uuid, A giving it as its hostname");
    if(frames_differ(frames_to(a), frames_to(b), 0, 1)) {
        ret += fail(gobj, "stats-agent: B's uuid given as A's hostname reaches A", "");
    }
    close_session(a);
    close_session(b);

    /*
     *  A back with its own name: B's hostname is B's
     */
    a = open_key_login(gobj, "na-3", "uuid-na", "host-a", KEY_C);
    b = open_key_login(gobj, "nb-3", "uuid-nb", "host-b", KEY_D);
    ret += check_response(gobj, to_agent(gobj, client, "command-agent", "host-b"),
        0, "sent to 1 nodes", "command-agent by a hostname of one node");
    if(frames_differ(frames_to(a), frames_to(b), 0, 1)) {
        ret += fail(gobj, "a hostname of one node reaches it, and it only", "");
    }
    close_session(a);
    close_session(b);

    /*
     *  A session with no key gives B's hostname
     */
    hgobj x = open_keyed_session(gobj, "nx-1", "uuid-nx", "host-b", NULL);
    b = open_key_login(gobj, "nb-4", "uuid-nb", "host-b", KEY_D);
    if(drops_of(x) != 0) {
        ret += fail(gobj, "a node with no key is dropped while allow_nodes_without_key", "");
    }
    ret += check_response(gobj, to_agent(gobj, client, "command-agent", "host-b"),
        -1, "more than one node", "command-agent by a hostname a session of no key gives too");
    if(frames_differ(frames_to(x), frames_to(b), 0, 0)) {
        ret += fail(gobj, "a session of no key takes a hostname of another node", "");
    }
    close_session(x);
    close_session(b);

    gobj_write_bool_attr(priv->cc, "enforce_node_keys", FALSE);
    return ret;
}

/***************************************************************************
 *  24. One key, one node: accept-node of a key already accepted for
 *      another node is refused (a WARNING): a copy of a node's disk carries
 *      its pair, and the copy, under another uuid, would log in as both.
 *      Up to 7.30.14 it was accepted.
 ***************************************************************************/
PRIVATE int test_one_key_one_node(hgobj gobj, hgobj client)
{
    int ret = 0;

    ret += accept_node_with(gobj, client, "uuid-dup", KEY_C, -1, "already accepted for node 'uuid-na'",
        "accept-node of a key accepted for another node");
    json_t *node = node_record(gobj, client, "uuid-dup");
    if(node) {
        ret += fail(gobj, "a key accepted for another node is written for this one", "");
    }
    JSON_DECREF(node)
    ret += accept_node_with(gobj, client, "uuid-na", KEY_C, 0, "accepted",
        "accept-node of a node with the key it has");
    return ret;
}

/***************************************************************************
 *  25. The card of an agent not authenticated yet is checked before
 *      anything of it is kept: an id that is not a string, or not shaped
 *      as a node uuid (a backtick, 300 characters), and a host that is not
 *      a string or is longer than a host name are refused, a WARNING each,
 *      and none of them is knocking. Up to 7.30.14 a value that was not a
 *      string gave an ERROR with a stack, and the others were kept.
 ***************************************************************************/
PRIVATE int test_card_checked(hgobj gobj, hgobj client)
{
    int ret = 0;
    char username[NAME_MAX];

    char long_id[301];
    memset(long_id, 'a', sizeof(long_id)-1);
    long_id[sizeof(long_id)-1] = 0;
    char long_host[HOST_NAME_MAX+2];
    memset(long_host, 'h', sizeof(long_host)-1);
    long_host[sizeof(long_host)-1] = 0;

    json_t *cards[] = {
        json_pack("{s:i, s:{s:[{s:s, s:s}]}}",
            "id", 7,
            "__md_iev__", "ievent_gate_stack", "dst_service", "controlcenter", "host", "h"),
        json_pack("{s:s, s:{s:[{s:s, s:i}]}}",
            "id", "uuid-hostint",
            "__md_iev__", "ievent_gate_stack", "dst_service", "controlcenter", "host", 7),
        json_pack("{s:s, s:{s:[{s:s, s:s}]}}",
            "id", "uuid`tick",
            "__md_iev__", "ievent_gate_stack", "dst_service", "controlcenter", "host", "h"),
        json_pack("{s:s, s:{s:[{s:s, s:s}]}}",
            "id", long_id,
            "__md_iev__", "ievent_gate_stack", "dst_service", "controlcenter", "host", "h"),
        json_pack("{s:s, s:{s:[{s:s, s:s}]}}",
            "id", "uuid-longhost",
            "__md_iev__", "ievent_gate_stack", "dst_service", "controlcenter", "host", long_host),
        0
    };
    const char *ids[] = {"7", "uuid-hostint", "uuid`tick", long_id, "uuid-longhost", 0};
    for(int i=0; cards[i]; i++) {
        char name[NAME_MAX];
        snprintf(name, sizeof(name), "card-%d", i);
        json_t *auth = authenticate_card(gobj, name, cards[i], KEY_B, username, sizeof(username));
        if(kw_get_int(gobj, auth, "result", 0, 0) >= 0) {
            ret += fail(gobj, "a card that is not valid is authenticated", ids[i]);
        }
        JSON_DECREF(auth)
        json_t *node = node_record(gobj, client, ids[i]);
        if(node) {
            ret += fail(gobj, "a card that is not valid is kept knocking", name);
        }
        JSON_DECREF(node)
    }
    return ret;
}

/***************************************************************************
 *  26. A refused node whose agent presents its key is said refused, not
 *      "another key" (the WARNING's `refusal`). Up to 7.30.14 it said
 *      another key.
 ***************************************************************************/
PRIVATE int test_refused_node_by_key(hgobj gobj)
{
    char username[NAME_MAX];
    json_t *auth = authenticate_by_key(gobj, "auth-r", "uuid-keyed", "keyed", KEY_A,
        username, sizeof(username));
    int ret = 0;
    if(kw_get_int(gobj, auth, "result", 0, 0) >= 0) {
        ret = fail(gobj, "a refused node is authenticated by its key", username);
    }
    JSON_DECREF(auth)
    return ret;
}

/*  The g_rowid of the record of `uuid` in the control center's treedb, or -1  */
PRIVATE json_int_t node_rowid(hgobj gobj, const char *uuid)
{
    hgobj treedb = gobj_find_service("treedb_controlcenter", TRUE);
    json_t *node = treedb? gobj_get_node(treedb, "nodes", json_pack("{s:s}", "id", uuid),
        json_pack("{s:b}", "with_metadata", 1), gobj) : NULL;
    json_int_t rowid = node? kw_get_int(gobj, node, "__md_treedb__`g_rowid", -1, 0) : -1;
    JSON_DECREF(node)
    return rowid;
}

/***************************************************************************
 *  27. An agent that opens its session again with nothing changed (its
 *      hostname, address and key) writes nothing: its link comes from a new
 *      port each time, and the port is no change. A new hostname is
 *      written, and so is a new address. Up to 7.30.14 every open wrote the
 *      record; in 7.30.15 every open from a new port did (every one).
 ***************************************************************************/
PRIVATE int test_record_written_on_change(hgobj gobj)
{
    int ret = 0;

    hgobj ch = open_key_login(gobj, "na-4", "uuid-na", "host-a", KEY_C);
    close_session(ch);
    json_int_t rowid = node_rowid(gobj, "uuid-na");
    ch = open_key_login(gobj, "na-5", "uuid-na", "host-a", KEY_C);
    close_session(ch);
    json_int_t rowid2 = node_rowid(gobj, "uuid-na");
    if(rowid < 0 || rowid2 != rowid) {
        char detail[80];
        snprintf(detail, sizeof(detail), "g_rowid %lld, then %lld", (long long)rowid, (long long)rowid2);
        ret += fail(gobj, "an open with nothing changed but its port writes the record of the node", detail);
    }
    ch = open_key_login(gobj, "na-6", "uuid-na", "host-a2", KEY_C);
    close_session(ch);
    if(node_rowid(gobj, "uuid-na") != rowid + 1) {
        ret += fail(gobj, "an open with a new hostname does not write it", "");
    }
    peer_ip = "10.0.0.2";
    ch = open_key_login(gobj, "na-6b", "uuid-na", "host-a2", KEY_C);
    close_session(ch);
    peer_ip = "10.0.0.1";
    if(node_rowid(gobj, "uuid-na") != rowid + 2) {
        ret += fail(gobj, "an open from a new address does not write it", "");
    }
    return ret;
}

/***************************************************************************
 *  28. A decision about a node drops its sessions whose key it does not
 *      accept now (an INFO each):
 *      - enforce_node_keys off: accept-node with the key a session logged
 *        in by keeps it; with a new key, drops it, and keeps a session of
 *        a token (let in with any key, as at its open); refuse-node drops
 *        a session logged in by the key;
 *      - enforce_node_keys on: accept-node with a new key drops a session
 *        that presents the old one.
 *      Up to 7.30.14 the session of a key no longer accepted stayed, and
 *      refuse-node dropped one session, and only with enforce_node_keys.
 ***************************************************************************/
PRIVATE int test_key_revoked(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    hgobj ch = open_keyed_session(gobj, "nr-0", "uuid-nr", "host-r", KEY_E);
    close_session(ch);
    ret += accept_node_with(gobj, client, "uuid-nr", KEY_E, 0, "accepted", "accept-node of R");

    hgobj r1 = open_key_login(gobj, "nr-1", "uuid-nr", "host-r", KEY_E);
    hgobj t1 = open_keyed_session(gobj, "nt-1", "uuid-nr", "host-r", KEY_E);    // a token's
    ret += accept_node_with(gobj, client, "uuid-nr", KEY_E, 0, "accepted", "accept-node of R again");
    if(drops_of(wire_of(r1)) != 0) {
        ret += fail(gobj, "accept-node with the key a session logged in by drops it", "");
    }
    ret += accept_node_with(gobj, client, "uuid-nr", KEY_F, 0, "accepted", "accept-node of R, a new key");
    if(frames_differ(drops_of(wire_of(r1)), drops_of(wire_of(t1)), 1, 0)) {
        ret += fail(gobj, "accept-node with a new key keeps the session logged in by the old one, "
            "or drops one of a token without enforce_node_keys", "");
    }
    close_session(r1);
    close_session(t1);

    hgobj r2 = open_key_login(gobj, "nr-2", "uuid-nr", "host-r", KEY_F);
    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "node_id", json_string("uuid-nr"));
    ret += check_response(gobj, gobj_command(priv->cc, "refuse-node", kw, client),
        0, "node 'uuid-nr' refused", "refuse-node of R");
    if(drops_of(wire_of(r2)) != 1) {
        ret += fail(gobj, "refuse-node keeps a session logged in by the key", "");
    }
    close_session(r2);

    gobj_write_bool_attr(priv->cc, "enforce_node_keys", TRUE);
    ret += accept_node_with(gobj, client, "uuid-nr", KEY_F, 0, "accepted", "accept-node of R, enforcing");
    hgobj r3 = open_keyed_session(gobj, "nr-3", "uuid-nr", "host-r", KEY_F);
    if(drops_of(r3) != 0) {
        ret += fail(gobj, "an accepted node with its key is dropped", "");
    }
    ret += accept_node_with(gobj, client, "uuid-nr", KEY_E, 0, "accepted", "accept-node of R, the old key");
    if(drops_of(wire_of(r3)) != 1) {
        ret += fail(gobj, "accept-node with a new key keeps the session of the old one, enforcing", "");
    }
    close_session(r3);
    gobj_write_bool_attr(priv->cc, "enforce_node_keys", FALSE);
    return ret;
}

/***************************************************************************
 *  29. logout-user asks its permission, and says `disabled` only when it
 *      is given: a user without `logout-user` reaches nothing; without
 *      `disabled` the authz service is not told to enable the user, with
 *      it, it is told what was given. Up to 7.30.14 anybody logged anybody
 *      out, and `disabled` went as false when not given (a disabled user
 *      was enabled again).
 ***************************************************************************/
PRIVATE int test_logout_user(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    hgobj authz = gobj_find_service("authz", TRUE);
    json_t *received = take_received(authz);
    JSON_DECREF(received)

    json_t *kw = viewer_kw(client);
    json_object_set_new(kw, "username", json_string("bob"));
    json_object_set_new(kw, "disabled", json_true());
    ret += check_response(gobj, gobj_command(priv->cc, "logout-user", kw, client),
        -1, "No permission to 'logout-user'", "logout-user needs its permission");
    if(count_received(authz, EV_REJECT_USER, NULL) != 0) {
        ret += fail(gobj, "logout-user without its permission reaches the authz service", "");
    }

    const char *given[] = {NULL, "1", "0", 0};
    for(int i=0; i<3; i++) {
        kw = client_kw(gobj_name(client));
        json_object_set_new(kw, "username", json_string("bob"));
        if(given[i]) {
            json_object_set_new(kw, "disabled", json_boolean(given[i][0]=='1'));
        }
        ret += check_response(gobj, gobj_command(priv->cc, "logout-user", kw, client),
            0, 0, "logout-user");
        received = take_received(authz);
        json_t *kw_reject = json_object_get(json_array_get(received, 0), "kw");
        json_t *disabled = json_object_get(kw_reject, "disabled");
        BOOL right = json_array_size(received) == 1 &&
            strcmp(kw_get_str(gobj, kw_reject, "username", "", 0), "bob")==0 &&
            (given[i]? json_is_boolean(disabled) && json_is_true(disabled) == (given[i][0]=='1') :
                disabled == NULL);
        if(!right) {
            ret += fail(gobj, "logout-user tells the authz service `disabled` only as given",
                given[i]? given[i] : "(not given)");
        }
        JSON_DECREF(received)
    }
    return ret;
}


/***************************************************************************
 *  30. A hostname cannot be planted, nor taken by a uuid. Node C is
 *      accepted with its key, enforce_node_keys on, its agent logged in by
 *      the key with `host-c`:
 *      - a session of a token that claims C's uuid with another hostname is
 *        refused, and C's record keeps `host-c`: only a session logged in
 *        by the key of an accepted node writes its hostname and address;
 *      - a session of a token whose card id IS `host-c` takes nothing by
 *        that name (a uuid of a session and a hostname of another node:
 *        ambiguous), and C's uuid still reaches C;
 *      - a session of a token with another uuid that gives `host-c` takes
 *        nothing either: two records have the name;
 *      - the record of a refused node names nobody.
 *      Up to the first fix the refused open renamed C's record, so the
 *      last session was the only `host-c` and got C's commands; and a card
 *      id of `host-c` was matched as a uuid, first.
 ***************************************************************************/
PRIVATE int test_names_not_planted(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    hgobj ch = open_keyed_session(gobj, "nc-0", "uuid-nc", "host-c", KEY_H);
    close_session(ch);
    ret += accept_node_with(gobj, client, "uuid-nc", KEY_H, 0, "accepted", "accept-node of C");
    gobj_write_bool_attr(priv->cc, "enforce_node_keys", TRUE);
    hgobj c = open_key_login(gobj, "nc-1", "uuid-nc", "host-c", KEY_H);

    hgobj x = open_keyed_session(gobj, "nx-2", "uuid-nc", "elsewhere", NULL);
    if(drops_of(x) != 1) {
        ret += fail(gobj, "a session of no key claiming an accepted node is not dropped", "");
    }
    close_session(x);
    ret += check_node_field(gobj, client, "uuid-nc", "hostname", "host-c",
        "a refused open writes the hostname of an accepted node");

    hgobj y = open_keyed_session(gobj, "ny-1", "host-c", "y-host", NULL);
    ret += check_response(gobj, to_agent(gobj, client, "command-agent", "host-c"),
        -1, "more than one node", "command-agent by a name that is a session's uuid and another's hostname");
    if(frames_differ(frames_to(y), frames_to(c), 0, 0)) {
        ret += fail(gobj, "a card id that is another node's hostname takes its commands", "");
    }
    ret += check_response(gobj, to_agent(gobj, client, "command-agent", "uuid-nc"),
        0, "sent to 1 nodes", "command-agent by C's uuid");
    if(frames_differ(frames_to(y), frames_to(c), 0, 1)) {
        ret += fail(gobj, "C's uuid reaches C, and C only", "");
    }
    close_session(y);

    hgobj x2 = open_keyed_session(gobj, "nx-3", "uuid-nx3", "host-c", NULL);
    ret += check_response(gobj, to_agent(gobj, client, "command-agent", "host-c"),
        -1, "more than one node", "command-agent by a hostname a session of a token gives too");
    if(frames_differ(frames_to(x2), frames_to(c), 0, 0)) {
        ret += fail(gobj, "a session of a token takes the hostname of an accepted node", "");
    }
    close_session(x2);
    close_session(c);
    gobj_write_bool_attr(priv->cc, "enforce_node_keys", FALSE);

    /*
     *  The record of a refused node names nobody: `keyed` (case 22) names
     *  the one node not refused that has it
     */
    hgobj k = open_keyed_session(gobj, "nk-1", "uuid-nk", "keyed", NULL);
    ret += check_response(gobj, to_agent(gobj, client, "command-agent", "keyed"),
        0, "sent to 1 nodes", "command-agent by a hostname a refused node had");
    if(frames_to(k) != 1) {
        ret += fail(gobj, "the hostname of a refused node blocks another node's", "");
    }
    close_session(k);
    return ret;
}

/***************************************************************************
 *  31. drop-agent reaches a node as command-agent does. enforce_node_keys
 *      off: A, by a token (no key, let in with a WARNING), gives the
 *      hostname B logged in with by its key. drop-agent by that name drops
 *      B's session, and not A's: A's record keeps its own name. Up to
 *      7.30.14 it dropped every session whose uuid or card host matched.
 ***************************************************************************/
PRIVATE int test_drop_agent_by_name(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    hgobj a = open_keyed_session(gobj, "na-7", "uuid-na", "host-b2", NULL);
    hgobj b = open_key_login(gobj, "nb-5", "uuid-nb", "host-b2", KEY_D);
    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string("host-b2"));
    ret += check_response(gobj, gobj_command(priv->cc, "drop-agent", kw, client),
        0, "Drop sent to 1 nodes", "drop-agent by a hostname");
    if(frames_differ(drops_of(wire_of(a)), drops_of(wire_of(b)), 0, 1)) {
        ret += fail(gobj, "drop-agent by B's hostname drops A, or not B", "");
    }
    close_session(a);
    close_session(b);
    return ret;
}

/***************************************************************************
 *  32. The hostname of a node without a key (a pending node: an agent of
 *      before 7.30.10, logged in by the shared token) is not taken through
 *      its record. enforce_node_keys on, allow_nodes_without_key (the
 *      configuration of production). Node P is in session, no key, its
 *      card `host-np`:
 *      - a token with a key that claims P's uuid is refused, and writes
 *        nothing: P's record keeps `host-np`;
 *      - a token with no key that claims P's uuid (let in: a node of
 *        before), and another uuid that gives `host-np`: the name is two
 *        nodes', command-agent and write-tty by it reach nobody;
 *      - both gone, `host-np` reaches P.
 *      In 7.30.15 either open renamed P's record, and the other uuid,
 *      the one record left with `host-np`, got P's commands and keys; gone,
 *      P was not reached by its name until it opened again.
 ***************************************************************************/
PRIVATE int test_pending_name_not_taken(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    gobj_write_bool_attr(priv->cc, "enforce_node_keys", TRUE);
    hgobj p = open_keyed_session(gobj, "np-1", "uuid-np", "host-np", NULL);
    if(drops_of(p) != 0) {
        ret += fail(gobj, "a node with no key is dropped while allow_nodes_without_key", "");
    }

    hgobj r = open_keyed_session(gobj, "nq-1", "uuid-np", "host-evil", KEY_J);
    if(drops_of(r) != 1) {
        ret += fail(gobj, "a token with a key claiming a pending node is not dropped", "");
    }
    close_session(r);
    ret += check_node_field(gobj, client, "uuid-np", "hostname", "host-np",
        "a refused open renames the record of a pending node");

    hgobj t = open_keyed_session(gobj, "nq-2", "uuid-np", "host-evil", NULL);
    hgobj x = open_keyed_session(gobj, "nq-3", "uuid-nq", "host-np", NULL);
    ret += check_response(gobj, to_agent(gobj, client, "command-agent", "host-np"),
        -1, "more than one node", "command-agent by the name of a pending node another uuid gives");
    write_tty_to(gobj, client, "host-np");
    int got_p = frames_to(p);
    int got_x = frames_to(x);
    if(got_x != 0 || got_p != 0 || frames_to(t) != 0) {
        ret += fail(gobj, "the name of a pending node is taken by another uuid", "");
    }
    close_session(x);
    close_session(t);

    ret += check_response(gobj, to_agent(gobj, client, "command-agent", "host-np"),
        0, "sent to 1 nodes", "command-agent by the name of a pending node, the others gone");
    if(frames_to(p) != 1) {
        ret += fail(gobj, "a pending node is not reached by its name once the others are gone", "");
    }
    close_session(p);
    gobj_write_bool_attr(priv->cc, "enforce_node_keys", FALSE);
    return ret;
}

/***************************************************************************
 *  33. An open does not leave a name behind. Node B is accepted with its
 *      key, its hostname `host-p13`; enforce_node_keys on:
 *      - allow_nodes_without_key off: a token with a key and a new uuid
 *        that gives `host-p13` is refused, and kept knocking, not written;
 *      - allow_nodes_without_key on: a token with no key and a new uuid
 *        that gives `host-p13` is let in (a pending node), and goes;
 *      then B logs in by its key: `host-p13` reaches B. In 7.30.15 each of
 *      them left a record with `host-p13`, and B's name was more than one
 *      node's for ever (until a refuse-node of the other uuid).
 ***************************************************************************/
PRIVATE int test_open_leaves_no_name(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    hgobj ch = open_keyed_session(gobj, "nb13-0", "uuid-nb13", "host-p13", KEY_I);
    close_session(ch);
    ret += accept_node_with(gobj, client, "uuid-nb13", KEY_I, 0, "accepted", "accept-node of B13");

    gobj_write_bool_attr(priv->cc, "enforce_node_keys", TRUE);
    gobj_write_bool_attr(priv->cc, "allow_nodes_without_key", FALSE);
    hgobj r = open_keyed_session(gobj, "np13-1", "uuid-p13", "host-p13", KEY_J);
    if(drops_of(r) != 1) {
        ret += fail(gobj, "a node never seen is not dropped when enforcing", "");
    }
    close_session(r);
    ret += check_node_field(gobj, client, "uuid-p13", "state", "knocking",
        "a refused open of a node never seen is kept knocking, not written");

    gobj_write_bool_attr(priv->cc, "allow_nodes_without_key", TRUE);
    hgobj k = open_keyed_session(gobj, "np13-2", "uuid-p13k", "host-p13", NULL);
    if(drops_of(k) != 0) {
        ret += fail(gobj, "a node with no key is dropped while allow_nodes_without_key", "");
    }
    close_session(k);

    hgobj b = open_key_login(gobj, "nb13-1", "uuid-nb13", "host-p13", KEY_I);
    ret += check_response(gobj, to_agent(gobj, client, "command-agent", "host-p13"),
        0, "sent to 1 nodes", "command-agent by B13's hostname, after opens that gave it");
    if(frames_to(b) != 1) {
        ret += fail(gobj, "an open that gave B13's hostname leaves it to nobody", "");
    }
    close_session(b);
    gobj_write_bool_attr(priv->cc, "enforce_node_keys", FALSE);
    return ret;
}

/***************************************************************************
 *  37. Accepting a node does not keep the name a session of a token wrote
 *      in its record. enforce_node_keys off. Node B is accepted with its
 *      key, its hostname `host-q12b`. Node P presented its key once (a
 *      pending record), then a session of a token with no key claimed P's
 *      uuid and gave `host-q12b`. accept-node of P with its key leaves its
 *      hostname and address empty, and `host-q12b` reaches B. Node R, whose
 *      last session presented the key accepted, keeps its hostname. In
 *      7.30.16 P was accepted named `host-q12b`, and the name was two
 *      nodes' until P logged in by its key.
 ***************************************************************************/
#define KEY_K   "4444444444444444444444444444444444444444444444444444444444444444"
#define KEY_L   "5555555555555555555555555555555555555555555555555555555555555555"
#define KEY_M   "6666666666666666666666666666666666666666666666666666666666666666"

PRIVATE int test_accept_drops_token_name(hgobj gobj, hgobj client)
{
    int ret = 0;

    hgobj ch = open_keyed_session(gobj, "q12b-0", "uuid-q12b", "host-q12b", KEY_K);
    close_session(ch);
    ret += accept_node_with(gobj, client, "uuid-q12b", KEY_K, 0, "accepted", "accept-node of B (37)");

    ch = open_keyed_session(gobj, "q12p-0", "uuid-q12p", "host-q12p", KEY_L);
    close_session(ch);
    hgobj t = open_keyed_session(gobj, "q12t-1", "uuid-q12p", "host-q12b", NULL);
    close_session(t);
    ret += check_node_field(gobj, client, "uuid-q12p", "hostname", "host-q12b",
        "a session of a token writes the hostname of a pending node");
    ret += accept_node_with(gobj, client, "uuid-q12p", KEY_L, 0, "accepted", "accept-node of P (37)");
    ret += check_node_field(gobj, client, "uuid-q12p", "hostname", "",
        "accept-node keeps the hostname a session of a token wrote");
    ret += check_node_field(gobj, client, "uuid-q12p", "peername", "",
        "accept-node keeps the address a session of a token wrote");

    hgobj b = open_key_login(gobj, "q12b-1", "uuid-q12b", "host-q12b", KEY_K);
    ret += check_response(gobj, to_agent(gobj, client, "command-agent", "host-q12b"),
        0, "sent to 1 nodes", "command-agent by B's hostname, after P is accepted");
    if(frames_to(b) != 1) {
        ret += fail(gobj, "B is not reached by its hostname after P is accepted", "");
    }
    close_session(b);

    ch = open_keyed_session(gobj, "q12r-0", "uuid-q12r", "host-q12r", KEY_M);
    close_session(ch);
    ret += accept_node_with(gobj, client, "uuid-q12r", KEY_M, 0, "accepted", "accept-node of R (37)");
    ret += check_node_field(gobj, client, "uuid-q12r", "hostname", "host-q12r",
        "accept-node drops the hostname written by a session that presented the key accepted");
    return ret;
}

/***************************************************************************
 *  34. enforce_node_keys off: a session of a token that claims the uuid of
 *      an accepted node, its channel first, does not take what goes to the
 *      node: the session logged in by the node's key gets it. In 7.30.15
 *      the first session of the uuid got it.
 *      And the card of a session of a token is checked: an `id` or a `host`
 *      that is not a string is refused (a WARNING), and nothing that looks
 *      for an agent after it (command-agent, list-agents, another agent's
 *      open) logs. In 7.30.15 it was let in, and each of them gave an
 *      ERROR with a stack. A `knows_operator_assertion` that is not a
 *      boolean (1, "yes") is no: the command goes with no assertion, and
 *      nothing logs (in 7.30.15 an ERROR with a stack at each command).
 ***************************************************************************/
PRIVATE int test_key_login_first_and_cards(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    hgobj t = open_keyed_session(gobj, "p1-t", "uuid-na", "host-a2", NULL);
    hgobj a = open_key_login(gobj, "p1-a", "uuid-na", "host-a2", KEY_C);
    ret += check_response(gobj, to_agent(gobj, client, "command-agent", "uuid-na"),
        0, "sent to 1 nodes", "command-agent by an accepted node's uuid, a token claiming it too");
    if(frames_differ(frames_to(t), frames_to(a), 0, 1)) {
        ret += fail(gobj, "a token claiming an accepted node's uuid takes what goes to the node", "");
    }
    close_session(t);
    close_session(a);

    json_t *cards[] = {
        json_pack("{s:i, s:{s:[{s:s}]}}",
            "id", 7, "__md_iev__", "ievent_gate_stack", "host", "h7"),
        json_pack("{s:s, s:{s:[{s:i}]}}",
            "id", "uuid-p7", "__md_iev__", "ievent_gate_stack", "host", 7),
        0
    };
    hgobj bad[2] = {0, 0};
    for(int i=0; cards[i]; i++) {
        char name[NAME_MAX];
        snprintf(name, sizeof(name), "p7-%d", i);
        bad[i] = open_session_card(gobj, name, cards[i], NULL, NULL);
        if(drops_of(bad[i]) != 1) {
            ret += fail(gobj, "a session of a token with a card not valid is let in", name);
        }
    }
    ret += check_response(gobj, to_agent(gobj, client, "command-agent", "uuid-p7"),
        -1, "sent to 0 nodes", "command-agent to a session with a card not valid");
    json_t *agents = gobj_command(priv->cc, "list-agents", client_kw(gobj_name(client)), client);
    JSON_DECREF(agents)
    hgobj other = open_keyed_session(gobj, "p7-other", "uuid-p7o", "h7", NULL);
    close_session(other);
    for(int i=0; i<2; i++) {
        close_session(bad[i]);
    }

    /*
     *  `knows_operator_assertion` that is not a boolean: the agent does not
     *  know it (no assertion is sent), and nothing is logged
     */
    json_t *knows[] = {json_integer(1), json_string("yes"), 0};
    for(int i=0; knows[i]; i++) {
        char name[NAME_MAX];
        char uuid[NAME_MAX];
        snprintf(name, sizeof(name), "p7-knows-%d", i);
        snprintf(uuid, sizeof(uuid), "uuid-p7k%d", i);
        json_t *card = json_pack("{s:s, s:o, s:{s:[{s:s}]}}",
            "id", uuid,
            "knows_operator_assertion", knows[i],
            "__md_iev__", "ievent_gate_stack", "host", name
        );
        hgobj k = open_session_card(gobj, name, card, NULL, NULL);
        json_t *kw = client_kw(gobj_name(client));
        json_object_set_new(kw, "__username__", json_string("alice@example.com"));
        json_object_set_new(kw, "agent_id", json_string(uuid));
        json_object_set_new(kw, "cmd2agent", json_string("list-yunos"));
        ret += check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
            0, "sent to 1 nodes", "command-agent to an agent whose knows_operator_assertion is no boolean");
        json_t *received = take_received(wire_of(k));
        size_t n = json_array_size(received);
        json_t *request = n? json_object_get(json_array_get(received, n-1), "kw") : NULL;
        if(!request || json_object_get(request, "__operator_assertion__")) {
            ret += fail(gobj, "an agent whose knows_operator_assertion is no boolean gets an assertion", name);
        }
        JSON_DECREF(received)
        close_session(k);
    }
    return ret;
}

/***************************************************************************
 *  35. A scenario over a name that more than one node gives. The scenario
 *      "amb" has a yuno on AGENT_HOST and one on `host-amb`, which two
 *      nodes without a key give:
 *      - its start is refused, saying so ("more than one node"), and
 *        nothing reaches any agent;
 *      - its stop stops what it reaches (the agent of AGENT_HOST) and names
 *        the rest: the run fails with "node 'host-amb' is the hostname of
 *        more than one node", and neither node of `host-amb` gets a step.
 *      In 7.30.15 the start ran its first step, and said "a node of the run
 *      is not connected" at the second; the stop said "not connected".
 ***************************************************************************/
PRIVATE json_t *amb_scenario(void)
{
    return json_pack(
        "{s:s, s:s, s:[{s:s}, {s:s, s:s}], s:{s:[{s:s, s:s}, {s:s, s:s}], s:[{s:s, s:s}, {s:s, s:s}]}}",
        "id", "amb",
        "node", AGENT_HOST,
        "yunos", "id", "1234", "id", "5678", "node", "host-amb",
        "actions",
            "start", "yuno", "1234", "command", "resume-load", "yuno", "5678", "command", "resume-load",
            "stop", "yuno", "1234", "command", "pause-load", "yuno", "5678", "command", "pause-load"
    );
}

PRIVATE int test_scenario_ambiguous_name(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", amb_scenario());
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        0, "scenario created: amb", "a scenario with a yuno on a name two nodes give");

    hgobj s1 = open_keyed_session(gobj, "amb-1", "uuid-amb1", "host-amb", NULL);
    hgobj s2 = open_keyed_session(gobj, "amb-2", "uuid-amb2", "host-amb", NULL);

    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("amb"));
    json_object_set_new(kw, "action", json_string("start"));
    ret += check_response(gobj, gobj_command(priv->cc, "run-scenario", kw, client),
        -1, "'host-amb' is the hostname of more than one node", "the start of a scenario on a name two nodes give");
    json_t *received = take_received(priv->agent_wire);
    if(json_array_size(received) != 0) {
        ret += fail(gobj, "a start refused for a name two nodes give sends a step", "");
    }
    JSON_DECREF(received)

    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("amb"));
    json_object_set_new(kw, "action", json_string("stop"));
    json_t *response = gobj_command(priv->cc, "run-scenario", kw, client);
    JSON_DECREF(response)
    json_t *step = agent_request(gobj, "the stop of amb reaches the agent of " AGENT_HOST);
    if(step) {
        agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, 0, "paused");
        JSON_DECREF(step)
    }
    if(count_received(client, EV_MT_COMMAND_ANSWER,
            "node 'host-amb' is the hostname of more than one node") != 1) {
        ret += fail(gobj, "the stop of amb names the step of a name two nodes give", "");
    }
    if(frames_differ(frames_to(s1), frames_to(s2), 0, 0)) {
        ret += fail(gobj, "a step on a name two nodes give reaches one of them", "");
    }
    close_session(s1);
    close_session(s2);
    return ret;
}

/***************************************************************************
 *  36. The stop of a max_run whose name more than one node gives. The
 *      scenario "mra" (max_run 1 s) starts and stops a yuno on `host-mra`.
 *      A (phase 2): it starts on the one node that gives `host-mra`, and a
 *      second node gives it then. B (phase 3, past the stop): the control
 *      center ran the stop, which did nothing, its step naming the name of
 *      more than one node; the stop is tried again in a minute (the warning
 *      "... tried again in a minute", a `stop_at` a minute away), and does
 *      not wait for a node to connect: they are connected, and waiting
 *      would never end. The second node goes, and a stop by the user stops
 *      it. In 7.30.15 the stop waited for a node to connect
 *      (`stop_waits_node`), and nothing ever woke it up.
 ***************************************************************************/
PRIVATE json_t *mra_scenario(void)
{
    return json_pack(
        "{s:s, s:s, s:I, s:[{s:s}], s:{s:[{s:s, s:s}], s:[{s:s, s:s}]}}",
        "id", "mra",
        "node", "host-mra",
        "max_run", (json_int_t)1,
        "yunos", "id", "1234",
        "actions",
            "start", "yuno", "1234", "command", "resume-load",
            "stop", "yuno", "1234", "command", "pause-load"
    );
}

/*  The node of `channel` answers the last request it got: `result`  */
PRIVATE int session_answers(hgobj gobj, hgobj channel, int result, const char *comment, const char *what)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *received = take_received(wire_of(channel));
    size_t n = json_array_size(received);
    json_t *step = n? json_incref(json_object_get(json_array_get(received, n-1), "kw")) : NULL;
    JSON_DECREF(received)
    if(!step) {
        return fail(gobj, what, "the node got no request");
    }
    json_t *kw = msg_iev_build_response(gobj, result, json_string(comment), 0, 0, json_deep_copy(step));
    kw_set_subdict_value(gobj, kw, "__temp__", "channel_gobj",
        json_integer((json_int_t)(uintptr_t)channel));
    gobj_send_event(priv->cc, EV_MT_COMMAND_ANSWER, kw, priv->input_side);
    JSON_DECREF(step)
    return 0;
}

PRIVATE int test_max_run_ambiguous_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj client = priv->client1;
    int ret = 0;

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", mra_scenario());
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        0, "scenario created: mra", "a scenario on a name that two nodes give later");

    priv->mra_1 = open_keyed_session(gobj, "mra-1", "uuid-mra1", "host-mra", NULL);
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("mra"));
    json_object_set_new(kw, "action", json_string("start"));
    json_t *response = gobj_command(priv->cc, "run-scenario", kw, client);
    JSON_DECREF(response)
    ret += session_answers(gobj, priv->mra_1, 0, "generating", "the start of mra reaches its node");
    count_received(client, EV_MT_COMMAND_ANSWER, 0);

    priv->mra_2 = open_keyed_session(gobj, "mra-2", "uuid-mra2", "host-mra", NULL);
    return ret;
}

PRIVATE int test_max_run_ambiguous_retried(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj client = priv->client1;
    int ret = 0;

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("mra"));
    json_t *response = gobj_command(priv->cc, "scenario-runs", kw, client);
    json_t *newest = json_array_get(kw_get_list(gobj, response, "data", 0, 0), 0);
    json_t *step = json_array_get(kw_get_list(gobj, newest, "steps", 0, 0), 0);
    if(strcmp(kw_get_str(gobj, newest, "action", "", 0), "stop")!=0 ||
            kw_get_int(gobj, newest, "result", 0, KW_WILD_NUMBER) >= 0 ||
            !strstr(kw_get_str(gobj, step, "comment", "", 0), "more than one node")) {
        ret += fail(gobj, "the stop of mra ran, and named the name of more than one node",
            kw_get_str(gobj, newest, "comment", "", 0));
    }
    JSON_DECREF(response)

    json_int_t now = (json_int_t)time_in_seconds();
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("mra"));
    response = gobj_command(priv->cc, "scenarios", kw, client);
    json_t *mra = json_array_get(kw_get_list(gobj, response, "data", 0, 0), 0);
    json_int_t stop_at = kw_get_int(gobj, mra, "stop_at", 0, KW_WILD_NUMBER);
    if(json_object_get(mra, "stop_waits_node")) {
        ret += fail(gobj, "the stop of a max_run on a name of two nodes waits for a node", "");
    }
    if(stop_at < now + 55 || stop_at > now + 60) {
        char detail[64];
        snprintf(detail, sizeof(detail), "%lld, now %lld", (long long)stop_at, (long long)now);
        ret += fail(gobj, "the stop of a max_run on a name of two nodes is tried again in a minute", detail);
    }
    JSON_DECREF(response)
    if(frames_differ(frames_to(priv->mra_1), frames_to(priv->mra_2), 0, 0)) {
        ret += fail(gobj, "the stop on a name two nodes give reaches one of them", "");
    }

    close_session(priv->mra_2);
    priv->mra_2 = 0;
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("mra"));
    json_object_set_new(kw, "action", json_string("stop"));
    response = gobj_command(priv->cc, "run-scenario", kw, client);
    JSON_DECREF(response)
    ret += session_answers(gobj, priv->mra_1, 0, "paused", "the stop of mra by the user reaches its node");
    count_received(client, EV_MT_COMMAND_ANSWER, 0);
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("mra"));
    response = gobj_command(priv->cc, "scenarios", kw, client);
    mra = json_array_get(kw_get_list(gobj, response, "data", 0, 0), 0);
    if(json_object_get(mra, "stop_at") || json_object_get(mra, "stop_waits_node")) {
        ret += fail(gobj, "a stopped mra has no stop scheduled", "");
    }
    JSON_DECREF(response)
    close_session(priv->mra_1);
    priv->mra_1 = 0;
    return ret;
}

PRIVATE int test_twin_agents(hgobj gobj)
{
    hgobj again = open_agent_session(gobj, "agent-again", "uuid-" AGENT_HOST, AGENT_HOST);
    hgobj copy = open_agent_session(gobj, "agent-copy", "uuid-" AGENT_HOST, AGENT_HOST "-copy");
    if(!copy || !again) {
        return -1;  // Error already logged
    }
    gobj_stop_tree(copy);
    gobj_destroy(copy);
    gobj_stop_tree(again);
    gobj_destroy(again);
    agent_opens(gobj);  // its link back: the record names AGENT_HOST again, not the copy's
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
            "start", "yuno", "1234", "command", "resume-load",
            "stop", "yuno", "9999", "command", "pause-load"
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
    if(!strstr(kw_get_str(gobj, step, "__command__", "", 0), "pause-load")) {
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
 *      `scenarios` says no `stop_waits_node` and a `stop_at` a minute away,
 *      and a save of mrn keeps that minute (up to 7.29.3 it brought the stop
 *      forward to at once). In 7.29.2 it waited for an agent to connect, so the safety stop was
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

PRIVATE json_t *mrn_scenario(void)
{
    return json_pack(
        "{s:s, s:s, s:I, s:[{s:s}], s:{s:[{s:s, s:s}], s:[{s:s, s:s}]}}",
        "id", "mrn",
        "node", AGENT_HOST,
        "max_run", (json_int_t)1,
        "yunos", "id", "1234",
        "actions",
            "start", "yuno", "1234", "command", "resume-load",
            "stop", "yuno", "1234", "command", "pause-load"
    );
}

PRIVATE int test_max_run_node_refuses_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj client = priv->client1;
    int ret = 0;

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", mrn_scenario());
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

    /*
     *  Saved again as it is, the stop keeps its minute: up to 7.29.3 the
     *  save rebuilt it from the runs without the retry, due at once
     */
    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", mrn_scenario());
    json_t *saved = gobj_command(priv->cc, "save-scenario", kw, client);
    json_t *saved_mrn = json_array_get(kw_get_list(gobj, saved, "data", 0, 0), 0);
    json_int_t saved_stop_at = kw_get_int(gobj, saved_mrn, "stop_at", 0, KW_WILD_NUMBER);
    if(kw_get_int(gobj, saved, "result", -1, KW_WILD_NUMBER) < 0 || saved_stop_at != stop_at) {
        char detail[64];
        snprintf(detail, sizeof(detail), "%lld, was %lld",
            (long long)saved_stop_at, (long long)stop_at);
        ret += fail(gobj, "a save of mrn keeps its stop tried again in a minute", detail);
    }
    JSON_DECREF(saved)
    mrn = mrn_scenario_of(gobj);
    if(kw_get_int(gobj, mrn, "stop_at", 0, KW_WILD_NUMBER) != stop_at) {
        ret += fail(gobj, "after a save, scenarios still says the stop of mrn in a minute", "");
    }
    JSON_DECREF(mrn)

    /*  Stopped by the user: nothing is scheduled any more  */
    kw = client_kw(gobj_name(client));
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
        {"start",   "resume-load"},
        {"stop",    "pause-load"},
        {"pause",   "pause-load"},
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

/***************************************************************************
 *  The operator assertion of a request for the agent: signed with the key
 *  of operator_assertion_key, for this agent's node, naming `sub` (any
 *  user if NULL), living operator_assertion_lifetime (60 s). Verified as
 *  the agent's C_AUTHZ does, with the public key.
 ***************************************************************************/
PRIVATE int check_assertion(hgobj gobj, json_t *request, const char *sub, const char *what)
{
    const char *token = kw_get_str(gobj, request, "__operator_assertion__", "", 0);
    if(empty_string(token)) {
        return fail(gobj, what, "no __operator_assertion__");
    }

    jwk_set_t *jwk_set = jwks_create(ASSERTION_PUBLIC_JWK);
    const jwk_item_t *key = jwk_set? jwks_item_get(jwk_set, 0) : NULL;
    jwt_checker_t *checker = key? jwt_checker_new() : NULL;
    if(!checker || jwt_checker_setkey(checker, JWT_ALG_ES256, key) != 0) {
        if(checker) {
            jwt_checker_free(checker);
        }
        if(jwk_set) {
            jwks_free(jwk_set);
        }
        return fail(gobj, what, "cannot make the checker");
    }
    jwt_checker_claim_set(checker, JWT_CLAIM_ISS, "controlcenter");

    int ret = 0;
    json_t *payload = jwt_checker_verify2(checker, token);
    if(!payload) {
        ret = fail(gobj, what, jwt_checker_error_msg(checker));
    } else {
        json_int_t lifetime = kw_get_int(gobj, payload, "exp", 0, 0) -
            kw_get_int(gobj, payload, "iat", 0, 0);
        const char *got_sub = kw_get_str(gobj, payload, "sub", "", 0);
        if(strcmp(kw_get_str(gobj, payload, "aud", "", 0), "uuid-" AGENT_HOST)!=0) {
            ret += fail(gobj, what, "aud is not the node of the agent");
        }
        if(sub? strcmp(got_sub, sub)!=0 : empty_string(got_sub)) {
            ret += fail(gobj, what, got_sub);
        }
        if(lifetime != 60) {
            ret += fail(gobj, what, "it does not live operator_assertion_lifetime");
        }
    }
    JSON_DECREF(payload)
    jwt_checker_free(checker);
    jwks_free(jwk_set);
    return ret;
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
        ret += check_assertion(gobj, request, NULL, "a step of a run carries its operator assertion");
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
    if(check_parallel_requests(gobj, requests, "pause-load",
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
        "step 1 (command-yuno id=1235 command=pause-load): the yuno refused";
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
    if(check_parallel_requests(gobj, requests, "pause-load",
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
        "step 1 (command-yuno id=1235 command=pause-load): not answered in %d ms",
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
 *  38. The stop of a max_run that does some steps only: the steps it did
 *      not do are tried again until each is done. The scenario "mrp"
 *      (max_run 1 s) has a yuno on the agent and one on `host-mrp`, a node
 *      in session by a token. A (phase 6): started, both steps answered,
 *      and the node of `host-mrp` goes. B (phase 7, past the stop): the
 *      control center sent the stop; the agent's step answered, the other
 *      not connected: one failed run (1 of 2 steps done), the warning
 *      "... did not do every step, the rest tried again when a node of it
 *      connects", and `stop_waits_node`. The node comes back. C (phase 8):
 *      the stop went to it alone, the agent got nothing; answered, it is
 *      done, and no stop is scheduled. In 7.30.16 the stop that did some
 *      steps ended the schedule, with no log, and the yuno on `host-mrp`
 *      went on.
 ***************************************************************************/
PRIVATE json_t *mrp_scenario_of(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = client_kw(gobj_name(priv->client1));
    json_object_set_new(kw, "scenario_id", json_string("mrp"));
    json_t *response = gobj_command(priv->cc, "scenarios", kw, priv->client1);
    json_t *mrp = json_incref(json_array_get(kw_get_list(gobj, response, "data", 0, 0), 0));
    JSON_DECREF(response)
    return mrp;
}

/*  The newest run of "mrp" (yours), or NULL  */
PRIVATE json_t *mrp_newest_run(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = client_kw(gobj_name(priv->client1));
    json_object_set_new(kw, "scenario_id", json_string("mrp"));
    json_t *response = gobj_command(priv->cc, "scenario-runs", kw, priv->client1);
    json_t *newest = json_incref(json_array_get(kw_get_list(gobj, response, "data", 0, 0), 0));
    JSON_DECREF(response)
    return newest;
}

PRIVATE int test_max_run_partial_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj client = priv->client1;
    int ret = 0;

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", json_pack(
        "{s:s, s:s, s:I, s:[{s:s}, {s:s, s:s}], s:{s:[{s:s, s:s}, {s:s, s:s}], s:[{s:s, s:s}, {s:s, s:s}]}}",
        "id", "mrp",
        "node", AGENT_HOST,
        "max_run", (json_int_t)1,
        "yunos", "id", "1234", "id", "5678", "node", "host-mrp",
        "actions",
            "start", "yuno", "1234", "command", "resume-load", "yuno", "5678", "command", "resume-load",
            "stop", "yuno", "1234", "command", "pause-load", "yuno", "5678", "command", "pause-load"
    ));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        0, "scenario created: mrp", "a scenario on two nodes");

    priv->mrp_node = open_keyed_session(gobj, "mrp-1", "uuid-mrp", "host-mrp", NULL);
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("mrp"));
    json_object_set_new(kw, "action", json_string("start"));
    json_t *response = gobj_command(priv->cc, "run-scenario", kw, client);
    JSON_DECREF(response)
    json_t *step = agent_request(gobj, "the start step of mrp goes to the agent");
    if(!step) {
        return ret - 1;
    }
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, 0, "generating");
    JSON_DECREF(step)
    ret += session_answers(gobj, priv->mrp_node, 0, "generating", "the start of mrp reaches its second node");
    count_received(client, EV_MT_COMMAND_ANSWER, 0);
    close_session(priv->mrp_node);
    priv->mrp_node = 0;
    return ret;
}

PRIVATE int test_max_run_partial_waits(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_t *step = agent_request(gobj, "the control center sends the stop of mrp to the agent");
    if(!step) {
        return -1;
    }
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, 0, "paused");
    JSON_DECREF(step)

    json_t *newest = mrp_newest_run(gobj);
    if(strcmp(kw_get_str(gobj, newest, "action", "", 0), "stop")!=0 ||
            kw_get_int(gobj, newest, "result", 0, KW_WILD_NUMBER) >= 0 ||
            !strstr(kw_get_str(gobj, newest, "comment", "", 0), "1 of 2 steps done")) {
        ret += fail(gobj, "the stop of mrp did one step of two", kw_get_str(gobj, newest, "comment", "", 0));
    }
    JSON_DECREF(newest)

    json_t *mrp = mrp_scenario_of(gobj);
    if(!json_is_true(json_object_get(mrp, "stop_waits_node"))) {
        ret += fail(gobj, "the rest of the stop of mrp waits for its node (stop_waits_node)", "");
    }
    JSON_DECREF(mrp)

    priv->mrp_node = open_keyed_session(gobj, "mrp-2", "uuid-mrp", "host-mrp", NULL);
    return ret;
}

PRIVATE int test_max_run_partial_retried(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_t *requests = agent_requests(gobj);
    if(json_array_size(requests) != 0) {
        ret += fail(gobj, "the rest of the stop of mrp goes to the agent again", "");
    }
    JSON_DECREF(requests)

    json_t *received = take_received(wire_of(priv->mrp_node));
    size_t nr = json_array_size(received);
    json_t *step = nr? json_incref(json_object_get(json_array_get(received, nr-1), "kw")) : NULL;
    JSON_DECREF(received)
    if(nr != 1 || !step) {
        JSON_DECREF(step)
        return ret + fail(gobj, "the rest of the stop of mrp goes to its node when it connects",
            "not one request");
    }
    if(!strstr(kw_get_str(gobj, step, "__command__", "", 0), "id=5678")) {
        ret += fail(gobj, "the rest of the stop of mrp", kw_get_str(gobj, step, "__command__", "", 0));
    }
    json_t *kw = msg_iev_build_response(gobj, 0, json_string("paused"), 0, 0, json_deep_copy(step));
    kw_set_subdict_value(gobj, kw, "__temp__", "channel_gobj",
        json_integer((json_int_t)(uintptr_t)priv->mrp_node));
    gobj_send_event(priv->cc, EV_MT_COMMAND_ANSWER, kw, priv->input_side);
    JSON_DECREF(step)

    json_t *newest = mrp_newest_run(gobj);
    if(strcmp(kw_get_str(gobj, newest, "action", "", 0), "stop")!=0 ||
            kw_get_int(gobj, newest, "result", -1, KW_WILD_NUMBER) != 0 ||
            json_array_size(kw_get_list(gobj, newest, "steps", 0, 0)) != 1) {
        ret += fail(gobj, "the rest of the stop of mrp, one step, is done",
            kw_get_str(gobj, newest, "comment", "", 0));
    }
    JSON_DECREF(newest)

    json_t *mrp = mrp_scenario_of(gobj);
    if(json_object_get(mrp, "stop_at") || json_object_get(mrp, "stop_waits_node")) {
        ret += fail(gobj, "a stopped mrp has no stop scheduled", "");
    }
    JSON_DECREF(mrp)

    /*
     *  A stop by a user that does some steps only cancels the max_run, as
     *  before: its rest is not the max_run's to try (it is not due)
     */
    hgobj client = priv->client1;
    const char *actions[] = {"start", "stop"};
    for(int i=0; i<2; i++) {
        kw = client_kw(gobj_name(client));
        json_object_set_new(kw, "scenario_id", json_string("mrp"));
        json_object_set_new(kw, "action", json_string(actions[i]));
        json_t *response = gobj_command(priv->cc, "run-scenario", kw, client);
        JSON_DECREF(response)
        step = agent_request(gobj, "a step of mrp by the user goes to the agent");
        if(!step) {
            return ret - 1;
        }
        agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, 0, "done");
        JSON_DECREF(step)
        ret += session_answers(gobj, priv->mrp_node, i == 0? 0 : -1, "refused",
            "a step of mrp by the user goes to its second node");
        count_received(client, EV_MT_COMMAND_ANSWER, 0);
    }
    newest = mrp_newest_run(gobj);
    if(!strstr(kw_get_str(gobj, newest, "comment", "", 0), "stop failed, 1 of 2 steps done")) {
        ret += fail(gobj, "the stop of mrp by the user did one step of two",
            kw_get_str(gobj, newest, "comment", "", 0));
    }
    JSON_DECREF(newest)
    mrp = mrp_scenario_of(gobj);
    if(json_object_get(mrp, "stop_at") || json_object_get(mrp, "stop_waits_node")) {
        ret += fail(gobj, "a stop by a user that did some steps cancels the max_run", "");
    }
    JSON_DECREF(mrp)
    close_session(priv->mrp_node);
    priv->mrp_node = 0;
    return ret;
}

/***************************************************************************
 *  A scenario as `scenarios` gives it, and its newest run (yours, or NULL)
 ***************************************************************************/
PRIVATE json_t *scenario_named(hgobj gobj, const char *id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = client_kw(gobj_name(priv->client1));
    json_object_set_new(kw, "scenario_id", json_string(id));
    json_t *response = gobj_command(priv->cc, "scenarios", kw, priv->client1);
    json_t *scenario = json_incref(json_array_get(kw_get_list(gobj, response, "data", 0, 0), 0));
    JSON_DECREF(response)
    return scenario;
}

PRIVATE json_t *newest_run_named(hgobj gobj, const char *id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = client_kw(gobj_name(priv->client1));
    json_object_set_new(kw, "scenario_id", json_string(id));
    json_t *response = gobj_command(priv->cc, "scenario-runs", kw, priv->client1);
    json_t *newest = json_incref(json_array_get(kw_get_list(gobj, response, "data", 0, 0), 0));
    JSON_DECREF(response)
    return newest;
}

/*  Does `scenarios` give a stop scheduled for `id`?  */
PRIVATE BOOL stop_scheduled(hgobj gobj, const char *id)
{
    json_t *scenario = scenario_named(gobj, id);
    BOOL scheduled = json_object_get(scenario, "stop_at") ||
        json_object_get(scenario, "stop_waits_node");
    JSON_DECREF(scenario)
    return scheduled;
}

/*  A scenario with one yuno on the agent, started by the user: its start answered  */
PRIVATE int start_one_yuno_scenario(hgobj gobj, const char *id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj client = priv->client1;
    int ret = 0;

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", json_pack(
        "{s:s, s:s, s:I, s:[{s:s}], s:{s:[{s:s, s:s}], s:[{s:s, s:s}]}}",
        "id", id,
        "node", AGENT_HOST,
        "max_run", (json_int_t)1,
        "yunos", "id", "1234",
        "actions",
            "start", "yuno", "1234", "command", "resume-load",
            "stop", "yuno", "1234", "command", "pause-load"
    ));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        0, 0, id);

    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string(id));
    json_object_set_new(kw, "action", json_string("start"));
    json_t *response = gobj_command(priv->cc, "run-scenario", kw, client);
    JSON_DECREF(response)
    json_t *step = agent_request(gobj, id);
    if(!step) {
        return ret - 1;
    }
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, 0, "generating");
    JSON_DECREF(step)
    count_received(client, EV_MT_COMMAND_ANSWER, 0);
    return ret;
}

/***************************************************************************
 *  41. Who may write a scenario but not run it does not make the control
 *      center run what it writes. The scenario "mrw" (max_run 1 s; the
 *      "writer" of main.c may write it) has a yuno on the agent and one on
 *      `host-mrw`, a node in session by a token that goes after the start.
 *      A (phase 8): started. B (phase 9, past the stop): the stop did the
 *      agent's step only, its rest waits for `host-mrw`. The writer saves
 *      "mrw" with another stop, and deletes it: both refused, its stop
 *      still pending. The step of `host-mrw` is changed in the treedb
 *      itself (not by save-scenario, which schedules again), and the node
 *      comes back. C (phase 10): the rest is due, and none of its steps is
 *      in the scenario: the stop does not run (a warning, it is tried again
 *      in a minute), and nothing reaches the agent nor the node. A user who
 *      may run it saves that stop: the rest is dropped, no stop is
 *      scheduled. In 7.30.17 the stop
 *      that did some steps made the scenario look stopped, so the writer's
 *      save went through, and a rest none of whose steps was in the
 *      scenario ran the whole new stop.
 ***************************************************************************/
PRIVATE json_t *mrw_scenario(json_t *stop)  // owned
{
    json_t *scenario = json_pack(
        "{s:s, s:s, s:I, s:[{s:s}, {s:s, s:s}, {s:s}], s:{s:[{s:s, s:s}, {s:s, s:s}]}}",
        "id", "mrw",
        "node", AGENT_HOST,
        "max_run", (json_int_t)1,
        "yunos", "id", "1234", "id", "5678", "node", "host-mrw", "id", "9999",
        "actions",
            "start", "yuno", "1234", "command", "resume-load", "yuno", "5678", "command", "resume-load"
    );
    json_object_set_new(json_object_get(scenario, "actions"), "stop", stop);
    return scenario;
}

/*  The stop with the step of host-mrw changed  */
PRIVATE json_t *mrw_changed_stop(void)
{
    return json_pack("[{s:s, s:s}, {s:s, s:s}]",
        "yuno", "1234", "command", "pause-load", "yuno", "5678", "command", "stop-load");
}

PRIVATE int test_mrw_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj client = priv->client1;
    int ret = 0;

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", mrw_scenario(json_pack("[{s:s, s:s}, {s:s, s:s}]",
        "yuno", "1234", "command", "pause-load", "yuno", "5678", "command", "pause-load")));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        0, "scenario created: mrw", "a scenario the writer may write, on two nodes");

    priv->mrw_node = open_keyed_session(gobj, "mrw-1", "uuid-mrw", "host-mrw", NULL);
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("mrw"));
    json_object_set_new(kw, "action", json_string("start"));
    json_t *response = gobj_command(priv->cc, "run-scenario", kw, client);
    JSON_DECREF(response)
    json_t *step = agent_request(gobj, "the start step of mrw goes to the agent");
    if(!step) {
        return ret - 1;
    }
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, 0, "generating");
    JSON_DECREF(step)
    ret += session_answers(gobj, priv->mrw_node, 0, "generating", "the start of mrw reaches its second node");
    count_received(client, EV_MT_COMMAND_ANSWER, 0);
    close_session(priv->mrw_node);
    priv->mrw_node = 0;
    return ret;
}

PRIVATE int test_mrw_writer(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj client = priv->client1;
    int ret = 0;

    json_t *step = agent_request(gobj, "the control center sends the stop of mrw to the agent");
    if(!step) {
        return -1;
    }
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, 0, "paused");
    JSON_DECREF(step)
    if(!stop_scheduled(gobj, "mrw")) {
        ret += fail(gobj, "the rest of the stop of mrw waits for its node", "");
    }

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "__username__", json_string("writer"));
    json_object_set_new(kw, "scenario", mrw_scenario(json_pack("[{s:s, s:s, s:s}]",
        "yuno", "9999", "service", "__yuno__", "command", "shutdown")));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        -1, "saving it needs 'run-scenarios' too",
        "a writer who may not run cannot save a scenario whose stop is pending");

    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "__username__", json_string("writer"));
    json_object_set_new(kw, "scenario_id", json_string("mrw"));
    ret += check_response(gobj, gobj_command(priv->cc, "delete-scenario", kw, client),
        -1, "deleting it needs 'run-scenarios' too",
        "a writer who may not run cannot delete a scenario whose stop is pending");
    if(!stop_scheduled(gobj, "mrw")) {
        ret += fail(gobj, "the refused writer leaves the stop of mrw pending", "");
    }

    /*
     *  The stop of host-mrw changed in the treedb itself, not by
     *  save-scenario (which schedules again): the rest is still due
     */
    hgobj treedb = gobj_find_service("treedb_controlcenter", TRUE);
    json_t *record = mrw_scenario(mrw_changed_stop());
    json_object_set_new(record, "links", json_array());
    json_object_set_new(record, "view", json_object());
    json_t *node = gobj_update_node(treedb, "scenarios", record, json_pack("{s:b}", "create", 1), gobj);
    if(!node) {
        ret += fail(gobj, "write the stop of mrw in the treedb", "");
    }
    JSON_DECREF(node)

    priv->mrw_node = open_keyed_session(gobj, "mrw-2", "uuid-mrw", "host-mrw", NULL);
    return ret;
}

PRIVATE int test_mrw_nothing_runs(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_t *requests = agent_requests(gobj);
    if(json_array_size(requests) != 0) {
        ret += fail(gobj, "a stop the run did not have does not reach the agent", "");
    }
    JSON_DECREF(requests)
    json_t *received = take_received(wire_of(priv->mrw_node));
    if(json_array_size(received) != 0) {
        ret += fail(gobj, "a stop the run did not have does not reach host-mrw", "");
    }
    JSON_DECREF(received)

    hgobj client = priv->client1;
    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", mrw_scenario(mrw_changed_stop()));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        0, 0, "a user who may run saves mrw with another stop for host-mrw");
    if(stop_scheduled(gobj, "mrw")) {
        ret += fail(gobj, "a rest that is not in the scenario saved is not scheduled", "");
    }
    close_session(priv->mrw_node);
    priv->mrw_node = 0;
    return ret;
}

/***************************************************************************
 *  42. The stop of a max_run whose step is answered "Yuno not found" is
 *      not done: the agent answers it also for a yuno that lives and is not
 *      connected to it yet (an agent just restarted, when the stops that
 *      waited for it are tried). It is tried again in a minute, as any
 *      stop that did nothing. The scenario "mrg" (max_run 1 s, one yuno on
 *      the agent). A (phase 10): started. B (phase 11, past the stop):
 *      answered "Yuno not found", the run is a failed stop, the warning
 *      "... tried again in a minute", and `scenarios` says a `stop_at` a
 *      minute away, no `stop_waits_node`. The yuno back, a stop by the user
 *      stops it: nothing is scheduled any more. In 7.30.18 such a stop was
 *      done, and the max_run of a yuno not reconnected yet was lost.
 ***************************************************************************/
PRIVATE int test_mrg_start(hgobj gobj)
{
    return start_one_yuno_scenario(gobj, "mrg");
}

PRIVATE int test_mrg_yuno_not_found(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj client = priv->client1;
    int ret = 0;

    json_t *step = agent_request(gobj, "the control center sends the stop of mrg");
    if(!step) {
        return -1;
    }
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, -1, "yuno_agent^node1: Yuno not found");
    JSON_DECREF(step)

    json_t *newest = newest_run_named(gobj, "mrg");
    if(strcmp(kw_get_str(gobj, newest, "action", "", 0), "stop")!=0 ||
            kw_get_int(gobj, newest, "result", 0, KW_WILD_NUMBER) >= 0) {
        ret += fail(gobj, "the stop of mrg is a failed run", kw_get_str(gobj, newest, "comment", "", 0));
    }
    JSON_DECREF(newest)

    json_int_t now = (json_int_t)time_in_seconds();
    json_t *mrg = scenario_named(gobj, "mrg");
    json_int_t stop_at = kw_get_int(gobj, mrg, "stop_at", 0, KW_WILD_NUMBER);
    if(json_object_get(mrg, "stop_waits_node")) {
        ret += fail(gobj, "a stop answered 'Yuno not found' does not wait for a node", "");
    }
    if(stop_at < now + 59 || stop_at > now + 60) {
        char detail[64];
        snprintf(detail, sizeof(detail), "%lld, now %lld", (long long)stop_at, (long long)now);
        ret += fail(gobj, "a stop answered 'Yuno not found' is tried again in a minute", detail);
    }
    JSON_DECREF(mrg)

    /*  The yuno back: stopped by the user, nothing is scheduled any more  */
    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("mrg"));
    json_object_set_new(kw, "action", json_string("stop"));
    json_t *response = gobj_command(priv->cc, "run-scenario", kw, client);
    JSON_DECREF(response)
    step = agent_request(gobj, "the stop step of mrg, by the user, goes to the agent");
    if(!step) {
        return ret - 1;
    }
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, 0, "paused");
    JSON_DECREF(step)
    count_received(client, EV_MT_COMMAND_ANSWER, 0);
    if(stop_scheduled(gobj, "mrg")) {
        ret += fail(gobj, "mrg stopped by the user has no stop scheduled", "");
    }
    return ret;
}

/***************************************************************************
 *  43. The stop of a max_run is not tried again after MRC_TRIES tries in
 *      a row that did nothing, and that end is said once and shown. The
 *      scenario "mrc" (max_run 1 s, one yuno on the agent). A (phase 11):
 *      started; the tries before the last are written into the treedb as
 *      the control center writes them (a minute apart, they would take
 *      minutes). B (phase 12, past the stop): the last try, answered -1
 *      ("the yuno is restarting"): no stop is scheduled, and a warning says
 *      why (in the expected logs). `scenarios` says `stop_given_up`, and no
 *      `stop_at`; the scenario runs, so the writer (who may not run it)
 *      cannot save it. A user who may run it saves it as it is: the stop is
 *      tried again (a `stop_at`, no `stop_given_up`). A stop by the user
 *      then stops it. In 7.30.17 a step that answered an error for ever
 *      was tried every minute, for ever; in 7.30.18 the stop given up was
 *      shown nowhere, and a save did not try it again.
 ***************************************************************************/
#define MRC_TRIES   10      // MAX_RUN_STOP_TRIES of c_controlcenter.c: tries in a row

PRIVATE int test_mrc_start(hgobj gobj)
{
    int ret = start_one_yuno_scenario(gobj, "mrc");
    ret += write_failed_tries(gobj, "mrc");
    return ret;
}

PRIVATE int test_mrc_given_up(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj client = priv->client1;
    int ret = 0;

    json_t *step = agent_request(gobj, "the control center sends the last try of the stop of mrc");
    if(!step) {
        return -1;
    }
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, -1, "the yuno is restarting");
    JSON_DECREF(step)
    if(stop_scheduled(gobj, "mrc")) {
        ret += fail(gobj, "the stop of a max_run is not tried again after its last try", "");
    }
    json_t *mrc = scenario_named(gobj, "mrc");
    if(!json_is_true(json_object_get(mrc, "stop_given_up"))) {
        ret += fail(gobj, "scenarios says the stop of mrc given up", "");
    }
    json_t *mrc_saved = json_pack("{s:s, s:s, s:I, s:O, s:O}",
        "id", "mrc",
        "node", AGENT_HOST,
        "max_run", (json_int_t)1,
        "yunos", json_object_get(mrc, "yunos"),
        "actions", json_object_get(mrc, "actions")
    );
    JSON_DECREF(mrc)

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "__username__", json_string("writer"));
    json_object_set_new(kw, "scenario", json_incref(mrc_saved));
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        -1, "saving it needs 'run-scenarios' too",
        "a writer who may not run cannot save a scenario whose stop was given up");

    /*  Saved by a user who may run it: its stop is tried again  */
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", mrc_saved);
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        0, 0, "mrc saved as it is");
    mrc = scenario_named(gobj, "mrc");
    if(json_object_get(mrc, "stop_given_up") || !json_object_get(mrc, "stop_at")) {
        ret += fail(gobj, "a save of mrc tries its stop again", "");
    }
    JSON_DECREF(mrc)

    /*  Stopped by the user before that try: nothing is scheduled any more  */
    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("mrc"));
    json_object_set_new(kw, "action", json_string("stop"));
    json_t *response = gobj_command(priv->cc, "run-scenario", kw, client);
    JSON_DECREF(response)
    step = agent_request(gobj, "the stop step of mrc, by the user, goes to the agent");
    if(!step) {
        return ret - 1;
    }
    agent_sends(gobj, EV_MT_COMMAND_ANSWER, step, 0, "paused");
    JSON_DECREF(step)
    count_received(client, EV_MT_COMMAND_ANSWER, 0);
    mrc = scenario_named(gobj, "mrc");
    if(json_object_get(mrc, "stop_at") || json_object_get(mrc, "stop_given_up")) {
        ret += fail(gobj, "mrc stopped by the user has no stop scheduled nor given up", "");
    }
    JSON_DECREF(mrc)
    return ret;
}

/***************************************************************************
 *  46. A max_run stop that cannot even start (the scenario lost its `stop`
 *      outside save-scenario) writes no run: its attempts count towards the
 *      same MRC_TRIES. The scenario "mrb" (max_run 1 s, one yuno on the
 *      agent). A (phase 12): started; the tries before the last are written
 *      into the treedb, as in case 43, and a save of "mrb" as it is counts
 *      them; then "mrb" loses its `stop` in the treedb itself. B (phase 13,
 *      past the stop): it could not start, nothing reached the agent, no
 *      stop is scheduled, and a warning says it once (in the expected
 *      logs). In 7.30.17 it was tried every minute, for ever, a warning
 *      each time.
 ***************************************************************************/
PRIVATE int write_failed_tries(hgobj gobj, const char *scenario_id)
{
    int ret = 0;

    json_t *start = newest_run_named(gobj, scenario_id);
    const char *start_id = kw_get_str(gobj, start, "id", "", 0);
    json_int_t started_at = kw_get_int(gobj, start, "started_at", 0, KW_WILD_NUMBER);
    if(strcmp(kw_get_str(gobj, start, "action", "", 0), "start")!=0) {
        JSON_DECREF(start)
        return fail(gobj, "the start is the newest run", scenario_id);
    }

    /*  Newer than the start: its second, and an id that sorts after its id  */
    char parent_ref[NAME_MAX];
    snprintf(parent_ref, sizeof(parent_ref), "scenarios^%s^runs", scenario_id);
    hgobj treedb = gobj_find_service("treedb_controlcenter", TRUE);
    for(int i=1; i<MRC_TRIES; i++) {
        char id[NAME_MAX];
        snprintf(id, sizeof(id), "%s-try%d", start_id, i);
        json_t *record = json_pack(
            "{s:s, s:s, s:s, s:s, s:I, s:I, s:i, s:s, s:[{s:s, s:s, s:i, s:s}]}",
            "id", id,
            "scenario_id", parent_ref,
            "action", "stop",
            "username", gobj_yuno_role_plus_name(),
            "started_at", started_at,
            "ended_at", started_at,
            "result", -1,
            "comment", "a try of the stop, written by the test",
            "steps",
                "node", AGENT_HOST,
                "line", "command-yuno id=1234 command=pause-load",
                "result", -1,
                "comment", "the yuno is restarting"
        );
        json_t *node = gobj_update_node(treedb, "scenario_runs", record,
            json_pack("{s:b, s:b, s:b}", "create", 1, "autolink", 1, "fkey_only_id", 1), gobj);
        if(!node) {
            ret += fail(gobj, "write a try of a stop", id);
        }
        JSON_DECREF(node)
    }
    JSON_DECREF(start)
    return ret;
}

PRIVATE int test_mrb_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj client = priv->client1;

    int ret = start_one_yuno_scenario(gobj, "mrb");
    ret += write_failed_tries(gobj, "mrb");

    /*  Saved as it is: the stop is scheduled again, counting the tries  */
    json_t *scenario = scenario_named(gobj, "mrb");
    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", json_pack("{s:s, s:s, s:I, s:O, s:O}",
        "id", "mrb",
        "node", AGENT_HOST,
        "max_run", (json_int_t)1,
        "yunos", json_object_get(scenario, "yunos"),
        "actions", json_object_get(scenario, "actions")
    ));
    JSON_DECREF(scenario)
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        0, 0, "mrb saved as it is");
    if(!stop_scheduled(gobj, "mrb")) {
        ret += fail(gobj, "mrb saved as it is keeps its stop", "");
    }

    /*  Its stop gone, in the treedb itself  */
    hgobj treedb = gobj_find_service("treedb_controlcenter", TRUE);
    json_t *record = json_pack("{s:s, s:s, s:I, s:[{s:s}], s:{s:[{s:s, s:s}]}, s:[], s:{}}",
        "id", "mrb",
        "node", AGENT_HOST,
        "max_run", (json_int_t)1,
        "yunos", "id", "1234",
        "actions", "start", "yuno", "1234", "command", "resume-load",
        "links",
        "view"
    );
    json_t *node = gobj_update_node(treedb, "scenarios", record, json_pack("{s:b}", "create", 1), gobj);
    if(!node) {
        ret += fail(gobj, "write mrb without its stop", "");
    }
    JSON_DECREF(node)
    json_t *requests = agent_requests(gobj);
    JSON_DECREF(requests)
    return ret;
}

PRIVATE int test_mrb_given_up(hgobj gobj)
{
    int ret = 0;

    json_t *requests = agent_requests(gobj);
    if(json_array_size(requests) != 0) {
        ret += fail(gobj, "a stop that cannot start sends nothing", "");
    }
    JSON_DECREF(requests)
    if(stop_scheduled(gobj, "mrb")) {
        ret += fail(gobj, "a stop that cannot start is not tried again after its last try", "");
    }
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
            "yuno", on_yuno, "command", "set-clients clients=5",
            "command", "resume-load"
    );
    if(with_off) {
        json_object_set_new(jn_switch, "off", json_pack("[{s:s}, {s:s}]",
            "command", "pause-load",
            "command", "set-clients clients=0"
        ));
    }
    return json_pack("{s:s, s:s, s:[{s:s, s:s, s:s, s:o}, {s:s, s:s}], s:{s:[{s:s, s:s}], s:[{s:s, s:s}]}}",
        "id", SW_SCENARIO,
        "node", AGENT_HOST,
        "yunos",
            "key", "sim", "id", "gen1", "service", "sim_load", "switch", jn_switch,
            "key", "gate", "id", "7601",
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

    const char *start_lines[] = {"command-yuno id=7601 command=play-yuno", 0};
    ret += run_serial(gobj, client, "start", NULL, start_lines, "the start of sw");
    count_received(client, EV_MT_COMMAND_ANSWER, 0);
    ret += check_response(gobj, gobj_command(priv->cc, "run-scenario", run_kw(client, "on", "gate"), client),
        -1, "yuno 'gate' has no switch", "on of a yuno with no switch is refused");

    const char *on_lines[] = {
        "command-yuno id=gen1 service=sim_load command=set-clients clients=5",
        "command-yuno id=gen1 service=sim_load command=resume-load",
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
            !strstr(kw_get_str(gobj, json_array_get(requests, 0), "__command__", "", 0), "pause-load") ||
            !strstr(kw_get_str(gobj, json_array_get(requests, 1), "__command__", "", 0), "clients=0")) {
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

    const char *stop_lines[] = {"command-yuno id=7601 command=pause-yuno", 0};
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
/***************************************************************************
 *  A command for the agent carries the operator assertion of who asks;
 *  with nobody asking, none
 ***************************************************************************/
PRIVATE int test_operator_assertion(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "__username__", json_string("alice@example.com"));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("list-yunos"));
    check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
        0, 0, "command-agent with an operator");
    json_t *request = agent_request(gobj, "command-agent with an operator reaches the agent");
    if(!request) {
        return -1;
    }
    ret += check_assertion(gobj, request, "alice@example.com",
        "command-agent carries the operator assertion of who asks");
    JSON_DECREF(request)

    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "__username__", json_string(""));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("list-yunos"));
    check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
        0, 0, "command-agent with nobody");
    request = agent_request(gobj, "command-agent with nobody reaches the agent");
    if(!request) {
        return -1;
    }
    if(json_object_get(request, "__operator_assertion__")) {
        ret += fail(gobj, "a request of nobody carries an operator assertion", "");
    }
    JSON_DECREF(request)
    return ret;
}

/***************************************************************************
 *  A request that carries a gbuffer (kw_deserialize() makes one of a
 *  peer's "__gbuffer___"): `command` of the control center with `kw`
 *  (owned) answers `expected`, the gbuffer keeps the test's reference and
 *  no other, and the agent gets its bytes when `reaches_agent`. Up to
 *  7.30.16 the handlers copied the kw with json_deep_copy() -- the integer
 *  without its reference -- and released the copy with KW_DECREF: the
 *  gbuffer was released once too often, also by a refused request.
 ***************************************************************************/
PRIVATE int check_command_with_gbuffer(
    hgobj gobj,
    hgobj client,
    const char *command,
    json_t *kw,     // owned
    int expected,
    BOOL reaches_agent,
    const char *what
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    gbuffer_t *gbuf = gbuffer_create(16, 16);
    gbuffer_append_string(gbuf, "peer bytes");
    json_object_set_new(kw, "gbuffer", json_integer((json_int_t)(uintptr_t)gbuf));
    gbuffer_incref(gbuf);   // the test's own, to look at it after

    ret += check_response(gobj, gobj_command(priv->cc, command, kw, client), expected, 0, what);
    if(gbuf->refcount != 1) {
        char detail[64];
        snprintf(detail, sizeof(detail), "refcount %lu, wanted 1", (unsigned long)gbuf->refcount);
        ret += fail(gobj, what, detail);
    }

    if(reaches_agent) {
        json_t *request = agent_request(gobj, what);
        gbuffer_t *got = (gbuffer_t *)(uintptr_t)kw_get_int(
            gobj, request, "gbuffer", 0, KW_EXTRACT
        );
        if(!got || gbuffer_leftbytes(got) != strlen("peer bytes") ||
                memcmp(gbuffer_cur_rd_pointer(got), "peer bytes", strlen("peer bytes"))!=0) {
            ret += fail(gobj, what, "the agent did not get the bytes");
        }
        GBUFFER_DECREF(got)
        JSON_DECREF(request)
    }

    GBUFFER_DECREF(gbuf)
    return ret;
}

PRIVATE int test_commands_with_gbuffer(hgobj gobj, hgobj client)
{
    int ret = 0;

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("list-yunos"));
    ret += check_command_with_gbuffer(gobj, client, "command-agent", kw, 0, TRUE,
        "command-agent with a gbuffer");

    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "stats2agent", json_string("stats-yuno"));
    ret += check_command_with_gbuffer(gobj, client, "stats-agent", kw, 0, TRUE,
        "stats-agent with a gbuffer");

    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string("nobody-node"));
    ret += check_command_with_gbuffer(gobj, client, "drop-agent", kw, -1, FALSE,
        "drop-agent of nobody with a gbuffer");

    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string("no-such-scenario"));
    ret += check_command_with_gbuffer(gobj, client, "watch-scenario", kw, -1, FALSE,
        "watch-scenario of nothing with a gbuffer");

    /*  Refused: no permission  */
    kw = viewer_kw(client);
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("list-yunos"));
    ret += check_command_with_gbuffer(gobj, client, "command-agent", kw, -1, FALSE,
        "a refused command-agent with a gbuffer");

    kw = viewer_kw(client);
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "stats2agent", json_string("stats-yuno"));
    ret += check_command_with_gbuffer(gobj, client, "stats-agent", kw, -1, FALSE,
        "a refused stats-agent with a gbuffer");

    kw = viewer_kw(client);
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    ret += check_command_with_gbuffer(gobj, client, "drop-agent", kw, -1, FALSE,
        "a refused drop-agent with a gbuffer");

    return ret;
}

/***************************************************************************
 *  44. A control center with no operator_assertion_key (the stop of case
 *      12 dropped it, and closed its treedb: the agent is named by its node
 *      uuid): command-agent and stats-agent with a gbuffer reach the agent
 *      with its bytes, and the gbuffer keeps only the test's reference. In 7.30.17 the kw for the agent was the
 *      request itself: the agent's gate serialized its gbuffer away, and
 *      one reference was never released.
 ***************************************************************************/
PRIVATE int test_no_assertion_key_with_gbuffer(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj client = priv->client1;
    int ret = 0;

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string("uuid-" AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("list-yunos"));
    ret += check_command_with_gbuffer(gobj, client, "command-agent", kw, 0, TRUE,
        "command-agent with a gbuffer and no assertion key");

    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string("uuid-" AGENT_HOST));
    json_object_set_new(kw, "stats2agent", json_string("stats-yuno"));
    ret += check_command_with_gbuffer(gobj, client, "stats-agent", kw, 0, TRUE,
        "stats-agent with a gbuffer and no assertion key");
    return ret;
}

/***************************************************************************
 *  40. write-tty: a user who may not is answered once, with the answer of a
 *      command (EV_MT_COMMAND_ANSWER, what its client takes), and its kw is
 *      released once; one who may sends a gbuffer that keeps only the
 *      test's reference. In 7.30.17 the refusal released the kw twice and
 *      answered EV_WRITE_TTY, which no client takes; the kw sent to the
 *      agent shared the gbuffer the agent's gate serialized away, so one
 *      reference was never released.
 ***************************************************************************/
PRIVATE json_t *write_tty_kw(json_t *kw)  // owned, returned
{
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "name", json_string("console-a"));
    json_object_set_new(kw, "content64", json_string("bHM="));
    return kw;
}

PRIVATE int test_write_tty_refused(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    count_received(client, EV_MT_COMMAND_ANSWER, 0);
    json_t *sent = take_received(priv->agent_wire);
    JSON_DECREF(sent)

    json_t *kw = write_tty_kw(viewer_kw(client));
    json_incref(kw);    // the test's own, to look at it after
    gobj_send_event(priv->cc, EV_WRITE_TTY, kw, client);
    if(kw->refcount != 1) {
        char detail[64];
        snprintf(detail, sizeof(detail), "refcount %lu, wanted 1", (unsigned long)kw->refcount);
        ret += fail(gobj, "a refused write-tty releases its kw once", detail);
    } else {
        JSON_DECREF(kw)
    }
    if(count_received(client, EV_MT_COMMAND_ANSWER, "No permission to 'write-tty'") != 1) {
        ret += fail(gobj, "a refused write-tty is answered as a command", "");
    }
    sent = take_received(priv->agent_wire);
    if(json_array_size(sent) != 0) {
        ret += fail(gobj, "a refused write-tty reaches no agent", "");
    }
    JSON_DECREF(sent)

    /*  Allowed, with a gbuffer (kw_deserialize() makes one of a peer's "__gbuffer___")  */
    gbuffer_t *gbuf = gbuffer_create(16, 16);
    gbuffer_append_string(gbuf, "peer bytes");
    kw = write_tty_kw(client_kw(gobj_name(client)));
    json_object_set_new(kw, "gbuffer", json_integer((json_int_t)(uintptr_t)gbuf));
    gbuffer_incref(gbuf);   // the test's own, to look at it after
    gobj_send_event(priv->cc, EV_WRITE_TTY, kw, client);
    if(gbuf->refcount != 1) {
        char detail[64];
        snprintf(detail, sizeof(detail), "refcount %lu, wanted 1", (unsigned long)gbuf->refcount);
        ret += fail(gobj, "a write-tty with a gbuffer", detail);
    }
    json_t *request = agent_request(gobj, "a write-tty with a gbuffer reaches the agent");
    if(request) {
        gbuffer_t *got = (gbuffer_t *)(uintptr_t)kw_get_int(
            gobj, request, "gbuffer", 0, KW_EXTRACT
        );
        if(!got || gbuffer_leftbytes(got) != strlen("peer bytes")) {
            ret += fail(gobj, "a write-tty with a gbuffer", "the agent did not get the bytes");
        }
        GBUFFER_DECREF(got)
        JSON_DECREF(request)
    } else {
        ret--;
    }
    GBUFFER_DECREF(gbuf)
    return ret;
}

/***************************************************************************
 *  45. A gate does not change a kw that somebody else holds. A caller keeps
 *      its kw, with a gbuffer and a route (`__md_iev__`), and hands
 *      gobj_command() and gobj_stats() a kw_incref() of it, on the agent's
 *      C_IEVENT_SRV and on a C_IEVENT_CLI in session: the frame carries the
 *      bytes, the caller's kw is as it was, its refcount 1, and its gbuffer
 *      held by it alone (released with it: the audit at the end finds no
 *      leak). Up to 7.30.17 mt_command() and mt_stats() of both gates wrote
 *      their stacks in the caller's kw and serialized its gbuffer away,
 *      whose reference was then never released.
 ***************************************************************************/
PRIVATE int check_gate_keeps_kw(hgobj gobj, hgobj gate, hgobj wire, BOOL stats, const char *what)
{
    int ret = 0;

    json_t *received = take_received(wire);
    JSON_DECREF(received)

    gbuffer_t *gbuf = gbuffer_create(16, 16);
    gbuffer_append_string(gbuf, "peer bytes");
    json_t *kw = json_pack("{s:s, s:{s:[{s:s, s:s}]}, s:{s:s}}",
        "service", "agent",
        "__md_iev__",
            "ievent_gate_stack",
                "dst_service", "controlcenter",
                "input_channel", "client-1",
        stats? "__md_stats__" : "__md_command__",
            "asked_by", "the caller"
    );
    json_object_set_new(kw, "gbuffer", json_integer((json_int_t)(uintptr_t)gbuf));  // the kw's reference
    json_t *before = json_deep_copy(kw);    // a json to compare, not a kw: no reference of the gbuffer

    json_t *webix = stats?
        gobj_stats(gate, "stats-yuno", kw_incref(kw), gobj) :
        gobj_command(gate, "list-yunos", kw_incref(kw), gobj);
    JSON_DECREF(webix)

    if(!json_equal(kw, before)) {
        ret += fail(gobj, what, "the caller's kw was changed");
    }
    if(kw->refcount != 1 || gbuf->refcount != 1) {
        char detail[80];
        snprintf(detail, sizeof(detail), "kw refcount %lu, gbuffer refcount %lu, wanted 1 and 1",
            (unsigned long)kw->refcount, (unsigned long)gbuf->refcount);
        ret += fail(gobj, what, detail);
    }
    received = take_received(wire);
    size_t n = json_array_size(received);
    json_t *frame = n? json_object_get(json_array_get(received, n-1), "kw") : NULL;
    gbuffer_t *got = frame?
        (gbuffer_t *)(uintptr_t)kw_get_int(gobj, frame, "gbuffer", 0, KW_EXTRACT) : NULL;
    if(!got || gbuffer_leftbytes(got) != strlen("peer bytes")) {
        ret += fail(gobj, what, "the frame does not carry the bytes");
    }
    GBUFFER_DECREF(got)
    JSON_DECREF(received)
    JSON_DECREF(before)
    KW_DECREF(kw)
    return ret;
}

PRIVATE int test_gates_keep_a_shared_kw(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    hgobj srv = gobj_child_by_name(priv->agent_channel, "agent-1");
    ret += check_gate_keeps_kw(gobj, srv, priv->agent_wire, FALSE, "gobj_command() on a C_IEVENT_SRV");
    ret += check_gate_keeps_kw(gobj, srv, priv->agent_wire, TRUE, "gobj_stats() on a C_IEVENT_SRV");

    hgobj cli = gobj_create_service("gate_cli", C_IEVENT_CLI, 0, gobj);
    hgobj cli_wire = create_peer("gate_cli_wire", cli);
    gobj_set_bottom_gobj(cli, cli_wire);
    gobj_change_state(cli, ST_SESSION);
    ret += check_gate_keeps_kw(gobj, cli, cli_wire, FALSE, "gobj_command() on a C_IEVENT_CLI");
    ret += check_gate_keeps_kw(gobj, cli, cli_wire, TRUE, "gobj_stats() on a C_IEVENT_CLI");
    gobj_stop(cli_wire);
    gobj_destroy(cli);
    return ret;
}

/***************************************************************************
 *  47. The agent's link is closing (its transport in ST_DISCONNECTED, as a
 *      peer's Close leaves it before EV_ON_CLOSE): C_IEVENT_SRV refuses an
 *      order at once, -1 "not in session". command-agent and stats-agent
 *      answer -1 with it; the start of a scenario (its steps in order)
 *      fails at its first step at once, and its stop (every step at once)
 *      fails at once, each step naming it; a write-tty is answered with
 *      it. Up to 7.30.26 the control center dropped that answer: "Command
 *      sent to 1 nodes", a run that waited its whole run_step_timeout, and
 *      keystrokes lost with no word.
 ***************************************************************************/
#define CLS_SCENARIO    "cls"

PRIVATE json_t *closing_link_scenario(void)
{
    return json_pack("{s:s, s:s, s:[{s:s}, {s:s}], s:{s:[{s:s, s:s}, {s:s, s:s}], s:[{s:s, s:s}, {s:s, s:s}]}}",
        "id", CLS_SCENARIO,
        "node", AGENT_HOST,
        "yunos",
            "id", "1301",
            "id", "1302",
        "actions",
            "start",
                "yuno", "1301", "command", "resume-load",
                "yuno", "1302", "command", "resume-load",
            "stop",
                "yuno", "1301", "command", "pause-load",
                "yuno", "1302", "command", "pause-load"
    );
}

PRIVATE json_t *closing_link_newest_run(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string(CLS_SCENARIO));
    json_t *response = gobj_command(priv->cc, "scenario-runs", kw, client);
    json_t *newest = json_incref(json_array_get(kw_get_list(gobj, response, "data", 0, 0), 0));
    JSON_DECREF(response)
    return newest;
}

PRIVATE int closing_link_run(hgobj gobj, hgobj client, const char *action, const char *answer,
    int steps_failed)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;
    char what[128];
    snprintf(what, sizeof(what), "the %s of a scenario, the agent's link closing", action);

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario_id", json_string(CLS_SCENARIO));
    json_object_set_new(kw, "action", json_string(action));
    json_t *response = gobj_command(priv->cc, "run-scenario", kw, client);
    if(response) {
        ret += fail(gobj, what, kw_get_str(gobj, response, "comment", "", 0));
        JSON_DECREF(response)
    }
    if(count_received(client, EV_MT_COMMAND_ANSWER, answer) != 1) {
        ret += fail(gobj, what, "not answered at once, failed");
    }
    json_t *run = closing_link_newest_run(gobj, client);
    json_t *steps = kw_get_list(gobj, run, "steps", 0, 0);
    int failed = 0;
    size_t idx; json_t *step;
    json_array_foreach(steps, idx, step) {
        if(kw_has_key(step, "result") && kw_get_int(gobj, step, "result", 0, KW_WILD_NUMBER) == -1 &&
                strstr(kw_get_str(gobj, step, "comment", "", 0), "not in session")) {
            failed++;
        }
    }
    if(strcmp(kw_get_str(gobj, run, "action", "", 0), action)!=0 || failed != steps_failed) {
        ret += fail(gobj, what, "its run is not written with its steps failed, naming the link");
    }
    JSON_DECREF(run)
    return ret;
}

PRIVATE int test_agent_link_closing(hgobj gobj, hgobj client)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_t *kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "scenario", closing_link_scenario());
    ret += check_response(gobj, gobj_command(priv->cc, "save-scenario", kw, client),
        0, "scenario created: " CLS_SCENARIO, "a scenario of two yunos (case 47)");
    count_received(client, EV_MT_COMMAND_ANSWER, 0);
    json_t *sent = take_received(priv->agent_wire);
    JSON_DECREF(sent)

    gobj_change_state(priv->agent_wire, ST_DISCONNECTED);

    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "cmd2agent", json_string("list-yunos"));
    ret += check_response(gobj, gobj_command(priv->cc, "command-agent", kw, client),
        -1, "not in session: Command sent to 0 nodes", "command-agent, the agent's link closing");

    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "stats2agent", json_string("stats-yuno"));
    ret += check_response(gobj, gobj_command(priv->cc, "stats-agent", kw, client),
        -1, "not in session: Stats sent to 0 nodes", "stats-agent, the agent's link closing");

    ret += closing_link_run(gobj, client, "start", "start failed, 0 of 2 steps done: a step failed", 1);
    ret += closing_link_run(gobj, client, "stop", "stop failed, 0 of 2 steps done: 2 of 2 steps failed", 2);

    kw = client_kw(gobj_name(client));
    json_object_set_new(kw, "agent_id", json_string(AGENT_HOST));
    json_object_set_new(kw, "name", json_string("console-a"));
    json_object_set_new(kw, "content64", json_string("bHM="));
    gobj_send_event(priv->cc, EV_WRITE_TTY, kw, client);
    if(count_received(client, EV_MT_COMMAND_ANSWER, "not in session") != 1) {
        ret += fail(gobj, "write-tty, the agent's link closing", "not answered at once");
    }

    sent = take_received(priv->agent_wire);
    if(json_array_size(sent) != 0) {
        ret += fail(gobj, "nothing reaches an agent whose link is closing", "");
    }
    JSON_DECREF(sent)

    gobj_change_state(priv->agent_wire, ST_IDLE);
    return ret;
}

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

    result += test_node_keys(gobj, client1);
    result += test_node_names(gobj, client1);
    result += test_one_key_one_node(gobj, client1);
    result += test_card_checked(gobj, client1);
    result += test_refused_node_by_key(gobj);
    result += test_record_written_on_change(gobj);
    result += test_key_revoked(gobj, client1);
    result += test_logout_user(gobj, client1);
    result += test_names_not_planted(gobj, client1);
    result += test_drop_agent_by_name(gobj, client1);
    result += test_pending_name_not_taken(gobj, client1);
    result += test_open_leaves_no_name(gobj, client1);
    result += test_accept_drops_token_name(gobj, client1);
    result += test_key_login_first_and_cards(gobj, client1);
    result += test_twin_agents(gobj);
    result += test_operator_assertion(gobj, client1);
    result += test_commands_with_gbuffer(gobj, client1);
    result += test_write_tty_refused(gobj, client1);
    result += test_gates_keep_a_shared_kw(gobj);
    result += test_agent_link_closing(gobj, client1);
    result += test_save_scenario_as_string(gobj, client1);
    result += test_save_refuses_framework_keys(gobj, client1);
    result += test_run_checks_old_scenarios(gobj, client1);
    result += test_forged_step_answers(gobj, client1, client2);
    result += test_injected_agent_events(gobj, client1, client2);
    result += test_late_answers_to_another_client(gobj, client3);
    result += test_several_mirrors(gobj, client1, client2);
    result += test_relays_only_for_who_asks(gobj, client1);
    result += test_agent_request_answers(gobj, client1);
    result += test_answer_for_a_client_gone(gobj, client1);
    result += test_agent_request_answers_unasked(gobj, client1, client2);
    result += test_client_that_does_not_take(gobj, client2);
    result += test_client_that_holds_bytes(gobj, client2);
    result += test_answer_to_local_requesters(gobj, client1);
    result += test_inject_in_a_loop(gobj, client2);
    result += test_scoped_viewer(gobj, client1);
    result += test_permissions_changed(gobj);
    result += test_scenario_ambiguous_name(gobj, client1);
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
 *  PTY frame, the 12 injected of cases 6 and 10) are due now.
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
     *  and phases 14 and 15 for the control center's drops timer to have
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
            priv->phase = 18;
        }
    }
    if(priv->phase == 14 || priv->phase == 15) {
        if(gobj_read_bool_attr(priv->cc, "drops_timer_armed")) {
            if(!test_msectimer(priv->t_wait)) {
                set_timeout(priv->timer, 100);
                KW_DECREF(kw)
                return 0;
            }
            priv->result += fail(gobj, "the drops timer did not fire in time",
                priv->phase == 14? "phase 14" : "phase 15");
            priv->phase = 18;
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
            priv->result += test_max_run_ambiguous_start(gobj);
            set_timeout(priv->timer, 2500);     // past the stop its max_run schedules
            KW_DECREF(kw)
            return 0;
        case 3:
            priv->result += test_max_run_ambiguous_retried(gobj);
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
            priv->result += test_max_run_partial_start(gobj);
            set_timeout(priv->timer, 2500);     // past the stop its max_run schedules
            KW_DECREF(kw)
            return 0;
        case 7:
            priv->result += test_max_run_partial_waits(gobj);
            set_timeout(priv->timer, 1500);     // the stop timer, at least 1 s
            KW_DECREF(kw)
            return 0;
        case 8:
            priv->result += test_max_run_partial_retried(gobj);
            priv->result += test_mrw_start(gobj);
            set_timeout(priv->timer, 2500);     // past the stop its max_run schedules
            KW_DECREF(kw)
            return 0;
        case 9:
            priv->result += test_mrw_writer(gobj);
            set_timeout(priv->timer, 1500);     // the stop timer, at least 1 s
            KW_DECREF(kw)
            return 0;
        case 10:
            priv->result += test_mrw_nothing_runs(gobj);
            priv->result += test_mrg_start(gobj);
            set_timeout(priv->timer, 2500);     // past the stop its max_run schedules
            KW_DECREF(kw)
            return 0;
        case 11:
            priv->result += test_mrg_yuno_not_found(gobj);
            priv->result += test_mrc_start(gobj);
            set_timeout(priv->timer, 2500);     // past the stop its max_run schedules
            KW_DECREF(kw)
            return 0;
        case 12:
            priv->result += test_mrc_given_up(gobj);
            priv->result += test_mrb_start(gobj);
            set_timeout(priv->timer, 2500);     // past the stop its max_run schedules
            KW_DECREF(kw)
            return 0;
        case 13:
            priv->result += test_mrb_given_up(gobj);
            priv->result += test_drops_window_shortened(gobj);
            priv->t_wait = start_msectimer(DROPS_WAIT_MS);
            set_timeout(priv->timer, 100);
            KW_DECREF(kw)
            return 0;
        case 14:
            priv->result += test_drops_counted_again(gobj);
            priv->t_wait = start_msectimer(DROPS_WAIT_MS);
            set_timeout(priv->timer, 100);
            KW_DECREF(kw)
            return 0;
        case 15:
            priv->result += test_drops_timer_said(gobj);
            priv->result += test_rates_traffic(gobj);
            set_timeout(priv->timer, 2*RATES_TICK_MS + 500);    // past two ticks
            KW_DECREF(kw)
            return 0;
        case 16:
            priv->result += test_rates_read(gobj);
            set_timeout(priv->timer, RATES_TICK_MS + 500);  // a tick with the 3 messages
            KW_DECREF(kw)
            return 0;
        case 17:
            priv->result += test_stop_outside_shutdown(gobj);
            priv->result += test_no_assertion_key_with_gbuffer(gobj);
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
