/***********************************************************************
 *          C_TEST_PLAY_PAUSE.C
 *
 *          GClass to test who may play and pause a yuno through a
 *          C_IEVENT_CLI: only the link to the yuno's own agent
 *
 *          The server is raw: a websocket gate (C_IOGATE -> C_CHANNEL ->
 *          C_WEBSOCKET -> C_TCP, no C_IEVENT_SRV) whose frames this gclass
 *          writes by hand. Two clients of this yuno connect to it, one after
 *          the other:
 *
 *              1. `cli_other`, a link configured to reach a `central`, as a
 *                 gate's link to a central or the agent's link to its
 *                 control center. Its server answers the identity card
 *                 claiming to be a `yuneta_agent` (a peer chooses what it
 *                 says), then sends EV_PAUSE_YUNO twice and EV_PLAY_YUNO.
 *                 All three are refused: each ack carries result -1 and a
 *                 comment, the yuno keeps playing, and ONE warning is
 *                 logged for the session. Up to 7.30.26 the first pause
 *                 paused the yuno.
 *              2. `cli_agent`, the link the agent gives every yuno it
 *                 launches (configured to reach a `yuneta_agent`). Its
 *                 EV_PAUSE_YUNO pauses the yuno: ack result 0.
 *
 *          A wrong result is logged as an error, which the expected-logs
 *          check of main.c does not expect.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include "c_test_play_pause.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int send_frame(hgobj gobj, hgobj channel, gobj_event_t event, json_t *kw);
PRIVATE json_t *frame_kw(const char *msg_type, const char *src_role);
PRIVATE void check(hgobj gobj, const char *what, BOOL ok);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
int test_play_pause_failed = 0;

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
    hgobj gobj_cli_other;   // a link that is not the one to the agent
    hgobj gobj_cli_agent;   // the link to the agent
    hgobj channel_other;    // the server's channel of each client
    hgobj channel_agent;
    int other_acks;         // acks of cli_other
    int agent_acks;         // acks of cli_agent
    int pauses;             // mt_pause of this gobj (the yuno paused)
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

    if(priv->gobj_input_side) {
        return 0;   // played again: what a wrong pause/play does
    }
    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

    priv->gobj_cli_other = gobj_find_service("cli_other", TRUE);
    priv->gobj_cli_agent = gobj_find_service("cli_agent", TRUE);
    gobj_start_tree(priv->gobj_cli_other);
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->pauses++;
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
PRIVATE json_t *frame_kw(const char *msg_type, const char *src_role)
{
    json_t *kw = json_object();
    json_t *jn_routing = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
        "dst_yuno", "",
        "dst_role", gobj_yuno_role(),
        "dst_service", "",
        "src_yuno", "",
        "src_role", src_role,
        "src_service", "agent",
        "user", "",
        "host", ""
    );
    msg_iev_push_stack(0, kw, IEVENT_STACK_ID, jn_routing);
    msg_iev_set_msg_type(0, kw, msg_type);
    return kw;
}

/***************************************************************************
 *  A frame written by hand, to the channel of a client
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
        test_play_pause_failed++;
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  A frame of a client to the raw server
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
    if(!iev_kw || !channel) {
        check(gobj, "a frame of a client and its channel", FALSE);
        KW_DECREF(iev_kw)
        KW_DECREF(kw)
        return -1;
    }

    if(iev_event == EV_IDENTITY_CARD) {
        json_t *jn_routing = msg_iev_get_stack(gobj, iev_kw, IEVENT_STACK_ID, TRUE);
        const char *wanted_role = kw_get_str(gobj, jn_routing, "dst_role", "", 0);
        /*
         *  Both servers say they are a yuneta_agent: the peer's word
         *  must not count, only the role the link was configured to reach
         */
        json_t *kw_ack = frame_kw("__answer__", "yuneta_agent");
        json_object_set_new(kw_ack, "result", json_integer(0));
        send_frame(gobj, channel, EV_IDENTITY_CARD_ACK, kw_ack);

        if(strcmp(wanted_role, "central")==0) {
            priv->channel_other = channel;
            send_frame(gobj, channel, EV_PAUSE_YUNO, frame_kw("__message__", "yuneta_agent"));
            send_frame(gobj, channel, EV_PAUSE_YUNO, frame_kw("__message__", "yuneta_agent"));
            send_frame(gobj, channel, EV_PLAY_YUNO, frame_kw("__message__", "yuneta_agent"));
        } else if(strcmp(wanted_role, "yuneta_agent")==0) {
            priv->channel_agent = channel;
            send_frame(gobj, channel, EV_PAUSE_YUNO, frame_kw("__message__", "yuneta_agent"));
        } else {
            check(gobj, "the identity card of a known client", FALSE);
        }

    } else if(iev_event == EV_PAUSE_YUNO_ACK || iev_event == EV_PLAY_YUNO_ACK) {
        json_int_t ack_result = kw_get_int(gobj, iev_kw, "result", 99, 0);
        const char *comment = kw_get_str(gobj, iev_kw, "comment", "", 0);

        if(channel == priv->channel_other) {
            priv->other_acks++;
            check(gobj, "a play/pause by a link that is not the agent's is refused (ack -1)",
                ack_result == -1);
            check(gobj, "the refusal says why", strstr(comment, "only the link to the yuno's agent") != NULL);
            check(gobj, "the yuno keeps playing", gobj_is_playing(gobj_yuno()));
            check(gobj, "the yuno was never paused", priv->pauses == 0);
            if(priv->other_acks == 3) {
                check(gobj, "the last of the three acks is the play's", iev_event == EV_PLAY_YUNO_ACK);
                gobj_start_tree(priv->gobj_cli_agent);
            }

        } else if(channel == priv->channel_agent) {
            priv->agent_acks++;
            check(gobj, "the agent's link pauses the yuno (ack 0)", ack_result == 0);
            check(gobj, "the ack of the agent's pause", iev_event == EV_PAUSE_YUNO_ACK);
            check(gobj, "the yuno is paused", !gobj_is_playing(gobj_yuno()));
            check(gobj, "the yuno was paused once", priv->pauses == 1);
            check(gobj, "the three refusals came first", priv->other_acks == 3);
            set_yuno_must_die();

        } else {
            check(gobj, "an ack of a known channel", FALSE);
        }
    }

    KW_DECREF(iev_kw)
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
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
GOBJ_DEFINE_GCLASS(C_TEST_PLAY_PAUSE);

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
        {EV_ON_MESSAGE,             ac_on_message,              0},
        {EV_STOPPED,                ac_stopped,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    /*
     *  The acks are public (and so output) here only so the raw server can
     *  decode them
     */
    event_type_t event_types[] = {
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_ON_MESSAGE,             0},
        {EV_STOPPED,                0},
        {EV_PLAY_YUNO_ACK,          EVF_OUTPUT_EVENT|EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
        {EV_PAUSE_YUNO_ACK,         EVF_OUTPUT_EVENT|EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
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
PUBLIC int register_c_test_play_pause(void)
{
    return create_gclass(C_TEST_PLAY_PAUSE);
}
