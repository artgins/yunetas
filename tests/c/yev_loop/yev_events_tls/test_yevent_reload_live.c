/****************************************************************************
 *          test_yevent_reload_live.c
 *
 *          Graceful TLS certificate reload across a LIVE connection.
 *
 *          Setup
 *          -----
 *          - Generate two throw-away self-signed certs: A and B.
 *          - Start a TLS server with cert A and a TLS client.
 *          - Handshake once, exchange a first message.
 *
 *          Reload
 *          ------
 *          - Call ytls_reload_certificates() on the server with cert B while
 *            the (server-side) SSL session built with cert A is still alive.
 *
 *          Process
 *          -------
 *          - Exchange a SECOND message on the SAME live connection. The
 *            goal: the already-completed TLS session must continue to work
 *            after the reload, because OpenSSL holds a reference on the
 *            SSL_CTX inside SSL_new() (and our mbedtls backend mirrors this
 *            with explicit refcounts on the shared state). If the swap
 *            freed the old context, the second message would trigger a
 *            use-after-free / bad_record_mac.
 *
 *          Also verified: ytls_get_cert_info() on the server-side ytls
 *          AFTER the reload reports cert B (not cert A) — so NEW
 *          connections would use the new material.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#define APP "test_yevent_reload_live"

#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <kwid.h>
#include <gobj.h>
#include <ytls.h>
#include <testing.h>
#include <ansi_escape_codes.h>
#include <yev_loop.h>

/***************************************************************
 *              Constants
 ***************************************************************/
const char *server_url = "tcps://localhost:3334";
#define MESSAGE_1 "first-message-under-cert-A------"
#define MESSAGE_2 "second-message-after-cert-B-swap"

#define TMP_DIR "/tmp/ytls_reload_live_test"
#define CERT_A (TMP_DIR "/cert_A.pem")
#define KEY_A  (TMP_DIR "/key_A.pem")
#define CERT_B (TMP_DIR "/cert_B.pem")
#define KEY_B  (TMP_DIR "/key_B.pem")

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);
PRIVATE int yev_client_callback(yev_event_h yev_event);
PRIVATE int yev_server_callback(yev_event_h yev_event);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_h yev_loop;
yev_event_h yev_event_accept;
yev_event_h yev_event_connect;

yev_event_h yev_server_reader_msg = 0;
yev_event_h yev_client_reader_msg = 0;

int result = 0;

hytls ytls_server;
hsskt sskt_server;

hytls ytls_client;
hsskt sskt_client;

BOOL server_secure_connected = FALSE;
BOOL client_secure_connected = FALSE;

/* Match state: how many of the expected messages have been received intact. */
int messages_matched = 0;

/* Which message the client is currently expecting to receive. */
const char *expected_message = MESSAGE_1;

/***************************************************************************
 *              Test helpers
 ***************************************************************************/
PRIVATE int send_clear_data(hytls ytls, hsskt sskt, gbuffer_t *gbuf)
{
    if(ytls_encrypt_data(ytls, sskt, gbuf)<0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "ytls_encrypt_data() FAILED",
            "error",        "%s", ytls_get_last_error(ytls, sskt),
            NULL
        );
        return -1;
    }
    return 0;
}

PRIVATE int generate_self_signed(const char *cert_path, const char *key_path, const char *cn)
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "openssl req -x509 -newkey rsa:2048 -nodes -sha256 -days 365 "
        "-keyout '%s' -out '%s' -subj '/CN=%s' "
        ">/dev/null 2>&1",
        key_path, cert_path, cn
    );
    int rc = system(cmd);
    return rc == 0 ? 0 : -1;
}

PRIVATE void cleanup_tmp(void)
{
    unlink(CERT_A); unlink(KEY_A);
    unlink(CERT_B); unlink(KEY_B);
    rmdir(TMP_DIR);
}

/***************************************************************************
 *  YTLS callbacks
 ***************************************************************************/
PRIVATE int ytls_server_on_handshake_done_callback(void *user_data, int error)
{
    server_secure_connected = TRUE;
    gobj_info_msg(0, "Server: secure connected");
    ytls_flush(ytls_server, sskt_server);
    return 0;
}

PRIVATE int ytls_client_on_handshake_done_callback(void *user_data, int error)
{
    client_secure_connected = TRUE;
    gobj_info_msg(0, "Client: secure connected");
    ytls_flush(ytls_client, sskt_client);
    return 0;
}

PRIVATE int ytls_server_on_clear_data_callback(void *user_data, gbuffer_t *gbuf)
{
    /* Echo back whatever the client sent. */
    gobj_info_msg(0, "Server: query from the client");
    send_clear_data(ytls_server, sskt_server, gbuf);
    return 0;
}

PRIVATE int ytls_server_on_encrypted_data_callback(void *user_data, gbuffer_t *gbuf)
{
    int fd = (int)(size_t)user_data;
    yev_event_h yev_tx_msg = yev_create_write_event(
        yev_loop,
        yev_server_callback,
        NULL,
        fd,
        gbuf
    );
    yev_start_event(yev_tx_msg);
    return 0;
}

PRIVATE int ytls_client_on_clear_data_callback(void *user_data, gbuffer_t *gbuf)
{
    /* The server echoes; we match against the currently expected message. */
    json_t *msg = gbuf2json(gbuffer_incref(gbuf), TRUE);
    const char *text = json_string_value(msg);

    if(!text || strcmp(text, expected_message) != 0) {
        printf("%sERROR%s <-- expected '%s', got '%s'\n",
            On_Red BWhite, Color_Off,
            expected_message, text ? text : "(null)");
        result += -1;
    } else {
        messages_matched++;
        gobj_info_msg(0, "Client and Server messages MATCH");
    }
    json_decref(msg);
    gbuffer_decref(gbuf);
    return 0;
}

PRIVATE int ytls_client_on_encrypted_data_callback(void *user_data, gbuffer_t *gbuf)
{
    int fd = (int)(size_t)user_data;
    yev_event_h yev_tx_msg = yev_create_write_event(
        yev_loop,
        yev_client_callback,
        NULL,
        fd,
        gbuf
    );
    yev_start_event(yev_tx_msg);
    return 0;
}

/***************************************************************************
 *  yev_loop callback
 ***************************************************************************/
PRIVATE int yev_loop_callback(yev_event_h yev_event) {
    if (!yev_event) {
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  yev_loop callback   SERVER
 ***************************************************************************/
PRIVATE int yev_server_callback(yev_event_h yev_event)
{
    if(!yev_event) {
        return -1;
    }

    int ret = 0;
    yev_state_t yev_state = yev_get_state(yev_event);
    switch(yev_get_type(yev_event)) {
        case YEV_ACCEPT_TYPE:
            if(yev_state == YEV_ST_IDLE) {
                gbuffer_t *gbuf = gbuffer_create(1024, 1024);
                yev_server_reader_msg = yev_create_read_event(
                    yev_loop,
                    yev_server_callback,
                    NULL,
                    yev_get_result(yev_event_accept),
                    gbuf
                );
                yev_start_event(yev_server_reader_msg);

                sskt_server = ytls_new_secure_filter(
                    ytls_server,
                    ytls_server_on_handshake_done_callback,
                    ytls_server_on_clear_data_callback,
                    ytls_server_on_encrypted_data_callback,
                    (void *)(uintptr_t)yev_get_result(yev_event_accept)
                );
                if(!sskt_server) {
                    ret = -1;
                }
            } else {
                ret = -1;
            }
            break;

        case YEV_WRITE_TYPE:
            yev_destroy_event(yev_event);
            yev_event = NULL;
            break;

        case YEV_READ_TYPE:
            if(yev_state == YEV_ST_IDLE) {
                gbuffer_t *gbuf_rx = yev_get_gbuf(yev_event);
                if(ytls_decrypt_data(ytls_server, sskt_server, gbuffer_incref(gbuf_rx))<0) {
                    gobj_log_error(0, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING,
                        "msg",          "%s", "ytls_decrypt_data() FAILED",
                        NULL
                    );
                }
                gbuffer_clear(gbuf_rx);
                yev_start_event(yev_event);
            } else {
                yev_set_gbuffer(yev_event, NULL);
                ret = -1;
            }
            break;

        default:
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING,
                "msg",          "%s", "yev_event not implemented",
                "event_type",   "%s", yev_event_type_name(yev_event),
                NULL
            );
            break;
    }

    return ret;
}

/***************************************************************************
 *  yev_loop callback   CLIENT
 ***************************************************************************/
PRIVATE int yev_client_callback(yev_event_h yev_event)
{
    if(!yev_event) {
        return -1;
    }

    int ret = 0;
    yev_state_t yev_state = yev_get_state(yev_event);
    switch(yev_get_type(yev_event)) {
        case YEV_CONNECT_TYPE:
            if(yev_state == YEV_ST_IDLE) {
                gbuffer_t *gbuf_rx = gbuffer_create(1024, 1024);
                yev_client_reader_msg = yev_create_read_event(
                    yev_loop,
                    yev_client_callback,
                    NULL,
                    yev_get_fd(yev_event_connect),
                    gbuf_rx
                );
                yev_start_event(yev_client_reader_msg);

                sskt_client = ytls_new_secure_filter(
                    ytls_client,
                    ytls_client_on_handshake_done_callback,
                    ytls_client_on_clear_data_callback,
                    ytls_client_on_encrypted_data_callback,
                    (void *)(uintptr_t)yev_get_fd(yev_event_connect)
                );
                if(!sskt_client) {
                    ret = -1;
                }
            } else {
                ret = -1;
            }
            break;

        case YEV_WRITE_TYPE:
            yev_destroy_event(yev_event);
            yev_event = NULL;
            break;

        case YEV_READ_TYPE:
            if(yev_state == YEV_ST_IDLE) {
                gbuffer_t *gbuf_rx = yev_get_gbuf(yev_event);
                if(ytls_decrypt_data(ytls_client, sskt_client, gbuffer_incref(gbuf_rx))<0) {
                    gobj_log_error(0, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_LIBURING,
                        "msg",          "%s", "ytls_decrypt_data() FAILED",
                        NULL
                    );
                }
                gbuffer_clear(gbuf_rx);
                yev_start_event(yev_event);
            } else {
                yev_set_gbuffer(yev_event, NULL);
                ret = -1;
            }
            break;

        default:
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING,
                "msg",          "%s", "yev_event not implemented",
                "event_type",   "%s", yev_event_type_name(yev_event),
                NULL
            );
            break;
    }

    return ret;
}

/***************************************************************************
 *              Test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    /*--------------------------------*
     *  Fresh tmp dir + certs A, B
     *--------------------------------*/
    cleanup_tmp();
    if(mkdir(TMP_DIR, 0700) != 0) {
        fprintf(stderr, "mkdir(%s) failed\n", TMP_DIR);
        return -1;
    }
    if(generate_self_signed(CERT_A, KEY_A, "ytls_live_A") != 0 ||
       generate_self_signed(CERT_B, KEY_B, "ytls_live_B") != 0) {
        fprintf(stderr, "cert generation FAILED (is `openssl` installed?)\n");
        cleanup_tmp();
        return -1;
    }

    /*--------------------------------*
     *  ssl SERVER with cert A
     *--------------------------------*/
    json_t *jn_crypto_s = json_pack("{s:s, s:s, s:s, s:b}",
        "library", TLS_LIBRARY_NAME,
        "ssl_certificate",     CERT_A,
        "ssl_certificate_key", KEY_A,
        "trace_tls", 0
    );
    ytls_server = ytls_init(0, jn_crypto_s, TRUE);
    JSON_DECREF(jn_crypto_s)

    /*--------------------------------*
     *  ssl CLIENT (no verification)
     *--------------------------------*/
    json_t *jn_crypto_c = json_pack("{s:s, s:b}",
        "library", TLS_LIBRARY_NAME,
        "trace_tls", 0
    );
    ytls_client = ytls_init(0, jn_crypto_c, FALSE);
    JSON_DECREF(jn_crypto_c)

    /*--------------------------------*
     *  event loop + listen + connect
     *--------------------------------*/
    yev_loop_create(0, 2024, 10, yev_loop_callback, &yev_loop);

    yev_event_accept = yev_create_accept_event(
        yev_loop, yev_server_callback,
        server_url, 0, FALSE, AF_INET, AI_ADDRCONFIG, 0
    );
    yev_start_event(yev_event_accept);
    yev_loop_run(yev_loop, 1);

    yev_event_connect = yev_create_connect_event(
        yev_loop, yev_client_callback,
        server_url, NULL, AF_INET, AI_ADDRCONFIG, 0
    );
    yev_start_event(yev_event_connect);
    yev_loop_run(yev_loop, 1);

    /*-------------------------------------------------------*
     *  PHASE 1: client sends MESSAGE_1 under cert A
     *-------------------------------------------------------*/
    if(!(server_secure_connected && client_secure_connected)) {
        fprintf(stderr, "FAIL: TLS handshake did not complete\n");
        result += -1;
        goto teardown;
    }

    expected_message = MESSAGE_1;
    {
        gbuffer_t *gbuf_tx = json2gbuf(0, json_string(MESSAGE_1), JSON_ENCODE_ANY);
        send_clear_data(ytls_client, sskt_client, gbuf_tx);
        yev_loop_run(yev_loop, 1);
    }
    if(messages_matched != 1) {
        fprintf(stderr, "FAIL: first message exchange did not complete (matched=%d)\n",
            messages_matched);
        result += -1;
        goto teardown;
    }

    /*-------------------------------------------------------*
     *  RELOAD: swap server cert A -> B WITHOUT tearing down
     *  the live session. The old SSL_CTX (for OpenSSL) or
     *  the old shared state (for mbedTLS) must stay alive
     *  via refcount until the session closes.
     *-------------------------------------------------------*/
    {
        json_t *jn_reload = json_pack("{s:s, s:s}",
            "ssl_certificate",     CERT_B,
            "ssl_certificate_key", KEY_B
        );
        int rc = ytls_reload_certificates(ytls_server, jn_reload);
        JSON_DECREF(jn_reload);
        if(rc != 0) {
            fprintf(stderr, "FAIL: ytls_reload_certificates returned %d\n", rc);
            result += -1;
            goto teardown;
        }

        /* NEW connections would see cert B. */
        json_t *info = ytls_get_cert_info(ytls_server);
        const char *subject = kw_get_str(0, info, "subject", "", 0);
        if(!subject || !strstr(subject, "ytls_live_B")) {
            fprintf(stderr,
                "FAIL: after reload, ytls_get_cert_info subject='%s' (expected 'ytls_live_B')\n",
                subject ? subject : "(null)");
            result += -1;
        }
        JSON_DECREF(info);
    }

    /*-------------------------------------------------------*
     *  PHASE 2: client sends MESSAGE_2 on the SAME session.
     *  If the reload disrupted the live session (use-after-
     *  free on the old ctx, or handshake invalidated), this
     *  exchange will fail and the match count stays at 1.
     *-------------------------------------------------------*/
    expected_message = MESSAGE_2;
    {
        gbuffer_t *gbuf_tx = json2gbuf(0, json_string(MESSAGE_2), JSON_ENCODE_ANY);
        send_clear_data(ytls_client, sskt_client, gbuf_tx);
        yev_loop_run(yev_loop, 1);
    }
    if(messages_matched != 2) {
        fprintf(stderr,
            "FAIL: second message exchange (post-reload) did not complete (matched=%d)\n",
            messages_matched);
        result += -1;
    }

teardown:
    yev_stop_event(yev_client_reader_msg);
    yev_stop_event(yev_server_reader_msg);
    yev_stop_event(yev_event_connect);
    yev_stop_event(yev_event_accept);
    yev_loop_run(yev_loop, 1);

    yev_destroy_event(yev_client_reader_msg);
    yev_destroy_event(yev_server_reader_msg);
    yev_destroy_event(yev_event_accept);
    yev_destroy_event(yev_event_connect);

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    if(sskt_server) {
        ytls_free_secure_filter(ytls_server, sskt_server);
    }
    if(sskt_client) {
        ytls_free_secure_filter(ytls_client, sskt_client);
    }

    EXEC_AND_RESET(ytls_cleanup, ytls_server)
    EXEC_AND_RESET(ytls_cleanup, ytls_client)

    cleanup_tmp();
    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(
        argc, argv,
        NULL, NULL, NULL, NULL, NULL, NULL
    );

    yuno_catch_signals();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("testing", 0, capture_log_write, 0);
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    const char *test = APP;
    /*
     *  The test runs two message exchanges — one before the cert reload
     *  (MESSAGE_1) and one after (MESSAGE_2) on the SAME live session —
     *  so "Server: query from the client" and "Client and Server messages
     *  MATCH" each fire twice, sandwiched around "TLS certificates
     *  reloaded". The expected-log FIFO must mirror that order exactly.
     */
    json_t *error_list = json_pack(
        "[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
        "msg", "Client: secure connected",
        "msg", "Server: secure connected",
        "msg", "Server: query from the client",
        "msg", "Client and Server messages MATCH",
        "msg", "TLS certificates reloaded",
        "msg", "Server: query from the client",
        "msg", "Client and Server messages MATCH"
    );

    set_expected_results(test, error_list, NULL, NULL, 1);

    time_measure_t time_measure;
    MT_START_TIME(time_measure)

    result += do_test();

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)

    result += test_json(NULL);

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "system memory not free");
        print_track_mem();
        result += -1;
    }
    if(result < 0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    }
    return result < 0 ? -1 : 0;
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int xtimes_once = 0;
    xtimes_once++;
    yev_loop_reset_running(yev_loop);
    if(xtimes_once > 1) {
        exit(-1);
    }
}

PUBLIC void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_NODEFER | SA_RESTART;
    sigaction(SIGALRM, &sigIntHandler, NULL);
    sigaction(SIGTERM, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    signal(SIGPIPE, SIG_IGN);
}
