/****************************************************************************
 *          C_TEST34_IDP_SILENT.C
 *
 *          Driver for test34_idp_silent: a login whose IdP dies with the
 *          connection to it open (main_test34). One plain socket, one
 *          request at a time. Each step says what the IdPs A and B must
 *          have received (their /token requests), the logins the BFF
 *          passed to the next IdP (its `idp_failovers`), and how long the
 *          answer may take.
 *
 *          bff, issuers [A, B], idp_timeout_ms 3000,
 *          idp_failover_response_ms 500, idp_down_retry_ms 6000:
 *             1. a login: A takes it, and the connection to A stays open;
 *             2. A goes silent (it answers nothing, the connection stays
 *                open: a host that died): the login passes to B after
 *                idp_failover_response_ms, 200 well before
 *                idp_timeout_ms. Up to 7.31.13 it waited idp_timeout_ms
 *                and answered 504: the SPA had given up at 15 s;
 *             3. a login: A is down, straight to B;
 *             4. a refresh of a session of A while A is silent: to A all
 *                the same, and it waits idp_timeout_ms (the short time is
 *                only for a login another IdP can take): 504;
 *             5. A is down and B is slow (1 s): B is waited for, not left
 *                for a down IdP -- when one IdP dies every login goes to
 *                the other, which is when it is slow;
 *             6. A answers again, and idp_down_retry_ms has passed: the
 *                login goes to A again.
 *
 *          The logs are asserted by the test harness via
 *          set_expected_results in main_test34, with their priority.
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

#include "c_test34_idp_silent.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TEST_NAME       "test34_idp_silent"
#define ORIGIN          "http://localhost"  /* the allowed_origin of the BFF (main_test34) */
#define LOGIN_BODY      "{\"username\": \"testuser\", \"password\": \"testpass\"}"
#define SILENT          60000               /* a latency_ms no step waits out */

/*
 *  A refresh token of a session of A: a JWT whose payload says
 *  {"iss": "<A>", "typ": "Refresh"}. The BFF reads the `iss` and does not
 *  check the signature; the mock does not either.
 */
#define RT_OF_A "refresh_token=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9." \
    "eyJpc3MiOiJodHRwOi8vMTI3LjAuMC4xOjE4OTUyL3JlYWxtcy90ZXN0IiwidHlwIjoiUmVmcmVzaCJ9.c2lnbmF0dXJl"

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    const char     *url;
    const char     *cookie;         /* Cookie header, NULL none */
    int             a_latency_ms;   /* how long A takes to answer */
    int             b_latency_ms;   /* how long B takes to answer */
    int             wait_ms;        /* waited before the step */
    int             status;         /* status expected */
    const char     *must_have;      /* a text the answer must carry, NULL none */
    int             a_tokens;       /* /token requests the step adds at A */
    int             b_tokens;       /* /token requests the step adds at B */
    int             failovers;      /* idp_failovers the step adds at the BFF */
    int             min_ms;         /* the answer comes no sooner */
    int             max_ms;         /* nor later */
} step_t;

/***************************************************************************
 *              Data
 ***************************************************************************/
PRIVATE const step_t steps[] = {
    /* 1 */ {"/auth/login",   NULL,    0,      0,    0,    200, "\"success\":true",        1, 0, 0, 0,    1000},
    /* 2 */ {"/auth/login",   NULL,    SILENT, 0,    0,    200, "\"success\":true",        1, 1, 1, 450,  2500},
    /* 3 */ {"/auth/login",   NULL,    SILENT, 0,    0,    200, "\"success\":true",        0, 1, 0, 0,    1000},
    /* 4 */ {"/auth/refresh", RT_OF_A, SILENT, 0,    0,    504, "auth_timeout",             1, 0, 0, 2900, 4500},
    /* 5 */ {"/auth/login",   NULL,    SILENT, 1000, 0,    200, "\"success\":true",        0, 1, 0, 950,  2500},
    /* 6 */ {"/auth/login",   NULL,    0,      0,    3000, 200, "\"success\":true",        1, 0, 0, 0,    1000},
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
SDATA (DTP_INTEGER,   "start_wait_ms", SDF_RD, "500",  "Time before the first step: the discovery of A at the start of the BFF"),
SDATA (DTP_STRING,    "bff_port",      SDF_RD, "",     "Port of the BFF on the loopback"),
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
    int         fd_browser;     /* the plain socket to the BFF */
    int         polls;          /* looks at the socket for the answer in flight */
    json_int_t  a_tokens;       /* the /token requests of A when the step was posted */
    json_int_t  b_tokens;       /* the /token requests of B when the step was posted */
    json_int_t  failovers;      /* the idp_failovers of the BFF when the step was posted */
    uint64_t    posted_ms;      /* when the step was posted (monotonic) */
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
    priv->fd_browser = -1;
    priv->step = -1;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->fd_browser >= 0) {
        close(priv->fd_browser);
        priv->fd_browser = -1;
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
 *  The gobjs of `gclass_name` under the service `service_name`: an iter
 *  the caller frees with gobj_free_iter()
 ***************************************************************************/
PRIVATE json_t *find_children(hgobj gobj, const char *service_name, const char *gclass_name)
{
    hgobj service = gobj_find_service(service_name, FALSE);
    if(!service) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", TEST_NAME ": service not found",
            "service",  "%s", service_name,
            NULL
        );
        return NULL;
    }
    json_t *jn_filter = json_pack("{s:s}",
        "__gclass_name__", gclass_name
    );
    return gobj_match_children_tree(service, jn_filter);
}

/***************************************************************************
 *  The /token requests the mocks of an IdP side have received, all of them
 ***************************************************************************/
PRIVATE json_int_t read_token_requests(hgobj gobj, const char *idp_side)
{
    json_int_t total = 0;
    json_t *dl = find_children(gobj, idp_side, "C_MOCK_KEYCLOAK");
    size_t idx;
    json_t *jn_mock;
    json_array_foreach(dl, idx, jn_mock) {
        hgobj mock = (hgobj)(size_t)json_integer_value(jn_mock);
        json_t *resp = gobj_stats(mock, "", 0, gobj);
        total += json_integer_value(
            json_object_get(json_object_get(resp, "data"), "token_requests"));
        JSON_DECREF(resp)
    }
    gobj_free_iter(dl);
    return total;
}

/***************************************************************************
 *  How long every mock of an IdP side takes to answer
 ***************************************************************************/
PRIVATE void set_latency(hgobj gobj, const char *idp_side, int latency_ms)
{
    json_t *dl = find_children(gobj, idp_side, "C_MOCK_KEYCLOAK");
    size_t idx;
    json_t *jn_mock;
    json_array_foreach(dl, idx, jn_mock) {
        hgobj mock = (hgobj)(size_t)json_integer_value(jn_mock);
        gobj_write_integer_attr(mock, "latency_ms", latency_ms);
    }
    gobj_free_iter(dl);
}

/***************************************************************************
 *  The idp_failovers of the BFF (its stats)
 ***************************************************************************/
PRIVATE json_int_t read_failovers(hgobj gobj)
{
    json_int_t failovers = -1;
    json_t *dl = find_children(gobj, "__bff_side__", "C_AUTH_BFF");
    if(json_array_size(dl) > 0) {
        hgobj bff_gobj = (hgobj)(size_t)json_integer_value(json_array_get(dl, 0));
        json_t *resp = gobj_stats(bff_gobj, "idp_failovers", 0, gobj);
        failovers = json_integer_value(
            json_object_get(json_object_get(resp, "data"), "idp_failovers"));
        JSON_DECREF(resp)
    }
    gobj_free_iter(dl);
    return failovers;
}

/***************************************************************************
 *  The plain socket to the BFF, opened the first time
 ***************************************************************************/
PRIVATE int browser(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->fd_browser >= 0) {
        return priv->fd_browser;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)atoi(gobj_read_str_attr(gobj, "bff_port")));

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0 || connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", TEST_NAME ": cannot connect to the BFF",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        if(fd >= 0) {
            close(fd);
        }
        return -1;
    }
    priv->fd_browser = fd;
    return fd;
}

/***************************************************************************
 *  The request of the current step, with how long A and B take
 ***************************************************************************/
PRIVATE int post_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const step_t *st = &steps[priv->step];

    set_latency(gobj, "__idp_a_side__", st->a_latency_ms);
    set_latency(gobj, "__idp_b_side__", st->b_latency_ms);
    priv->a_tokens = read_token_requests(gobj, "__idp_a_side__");
    priv->b_tokens = read_token_requests(gobj, "__idp_b_side__");
    priv->failovers = read_failovers(gobj);

    int fd = browser(gobj);
    if(fd < 0) {
        return -1;  // Error already logged
    }

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
    priv->posted_ms = time_in_milliseconds_monotonic();
    return 0;
}

/***************************************************************************
 *  Read what has come: is the answer whole (its headers and the
 *  Content-Length of its body)
 ***************************************************************************/
PRIVATE BOOL answer_is_whole(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int fd = priv->fd_browser;

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
 *  A count of the step that is not the one expected
 ***************************************************************************/
PRIVATE void check_count(hgobj gobj, const char *what, json_int_t expected, json_int_t added)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(expected != added) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", TEST_NAME ": a count of the step is not the one expected",
            "step",         "%d", priv->step + 1,
            "count",        "%s", what,
            "expected",     "%d", (int)expected,
            "added",        "%d", (int)added,
            NULL
        );
    }
}

/***************************************************************************
 *  The answer of the current step, and where its request went
 ***************************************************************************/
PRIVATE void check_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const step_t *st = &steps[priv->step];
    const char *answer = priv->answer;

    check_count(gobj, "/token requests at A", st->a_tokens,
        read_token_requests(gobj, "__idp_a_side__") - priv->a_tokens);
    check_count(gobj, "/token requests at B", st->b_tokens,
        read_token_requests(gobj, "__idp_b_side__") - priv->b_tokens);
    check_count(gobj, "idp_failovers of the BFF", st->failovers,
        read_failovers(gobj) - priv->failovers);

    json_int_t took_ms = (json_int_t)(time_in_milliseconds_monotonic() - priv->posted_ms);
    if(took_ms < st->min_ms || took_ms > st->max_ms) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", TEST_NAME ": an answer did not come in its time",
            "step",         "%d", priv->step + 1,
            "min_ms",       "%d", st->min_ms,
            "max_ms",       "%d", st->max_ms,
            "took_ms",      "%d", (int)took_ms,
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
            "answer",       "%.600s", answer,
            NULL
        );
    }
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  Timer fired: the start wait, the wait of a step,
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
        if(!priv->waited && next->wait_ms > 0) {
            priv->waited = TRUE;
            set_timeout0(priv->timer, next->wait_ms);
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

    if(priv->fd_browser >= 0) {
        close(priv->fd_browser);
        priv->fd_browser = -1;
    }

    const char *sides[] = {"__bff_side__", "__idp_a_side__", "__idp_b_side__"};
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
GOBJ_DEFINE_GCLASS(C_TEST34_IDP_SILENT);

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
PUBLIC int register_c_test34_idp_silent(void)
{
    return create_gclass(C_TEST34_IDP_SILENT);
}
