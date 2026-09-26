/***********************************************************************
 *          C_TEST5.C
 *
 *          Test a BURST of TLS messages sent at once, as a client does
 *          when it resends its window right after (re)connecting.
 *
 *          Sends BURST_COUNT messages of MESSAGE_SIZE the moment the
 *          link opens, so C_TCP has a long tx queue while the socket's
 *          send buffer fills and writes come back short. Up to 7.25.7
 *          C_TCP let a second TLS write start while the first was still
 *          in flight; a short write then had its remainder sent AFTER
 *          the second one and the peer failed the record MAC ("bad
 *          record mac"). Seen in yunovatios' stress test on the central.
 *
 *          The client's C_TCP must end with max_tx_in_progress == 1: that
 *          is the rule the fix establishes, checked without depending on
 *          the kernel reordering (on a loopback it rarely does).
 *
 *          Every message carries its sequence number and a pattern
 *          derived from it: each echo must arrive complete, intact and
 *          in order. ROUNDS bursts, each one sent when the previous has
 *          come back whole (the test's memory cap is 64 MB).
 *
 *          Tasks
 *          - Play pepon as server with echo
 *          - Open __out_side__
 *          - On open, send BURST_COUNT messages at once
 *          - Verify every echoed message; a whole burst back sends the next
 *            one, ROUNDS times
 *          - Shutdown; a watchdog fails the test if the burst never ends
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <c_pepon.h>
#include "c_test5.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BURST_COUNT         400
#define MESSAGE_SIZE        (64 * 1024)     // 400 x 64 KB = 25 MB at once
#define ROUNDS              10              // each burst when the last came back whole
#define WATCHDOG_MS         60000

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_INTEGER,     "timeout",          SDF_RD,             "1000",     "Timeout"),
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
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    json_int_t timeout;
    hgobj timer;

    hgobj pepon;

    hgobj gobj_output_side;
    json_int_t txMsgs;
    json_int_t rxMsgs;

    char *pattern_buf;          // One message; its first 4 bytes are the sequence
    BOOL done;
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
    json_t *kw_pepon = json_pack("{s:b}",
        "do_echo", 1
    );
    priv->pepon = gobj_create_pure_child("server", C_PEPON, kw_pepon, gobj);

    priv->pattern_buf = GBMEM_MALLOC(MESSAGE_SIZE);

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,               gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    if(!gobj_is_running(priv->pepon)) {
        gobj_start(priv->pepon);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->timer);
    gobj_stop(priv->pepon);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_play(priv->pepon);
    set_timeout(priv->timer, 1000); // timeout to connecting

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_pause(priv->pepon);

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBMEM_FREE(priv->pattern_buf);
}



                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  The message of sequence `seq`: the sequence, then a pattern that moves
 *  with it, so a message swapped with another or cut short shows
 ***************************************************************************/
PRIVATE void fill_message(char *p, uint32_t seq)
{
    memcpy(p, &seq, sizeof(seq));
    for(int i = (int)sizeof(seq); i < MESSAGE_SIZE; i++) {
        p[i] = (char)((i + seq) % 251);    // prime modulus avoids alignment artefacts
    }
}

/***************************************************************************
 *  BURST_COUNT messages at once, continuing the sequence
 ***************************************************************************/
PRIVATE void send_burst(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int i = 0; i < BURST_COUNT; i++) {
        fill_message(priv->pattern_buf, (uint32_t)priv->txMsgs);
        gbuffer_t *gbuf_to_send = gbuffer_create(MESSAGE_SIZE, MESSAGE_SIZE);
        gbuffer_append(gbuf_to_send, priv->pattern_buf, MESSAGE_SIZE);
        priv->txMsgs++;
        json_t *kw_send = json_pack("{s:I}",
            "gbuffer", (json_int_t)(uintptr_t)gbuf_to_send
        );
        gobj_send_event(priv->gobj_output_side, EV_SEND_MESSAGE, kw_send, gobj);
    }
    set_timeout(priv->timer, WATCHDOG_MS);
}

/***************************************************************************
 *  Once: the verdict is logged by the caller
 ***************************************************************************/
PRIVATE void finish(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->done) {
        priv->done = TRUE;
        clear_timeout(priv->timer);
        set_yuno_must_die();
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Connected
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    send_burst(gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_to_connect(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);
    gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_output_side);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Watchdog: the burst did not come back whole
 ***************************************************************************/
PRIVATE int ac_timeout_watchdog(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_error(0, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "Burst did not come back whole",
        "sent",         "%d", (int)priv->txMsgs,
        "received",     "%d", (int)priv->rxMsgs,
        NULL
    );
    finish(gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Each echo: complete, intact, and the next in sequence
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->done) {
        KW_DECREF(kw)
        return 0;
    }

    uint32_t expected = (uint32_t)priv->rxMsgs;
    priv->rxMsgs++;

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    size_t received_len = gbuffer_leftbytes(gbuf);
    char *p = gbuffer_cur_rd_pointer(gbuf);

    uint32_t seq = 0;
    if(received_len >= sizeof(seq)) {
        memcpy(&seq, p, sizeof(seq));
    }
    fill_message(priv->pattern_buf, expected);

    if(received_len != MESSAGE_SIZE) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Burst message size mismatch",
            "expected",     "%d", (int)MESSAGE_SIZE,
            "received",     "%d", (int)received_len,
            "seq",          "%d", (int)expected,
            NULL
        );
        finish(gobj);
    } else if(seq != expected) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Burst message out of order",
            "expected",     "%d", (int)expected,
            "received",     "%d", (int)seq,
            NULL
        );
        finish(gobj);
    } else if(memcmp(p, priv->pattern_buf, MESSAGE_SIZE) != 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Burst message content mismatch",
            "seq",          "%d", (int)expected,
            NULL
        );
        finish(gobj);
    } else if(priv->rxMsgs == BURST_COUNT * ROUNDS) {
        /*
         *  The echoes coming back whole is luck as much as correctness:
         *  whether io_uring sends a short write's rest behind a later write
         *  depends on the kernel and on how full the socket is. The rule
         *  itself is checked: never more than one write in flight.
         */
        hgobj gobj_tcp = gobj_bottom_gobj(          // the protocol's transport
            gobj_child_by_name(
                gobj_child_by_name(priv->gobj_output_side, "output"),
                "output"
            )
        );
        json_int_t max_tx_in_progress = gobj_read_integer_attr(gobj_tcp, "max_tx_in_progress");
        if(max_tx_in_progress != 1) {
            gobj_log_error(0, 0,
                "function",             "%s", __FUNCTION__,
                "msgset",               "%s", MSGSET_INTERNAL,
                "msg",                  "%s", "More than one TLS write in flight",
                "max_tx_in_progress",   "%d", (int)max_tx_in_progress,
                NULL
            );
        } else {
            gobj_log_warning(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Burst OK",
                NULL
            );
        }
        finish(gobj);
    } else if(priv->rxMsgs == priv->txMsgs) {
        send_burst(gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)

    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }

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
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST5);

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
    ev_action_t st_closed[] = {
        {EV_STOPPED,                ac_stopped,                 0},
        {EV_TIMEOUT,                ac_timeout_to_connect,      0},
        {EV_ON_OPEN,                ac_on_open,                 ST_OPENED},
        {0,0,0}
    };
    ev_action_t st_opened[] = {
        {EV_ON_MESSAGE,             ac_on_message,              0},
        {EV_ON_CLOSE,               ac_on_close,                ST_CLOSED},
        {EV_TIMEOUT,                ac_timeout_watchdog,        0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_CLOSED,                 st_closed},
        {ST_OPENED,                 st_opened},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_OPEN,                0},
        {EV_ON_MESSAGE,             0},
        {EV_ON_CLOSE,               0},
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
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,  // s_user_trace_level,
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
PUBLIC int register_c_test5(void)
{
    return create_gclass(C_TEST5);
}
