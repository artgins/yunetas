/****************************************************************************
 *          test_session_tickets.c
 *
 *          A TLS client writes after the TLS 1.3 server sent its session
 *          tickets. The server is OpenSSL's backend, which sends two
 *          tickets after each handshake by default (as nginx, Python and
 *          a C_TCP_S with OpenSSL do). The client is each backend compiled
 *          in, so with both the case CROSSES the libraries: an mbedTLS
 *          client against an OpenSSL server.
 *
 *          The handshake runs by pumping the encrypted bytes between a
 *          client filter and a server filter (no sockets). The tickets
 *          reach the client before its first write, as they do to a client
 *          that waits a moment after connecting. Then:
 *              1. the server sent bytes after the client ended its
 *                 handshake (the tickets);
 *              2. the client's write succeeds and the server reads it;
 *              3. the server's answer reaches the client;
 *              4. a second write of the client succeeds too.
 *          In 7.30.20 (since 7.28.0) the mbedTLS client failed 2: its
 *          mbedtls_ssl_write() answered MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET,
 *          taken for a fatal error ("mbedtls_ssl_write() FAILED"), and the
 *          link did not pass a byte.
 *
 *          Needs the OpenSSL backend (the server); skipped without it.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#define APP "test_session_tickets"

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <yuneta_config.h>
#include <gobj.h>
#include <kwid.h>
#include <helpers.h>
#include <gbuffer.h>
#include <glogger.h>
#include <ytls.h>

#ifndef CONFIG_HAVE_OPENSSL

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    printf("%s: SKIP (CONFIG_HAVE_OPENSSL not set: the server is OpenSSL's)\n", APP);
    return 0;
}

#else /* CONFIG_HAVE_OPENSSL */

/***************************************************************
 *              Constants
 ***************************************************************/
#define TMP_DIR     "/tmp/ytls_session_tickets"
#define CERT_PATH   TMP_DIR "/cert.pem"
#define KEY_PATH    TMP_DIR "/key.pem"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int global_result = 0;

PRIVATE int tag_client = 0;
PRIVATE int tag_server = 1;

PRIVATE gbuffer_t *cli_to_srv = NULL;   /* encrypted bytes client -> server */
PRIVATE gbuffer_t *srv_to_cli = NULL;   /* encrypted bytes server -> client */

PRIVATE int hs_done[2]  = {0, 0};       /* [tag] handshake callback fired */
PRIVATE int hs_error[2] = {0, 0};       /* [tag] last handshake error */

PRIVATE char clear_read[2][256];        /* [tag] clear data read, appended */

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int on_handshake_done(void *user_data, int error);
PRIVATE int on_clear_data(void *user_data, gbuffer_t *gbuf);
PRIVATE int on_encrypted_data(void *user_data, gbuffer_t *gbuf);

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void ok_or_fail(int cond, const char *library, const char *name)
{
    if(cond) {
        printf("ok   [%s client, openssl server] %s\n", library, name);
    } else {
        printf("FAIL [%s client, openssl server] %s\n", library, name);
        global_result += -1;
    }
}

/***************************************************************************
 *  A self-signed cert for localhost
 ***************************************************************************/
PRIVATE int make_cert(void)
{
    if(mkdir(TMP_DIR, 0700) != 0) {
        return -1;
    }
    char cmd[3*PATH_MAX];
    snprintf(cmd, sizeof(cmd),
        "openssl req -x509 -newkey rsa:2048 -nodes -sha256 -days 30 "
        "-keyout '%s' -out '%s' -subj '/CN=localhost' "
        "-addext 'subjectAltName=DNS:localhost' >/dev/null 2>&1",
        KEY_PATH, CERT_PATH
    );
    return system(cmd) == 0? 0 : -1;
}

/***************************************************************************
 *  Hand the bytes queued for `sskt` to it. Returns how many.
 ***************************************************************************/
PRIVATE size_t drain(hytls ytls, hsskt sskt, gbuffer_t *queue)
{
    size_t n = gbuffer_leftbytes(queue);
    if(n == 0) {
        return 0;
    }
    gbuffer_t *g = gbuffer_create(n, n);
    gbuffer_append(g, gbuffer_cur_rd_pointer(queue), n);
    gbuffer_clear(queue);
    ytls_decrypt_data(ytls, sskt, g);   /* takes ownership */
    return n;
}

/***************************************************************************
 *  Pump both ways until nothing is queued. Returns the bytes the client got.
 ***************************************************************************/
PRIVATE size_t pump(hytls s_ytls, hsskt s, hytls c_ytls, hsskt c)
{
    size_t to_client = 0;
    for(int i = 0; i < 50; i++) {
        size_t n = drain(s_ytls, s, cli_to_srv);
        size_t m = drain(c_ytls, c, srv_to_cli);
        to_client += m;
        if(n == 0 && m == 0) {
            break;
        }
    }
    return to_client;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int write_clear(hytls ytls, hsskt sskt, const char *text)
{
    size_t len = strlen(text);
    gbuffer_t *g = gbuffer_create(len, len);
    gbuffer_append(g, (void *)text, len);
    return ytls_encrypt_data(ytls, sskt, g);    /* takes ownership */
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_client(const char *library)
{
    json_t *server_cfg = json_pack("{s:s, s:s, s:s}",
        "library",              "openssl",
        "ssl_certificate",      CERT_PATH,
        "ssl_certificate_key",  KEY_PATH
    );
    json_t *client_cfg = json_pack("{s:s, s:s, s:s}",
        "library",                  library,
        "ssl_trusted_certificate",  CERT_PATH,
        "ssl_server_name",          "localhost"
    );
    hytls s_ytls = ytls_init(0, server_cfg, TRUE);
    hytls c_ytls = ytls_init(0, client_cfg, FALSE);
    JSON_DECREF(server_cfg)
    JSON_DECREF(client_cfg)
    if(!s_ytls || !c_ytls) {
        ok_or_fail(0, library, "the TLS contexts are made");
        if(s_ytls) {
            ytls_cleanup(s_ytls);
        }
        if(c_ytls) {
            ytls_cleanup(c_ytls);
        }
        return;
    }

    hs_done[0] = hs_done[1] = 0;
    hs_error[0] = hs_error[1] = 0;
    clear_read[0][0] = clear_read[1][0] = 0;
    cli_to_srv = gbuffer_create(64*1024, 64*1024);
    srv_to_cli = gbuffer_create(64*1024, 64*1024);

    hsskt s = ytls_new_secure_filter(s_ytls, on_handshake_done, on_clear_data, on_encrypted_data, &tag_server);
    hsskt c = ytls_new_secure_filter(c_ytls, on_handshake_done, on_clear_data, on_encrypted_data, &tag_client);
    if(s && c) {
        ytls_do_handshake(c_ytls, c);
        ytls_do_handshake(s_ytls, s);
        size_t after_handshake = 0;
        for(int i = 0; i < 50; i++) {
            BOOL client_was_done = hs_done[tag_client];
            drain(s_ytls, s, cli_to_srv);
            size_t m = drain(c_ytls, c, srv_to_cli);
            if(client_was_done) {
                after_handshake += m;
            }
            if(hs_done[0] && hs_done[1] &&
                    gbuffer_leftbytes(cli_to_srv) == 0 && gbuffer_leftbytes(srv_to_cli) == 0) {
                break;
            }
        }
        ok_or_fail(hs_done[0] && hs_done[1] && hs_error[0] == 0 && hs_error[1] == 0,
            library, "the handshake ends on both sides");
        ok_or_fail(after_handshake > 0,
            library, "the server sent bytes after the client ended its handshake (the tickets)");

        int rc = write_clear(c_ytls, c, "ping");
        ok_or_fail(rc == 0, library, "1 the client writes after the tickets arrived");
        pump(s_ytls, s, c_ytls, c);
        ok_or_fail(strcmp(clear_read[tag_server], "ping") == 0,
            library, "1 the server reads what the client wrote");

        rc = write_clear(s_ytls, s, "pong");
        ok_or_fail(rc == 0, library, "2 the server answers");
        pump(s_ytls, s, c_ytls, c);
        ok_or_fail(strcmp(clear_read[tag_client], "pong") == 0,
            library, "2 the client reads the answer");

        rc = write_clear(c_ytls, c, "again");
        ok_or_fail(rc == 0, library, "3 a second write of the client");
        pump(s_ytls, s, c_ytls, c);
        ok_or_fail(strcmp(clear_read[tag_server], "pingagain") == 0,
            library, "3 the server reads it");
    } else {
        ok_or_fail(0, library, "the secure filters are made");
    }

    if(c) {
        ytls_free_secure_filter(c_ytls, c);
    }
    if(s) {
        ytls_free_secure_filter(s_ytls, s);
    }
    GBUFFER_DECREF(cli_to_srv)
    GBUFFER_DECREF(srv_to_cli)
    ytls_cleanup(c_ytls);
    ytls_cleanup(s_ytls);
}

/***************************************************************************
 *              Test
 ***************************************************************************/
int main(int argc, char *argv[])
{
    gbmem_setup(
        0,          // mem_max_block, the default
        0,          // mem_max_system_memory, the default
        FALSE,      // use_own_system_memory
        0,          // mem_min_block
        0           // mem_superblock
    );

    sys_malloc_fn_t malloc_func;
    sys_free_fn_t free_func;

    gbmem_get_json_allocators(&malloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    gobj_start_up(
        argc,
        argv,
        NULL,   // jn_global_settings
        NULL,   // persistent_attrs
        NULL,   // global_command_parser
        NULL,   // global_stats_parser
        NULL,   // global_authz_checker
        NULL    // global_authentication_parser
    );

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    if(is_directory(TMP_DIR)) {
        rmrdir(TMP_DIR);
    }
    if(make_cert() != 0) {
        printf("FAIL the certificate cannot be made (is `openssl` installed?)\n");
        global_result += -1;
    } else {
        test_client("openssl");
#ifdef CONFIG_HAVE_MBEDTLS
        test_client("mbedtls");
#endif
    }
    if(is_directory(TMP_DIR)) {
        rmrdir(TMP_DIR);
    }

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}

/***************************************************************************
 *  Filter callbacks
 ***************************************************************************/
PRIVATE int on_handshake_done(void *user_data, int error)
{
    int tag = *(int *)user_data;
    hs_done[tag] = 1;
    hs_error[tag] = error;
    return 0;
}

PRIVATE int on_clear_data(void *user_data, gbuffer_t *gbuf)
{
    int tag = *(int *)user_data;
    size_t n = gbuffer_leftbytes(gbuf);
    size_t used = strlen(clear_read[tag]);
    if(used + n < sizeof(clear_read[tag])) {
        memcpy(clear_read[tag] + used, gbuffer_cur_rd_pointer(gbuf), n);
        clear_read[tag][used + n] = 0;
    }
    GBUFFER_DECREF(gbuf)
    return 0;
}

PRIVATE int on_encrypted_data(void *user_data, gbuffer_t *gbuf)
{
    int tag = *(int *)user_data;
    gbuffer_t *queue = (tag == tag_client) ? cli_to_srv : srv_to_cli;
    size_t n = gbuffer_leftbytes(gbuf);
    if(n > 0) {
        gbuffer_append(queue, gbuffer_cur_rd_pointer(gbuf), n);
    }
    GBUFFER_DECREF(gbuf)
    return 0;
}

#endif /* CONFIG_HAVE_OPENSSL */
