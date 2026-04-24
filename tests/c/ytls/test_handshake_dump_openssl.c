/****************************************************************************
 *          test_handshake_dump_openssl.c
 *
 *          Verify that a failing TLS handshake on the OpenSSL backend emits
 *          the forensic transcript of the raw peer bytes.
 *
 *          Scenario: a server-side ytls receives an HTTP-style request on
 *          the TLS port (a classic scanner / misconfig signature). The
 *          handshake MUST fail, on_handshake_done_cb MUST be invoked with
 *          error=-1, and the log MUST contain:
 *              1. The forensic title "TLS handshake FAILS: raw peer bytes"
 *              2. The hex bytes of "GET " (47 45 54 20) from the payload
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#define APP "test_handshake_dump_openssl"

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
#define TMP_DIR   "/tmp/ytls_hs_dump_openssl"
#define CERT_PATH (TMP_DIR "/cert.pem")
#define KEY_PATH  (TMP_DIR "/key.pem")

#define HTTP_GARBAGE "GET /index.html HTTP/1.1\r\nHost: attacker\r\n\r\n"

#define CAPTURE_BUFSZ (256 * 1024)

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

    /*------------------------------------------------*
     *  Register a capture log handler AFTER gobj_start_up
     *  (which calls glog_init internally).
     *------------------------------------------------*/
    capture_buf = malloc(CAPTURE_BUFSZ);
    if(!capture_buf) {
        fprintf(stderr, "%s: no memory for capture_buf\n", APP);
        gobj_end();
        return 1;
    }
    capture_buf[0] = '\0';
    capture_len = 0;

    gobj_log_register_handler("capture", 0, capture_write_fn, 0);
    gobj_log_add_handler("capture", "capture", LOG_OPT_ALL, 0);

    /*------------------------------------------------*
     *  Disposable self-signed cert
     *------------------------------------------------*/
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

    /*------------------------------------------------*
     *  Server-side ytls with the self-signed cert
     *------------------------------------------------*/
    json_t *cfg = json_pack("{s:s, s:s, s:s}",
        "library",             "openssl",
        "ssl_certificate",     CERT_PATH,
        "ssl_certificate_key", KEY_PATH
    );
    hytls ytls = ytls_init(0, cfg, TRUE /* server */);
    JSON_DECREF(cfg);
    if(!ytls) {
        fprintf(stderr, "%s: ytls_init FAILED\n", APP);
        result = 1;
        goto out;
    }

    hsskt sskt = ytls_new_secure_filter(
        ytls,
        on_handshake_done,
        on_clear_data,
        on_encrypted_data,
        NULL
    );
    if(!sskt) {
        fprintf(stderr, "%s: ytls_new_secure_filter FAILED\n", APP);
        ytls_cleanup(ytls);
        result = 1;
        goto out;
    }

    /*------------------------------------------------*
     *  Kick the filter: server-side returns WANT_READ
     *  because no client bytes have arrived yet.
     *------------------------------------------------*/
    (void)ytls_do_handshake(ytls, sskt);

    /*------------------------------------------------*
     *  Push garbage (HTTP request on TLS port).
     *  This triggers decrypt_data → capture_handshake_bytes
     *  → BIO_write → do_handshake → fatal → dump_handshake_transcript.
     *------------------------------------------------*/
    gbuffer_t *gbuf = gbuffer_create(sizeof(HTTP_GARBAGE), sizeof(HTTP_GARBAGE));
    gbuffer_append(gbuf, (void *)HTTP_GARBAGE, sizeof(HTTP_GARBAGE) - 1);
    int rc = ytls_decrypt_data(ytls, sskt, gbuf);

    if(rc > -1000) {
        fprintf(stderr,
            "%s: ytls_decrypt_data returned %d, expected < -1000 (TLS error)\n",
            APP, rc
        );
        result++;
    }

    if(handshake_cb_calls != 1) {
        fprintf(stderr,
            "%s: on_handshake_done called %d times, expected 1\n",
            APP, handshake_cb_calls
        );
        result++;
    }
    if(handshake_cb_last_error != -1) {
        fprintf(stderr,
            "%s: on_handshake_done error=%d, expected -1\n",
            APP, handshake_cb_last_error
        );
        result++;
    }

    /*------------------------------------------------*
     *  The captured log MUST contain the forensic title
     *  and the hex bytes of "GET " (47 45 54 20).
     *------------------------------------------------*/
    if(!strstr(capture_buf, "TLS handshake FAILS: raw peer bytes")) {
        fprintf(stderr,
            "%s: capture log does NOT contain the forensic title\n",
            APP
        );
        result++;
    }
    if(!strstr(capture_buf, "47 45 54 20")) {
        fprintf(stderr,
            "%s: capture log does NOT contain hex bytes of 'GET '\n",
            APP
        );
        result++;
    }

    ytls_free_secure_filter(ytls, sskt);
    ytls_cleanup(ytls);

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

/***************************************************************************
 *  Append every logged line into the capture buffer so the test can
 *  grep for the forensic strings. Truncates silently if the buffer fills.
 ***************************************************************************/
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

/***************************************************************************
 *  Shell out to `openssl req` for a throw-away self-signed cert.
 ***************************************************************************/
PRIVATE int generate_self_signed(const char *cert_path, const char *key_path)
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "openssl req -x509 -newkey rsa:2048 -nodes -sha256 "
        "-days 30 "
        "-keyout '%s' -out '%s' "
        "-subj '/CN=ytls_hs_dump' "
        ">/dev/null 2>&1",
        key_path, cert_path
    );
    int rc = system(cmd);
    if(rc != 0) {
        return -1;
    }
    struct stat st;
    if(stat(cert_path, &st) != 0 || st.st_size == 0) return -1;
    if(stat(key_path,  &st) != 0 || st.st_size == 0) return -1;
    return 0;
}

/***************************************************************************
 *  Remove TMP_DIR contents, ignoring errors.
 ***************************************************************************/
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
