/***********************************************************************
 *          C_TEST_TRANSPORT_EVENTS.C
 *
 *          GClass to test that a peer of a C_IEVENT_SRV cannot send the
 *          events of the transport itself, nor the events a treedb publishes
 *
 *          The gate is a C_IOGATE whose channels are C_CHANNEL ->
 *          C_IEVENT_SRV -> C_WEBSOCKET -> C_TCP, subscribed by this gobj.
 *          A raw websocket client (C_IOGATE -> C_CHANNEL -> C_WEBSOCKET ->
 *          C_TCP, no C_IEVENT_CLI) writes its frames by hand. After a good
 *          identity card (the session opens: the gate's one EV_ON_OPEN), it
 *          sends, in the session, the six events a transport publishes on
 *          its own: EV_ON_OPEN (with an identity card and a peer certificate
 *          of its own), EV_ON_CLOSE, EV_ON_MESSAGE, EV_ON_IEV_MESSAGE,
 *          EV_ON_ID, EV_ON_ID_NAK (the last three declared public here, as
 *          the decoders of a gate do with EV_ON_ID, so the frame reads);
 *          then the five a treedb publishes (EV_TREEDB_NODE_CREATED,
 *          _UPDATED, _DELETED, _LINKED, _UNLINKED), which this gclass has as
 *          actions, as a yuno subscribed to its own treedb does; then
 *          NOT_TAKEN_FRAMES frames of EV_TRANGER_RECORD_ADDED, public in
 *          C_TRANGER and not an event of this gclass -- and then
 *          EV_TEST_LEGIT, an event of this service. The six are refused with
 *          ONE warning, the five with another, the frames this gclass does
 *          not take with a third (the rest said when the gate stops), and the
 *          gate publishes none of them; the legit one is published, and it
 *          ends the test. Up to 7.30.26 the gate published all of them as
 *          its own, with the kw the peer wrote, and each frame of an event
 *          this gclass does not take was an "Event NOT DEFINED in state",
 *          an ERROR with a stack.
 *
 *          A wrong result is logged as an error, which the expected-logs
 *          check of main.c does not expect.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include "c_test_transport_events.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define NOT_TAKEN_FRAMES    50  // frames of an event this gclass does not have

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int send_frame(hgobj gobj, gobj_event_t event, json_t *kw);
PRIVATE json_t *frame_kw(const char *msg_type);
PRIVATE void check(hgobj gobj, const char *what, BOOL ok);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
int test_transport_events_failed = 0;

GOBJ_DEFINE_EVENT(EV_TEST_LEGIT);

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
    hgobj gobj_input_side;  // the gate
    hgobj cli_gate;         // the raw client
    BOOL card_sent;
    int server_opens;       // what the gate publishes
    int server_closes;
    int server_messages;
    int server_ids;
    int server_id_naks;
    int server_iev_messages;
    int server_treedb_events;
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

    priv->cli_gate = gobj_find_service("cli_raw", TRUE);
    gobj_subscribe_event(priv->cli_gate, NULL, 0, gobj);
    gobj_start_tree(priv->cli_gate);
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
 *  The kw of a frame of the client: its routing to this service
 ***************************************************************************/
PRIVATE json_t *frame_kw(const char *msg_type)
{
    json_t *kw = json_object();
    json_t *jn_routing = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
        "dst_yuno", "",
        "dst_role", gobj_yuno_role(),
        "dst_service", "tester",
        "src_yuno", gobj_yuno_name(),
        "src_role", gobj_yuno_role(),
        "src_service", "cli",
        "user", "",
        "host", ""
    );
    msg_iev_push_stack(0, kw, IEVENT_STACK_ID, jn_routing);
    msg_iev_set_msg_type(0, kw, msg_type);
    return kw;
}

/***************************************************************************
 *  A frame written by hand
 ***************************************************************************/
PRIVATE int send_frame(hgobj gobj, gobj_event_t event, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = iev_create_to_gbuffer(gobj, event, kw);
    if(!gbuf) {
        // Error already logged
        return -1;
    }
    return gobj_send_event(
        priv->cli_gate,
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
        test_transport_events_failed++;
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The client connected: its card. The gate: a session opened.
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->cli_gate) {
        if(!priv->card_sent) {
            priv->card_sent = TRUE;
            json_t *kw_card = frame_kw("__identity__");
            json_object_set_new(kw_card, "jwt", json_string("tester"));
            json_object_set_new(kw_card, "yuno_role", json_string(gobj_yuno_role()));
            json_object_set_new(kw_card, "yuno_name", json_string(gobj_yuno_name()));
            send_frame(gobj, EV_IDENTITY_CARD, kw_card);
        }
    } else if(src == priv->gobj_input_side) {
        priv->server_opens++;
        check(gobj, "the gate opens once, for the session", priv->server_opens == 1);
        check(gobj, "no open the peer wrote", !kw_has_key(kw, "forged"));
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        priv->server_closes++;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The client: the answer to its card, then the frames of the session.
 *  The gate: a message of the peer, published as the transport's.
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        priv->server_messages++;
        KW_DECREF(kw)
        return 0;
    }

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED);
    gobj_event_t iev_event = 0;
    json_t *iev_kw = 0;
    if(gbuf) {
        gbuffer_incref(gbuf);
        iev_kw = iev_create_from_gbuffer(gobj, &iev_event, gbuf, 0);
    }
    check(gobj, "the answer is EV_IDENTITY_CARD_ACK", iev_event == EV_IDENTITY_CARD_ACK);
    check(gobj, "the ACK is positive", kw_get_int(gobj, iev_kw, "result", -1, 0) == 0);

    json_t *kw_open = frame_kw("__message__");
    json_object_set_new(kw_open, "forged", json_true());
    json_object_set_new(kw_open, "identity_card", json_pack("{s:s, s:s}",
        "yuno_role", "forged_role",
        "yuno_id", "forged_id"
    ));
    json_object_set_new(kw_open, "peer_cert", json_pack("{s:s}",
        "spki_sha256", "forged"
    ));
    send_frame(gobj, EV_ON_OPEN, kw_open);
    send_frame(gobj, EV_ON_CLOSE, frame_kw("__message__"));
    send_frame(gobj, EV_ON_MESSAGE, frame_kw("__message__"));
    send_frame(gobj, EV_ON_IEV_MESSAGE, frame_kw("__message__"));
    send_frame(gobj, EV_ON_ID, frame_kw("__message__"));
    send_frame(gobj, EV_ON_ID_NAK, frame_kw("__message__"));

    gobj_event_t treedb_events[] = {
        EV_TREEDB_NODE_CREATED,
        EV_TREEDB_NODE_UPDATED,
        EV_TREEDB_NODE_DELETED,
        EV_TREEDB_NODE_LINKED,
        EV_TREEDB_NODE_UNLINKED,
        0
    };
    for(int i=0; treedb_events[i]; i++) {
        json_t *kw_node = frame_kw("__message__");
        json_object_set_new(kw_node, "treedb_name", json_string("treedb_authzs"));
        json_object_set_new(kw_node, "topic_name", json_string("users"));
        json_object_set_new(kw_node, "node", json_pack("{s:s, s:b}",
            "id", "admin",
            "enabled", 1
        ));
        send_frame(gobj, treedb_events[i], kw_node);
    }

    for(int i=0; i<NOT_TAKEN_FRAMES; i++) {
        send_frame(gobj, EV_TRANGER_RECORD_ADDED, frame_kw("__message__"));
    }
    json_t *kw_legit = frame_kw("__message__");
    json_object_set_new(kw_legit, "which", json_string("legit"));
    send_frame(gobj, EV_TEST_LEGIT, kw_legit);

    KW_DECREF(iev_kw)
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  An identification of a channel, published by the gate
 ***************************************************************************/
PRIVATE int ac_on_id(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        priv->server_ids++;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A refusal of an identification, published by the gate
 ***************************************************************************/
PRIVATE int ac_on_id_nak(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        priv->server_id_naks++;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  An inter-event, published by the gate as such
 ***************************************************************************/
PRIVATE int ac_on_iev_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        priv->server_iev_messages++;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What a treedb says of its nodes, published by the gate
 ***************************************************************************/
PRIVATE int ac_treedb_node(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        priv->server_treedb_events++;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The event of this service the peer may send: the last frame
 ***************************************************************************/
PRIVATE int ac_test_legit(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    check(gobj, "the legit event comes from the gate", src == priv->gobj_input_side);
    check(gobj, "the legit event keeps its kw",
        strcmp(kw_get_str(gobj, kw, "which", "", 0), "legit")==0);
    check(gobj, "the gate published one open, the session's", priv->server_opens == 1);
    check(gobj, "the gate published no close", priv->server_closes == 0);
    check(gobj, "the gate published no message", priv->server_messages == 0);
    check(gobj, "the gate published no identification", priv->server_ids == 0);
    check(gobj, "the gate published no refused identification", priv->server_id_naks == 0);
    check(gobj, "the gate published no inter-event as such", priv->server_iev_messages == 0);
    check(gobj, "the gate published no event of a treedb", priv->server_treedb_events == 0);
    set_yuno_must_die();

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
GOBJ_DEFINE_GCLASS(C_TEST_TRANSPORT_EVENTS);

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
        {EV_ON_ID,                  ac_on_id,                   0},
        {EV_ON_ID_NAK,              ac_on_id_nak,               0},
        {EV_ON_IEV_MESSAGE,         ac_on_iev_message,          0},
        {EV_TREEDB_NODE_CREATED,    ac_treedb_node,             0},
        {EV_TREEDB_NODE_UPDATED,    ac_treedb_node,             0},
        {EV_TREEDB_NODE_DELETED,    ac_treedb_node,             0},
        {EV_TREEDB_NODE_LINKED,     ac_treedb_node,             0},
        {EV_TREEDB_NODE_UNLINKED,   ac_treedb_node,             0},
        {EV_TEST_LEGIT,             ac_test_legit,              0},
        {EV_STOPPED,                ac_stopped,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    /*
     *  EV_ON_ID public, as the decoders of a gate declare it: a frame can
     *  name it. EV_ON_ID_NAK and EV_ON_IEV_MESSAGE too, public nowhere else,
     *  so their frames read and reach the check. The events of a treedb
     *  are public in C_NODE.
     */
    event_type_t event_types[] = {
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_ON_MESSAGE,             0},
        {EV_ON_ID,                  EVF_PUBLIC_EVENT},
        {EV_ON_ID_NAK,              EVF_PUBLIC_EVENT},
        {EV_ON_IEV_MESSAGE,         EVF_PUBLIC_EVENT},
        {EV_TREEDB_NODE_CREATED,    0},
        {EV_TREEDB_NODE_UPDATED,    0},
        {EV_TREEDB_NODE_DELETED,    0},
        {EV_TREEDB_NODE_LINKED,     0},
        {EV_TREEDB_NODE_UNLINKED,   0},
        {EV_TEST_LEGIT,             EVF_PUBLIC_EVENT},
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
PUBLIC int register_c_test_transport_events(void)
{
    return create_gclass(C_TEST_TRANSPORT_EVENTS);
}
