/***********************************************************************
 *          C_TEST_TCP_S_STATS.C
 *
 *          GClass to test the connection stats of C_TCP_S: `connxs` (the
 *          connections it holds now) and `tconnxs` (the connections it
 *          accepted since it started).
 *
 *          Two C_TCP_S, each in a C_IOGATE of its own with 3 channels
 *          (C_PROT_TCP4H over C_TCP): `legacy` on 127.0.0.1:7814 accepts
 *          with child_tree_filter; `new` on 127.0.0.1:7815 leaves each
 *          channel's C_TCP to accept by itself. The peers are raw sockets.
 *
 *              1. 2 peers connect to each:           connxs 2, tconnxs 2
 *              2. 1 peer of each closes:             connxs 1, tconnxs 2
 *              3. 1 more peer connects to each:      connxs 2, tconnxs 3
 *
 *          Up to 7.25.20 both stats read 0 always: they were SDF_STATS
 *          attrs backed by priv counters that no mt_reading served, and
 *          the `new` server does not see the accepts at all.
 *
 *          A wrong count is logged as an error, which the expected-logs
 *          check of main.c does not expect.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "c_test_tcp_s_stats.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define LEGACY_PORT 7814
#define NEW_PORT    7815
#define MAX_PEERS   3

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    const char *gate;   // the C_IOGATE service of the server
    int port;
    int fds[MAX_PEERS];
} server_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int connect_peer(hgobj gobj, server_t *server, int idx);
PRIVATE void close_peer(server_t *server, int idx);
PRIVATE void check_stats(hgobj gobj, server_t *server, const char *phase, int connxs, int tconnxs);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE server_t servers[] = {
    {"__legacy_side__", LEGACY_PORT, {-1, -1, -1}},
    {"__new_side__",    NEW_PORT,    {-1, -1, -1}},
    {0}
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
PRIVATE const trace_level_t s_user_trace_level[16] = {
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    int step;
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
    for(int s=0; servers[s].gate; s++) {
        for(int i=0; i<MAX_PEERS; i++) {
            close_peer(&servers[s], i);
        }
    }
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int s=0; servers[s].gate; s++) {
        hgobj gate = gobj_find_service(servers[s].gate, TRUE);
        gobj_subscribe_event(gate, NULL, 0, gobj);
        gobj_start_tree(gate);
    }

    set_timeout(priv->timer, 300);
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
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  A raw client socket connected to the server. The kernel completes a
 *  loopback connect into the listen backlog, before the server accepts it.
 ***************************************************************************/
PRIVATE int connect_peer(hgobj gobj, server_t *server, int idx)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "socket() FAILED",
            "errno",        "%d", errno,
            NULL
        );
        return -1;
    }
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)server->port);
    inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
    if(connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "connect() FAILED",
            "port",         "%d", server->port,
            "errno",        "%d", errno,
            NULL
        );
        close(fd);
        return -1;
    }
    server->fds[idx] = fd;
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void close_peer(server_t *server, int idx)
{
    if(server->fds[idx] >= 0) {
        close(server->fds[idx]);
        server->fds[idx] = -1;
    }
}

/***************************************************************************
 *  The stats of the C_TCP_S of `server`, as the attrs and as `stats` read
 ***************************************************************************/
PRIVATE void check_stats(hgobj gobj, server_t *server, const char *phase, int connxs, int tconnxs)
{
    hgobj gate = gobj_find_service(server->gate, TRUE);
    hgobj server_port = gobj_find_child(gate, json_pack("{s:s}", "__gclass_name__", C_TCP_S));

    json_int_t got_connxs = gobj_read_integer_attr(server_port, "connxs");
    json_int_t got_tconnxs = gobj_read_integer_attr(server_port, "tconnxs");

    json_t *jn_stats = gobj_stats(server_port, "", 0, gobj);
    json_t *jn_data = kw_get_dict(gobj, jn_stats, "data", 0, 0);
    json_int_t stats_connxs = kw_get_int(gobj, jn_data, "connxs", -1, 0);
    json_int_t stats_tconnxs = kw_get_int(gobj, jn_data, "tconnxs", -1, 0);
    JSON_DECREF(jn_stats)

    if(got_connxs != connxs || got_tconnxs != tconnxs ||
            stats_connxs != connxs || stats_tconnxs != tconnxs) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "wrong connection stats",
            "server",       "%s", server->gate,
            "phase",        "%s", phase,
            "connxs",       "%ld", (long)got_connxs,
            "tconnxs",      "%ld", (long)got_tconnxs,
            "stats_connxs", "%ld", (long)stats_connxs,
            "stats_tconnxs", "%ld", (long)stats_tconnxs,
            "expected_connxs", "%d", connxs,
            "expected_tconnxs", "%d", tconnxs,
            NULL
        );
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Each timeout is one step of the test
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->step++) {
        case 0:
            for(int s=0; servers[s].gate; s++) {
                connect_peer(gobj, &servers[s], 0);
                connect_peer(gobj, &servers[s], 1);
            }
            set_timeout(priv->timer, 300);
            break;

        case 1:
            for(int s=0; servers[s].gate; s++) {
                check_stats(gobj, &servers[s], "2 peers connected", 2, 2);
                close_peer(&servers[s], 0);
            }
            set_timeout(priv->timer, 300);
            break;

        case 2:
            for(int s=0; servers[s].gate; s++) {
                check_stats(gobj, &servers[s], "1 peer closed", 1, 2);
                connect_peer(gobj, &servers[s], 2);
            }
            set_timeout(priv->timer, 300);
            break;

        default:
            for(int s=0; servers[s].gate; s++) {
                check_stats(gobj, &servers[s], "1 peer more", 2, 3);
            }
            set_yuno_must_die();
            break;
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The channels of the servers open and close: nothing to do
 ***************************************************************************/
PRIVATE int ac_channel(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
    return 0;
}




/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_TCP_S_STATS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_idle[] = {
        {EV_TIMEOUT,                ac_timeout,                 0},
        {EV_ON_OPEN,                ac_channel,                 0},
        {EV_ON_CLOSE,               ac_channel,                 0},
        {EV_STOPPED,                ac_stopped,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_STOPPED,                0},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,
        0   // gcflag_t
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_test_tcp_s_stats(void)
{
    return create_gclass(C_TEST_TCP_S_STATS);
}
