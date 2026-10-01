/***********************************************************************
 *          C_TEST6.C
 *
 *          A class to test C_TCP_S: a TLS server (child_tree_filter) stopped
 *          and started again while a connection of it lives, and its
 *          certificates reloaded.
 *
 *          A connection accepted with child_tree_filter outlives a stop of
 *          its listener, and its clisrv keeps using the ytls of the server
 *          (its pointer, and its TLS session's). Up to 7.25.20 the start
 *          after the stop freed that ytls (ytls_cleanup) and made a new
 *          one: the next record of the live connection was decrypted with
 *          freed memory. main_test6.c poisons freed memory, so that read
 *          fails every time.
 *
 *          Tasks
 *          - Play pepon (a TLS echo server) and open __output_side__
 *          - On open, send "one"
 *          - On its echo: stop the C_TCP_S and start it again in the same
 *            turn, and reload its certificates (reload-certs)
 *          - 300 ms later, send "two" on the connection that lived across
 *            it
 *          - On its echo, close it, and open __output_side2__, a new
 *            connection; on its open, send "three"
 *          - "two" must come back on the old connection. Then that
 *            connection is closed (its tree stopped: no reconnection), so
 *            the echo of "three" can only go to the new one (pepon's gate
 *            echoes to one open channel), and it must come back on it
 *          - The restart reloaded no certificate (they did not change: no
 *            "TLS certificates reloaded"), reload-certs one. Then the
 *            certificate file is touched, and a stop and a start of the
 *            server reload it: two in all
 *
 *          The server's certificates are copies, in a dir of this run, so
 *          the test can touch them.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <c_pepon.h>
#include "c_test6.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void send_text(hgobj gobj, hgobj gate, const char *text);
PRIVATE void restart_server(hgobj gobj);
PRIVATE void test_fail(hgobj gobj, const char *what);
PRIVATE int copy_file(const char *from, const char *to);
PRIVATE void use_cert_copies(hgobj gobj);
PRIVATE void touch_cert(hgobj gobj);

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
    hgobj pepon;
    hgobj gobj_output_side;
    hgobj gobj_output_side2;
    int phase;
    char got1[128];     // the echoes of the connection that lives across the restart
    char got2[128];     // the echoes of the connection made after it
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
     *  Copies of the test certificates, that the test can touch
     */
    snprintf(priv->cert_dir, sizeof(priv->cert_dir), "/tmp/test_tcps_test6.XXXXXX");
    if(!mkdtemp(priv->cert_dir)) {
        priv->cert_dir[0] = 0;
        test_fail(gobj, "TEST: cannot create the dir of the certificate copies");
    } else {
        build_path(priv->cert, sizeof(priv->cert), priv->cert_dir, "localhost.crt", NULL);
        build_path(priv->key, sizeof(priv->key), priv->cert_dir, "localhost.key", NULL);
        if(copy_file("/yuneta/agent/certs/localhost.crt", priv->cert) < 0 ||
                copy_file("/yuneta/agent/certs/localhost.key", priv->key) < 0) {
            test_fail(gobj, "TEST: cannot copy the test certificates");
        }
    }

    priv->pepon = gobj_create_pure_child(
        "server",
        C_PEPON,
        json_pack("{s:b}", "do_echo", 1),
        gobj
    );
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

    if(!gobj_is_running(priv->pepon)) {
        gobj_start(priv->pepon);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->pepon);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    use_cert_copies(gobj);
    gobj_play(priv->pepon);

    priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);
    gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);
    priv->gobj_output_side2 = gobj_find_service("__output_side2__", TRUE);
    gobj_subscribe_event(priv->gobj_output_side2, NULL, 0, gobj);

    set_timeout(priv->timer, 500);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_pause(priv->pepon);

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
PRIVATE void send_text(hgobj gobj, hgobj gate, const char *text)
{
    gbuffer_t *gbuf = gbuffer_create(strlen(text), strlen(text));
    gbuffer_append_string(gbuf, text);
    gobj_send_event(
        gate,
        EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        gobj
    );
}

/***************************************************************************
 *  The server stopped and started again in the same turn, its certificates
 *  reloaded: the connection of __output_side__ lives across it
 ***************************************************************************/
PRIVATE void restart_server(hgobj gobj)
{
    hgobj input_side = gobj_find_service("__input_side__", TRUE);
    hgobj server = gobj_find_child(input_side, json_pack("{s:s}", "__gclass_name__", C_TCP_S));
    if(!server) {
        test_fail(gobj, "TEST: C_TCP_S not found");
        return;
    }

    gobj_stop(server);
    gobj_start(server);

    json_t *jn_resp = gobj_command(server, "reload-certs", json_object(), gobj);
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
        test_fail(gobj, "TEST: reload-certs FAILED");
    }
    JSON_DECREF(jn_resp)
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_fail(hgobj gobj, const char *what)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", what,
        "got1",         "%s", priv->got1,
        "got2",         "%s", priv->got2,
        NULL
    );
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
 *  The server listens with the certificate copies
 ***************************************************************************/
PRIVATE void use_cert_copies(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj input_side = gobj_find_service("__input_side__", TRUE);
    hgobj server = gobj_find_child(input_side, json_pack("{s:s}", "__gclass_name__", C_TCP_S));
    json_t *jn_crypto = json_deep_copy(gobj_read_json_attr(server, "crypto"));
    json_object_set_new(jn_crypto, "ssl_certificate", json_string(priv->cert));
    json_object_set_new(jn_crypto, "ssl_certificate_key", json_string(priv->key));
    gobj_write_json_attr(server, "crypto", jn_crypto);
    JSON_DECREF(jn_crypto)
}

/***************************************************************************
 *  The certificate "renewed": its mtime moves
 ***************************************************************************/
PRIVATE void touch_cert(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    struct timespec times[2];
    clock_gettime(CLOCK_REALTIME, &times[0]);
    times[0].tv_sec += 10;
    times[1] = times[0];
    if(utimensat(AT_FDCWD, priv->cert, times, 0) < 0) {
        test_fail(gobj, "TEST: cannot touch the certificate copy");
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  A connection opened: its first message
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_output_side) {
        send_text(gobj, priv->gobj_output_side, "one");
    } else if(src == priv->gobj_output_side2) {
        send_text(gobj, priv->gobj_output_side2, "three");
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  An echo
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    char *got = (src == priv->gobj_output_side)? priv->got1 : priv->got2;
    if(gbuf) {
        size_t len = strlen(got);
        snprintf(got + len, sizeof(priv->got1) - len, "%.*s ",
            (int)gbuffer_leftbytes(gbuf), (char *)gbuffer_cur_rd_pointer(gbuf)
        );
    }

    if(priv->phase == 1 && strcmp(priv->got1, "one ")==0) {
        restart_server(gobj);
        priv->phase = 2;
        set_timeout(priv->timer, 300);

    } else if(priv->phase == 3 && src == priv->gobj_output_side && strstr(priv->got1, "two")) {
        /*
         *  "two" back on the old connection: it is closed, and a new one
         *  opened
         */
        gobj_stop_tree(priv->gobj_output_side);
        gobj_start_tree(priv->gobj_output_side2);
        priv->phase = 4;
        set_timeout(priv->timer, 3000);

    } else if(priv->phase == 4 && src == priv->gobj_output_side2 && strstr(priv->got2, "three")) {
        /*
         *  "three" back on the new connection. The restart reloaded nothing
         *  (the certificates did not change), reload-certs once. Now the
         *  certificate changes: a restart reloads it.
         */
        if(test6_reloads != 1) {
            test_fail(gobj, "TEST: a restart with the same certificates reloaded them (or reload-certs did not)");
        }
        touch_cert(gobj);
        hgobj input_side = gobj_find_service("__input_side__", TRUE);
        hgobj server = gobj_find_child(input_side, json_pack("{s:s}", "__gclass_name__", C_TCP_S));
        gobj_stop(server);
        gobj_start(server);
        priv->phase = 5;
        set_timeout(priv->timer, 300);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A connection closed: nothing to do
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

    switch(priv->phase) {
        case 0:
            gobj_start_tree(priv->gobj_output_side);
            priv->phase = 1;
            set_timeout(priv->timer, 3000);     // the connect and the echo must not take this long
            break;

        case 2:
            send_text(gobj, priv->gobj_output_side, "two");
            priv->phase = 3;
            set_timeout(priv->timer, 3000);
            break;

        case 5:
            if(test6_reloads != 2) {
                test_fail(gobj, "TEST: a restart after the certificate changed did not reload it");
            } else {
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INFO,
                    "msg",          "%s", "TEST: the old connection and a new one work after the restart, and only changed certificates are reloaded",
                    NULL
                );
            }
            set_yuno_must_die();
            break;

        default:
            test_fail(gobj, "TEST: an echo did not come back");
            set_yuno_must_die();
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
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
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST6);

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
        {EV_ON_MESSAGE,             ac_on_message,              0},
        {EV_ON_CLOSE,               ac_on_close,                0},
        {EV_STOPPED,                ac_stopped,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_OPEN,                0},
        {EV_ON_MESSAGE,             0},
        {EV_ON_CLOSE,               0},
        {EV_TIMEOUT,                0},
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
PUBLIC int register_c_test6(void)
{
    return create_gclass(C_TEST6);
}
