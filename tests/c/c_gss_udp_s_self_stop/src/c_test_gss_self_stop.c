/***********************************************************************
 *          C_TEST_GSS_SELF_STOP.C
 *
 *          Driver of the C_GSS_UDP_S test: its C_UDP_S stops by itself.
 *
 *          A C_GSS_UDP_S child (timeout_base 1 s, disable_end_of_frame, so
 *          each datagram comes as it is, in the gbuffer C_UDP_S read it
 *          into) and a plain UDP socket, the peer.
 *
 *              0. The C_UDP_S's rx_buffer_size goes down to 0 and the peer
 *                 sends "one". The host keeps that gbuffer, so the next
 *                 read needs a new one, of 0 bytes: the read cannot be
 *                 started again, and C_UDP_S stops by itself (EV_STOPPED).
 *              1. (50 ms) rx_buffer_size back to 4096, the gbuffer
 *                 released, and the host sends "lost" to the peer: refused
 *                 by C_GSS_UDP_S, ONE warning.
 *              2. (after the next timeout_base) the peer sends "two".
 *              3. The host must have got "two", and sends "back".
 *              4. The peer must have got "back", and not "lost".
 *
 *          Up to 7.25.20 C_GSS_UDP_S took the EV_STOPPED of its C_UDP_S
 *          with no action: it went on sending to it ("Event NOT DEFINED in
 *          state" per datagram, from C_UDP_S in ST_STOPPED), and the
 *          service could not receive again.
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
#include "c_test_gss_self_stop.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define GSS_PORT    34296
#define GSS_URL     "udp://127.0.0.1:34296"
#define PEER_PORT   34297

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void peer_send(hgobj gobj, const char *data);
PRIVATE void host_send(hgobj gobj, const char *data);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
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
    hgobj gobj_gss;
    hgobj gobj_udp_s;
    int peer_fd;
    int phase;
    gbuffer_t *kept;
    char received[128];     // what the host got, in order
    struct sockaddr_in peer_addr;
    struct sockaddr_in server_addr;
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

    priv->peer_fd = -1;
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    priv->gobj_gss = gobj_create(
        "gss",
        C_GSS_UDP_S,
        json_pack("{s:s, s:i, s:b}",
            "url", GSS_URL,
            "timeout_base", 1000,
            "disable_end_of_frame", 1
        ),
        gobj
    );
    priv->gobj_udp_s = gobj_find_child(
        priv->gobj_gss,
        json_pack("{s:s}", "__gclass_name__", C_UDP_S)
    );
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBUFFER_DECREF(priv->kept)
    if(priv->peer_fd >= 0) {
        close(priv->peer_fd);
        priv->peer_fd = -1;
    }
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    gobj_start(priv->gobj_gss);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    gobj_stop(priv->gobj_gss);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  The peer: a plain UDP socket of the test
     */
    memset(&priv->peer_addr, 0, sizeof(priv->peer_addr));
    priv->peer_addr.sin_family = AF_INET;
    priv->peer_addr.sin_port = htons(PEER_PORT);
    priv->peer_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    priv->server_addr = priv->peer_addr;
    priv->server_addr.sin_port = htons(GSS_PORT);

    priv->peer_fd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
    if(priv->peer_fd < 0 ||
            bind(priv->peer_fd, (struct sockaddr *)&priv->peer_addr, sizeof(priv->peer_addr)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot bind the peer socket",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        set_yuno_must_die();
        return -1;
    }

    /*
     *  0. The next read will need a gbuffer of 0 bytes; the peer sends
     */
    gobj_write_integer_attr(priv->gobj_udp_s, "rx_buffer_size", 0);
    peer_send(gobj, "one");

    set_timeout(priv->timer, 50);
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
 *
 ***************************************************************************/
PRIVATE void peer_send(hgobj gobj, const char *data)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(sendto(priv->peer_fd, data, strlen(data), 0,
            (struct sockaddr *)&priv->server_addr, sizeof(priv->server_addr)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: sendto() FAILED",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
    }
}

/***************************************************************************
 *  The host sends to the peer, by its address
 ***************************************************************************/
PRIVATE void host_send(hgobj gobj, const char *data)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(strlen(data), strlen(data));
    gbuffer_append_string(gbuf, data);
    gbuffer_setaddr(gbuf, (struct sockaddr *)&priv->peer_addr, sizeof(priv->peer_addr));
    gobj_send_event(
        priv->gobj_gss,
        EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        gobj
    );
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The phases
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->phase++) {
        case 0:
            gobj_write_integer_attr(priv->gobj_udp_s, "rx_buffer_size", 4096);
            GBUFFER_DECREF(priv->kept)
            host_send(gobj, "lost");
            set_timeout(priv->timer, 1200);
            break;

        case 1:
            peer_send(gobj, "two");
            set_timeout(priv->timer, 200);
            break;

        case 2:
            host_send(gobj, "back");
            set_timeout(priv->timer, 200);
            break;

        default:
            {
                char got[64] = "";
                ssize_t n = recv(priv->peer_fd, got, sizeof(got) - 1, 0);
                if(n > 0) {
                    got[n] = 0;
                }
                if(strcmp(priv->received, "one two ")!=0 || strcmp(got, "back")!=0) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "TEST: C_GSS_UDP_S is deaf or mute after its C_UDP_S stopped by itself",
                        "host_received",    "%s", priv->received,
                        "host_expected",    "%s", "one two ",
                        "peer_received",    "%s", got,
                        "peer_expected",    "%s", "back",
                        NULL
                    );
                } else {
                    gobj_log_info(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INFO,
                        "msg",          "%s", "TEST: C_GSS_UDP_S refused the send while stopped, and hears and sends again",
                        NULL
                    );
                }
                set_yuno_must_die();
            }
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A datagram, as it came. The first one is kept: the next read of
 *  C_UDP_S then needs a new gbuffer.
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(gbuf) {
        size_t len = strlen(priv->received);
        snprintf(priv->received + len, sizeof(priv->received) - len, "%.*s ",
            (int)gbuffer_leftbytes(gbuf), (char *)gbuffer_cur_rd_pointer(gbuf)
        );
        if(!priv->kept) {
            priv->kept = gbuffer_incref(gbuf);
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A peer opened or closed: nothing to do
 ***************************************************************************/
PRIVATE int ac_channel(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
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
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_GSS_SELF_STOP);

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
        {EV_TIMEOUT,            ac_timeout,         0},
        {EV_ON_MESSAGE,         ac_on_message,      0},
        {EV_ON_OPEN,            ac_channel,         0},
        {EV_ON_CLOSE,           ac_channel,         0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
        {EV_ON_MESSAGE,         0},
        {EV_ON_OPEN,            0},
        {EV_ON_CLOSE,           0},
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
PUBLIC int register_c_test_gss_self_stop(void)
{
    return create_gclass(C_TEST_GSS_SELF_STOP);
}
