/****************************************************************************
 *          test_yevent_reload_stress.c
 *
 *          Stress variant of test_yevent_reload_live.
 *
 *          Hypothesis under test
 *          ---------------------
 *          A single long-lived TLS session must survive an arbitrarily
 *          large number of certificate reloads on the server side. Each
 *          reload drops the server's reference to the previous ctx/state,
 *          but the live sskt_server holds its own ref — so the old ctx
 *          is only actually freed when the session finally closes.
 *
 *          If ytls_reload_certificates() leaked an SSL_CTX or forgot to
 *          decref the mbedtls state, running this loop N times would
 *          accumulate N orphaned objects; `get_cur_system_memory()`
 *          would grow linearly, or valgrind would flag a leak.
 *
 *          Flow
 *          ----
 *          1. Generate 3 certs (A, B, C) once, up front.
 *          2. Start server with cert A, connect client, handshake.
 *          3. Loop RELOAD_ITERS times:
 *             - Reload server to cert A/B/C in round-robin.
 *             - Client sends a per-iteration MESSAGE, server echoes,
 *               client asserts the response matches.
 *          4. Assert messages_matched == RELOAD_ITERS.
 *          5. Teardown + gobj_end() + get_cur_system_memory() == 0.
 *
 *          Smoke test for the common case: RELOAD_ITERS is intentionally
 *          modest (50) so the test finishes quickly in CI. Point it at
 *          valgrind for the exhaustive check.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#define APP "test_yevent_reload_stress"

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
const char *server_url = "tcps://localhost:3335";

#define RELOAD_ITERS 50

#define TMP_DIR    "/tmp/ytls_reload_stress_test"
#define CERT_A     (TMP_DIR "/cert_A.pem")
#define KEY_A      (TMP_DIR "/key_A.pem")
#define CERT_B     (TMP_DIR "/cert_B.pem")
#define KEY_B      (TMP_DIR "/key_B.pem")
#define CERT_C     (TMP_DIR "/cert_C.pem")
#define KEY_C      (TMP_DIR "/key_C.pem")

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);
PRIVATE int yev_client_callback(yev_event_h yev_event);
PRIVATE int yev_server_callback(yev_event_h yev_event);
PRIVATE int generate_self_signed(const char *cert_path, const char *key_path, const char *cn);
PRIVATE void cleanup_tmp(void);

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

int messages_matched = 0;

/* Changes every iteration; client asserts echoes match. */
char expected_message[64];

/***************************************************************************
 *              Helpers
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
    return system(cmd) == 0 ? 0 : -1;
}

PRIVATE void cleanup_tmp(void)
{
    unlink(CERT_A); unlink(KEY_A);
    unlink(CERT_B); unlink(KEY_B);
    unlink(CERT_C); unlink(KEY_C);
    rmdir(TMP_DIR);
}

/***************************************************************************
 *  YTLS callbacks
 ***************************************************************************/
PRIVATE int ytls_server_on_handshake_done_callback(void *user_data, int error)
{
    server_secure_connected = TRUE;
    ytls_flush(ytls_server, sskt_server);
    return 0;
}

PRIVATE int ytls_client_on_handshake_done_callback(void *user_data, int error)
{
    client_secure_connected = TRUE;
    ytls_flush(ytls_client, sskt_client);
    return 0;
}

PRIVATE int ytls_server_on_clear_data_callback(void *user_data, gbuffer_t *gbuf)
{
    /* Server echoes whatever the client sent. */
    send_clear_data(ytls_server, sskt_server, gbuf);
    return 0;
}

PRIVATE int ytls_server_on_encrypted_data_callback(void *user_data, gbuffer_t *gbuf)
{
    int fd = (int)(size_t)user_data;
    yev_event_h yev_tx_msg = yev_create_write_event(
        yev_loop, yev_server_callback, NULL, fd, gbuf
    );
    yev_start_event(yev_tx_msg);
    return 0;
}

PRIVATE int ytls_client_on_clear_data_callback(void *user_data, gbuffer_t *gbuf)
{
    json_t *msg = gbuf2json(gbuffer_incref(gbuf), TRUE);
    const char *text = json_string_value(msg);

    if(!text || strcmp(text, expected_message) != 0) {
        printf("%sERROR%s <-- expected '%s', got '%s'\n",
            On_Red BWhite, Color_Off,
            expected_message, text ? text : "(null)");
        result += -1;
    } else {
        messages_matched++;
    }
    json_decref(msg);
    gbuffer_decref(gbuf);
    return 0;
}

PRIVATE int ytls_client_on_encrypted_data_callback(void *user_data, gbuffer_t *gbuf)
{
    int fd = (int)(size_t)user_data;
    yev_event_h yev_tx_msg = yev_create_write_event(
        yev_loop, yev_client_callback, NULL, fd, gbuf
    );
    yev_start_event(yev_tx_msg);
    return 0;
}

/***************************************************************************
 *  yev_loop callback
 ***************************************************************************/
PRIVATE int yev_loop_callback(yev_event_h yev_event) {
    if (!yev_event) return -1;
    return 0;
}

/***************************************************************************
 *  yev_loop callback   SERVER
 ***************************************************************************/
PRIVATE int yev_server_callback(yev_event_h yev_event)
{
    if(!yev_event) return -1;

    int ret = 0;
    yev_state_t yev_state = yev_get_state(yev_event);
    switch(yev_get_type(yev_event)) {
        case YEV_ACCEPT_TYPE:
            if(yev_state == YEV_ST_IDLE) {
                gbuffer_t *gbuf = gbuffer_create(1024, 1024);
                yev_server_reader_msg = yev_create_read_event(
                    yev_loop, yev_server_callback, NULL,
                    yev_get_result(yev_event_accept), gbuf
                );
                yev_start_event(yev_server_reader_msg);

                sskt_server = ytls_new_secure_filter(
                    ytls_server,
                    ytls_server_on_handshake_done_callback,
                    ytls_server_on_clear_data_callback,
                    ytls_server_on_encrypted_data_callback,
                    (void *)(uintptr_t)yev_get_result(yev_event_accept)
                );
                if(!sskt_server) ret = -1;
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
                ytls_decrypt_data(ytls_server, sskt_server, gbuffer_incref(gbuf_rx));
                gbuffer_clear(gbuf_rx);
                yev_start_event(yev_event);
            } else {
                yev_set_gbuffer(yev_event, NULL);
                ret = -1;
            }
            break;

        default:
            break;
    }

    return ret;
}

/***************************************************************************
 *  yev_loop callback   CLIENT
 ***************************************************************************/
PRIVATE int yev_client_callback(yev_event_h yev_event)
{
    if(!yev_event) return -1;

    int ret = 0;
    yev_state_t yev_state = yev_get_state(yev_event);
    switch(yev_get_type(yev_event)) {
        case YEV_CONNECT_TYPE:
            if(yev_state == YEV_ST_IDLE) {
                gbuffer_t *gbuf_rx = gbuffer_create(1024, 1024);
                yev_client_reader_msg = yev_create_read_event(
                    yev_loop, yev_client_callback, NULL,
                    yev_get_fd(yev_event_connect), gbuf_rx
                );
                yev_start_event(yev_client_reader_msg);

                sskt_client = ytls_new_secure_filter(
                    ytls_client,
                    ytls_client_on_handshake_done_callback,
                    ytls_client_on_clear_data_callback,
                    ytls_client_on_encrypted_data_callback,
                    (void *)(uintptr_t)yev_get_fd(yev_event_connect)
                );
                if(!sskt_client) ret = -1;
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
                ytls_decrypt_data(ytls_client, sskt_client, gbuffer_incref(gbuf_rx));
                gbuffer_clear(gbuf_rx);
                yev_start_event(yev_event);
            } else {
                yev_set_gbuffer(yev_event, NULL);
                ret = -1;
            }
            break;

        default:
            break;
    }

    return ret;
}

/***************************************************************************
 *              Test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    /*-------------------------*
     *  Certs A, B, C
     *-------------------------*/
    cleanup_tmp();
    if(mkdir(TMP_DIR, 0700) != 0) {
        fprintf(stderr, "mkdir FAILED\n");
        return -1;
    }
    if(generate_self_signed(CERT_A, KEY_A, "stress_A") != 0 ||
       generate_self_signed(CERT_B, KEY_B, "stress_B") != 0 ||
       generate_self_signed(CERT_C, KEY_C, "stress_C") != 0) {
        fprintf(stderr, "cert generation FAILED\n");
        cleanup_tmp();
        return -1;
    }

    /*-------------------------*
     *  ytls server / client
     *-------------------------*/
    json_t *jn_crypto_s = json_pack("{s:s, s:s, s:s, s:b}",
        "library", TLS_LIBRARY_NAME,
        "ssl_certificate",     CERT_A,
        "ssl_certificate_key", KEY_A,
        "trace_tls", 0
    );
    ytls_server = ytls_init(0, jn_crypto_s, TRUE);
    JSON_DECREF(jn_crypto_s);

    json_t *jn_crypto_c = json_pack("{s:s, s:b}",
        "library", TLS_LIBRARY_NAME,
        "trace_tls", 0
    );
    ytls_client = ytls_init(0, jn_crypto_c, FALSE);
    JSON_DECREF(jn_crypto_c);

    /*-------------------------*
     *  event loop + handshake
     *-------------------------*/
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

    if(!(server_secure_connected && client_secure_connected)) {
        fprintf(stderr, "FAIL: handshake did not complete\n");
        result += -1;
        goto teardown;
    }

    /*---------------------------------------------------------*
     *  Stress loop: reload + exchange per iteration.
     *  Round-robin across 3 certs so the old state being freed
     *  changes identity every step.
     *---------------------------------------------------------*/
    const char *certs[3][2] = {
        { CERT_A, KEY_A },
        { CERT_B, KEY_B },
        { CERT_C, KEY_C },
    };

    for(int i = 0; i < RELOAD_ITERS; i++) {
        const char *cert = certs[i % 3][0];
        const char *key  = certs[i % 3][1];

        json_t *jn_reload = json_pack("{s:s, s:s}",
            "ssl_certificate",     cert,
            "ssl_certificate_key", key
        );
        int rc = ytls_reload_certificates(ytls_server, jn_reload);
        JSON_DECREF(jn_reload);
        if(rc != 0) {
            fprintf(stderr, "FAIL: reload iter %d rc=%d\n", i, rc);
            result += -1;
            break;
        }

        snprintf(expected_message, sizeof(expected_message), "iter-%d", i);
        gbuffer_t *gbuf_tx = json2gbuf(0, json_string(expected_message), JSON_ENCODE_ANY);
        send_clear_data(ytls_client, sskt_client, gbuf_tx);
        yev_loop_run(yev_loop, 1);
    }

    if(messages_matched != RELOAD_ITERS) {
        fprintf(stderr, "FAIL: matched %d/%d messages across reloads\n",
            messages_matched, RELOAD_ITERS);
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

    if(sskt_server) ytls_free_secure_filter(ytls_server, sskt_server);
    if(sskt_client) ytls_free_secure_filter(ytls_client, sskt_client);

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

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    time_measure_t time_measure;
    MT_START_TIME(time_measure)

    result += do_test();

    MT_INCREMENT_COUNT(time_measure, RELOAD_ITERS)
    MT_PRINT_TIME(time_measure, APP)

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "system memory not free");
        print_track_mem();
        result += -1;
    }
    if(result < 0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    } else {
        printf("%s: PASS (%d reloads + %d messages)\n", APP, RELOAD_ITERS, messages_matched);
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
