/****************************************************************************
 *          C_PERF_AUTH_BFF.C
 *
 *          Throughput benchmark for c_auth_bff, ping_pong style.
 *
 *          NUM_SLOTS concurrent HTTP/1.1 keep-alive clients pump
 *          /auth/login POSTs at NUM_SLOTS parallel BFF channels for
 *          `run_seconds` seconds.  Every second the test prints live
 *          Msg/sec and Bytes/sec to the console (ANSI overwrite in
 *          place), same layout as perf_yev_ping_pong.
 *
 *          Mock Keycloak runs with latency_ms=0 so the bottleneck is
 *          the BFF + parser + task + event-loop hot path, not the
 *          simulated upstream.
 *
 *          Each slot opens ONE TCP connection and pumps every request
 *          through it — no reconnect per iteration, same persistent
 *          pattern perf_c_tcps uses.  http_cl is created volatil so
 *          ac_stopped destroys it at end-of-run.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdio.h>

#include <yunetas.h>
#include <ansi_escape_codes.h>

#include "c_perf_auth_bff.h"
#include "test_helpers.h"

/***************************************************************************
 *          Tunables
 ***************************************************************************/
#define NUM_SLOTS              5
#define DEFAULT_RUN_SECONDS    10

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
SDATA (DTP_STRING,    "bff_url",        SDF_RD, "http://127.0.0.1:18801/", "BFF URL"),
SDATA (DTP_INTEGER,   "run_seconds",    SDF_RD, "10",   "Seconds to run the benchmark"),
SDATA (DTP_POINTER,   "user_data",      0,      0,      "user data"),
SDATA_END()
};

enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    {"messages",    "Trace perf orchestrator flow"},
    {0, 0}
};

typedef enum {
    SLOT_IDLE       = 0,
    SLOT_CONNECTING,
    SLOT_POSTED,
} slot_state_t;

typedef struct {
    hgobj           http_cl;
    slot_state_t    state;
} slot_t;

typedef struct _PRIVATE_DATA {
    hgobj       timer;
    slot_t      slots[NUM_SLOTS];

    int         run_seconds;
    int         elapsed_seconds;
    BOOL        launched;
    BOOL        dying;

    /* Live throughput (reset every 1 s). */
    uint64_t    msg_window;
    uint64_t    bytes_window;
} PRIVATE_DATA;




            /***************************
             *      Prototypes
             ***************************/




PRIVATE void launch_all_slots(hgobj gobj);
PRIVATE void open_slot(hgobj gobj, int slot_idx);
PRIVATE void send_request(hgobj gobj, int slot_idx);
PRIVATE void stop_all_slots(hgobj gobj);
PRIVATE int  slot_index_for_src(hgobj gobj, hgobj src);
PRIVATE void print_throughput(uint64_t msg, uint64_t bytes);




            /******************************
             *      Framework Methods
             ******************************/




PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER0, 0, gobj);

    priv->run_seconds = (int)gobj_read_integer_attr(gobj, "run_seconds");
    if(priv->run_seconds <= 0) {
        priv->run_seconds = DEFAULT_RUN_SECONDS;
    }
}

PRIVATE void mt_destroy(hgobj gobj)
{
}

PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gobj_start(priv->timer);
    return 0;
}

PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gobj_stop(priv->timer);
    for(int i = 0; i < NUM_SLOTS; i++) {
        if(priv->slots[i].http_cl && gobj_is_running(priv->slots[i].http_cl)) {
            gobj_stop_tree(priv->slots[i].http_cl);
        }
    }
    return 0;
}

PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    printf("\n----------------> Quit in %d seconds <-----------------\n\n",
        priv->run_seconds);
    fflush(stdout);

    /* Warm-up delay so the BFF-side services are fully up. */
    set_timeout0(priv->timer, 500);
    return 0;
}

PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    clear_timeout0(priv->timer);
    return 0;
}




            /***************************
             *      Helpers
             ***************************/




PRIVATE int slot_index_for_src(hgobj gobj, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    for(int i = 0; i < NUM_SLOTS; i++) {
        if(priv->slots[i].http_cl == src) {
            return i;
        }
    }
    return -1;
}

PRIVATE void open_slot(hgobj gobj, int slot_idx)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    slot_t *slot = &priv->slots[slot_idx];

    const char *bff_url = gobj_read_str_attr(gobj, "bff_url");

    slot->http_cl = gobj_create_volatil(
        gobj_name(gobj),
        C_PROT_HTTP_CL,
        json_pack("{s:s}", "url", bff_url),
        gobj
    );
    gobj_set_bottom_gobj(
        slot->http_cl,
        gobj_create_pure_child(
            gobj_name(gobj),
            C_TCP,
            json_pack("{s:s}", "url", bff_url),
            slot->http_cl
        )
    );
    slot->state = SLOT_CONNECTING;
    gobj_start_tree(slot->http_cl);
}

PRIVATE void send_request(hgobj gobj, int slot_idx)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    slot_t *slot = &priv->slots[slot_idx];

    json_t *jn_headers = json_pack("{s:s}",
        "Origin", "http://localhost"
    );
    json_t *jn_data = json_pack("{s:s, s:s}",
        "username", "perfuser",
        "password", "perfpass"
    );
    json_t *query = json_pack("{s:s, s:s, s:o, s:o}",
        "method",   "POST",
        "resource", "/auth/login",
        "headers",  jn_headers,
        "data",     jn_data
    );
    gobj_send_event(slot->http_cl, EV_SEND_MESSAGE, query, gobj);
    slot->state = SLOT_POSTED;
}

PRIVATE void stop_all_slots(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    for(int i = 0; i < NUM_SLOTS; i++) {
        slot_t *slot = &priv->slots[i];
        if(slot->http_cl && gobj_is_running(slot->http_cl)) {
            gobj_stop_tree(slot->http_cl);
        }
        slot->state = SLOT_IDLE;
    }
}

PRIVATE void launch_all_slots(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->launched) {
        return;
    }
    priv->launched = TRUE;

    for(int i = 0; i < NUM_SLOTS; i++) {
        open_slot(gobj, i);
    }
}

/*
 *  Fixed estimate of a /auth/login reply's wire size:
 *    ~170 B  body JSON
 *    ~300 B  two Set-Cookie headers (HttpOnly, Secure, SameSite, Path)
 *    ~ 80 B  status line + Content-Type + Content-Length + CRLFs
 *    -----
 *    ~550 B  per response
 *  Absolute accuracy doesn't matter here — every iteration is
 *  accounted the same way, so the Bytes/sec curve tracks Msg/sec
 *  faithfully for run-to-run comparisons.
 */
#define BYTES_PER_REPLY 550

PRIVATE void print_throughput(uint64_t msg, uint64_t bytes)
{
    char nice[64];
    printf("\n" Erase_Whole_Line Move_Horizontal, 1);
    nice_size(nice, sizeof(nice), msg, FALSE);
    printf("Msg/sec    : %s\n", nice);
    printf(Erase_Whole_Line Move_Horizontal, 1);
    nice_size(nice, sizeof(nice), bytes, FALSE);
    printf("Bytes/sec  : %s\n", nice);
    printf(Cursor_Up, 3);
    printf(Move_Horizontal, 1);
    fflush(stdout);
}




            /***************************
             *      Actions
             ***************************/




PRIVATE int ac_timer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->dying) {
        /*
         *  Tear down both sides of the bench cleanly before the yuno
         *  exits — otherwise gobj_end finds the BFF's in-flight
         *  C_PROT_HTTP_SR still running and logs "Destroying a
         *  RUNNING gobj".
         */
        hgobj bff_side = gobj_find_service("__bff_side__", FALSE);
        if(bff_side) {
            gobj_stop_tree(bff_side);
        }
        hgobj kc_side = gobj_find_service("__idp_side__", FALSE);
        if(kc_side) {
            gobj_stop_tree(kc_side);
        }
        JSON_DECREF(kw)
        set_yuno_must_die();
        return 0;
    }

    if(!priv->launched) {
        /* Warm-up elapsed — fire the slots and start the 1 s sampler. */
        launch_all_slots(gobj);
        set_timeout_periodic0(priv->timer, 1000);
        JSON_DECREF(kw)
        return 0;
    }

    /* Periodic 1 s tick: print the window + advance toward the cap. */
    print_throughput(priv->msg_window, priv->bytes_window);
    priv->msg_window = 0;
    priv->bytes_window = 0;
    priv->elapsed_seconds++;

    if(priv->elapsed_seconds >= priv->run_seconds) {
        stop_all_slots(gobj);
        priv->dying = TRUE;
        /* Next tick (1 s away) fires set_yuno_must_die.  Plenty of
         * time for ac_stopped to drain every slot. */
    }

    JSON_DECREF(kw)
    return 0;
}

PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    int idx = slot_index_for_src(gobj, src);
    if(idx < 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "perf: EV_ON_OPEN from unknown src",
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    send_request(gobj, idx);

    JSON_DECREF(kw)
    return 0;
}

PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int idx = slot_index_for_src(gobj, src);
    if(idx < 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "perf: EV_ON_MESSAGE from unknown src",
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    int status = (int)kw_get_int(gobj, kw, "response_status_code", -1, 0);
    if(status == 200) {
        priv->msg_window++;
        priv->bytes_window += BYTES_PER_REPLY;
    } else {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "perf: non-200 response",
            "slot",     "%d", idx,
            "status",   "%d", status,
            NULL
        );
    }

    JSON_DECREF(kw)

    /* Keep pumping until the death flag flips.  After stop_all_slots
     * the http_cl is no longer running so this send is a no-op (and
     * it won't leak — we guard above via slot_index_for_src). */
    if(!priv->dying) {
        send_request(gobj, idx);
    }
    return 0;
}

PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int idx = slot_index_for_src(gobj, src);
    if(idx >= 0) {
        priv->slots[idx].http_cl = NULL;
    }

    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
    JSON_DECREF(kw)
    return 0;
}




                    /***************************
                     *      FSM
                     ***************************/




PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_play    = mt_play,
    .mt_pause   = mt_pause,
};

GOBJ_DEFINE_GCLASS(C_PERF_AUTH_BFF);

PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    ev_action_t st_idle[] = {
        {EV_TIMEOUT,            ac_timer,       0},
        {EV_TIMEOUT_PERIODIC,   ac_timer,       0},
        {EV_ON_OPEN,            ac_on_open,     0},
        {EV_ON_MESSAGE,         ac_on_message,  0},
        {EV_ON_CLOSE,           ac_on_close,    0},
        {EV_STOPPED,            ac_stopped,     0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
        {EV_TIMEOUT_PERIODIC,   0},
        {EV_ON_OPEN,            0},
        {EV_ON_MESSAGE,         0},
        {EV_ON_CLOSE,           0},
        {EV_STOPPED,            0},
        {0, 0}
    };

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,
        0,
        s_user_trace_level,
        0
    );
    if(!__gclass__) {
        return -1;
    }
    return 0;
}

PUBLIC int register_c_perf_auth_bff(void)
{
    return create_gclass(C_PERF_AUTH_BFF);
}
