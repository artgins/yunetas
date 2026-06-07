/****************************************************************************
 *          test_tls_floor_openssl.c
 *
 *          Verify the secure-by-default TLS protocol floor on the OpenSSL
 *          backend and that neither the downgrade nor the rejection is silent
 *          (yuneta "no silent errors" norm).
 *
 *          Test A — explicit legacy downgrade is logged:
 *              a gate built with ssl_min_version="TLS1.0" MUST log the
 *              "legacy floor below TLS1.2" warning, so the reduced-security
 *              surface is enumerable.
 *
 *          Test B — the default floor (TLS1.2) rejects a legacy peer AND
 *          leaves a trace: a server built WITHOUT ssl_min_version (default
 *          TLS1.2 floor) fed a real TLS1.0 ClientHello MUST fail the
 *          handshake (cb error=-1) and log "TLS handshake rejected" even
 *          with trace_tls off — so the legacy IoT device is identifiable.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#define APP "test_tls_floor_openssl"

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

/***************************************************************
 *              Constants
 ***************************************************************/
#define TMP_DIR   "/tmp/ytls_floor_openssl"
#define CERT_PATH (TMP_DIR "/cert.pem")
#define KEY_PATH  (TMP_DIR "/key.pem")

#define CAPTURE_BUFSZ (256 * 1024)

/*
 *  A minimal but valid TLS1.0 ClientHello (no supported_versions extension,
 *  so the server negotiates on client_version=0x0301). A server with the
 *  default TLS1.2 floor rejects it on protocol version.
 *
 *  Record:    16 03 01 00 2D                 (handshake, TLS1.0, len 45)
 *  Handshake: 01 00 00 29                     (ClientHello, len 41)
 *    client_version: 03 01                    (TLS1.0)
 *    random:         32 x 00
 *    session_id:     00                       (len 0)
 *    cipher_suites:  00 02  00 2F             (TLS_RSA_WITH_AES_128_CBC_SHA)
 *    compression:    01 00                    (null)
 */
PRIVATE const unsigned char TLS10_CLIENT_HELLO[] = {
    0x16, 0x03, 0x01, 0x00, 0x2D,
    0x01, 0x00, 0x00, 0x29,
    0x03, 0x01,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,
    0x00, 0x02, 0x00, 0x2F,
    0x01, 0x00
};

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE char   *capture_buf = NULL;
PRIVATE size_t  capture_len = 0;

PRIVATE int     handshake_cb_calls = 0;
PRIVATE int     handshake_cb_last_error = 0;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int   generate_self_signed(const char *cert_path, const char *key_path);
PRIVATE void  cleanup_tmp(void);
PRIVATE int   capture_write_fn(void *v, int priority, const char *bf, size_t len);
PRIVATE void  capture_reset(void);

PRIVATE int   on_handshake_done(void *user_data, int error);
PRIVATE int   on_clear_data(void *user_data, gbuffer_t *gbuf);
PRIVATE int   on_encrypted_data(void *user_data, gbuffer_t *gbuf);

/***************************************************************************
 *              Test
 ***************************************************************************/
int main(int argc, char *argv[])
{
    int result = 0;

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    capture_buf = malloc(CAPTURE_BUFSZ);
    if(!capture_buf) {
        fprintf(stderr, "%s: no memory for capture_buf\n", APP);
        gobj_end();
        return 1;
    }
    capture_reset();

    gobj_log_register_handler("capture", 0, capture_write_fn, 0);
    gobj_log_add_handler("capture", "capture", LOG_OPT_ALL, 0);

    cleanup_tmp();
    if(mkdir(TMP_DIR, 0700) != 0) {
        fprintf(stderr, "%s: mkdir(%s) FAILED\n", APP, TMP_DIR);
        result = 1;
        goto out;
    }
    if(generate_self_signed(CERT_PATH, KEY_PATH) != 0) {
        fprintf(stderr, "%s: cert generation FAILED (is `openssl` installed?)\n", APP);
        result = 1;
        goto out;
    }

    /*================================================================*
     *  Test A: explicit legacy downgrade must be logged
     *================================================================*/
    capture_reset();
    {
        json_t *cfg = json_pack("{s:s, s:s, s:s, s:s}",
            "library",             "openssl",
            "ssl_certificate",     CERT_PATH,
            "ssl_certificate_key", KEY_PATH,
            "ssl_min_version",     "TLS1.0"
        );
        hytls ytls = ytls_init(0, cfg, TRUE /* server */);
        JSON_DECREF(cfg);
        if(!ytls) {
            fprintf(stderr, "%s[A]: ytls_init FAILED\n", APP);
            result++;
        } else {
            if(!strstr(capture_buf, "legacy floor below TLS1.2")) {
                fprintf(stderr, "%s[A]: explicit downgrade NOT logged\n", APP);
                result++;
            } else {
                printf("%s[A]: ok - legacy downgrade logged\n", APP);
            }
            ytls_cleanup(ytls);
        }
    }

    /*================================================================*
     *  Test B: default floor (TLS1.2) rejects a TLS1.0 ClientHello
     *          AND leaves a trace, with trace_tls off
     *================================================================*/
    capture_reset();
    handshake_cb_calls = 0;
    handshake_cb_last_error = 0;
    {
        json_t *cfg = json_pack("{s:s, s:s, s:s}",
            "library",             "openssl",
            "ssl_certificate",     CERT_PATH,
            "ssl_certificate_key", KEY_PATH
        );
        hytls ytls = ytls_init(0, cfg, TRUE /* server */);
        JSON_DECREF(cfg);
        if(!ytls) {
            fprintf(stderr, "%s[B]: ytls_init FAILED\n", APP);
            result++;
            goto out;
        }

        hsskt sskt = ytls_new_secure_filter(
            ytls, on_handshake_done, on_clear_data, on_encrypted_data, NULL
        );
        if(!sskt) {
            fprintf(stderr, "%s[B]: ytls_new_secure_filter FAILED\n", APP);
            ytls_cleanup(ytls);
            result++;
            goto out;
        }

        (void)ytls_do_handshake(ytls, sskt);   // kick: server returns WANT_READ

        gbuffer_t *gbuf = gbuffer_create(sizeof(TLS10_CLIENT_HELLO), sizeof(TLS10_CLIENT_HELLO));
        gbuffer_append(gbuf, (void *)TLS10_CLIENT_HELLO, sizeof(TLS10_CLIENT_HELLO));
        (void)ytls_decrypt_data(ytls, sskt, gbuf);

        if(handshake_cb_last_error != -1) {
            fprintf(stderr, "%s[B]: handshake error=%d, expected -1 (TLS1.0 must be rejected)\n",
                APP, handshake_cb_last_error);
            result++;
        }
        if(!strstr(capture_buf, "TLS handshake rejected")) {
            fprintf(stderr, "%s[B]: rejection NOT traced in the log\n", APP);
            result++;
        }
        if(handshake_cb_last_error == -1 && strstr(capture_buf, "TLS handshake rejected")) {
            printf("%s[B]: ok - TLS1.0 rejected by default floor and traced\n", APP);
        }

        ytls_free_secure_filter(ytls, sskt);
        ytls_cleanup(ytls);
    }

    /*================================================================*
     *  Test C: re-enabling TLS renegotiation must be logged
     *          (default is now disabled; explicit enable = downgrade)
     *================================================================*/
    capture_reset();
    {
        json_t *cfg = json_pack("{s:s, s:s, s:s, s:b}",
            "library",                   "openssl",
            "ssl_certificate",           CERT_PATH,
            "ssl_certificate_key",       KEY_PATH,
            "ssl_disable_renegotiation", 0   /* explicitly re-enable reneg */
        );
        hytls ytls = ytls_init(0, cfg, TRUE /* server */);
        JSON_DECREF(cfg);
        if(!ytls) {
            fprintf(stderr, "%s[C]: ytls_init FAILED\n", APP);
            result++;
        } else {
            if(!strstr(capture_buf, "renegotiation explicitly enabled")) {
                fprintf(stderr, "%s[C]: reneg re-enable NOT logged\n", APP);
                result++;
            } else {
                printf("%s[C]: ok - reneg re-enable logged\n", APP);
            }
            ytls_cleanup(ytls);
        }
    }

    if(result == 0) {
        printf("%s: PASS\n", APP);
    } else {
        printf("%s: FAIL (%d check(s) failed)\n", APP, result);
    }

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
PRIVATE void capture_reset(void)
{
    capture_len = 0;
    if(capture_buf) {
        capture_buf[0] = '\0';
    }
}

PRIVATE int capture_write_fn(void *v, int priority, const char *bf, size_t len)
{
    (void)v; (void)priority;
    if(!capture_buf || len == 0) {
        return 0;
    }
    if(capture_len + len + 2 >= CAPTURE_BUFSZ) {
        return 0;
    }
    memcpy(capture_buf + capture_len, bf, len);
    capture_len += len;
    capture_buf[capture_len++] = '\n';
    capture_buf[capture_len] = '\0';
    return 0;
}

PRIVATE int generate_self_signed(const char *cert_path, const char *key_path)
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "openssl req -x509 -newkey rsa:2048 -nodes -sha256 "
        "-days 30 -keyout '%s' -out '%s' "
        "-subj '/CN=ytls_floor' >/dev/null 2>&1",
        key_path, cert_path
    );
    if(system(cmd) != 0) {
        return -1;
    }
    struct stat st;
    if(stat(cert_path, &st) != 0 || st.st_size == 0) return -1;
    if(stat(key_path,  &st) != 0 || st.st_size == 0) return -1;
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
    (void)user_data;
    handshake_cb_calls++;
    handshake_cb_last_error = error;
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
    (void)user_data;
    GBUFFER_DECREF(gbuf)
    return 0;
}

#endif /* CONFIG_HAVE_OPENSSL */
