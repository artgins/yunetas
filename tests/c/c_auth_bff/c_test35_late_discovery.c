/****************************************************************************
 *          C_TEST35_LATE_DISCOVERY.C
 *
 *          Driver for test35_late_discovery: a discovery that times out with
 *          its connection to the IdP open (main_test35). One BFF with one
 *          IdP A, a mock on a keep-alive connection that answers in order:
 *          its first answer 4 s after the request, each next one 1 s
 *          after the one before (serial_latency_ms), and each token of the
 *          user of the refresh_token it got (username_from_refresh_token).
 *          The BFF has idp_timeout_ms 2000 (it fires at 2-3 s: C_TASK
 *          times it with a C_TIMER).
 *
 *             1. A refresh of U1 at 3.3 s: the discovery of the start timed
 *                out, its answer still to come. 200, the tokens of U1.
 *             2. The browser closes, another one comes: a refresh of U2.
 *                200, the tokens of U2.
 *
 *          Up to 7.31.29 the discovery that timed out left the connection
 *          open: the next discovery took its late answer, the refresh of U1
 *          took the answer of that discovery (an error for U1), and the
 *          refresh of U2 took the answer of U1's: U2 got the session of U1.
 *
 *          The logs are asserted by the test harness via
 *          set_expected_results in main_test35, with their priority.
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

#include "c_test35_late_discovery.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TEST_NAME       "test35_late_discovery"
#define ORIGIN          "http://localhost"  /* the allowed_origin of the BFF (main_test35) */
#define LOGIN_BODY      "{\"username\": \"testuser\", \"password\": \"testpass\"}"

/*
 *  Refresh tokens of U1 and U2: the mock makes the user of a token from the
 *  part after the last '.' (username_from_refresh_token)
 */
#define RT_OF_U1 "refresh_token=opaque-session.U1"
#define RT_OF_U2 "refresh_token=opaque-session.U2"

typedef enum {
    ACT_NONE = 0,
    ACT_NEW_BROWSER,    /* the browser closes, another one connects */
} step_action_t;

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    int             bff;            /* 1: the one BFF */
    const char     *url;
    const char     *cookie;         /* Cookie header, NULL none */
    step_action_t   action;         /* done before the step */
    int             wait_ms;        /* waited before the step, after its action */
    int             status;         /* status expected */
    const char     *must_have;      /* a text the answer must carry, NULL none */
    int             max_ms;         /* the answer comes within it; 0: not checked */
} step_t;

/***************************************************************************
 *              Data
 ***************************************************************************/
PRIVATE const step_t steps[] = {
    /* 1 */ {1, "/auth/refresh", RT_OF_U1,      ACT_NONE,        0,  200, "\"username\":\"U1\"", 0},
    /* 2 */ {1, "/auth/refresh", RT_OF_U2,      ACT_NEW_BROWSER, 50, 200, "\"username\":\"U2\"", 0},
};
#define NSTEPS ((int)(sizeof(steps)/sizeof(steps[0])))

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int post_step(hgobj gobj);
PRIVATE BOOL answer_is_whole(hgobj gobj);
PRIVATE void check_step(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type----------name-------------flag----default-description*/
SDATA (DTP_INTEGER,   "poll_ms",       SDF_RD, "50",   "Time between two looks at the socket for the answer of a step"),
SDATA (DTP_INTEGER,   "max_polls",     SDF_RD, "200",  "Looks at the socket before an answer is given up"),
SDATA (DTP_INTEGER,   "start_wait_ms", SDF_RD, "3300", "Time before the first step: the discovery of the start has timed out (2-3 s), its answer still to come (4 s)"),
SDATA (DTP_STRING,    "bff1_port",     SDF_RD, "",     "Port of bff1 on the loopback"),
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
    hgobj       timer;
    BOOL        dying;          /* TRUE once a death-timer is armed */
    BOOL        started;        /* the start wait is over */
    BOOL        waited;         /* the wait of the next step is over */
    BOOL        posted;         /* the step in flight has been posted */
    int         step;           /* index of the step in flight; -1 before the first */
    int         fd_browser[4];  /* the plain socket of each BFF (1, 2, 3) */
    int         polls;          /* looks at the socket for the answer in flight */
    uint64_t    answer_deadline;    /* msectimer: max_ms of the step in flight, from its post */
    size_t      got;            /* bytes of the answer in flight */
    char        answer[16*1024];
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
    for(int i = 0; i < 4; i++) {
        priv->fd_browser[i] = -1;
    }
    priv->step = -1;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int i = 0; i < 4; i++) {
        if(priv->fd_browser[i] >= 0) {
            close(priv->fd_browser[i]);
            priv->fd_browser[i] = -1;
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

    set_timeout0(priv->timer, gobj_read_integer_attr(gobj, "start_wait_ms"));
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
 *  The plain socket of a BFF, opened the first time
 ***************************************************************************/
PRIVATE int browser_of(hgobj gobj, int bff)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->fd_browser[bff] >= 0) {
        return priv->fd_browser[bff];
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)atoi(
        gobj_read_str_attr(gobj, "bff1_port")));

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0 || connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", TEST_NAME ": cannot connect to a BFF",
            "bff",          "%d", bff,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        if(fd >= 0) {
            close(fd);
        }
        return -1;
    }
    priv->fd_browser[bff] = fd;
    return fd;
}

/***************************************************************************
 *  The request of the current step, with what A does with it
 ***************************************************************************/
PRIVATE int post_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const step_t *st = &steps[priv->step];


    int fd = browser_of(gobj, st->bff);
    if(fd < 0) {
        return -1;  // Error already logged
    }
    priv->answer_deadline = start_msectimer(st->max_ms);

    const char *body = strcmp(st->url, "/auth/login") == 0? LOGIN_BODY : "{}";
    char cookie_line[512] = "";
    if(st->cookie) {
        snprintf(cookie_line, sizeof(cookie_line), "Cookie: %s\r\n", st->cookie);
    }
    char request[1024];
    int len = snprintf(request, sizeof(request),
        "POST %s HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Origin: " ORIGIN "\r\n"
        "%s"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        st->url,
        cookie_line,
        (int)strlen(body), body
    );
    if(send(fd, request, (size_t)len, 0) != len) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", TEST_NAME ": cannot post the request",
            "step",         "%d", priv->step + 1,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Read what has come: is the answer whole (its headers and the
 *  Content-Length of its body)
 ***************************************************************************/
PRIVATE BOOL answer_is_whole(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int fd = priv->fd_browser[steps[priv->step].bff];

    while(fd >= 0 && priv->got < sizeof(priv->answer) - 1) {
        ssize_t n = recv(fd, priv->answer + priv->got,
            sizeof(priv->answer) - 1 - priv->got, MSG_DONTWAIT);
        if(n <= 0) {
            break;
        }
        priv->got += (size_t)n;
    }
    priv->answer[priv->got] = '\0';

    const char *end_of_headers = strstr(priv->answer, "\r\n\r\n");
    const char *content_length = strcasestr(priv->answer, "Content-Length:");
    if(!end_of_headers || !content_length || content_length > end_of_headers) {
        return FALSE;
    }
    size_t body_len = (size_t)atoi(content_length + strlen("Content-Length:"));
    return end_of_headers + 4 + body_len <= priv->answer + priv->got;
}

/***************************************************************************
 *  The answer of the current step, and where its request went
 ***************************************************************************/
PRIVATE void check_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const step_t *st = &steps[priv->step];
    const char *answer = priv->answer;


    if(st->max_ms > 0 && test_msectimer(priv->answer_deadline)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", TEST_NAME ": an answer came later than the step allows",
            "step",         "%d", priv->step + 1,
            "max_ms",       "%d", st->max_ms,
            NULL
        );
    }

    char status_line[32];
    snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d ", st->status);
    if(strncmp(answer, status_line, strlen(status_line)) != 0 ||
            (st->must_have && !strstr(answer, st->must_have))) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", TEST_NAME ": an answer is not the one expected",
            "step",         "%d", priv->step + 1,
            "status",       "%d", st->status,
            "must_have",    "%s", st->must_have? st->must_have : "",
            "answer",       "%.300s", answer,
            "body",         "%.300s", strstr(answer, "\r\n\r\n")? strstr(answer, "\r\n\r\n") + 4 : "",
            NULL
        );
    }
}

/***************************************************************************
 *  The action before a step
 ***************************************************************************/
PRIVATE void do_action(hgobj gobj, step_action_t action)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(action) {
        case ACT_NEW_BROWSER:
            for(int i = 0; i < 4; i++) {
                if(priv->fd_browser[i] >= 0) {
                    close(priv->fd_browser[i]);
                    priv->fd_browser[i] = -1;
                }
            }
            break;
        case ACT_NONE:
            break;
    }
}

/***************************************************************************
 *  Timer fired: the start wait, the wait of a step (after its action),
 *  the answer of the step in flight (checked once whole, or when given
 *  up); then the next step, or, after the last, stop and die.
 ***************************************************************************/
PRIVATE int ac_timer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->dying) {
        JSON_DECREF(kw)
        set_yuno_must_die();
        return 0;
    }

    if(priv->started && priv->posted) {
        if(!answer_is_whole(gobj) &&
                ++priv->polls < (int)gobj_read_integer_attr(gobj, "max_polls")) {
            set_timeout0(priv->timer, gobj_read_integer_attr(gobj, "poll_ms"));
            JSON_DECREF(kw)
            return 0;
        }
        check_step(gobj);
        priv->posted = FALSE;
        priv->waited = FALSE;
    }
    priv->started = TRUE;

    if(!priv->posted && priv->step + 1 < NSTEPS) {
        const step_t *next = &steps[priv->step + 1];
        if(!priv->waited && (next->action != ACT_NONE || next->wait_ms > 0)) {
            priv->waited = TRUE;
            do_action(gobj, next->action);
            set_timeout0(priv->timer, next->wait_ms > 0? next->wait_ms : 100);
            JSON_DECREF(kw)
            return 0;
        }
        priv->step++;
        priv->polls = 0;
        priv->got = 0;
        priv->answer[0] = '\0';
        if(post_step(gobj) == 0) {
            priv->posted = TRUE;
            set_timeout0(priv->timer, gobj_read_integer_attr(gobj, "poll_ms"));
            JSON_DECREF(kw)
            return 0;
        }
        // Error already logged
    }

    for(int i = 0; i < 4; i++) {
        if(priv->fd_browser[i] >= 0) {
            close(priv->fd_browser[i]);
            priv->fd_browser[i] = -1;
        }
    }

    const char *sides[] = {
        "__bff1_side__", "__idp_a_side__"
    };
    for(size_t i = 0; i < sizeof(sides)/sizeof(sides[0]); i++) {
        hgobj side = gobj_find_service(sides[i], FALSE);
        if(side) {
            gobj_stop_tree(side);
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
GOBJ_DEFINE_GCLASS(C_TEST35_LATE_DISCOVERY);

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
PUBLIC int register_c_test35_late_discovery(void)
{
    return create_gclass(C_TEST35_LATE_DISCOVERY);
}
