/****************************************************************************
 *          test_handshake_reject_openssl.c
 *
 *          Verify that a bogus TLS handshake on the OpenSSL backend is
 *          rejected cleanly.
 *
 *          Scenario: a server-side ytls receives an HTTP-style request on
 *          the TLS port (a classic scanner / misconfig signature). The
 *          handshake MUST fail without crashing:
 *              1. ytls_decrypt_data() returns a TLS error (< -1000)
 *              2. on_handshake_done_cb is invoked exactly once with error=-1
 *          Two more peers fail theirs in a row: the callback is called for
 *          each, and "TLS handshake rejected" is written ONCE for the three.
 *          A second context (another listener) fails one: its own line, its
 *          window is not the first's. At the end of the first context, the
 *          two it did not write are said: "TLS handshakes rejected, not said
 *          in their window", "suppressed" 2. Up to 7.30.19 the window was the
 *          process's: the second context's reject went into the first's
 *          count, and the count of the last window was never said.
 *          Nothing closes a window by time: what it did not write is said
 *          at the next reject after it, or when its context ends.
 *
 *          Client contexts: C_TCP makes one at each start of a client, so
 *          a client that connects again and again makes a context per
 *          connection. Their window is the process's: 20 client contexts
 *          that fail 2 handshakes each write one line (7.30.20: 40, a
 *          window per context, and its count said at its end). After the
 *          window, the next reject says the count of the rest.
 *
 *          The yuno ends: 40 more client rejects in one window, in two
 *          contexts still open, and the shutdown is marked. The first of
 *          them to end says the count of the window, ONCE: the second says
 *          nothing. Up to 7.30.21 a client context said it only when its
 *          window had passed, so the rejects of the last window of the
 *          process were never said.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#define APP "test_handshake_reject_openssl"

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
#define TMP_DIR   "/tmp/ytls_hs_reject_openssl"
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
PRIVATE long    suppressed_sum = 0;    // the "suppressed" of every line, summed

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
PRIVATE void  fail_one_client_handshake(hytls ytls);

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
     *  This triggers decrypt_data → BIO_write → do_handshake → fatal.
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

    ytls_free_secure_filter(ytls, sskt);

    /*------------------------------------------------*
     *  Two more peers fail their handshake at once:
     *  ONE "TLS handshake rejected" for the three (one
     *  per 10 s for the context; the rest are counted
     *  and said at the next reject after the window,
     *  or when the context ends). Up to 7.30.18 each
     *  one wrote it.
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
        "library",             "openssl",
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

    /*------------------------------------------------*
     *  20 client contexts, 2 failed handshakes each:
     *  one window for all of them, so 1 line (up to 2,
     *  should a window end in the middle), not 40.
     *------------------------------------------------*/
    int rejected_before = rejected_logs;
    int not_said_before = not_said_logs;
    suppressed_sum = 0;
    for(int i=0; i<20; i++) {
        cfg = json_pack("{s:s, s:s, s:s}",
            "library",                  "openssl",
            "ssl_trusted_certificate",  CERT_PATH,
            "ssl_server_name",          "localhost"
        );
        hytls ytls_c = ytls_init(0, cfg, FALSE /* client */);
        JSON_DECREF(cfg);
        if(!ytls_c) {
            fprintf(stderr, "%s: ytls_init of client context %d FAILED\n", APP, i);
            result++;
            break;
        }
        fail_one_client_handshake(ytls_c);
        fail_one_client_handshake(ytls_c);
        ytls_cleanup(ytls_c);
    }
    int client_lines = (rejected_logs - rejected_before) + (not_said_logs - not_said_before);
    if(client_lines < 1 || client_lines > 2) {
        fprintf(stderr,
            "%s: %d lines for the 40 rejects of 20 client contexts, expected 1 (2 at most)\n",
            APP, client_lines
        );
        result++;
    }

    /*------------------------------------------------*
     *  After the window, one more: its line says the
     *  rest. Every reject is written or counted.
     *------------------------------------------------*/
    printf("%s: waiting 11 s for the window of the client contexts to pass\n", APP);
    sleep(11);
    cfg = json_pack("{s:s, s:s, s:s}",
        "library",                  "openssl",
        "ssl_trusted_certificate",  CERT_PATH,
        "ssl_server_name",          "localhost"
    );
    hytls ytls_last = ytls_init(0, cfg, FALSE /* client */);
    JSON_DECREF(cfg);
    if(!ytls_last) {
        fprintf(stderr, "%s: ytls_init of the last client context FAILED\n", APP);
        result++;
    } else {
        fail_one_client_handshake(ytls_last);
        ytls_cleanup(ytls_last);
    }
    long said = (long)(rejected_logs - rejected_before) + suppressed_sum;
    if(said != 41) {
        fprintf(stderr,
            "%s: of 41 rejects of client contexts, %ld written or counted, expected 41\n",
            APP, said
        );
        result++;
    }

    /*------------------------------------------------*
     *  The yuno ends with a window of the clients
     *  open: the first client context that ends in
     *  the shutdown says its count, once.
     *------------------------------------------------*/
    rejected_before = rejected_logs;
    not_said_before = not_said_logs;
    suppressed_sum = 0;
    hytls ytls_end[2] = {0};
    for(int i=0; i<2; i++) {
        cfg = json_pack("{s:s, s:s, s:s}",
            "library",                  "openssl",
            "ssl_trusted_certificate",  CERT_PATH,
            "ssl_server_name",          "localhost"
        );
        ytls_end[i] = ytls_init(0, cfg, FALSE /* client */);
        JSON_DECREF(cfg);
        if(!ytls_end[i]) {
            fprintf(stderr, "%s: ytls_init of the client context %d of the end FAILED\n", APP, i);
            result++;
            continue;
        }
        for(int j=0; j<20; j++) {
            fail_one_client_handshake(ytls_end[i]);
        }
    }
    gobj_set_shutdown();
    for(int i=0; i<2; i++) {
        if(ytls_end[i]) {
            ytls_cleanup(ytls_end[i]);
        }
    }
    said = (long)(rejected_logs - rejected_before) + suppressed_sum;
    int end_lines = not_said_logs - not_said_before;
    if(said != 40 || end_lines != 1) {
        fprintf(stderr,
            "%s: at the end of the yuno, %ld of 40 client rejects written or counted, in %d lines of rejects not said, expected 40 in 1\n",
            APP, said, end_lines
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
    const char *suppressed = "\"suppressed\": ";
    const char *at = bf? memmem(bf, len, suppressed, strlen(suppressed)) : NULL;
    if(at) {
        suppressed_sum += atol(at + strlen(suppressed));
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

/***************************************************************************
 *  A client whose server answers its ClientHello with an HTTP answer
 ***************************************************************************/
PRIVATE void fail_one_client_handshake(hytls ytls)
{
    hsskt sskt = ytls_new_secure_filter(
        ytls,
        on_handshake_done,
        on_clear_data,
        on_encrypted_data,
        NULL
    );
    if(!sskt) {
        fprintf(stderr, "%s: ytls_new_secure_filter of a client FAILED\n", APP);
        return;
    }
    (void)ytls_do_handshake(ytls, sskt);
    const char *answer = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
    gbuffer_t *gbuf = gbuffer_create(strlen(answer), strlen(answer));
    gbuffer_append(gbuf, (void *)answer, strlen(answer));
    (void)ytls_decrypt_data(ytls, sskt, gbuf);
    ytls_free_secure_filter(ytls, sskt);
}

#endif /* CONFIG_HAVE_OPENSSL */
