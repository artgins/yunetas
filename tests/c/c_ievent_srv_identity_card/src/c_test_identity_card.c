/***********************************************************************
 *          C_TEST_IDENTITY_CARD.C
 *
 *          GClass to test the identity cards a peer sends to C_IEVENT_SRV
 *
 *          A raw websocket client (C_IOGATE -> C_CHANNEL -> C_WEBSOCKET ->
 *          C_TCP, no C_IEVENT_CLI) writes its frames by hand, as a peer
 *          that is not a yuno, or a misconfigured one, would. Each card
 *          but the last is refused, and the gate closes the channel; the
 *          client connects again and sends the next one:
 *
 *              0. a card without __md_iev__
 *              1. dst_role of another yuno, and a 4 KB key of its own
 *              2. dst_yuno of another yuno
 *              3. no src_role
 *              4. no src_service
 *              5. dst_service not in this yuno
 *              6. a jwt that is not a string
 *              7. a command before any card
 *              8. a good card: EV_IDENTITY_CARD_ACK, the session opens
 *
 *          Each refusal is caused by the peer: ONE warning, with the kw
 *          capped and no stack (main.c counts the lines, their priority and
 *          their size). Up to 7.25.20 each was an error with the whole kw
 *          dumped, followed by a second error, "event UNKNOWN in
 *          not-session state", with the whole kw again; a card without
 *          __md_iev__ added a third, with a stack; a jwt that is not a
 *          string reached the authentication.
 *
 *          A wrong result is logged as an error, which the expected-logs
 *          check of main.c does not expect.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include "c_test_identity_card.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define LAST_CARD   8

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int send_card(hgobj gobj, int card);
PRIVATE int send_frame(hgobj gobj, gobj_event_t event, json_t *kw);
PRIVATE json_t *card_routing(
    const char *dst_yuno,
    const char *dst_role,
    const char *dst_service,
    const char *src_role,
    const char *src_service
);
PRIVATE void check(hgobj gobj, const char *what, BOOL ok);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
int test_identity_card_failed = 0;

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
    hgobj gobj_input_side;
    hgobj cli_gate;
    int card;           // the card sent on the current connection
    int client_opens;
    int client_closes;
    int server_opens;   // sessions: only the good card opens one
    int acks;
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
 *  The routing a C_IEVENT_CLI puts on its card, with the fields given
 ***************************************************************************/
PRIVATE json_t *card_routing(
    const char *dst_yuno,
    const char *dst_role,
    const char *dst_service,
    const char *src_role,
    const char *src_service
)
{
    return json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
        "dst_yuno", dst_yuno,
        "dst_role", dst_role,
        "dst_service", dst_service,
        "src_yuno", gobj_yuno_name(),
        "src_role", src_role,
        "src_service", src_service,
        "user", "",
        "host", ""
    );
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
 *  The card number `card`
 ***************************************************************************/
PRIVATE int send_card(hgobj gobj, int card)
{
    const char *role = gobj_yuno_role();
    json_t *kw = json_pack("{s:s}", "jwt", "tester-jwt-credential");
    json_t *jn_routing = 0;

    switch(card) {
        case 0:
            break;

        case 1:
            {
                char big[4*1024];
                memset(big, 'x', sizeof(big)-1);
                big[sizeof(big)-1] = 0;
                json_object_set_new(kw, "extra", json_string(big));
                jn_routing = card_routing("", "other_role", "tester", role, "cli");
            }
            break;

        case 2:
            jn_routing = card_routing("other_yuno", role, "tester", role, "cli");
            break;

        case 3:
            jn_routing = card_routing("", role, "tester", "", "cli");
            break;

        case 4:
            jn_routing = card_routing("", role, "tester", role, "");
            break;

        case 5:
            jn_routing = card_routing("", role, "no_such_service", role, "cli");
            break;

        case 6:
            json_object_set_new(kw, "jwt", json_integer(6));
            jn_routing = card_routing("", role, "tester", role, "cli");
            break;

        case 7:
            json_object_set_new(kw, "__command__", json_string("help"));
            msg_iev_push_stack(gobj, kw, IEVENT_STACK_ID,
                card_routing("", role, "tester", role, "cli")
            );
            msg_iev_set_msg_type(gobj, kw, "__command__");
            return send_frame(gobj, EV_MT_COMMAND, kw);

        default:
            jn_routing = card_routing("", role, "tester", role, "cli");
            break;
    }

    if(jn_routing) {
        msg_iev_push_stack(gobj, kw, IEVENT_STACK_ID, jn_routing);
        msg_iev_set_msg_type(gobj, kw, "__identity__");
    }
    return send_frame(gobj, EV_IDENTITY_CARD, kw);
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
        test_identity_card_failed++;
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The client connected: its next card. The gate: a session opened.
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->cli_gate) {
        priv->card = priv->client_opens++;
        send_card(gobj, priv->card);
    } else {
        priv->server_opens++;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The gate closed the client after a refused card
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->cli_gate) {
        priv->client_closes++;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The answer to the good card: the test is done
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED);
    gobj_event_t iev_event = 0;
    json_t *iev_kw = 0;
    if(gbuf) {
        gbuffer_incref(gbuf);
        iev_kw = iev_create_from_gbuffer(gobj, &iev_event, gbuf, 0);
    }
    check(gobj, "the answer is EV_IDENTITY_CARD_ACK", iev_event == EV_IDENTITY_CARD_ACK);
    check(gobj, "the ACK answers the last card", priv->card == LAST_CARD);
    check(gobj, "the ACK is positive", kw_get_int(gobj, iev_kw, "result", -1, 0) == 0);
    priv->acks++;

    check(gobj, "each refused card closed its channel", priv->client_closes == LAST_CARD);
    check(gobj, "only the good card opened a session", priv->server_opens == 1);
    set_yuno_must_die();

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
GOBJ_DEFINE_GCLASS(C_TEST_IDENTITY_CARD);

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

    event_type_t event_types[] = {
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_ON_MESSAGE,             0},
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
PUBLIC int register_c_test_identity_card(void)
{
    return create_gclass(C_TEST_IDENTITY_CARD);
}
