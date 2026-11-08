/***********************************************************************
 *          C_TEST14.C
 *
 *          A class to test C_TCP / C_TCP_S
 *          Test: a TLS server whose certificate cannot be loaded does not
 *          listen, the yuno goes on, and it listens when the certificate
 *          is there
 *
 *          The gate of the agent in small: C_TCP_S servers share one pool of
 *          channels, `plain_port`, `tls_port` and `tls_port2`. The
 *          certificate and key of the two TLS servers name files of a dir of
 *          the run that do not exist yet.
 *          Up to 7.30.17 ytls exited the yuno with code 0 at the start
 *          (LOG_OPT_EXIT_ZERO): an agent without its pair died, and
 *          on a sealed node only SSH could bring it back. Up to 7.30.14 the
 *          mbedTLS backend listened with no TLS context, and the first peer
 *          crashed the yuno.
 *
 *          Tasks
 *          - 300 ms: the yuno lives; `tls_port` is not listening (its port
 *            refuses a connection, `listening` and `tls_ready` false),
 *            `plain_port` is (a raw socket connects). `reload-certs` with
 *            the files still missing answers -1.
 *          - 4 s later: the certificate and key are put in place. Between the
 *            two, a try of the timer found nothing changed: it must load
 *            nothing and say nothing (the logs are checked in order).
 *            `reload-certs` of `tls_port2` loads them at once: it answers 0
 *            and listens, while `tls_port` waits for its timer.
 *          - each 500 ms: when `tls_port` listens (its timer), a TLS client
 *            connects; the end is its channel opened (the handshake
 *            ended), without a restart of anything.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "c_test14.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define POLLS_MAX   24      // each 500 ms (a tick of 1 s): the try that loads the certificate is 4 s after the copy

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void check(hgobj gobj, const char *what, BOOL ok);
PRIVATE int connect_to(int port);
PRIVATE int copy_file(const char *from, const char *to);
PRIVATE hgobj find_server(const char *name);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
int test14_reached_end = 0;
int test14_failed = 0;

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
    hgobj gobj_input_side;
    hgobj gobj_output_side;
    int phase;
    int polls;
    BOOL client_started;
    char cert_dir[PATH_MAX];
    char cert[PATH_MAX];
    char key[PATH_MAX];
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

    /*
     *  The dir of the certificate of tls_port, empty: the files come later
     */
    snprintf(priv->cert_dir, sizeof(priv->cert_dir), "/tmp/%s.XXXXXX", TEST14_APP_NAME);
    if(!mkdtemp(priv->cert_dir)) {
        priv->cert_dir[0] = 0;
        check(gobj, "TEST: cannot create the dir of the certificate", FALSE);
        return;
    }
    if(!build_path(priv->cert, sizeof(priv->cert), priv->cert_dir, "localhost.crt", NULL) ||
        !build_path(priv->key, sizeof(priv->key), priv->cert_dir, "localhost.key", NULL)
    ) {
        check(gobj, "TEST: cannot build the paths of the certificate", FALSE);
    }
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->cert_dir[0]) {
        unlink(priv->cert);
        unlink(priv->key);
        rmdir(priv->cert_dir);
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

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  The TLS servers name the files of the dir of the run, not there yet
     */
    const char *servers[] = {"tls_port", "tls_port2", 0};
    for(int i=0; servers[i]; i++) {
        hgobj server = find_server(servers[i]);
        json_t *jn_crypto = json_deep_copy(gobj_read_json_attr(server, "crypto"));
        json_object_set_new(jn_crypto, "ssl_certificate", json_string(priv->cert));
        json_object_set_new(jn_crypto, "ssl_certificate_key", json_string(priv->key));
        gobj_write_json_attr(server, "crypto", jn_crypto);
        JSON_DECREF(jn_crypto)
    }

    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

    priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);
    gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);

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
 *
 ***************************************************************************/
PRIVATE void check(hgobj gobj, const char *what, BOOL ok)
{
    if(!ok) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL",
            "what",         "%s", what,
            NULL
        );
        test14_failed++;
    }
}

/***************************************************************************
 *  A raw socket to 127.0.0.1:<port>; it sends nothing. -1: refused
 ***************************************************************************/
PRIVATE int connect_to(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM|SOCK_CLOEXEC, 0);
    if(fd < 0) {
        return -1;  // the test says it
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;  // the test says it
    }
    return fd;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int copy_file(const char *from, const char *to)
{
    FILE *in = fopen(from, "rb");
    if(!in) {
        return -1;
    }
    FILE *out = fopen(to, "wb");
    if(!out) {
        fclose(in);
        return -1;
    }
    char bf[4096];
    size_t n;
    while((n = fread(bf, 1, sizeof(bf), in)) > 0) {
        fwrite(bf, 1, n, out);
    }
    fclose(in);
    fclose(out);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE hgobj find_server(const char *name)
{
    hgobj gate = gobj_find_service("__input_side__", TRUE);
    return gobj_find_child(gate, json_pack("{s:s, s:s}",
        "__gclass_name__", C_TCP_S,
        "__gobj_name__", name
    ));
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  A channel opened: the one of the TLS client ends the test
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_output_side && !test14_reached_end) {
        hgobj tls_port = find_server("tls_port");
        check(gobj, "tls_port says it listens", gobj_read_bool_attr(tls_port, "listening"));
        check(gobj, "tls_port says its TLS context is loaded", gobj_read_bool_attr(tls_port, "tls_ready"));

        test14_reached_end = 1;
        clear_timeout(priv->timer);
        gobj_stop_tree(priv->gobj_output_side);
        set_yuno_must_die();
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The phases
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj tls_port = find_server("tls_port");
    hgobj plain_port = find_server("plain_port");

    priv->phase++;

    switch(priv->phase) {
        case 1:
            {
                /*
                 *  The yuno lives, plain_port listens, tls_port does not
                 */
                check(gobj, "plain_port listens", gobj_read_bool_attr(plain_port, "listening"));
                check(gobj, "tls_port does not listen", !gobj_read_bool_attr(tls_port, "listening"));
                check(gobj, "tls_port has no TLS context", !gobj_read_bool_attr(tls_port, "tls_ready"));
                check(gobj, "tls_port is a TLS url", gobj_read_bool_attr(tls_port, "use_ssl"));

                int fd = connect_to(atoi(TEST14_PORT_PLAIN));
                check(gobj, "a peer connects to plain_port", fd >= 0);
                if(fd >= 0) {
                    close(fd);
                }
                fd = connect_to(atoi(TEST14_PORT_TLS));
                check(gobj, "tls_port refuses a peer: no TLS context, no listen", fd < 0);
                if(fd >= 0) {
                    close(fd);
                }

                json_t *jn_resp = gobj_command(tls_port, "reload-certs", json_object(), gobj);
                check(gobj, "reload-certs without the files answers -1",
                    kw_get_int(gobj, jn_resp, "result", 0, 0) < 0
                );
                JSON_DECREF(jn_resp)
                check(gobj, "tls_port still does not listen", !gobj_read_bool_attr(tls_port, "listening"));
                check(gobj, "tls_port2 does not listen", !gobj_read_bool_attr(find_server("tls_port2"), "listening"));

                /*
                 *  The timer of the yuno ticks each second: in 4 s the
                 *  try of tls_port (due 2 s after reload-certs) comes
                 *  before the copy, and finds nothing changed
                 */
                set_timeout(priv->timer, 4000);
            }
            break;

        case 2:
            {
                /*
                 *  The certificate and key are put in place: reload-certs
                 *  loads them at once in tls_port2, tls_port waits for its
                 *  timer
                 */
                if(copy_file("/yuneta/agent/certs/localhost.crt", priv->cert) < 0 ||
                        copy_file("/yuneta/agent/certs/localhost.key", priv->key) < 0) {
                    check(gobj, "TEST: cannot copy the test certificates", FALSE);
                }
                hgobj tls_port2 = find_server("tls_port2");
                json_t *jn_resp = gobj_command(tls_port2, "reload-certs", json_object(), gobj);
                check(gobj, "reload-certs with the files answers 0",
                    kw_get_int(gobj, jn_resp, "result", -1, 0) == 0
                );
                JSON_DECREF(jn_resp)
                check(gobj, "tls_port2 listens after reload-certs", gobj_read_bool_attr(tls_port2, "listening"));
                check(gobj, "tls_port2 has its TLS context", gobj_read_bool_attr(tls_port2, "tls_ready"));
                check(gobj, "tls_port waits for its timer", !gobj_read_bool_attr(tls_port, "listening"));
                int fd = connect_to(atoi(TEST14_PORT_TLS2));
                check(gobj, "a peer connects to tls_port2", fd >= 0);
                if(fd >= 0) {
                    close(fd);
                }
                set_timeout(priv->timer, 500);
            }
            break;

        default:
            /*
             *  Waiting for tls_port to listen, then for the TLS client
             */
            priv->polls++;
            if(priv->polls > POLLS_MAX) {
                check(gobj, priv->client_started?
                    "the TLS client did not open its channel" :
                    "tls_port did not listen once its certificate was there",
                    FALSE
                );
                test14_reached_end = 1;
                set_yuno_must_die();
                break;
            }
            if(!priv->client_started && gobj_read_bool_attr(tls_port, "listening")) {
                priv->client_started = TRUE;
                gobj_start_tree(priv->gobj_output_side);
            }
            set_timeout(priv->timer, 500);
            break;
    }

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
GOBJ_DEFINE_GCLASS(C_TEST14);

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
PUBLIC int register_c_test14(void)
{
    return create_gclass(C_TEST14);
}
