/****************************************************************************
 *          C_TEST23_COOKIE_NO_HOST.C
 *
 *          Driver for test23_cookie_no_host. The BFF has `cookie_domain`
 *          'example.com'; the mock IdP answers every login with tokens.
 *
 *          Sequence
 *          ========
 *            mt_play
 *              → set a 500 ms timer (the listen stacks open their sockets)
 *
 *            EV_TIMEOUT (post)
 *              → from a plain socket, two POST /auth/login pipelined: the
 *                first with "Host: app.example.com", the second with no
 *                Host
 *              → set a check_delay_ms timer
 *
 *            EV_TIMEOUT (check)
 *              → the first answer: 200, its Set-Cookie lines with
 *                "Domain=example.com"
 *              → the second answer: 200, its Set-Cookie lines with no
 *                Domain
 *              → arm the death-timer (100 ms) so io_uring closes drain
 *
 *            EV_TIMEOUT (death)
 *              → set_yuno_must_die
 *
 *          The WARNING of the login with no Host is asserted by the test
 *          harness via set_expected_results in main_test23.
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

#include "test_helpers.h"
#include "c_test23_cookie_no_host.h"

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void post_login(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type----------name-------------flag----default-description*/
SDATA (DTP_INTEGER,   "check_delay_ms", SDF_RD, "1500", "Time to wait before checking the answers"),
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
    BOOL    posted;         /* TRUE once the logins are posted */
    int     fd_browser;     /* the plain socket of the logins */
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
    priv->fd_browser = -1;
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

    set_timeout(priv->timer, 500);
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
             *      Local Methods
             ***************************/




/***************************************************************************
 *  Two POST /auth/login pipelined on one plain socket: the first with a
 *  Host of the cookie_domain, the second with no Host
 ***************************************************************************/
PRIVATE void post_login(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)atoi(gobj_read_str_attr(gobj, "bff_port")));

    const char body[] = "{\"username\": \"testuser\", \"password\": \"testpass\"}";
    char request[1024];
    int len = snprintf(request, sizeof(request),
        "POST /auth/login HTTP/1.1\r\n"
        "Host: app.example.com\r\n"
        "Origin: http://localhost\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s"
        "POST /auth/login HTTP/1.1\r\n"
        "Origin: http://localhost\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        (int)strlen(body), body,
        (int)strlen(body), body
    );

    priv->fd_browser = socket(AF_INET, SOCK_STREAM, 0);
    if(priv->fd_browser < 0 ||
            connect(priv->fd_browser, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
            send(priv->fd_browser, request, (size_t)len, 0) != len) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "test23_cookie_no_host: cannot post the logins",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
    }
}

/***************************************************************************
 *  The Set-Cookie lines of one answer (up to `end`): with Domain, or not
 ***************************************************************************/
PRIVATE void check_cookies(hgobj gobj, const char *what, const char *answer,
    const char *end, BOOL with_domain)
{
    size_t len = (size_t)(end - answer);
    BOOL ok200 = len > 12 && strncmp(answer, "HTTP/1.1 200", 12) == 0;
    BOOL has_cookie = memmem(answer, len, "Set-Cookie: ", 12) != NULL;
    BOOL has_domain = memmem(answer, len, "; Domain=example.com", 20) != NULL;
    if(!ok200 || !has_cookie || has_domain != with_domain) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", what,
            "ok200",        "%d", ok200,
            "has_cookie",   "%d", has_cookie,
            "has_domain",   "%d", has_domain,
            NULL
        );
    }
}

/***************************************************************************
 *  The two answers, in order
 ***************************************************************************/
PRIVATE void check_the_answers(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char answer[16*1024];
    size_t got = 0;
    while(priv->fd_browser >= 0 && got < sizeof(answer) - 1) {
        ssize_t n = recv(priv->fd_browser, answer + got, sizeof(answer) - 1 - got, MSG_DONTWAIT);
        if(n <= 0) {
            break;
        }
        got += (size_t)n;
    }
    answer[got] = '\0';

    char *second = got > 0? strstr(answer + 1, "HTTP/1.1 ") : NULL;
    if(!second) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "test23_cookie_no_host: the two answers did not come",
            "bytes",        "%d", (int)got,
            NULL
        );
        return;
    }
    check_cookies(gobj, "test23_cookie_no_host: the login with a Host of the domain did not get Domain",
        answer, second, TRUE);
    check_cookies(gobj, "test23_cookie_no_host: the login with no Host got a Domain",
        second, answer + got, FALSE);
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  Timer fired.  Two roles, distinguished by priv->dying.
 ***************************************************************************/
PRIVATE int ac_timer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->dying) {
        JSON_DECREF(kw)
        set_yuno_must_die();
        return 0;
    }

    if(!priv->posted) {
        priv->posted = TRUE;
        post_login(gobj);
        int delay = (int)gobj_read_integer_attr(gobj, "check_delay_ms");
        set_timeout(priv->timer, delay);
        JSON_DECREF(kw)
        return 0;
    }

    check_the_answers(gobj);

    if(priv->fd_browser >= 0) {
        close(priv->fd_browser);
        priv->fd_browser = -1;
    }

    /*
     *  Drain anything still RUNNING on the IdP server side before
     *  set_yuno_must_die.  Same dance as the other tests: stop the
     *  bff/idp listen trees, then 100 ms grace, then die.
     */
    hgobj bff_side = gobj_find_service("__bff_side__", FALSE);
    if(bff_side) {
        gobj_stop_tree(bff_side);
    }
    hgobj idp_side = gobj_find_service("__idp_side__", FALSE);
    if(idp_side) {
        gobj_stop_tree(idp_side);
    }

    priv->dying = TRUE;
    set_timeout(priv->timer, 100);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Volatil child stopped: destroy it.
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
GOBJ_DEFINE_GCLASS(C_TEST23_COOKIE_NO_HOST);

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
PUBLIC int register_c_test23_cookie_no_host(void)
{
    return create_gclass(C_TEST23_COOKIE_NO_HOST);
}
