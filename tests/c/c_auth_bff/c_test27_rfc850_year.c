/****************************************************************************
 *          C_TEST27_RFC850_YEAR.C
 *
 *          Driver for test27_rfc850_year: the year of an RFC 850 date, which
 *          writes two digits of it (RFC 9110 §5.6.7): a date that appears to
 *          be more than 50 years in the future is the most recent year in
 *          the past with those digits. One step per date, a refresh the IdP
 *          answers 429 with that date as its Retry-After: it must be passed
 *          on as the IMF-fixdate of the year expected, or not passed on.
 *
 *          The rule depends on the date of today, so a test with the clock
 *          of today would rot, and would see one side of the rule only (the
 *          next century comes into play from 2051 on). The clock of this
 *          test is its own: the binary is linked with --wrap=gmtime_r
 *          (CMakeLists.txt), and each step says what "now" is. gmtime_r()
 *          is how read_retry_after() reads the year; no timer of the yuno
 *          reads it, and a log with a date of 2099 harms nobody.
 *
 *          A wrong century never passes by chance: 100 years are 5 or 6
 *          weekdays, and read_retry_after() checks the day-name.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <yunetas.h>

#include "c_test27_rfc850_year.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TEST_NAME       "test27_rfc850_year"
#define ORIGIN          "http://localhost"  /* the allowed_origin of the BFF (main_test27) */

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    const char *now;            /* what gmtime_r() answers: "YYYY-MM-DD hh:mm:ss" */
    const char *retry_after;    /* the Retry-After of the IdP */
    const char *passed_on;      /* the one the browser must get, NULL none */
} step_t;

/***************************************************************************
 *              Data
 ***************************************************************************/
PRIVATE const step_t steps[] = {
    /* 1: six days more than 50 years ahead: 1976, not 2076 */
    {"2026-10-31 12:00:00", "Saturday, 06-Nov-76 08:49:37 GMT",
        "Sat, 06 Nov 1976 08:49:37 GMT"},
    /* 2: exactly 50 years ahead: not more than 50 */
    {"2026-10-31 12:00:00", "Saturday, 31-Oct-76 12:00:00 GMT",
        "Sat, 31 Oct 2076 12:00:00 GMT"},
    /* 3: one second more than 50 years ahead */
    {"2026-10-31 12:00:00", "Sunday, 31-Oct-76 12:00:01 GMT",
        "Sun, 31 Oct 1976 12:00:01 GMT"},
    /* 4: less than 50 years ahead */
    {"2026-10-31 12:00:00", "Tuesday, 31-Dec-75 23:59:59 GMT",
        "Tue, 31 Dec 2075 23:59:59 GMT"},
    /* 5: now */
    {"2026-10-31 12:00:00", "Saturday, 31-Oct-26 12:00:00 GMT",
        "Sat, 31 Oct 2026 12:00:00 GMT"},
    /* 6: the past */
    {"2026-10-31 12:00:00", "Friday, 31-Dec-99 23:59:59 GMT",
        "Fri, 31 Dec 1999 23:59:59 GMT"},
    /* 7: the first year of the century */
    {"2026-10-31 12:00:00", "Saturday, 01-Jan-00 00:00:00 GMT",
        "Sat, 01 Jan 2000 00:00:00 GMT"},
    /* 8: the next century: "05" is 2105, not 2005 */
    {"2060-06-15 00:00:00", "Thursday, 01-Jan-05 00:00:00 GMT",
        "Thu, 01 Jan 2105 00:00:00 GMT"},
    /* 9: exactly 50 years ahead */
    {"2060-06-15 00:00:00", "Sunday, 15-Jun-10 00:00:00 GMT",
        "Sun, 15 Jun 2110 00:00:00 GMT"},
    /* 10: one second more than 50 years ahead: 2010 */
    {"2060-06-15 00:00:00", "Tuesday, 15-Jun-10 00:00:01 GMT",
        "Tue, 15 Jun 2010 00:00:01 GMT"},
    /* 11: this century, ahead */
    {"2060-06-15 00:00:00", "Thursday, 31-Dec-99 23:59:59 GMT",
        "Thu, 31 Dec 2099 23:59:59 GMT"},
    /* 12: "11": 2111 is more than 50 years ahead */
    {"2060-06-15 00:00:00", "Saturday, 01-Jan-11 00:00:00 GMT",
        "Sat, 01 Jan 2011 00:00:00 GMT"},
    /* 13: now */
    {"2060-06-15 00:00:00", "Tuesday, 15-Jun-60 00:00:00 GMT",
        "Tue, 15 Jun 2060 00:00:00 GMT"},
    /* 14: "00" one second ahead: the next century */
    {"2099-12-31 23:59:59", "Friday, 01-Jan-00 00:00:00 GMT",
        "Fri, 01 Jan 2100 00:00:00 GMT"},
    /* 15: exactly 50 years ahead */
    {"2099-12-31 23:59:59", "Wednesday, 31-Dec-49 23:59:59 GMT",
        "Wed, 31 Dec 2149 23:59:59 GMT"},
    /* 16: "50": one second more than 50 years ahead */
    {"2099-12-31 23:59:59", "Saturday, 01-Jan-50 00:00:00 GMT",
        "Sat, 01 Jan 2050 00:00:00 GMT"},
    /* 17: exactly 50 years ahead, in this century */
    {"2000-01-01 00:00:00", "Saturday, 01-Jan-50 00:00:00 GMT",
        "Sat, 01 Jan 2050 00:00:00 GMT"},
    /* 18: one second more than 50 years ahead */
    {"2000-01-01 00:00:00", "Sunday, 01-Jan-50 00:00:01 GMT",
        "Sun, 01 Jan 1950 00:00:01 GMT"},
    /* 19: "99": the past of the century before */
    {"2000-01-01 00:00:00", "Friday, 01-Jan-99 00:00:00 GMT",
        "Fri, 01 Jan 1999 00:00:00 GMT"},
    /* 20: a day-name that is not the one of the date: not passed on */
    {"2026-10-31 12:00:00", "Sunday, 06-Nov-76 08:49:37 GMT", NULL},
    /* 21: the day-name of 2076, the year it is not */
    {"2026-10-31 12:00:00", "Friday, 06-Nov-76 08:49:37 GMT", NULL},
};
#define NSTEPS ((int)(sizeof(steps)/sizeof(steps[0])))

/*
 *  The clock of the step in flight (see the head of this file)
 */
PRIVATE struct tm s_now;
PRIVATE BOOL s_now_set = FALSE;

struct tm *__real_gmtime_r(const time_t *timep, struct tm *result);
struct tm *__wrap_gmtime_r(const time_t *timep, struct tm *result);

/***************************************************************************
 *  gmtime_r() of this binary
 ***************************************************************************/
struct tm *__wrap_gmtime_r(const time_t *timep, struct tm *result)
{
    if(s_now_set) {
        *result = s_now;
        return result;
    }
    return __real_gmtime_r(timep, result);
}

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
 *  The request of the current step, with the Retry-After of the mock and
 *  the clock of the step
 ***************************************************************************/
PRIVATE int post_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const step_t *st = &steps[priv->step];
    hgobj mock = find_mock(gobj);
    if(!mock) {
        return -1;  // Error already logged
    }
    gobj_write_integer_attr(mock, "return_status", 429);
    gobj_write_str_attr(mock, "retry_after", st->retry_after);

    memset(&s_now, 0, sizeof(s_now));
    int year = 0, month = 0;
    if(sscanf(st->now, "%d-%d-%d %d:%d:%d", &year, &month, &s_now.tm_mday,
            &s_now.tm_hour, &s_now.tm_min, &s_now.tm_sec) != 6) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", TEST_NAME ": a step with a bad date",
            "now",      "%s", st->now,
            NULL
        );
        return -1;
    }
    s_now.tm_year = year - 1900;
    s_now.tm_mon = month - 1;
    s_now_set = TRUE;

    char request[512];
    int len = snprintf(request, sizeof(request),
        "POST /auth/refresh HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Origin: " ORIGIN "\r\n"
        "Cookie: refresh_token=RT1\r\n"
        "Content-Length: 0\r\n"
        "\r\n"
    );
    return send_request(gobj, request, (size_t)len);
}

/***************************************************************************
 *  Read what has come: TRUE when the answer is whole
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
    if(!end_of_headers || !content_length) {
        return FALSE;
    }
    size_t body_len = (size_t)atoi(content_length + strlen("Content-Length:"));
    return end_of_headers + 4 + body_len <= priv->answer + priv->got;
}

/***************************************************************************
 *  The answer of the current step
 ***************************************************************************/
PRIVATE void check_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const step_t *st = &steps[priv->step];

    char expected[128] = "\r\nRetry-After:";
    if(st->passed_on) {
        snprintf(expected, sizeof(expected), "\r\nRetry-After: %s\r\n", st->passed_on);
    }
    BOOL has_it = strstr(priv->answer, expected) != NULL;
    if(strncmp(priv->answer, "HTTP/1.1 429 ", 13) != 0 || has_it != (st->passed_on != NULL)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", TEST_NAME ": a Retry-After not read as expected",
            "step",         "%d", priv->step + 1,
            "now",          "%s", st->now,
            "retry_after",  "%s", st->retry_after,
            "expected",     "%s", st->passed_on? st->passed_on : "not passed on",
            "answer",       "%.400s", priv->answer,
            NULL
        );
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
        BOOL done = read_answer(gobj);
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

    s_now_set = FALSE;
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
GOBJ_DEFINE_GCLASS(C_TEST27_RFC850_YEAR);

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
PUBLIC int register_c_test27_rfc850_year(void)
{
    return create_gclass(C_TEST27_RFC850_YEAR);
}
