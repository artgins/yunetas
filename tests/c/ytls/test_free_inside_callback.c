/****************************************************************************
 *          test_free_inside_callback.c
 *
 *  A ytls session freed by its owner from INSIDE a callback is not touched
 *  again: every call in progress returns -2222 on its way out.
 *
 *  The owner (C_TCP) ends its connection inside a callback when a write of
 *  the encrypted data cannot start: set_disconnected() frees the session
 *  (ytls_free_secure_filter()) and the callback answers -1. Here a client's
 *  on_encrypted_data_cb does exactly that, in two places:
 *
 *      1. the ClientHello of ytls_do_handshake(): it must answer -2222.
 *      2. a write after the handshake, ytls_encrypt_data(): -2222.
 *
 *  Up to 7.25.21 only on_clear_data_cb was covered: after the callback the
 *  backend read the freed session (the loop of flush_encrypted_data, the
 *  log of a failed callback, the state of the handshake after it). The
 *  memory freed is poisoned and kept in a quarantine (poison_alloc.c of the
 *  emailsender tests), so that read crashes instead of reading what was
 *  there.
 *
 *  Run for every backend compiled in (OpenSSL, mbedTLS).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#define APP "test_free_inside_callback"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <yuneta_config.h>
#include <gobj.h>
#include <kwid.h>
#include <gbuffer.h>
#include <glogger.h>
#include <helpers.h>
#include <ytls.h>
#include "poison_alloc.h"

/***************************************************************
 *              Constants
 ***************************************************************/
#define TMP_DIR   "/tmp/ytls_free_inside_callback"
#define CERT_PATH (TMP_DIR "/cert.pem")
#define KEY_PATH  (TMP_DIR "/key.pem")

/***************************************************************
 *              Data
 ***************************************************************/
typedef struct {
    hytls ytls;
    hsskt sskt;
    gbuffer_t *out;         // the encrypted bytes it sent
    int handshake_done;
    BOOL free_on_send;      // the next send frees the session (a write that cannot start)
} peer_t;

PRIVATE peer_t client;
PRIVATE peer_t server;

/***************************************************************
 *              Callbacks
 ***************************************************************/
PRIVATE int on_handshake_done(void *user_data, int error)
{
    peer_t *peer = user_data;
    if(error == 0) {
        peer->handshake_done = 1;
    }
    return 0;
}

PRIVATE int on_clear_data(void *user_data, gbuffer_t *gbuf)
{
    GBUFFER_DECREF(gbuf)
    return 0;
}

PRIVATE int on_encrypted_data(void *user_data, gbuffer_t *gbuf)
{
    peer_t *peer = user_data;
    if(peer->free_on_send) {
        /*
         *  What C_TCP does when the write cannot start: the end of the
         *  connection frees the session, inside this callback
         */
        peer->free_on_send = FALSE;
        ytls_free_secure_filter(peer->ytls, peer->sskt);
        peer->sskt = NULL;
        GBUFFER_DECREF(gbuf)
        return -1;
    }
    gbuffer_append_gbuf(peer->out, gbuf);
    GBUFFER_DECREF(gbuf)
    return 0;
}

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE int generate_self_signed(void)
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "openssl req -x509 -newkey rsa:2048 -nodes -sha256 -days 30 "
        "-keyout '%s' -out '%s' -subj '/CN=localhost' "
        "-addext 'subjectAltName=DNS:localhost' >/dev/null 2>&1",
        KEY_PATH, CERT_PATH
    );
    if(system(cmd) != 0) {
        return -1;
    }
    struct stat st;
    if(stat(CERT_PATH, &st) != 0 || st.st_size == 0) {
        return -1;
    }
    return 0;
}

/*
 *  The bytes one peer sent, into the other one
 */
PRIVATE void deliver(peer_t *from, peer_t *to)
{
    size_t len = gbuffer_leftbytes(from->out);
    if(!len || !to->sskt) {
        return;
    }
    gbuffer_t *gbuf = gbuffer_create(len, len);
    gbuffer_append(gbuf, gbuffer_get(from->out, len), len);
    gbuffer_clear(from->out);
    ytls_decrypt_data(to->ytls, to->sskt, gbuf);
}

PRIVATE int open_peers(const char *library)
{
    json_t *server_cfg = json_pack("{s:s, s:s, s:s}",
        "library",             library,
        "ssl_certificate",     CERT_PATH,
        "ssl_certificate_key", KEY_PATH
    );
    json_t *client_cfg = json_pack("{s:s, s:b}",
        "library",                      library,
        "ssl_allow_insecure_client",    1
    );
    memset(&client, 0, sizeof(client));
    memset(&server, 0, sizeof(server));
    server.ytls = ytls_init(0, server_cfg, TRUE);
    client.ytls = ytls_init(0, client_cfg, FALSE);
    JSON_DECREF(server_cfg)
    JSON_DECREF(client_cfg)
    if(!server.ytls || !client.ytls) {
        printf("%s: %s: ytls_init FAILED\n", APP, library);
        return -1;
    }
    server.out = gbuffer_create(64*1024, 64*1024);
    client.out = gbuffer_create(64*1024, 64*1024);
    server.sskt = ytls_new_secure_filter(server.ytls, on_handshake_done, on_clear_data, on_encrypted_data, &server);
    client.sskt = ytls_new_secure_filter(client.ytls, on_handshake_done, on_clear_data, on_encrypted_data, &client);
    if(!server.sskt || !client.sskt) {
        printf("%s: %s: ytls_new_secure_filter FAILED\n", APP, library);
        return -1;
    }
    return 0;
}

PRIVATE void close_peers(void)
{
    peer_t *peers[] = {&client, &server};
    for(int i = 0; i < 2; i++) {
        if(peers[i]->sskt) {
            ytls_free_secure_filter(peers[i]->ytls, peers[i]->sskt);
            peers[i]->sskt = NULL;
        }
        if(peers[i]->ytls) {
            ytls_cleanup(peers[i]->ytls);
            peers[i]->ytls = NULL;
        }
        GBUFFER_DECREF(peers[i]->out)
    }
}

PRIVATE int check(const char *library, const char *what, BOOL ok)
{
    printf("%s %s: %s\n", ok? "ok  " : "FAIL", library, what);
    return ok? 0 : -1;
}

/***************************************************************
 *              Tests
 ***************************************************************/
PRIVATE int test_backend(const char *library)
{
    int result = 0;

    /*
     *  1. The ClientHello's write cannot start
     */
    if(open_peers(library) < 0) {
        close_peers();
        return -1;
    }
    server.free_on_send = FALSE;
    ytls_do_handshake(server.ytls, server.sskt);
    client.free_on_send = TRUE;
    int ret = ytls_do_handshake(client.ytls, client.sskt);
    result += check(library, "a session freed in the ClientHello's callback: do_handshake answers -2222",
        ret == -2222 && client.sskt == NULL);
    close_peers();

    /*
     *  2. A write after the handshake cannot start
     */
    if(open_peers(library) < 0) {
        close_peers();
        return -1;
    }
    ytls_do_handshake(server.ytls, server.sskt);
    ytls_do_handshake(client.ytls, client.sskt);
    for(int i = 0; i < 10 && !(client.handshake_done && server.handshake_done); i++) {
        deliver(&client, &server);
        deliver(&server, &client);
    }
    result += check(library, "the handshake ends", client.handshake_done && server.handshake_done);
    if(client.handshake_done) {
        gbuffer_t *gbuf = gbuffer_create(64, 64);
        gbuffer_append_string(gbuf, "hello");
        client.free_on_send = TRUE;
        ret = ytls_encrypt_data(client.ytls, client.sskt, gbuf);
        result += check(library, "a session freed in the write's callback: encrypt_data answers -2222",
            ret == -2222 && client.sskt == NULL);
    }
    close_peers();

    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    install_poison_allocators();

    int result = 0;
    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    rmrdir(TMP_DIR);
    mkdir(TMP_DIR, 0700);
    if(generate_self_signed() != 0) {
        printf("%s: cert generation FAILED (is `openssl` installed?)\n", APP);
        result = -1;
    } else {
#ifdef CONFIG_HAVE_OPENSSL
        result += test_backend("openssl");
#endif
#ifdef CONFIG_HAVE_MBEDTLS
        result += test_backend("mbedtls");
#endif
    }
    rmrdir(TMP_DIR);

    gobj_end();
    release_quarantine();

    if(get_cur_system_memory() != 0) {
        printf("%s: system memory not free\n", APP);
        print_track_mem();
        result = -1;
    }
    printf("%s: %s\n", APP, result == 0? "PASS" : "FAIL");
    return result == 0? 0 : 1;
}
