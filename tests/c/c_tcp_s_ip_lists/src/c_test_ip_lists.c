/***********************************************************************
 *          C_TEST_IP_LISTS.C
 *
 *          GClass to test the yuno's ip lists at C_TCP_S accept
 *
 *          Up to 7.25.4 the accept path asked only the ALLOW-list, and only
 *          with `only_allowed_ips`: an ip in `denied_ips` was refused by an
 *          authenticating gate at login, and by nothing at all on a gate
 *          that does not authenticate (an mqtt/IoT field port).
 *
 *          The peers are raw sockets bound to 127.0.1.x, which the kernel
 *          delivers over loopback with that source address, so a list can
 *          name them while 127.0.0.1 stays the exempt local peer.
 *
 *          Phase 1, only_allowed_ips = false, denied: 127.0.1.2, 127.0.0.1
 *              127.0.1.2   refused ("TCP_S: Ip denied")
 *              127.0.1.3   accepted (no list names it)
 *              127.0.0.1   accepted (loopback is exempt, even if listed)
 *
 *          Phase 2, only_allowed_ips = true, allowed: 127.0.1.2, 127.0.1.4
 *              127.0.1.2   refused ("TCP_S: Ip denied": denied wins)
 *              127.0.1.3   refused ("TCP_S: Ip not allowed")
 *              127.0.1.4   accepted
 *
 *          And the key a peername is looked up by: the ip without its port,
 *          for ipv4, ipv6 ("[2001:db8::1]:443") and an ipv4 seen by a
 *          dual-stack socket ("[::ffff:1.2.3.4]:80"). Up to 7.25.4 the port
 *          was cut at the first ':', so no list could name an ipv6 peer.
 *
 *          A wrong verdict is logged as an error, which the expected-logs
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
#include "c_test_ip_lists.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TEST_PORT   7793

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    const char *src_ip;
    BOOL must_be_accepted;
    int fd;
} peer_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int connect_peers(hgobj gobj, peer_t *peers);
PRIVATE int check_peers(hgobj gobj, peer_t *peers, const char *phase);
PRIVATE void close_peers(peer_t *peers);
PRIVATE int check_peername_keys(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE peer_t phase1[] = {
    {"127.0.1.2", FALSE, -1},
    {"127.0.1.3", TRUE,  -1},
    {"127.0.0.1", TRUE,  -1},
    {0}
};
PRIVATE peer_t phase2[] = {
    {"127.0.1.2", FALSE, -1},
    {"127.0.1.3", FALSE, -1},
    {"127.0.1.4", TRUE,  -1},
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
enum {
    TRACE_MESSAGES  = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    hgobj gobj_input_side;
    int step;
    int opens;
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
    close_peers(phase1);
    close_peers(phase2);
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *denied_ips = gobj_read_json_attr(gobj_yuno(), "denied_ips");
    json_object_set_new(denied_ips, "127.0.1.2", json_true());
    json_object_set_new(denied_ips, "127.0.0.1", json_true());

    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

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
 *  A client socket bound to `src_ip`, connected to the test port.
 *  The kernel completes a loopback connect into the listen backlog, before
 *  the server has accepted it.
 ***************************************************************************/
PRIVATE int connect_peers(hgobj gobj, peer_t *peers)
{
    for(int i=0; peers[i].src_ip; i++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if(fd < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "socket() FAILED",
                "errno",        "%s", strerror(errno),
                NULL
            );
            return -1;
        }
        struct sockaddr_in src = {0};
        src.sin_family = AF_INET;
        inet_pton(AF_INET, peers[i].src_ip, &src.sin_addr);
        struct sockaddr_in dst = {0};
        dst.sin_family = AF_INET;
        dst.sin_port = htons(TEST_PORT);
        inet_pton(AF_INET, "127.0.0.1", &dst.sin_addr);

        if(bind(fd, (struct sockaddr *)&src, sizeof(src))<0 ||
            connect(fd, (struct sockaddr *)&dst, sizeof(dst))<0
        ) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "bind()/connect() FAILED",
                "src_ip",       "%s", peers[i].src_ip,
                "errno",        "%s", strerror(errno),
                NULL
            );
            close(fd);
            return -1;
        }
        peers[i].fd = fd;
    }
    return 0;
}

/***************************************************************************
 *  A refused peer reads end-of-file (or a reset); an accepted one has
 *  nothing to read yet and stays open.
 ***************************************************************************/
PRIVATE int check_peers(hgobj gobj, peer_t *peers, const char *phase)
{
    int ret = 0;
    for(int i=0; peers[i].src_ip; i++) {
        char c;
        ssize_t n = recv(peers[i].fd, &c, 1, MSG_DONTWAIT);
        BOOL accepted = (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))?TRUE:FALSE;
        if(accepted != peers[i].must_be_accepted) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "wrong ip list verdict at accept",
                "phase",        "%s", phase,
                "src_ip",       "%s", peers[i].src_ip,
                "expected",     "%s", peers[i].must_be_accepted?"accepted":"refused",
                "got",          "%s", accepted?"accepted":"refused",
                NULL
            );
            ret = -1;
        }
    }
    return ret;
}

/***************************************************************************
 *  The key of the lists for every form of peername
 ***************************************************************************/
PRIVATE int check_peername_keys(hgobj gobj)
{
    struct {
        const char *peername;
        BOOL denied;
    } cases[] = {
        {"10.1.2.3:5678",           TRUE},
        {"10.1.2.3",                TRUE},
        {"10.1.2.4:5678",           FALSE},
        {"[2001:db8::1]:443",       TRUE},
        {"2001:db8::1",             TRUE},
        {"[2001:db8::2]:443",       FALSE},
        {"[::ffff:10.1.2.3]:80",    TRUE},
        {"[::ffff:10.1.2.4]:80",    FALSE},
        {0}
    };

    json_t *denied_ips = gobj_read_json_attr(gobj_yuno(), "denied_ips");
    json_object_set_new(denied_ips, "10.1.2.3", json_true());
    json_object_set_new(denied_ips, "2001:db8::1", json_true());

    int ret = 0;
    for(int i=0; cases[i].peername; i++) {
        if(is_ip_denied(cases[i].peername) != cases[i].denied) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "wrong ip list key for a peername",
                "peername",     "%s", cases[i].peername,
                "expected",     "%s", cases[i].denied?"denied":"not denied",
                NULL
            );
            ret = -1;
        }
    }

    json_object_del(denied_ips, "10.1.2.3");
    json_object_del(denied_ips, "2001:db8::1");
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void close_peers(peer_t *peers)
{
    for(int i=0; peers[i].src_ip; i++) {
        if(peers[i].fd >= 0) {
            close(peers[i].fd);
            peers[i].fd = -1;
        }
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
            check_peername_keys(gobj);
            connect_peers(gobj, phase1);
            set_timeout(priv->timer, 300);
            break;

        case 1:
            {
                check_peers(gobj, phase1, "deny-list alone");
                close_peers(phase1);

                json_t *allowed_ips = gobj_read_json_attr(gobj_yuno(), "allowed_ips");
                json_object_set_new(allowed_ips, "127.0.1.2", json_true());
                json_object_set_new(allowed_ips, "127.0.1.4", json_true());
                hgobj server_port = gobj_find_child(
                    priv->gobj_input_side,
                    json_pack("{s:s}", "__gclass_name__", C_TCP_S)
                );
                gobj_write_bool_attr(server_port, "only_allowed_ips", TRUE);

                connect_peers(gobj, phase2);
                set_timeout(priv->timer, 300);
            }
            break;

        default:
            check_peers(gobj, phase2, "deny-list with only_allowed_ips");
            close_peers(phase2);
            if(priv->opens != 3) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "wrong number of channels opened",
                    "expected",     "%d", 3,
                    "got",          "%d", priv->opens,
                    NULL
                );
            }
            set_yuno_must_die();
            break;
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A channel opened: only an accepted peer gets one
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->opens++;

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A channel closed: the accepted peers, when the test closes them
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
GOBJ_DEFINE_GCLASS(C_TEST_IP_LISTS);

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
        {EV_ON_OPEN,                ac_on_open,                 0},
        {EV_ON_CLOSE,               ac_on_close,                0},
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
        s_user_trace_level,  // s_user_trace_level,
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
PUBLIC int register_c_test_ip_lists(void)
{
    return create_gclass(C_TEST_IP_LISTS);
}
