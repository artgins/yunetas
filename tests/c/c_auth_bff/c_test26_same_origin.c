/****************************************************************************
 *          C_TEST26_SAME_ORIGIN.C
 *
 *          Driver for test26_same_origin: a BFF with no allowed_origin (it
 *          serves its own origin only) and with an allowed_redirect_uri
 *          "http://localhost/app/". One plain socket, one request at a
 *          time; after each step the mock IdP's `last_form` must be empty
 *          when the BFF must not call the IdP.
 *
 *          With no allowed_origin the Origin must be "http://" or
 *          "https://" and the Host of the request:
 *            1-2. a login from "http://localhost:18890" and from
 *                 "https://localhost:18890" (Host "localhost:18890"): 200;
 *              3. a refresh from "http://LOCALHOST:18890" (a host name is
 *                 not case-sensitive): 200;
 *            4-9. a login from a sibling ("http://other.localhost:18890"),
 *                 another port, another scheme, no Origin, "null", or with
 *                 no Host: 403 origin_not_allowed, its error naming the
 *                 Origin and the Host, no call to the IdP;
 *          10-11. a refresh and a logout with a refresh_token cookie from
 *                 the sibling: 403, no call to the IdP;
 *             12. a logout with only the access_token cookie from the
 *                 sibling: 403, no Set-Cookie;
 *             13. a logout with no cookie from the sibling: 200, no
 *                 Set-Cookie (nothing to end);
 *             14. /auth/token from the BFF's own origin: 403 (the token is
 *                 given only to an allowed_origin set);
 *             15. a logout with a refresh_token cookie from the BFF's own
 *                 origin: 200, both cookies cleared.
 *          The redirect_uri of a callback, allowed_redirect_uri being
 *          "http://localhost/app/":
 *          16-17. "http://localhost/app/" and
 *                 "http://localhost/app/cb?x=1": 200;
 *          18-28. "http://localhost/app" (shorter), "http://localhost/apps/x",
 *                 "http://localhost.evil.com/app/",
 *                 "http://localhost@evil.com/app/", "http://localhost/app/../x",
 *                 "http://localhost/app/%2e%2E/x", "http://localhost/app/./x",
 *                 "http://localhost/app/..\\x" (a backslash),
 *                 "http://localhost/app/x#frag", "http://localhost/app/a b",
 *                 "http://localhost/app/é": 400 redirect_uri_not_allowed,
 *                 no call to the IdP;
 *             29. "http://localhost/app/x/...y" (not a dot-segment): 200.
 *
 *          The logs are asserted by the test harness via
 *          set_expected_results in main_test26.
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

#include "c_test26_same_origin.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TEST_NAME       "test26_same_origin"
#define OWN_HOST        "localhost:18890"           /* the Host of the requests: the BFF (main_test26) */
#define OWN_ORIGIN      "http://" OWN_HOST
#define SIBLING_ORIGIN  "http://other.localhost:18890"
#define NO_ORIGIN       "@none"                     /* a request with no Origin header */
#define NO_HOST         "@none"                     /* a request with no Host header */
#define MAX_ANSWERS     1

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    const char *url;
    const char *host;           /* NULL: OWN_HOST; NO_HOST: none */
    const char *origin;         /* NULL: OWN_ORIGIN; NO_ORIGIN: none */
    const char *cookie;         /* Cookie header, NULL none */
    const char *body;           /* NULL: the test credentials for /auth/login, "{}" for the others */
    int         status;         /* status expected */
    const char *must_have[3];   /* texts the answer must carry (NULL ends) */
    const char *must_not[2];    /* texts it must not carry (NULL ends) */
    BOOL        no_idp;         /* the IdP must not be called */
} step_t;

#define CALLBACK_BODY(ruri) \
    "{\"code\": \"C\", \"code_verifier\": \"V\", \"redirect_uri\": \"" ruri "\"}"

/***************************************************************************
 *              Data
 ***************************************************************************/
PRIVATE const step_t steps[] = {
    /* 1-3 */
    {"/auth/login", NULL, NULL, NULL, NULL, 200,
        {"Set-Cookie: access_token=ey", NULL}, {NULL}, FALSE},
    {"/auth/login", NULL, "https://" OWN_HOST, NULL, NULL, 200,
        {"Set-Cookie: access_token=ey", NULL}, {NULL}, FALSE},
    {"/auth/refresh", NULL, "http://LOCALHOST:18890", "refresh_token=RT1", NULL, 200,
        {"Set-Cookie: access_token=ey", NULL}, {NULL}, FALSE},
    /* 4-9 */
    {"/auth/login", NULL, SIBLING_ORIGIN, NULL, NULL, 403,
        {"origin_not_allowed", "Origin '" SIBLING_ORIGIN "' not allowed for /auth/login: "
            "allowed_origin is not set", "the Host ('" OWN_HOST "')"},
        {"Set-Cookie", NULL}, TRUE},
    {"/auth/login", NULL, "http://localhost:18899", NULL, NULL, 403,
        {"origin_not_allowed", NULL}, {"Set-Cookie", NULL}, TRUE},
    {"/auth/login", NULL, "ftp://" OWN_HOST, NULL, NULL, 403,
        {"origin_not_allowed", NULL}, {"Set-Cookie", NULL}, TRUE},
    {"/auth/login", NULL, NO_ORIGIN, NULL, NULL, 403,
        {"origin_not_allowed", "Origin none not allowed", NULL}, {"Set-Cookie", NULL}, TRUE},
    {"/auth/login", NULL, "null", NULL, NULL, 403,
        {"origin_not_allowed", NULL}, {"Set-Cookie", NULL}, TRUE},
    {"/auth/login", NO_HOST, NULL, NULL, NULL, 403,
        {"origin_not_allowed", "the Host (none)", NULL}, {"Set-Cookie", NULL}, TRUE},
    /* 10-13 */
    {"/auth/refresh", NULL, SIBLING_ORIGIN, "refresh_token=RT1", NULL, 403,
        {"origin_not_allowed", NULL}, {"Set-Cookie", NULL}, TRUE},
    {"/auth/logout", NULL, SIBLING_ORIGIN, "refresh_token=RT1", NULL, 403,
        {"origin_not_allowed", NULL}, {"Set-Cookie", NULL}, TRUE},
    {"/auth/logout", NULL, SIBLING_ORIGIN, "access_token=AT1", NULL, 403,
        {"origin_not_allowed", NULL}, {"Set-Cookie", NULL}, TRUE},
    {"/auth/logout", NULL, SIBLING_ORIGIN, NULL, NULL, 200,
        {"\"success\":true", NULL}, {"Set-Cookie", NULL}, TRUE},
    /* 14-15 */
    {"/auth/token", NULL, NULL, "access_token=AT1", NULL, 403,
        {"origin_not_allowed", "allowed_origin is not set", NULL}, {"AT1", NULL}, TRUE},
    {"/auth/logout", NULL, NULL, "refresh_token=RT1", NULL, 200,
        {"Set-Cookie: access_token=; Max-Age=0;", "Set-Cookie: refresh_token=; Max-Age=0;", NULL},
        {NULL}, FALSE},
    /* 16-17 */
    {"/auth/callback", NULL, NULL, NULL, CALLBACK_BODY("http://localhost/app/"), 200,
        {"Set-Cookie: access_token=ey", NULL}, {NULL}, FALSE},
    {"/auth/callback", NULL, NULL, NULL, CALLBACK_BODY("http://localhost/app/cb?x=1"), 200,
        {"Set-Cookie: access_token=ey", NULL}, {NULL}, FALSE},
    /* 18-28 */
    {"/auth/callback", NULL, NULL, NULL, CALLBACK_BODY("http://localhost/app"), 400,
        {"redirect_uri_not_allowed", NULL}, {"Set-Cookie", NULL}, TRUE},
    {"/auth/callback", NULL, NULL, NULL, CALLBACK_BODY("http://localhost/apps/x"), 400,
        {"redirect_uri_not_allowed", NULL}, {"Set-Cookie", NULL}, TRUE},
    {"/auth/callback", NULL, NULL, NULL, CALLBACK_BODY("http://localhost.evil.com/app/"), 400,
        {"redirect_uri_not_allowed", NULL}, {"Set-Cookie", NULL}, TRUE},
    {"/auth/callback", NULL, NULL, NULL, CALLBACK_BODY("http://localhost@evil.com/app/"), 400,
        {"redirect_uri_not_allowed", NULL}, {"Set-Cookie", NULL}, TRUE},
    {"/auth/callback", NULL, NULL, NULL, CALLBACK_BODY("http://localhost/app/../x"), 400,
        {"redirect_uri_not_allowed", NULL}, {"Set-Cookie", NULL}, TRUE},
    {"/auth/callback", NULL, NULL, NULL, CALLBACK_BODY("http://localhost/app/%2e%2E/x"), 400,
        {"redirect_uri_not_allowed", NULL}, {"Set-Cookie", NULL}, TRUE},
    {"/auth/callback", NULL, NULL, NULL, CALLBACK_BODY("http://localhost/app/./x"), 400,
        {"redirect_uri_not_allowed", NULL}, {"Set-Cookie", NULL}, TRUE},
    {"/auth/callback", NULL, NULL, NULL, CALLBACK_BODY("http://localhost/app/..\\\\x"), 400,
        {"redirect_uri_not_allowed", NULL}, {"Set-Cookie", NULL}, TRUE},
    {"/auth/callback", NULL, NULL, NULL, CALLBACK_BODY("http://localhost/app/x#frag"), 400,
        {"redirect_uri_not_allowed", NULL}, {"Set-Cookie", NULL}, TRUE},
    {"/auth/callback", NULL, NULL, NULL, CALLBACK_BODY("http://localhost/app/a b"), 400,
        {"redirect_uri_not_allowed", NULL}, {"Set-Cookie", NULL}, TRUE},
    {"/auth/callback", NULL, NULL, NULL, CALLBACK_BODY("http://localhost/app/\\u00e9"), 400,
        {"redirect_uri_not_allowed", NULL}, {"Set-Cookie", NULL}, TRUE},
    /* 29 */
    {"/auth/callback", NULL, NULL, NULL, CALLBACK_BODY("http://localhost/app/x/...y"), 200,
        {"Set-Cookie: access_token=ey", NULL}, {NULL}, FALSE},
};
#define NSTEPS ((int)(sizeof(steps)/sizeof(steps[0])))

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int post_step(hgobj gobj);
PRIVATE int read_answers(hgobj gobj, const char **starts, int max);
PRIVATE void check_step(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type----------name-------------flag----default-description*/
SDATA (DTP_INTEGER,   "poll_ms",       SDF_RD, "50",   "Time between two looks at the socket for the answer of a step"),
SDATA (DTP_INTEGER,   "max_polls",     SDF_RD, "100",  "Looks at the socket before an answer is given up"),
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
    int     step;           /* step whose answer is awaited; -1 before the first */
    int     fd_browser;     /* the plain socket of the requests */
    int     polls;          /* looks at the socket for the answer in flight */
    BOOL    eof;            /* the BFF closed the connection */
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
 *  The form of the mock cleared, before a step
 ***************************************************************************/
PRIVATE int clear_mock_form(hgobj gobj)
{
    hgobj mock = find_mock(gobj);
    if(!mock) {
        return -1;  // Error already logged
    }
    gobj_write_str_attr(mock, "last_form", "");
    return 0;
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
 *  The request of the current step
 ***************************************************************************/
PRIVATE int post_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const step_t *st = &steps[priv->step];
    if(clear_mock_form(gobj) < 0) {
        return -1;  // Error already logged
    }

    const char *body = st->body;
    if(!body) {
        body = strcmp(st->url, "/auth/login") == 0?
            "{\"username\": \"testuser\", \"password\": \"testpass\"}" : "{}";
    }
    char host_line[128] = "";
    const char *host = st->host? st->host : OWN_HOST;
    if(strcmp(host, NO_HOST) != 0) {
        snprintf(host_line, sizeof(host_line), "Host: %s\r\n", host);
    }
    char origin_line[128] = "";
    const char *origin = st->origin? st->origin : OWN_ORIGIN;
    if(strcmp(origin, NO_ORIGIN) != 0) {
        snprintf(origin_line, sizeof(origin_line), "Origin: %s\r\n", origin);
    }
    char cookie_line[128] = "";
    if(st->cookie) {
        snprintf(cookie_line, sizeof(cookie_line), "Cookie: %s\r\n", st->cookie);
    }

    char request[2048];
    int len = snprintf(request, sizeof(request),
        "POST %s HTTP/1.1\r\n"
        "%s"
        "%s"
        "%s"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        st->url,
        host_line,
        origin_line,
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
        if(n == 0) {
            priv->eof = TRUE;
        }
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
 *  The answer of the current step
 ***************************************************************************/
PRIVATE void check_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const step_t *st = &steps[priv->step];
    const char *answer = priv->answer;

    hgobj mock = find_mock(gobj);
    if(mock && st->no_idp && !empty_string(gobj_read_str_attr(mock, "last_form"))) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", TEST_NAME ": the IdP was called, and must not be",
            "step",         "%d", priv->step + 1,
            "form",         "%.200s", gobj_read_str_attr(mock, "last_form"),
            NULL
        );
    }

    char status_line[32];
    snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d ", st->status);
    if(strncmp(answer, status_line, strlen(status_line)) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", TEST_NAME ": an answer has not the status expected",
            "step",         "%d", priv->step + 1,
            "expected",     "%d", st->status,
            "answer",       "%.300s", answer,
            NULL
        );
        return;
    }
    for(int i = 0; i < 3 && st->must_have[i]; i++) {
        if(!strstr(answer, st->must_have[i])) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", TEST_NAME ": an answer lacks a text expected",
                "step",         "%d", priv->step + 1,
                "expected",     "%s", st->must_have[i],
                "answer",       "%.600s", answer,
                NULL
            );
        }
    }
    for(int i = 0; i < 2 && st->must_not[i]; i++) {
        if(strstr(answer, st->must_not[i])) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", TEST_NAME ": an answer carries a text it must not",
                "step",         "%d", priv->step + 1,
                "unexpected",   "%s", st->must_not[i],
                "answer",       "%.600s", answer,
                NULL
            );
        }
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
        failed = connect_browser(gobj) < 0;
    } else {
        const char *starts[MAX_ANSWERS];
        BOOL done = read_answers(gobj, starts, MAX_ANSWERS) >= MAX_ANSWERS;
        if(!done && ++priv->polls < (int)gobj_read_integer_attr(gobj, "max_polls")) {
            set_timeout0(priv->timer, gobj_read_integer_attr(gobj, "poll_ms"));
            JSON_DECREF(kw)
            return 0;
        }
        check_step(gobj);
    }

    if(!failed && priv->step < NSTEPS - 1) {
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
GOBJ_DEFINE_GCLASS(C_TEST26_SAME_ORIGIN);

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
PUBLIC int register_c_test26_same_origin(void)
{
    return create_gclass(C_TEST26_SAME_ORIGIN);
}
