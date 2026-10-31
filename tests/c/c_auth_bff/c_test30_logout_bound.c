/****************************************************************************
 *          C_TEST30_LOGOUT_BOUND.C
 *
 *          Driver for test30_logout_bound: a logout answers within
 *          idp_logout_timeout_ms, cookies cleared, whatever the IdP does, so
 *          that the answer reaches a SPA that gives up at 15 s. In 7.29.3 it
 *          waited for idp_timeout_ms or idp_connect_timeout_ms, 30 s each.
 *
 *          Six BFFs with the default times (but the last), each with its
 *          IdP, each on its own plain socket, all at once, so that the test
 *          takes 10 s and not a minute:
 *            1. the IdP takes the /logout and stays silent: 504 auth_timeout;
 *            2. nobody listens at the port of the IdP: 504 auth_timeout;
 *            3. the IdP cuts the connection of the first /logout and stays
 *               silent at the second: 504 auth_timeout, 2 /logout: the
 *               resend comes under the same time;
 *            4. the BFF has an `issuer`, and the IdP takes the discovery and
 *               stays silent: 502 auth_service_unavailable: the logout waits
 *               behind the discovery under the same time, and a second
 *               logout 6 s later does not start that time again: both are
 *               answered at 10 s;
 *            5. the BFF has an `issuer`, and nobody listens at its port: the
 *               same;
 *            6. a logout and two logins in one write, idp_logout_timeout_ms
 *               1000 and an IdP that takes 800 ms for each: 200, 200 and
 *               200: the time of a logout ends with it, and it does not fall
 *               on the requests that come after (a C_TIMER counts in seconds,
 *               so it would fall between 1 s and 2 s).
 *
 *          The logs are asserted by the test harness via
 *          set_expected_results in main_test30.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <yunetas.h>

#include "c_test30_logout_bound.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TEST_NAME       "test30_logout_bound"
#define ORIGIN          "http://localhost"  /* the allowed_origin of the BFFs (main_test30) */
#define AT_CLEARED      "Set-Cookie: access_token=; Max-Age=0;"
#define RT_CLEARED      "Set-Cookie: refresh_token=; Max-Age=0;"
#define POLL_MS         50
#define SPA_GIVES_UP_MS 15000       /* BFF_TIMEOUT_MS of the SPAs */
#define MAX_ANSWERS     3

#define LOGOUT_REQUEST \
    "POST /auth/logout HTTP/1.1\r\n" \
    "Host: localhost\r\n" \
    "Origin: " ORIGIN "\r\n" \
    "Cookie: refresh_token=RT1\r\n" \
    "Content-Length: 0\r\n" \
    "\r\n"
#define LOGIN_BODY  "{\"username\": \"testuser\", \"password\": \"testpass\"}"
#define LOGIN_REQUEST \
    "POST /auth/login HTTP/1.1\r\n" \
    "Host: localhost\r\n" \
    "Origin: " ORIGIN "\r\n" \
    "Content-Type: application/json\r\n" \
    "Content-Length: 48\r\n" \
    "\r\n" \
    LOGIN_BODY

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    int         port;           /* of the BFF (main_test30) */
    const char *mock_service;   /* the service of its mock IdP, NULL none */
    const char *request;
    uint64_t    repeat_after_ms;    /* the request goes again after this, 0 no */
    int         answers;        /* whole answers expected */
    int         status;         /* of the first */
    int         status_rest;    /* of the others */
    const char *error_code;     /* its error_code, NULL none */
    int         logouts;        /* /logout requests that must reach the mock */
    uint64_t    min_ms;         /* the first answer not before this */
    uint64_t    max_ms;         /* nor after this */
} slot_t;

typedef struct {
    int         fd;
    uint64_t    sent_at;
    uint64_t    first_answer_at;
    BOOL        repeated;
    BOOL        done;
    size_t      got;
    char        answer[16*1024];
} slot_state_t;

/***************************************************************************
 *              Data
 ***************************************************************************/
PRIVATE const slot_t slots[] = {
    /* 1 */ {18898, "__idp_mute__", LOGOUT_REQUEST, 0, 1, 504, 0, "auth_timeout", 1,
        9000, SPA_GIVES_UP_MS},
    /* 2 */ {18900, NULL, LOGOUT_REQUEST, 0, 1, 504, 0, "auth_timeout", 0,
        9000, SPA_GIVES_UP_MS},
    /* 3 */ {18901, "__idp_cut__", LOGOUT_REQUEST, 0, 1, 504, 0, "auth_timeout", 2,
        9000, SPA_GIVES_UP_MS},
    /* 4 */ {18902, "__idp_mute_discovery__", LOGOUT_REQUEST, 6000, 2, 502, 502,
        "auth_service_unavailable", 0, 9000, SPA_GIVES_UP_MS},
    /* 5 */ {18903, NULL, LOGOUT_REQUEST, 0, 1, 502, 0, "auth_service_unavailable", 0,
        9000, SPA_GIVES_UP_MS},
    /* 6 */ {18904, "__idp_slow__", LOGOUT_REQUEST LOGIN_REQUEST LOGIN_REQUEST, 0, 3, 200, 200,
        NULL, 1, 0, SPA_GIVES_UP_MS},
};
#define NSLOTS ((int)(sizeof(slots)/sizeof(slots[0])))

/*
 *  The services to stop at the end, as main_test30 names them
 */
PRIVATE const char *services[] = {
    "__bff_1__", "__bff_2__", "__bff_3__", "__bff_4__", "__bff_5__", "__bff_6__",
    "__idp_mute__", "__idp_cut__", "__idp_mute_discovery__", "__idp_slow__",
    NULL
};

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type----------name-------------flag----default-description*/
SDATA (DTP_POINTER,   "user_data",     0,      0,      "user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    {"messages",    "Trace test driver flow"},
    {0, 0}
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj           timer;
    BOOL            dying;      /* TRUE once a death-timer is armed */
    int             next_slot;  /* the next one to send its request */
    slot_state_t    state[NSLOTS];
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

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER0, 0, gobj);  // C_TIMER counts in seconds
    for(int i = 0; i < NSLOTS; i++) {
        priv->state[i].fd = -1;
    }
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int i = 0; i < NSLOTS; i++) {
        if(priv->state[i].fd >= 0) {
            close(priv->state[i].fd);
            priv->state[i].fd = -1;
        }
    }
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

    set_timeout0(priv->timer, 500);
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout0(priv->timer);
    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  The mock IdP of a service
 ***************************************************************************/
PRIVATE hgobj find_mock(hgobj gobj, const char *service)
{
    hgobj idp_side = gobj_find_service(service, FALSE);
    if(!idp_side) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", TEST_NAME ": mock service not found",
            "service",  "%s", service,
            NULL
        );
        return NULL;
    }
    json_t *jn_filter = json_pack("{s:s}",
        "__gclass_name__", "C_MOCK_KEYCLOAK"
    );
    json_t *dl = gobj_match_children_tree(idp_side, jn_filter);
    hgobj mock = NULL;
    if(dl && json_array_size(dl) > 0) {
        mock = (hgobj)(size_t)json_integer_value(json_array_get(dl, 0));
    }
    gobj_free_iter(dl);
    return mock;
}

/***************************************************************************
 *  An error of a slot, with what it got
 ***************************************************************************/
PRIVATE void slot_failed(hgobj gobj, int idx, const char *what, const char *text)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_APP,
        "msg",          "%s", what,
        "slot",         "%d", idx + 1,
        "text",         "%s", text,
        "answer",       "%.600s", priv->state[idx].answer,
        NULL
    );
}

/***************************************************************************
 *  Connect the socket of a slot and send its request at once
 ***************************************************************************/
PRIVATE void send_slot(hgobj gobj, int idx)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    slot_state_t *state = &priv->state[idx];

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)slots[idx].port);

    size_t len = strlen(slots[idx].request);
    state->fd = socket(AF_INET, SOCK_STREAM, 0);
    if(state->fd < 0 ||
            connect(state->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
            send(state->fd, slots[idx].request, len, 0) != (ssize_t)len) {
        slot_failed(gobj, idx, TEST_NAME ": cannot send the request", strerror(errno));
        state->done = TRUE;
        return;
    }
    state->sent_at = time_in_milliseconds_monotonic();
}

/***************************************************************************
 *  Read what has come to a slot and count the answers that are whole.
 ***************************************************************************/
PRIVATE int read_answers(hgobj gobj, int idx, const char **starts)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    slot_state_t *state = &priv->state[idx];

    while(state->got < sizeof(state->answer) - 1) {
        ssize_t n = recv(state->fd, state->answer + state->got,
            sizeof(state->answer) - 1 - state->got, MSG_DONTWAIT);
        if(n <= 0) {
            break;
        }
        state->got += (size_t)n;
    }
    state->answer[state->got] = '\0';

    int whole = 0;
    const char *p = state->answer;
    while(whole < MAX_ANSWERS && *p) {
        const char *end_of_headers = strstr(p, "\r\n\r\n");
        const char *content_length = strcasestr(p, "Content-Length:");
        if(!end_of_headers || !content_length || content_length > end_of_headers) {
            break;
        }
        size_t body_len = (size_t)atoi(content_length + strlen("Content-Length:"));
        const char *next = end_of_headers + 4 + body_len;
        if(next > state->answer + state->got) {
            break;
        }
        starts[whole++] = p;
        p = next;
    }
    if(whole > 0 && !state->first_answer_at) {
        state->first_answer_at = time_in_milliseconds_monotonic();
    }
    return whole;
}

/***************************************************************************
 *  What a slot must have got, once it got it
 ***************************************************************************/
PRIVATE void check_slot(hgobj gobj, int idx, const char **starts)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const slot_t *slot = &slots[idx];
    slot_state_t *state = &priv->state[idx];

    uint64_t elapsed = state->first_answer_at - state->sent_at;
    if(elapsed < slot->min_ms || elapsed >= slot->max_ms) {
        char text[64];
        snprintf(text, sizeof(text), "%llu ms", (unsigned long long)elapsed);
        slot_failed(gobj, idx, TEST_NAME ": the answer did not come in its time", text);
    }

    char status_line[32];
    snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d ", slot->status);
    if(strncmp(starts[0], status_line, strlen(status_line)) != 0) {
        slot_failed(gobj, idx, TEST_NAME ": an answer has not the status expected", status_line);
    }
    if(!strstr(starts[0], AT_CLEARED) || !strstr(starts[0], RT_CLEARED)) {
        slot_failed(gobj, idx, TEST_NAME ": a logout that keeps the cookies", "");
    }
    if(slot->error_code && !strstr(starts[0], slot->error_code)) {
        slot_failed(gobj, idx, TEST_NAME ": an answer lacks its error_code", slot->error_code);
    }
    snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d ", slot->status_rest);
    for(int i = 1; i < slot->answers; i++) {
        if(strncmp(starts[i], status_line, strlen(status_line)) != 0) {
            slot_failed(gobj, idx, TEST_NAME ": a request after the first has not its status",
                status_line);
        }
    }
    if(slot->mock_service) {
        hgobj mock = find_mock(gobj, slot->mock_service);
        int logouts = mock? (int)gobj_read_integer_attr(mock, "logouts_seen") : -1;
        if(logouts != slot->logouts) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", TEST_NAME ": the /logout requests that reached the IdP",
                "slot",         "%d", idx + 1,
                "expected",     "%d", slot->logouts,
                "seen",         "%d", logouts,
                NULL
            );
        }
    }
}

/***************************************************************************
 *  Look at the slots that wait. TRUE when none is left.
 ***************************************************************************/
PRIVATE BOOL look_at_slots(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL all_done = priv->next_slot == NSLOTS;
    for(int i = 0; i < priv->next_slot; i++) {
        slot_state_t *state = &priv->state[i];
        if(state->done) {
            continue;
        }
        uint64_t waited = time_in_milliseconds_monotonic() - state->sent_at;
        if(slots[i].repeat_after_ms && !state->repeated && waited >= slots[i].repeat_after_ms) {
            state->repeated = TRUE;
            size_t len = strlen(slots[i].request);
            if(send(state->fd, slots[i].request, len, MSG_DONTWAIT) != (ssize_t)len) {
                slot_failed(gobj, i, TEST_NAME ": cannot send the request again", strerror(errno));
            }
        }
        const char *starts[MAX_ANSWERS];
        if(read_answers(gobj, i, starts) >= slots[i].answers) {
            check_slot(gobj, i, starts);
            state->done = TRUE;
        } else if(waited >= SPA_GIVES_UP_MS) {
            slot_failed(gobj, i, TEST_NAME ": the SPA gave up: no answer", "");
            state->done = TRUE;
        } else {
            all_done = FALSE;
        }
    }
    return all_done;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  Timer fired: send the next request, one per tick so that the logs come
 *  in the order of the slots, and look at the answers. When all came, stop
 *  and die.
 ***************************************************************************/
PRIVATE int ac_timer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->dying) {
        JSON_DECREF(kw)
        set_yuno_must_die();
        return 0;
    }

    if(priv->next_slot < NSLOTS) {
        send_slot(gobj, priv->next_slot);
        priv->next_slot++;
    }

    if(!look_at_slots(gobj)) {
        set_timeout0(priv->timer, POLL_MS);
        JSON_DECREF(kw)
        return 0;
    }

    for(int i = 0; i < NSLOTS; i++) {
        if(priv->state[i].fd >= 0) {
            close(priv->state[i].fd);
            priv->state[i].fd = -1;
        }
    }
    for(const char **name = services; *name; name++) {
        hgobj service = gobj_find_service(*name, FALSE);
        if(service) {
            gobj_stop_tree(service);
        }
    }

    priv->dying = TRUE;
    set_timeout0(priv->timer, 100);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Volatil child stopped: destroy it. The C_TIMER0 publishes one at each
 *  cancel, and is not volatil.
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
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

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST30_LOGOUT_BOUND);

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    ev_action_t st_idle[] = {
        {EV_TIMEOUT,        ac_timer,       0},
        {EV_STOPPED,        ac_stopped,     0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,        0},
        {EV_STOPPED,        0},
        {0, 0}
    };

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,              // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,              // authz_table
        0,              // command_table
        s_user_trace_level,
        0               // gcflag_t
    );
    if(!__gclass__) {
        return -1;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_test30_logout_bound(void)
{
    return create_gclass(C_TEST30_LOGOUT_BOUND);
}
