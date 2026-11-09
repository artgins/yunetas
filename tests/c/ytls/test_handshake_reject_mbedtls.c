/****************************************************************************
 *          test_handshake_reject_mbedtls.c
 *
 *          Verify that a bogus TLS handshake on the mbedTLS backend is
 *          rejected cleanly.
 *
 *          Scenario: a server-side ytls receives an HTTP-style request on
 *          the TLS port (a classic scanner / misconfig signature). The
 *          handshake MUST fail without crashing:
 *              1. ytls_decrypt_data() returns a TLS error (< -1000)
 *              2. on_handshake_done_cb is invoked exactly once with error=-1
 *              3. ytls_get_last_error() says the backend's reason (what
 *                 C_TCP puts in its disconnect_cause). Up to 7.25.21 the
 *                 mbedTLS backend never wrote it: the cause was a bare
 *                 "TLS handshake failed".
 *          Two more peers fail theirs in a row: the callback is called for
 *          each, and "TLS handshake rejected" is written ONCE for the three.
 *          A second context (another listener) fails one: its own line, its
 *          window is not the first's. At the end of the first context, the
 *          two it did not write are said: "TLS handshakes rejected, not said
 *          in their window", "suppressed" 2. Up to 7.30.19 the window was the
 *          process's: the second context's reject went into the first's
 *          count, and the count of the last window was never said.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#define APP "test_handshake_reject_mbedtls"

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

#ifndef CONFIG_HAVE_MBEDTLS

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    printf("%s: SKIP (CONFIG_HAVE_MBEDTLS not set)\n", APP);
    return 0;
}

#else /* CONFIG_HAVE_MBEDTLS */

/***************************************************************
 *              Constants
 ***************************************************************/
#define TMP_DIR   "/tmp/ytls_hs_reject_mbedtls"
#define CERT_PATH (TMP_DIR "/cert.pem")
#define KEY_PATH  (TMP_DIR "/key.pem")

#define HTTP_GARBAGE "GET /index.html HTTP/1.1\r\nHost: attacker\r\n\r\n"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int     handshake_cb_calls = 0;
PRIVATE int     handshake_cb_last_error = 0;
PRIVATE int     rejected_logs = 0;     // "TLS handshake rejected" written
PRIVATE int     not_said_logs = 0;     // "TLS handshakes rejected, not said in their window"
PRIVATE int     not_said_two = 0;      // the same, with "suppressed" 2

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int   generate_self_signed(const char *cert_path, const char *key_path);
PRIVATE void  cleanup_tmp(void);

PRIVATE int   on_handshake_done(void *user_data, int error);
PRIVATE int   on_clear_data(void *user_data, gbuffer_t *gbuf);
PRIVATE int   on_encrypted_data(void *user_data, gbuffer_t *gbuf);
PRIVATE int   count_rejected_write(void *v, int priority, const char *bf, size_t len);
PRIVATE void  fail_one_handshake(hytls ytls);

/***************************************************************************
 *              Test
 ***************************************************************************/
int main(int argc, char *argv[])
{
    int result = 0;

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    gobj_log_register_handler("count_rejected", 0, count_rejected_write, 0);
    gobj_log_add_handler("count_rejected", "count_rejected", LOG_OPT_ALL, 0);

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
        "library",             "mbedtls",
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
     *  This triggers decrypt_data → encrypted_buffer → do_handshake → fatal.
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

    const char *reason = ytls_get_last_error(ytls, sskt);
    if(!reason || !*reason) {
        fprintf(stderr,
            "%s: ytls_get_last_error() empty after a failed handshake\n",
            APP
        );
        result++;
    } else {
        printf("%s: reason of the failed handshake: %s\n", APP, reason);
    }

    ytls_free_secure_filter(ytls, sskt);

    /*------------------------------------------------*
     *  Two more peers fail their handshake at once:
     *  ONE "TLS handshake rejected" for the three (one
     *  per 10 s for the process, the rest counted in
     *  the next one). Up to 7.30.18 each one wrote it.
     *------------------------------------------------*/
    for(int i=0; i<2; i++) {
        fail_one_handshake(ytls);
    }
    if(handshake_cb_calls != 3) {
        fprintf(stderr,
            "%s: on_handshake_done called %d times for three peers, expected 3\n",
            APP, handshake_cb_calls
        );
        result++;
    }
    if(rejected_logs != 1) {
        fprintf(stderr,
            "%s: \"TLS handshake rejected\" written %d times for three peers in a row, expected 1\n",
            APP, rejected_logs
        );
        result++;
    }

    /*------------------------------------------------*
     *  A second context (another listener) fails one:
     *  its own line. Its end says nothing (it wrote
     *  all of its own); the end of the first says the
     *  two it did not write.
     *------------------------------------------------*/
    cfg = json_pack("{s:s, s:s, s:s}",
        "library",             "mbedtls",
        "ssl_certificate",     CERT_PATH,
        "ssl_certificate_key", KEY_PATH
    );
    hytls ytls_b = ytls_init(0, cfg, TRUE /* server */);
    JSON_DECREF(cfg);
    if(!ytls_b) {
        fprintf(stderr, "%s: ytls_init of the second context FAILED\n", APP);
        result++;
    } else {
        fail_one_handshake(ytls_b);
        if(rejected_logs != 2) {
            fprintf(stderr,
                "%s: \"TLS handshake rejected\" written %d times, expected 2: one per context\n",
                APP, rejected_logs
            );
            result++;
        }
        ytls_cleanup(ytls_b);
        if(not_said_logs != 0) {
            fprintf(stderr,
                "%s: the end of the second context said %d lines of rejects not said, expected 0\n",
                APP, not_said_logs
            );
            result++;
        }
    }

    ytls_cleanup(ytls);

    if(not_said_logs != 1 || not_said_two != 1) {
        fprintf(stderr,
            "%s: at the end of the first context, %d lines of rejects not said (%d with \"suppressed\": 2), expected 1 (1)\n",
            APP, not_said_logs, not_said_two
        );
        result++;
    }

    if(result == 0) {
        printf("%s: PASS\n", APP);
    } else {
        printf("%s: FAIL (%d check(s) failed)\n", APP, result);
    }

out:
    cleanup_tmp();
    gobj_end();
    return result;
}

/***************************************************************
 *              Local Methods
 ***************************************************************/

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
        "-subj '/CN=ytls_hs_reject' "
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

/***************************************************************************
 *  Log handler: counts the "TLS handshake rejected"
 ***************************************************************************/
PRIVATE int count_rejected_write(void *v, int priority, const char *bf, size_t len)
{
    (void)v;
    (void)priority;
    const char *needle = "TLS handshake rejected";
    if(bf && memmem(bf, len, needle, strlen(needle))) {
        rejected_logs++;
    }
    const char *not_said = "TLS handshakes rejected, not said in their window";
    if(bf && memmem(bf, len, not_said, strlen(not_said))) {
        not_said_logs++;
        const char *two = "\"suppressed\": 2";
        if(memmem(bf, len, two, strlen(two))) {
            not_said_two++;
        }
    }
    return 0;
}

/***************************************************************************
 *  A peer that sends an HTTP request to the TLS port of `ytls`
 ***************************************************************************/
PRIVATE void fail_one_handshake(hytls ytls)
{
    hsskt sskt = ytls_new_secure_filter(
        ytls,
        on_handshake_done,
        on_clear_data,
        on_encrypted_data,
        NULL
    );
    if(!sskt) {
        fprintf(stderr, "%s: ytls_new_secure_filter FAILED\n", APP);
        return;
    }
    (void)ytls_do_handshake(ytls, sskt);
    gbuffer_t *gbuf = gbuffer_create(sizeof(HTTP_GARBAGE), sizeof(HTTP_GARBAGE));
    gbuffer_append(gbuf, (void *)HTTP_GARBAGE, sizeof(HTTP_GARBAGE) - 1);
    (void)ytls_decrypt_data(ytls, sskt, gbuf);
    ytls_free_secure_filter(ytls, sskt);
}

#endif /* CONFIG_HAVE_MBEDTLS */
