/****************************************************************************
 *          C_TEST24_NO_REFRESH_TOKEN.C
 *
 *          Driver for test24_no_refresh_token. The mock IdP answers every
 *          token request with its envelope, changed by its `body_patch`
 *          before each step. Eight requests on one plain socket, one at a
 *          time (each answer is read whole before the next request):
 *
 *            1. login, the answer with no refresh_token: 200, the
 *               access_token cookie, the refresh_token cookie CLEARED
 *               ("refresh_token=; Max-Age=0"), refresh_expires_in 0; a
 *               WARNING that the session cannot be refreshed;
 *            2. the same again: the same answer, and no second WARNING;
 *            3. refresh with "Cookie: refresh_token=OLDRT123", the answer
 *               with no refresh_token: 200, the access_token cookie, and
 *               NO Set-Cookie for the refresh_token (the browser keeps
 *               its own);
 *            4. refresh, a full answer: 200, both cookies;
 *            5. login, an access_token that is an integer: 502
 *               auth_unexpected_error, its error saying the type;
 *            6. login, a refresh_token that is an object: the same;
 *            7. login, expires_in "450" (a number as text): 200, the
 *               access_token cookie with Max-Age=450;
 *            8. login, expires_in "soon": 200, Max-Age=300 (the default),
 *               a WARNING.
 *
 *          The logs are asserted by the test harness via
 *          set_expected_results in main_test24.
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

#include "c_test24_no_refresh_token.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TEST_NAME   "test24_no_refresh_token"

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    const char *url;            /* /auth/login or /auth/refresh */
    const char *body_patch;     /* json of the mock's body_patch for this step */
    int         status;         /* status expected */
    const char *must_have[4];   /* texts the answer must carry (NULL ends) */
    const char *must_not[2];    /* texts it must not carry (NULL ends) */
} step_t;

/***************************************************************************
 *              Data
 ***************************************************************************/
PRIVATE const step_t steps[] = {
    {"/auth/login", "{\"refresh_token\": null}", 200,
        {"Set-Cookie: access_token=ey", "Set-Cookie: refresh_token=; Max-Age=0;",
            "\"refresh_expires_in\":0", NULL},
        {"Set-Cookie: refresh_token=ey", NULL}},
    {"/auth/login", "{\"refresh_token\": null}", 200,
        {"Set-Cookie: access_token=ey", "Set-Cookie: refresh_token=; Max-Age=0;",
            "\"refresh_expires_in\":0", NULL},
        {"Set-Cookie: refresh_token=ey", NULL}},
    {"/auth/refresh", "{\"refresh_token\": null}", 200,
        {"Set-Cookie: access_token=ey", NULL},
        {"Set-Cookie: refresh_token", NULL}},
    {"/auth/refresh", "{}", 200,
        {"Set-Cookie: access_token=ey", "Set-Cookie: refresh_token=ey",
            "\"refresh_expires_in\":1800", NULL},
        {NULL}},
    {"/auth/login", "{\"access_token\": 123}", 502,
        {"auth_unexpected_error", "access_token that is not a string", NULL},
        {"Set-Cookie", NULL}},
    {"/auth/login", "{\"refresh_token\": {}}", 502,
        {"auth_unexpected_error", "refresh_token that is not a string", NULL},
        {"Set-Cookie", NULL}},
    {"/auth/login", "{\"expires_in\": \"450\"}", 200,
        {"Set-Cookie: access_token=ey", "; Max-Age=450;", NULL},
        {NULL}},
    {"/auth/login", "{\"expires_in\": \"soon\"}", 200,
        {"Set-Cookie: access_token=ey", "; Max-Age=300;", NULL},
        {NULL}},
};
#define NSTEPS ((int)(sizeof(steps)/sizeof(steps[0])))

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int post_step(hgobj gobj);
PRIVATE BOOL read_answer(hgobj gobj);
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
 *  The mock IdP of __idp_side__: its next answers get `body_patch` (json
 *  text) over its envelope
 ***************************************************************************/
PRIVATE int set_mock_patch(hgobj gobj, const char *body_patch)
{
    hgobj idp_side = gobj_find_service("__idp_side__", FALSE);
    if(!idp_side) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", TEST_NAME ": __idp_side__ service not found",
            NULL
        );
        return -1;
    }
    json_t *jn_filter = json_pack("{s:s}",
        "__gclass_name__", "C_MOCK_KEYCLOAK"
    );
    json_t *dl = gobj_match_children_tree(idp_side, jn_filter);
    int ret = 0;
    if(dl && json_array_size(dl) > 0) {
        hgobj mock = (hgobj)(size_t)json_integer_value(json_array_get(dl, 0));
        json_t *jn_patch = anystring2json(body_patch, strlen(body_patch), FALSE);
        gobj_write_json_attr(mock, "body_patch", jn_patch);
        JSON_DECREF(jn_patch)
    } else {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", TEST_NAME ": no C_MOCK_KEYCLOAK found under __idp_side__",
            NULL
        );
        ret = -1;
    }
    gobj_free_iter(dl);
    return ret;
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
 *  The request of the current step, with the mock answer set for it
 ***************************************************************************/
PRIVATE int post_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const step_t *st = &steps[priv->step];

    if(set_mock_patch(gobj, st->body_patch) < 0) {
        return -1;  // Error already logged
    }

    BOOL refresh = strcmp(st->url, "/auth/refresh") == 0;
    const char *body = refresh?
        "{}" : "{\"username\": \"testuser\", \"password\": \"testpass\"}";
    char request[1024];
    int len = snprintf(request, sizeof(request),
        "POST %s HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Origin: http://localhost\r\n"
        "%s"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        st->url,
        refresh? "Cookie: refresh_token=OLDRT123\r\n" : "",
        (int)strlen(body), body
    );

    if(send(priv->fd_browser, request, (size_t)len, 0) != len) {
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
 *  What has come of the answer of the current step: TRUE once it is whole
 *  (its headers and the Content-Length of its body)
 ***************************************************************************/
PRIVATE BOOL read_answer(hgobj gobj)
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

    const char *end_of_headers = strstr(priv->answer, "\r\n\r\n");
    const char *content_length = strcasestr(priv->answer, "Content-Length:");
    if(!end_of_headers || !content_length || content_length > end_of_headers) {
        return FALSE;
    }
    size_t body_len = (size_t)atoi(content_length + strlen("Content-Length:"));
    return priv->got >= (size_t)(end_of_headers + 4 - priv->answer) + body_len;
}

/***************************************************************************
 *  The answer of the current step
 ***************************************************************************/
PRIVATE void check_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const step_t *st = &steps[priv->step];
    const char *answer = priv->answer;

    char status_line[32];
    snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d ", st->status);
    if(strncmp(answer, status_line, strlen(status_line)) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", TEST_NAME ": an answer has not the status expected",
            "step",         "%d", priv->step + 1,
            "expected",     "%d", st->status,
            "answer",       "%.200s", answer,
            NULL
        );
        return;
    }
    for(int i = 0; st->must_have[i]; i++) {
        if(!strstr(answer, st->must_have[i])) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", TEST_NAME ": an answer lacks a text expected",
                "step",         "%d", priv->step + 1,
                "expected",     "%s", st->must_have[i],
                "answer",       "%s", answer,
                NULL
            );
        }
    }
    for(int i = 0; st->must_not[i]; i++) {
        if(strstr(answer, st->must_not[i])) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", TEST_NAME ": an answer carries a text it must not",
                "step",         "%d", priv->step + 1,
                "unexpected",   "%s", st->must_not[i],
                "answer",       "%s", answer,
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
    } else if(!read_answer(gobj) &&
            ++priv->polls < (int)gobj_read_integer_attr(gobj, "max_polls")) {
        set_timeout0(priv->timer, gobj_read_integer_attr(gobj, "poll_ms"));
        JSON_DECREF(kw)
        return 0;
    } else {
        check_step(gobj);
    }

    if(!failed && priv->step + 1 < NSTEPS) {
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
GOBJ_DEFINE_GCLASS(C_TEST24_NO_REFRESH_TOKEN);

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
PUBLIC int register_c_test24_no_refresh_token(void)
{
    return create_gclass(C_TEST24_NO_REFRESH_TOKEN);
}
