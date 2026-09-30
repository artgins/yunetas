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
 *          Two more C_TCP_S, `shared_a` on 127.0.0.1:7816 and `shared_b`
 *          on 127.0.0.2:7816, share ONE C_IOGATE and its 4 channels (the
 *          same port on two hosts, as the agent's servers share a pool).
 *
 *              1. 2 peers connect to each:           connxs 2, tconnxs 2
 *              2. 1 peer of each closes:             connxs 1, tconnxs 2
 *              3. 1 more peer connects to each:      connxs 2, tconnxs 3
 *              4. `shared_a` stopped, and started again: the same counts
 *
 *          Then a C_IOGATE of the new method whose 2 channels have no
 *          C_TCP (the C_TCP_S creates clisrv-1 and clisrv-2) is stopped, a
 *          third channel added, and started again: its clisrv must be
 *          clisrv-3, not a second clisrv-1.
 *
 *          Up to 7.25.20 both stats read 0 always: they were SDF_STATS
 *          attrs backed by priv counters that no mt_reading served, and
 *          the `new` server does not see the accepts at all. The first
 *          count of this branch matched a connection by its local PORT:
 *          `shared_a` and `shared_b` each counted the connections of both.
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
#define SHARED_PORT 7816
#define MAX_PEERS   3

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    const char *gate;   // the C_IOGATE service of the server
    const char *name;   // the C_TCP_S
    const char *host;
    int port;
    int fds[MAX_PEERS];
} server_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int connect_peer(hgobj gobj, server_t *server, int idx);
PRIVATE void close_peer(server_t *server, int idx);
PRIVATE void check_stats(hgobj gobj, server_t *server, const char *phase, int connxs, int tconnxs);
PRIVATE hgobj find_server(server_t *server);
PRIVATE void add_names_channel(hgobj gobj);
PRIVATE void check_names_channel(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE server_t servers[] = {
    {"__legacy_side__", "legacy_port", "127.0.0.1", LEGACY_PORT, {-1, -1, -1}},
    {"__new_side__",    "new_port",    "127.0.0.1", NEW_PORT,    {-1, -1, -1}},
    {"__shared_side__", "shared_a",    "127.0.0.1", SHARED_PORT, {-1, -1, -1}},
    {"__shared_side__", "shared_b",    "127.0.0.2", SHARED_PORT, {-1, -1, -1}},
    {0}
};
PRIVATE const char *gates[] = {
    "__legacy_side__", "__new_side__", "__shared_side__", "__names_side__", 0
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

    for(int g=0; gates[g]; g++) {
        hgobj gate = gobj_find_service(gates[g], TRUE);
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
    inet_pton(AF_INET, server->host, &sa.sin_addr);
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
 *  The C_TCP_S of `server`
 ***************************************************************************/
PRIVATE hgobj find_server(server_t *server)
{
    hgobj gate = gobj_find_service(server->gate, TRUE);
    return gobj_find_child(gate, json_pack("{s:s, s:s}",
        "__gclass_name__", C_TCP_S,
        "__gobj_name__", server->name
    ));
}

/***************************************************************************
 *  A third channel of the names gate, with no C_TCP, while it is stopped
 ***************************************************************************/
PRIVATE void add_names_channel(hgobj gobj)
{
    hgobj gate = gobj_find_service("__names_side__", TRUE);
    hgobj channel = gobj_create("names-3", C_CHANNEL, 0, gate);
    hgobj prot = gobj_create("names-3", C_PROT_TCP4H, 0, channel);
    gobj_set_bottom_gobj(channel, prot);
}

/***************************************************************************
 *  Its clisrv, created by the C_TCP_S at its start again, is clisrv-3
 ***************************************************************************/
PRIVATE void check_names_channel(hgobj gobj)
{
    hgobj gate = gobj_find_service("__names_side__", TRUE);
    hgobj channel = gobj_find_child(gate, json_pack("{s:s}", "__gobj_name__", "names-3"));
    hgobj clisrv = channel? gobj_last_bottom_gobj(channel) : 0;
    const char *name = clisrv? gobj_name(clisrv) : "";
    if(!clisrv || gobj_gclass_name(clisrv) != C_TCP || strcmp(name, "clisrv-3")!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "wrong name of a clisrv created at a restart",
            "clisrv",       "%s", name,
            "expected",     "%s", "clisrv-3",
            NULL
        );
    }
}

/***************************************************************************
 *  The stats of the C_TCP_S of `server`, as the attrs and as `stats` read
 ***************************************************************************/
PRIVATE void check_stats(hgobj gobj, server_t *server, const char *phase, int connxs, int tconnxs)
{
    hgobj server_port = find_server(server);

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
            "server",       "%s", server->name,
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

        case 3:
            for(int s=0; servers[s].gate; s++) {
                check_stats(gobj, &servers[s], "1 peer more", 2, 3);
            }
            gobj_stop(find_server(&servers[2]));    // shared_a
            gobj_stop_tree(gobj_find_service("__names_side__", TRUE));
            set_timeout(priv->timer, 300);
            break;

        case 4:
            gobj_start(find_server(&servers[2]));
            add_names_channel(gobj);
            gobj_start_tree(gobj_find_service("__names_side__", TRUE));
            set_timeout(priv->timer, 300);
            break;

        default:
            for(int s=0; servers[s].gate; s++) {
                check_stats(gobj, &servers[s], "shared_a started again", 2, 3);
            }
            check_names_channel(gobj);
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
