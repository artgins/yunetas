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
 *                run refused naming the step.
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
 *  What reached a peer since the last time (yours)
 ***************************************************************************/
PRIVATE json_t *take_received(hgobj peer)
{
    json_t *received = json_incref(gobj_read_user_data(peer, "received"));
    gobj_write_user_data(peer, "received", json_array());
    return received;
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

    if(result == 0) {
        gobj_log_info(gobj, 0,
            "msgset", "%s", MSGSET_INFO,
            "msg", "%s", "All controlcenter scenarios tests PASSED",
            NULL
        );
    }
    return result;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  EV_TIMEOUT: runs the tests inside the event loop, then exits
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    run_tests(gobj);

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
