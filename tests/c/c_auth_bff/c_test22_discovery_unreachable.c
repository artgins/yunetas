/****************************************************************************
 *          C_TEST22_DISCOVERY_UNREACHABLE.C
 *
 *          Driver for test22_discovery_unreachable. The BFF's OIDC
 *          discovery connects to an IdP that cannot be reached (a listener
 *          of main_test22 whose accept queue is full: the kernel drops the
 *          SYN), and its connect watchdog (`idp_connect_timeout_ms` = 500)
 *          ends the task with result -2.
 *
 *          Sequence
 *          ========
 *            mt_play
 *              → POST /auth/login to the BFF from a plain socket: it is
 *                queued behind the discovery, which has no endpoints yet
 *              → set a check_delay_ms timer
 *
 *            EV_TIMEOUT (initial)
 *              → the login must have been answered 502
 *                auth_service_unavailable (the queue drained)
 *              → cross-check gobj_stats: idp_timeouts == 1
 *              → arm the death-timer (100 ms) so io_uring closes drain
 *
 *            EV_TIMEOUT (death)
 *              → set_yuno_must_die
 *
 *          The ERRORs are asserted by the test harness via
 *          set_expected_results in main_test22: ONE for the failure of the
 *          discovery, and the one of the 502.
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
#include "c_test22_discovery_unreachable.h"

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void post_login(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type----------name-------------flag----default-description*/
SDATA (DTP_INTEGER,   "check_delay_ms", SDF_RD, "3000", "Time to wait before checking the answer and the BFF stats"),
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
    int     fd_browser;     /* the plain socket of the login queued */
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

    post_login(gobj);

    int delay = (int)gobj_read_integer_attr(gobj, "check_delay_ms");
    set_timeout(priv->timer, delay);
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
 *  POST /auth/login from a plain socket: the kernel completes the
 *  connection, and the BFF takes the request in the loop
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
    char request[512];
    int len = snprintf(request, sizeof(request),
        "POST /auth/login HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Origin: http://localhost\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        (int)strlen(body), body
    );

    priv->fd_browser = socket(AF_INET, SOCK_STREAM, 0);
    if(priv->fd_browser < 0 ||
            connect(priv->fd_browser, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
            send(priv->fd_browser, request, (size_t)len, 0) != len) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "test22_discovery_unreachable: cannot post the login",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
    }
}

/***************************************************************************
 *  The login queued must have been answered 502 auth_service_unavailable
 ***************************************************************************/
PRIVATE void check_the_answer(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char answer[4096];
    ssize_t n = -1;
    if(priv->fd_browser >= 0) {
        n = recv(priv->fd_browser, answer, sizeof(answer) - 1, MSG_DONTWAIT);
    }
    if(n <= 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "test22_discovery_unreachable: the login queued got no answer",
            "errno",        "%d", n < 0? errno : 0,
            NULL
        );
        return;
    }
    answer[n] = '\0';
    char *eol = strstr(answer, "\r\n");
    BOOL is_502 = strncmp(answer, "HTTP/1.", 7) == 0 && eol &&
        memmem(answer, (size_t)(eol - answer), " 502", 4) != NULL;
    if(!is_502 || !strstr(answer, "auth_service_unavailable")) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "test22_discovery_unreachable: the login queued was not answered 502 auth_service_unavailable",
            "answer",       "%.200s", answer,
            NULL
        );
    }
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

    /*
     *  Initial role: the discovery's connect watchdog has fired by now,
     *  the login queued was answered, and the timeout counted like any
     *  other IdP timeout.
     */
    check_the_answer(gobj);

    hgobj bff = test_helpers_find_bff(gobj);
    if(bff) {
        const test_stat_expect_t expected[] = {
            {"idp_timeouts", 1},
            {NULL, 0}
        };
        test_helpers_check_stats(gobj, bff, "test22_discovery_unreachable", expected);
    }

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
GOBJ_DEFINE_GCLASS(C_TEST22_DISCOVERY_UNREACHABLE);

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
PUBLIC int register_c_test22_discovery_unreachable(void)
{
    return create_gclass(C_TEST22_DISCOVERY_UNREACHABLE);
}
