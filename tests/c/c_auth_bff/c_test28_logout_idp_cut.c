/****************************************************************************
 *          C_TEST28_LOGOUT_IDP_CUT.C
 *
 *          Driver for test28_logout_idp_cut: a logout whose IdP does not
 *          answer ends in the browser all the same. One plain socket, one
 *          request at a time. The mock IdP counts the /logout requests that
 *          reached it.
 *
 *          The mock answers the discovery of the start with no endpoints
 *          (override_discovery_body, main_test28), and steps 1-4 keep it
 *          failing, each in its own way: the BFF retries the discovery for
 *          each request, and each is a WARNING (data of a peer):
 *            1. a logout, the discovery answer has no endpoints: 502
 *               auth_service_unavailable, both cookies cleared, no /logout;
 *            2. the same, the discovery answer breaks the protocol;
 *            3. the same, its endpoints are numbers;
 *            4. the same, its endpoints are not URLs;
 *            5. the same, its token_endpoint is longer than a path;
 *            6. the same, the IdP cuts the connection of each discovery: a
 *               discovery goes twice, not for ever;
 *            7. a logout, the IdP drops the connection of the first /logout
 *               (the race of a connection kept open): sent again on the next
 *               connection, 200, both cookies cleared, 2 /logout;
 *            8. a logout, the IdP drops the first two: 502
 *               auth_service_unavailable, both cookies cleared, 2 /logout,
 *               not a third;
 *            9. a logout, the IdP takes it and stays silent: 504
 *               auth_timeout, both cookies cleared, 1 /logout;
 *           10. a logout: 200, both cookies cleared, 1 /logout;
 *           11. a login, the IdP drops the connection: 502
 *               auth_service_unavailable, no Set-Cookie: a logout is the one
 *               request that goes twice;
 *           12. three requests in one write, the queue being 1 long: a
 *               login (the IdP answers it 300 ms later), a logout that waits,
 *               and a logout the queue has no room for: 200, 200 and 503
 *               server_busy, in that order, both cookies cleared in the 503.
 *
 *          The logs are asserted by the test harness via
 *          set_expected_results in main_test28.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <yunetas.h>

#include "c_test28_logout_idp_cut.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TEST_NAME       "test28_logout_idp_cut"
#define ORIGIN          "http://localhost"  /* the allowed_origin of the BFF (main_test28) */
#define AT_CLEARED      "Set-Cookie: access_token=; Max-Age=0;"
#define RT_CLEARED      "Set-Cookie: refresh_token=; Max-Age=0;"
#define MAX_PIPELINED   3
#define PIPELINED_LATENCY_MS    300
#define BROKEN_ANSWER   "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n" \
                        "Bad Header: x\r\nContent-Length: 2\r\n\r\n{}"

#define DROP_DISCOVERIES "@drop"        /* raw_discovery: the IdP drops the connection of two discoveries */
#define LONG_ENDPOINT   "@long"         /* discovery_body: a token_endpoint of PATH_MAX bytes */

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    const char *url;
    const char *cookie;         /* Cookie header, NULL none */
    int         drop_logouts;   /* /logout requests the mock drops */
    BOOL        silent_logout;  /* the mock takes /logout and stays silent */
    BOOL        drop_token;     /* the mock drops the connection of the next /token */
    int         status;         /* status expected */
    const char *must_have[3];   /* texts the answer must carry (NULL ends) */
    const char *must_not;       /* a text it must not carry, or NULL */
    int         logouts;        /* /logout requests that must reach the mock */
    const char *discovery_body; /* json of the discovery answer, NULL the good one */
    const char *raw_discovery;  /* the discovery answer as it is, NULL none */
} step_t;

/***************************************************************************
 *              Data
 ***************************************************************************/
PRIVATE const step_t steps[] = {
    /* 1 */
    {"/auth/logout", "refresh_token=RT1", 0, FALSE, FALSE, 502,
        {"auth_service_unavailable", AT_CLEARED, RT_CLEARED}, NULL, 0,
        "{\"issuer\": \"no endpoints\"}", NULL},
    /* 2 */
    {"/auth/logout", "refresh_token=RT1", 0, FALSE, FALSE, 502,
        {"auth_service_unavailable", AT_CLEARED, RT_CLEARED}, NULL, 0,
        NULL, BROKEN_ANSWER},
    /* 3 */
    {"/auth/logout", "refresh_token=RT1", 0, FALSE, FALSE, 502,
        {"auth_service_unavailable", AT_CLEARED, RT_CLEARED}, NULL, 0,
        "{\"token_endpoint\": 5, \"end_session_endpoint\": 6}", NULL},
    /* 4 */
    {"/auth/logout", "refresh_token=RT1", 0, FALSE, FALSE, 502,
        {"auth_service_unavailable", AT_CLEARED, RT_CLEARED}, NULL, 0,
        "{\"token_endpoint\": \"no-scheme\", \"end_session_endpoint\": \"no-scheme\"}", NULL},
    /* 5 */
    {"/auth/logout", "refresh_token=RT1", 0, FALSE, FALSE, 502,
        {"auth_service_unavailable", AT_CLEARED, RT_CLEARED}, NULL, 0,
        LONG_ENDPOINT, NULL},
    /* 6 */
    {"/auth/logout", "refresh_token=RT1", 0, FALSE, FALSE, 502,
        {"auth_service_unavailable", AT_CLEARED, RT_CLEARED}, NULL, 0,
        NULL, DROP_DISCOVERIES},
    /* 7 */
    {"/auth/logout", "refresh_token=RT1", 1, FALSE, FALSE, 200,
        {"\"success\":true", AT_CLEARED, RT_CLEARED}, NULL, 2, NULL, NULL},
    /* 8 */
    {"/auth/logout", "refresh_token=RT1", 2, FALSE, FALSE, 502,
        {"auth_service_unavailable", AT_CLEARED, RT_CLEARED}, NULL, 2, NULL, NULL},
    /* 9 */
    {"/auth/logout", "refresh_token=RT1", 0, TRUE, FALSE, 504,
        {"auth_timeout", AT_CLEARED, RT_CLEARED}, NULL, 1, NULL, NULL},
    /* 10 */
    {"/auth/logout", "refresh_token=RT1", 0, FALSE, FALSE, 200,
        {"\"success\":true", AT_CLEARED, RT_CLEARED}, NULL, 1, NULL, NULL},
    /* 11 */
    {"/auth/login", NULL, 0, FALSE, TRUE, 502,
        {"auth_service_unavailable", "closed the connection before answering", NULL},
        "Set-Cookie", 0, NULL, NULL},
};
#define NSTEPS ((int)(sizeof(steps)/sizeof(steps[0])))

/*
 *  Step NSTEPS: three requests in one write, their statuses in order
 */
#define LOGIN_BODY  "{\"username\": \"testuser\", \"password\": \"testpass\"}"
#define LOGOUT_REQUEST \
    "POST /auth/logout HTTP/1.1\r\n" \
    "Host: localhost\r\n" \
    "Origin: " ORIGIN "\r\n" \
    "Cookie: refresh_token=RT1\r\n" \
    "Content-Length: 0\r\n" \
    "\r\n"
PRIVATE const char *pipelined_requests =
    "POST /auth/login HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "Origin: " ORIGIN "\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 48\r\n"
    "\r\n"
    LOGIN_BODY
    LOGOUT_REQUEST
    LOGOUT_REQUEST;
PRIVATE const int pipelined_statuses[MAX_PIPELINED] = {200, 200, 503};

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int post_step(hgobj gobj);
PRIVATE int read_answers(hgobj gobj, const char **starts, int max);
PRIVATE void check_step(hgobj gobj);
PRIVATE void check_pipelined(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type----------name-------------flag----default-description*/
SDATA (DTP_INTEGER,   "poll_ms",       SDF_RD, "50",   "Time between two looks at the socket for the answer of a step"),
SDATA (DTP_INTEGER,   "max_polls",     SDF_RD, "200",  "Looks at the socket before an answer is given up"),
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
    hgobj   timer;
    BOOL    dying;          /* TRUE once a death-timer is armed */
    BOOL    discovery_settled;  /* the discovery of the start has had time to fail */
    int     step;           /* step whose answer is awaited; -1 before the first */
    int     fd_browser;     /* the plain socket of the requests */
    int     polls;          /* looks at the socket for the answer in flight */
    size_t  got;            /* bytes of the answer in flight */
    char    answer[16*1024];
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
 *  The mock IdP of __idp_side__
 ***************************************************************************/
PRIVATE hgobj find_mock(hgobj gobj)
{
    hgobj idp_side = gobj_find_service("__idp_side__", FALSE);
    if(!idp_side) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", TEST_NAME ": __idp_side__ service not found",
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
    } else {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", TEST_NAME ": no C_MOCK_KEYCLOAK found under __idp_side__",
            NULL
        );
    }
    gobj_free_iter(dl);
    return mock;
}

/***************************************************************************
 *  Open the plain socket of the requests
 ***************************************************************************/
PRIVATE int connect_browser(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)atoi(gobj_read_str_attr(gobj, "bff_port")));

    priv->fd_browser = socket(AF_INET, SOCK_STREAM, 0);
    if(priv->fd_browser < 0 ||
            connect(priv->fd_browser, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", TEST_NAME ": cannot connect to the BFF",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Send all of a request
 ***************************************************************************/
PRIVATE int send_request(hgobj gobj, const char *request, size_t len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(send(priv->fd_browser, request, len, 0) != (ssize_t)len) {
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
 *  The request of the current step, with what the mock does with it
 ***************************************************************************/
PRIVATE int post_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj mock = find_mock(gobj);
    if(!mock) {
        return -1;  // Error already logged
    }
    gobj_write_integer_attr(mock, "logouts_seen", 0);

    if(priv->step == NSTEPS) {
        gobj_write_integer_attr(mock, "drop_logout_requests", 0);
        gobj_write_bool_attr(mock, "silent_logout", FALSE);
        gobj_write_integer_attr(mock, "latency_ms", PIPELINED_LATENCY_MS);
        return send_request(gobj, pipelined_requests, strlen(pipelined_requests));
    }

    const step_t *st = &steps[priv->step];
    const char *discovery_body = st->discovery_body? st->discovery_body : "{}";
    json_t *jn_discovery;
    if(strcmp(discovery_body, LONG_ENDPOINT) == 0) {
        char endpoint[PATH_MAX + 1];
        memset(endpoint, 'a', PATH_MAX);
        memcpy(endpoint, "http://", 7);
        endpoint[PATH_MAX] = '\0';
        jn_discovery = json_pack("{s:s, s:s}",
            "token_endpoint",       endpoint,
            "end_session_endpoint", "http://localhost/"
        );
    } else {
        jn_discovery = anystring2json(discovery_body, strlen(discovery_body), FALSE);
    }
    gobj_write_json_attr(mock, "override_discovery_body", jn_discovery);
    JSON_DECREF(jn_discovery)
    BOOL drop_discoveries = st->raw_discovery && strcmp(st->raw_discovery, DROP_DISCOVERIES) == 0;
    gobj_write_integer_attr(mock, "drop_discovery_requests", drop_discoveries? 2 : 0);
    gobj_write_str_attr(mock, "raw_discovery",
        st->raw_discovery && !drop_discoveries? st->raw_discovery : "");
    gobj_write_integer_attr(mock, "drop_logout_requests", st->drop_logouts);
    gobj_write_bool_attr(mock, "silent_logout", st->silent_logout);
    gobj_write_bool_attr(mock, "drop_token_request", st->drop_token);

    const char *body = strcmp(st->url, "/auth/login") == 0? LOGIN_BODY : "{}";
    char cookie_line[128] = "";
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
    return send_request(gobj, request, (size_t)len);
}

/***************************************************************************
 *  Read what has come, and count the answers that are whole (their
 *  headers and the Content-Length of their body): their starts in
 *  `starts`, at most `max`.
 ***************************************************************************/
PRIVATE int read_answers(hgobj gobj, const char **starts, int max)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    while(priv->got < sizeof(priv->answer) - 1) {
        ssize_t n = recv(priv->fd_browser, priv->answer + priv->got,
            sizeof(priv->answer) - 1 - priv->got, MSG_DONTWAIT);
        if(n <= 0) {
            break;
        }
        priv->got += (size_t)n;
    }
    priv->answer[priv->got] = '\0';

    int whole = 0;
    const char *p = priv->answer;
    while(whole < max && *p) {
        const char *end_of_headers = strstr(p, "\r\n\r\n");
        const char *content_length = strcasestr(p, "Content-Length:");
        if(!end_of_headers || !content_length || content_length > end_of_headers) {
            break;
        }
        size_t body_len = (size_t)atoi(content_length + strlen("Content-Length:"));
        const char *next = end_of_headers + 4 + body_len;
        if(next > priv->answer + priv->got) {
            break;
        }
        starts[whole++] = p;
        p = next;
    }
    return whole;
}

/***************************************************************************
 *  An error of a step, with the answer
 ***************************************************************************/
PRIVATE void step_failed(hgobj gobj, const char *what, const char *text, const char *answer)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_APP,
        "msg",          "%s", what,
        "step",         "%d", priv->step + 1,
        "text",         "%s", text,
        "answer",       "%.600s", answer,
        NULL
    );
}

/***************************************************************************
 *  The answer of the current step
 ***************************************************************************/
PRIVATE void check_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const step_t *st = &steps[priv->step];
    const char *answer = priv->answer;

    hgobj mock = find_mock(gobj);
    int logouts = mock? (int)gobj_read_integer_attr(mock, "logouts_seen") : -1;
    if(logouts != st->logouts) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", TEST_NAME ": the /logout requests that reached the IdP",
            "step",         "%d", priv->step + 1,
            "expected",     "%d", st->logouts,
            "seen",         "%d", logouts,
            NULL
        );
    }

    char status_line[32];
    snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d ", st->status);
    if(strncmp(answer, status_line, strlen(status_line)) != 0) {
        step_failed(gobj, TEST_NAME ": an answer has not the status expected", status_line, answer);
        return;
    }
    for(int i = 0; i < 3 && st->must_have[i]; i++) {
        if(!strstr(answer, st->must_have[i])) {
            step_failed(gobj, TEST_NAME ": an answer lacks a text expected", st->must_have[i], answer);
        }
    }
    if(st->must_not && strstr(answer, st->must_not)) {
        step_failed(gobj, TEST_NAME ": an answer carries a text it must not", st->must_not, answer);
    }
}

/***************************************************************************
 *  The answers of the pipelined requests, in their order; the third, the
 *  one the queue had no room for, with the cookies cleared
 ***************************************************************************/
PRIVATE void check_pipelined(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *starts[MAX_PIPELINED];
    int whole = read_answers(gobj, starts, MAX_PIPELINED);
    if(whole != MAX_PIPELINED) {
        step_failed(gobj, TEST_NAME ": the pipelined requests lack answers", "", priv->answer);
        return;
    }
    for(int i = 0; i < MAX_PIPELINED; i++) {
        char status_line[32];
        snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d ", pipelined_statuses[i]);
        if(strncmp(starts[i], status_line, strlen(status_line)) != 0) {
            step_failed(gobj, TEST_NAME ": a pipelined answer has not its status",
                status_line, starts[i]);
        }
    }
    if(!strstr(starts[2], "server_busy") || !strstr(starts[2], AT_CLEARED) ||
            !strstr(starts[2], RT_CLEARED)) {
        step_failed(gobj, TEST_NAME ": a logout refused by a full queue keeps the session",
            "", starts[2]);
    }
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  Timer fired: look for the answer of the step in flight (none before the
 *  first) and check it once whole, or when it is given up; then post the
 *  next one, or, after the last, stop and die.
 ***************************************************************************/
PRIVATE int ac_timer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->dying) {
        JSON_DECREF(kw)
        set_yuno_must_die();
        return 0;
    }

    BOOL failed = FALSE;
    if(priv->step < 0) {
        /*
         *  Only once the discovery of the start has failed: step 1 is about
         *  the one it causes itself
         */
        hgobj mock = find_mock(gobj);
        if(mock && gobj_read_integer_attr(mock, "discoveries_seen") == 0 &&
                ++priv->polls < (int)gobj_read_integer_attr(gobj, "max_polls")) {
            set_timeout0(priv->timer, gobj_read_integer_attr(gobj, "poll_ms"));
            JSON_DECREF(kw)
            return 0;
        }
        if(!priv->discovery_settled) {
            priv->discovery_settled = TRUE;
            set_timeout0(priv->timer, 300);
            JSON_DECREF(kw)
            return 0;
        }
        failed = connect_browser(gobj) < 0;
    } else {
        int expected = priv->step == NSTEPS? MAX_PIPELINED : 1;
        const char *starts[MAX_PIPELINED];
        BOOL done = read_answers(gobj, starts, expected) >= expected;
        if(!done && ++priv->polls < (int)gobj_read_integer_attr(gobj, "max_polls")) {
            set_timeout0(priv->timer, gobj_read_integer_attr(gobj, "poll_ms"));
            JSON_DECREF(kw)
            return 0;
        }
        if(priv->step == NSTEPS) {
            check_pipelined(gobj);
        } else {
            check_step(gobj);
        }
    }

    if(!failed && priv->step < NSTEPS) {
        priv->step++;
        priv->polls = 0;
        priv->got = 0;
        priv->answer[0] = '\0';
        if(post_step(gobj) == 0) {
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

    hgobj bff_side = gobj_find_service("__bff_side__", FALSE);
    if(bff_side) {
        gobj_stop_tree(bff_side);
    }
    hgobj idp_side = gobj_find_service("__idp_side__", FALSE);
    if(idp_side) {
        gobj_stop_tree(idp_side);
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
GOBJ_DEFINE_GCLASS(C_TEST28_LOGOUT_IDP_CUT);

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
PUBLIC int register_c_test28_logout_idp_cut(void)
{
    return create_gclass(C_TEST28_LOGOUT_IDP_CUT);
}
