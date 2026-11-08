/***********************************************************************
 *          C_TEST10.C
 *
 *          A class to test C_TCP
 *          Test: the rest of the write of a dropped CLIENT goes to its own
 *          socket, never to a file that took the number of that socket.
 *
 *          The C_TCP connects to a listener of the test with a 4 KB
 *          receive buffer; the test accepts the connection (the peer) and
 *          does not read yet, and finds the number of the client's socket
 *          (the socket whose name is the peer's peer). The C_TCP is sent
 *          8 MB in chunks of 1 MB, so a write stays in flight, and 300 ms
 *          later it is DROPPED: the drop waits in ST_WAIT_STOPPED for that
 *          write. Right after the drop, a file of the test takes the number
 *          of the client's socket if that number is free. Then the peer
 *          reads, every 20 ms, until the end of the connection.
 *
 *          The client's socket must stay open until the stop ends, the file
 *          must receive nothing, the peer must receive whole chunks (the
 *          one in flight completed; the queued ones go away with the
 *          connection), and the number of the socket is closed when the
 *          stop ends (EV_DISCONNECTED).
 *
 *          Up to 7.30.17 the drop of a client closed its socket at once
 *          (the stop of its connect event), and a write completed short in
 *          ST_WAIT_STOPPED re-sent its rest with the number of that closed
 *          socket: into the file that took it (red: "the rest of the write
 *          of a dropped client went to another file"), or, with no file,
 *          to nothing (EBADF) and the data was lost.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "c_test10.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define CHUNKS          12
#define CHUNK_SIZE      (1024*1024)
#define DROP_AFTER_MS   300
#define READ_EVERY_MS   20
#define READ_BURST      (256*1024)
#define MAX_FD_SCAN     4096
#define WATCHDOG_MS     15000

enum {
    PHASE_CONNECT = 0,      // waiting the connection
    PHASE_FILL,             // the writes go, until the window closes
    PHASE_DRAIN,            // dropped: the peer reads until the end of the connection
    PHASE_END
};

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
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
    hgobj timer;            // the phases (seconds)
    hgobj read_timer;       // the reads of the peer (milliseconds)
    hgobj gobj_tcp;
    int fd_listen;
    int fd_peer;
    int fd_client;          // the number of the client's socket, found by its name
    int fd_file;            // the file opened right after the drop
    int phase;
    BOOL peer_ended;
    BOOL disconnected;
    size_t received;
    char *read_buffer;
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

    priv->timer = gobj_create_pure_child("phases", C_TIMER, 0, gobj);
    priv->read_timer = gobj_create_pure_child("reads", C_TIMER0, 0, gobj);
    priv->fd_peer = -1;
    priv->fd_client = -1;
    priv->fd_file = -1;
    priv->read_buffer = gbmem_malloc(READ_BURST);

    /*
     *  A listener of the test, on a port of the kernel's choice, with a
     *  small receive buffer (its connections inherit it)
     */
    char url[PATH_MAX] = "";
    priv->fd_listen = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t addrlen = sizeof(addr);
    int rcvbuf = 128*1024;  // more than a segment of the loopback: the window opens as the peer reads
    if(priv->fd_listen < 0 ||
            setsockopt(priv->fd_listen, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf))<0 ||
            bind(priv->fd_listen, (struct sockaddr *)&addr, sizeof(addr))<0 ||
            listen(priv->fd_listen, 4)<0 ||
            getsockname(priv->fd_listen, (struct sockaddr *)&addr, &addrlen)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot create the listener",
            NULL
        );
    } else {
        snprintf(url, sizeof(url), "tcp://127.0.0.1:%d", (int)ntohs(addr.sin_port));
    }

    priv->gobj_tcp = gobj_create(
        "tcp_client",
        C_TCP,
        json_pack("{s:s, s:i, s:i}",
            "url", url,
            "timeout_inactivity", -1,
            "timeout_between_connections", 60000
        ),
        gobj
    );
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->fd_peer >= 0) {
        close(priv->fd_peer);
        priv->fd_peer = -1;
    }
    if(priv->fd_file >= 0) {
        close(priv->fd_file);
        priv->fd_file = -1;
    }
    if(priv->fd_listen >= 0) {
        close(priv->fd_listen);
        priv->fd_listen = -1;
    }
    GBMEM_FREE(priv->read_buffer)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    gobj_start(priv->read_timer);

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
    clear_timeout0(priv->read_timer);
    gobj_stop(priv->read_timer);
    if(gobj_is_running(priv->gobj_tcp)) {
        gobj_stop(priv->gobj_tcp);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->gobj_tcp);
    set_timeout(priv->timer, WATCHDOG_MS);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    clear_timeout0(priv->read_timer);

    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_fail(hgobj gobj, const char *what)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", what,
        NULL
    );
}

/***************************************************************************
 *  Is `fd` the socket whose local name is `name`?
 ***************************************************************************/
PRIVATE BOOL is_the_socket(int fd, const struct sockaddr_in *name)
{
    struct stat st;
    if(fstat(fd, &st) < 0 || !S_ISSOCK(st.st_mode)) {
        return FALSE;
    }
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    if(getsockname(fd, (struct sockaddr *)&addr, &addrlen) < 0 || addr.sin_family != AF_INET) {
        return FALSE;
    }
    return addr.sin_port == name->sin_port && addr.sin_addr.s_addr == name->sin_addr.s_addr;
}

/***************************************************************************
 *  The peer: accepted by the test. The client's socket: the one whose
 *  name is the peer's peer
 ***************************************************************************/
PRIVATE int accept_the_peer(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->fd_peer = accept4(priv->fd_listen, NULL, NULL, SOCK_NONBLOCK);
    if(priv->fd_peer < 0) {
        test_fail(gobj, "TEST: cannot accept the peer");
        return -1;
    }
    struct sockaddr_in client_name;
    socklen_t addrlen = sizeof(client_name);
    if(getpeername(priv->fd_peer, (struct sockaddr *)&client_name, &addrlen) < 0) {
        test_fail(gobj, "TEST: getpeername() of the peer failed");
        return -1;
    }
    for(int fd = 3; fd < MAX_FD_SCAN; fd++) {
        if(fd != priv->fd_peer && is_the_socket(fd, &client_name)) {
            priv->fd_client = fd;
            return 0;
        }
    }
    test_fail(gobj, "TEST: the socket of the client not found");
    return -1;
}

/***************************************************************************
 *  Right after the drop: a file takes the number of the client's socket,
 *  if that number is free
 ***************************************************************************/
PRIVATE void open_the_file(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL number_free = (fcntl(priv->fd_client, F_GETFD) < 0 && errno == EBADF);

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/tmp/test_tcp_test10_XXXXXX");
    int fd = mkstemp(path);
    if(fd < 0) {
        test_fail(gobj, "TEST: cannot create the file");
        return;
    }
    unlink(path);   // open while the test runs, gone after it

    if(number_free && fd == priv->fd_client) {
        priv->fd_file = fd;
        test_fail(gobj, "TEST: the socket of a dropped client was closed before its stop ended");
    } else if(number_free) {
        /*
         *  The number is free: the socket was closed by the drop
         */
        if(dup2(fd, priv->fd_client) < 0) {
            test_fail(gobj, "TEST: dup2() of the file failed");
            close(fd);
            return;
        }
        close(fd);
        priv->fd_file = priv->fd_client;
        test_fail(gobj, "TEST: the socket of a dropped client was closed before its stop ended");
    } else {
        priv->fd_file = fd;
    }
}

/***************************************************************************
 *  The peer reads a burst: until there is nothing, or the end
 ***************************************************************************/
PRIVATE void read_a_burst(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    ssize_t n;
    while((n = recv(priv->fd_peer, priv->read_buffer, READ_BURST, MSG_DONTWAIT)) > 0) {
        priv->received += (size_t)n;
    }
    if(n == 0) {
        priv->peer_ended = TRUE;
    } else if(errno != EAGAIN && errno != EWOULDBLOCK) {
        priv->peer_ended = TRUE;    // a reset ends it too: the checks say what came
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "TEST: the peer's read ended with an error",
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
    }
}

/***************************************************************************
 *  The end: what the file and the peer got
 ***************************************************************************/
PRIVATE void check_the_end(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = PHASE_END;
    clear_timeout(priv->timer);
    clear_timeout0(priv->read_timer);

    struct stat st;
    if(priv->fd_file >= 0 && fstat(priv->fd_file, &st) == 0 && st.st_size > 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: the rest of the write of a dropped client went to another file",
            "file_bytes",   "%ld", (long)st.st_size,
            NULL
        );
    }
    if(priv->received == 0 || (priv->received % CHUNK_SIZE) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: the peer did not get whole chunks: the write in flight did not complete",
            "received",     "%ld", (long)priv->received,
            NULL
        );
    }
    if(priv->fd_file != priv->fd_client &&
            !(fcntl(priv->fd_client, F_GETFD) < 0 && errno == EBADF)) {
        test_fail(gobj, "TEST: the socket of the dropped client was not closed at the end of its stop");
    }

    set_yuno_must_die();
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Connected: 8 MB at once, more than the window and the buffers take
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase == PHASE_CONNECT) {
        priv->phase = PHASE_FILL;
        if(accept_the_peer(gobj) < 0) {
            set_yuno_must_die();
            KW_DECREF(kw)
            return 0;
        }
        for(int i = 0; i < CHUNKS; i++) {
            gbuffer_t *gbuf = gbuffer_create(CHUNK_SIZE, CHUNK_SIZE);
            memset(gbuffer_cur_wr_pointer(gbuf), 'x', CHUNK_SIZE);
            gbuffer_set_wr(gbuf, CHUNK_SIZE);
            gobj_send_event(
                priv->gobj_tcp,
                EV_TX_DATA,
                json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
                gobj
            );
        }
        set_timeout(priv->timer, DROP_AFTER_MS);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The end of the drop
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase != PHASE_DRAIN) {
        test_fail(gobj, "TEST: disconnected before the drop");
    }
    priv->disconnected = TRUE;
    if(priv->peer_ended) {
        check_the_end(gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The other events of the transport: nothing to do
 ***************************************************************************/
PRIVATE int ac_transport_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The timers: the reads of the peer (C_TIMER0) and the phases (C_TIMER)
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->read_timer) {
        if(priv->phase == PHASE_DRAIN) {
            if(!priv->peer_ended) {
                read_a_burst(gobj);
            }
            if(priv->peer_ended && priv->disconnected) {
                check_the_end(gobj);
            } else {
                set_timeout0(priv->read_timer, READ_EVERY_MS);
            }
        }
        JSON_DECREF(kw)
        return 0;
    }

    switch(priv->phase) {
        case PHASE_FILL:
            /*
             *  The drop: it must wait for the write in flight
             */
            priv->phase = PHASE_DRAIN;
            gobj_send_event(priv->gobj_tcp, EV_DROP, 0, gobj);
            if(!gobj_in_this_state(priv->gobj_tcp, ST_WAIT_STOPPED)) {
                test_fail(gobj, "TEST: the drop did not wait (not in ST_WAIT_STOPPED): nothing tested");
                set_yuno_must_die();
                break;
            }
            open_the_file(gobj);
            set_timeout(priv->timer, WATCHDOG_MS);
            set_timeout0(priv->read_timer, READ_EVERY_MS);
            break;

        case PHASE_DRAIN:
            test_fail(gobj, "TEST: the drop did not end");
            set_yuno_must_die();
            break;

        case PHASE_CONNECT:
            test_fail(gobj, "TEST: the C_TCP did not connect");
            set_yuno_must_die();
            break;

        default:
            test_fail(gobj, "TEST: unexpected phase");
            set_yuno_must_die();
            break;
    }

    JSON_DECREF(kw)
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
GOBJ_DEFINE_GCLASS(C_TEST10);

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
        {EV_CONNECTED,          ac_connected,       0},
        {EV_DISCONNECTED,       ac_disconnected,    0},
        {EV_STOPPED,            ac_transport_event, 0},
        {EV_RX_DATA,            ac_transport_event, 0},
        {EV_TX_READY,           ac_transport_event, 0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
        {EV_CONNECTED,          0},
        {EV_DISCONNECTED,       0},
        {EV_STOPPED,            0},
        {EV_RX_DATA,            0},
        {EV_TX_READY,           0},
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
        0,              // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,              // authz_table
        0,              // command_table
        s_user_trace_level,
        0               // gcflag_t
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
PUBLIC int register_c_test10(void)
{
    return create_gclass(C_TEST10);
}
