/****************************************************************************
 *          test_tls_verify_openssl.c
 *
 *          Verify the OpenSSL-backend peer verification: a client with a CA
 *          configured (computed default REQUIRED) validates the server cert
 *          chain AND hostname, and nothing is silent.
 *
 *          Drives a real handshake by pumping the encrypted bytes between a
 *          client filter and a server filter (loopback, no sockets).
 *
 *          Test 1: trusted cert + matching host       -> handshake SUCCEEDS
 *          Test 2: trusted cert + WRONG host          -> handshake REJECTED
 *          Test 3: required verify, no CA (untrusted) -> handshake REJECTED
 *          Test 4: client with no CA (NONE)           -> "no validation" warning
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#define APP "test_tls_verify_openssl"

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
#include <ytls.h>

#ifndef CONFIG_HAVE_OPENSSL

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    printf("%s: SKIP (CONFIG_HAVE_OPENSSL not set)\n", APP);
    return 0;
}

#else /* CONFIG_HAVE_OPENSSL */

#define TMP_DIR   "/tmp/ytls_verify_openssl"
#define CERT_PATH (TMP_DIR "/cert.pem")
#define KEY_PATH  (TMP_DIR "/key.pem")
#define CAPTURE_BUFSZ (256 * 1024)

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE char   *capture_buf = NULL;
PRIVATE size_t  capture_len = 0;

PRIVATE int     tag_client = 0;
PRIVATE int     tag_server = 1;

PRIVATE gbuffer_t *cli_to_srv = NULL;   /* encrypted bytes client -> server */
PRIVATE gbuffer_t *srv_to_cli = NULL;   /* encrypted bytes server -> client */

PRIVATE int     hs_done[2]  = {0, 0};   /* [tag] handshake callback fired */
PRIVATE int     hs_error[2] = {0, 0};   /* [tag] last handshake error */

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int   generate_self_signed(void);
PRIVATE void  cleanup_tmp(void);
PRIVATE int   capture_write_fn(void *v, int priority, const char *bf, size_t len);
PRIVATE void  capture_reset(void);

PRIVATE int   on_handshake_done(void *user_data, int error);
PRIVATE int   on_clear_data(void *user_data, gbuffer_t *gbuf);
PRIVATE int   on_encrypted_data(void *user_data, gbuffer_t *gbuf);

PRIVATE void  drain(hytls ytls, hsskt sskt, gbuffer_t *queue);
PRIVATE int   run_handshake(hytls c_ytls, hsskt c, hytls s_ytls, hsskt s);

/***************************************************************************
 *  One client/server handshake attempt with a given client config.
 *  Returns the client-side handshake error (0 = success, -1 = rejected),
 *  or -99 on setup failure.
 ***************************************************************************/
PRIVATE int attempt(json_t *client_cfg)
{
    int ret = -99;
    json_t *server_cfg = json_pack("{s:s, s:s, s:s}",
        "library",             "openssl",
        "ssl_certificate",     CERT_PATH,
        "ssl_certificate_key", KEY_PATH
    );
    hytls s_ytls = ytls_init(0, server_cfg, TRUE);
    hytls c_ytls = ytls_init(0, client_cfg, FALSE);
    JSON_DECREF(server_cfg);
    JSON_DECREF(client_cfg);
    if(!s_ytls || !c_ytls) {
        if(s_ytls) ytls_cleanup(s_ytls);
        if(c_ytls) ytls_cleanup(c_ytls);
        return -99;
    }

    hs_done[0] = hs_done[1] = 0;
    hs_error[0] = hs_error[1] = 0;
    cli_to_srv = gbuffer_create(64*1024, 64*1024);
    srv_to_cli = gbuffer_create(64*1024, 64*1024);

    hsskt s = ytls_new_secure_filter(s_ytls, on_handshake_done, on_clear_data, on_encrypted_data, &tag_server);
    hsskt c = ytls_new_secure_filter(c_ytls, on_handshake_done, on_clear_data, on_encrypted_data, &tag_client);
    if(s && c) {
        ret = run_handshake(c_ytls, c, s_ytls, s);
    }

    if(c) ytls_free_secure_filter(c_ytls, c);
    if(s) ytls_free_secure_filter(s_ytls, s);
    GBUFFER_DECREF(cli_to_srv)
    GBUFFER_DECREF(srv_to_cli)
    ytls_cleanup(c_ytls);
    ytls_cleanup(s_ytls);
    return ret;
}

/***************************************************************************
 *              Test
 ***************************************************************************/
int main(int argc, char *argv[])
{
    int result = 0;

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    capture_buf = malloc(CAPTURE_BUFSZ);
    if(!capture_buf) { gobj_end(); return 1; }
    capture_reset();
    gobj_log_register_handler("capture", 0, capture_write_fn, 0);
    gobj_log_add_handler("capture", "capture", LOG_OPT_ALL, 0);

    cleanup_tmp();
    if(mkdir(TMP_DIR, 0700) != 0 || generate_self_signed() != 0) {
        fprintf(stderr, "%s: cert setup FAILED (is `openssl` installed?)\n", APP);
        result = 1;
        goto out;
    }

    /* Test 1: trusted cert + matching host -> SUCCESS */
    capture_reset();
    {
        json_t *cfg = json_pack("{s:s, s:s, s:s}",
            "library",                 "openssl",
            "ssl_trusted_certificate", CERT_PATH,   /* trust the self-signed cert */
            "ssl_server_name",         "localhost"
        );
        int e = attempt(cfg);
        if(e == 0) {
            printf("%s[1]: ok - trusted cert + host match -> handshake OK\n", APP);
        } else {
            fprintf(stderr, "%s[1]: expected success, got client error=%d\n", APP, e);
            result++;
        }
    }

    /* Test 2: trusted cert + WRONG host -> REJECTED */
    capture_reset();
    {
        json_t *cfg = json_pack("{s:s, s:s, s:s}",
            "library",                 "openssl",
            "ssl_trusted_certificate", CERT_PATH,
            "ssl_server_name",         "wronghost.invalid"
        );
        int e = attempt(cfg);
        if(e == -1) {
            printf("%s[2]: ok - hostname mismatch -> handshake rejected\n", APP);
        } else {
            fprintf(stderr, "%s[2]: expected rejection, got client error=%d\n", APP, e);
            result++;
        }
    }

    /* Test 3: required verify but no CA -> untrusted -> REJECTED */
    capture_reset();
    {
        json_t *cfg = json_pack("{s:s, s:s, s:s}",
            "library",         "openssl",
            "ssl_verify_mode", "required",
            "ssl_server_name", "localhost"
        );
        int e = attempt(cfg);
        if(e == -1) {
            printf("%s[3]: ok - unknown CA -> handshake rejected\n", APP);
        } else {
            fprintf(stderr, "%s[3]: expected rejection, got client error=%d\n", APP, e);
            result++;
        }
    }

    /* Test 4: client with no CA -> NONE -> "no validation" warning */
    capture_reset();
    {
        json_t *cfg = json_pack("{s:s}", "library", "openssl");
        hytls c = ytls_init(0, cfg, FALSE);
        JSON_DECREF(cfg);
        if(!c) {
            fprintf(stderr, "%s[4]: ytls_init FAILED\n", APP);
            result++;
        } else {
            if(strstr(capture_buf, "WITHOUT server-certificate validation")) {
                printf("%s[4]: ok - unverified client logged\n", APP);
            } else {
                fprintf(stderr, "%s[4]: missing no-validation warning\n", APP);
                result++;
            }
            ytls_cleanup(c);
        }
    }

    printf("\n%s: %s\n", APP, result==0 ? "PASS" : "FAIL");

out:
    cleanup_tmp();
    gobj_log_del_handler("capture");
    free(capture_buf);
    capture_buf = NULL;
    gobj_end();
    return result;
}

/***************************************************************
 *              Local Methods
 ***************************************************************/
PRIVATE void drain(hytls ytls, hsskt sskt, gbuffer_t *queue)
{
    size_t n = gbuffer_leftbytes(queue);
    if(n == 0) {
        return;
    }
    gbuffer_t *g = gbuffer_create(n, n);
    gbuffer_append(g, gbuffer_cur_rd_pointer(queue), n);
    gbuffer_clear(queue);
    ytls_decrypt_data(ytls, sskt, g);   /* takes ownership */
}

PRIVATE int run_handshake(hytls c_ytls, hsskt c, hytls s_ytls, hsskt s)
{
    ytls_do_handshake(c_ytls, c);   /* client sends ClientHello into cli_to_srv */
    for(int i=0; i<50 && (!hs_done[0] || !hs_done[1]); i++) {
        drain(s_ytls, s, cli_to_srv);   /* server consumes client bytes */
        ytls_do_handshake(s_ytls, s);
        drain(c_ytls, c, srv_to_cli);   /* client consumes server bytes */
        ytls_do_handshake(c_ytls, c);
    }
    return hs_done[0] ? hs_error[0] : -1;   /* client verdict */
}

PRIVATE void capture_reset(void)
{
    capture_len = 0;
    if(capture_buf) capture_buf[0] = '\0';
}

PRIVATE int capture_write_fn(void *v, int priority, const char *bf, size_t len)
{
    (void)v; (void)priority;
    if(!capture_buf || len == 0 || capture_len + len + 2 >= CAPTURE_BUFSZ) {
        return 0;
    }
    memcpy(capture_buf + capture_len, bf, len);
    capture_len += len;
    capture_buf[capture_len++] = '\n';
    capture_buf[capture_len] = '\0';
    return 0;
}

PRIVATE int generate_self_signed(void)
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "openssl req -x509 -newkey rsa:2048 -nodes -sha256 -days 30 "
        "-keyout '%s' -out '%s' -subj '/CN=localhost' "
        "-addext 'subjectAltName=DNS:localhost' >/dev/null 2>&1",
        KEY_PATH, CERT_PATH
    );
    if(system(cmd) != 0) return -1;
    struct stat st;
    if(stat(CERT_PATH, &st) != 0 || st.st_size == 0) return -1;
    if(stat(KEY_PATH,  &st) != 0 || st.st_size == 0) return -1;
    return 0;
}

PRIVATE void cleanup_tmp(void)
{
    unlink(CERT_PATH);
    unlink(KEY_PATH);
    rmdir(TMP_DIR);
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
    (void)user_data;
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
