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
 *              127.0.1.2   refused (denied wins; not logged: see phase 3)
 *              127.0.1.3   refused ("TCP_S: Ip not allowed")
 *              127.0.1.4   accepted
 *
 *          Phase 3, 127.0.1.2 connects 5 times more: refused, not logged.
 *              A refusal is logged on the transition, the first one of a
 *              cause and then one a minute at most, with the count of the
 *              ones in between; refusedConnxs counts all 8. Up to 7.25.4
 *              each refusal wrote its line: a denied host in a loop was a
 *              flood of the log.
 *
 *          And the key a peername is looked up by: the ip without its port,
 *          for ipv4, ipv6 ("[2001:db8::1]:443") and an ipv4 seen by a
 *          dual-stack socket ("[::ffff:1.2.3.4]:80"). Up to 7.25.4 the port
 *          was cut at the first ':', so no list could name an ipv6 peer.
 *
 *          Before the peers, the form the lists are kept in: the entries
 *          the config brings as 7.25.4 stored them (typed by hand) are
 *          renamed at load to the form a peer is looked up by, or dropped
 *          when they are no ip; and the add-/remove- commands store that
 *          form, refuse what is not an ip, and take a link-local address
 *          with its interface (required in allowed_ips; in denied_ips an
 *          entry without it denies the address on every interface). Up to
 *          7.25.4 an entry like 2001:DB8::1 was stored as typed, answered
 *          success, and never matched a peer.
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
PRIVATE int check_normalized_lists(hgobj gobj);
PRIVATE int check_ip_commands(hgobj gobj);

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
PRIVATE peer_t phase3[] = {
    {"127.0.1.2", FALSE, -1},
    {"127.0.1.2", FALSE, -1},
    {"127.0.1.2", FALSE, -1},
    {"127.0.1.2", FALSE, -1},
    {"127.0.1.2", FALSE, -1},
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
    close_peers(phase3);
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    check_normalized_lists(gobj);
    check_ip_commands(gobj);

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
 *  The lists as the config left them, normalised at load (main.c)
 ***************************************************************************/
extern int ip_list_saves_allowed;
extern int ip_list_saves_denied;

PRIVATE int check_normalized_lists(hgobj gobj)
{
    int ret = 0;
    if(ip_list_saves_allowed != 1 || ip_list_saves_denied != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "ip lists normalised at load and not saved",
            "saves_allowed","%d", ip_list_saves_allowed,
            "saves_denied", "%d", ip_list_saves_denied,
            NULL
        );
        ret = -1;
    }
    json_t *expected_allowed = json_pack("{s:b, s:b}",
        "fe80::8%1", 1,
        "10.9.9.12", 1
    );
    json_t *expected_denied = json_pack("{s:b, s:b, s:b}",
        "2001:db8::5", 1,
        "10.9.9.9", 1,
        "10.9.9.11", 1
    );
    struct {
        const char *attr;
        json_t *expected;
    } lists[] = {
        {"allowed_ips", expected_allowed},
        {"denied_ips",  expected_denied},
        {0}
    };
    for(int i=0; lists[i].attr; i++) {
        json_t *jn_list = gobj_read_json_attr(gobj_yuno(), lists[i].attr);
        if(!json_equal(jn_list, lists[i].expected)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "ip list not normalised at load",
                "attr",         "%s", lists[i].attr,
                "expected",     "%j", lists[i].expected,
                "got",          "%j", jn_list,
                NULL
            );
            ret = -1;
        }
        JSON_DECREF(lists[i].expected)
    }

    json_object_clear(gobj_read_json_attr(gobj_yuno(), "allowed_ips"));
    json_object_clear(gobj_read_json_attr(gobj_yuno(), "denied_ips"));
    return ret;
}

/***************************************************************************
 *  The add-/remove- commands store the form a peer is looked up by, and
 *  refuse what is not an ip. Up to 7.25.4 they stored the text as typed and
 *  answered success, and an entry like 2001:DB8::1 never matched a peer.
 ***************************************************************************/
PRIVATE int check_ip_commands(hgobj gobj)
{
    struct {
        const char *command;
        const char *ip;
        int result;
        const char *stored;     // key expected in the list, or NULL
    } cases[] = {
        {"add-denied-ip",       "2001:DB8::1",              0,  "2001:db8::1"},
        {"add-denied-ip",       "2001:db8:0:0:0:0:0:2",     0,  "2001:db8::2"},
        {"add-denied-ip",       "::ffff:203.0.113.7",       0,  "203.0.113.7"},
        {"add-denied-ip",       "fe80::1",                  0,  "fe80::1"},
        {"add-denied-ip",       "fe80::2%lo",               0,  "fe80::2%1"},
        {"add-denied-ip",       "[2001:db8::3]",            -1, NULL},
        {"add-denied-ip",       "203.0.113.8:443",          -1, NULL},
        {"add-denied-ip",       "localhost",                -1, NULL},
        {"add-denied-ip",       "203.0.113.9%eth0",         -1, NULL},
        {"add-denied-ip",       "2001:db8::4%1",            -1, NULL},
        {"add-denied-ip",       "fe80::5%nosuchif0",        -1, NULL},
        {"add-allowed-ip",      "FE80::9",                  -1, NULL},
        {"add-allowed-ip",      "fe80::9%1",                0,  "fe80::9%1"},
        {"add-allowed-ip",      "::FFFF:198.51.100.1",      0,  "198.51.100.1"},
        {0}
    };

    int ret = 0;
    for(int i=0; cases[i].command; i++) {
        BOOL denied_list = strstr(cases[i].command, "denied")?TRUE:FALSE;
        json_t *jn_resp = gobj_command(
            gobj_yuno(),
            cases[i].command,
            json_pack("{s:s, s:b}",
                "ip", cases[i].ip,
                denied_list?"denied":"allowed", 1
            ),
            gobj
        );
        int result = (int)kw_get_int(gobj, jn_resp, "result", -99, 0);
        json_t *jn_list = gobj_read_json_attr(
            gobj_yuno(), denied_list?"denied_ips":"allowed_ips"
        );
        BOOL stored_ok = cases[i].stored?
            json_is_true(json_object_get(jn_list, cases[i].stored)):
            (json_object_get(jn_list, cases[i].ip)==NULL);
        if(result != cases[i].result || !stored_ok) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "wrong answer of an ip list command",
                "command",      "%s", cases[i].command,
                "ip",           "%s", cases[i].ip,
                "expected",     "%d", cases[i].result,
                "got",          "%d", result,
                "stored",       "%s", cases[i].stored?cases[i].stored:"(nothing)",
                "response",     "%j", jn_resp,
                NULL
            );
            ret = -1;
        }
        JSON_DECREF(jn_resp)
    }

    /*
     *  What each entry does to a peer
     */
    struct {
        const char *peername;
        BOOL denied;
        BOOL allowed;
    } peers[] = {
        {"[2001:db8::1]:443",           TRUE,   FALSE},
        {"[2001:db8::2]:443",           TRUE,   FALSE},
        {"203.0.113.7:5000",            TRUE,   FALSE},
        {"[::ffff:203.0.113.7]:5000",   TRUE,   FALSE},
        {"[fe80::1%2]:22",              TRUE,   FALSE},     // no interface: every one
        {"[fe80::1%7]:22",              TRUE,   FALSE},
        {"[fe80::2%1]:22",              TRUE,   FALSE},
        {"[fe80::2%2]:22",              FALSE,  FALSE},     // another interface
        {"[fe80::9%1]:22",              FALSE,  TRUE},
        {"[fe80::9%2]:22",              FALSE,  FALSE},     // another link, another host
        {"[::ffff:198.51.100.1]:80",    FALSE,  TRUE},
        {0}
    };
    for(int i=0; peers[i].peername; i++) {
        BOOL denied = is_ip_denied(peers[i].peername);
        BOOL allowed = is_ip_allowed(peers[i].peername);
        if(denied != peers[i].denied || allowed != peers[i].allowed) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "wrong ip list verdict for a peername",
                "peername",     "%s", peers[i].peername,
                "denied",       "%s", denied?"yes":"no",
                "allowed",      "%s", allowed?"yes":"no",
                NULL
            );
            ret = -1;
        }
    }

    /*
     *  Removed by any form of the same ip; one not in the list is an error
     */
    struct {
        const char *command;
        const char *ip;
        int result;
    } removes[] = {
        {"remove-denied-ip",    "2001:DB8:0:0:0:0:0:1",     0},
        {"remove-denied-ip",    "2001:db8::2",              0},
        {"remove-denied-ip",    "::FFFF:203.0.113.7",       0},
        {"remove-denied-ip",    "fe80::1",                  0},
        {"remove-denied-ip",    "fe80::2%1",                0},
        {"remove-denied-ip",    "fe80::2%1",                -1},
        {"remove-denied-ip",    "not-an-ip",                -1},
        {"remove-allowed-ip",   "fe80::9%lo",               0},
        {"remove-allowed-ip",   "198.51.100.1",             0},
        {0}
    };
    for(int i=0; removes[i].command; i++) {
        json_t *jn_resp = gobj_command(
            gobj_yuno(),
            removes[i].command,
            json_pack("{s:s}", "ip", removes[i].ip),
            gobj
        );
        int result = (int)kw_get_int(gobj, jn_resp, "result", -99, 0);
        if(result != removes[i].result) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "wrong answer of an ip list command",
                "command",      "%s", removes[i].command,
                "ip",           "%s", removes[i].ip,
                "expected",     "%d", removes[i].result,
                "got",          "%d", result,
                "response",     "%j", jn_resp,
                NULL
            );
            ret = -1;
        }
        JSON_DECREF(jn_resp)
    }

    size_t left = json_object_size(gobj_read_json_attr(gobj_yuno(), "denied_ips")) +
        json_object_size(gobj_read_json_attr(gobj_yuno(), "allowed_ips"));
    if(left != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "ip lists not empty after the removes",
            "denied_ips",   "%j", gobj_read_json_attr(gobj_yuno(), "denied_ips"),
            "allowed_ips",  "%j", gobj_read_json_attr(gobj_yuno(), "allowed_ips"),
            NULL
        );
        ret = -1;
    }
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

        case 2:
            check_peers(gobj, phase2, "deny-list with only_allowed_ips");
            close_peers(phase2);
            connect_peers(gobj, phase3);
            set_timeout(priv->timer, 300);
            break;

        default:
            {
                check_peers(gobj, phase3, "a denied host that reconnects");
                close_peers(phase3);
                hgobj server_port = gobj_find_child(
                    priv->gobj_input_side,
                    json_pack("{s:s}", "__gclass_name__", C_TCP_S)
                );
                json_int_t refused = gobj_read_integer_attr(server_port, "refusedConnxs");
                if(refused != 8) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "wrong count of refused connections",
                        "expected",     "%d", 8,
                        "got",          "%ld", (long)refused,
                        NULL
                    );
                }
            }
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
