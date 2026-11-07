/***********************************************************************
 *          C_TEST10.C
 *
 *          A class to test C_TCP / C_TCP_S
 *          Test: a TLS server with ssl_verify_mode "request" asks the
 *          client for its certificate and takes it as it comes: its chain
 *          is not verified and NOT reported (no "did NOT verify" warning:
 *          the owner pins it), and its EV_ON_OPEN carries peer_cert with
 *          "verified" false and the spki_sha256 of the client's key -- the
 *          same value `openssl pkey -pubout -outform DER | sha256sum` gives.
 *          A client without a certificate is let in, with no peer_cert.
 *
 *          Tasks
 *          - Start the input side (the server) and two clients: one with
 *            the test certificate as its own, one without
 *          - Each opening of the server is checked: one with the key of the
 *            test certificate, one with no certificate
 *          - Both opened: end. 3000 ms with no end: a fail
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include "c_test10.h"

/*  sha256 of the public key of /yuneta/agent/certs/localhost.key  */
#define TEST10_CLIENT_SPKI "89e7f3e744aa67949802f14616ecd46bdd1af70e0e213342608af8ad289e5d79"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void check(hgobj gobj, const char *what, BOOL ok);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
int test10_reached_end = 0;
int test10_failed = 0;

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
PRIVATE const trace_level_t s_user_trace_level[16] = {
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    hgobj gobj_input_side;
    hgobj gobj_output_side;
    hgobj gobj_output_side2;
    int with_key;       // server openings with the expected key
    int without_key;    // server openings with no certificate
    int other;          // anything else
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

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->timer);

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

    priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);
    gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_output_side);

    priv->gobj_output_side2 = gobj_find_service("__output_side2__", TRUE);
    gobj_subscribe_event(priv->gobj_output_side2, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_output_side2);

    set_timeout(priv->timer, 3000);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    return 0;
}




                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




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
        test10_failed++;
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  An opening of the server: who the client is
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        json_t *peer_cert = json_object_get(kw, "peer_cert");
        if(!peer_cert) {
            priv->without_key++;
        } else if(strcmp(kw_get_str(gobj, peer_cert, "spki_sha256", "", 0), TEST10_CLIENT_SPKI)==0 &&
                !kw_get_bool(gobj, peer_cert, "verified", TRUE, 0) &&
                strcmp(kw_get_str(gobj, peer_cert, "cn", "", 0), "localhost")==0) {
            priv->with_key++;
        } else {
            priv->other++;
            check(gobj, "the client's certificate as the server saw it", FALSE);
        }
        if(priv->with_key + priv->without_key + priv->other == 2) {
            check(gobj, "one client with its key, one without", priv->with_key == 1 && priv->without_key == 1);
            test10_reached_end = 1;
            clear_timeout(priv->timer);
            gobj_stop_tree(priv->gobj_output_side);
            gobj_stop_tree(priv->gobj_output_side2);
            set_yuno_must_die();
        }
    }
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
 *  No end in time
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    check(gobj, "both clients opened in time", FALSE);
    gobj_stop_tree(priv->gobj_output_side);
    gobj_stop_tree(priv->gobj_output_side2);
    set_yuno_must_die();
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
GOBJ_DEFINE_GCLASS(C_TEST10);

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
        {EV_TIMEOUT,                ac_timeout,                 0},
        {EV_ON_OPEN,                ac_on_open,                 0},
        {EV_ON_CLOSE,               ac_on_close,                0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
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
PUBLIC int register_c_test10(void)
{
    return create_gclass(C_TEST10);
}
