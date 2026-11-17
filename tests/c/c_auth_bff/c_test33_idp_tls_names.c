/****************************************************************************
 *          C_TEST33_IDP_TLS_NAMES.C
 *
 *          Driver for test33_idp_tls_names. The BFF (main_test33) has
 *          two https issuers, `localhost` and `127.0.0.1`, where nobody
 *          listens. Its discovery of the first fails at the start (it is
 *          down), and a login is sent: it goes to the second IdP, then back
 *          to the first, and is answered 502. Both outbounds have set up TLS
 *          by then.
 *
 *          Checks, when the answer is there:
 *            - the login is answered 502 auth_service_unavailable;
 *            - the C_TCP of each IdP ("bff-1-idp", "bff-1-idp2") verifies
 *              against the host of its own url: ssl_server_name
 *              "localhost" and "127.0.0.1";
 *            - the crypto attr of the BFF has no ssl_server_name (its
 *              outbounds got copies of it).
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
#include "c_test33_idp_tls_names.h"

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void post_login(hgobj gobj);
PRIVATE void check_the_names(hgobj gobj);

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
            "msg",          "%s", "test33_idp_tls_names: cannot post the login",
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
            "msg",          "%s", "test33_idp_tls_names: the login queued got no answer",
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
            "msg",          "%s", "test33_idp_tls_names: the login queued was not answered 502 auth_service_unavailable",
            "answer",       "%.200s", answer,
            NULL
        );
    }
}




/***************************************************************************
 *  The ssl_server_name of a C_TCP of the BFF side, by its name
 ***************************************************************************/
PRIVATE const char *server_name_of(hgobj gobj, const char *tcp_name)
{
    hgobj bff_side = gobj_find_service("__bff_side__", FALSE);
    json_t *jn_filter = json_pack("{s:s, s:s}",
        "__gclass_name__", "C_TCP",
        "__gobj_name__", tcp_name
    );
    json_t *dl = bff_side? gobj_match_children_tree(bff_side, jn_filter) : NULL;
    if(!bff_side) {
        JSON_DECREF(jn_filter)
    }
    const char *name = NULL;
    if(dl && json_array_size(dl) == 1) {
        hgobj tcp = (hgobj)(size_t)json_integer_value(json_array_get(dl, 0));
        name = json_string_value(json_object_get(
            gobj_read_json_attr(tcp, "crypto"), "ssl_server_name"));
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "test33_idp_tls_names: the C_TCP of an IdP not found",
            "name",         "%s", tcp_name,
            NULL
        );
    }
    gobj_free_iter(dl);
    return name;
}

/***************************************************************************
 *  Each IdP verified against its own name, the BFF's crypto untouched
 ***************************************************************************/
PRIVATE void check_the_names(hgobj gobj)
{
    const struct {
        const char *tcp;
        const char *expected;
    } idps[] = {
        {"bff-1-idp",  "localhost"},
        {"bff-1-idp2", "127.0.0.1"},
    };
    for(size_t i = 0; i < sizeof(idps)/sizeof(idps[0]); i++) {
        const char *name = server_name_of(gobj, idps[i].tcp);
        if(!name || strcmp(name, idps[i].expected) != 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", "test33_idp_tls_names: an IdP is verified against another name",
                "tcp",          "%s", idps[i].tcp,
                "expected",     "%s", idps[i].expected,
                "ssl_server_name", "%s", name? name : "(none)",
                NULL
            );
        }
    }

    hgobj bff = test_helpers_find_bff(gobj);
    if(bff && json_object_get(gobj_read_json_attr(bff, "crypto"), "ssl_server_name")) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "test33_idp_tls_names: the crypto of the BFF was written by an outbound",
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
     *  Initial role: both IdPs have been tried by now (each one's
     *  connect watchdog fired), and the login was answered.
     */
    check_the_answer(gobj);
    check_the_names(gobj);

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
GOBJ_DEFINE_GCLASS(C_TEST33_IDP_TLS_NAMES);

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
PUBLIC int register_c_test33_idp_tls_names(void)
{
    return create_gclass(C_TEST33_IDP_TLS_NAMES);
}
