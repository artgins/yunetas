/***********************************************************************
 *          C_TEST_CLI_PEER.C
 *
 *          GClass to test what C_IEVENT_CLI does with the faults of the
 *          server it is connected to
 *
 *          The server is raw: a websocket gate (C_IOGATE -> C_CHANNEL ->
 *          C_WEBSOCKET -> C_TCP, no C_IEVENT_SRV) whose frames this gclass
 *          writes by hand, as a server that is not a yuno, or a broken one,
 *          would. To each identity card of the client it answers with the
 *          next fault; the client closes the channel, connects again and
 *          sends its card again:
 *
 *              0. a session event (EV_MT_STATS) before the session
 *              1. the ACK, then a frame whose dst_role is another's
 *              2. the ACK, then a frame whose dst_yuno is another's
 *              3. the ACK, then a subscription (__subscribing__)
 *              4. the ACK, then an unsubscription (__unsubscribing__)
 *              5. the ACK: the session opens; then, in the session,
 *                 EV_ON_OPEN and EV_ON_CLOSE (events of the transport
 *                 itself, which C_PROT_HTTP_SR declares public), the ACK
 *                 again, a stats and a command request for a service that
 *                 does not exist, and EV_TREEDB_NODE_UPDATED twice: with no
 *                 dst_service, and with a dst_service that is no service.
 *                 The first three are refused (a WARNING each, the channel
 *                 stays, the app hears nothing); the requests are answered
 *                 with an error (a WARNING each, the name masked: up to
 *                 7.26.9 in clear); the last two are published, as ever,
 *                 the empty name without a log and the unknown one with a
 *                 WARNING. The second ends the test.
 *
 *          The names the server writes (dst_role, dst_yuno, dst_service,
 *          the service of a request) carry a credential. Each fault is the
 *          server's: ONE warning, no stack, its names masked and capped
 *          (main.c counts the lines, their priority, and looks for the
 *          credentials). Up to 7.26.9
 *          each was an ERROR with the names in clear, and the session event
 *          before the session raised "Event NOT DEFINED in state" (an ERROR
 *          with a stack) before its own; EV_ON_OPEN and EV_ON_CLOSE of the
 *          server were published to the app as the client's own, the ACK
 *          in session was "Event NOT DEFINED in state", and an empty or
 *          unknown dst_service an ERROR with a stack.
 *
 *          A wrong result is logged as an error, which the expected-logs
 *          check of main.c does not expect.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include "c_test_cli_peer.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define LAST_STEP   5

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int answer_card(hgobj gobj, hgobj channel, int step);
PRIVATE int send_frame(hgobj gobj, hgobj channel, gobj_event_t event, json_t *kw);
PRIVATE json_t *frame_kw(
    const char *msg_type,
    const char *dst_yuno,
    const char *dst_role,
    const char *dst_service
);
PRIVATE void check(hgobj gobj, const char *what, BOOL ok);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
int test_cli_peer_failed = 0;

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
PRIVATE const trace_level_t s_user_trace_level[16] = {
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj gobj_input_side;  // the raw server
    hgobj gobj_cli;         // the C_IEVENT_CLI
    int step;               // the next fault, one per identity card
    int client_opens;       // sessions of the client
    int client_closes;
    int forwarded;          // EV_TREEDB_NODE_UPDATED published by the client
} PRIVATE_DATA;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
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
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

    priv->gobj_cli = gobj_find_service("cli", TRUE);
    gobj_subscribe_event(priv->gobj_cli, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_cli);
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




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  The kw of a frame of the server: its routing to the client, and its type
 ***************************************************************************/
PRIVATE json_t *frame_kw(
    const char *msg_type,
    const char *dst_yuno,
    const char *dst_role,
    const char *dst_service
)
{
    json_t *kw = json_object();
    json_t *jn_routing = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
        "dst_yuno", dst_yuno,
        "dst_role", dst_role,
        "dst_service", dst_service,
        "src_yuno", "",
        "src_role", "raw_server",
        "src_service", "raw",
        "user", "",
        "host", ""
    );
    msg_iev_push_stack(0, kw, IEVENT_STACK_ID, jn_routing);
    msg_iev_set_msg_type(0, kw, msg_type);
    return kw;
}

/***************************************************************************
 *  A frame written by hand, to the channel of the client
 ***************************************************************************/
PRIVATE int send_frame(hgobj gobj, hgobj channel, gobj_event_t event, json_t *kw)
{
    gbuffer_t *gbuf = iev_create_to_gbuffer(gobj, event, kw);
    if(!gbuf) {
        // Error already logged
        return -1;
    }
    return gobj_send_event(
        channel,
        EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
        gobj
    );
}

/***************************************************************************
 *  The answer to the identity card number `step`
 ***************************************************************************/
PRIVATE int answer_card(hgobj gobj, hgobj channel, int step)
{
    const char *role = gobj_yuno_role();

    if(step == 0) {
        return send_frame(gobj, channel, EV_MT_STATS,
            frame_kw("__stats__", "", role, "cli")
        );
    }

    json_t *kw_ack = frame_kw("__answer__", "", role, "cli");
    json_object_set_new(kw_ack, "result", json_integer(0));
    send_frame(gobj, channel, EV_IDENTITY_CARD_ACK, kw_ack);

    switch(step) {
        case 1:
            return send_frame(gobj, channel, EV_MT_STATS,
                frame_kw("__stats__", "", "other_role password=tester-secret-role", "cli")
            );
        case 2:
            return send_frame(gobj, channel, EV_MT_STATS,
                frame_kw("__stats__", "other_yuno token=tester-secret-yuno", role, "cli")
            );
        case 3:
            return send_frame(gobj, channel, EV_MT_STATS,
                frame_kw("__subscribing__", "", role, "cli password=tester-secret-subscribing")
            );
        case 4:
            return send_frame(gobj, channel, EV_MT_STATS,
                frame_kw("__unsubscribing__", "", role, "cli password=tester-secret-unsubscribing")
            );
        default:
            break;
    }

    send_frame(gobj, channel, EV_ON_OPEN, frame_kw("__message__", "", role, "cli"));
    send_frame(gobj, channel, EV_ON_CLOSE, frame_kw("__message__", "", role, "cli"));
    json_t *kw_ack2 = frame_kw("__answer__", "", role, "cli");
    json_object_set_new(kw_ack2, "result", json_integer(0));
    send_frame(gobj, channel, EV_IDENTITY_CARD_ACK, kw_ack2);
    json_t *kw_stats = frame_kw("__stats__", "", role, "cli");
    json_object_set_new(kw_stats, "service", json_string("nobody token=tester-secret-stats"));
    json_object_set_new(kw_stats, "__stats__", json_string("stats"));
    send_frame(gobj, channel, EV_MT_STATS, kw_stats);
    json_t *kw_command = frame_kw("__command__", "", role, "cli");
    json_object_set_new(kw_command, "service", json_string("nobody token=tester-secret-command"));
    json_object_set_new(kw_command, "__command__", json_string("help"));
    send_frame(gobj, channel, EV_MT_COMMAND, kw_command);
    send_frame(gobj, channel, EV_TREEDB_NODE_UPDATED, frame_kw("__message__", "", role, ""));
    return send_frame(gobj, channel, EV_TREEDB_NODE_UPDATED,
        frame_kw("__message__", "", role, "nobody password=tester-secret-service")
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void check(hgobj gobj, const char *what, BOOL ok)
{
    if(!ok) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL",
            "what",         "%s", what,
            NULL
        );
        test_cli_peer_failed++;
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The client opened a session: after the last card, the test is done
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_cli) {
        priv->client_opens++;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  An event of the server the client publishes (no service of its name):
 *  after the second one, the test is done
 ***************************************************************************/
PRIVATE int ac_forwarded(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_cli) {
        priv->forwarded++;
        if(priv->forwarded == 2) {
            check(gobj, "the last card was answered", priv->step > LAST_STEP);
            check(gobj, "each fault closed the channel, and the transport's own events were not heard",
                priv->client_closes == LAST_STEP);
            check(gobj, "a session opened for each ACK, and the transport's own events were not heard",
                priv->client_opens == LAST_STEP);
            set_yuno_must_die();
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The client closed its channel after a fault (EV_ON_OPEN_ERROR when it
 *  had no session yet)
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_cli) {
        priv->client_closes++;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A frame of the client to the raw server: its identity card is answered
 *  with the next fault
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED);
    hgobj channel = (hgobj)(uintptr_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    gobj_event_t iev_event = 0;
    json_t *iev_kw = 0;
    if(gbuf) {
        gbuffer_incref(gbuf);
        iev_kw = iev_create_from_gbuffer(gobj, &iev_event, gbuf, 0);
    }
    if(iev_event == EV_IDENTITY_CARD && channel) {
        answer_card(gobj, channel, priv->step++);
    }

    KW_DECREF(iev_kw)
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
    KW_DECREF(kw)
    return 0;
}




/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_CLI_PEER);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *
 ***************************************************************************/
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

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_idle[] = {
        {EV_ON_OPEN,                ac_on_open,                 0},
        {EV_ON_CLOSE,               ac_on_close,                0},
        {EV_ON_OPEN_ERROR,          ac_on_close,                0},
        {EV_ON_MESSAGE,             ac_on_message,              0},
        {EV_TREEDB_NODE_UPDATED,    ac_forwarded,               0},
        {EV_STOPPED,                ac_stopped,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_ON_OPEN_ERROR,          0},
        {EV_ON_MESSAGE,             0},
        {EV_TREEDB_NODE_UPDATED,    0},
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
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,
        0   // gcflag_t
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_test_cli_peer(void)
{
    return create_gclass(C_TEST_CLI_PEER);
}
