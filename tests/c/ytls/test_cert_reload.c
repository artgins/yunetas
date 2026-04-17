/****************************************************************************
 *          test_cert_reload.c
 *
 *          Exercise ytls_reload_certificates() + ytls_get_cert_info().
 *
 *          Flow
 *          ----
 *          1. Generate self-signed cert A and cert B with different CNs
 *             via `openssl req` invoked from system().
 *          2. ytls_init() with cert A (server mode).
 *          3. ytls_get_cert_info() → subject MUST contain "ytls_test_A".
 *          4. ytls_reload_certificates() with cert B → MUST return 0.
 *          5. ytls_get_cert_info() → subject MUST contain "ytls_test_B".
 *             (i.e. the swap happened.)
 *          6. ytls_reload_certificates() with a non-existent file → MUST
 *             return -1 AND the previous cert must survive intact
 *             (ytls_get_cert_info still shows cert B).
 *          7. ytls_cleanup + tmp files removed.
 *
 *          Covers both swap direction AND error path (old ctx is kept
 *          when the new config is invalid). Does NOT exercise a live
 *          TCP connection across a reload — that is left to an
 *          integration test using c_tcp_s listeners.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#define APP "test_cert_reload"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <kwid.h>
#include <gobj.h>
#include <ytls.h>

/***************************************************************
 *              Constants
 ***************************************************************/
#define TMP_DIR "/tmp/ytls_reload_test"
#define CERT_A (TMP_DIR "/cert_A.pem")
#define KEY_A  (TMP_DIR "/key_A.pem")
#define CERT_B (TMP_DIR "/cert_B.pem")
#define KEY_B  (TMP_DIR "/key_B.pem")
#define BOGUS  (TMP_DIR "/does_not_exist.pem")

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int generate_self_signed(const char *cert_path, const char *key_path, const char *cn);
PRIVATE int expect_subject_contains(hytls ytls, const char *needle, const char *label);
PRIVATE void cleanup_tmp(void);

/***************************************************************
 *              Data
 ***************************************************************/
int result = 0;

/***************************************************************************
 *              Test
 ***************************************************************************/
int main(int argc, char *argv[])
{
    gobj_start_up(
        argc,
        argv,
        NULL,   /* jn_global_settings */
        NULL,   /* persistent_attrs */
        NULL,   /* global_command_parser */
        NULL,   /* global_statistics_parser */
        NULL,   /* global_authorization_checker */
        NULL    /* global_authentication_parser */
    );

    /*-----------------------------*
     *  Setup: fresh tmp dir + certs
     *-----------------------------*/
    cleanup_tmp();
    if(mkdir(TMP_DIR, 0700) != 0) {
        fprintf(stderr, "mkdir(%s) failed\n", TMP_DIR);
        result = 1;
        goto out;
    }
    if(generate_self_signed(CERT_A, KEY_A, "ytls_test_A") != 0) {
        fprintf(stderr, "generate cert A FAILED\n");
        result = 1;
        goto out;
    }
    if(generate_self_signed(CERT_B, KEY_B, "ytls_test_B") != 0) {
        fprintf(stderr, "generate cert B FAILED\n");
        result = 1;
        goto out;
    }

    /*-----------------------------*
     *  1. Init with cert A
     *-----------------------------*/
    json_t *jn_cfg_A = json_pack("{s:s, s:s}",
        "ssl_certificate",     CERT_A,
        "ssl_certificate_key", KEY_A
    );
    hytls ytls = ytls_init(0, jn_cfg_A, TRUE);
    JSON_DECREF(jn_cfg_A);
    if(!ytls) {
        fprintf(stderr, "FAIL: ytls_init with cert A returned NULL\n");
        result = 1;
        goto out;
    }

    if(expect_subject_contains(ytls, "ytls_test_A", "after init") != 0) {
        result = 1;
        goto done;
    }

    /*-----------------------------*
     *  2. Reload to cert B
     *-----------------------------*/
    json_t *jn_cfg_B = json_pack("{s:s, s:s}",
        "ssl_certificate",     CERT_B,
        "ssl_certificate_key", KEY_B
    );
    int rc = ytls_reload_certificates(ytls, jn_cfg_B);
    JSON_DECREF(jn_cfg_B);
    if(rc != 0) {
        fprintf(stderr, "FAIL: ytls_reload_certificates(B) returned %d\n", rc);
        result = 1;
        goto done;
    }

    if(expect_subject_contains(ytls, "ytls_test_B", "after reload to B") != 0) {
        result = 1;
        goto done;
    }

    /*-----------------------------*
     *  3. Reload to invalid path; old cert (B) must survive
     *-----------------------------*/
    json_t *jn_cfg_bogus = json_pack("{s:s, s:s}",
        "ssl_certificate",     BOGUS,
        "ssl_certificate_key", BOGUS
    );
    rc = ytls_reload_certificates(ytls, jn_cfg_bogus);
    JSON_DECREF(jn_cfg_bogus);
    if(rc == 0) {
        fprintf(stderr, "FAIL: reload with bogus path unexpectedly SUCCEEDED\n");
        result = 1;
        goto done;
    }

    if(expect_subject_contains(ytls, "ytls_test_B", "after failed reload") != 0) {
        fprintf(stderr, "FAIL: failed reload corrupted the previous context\n");
        result = 1;
        goto done;
    }

    /*-----------------------------*
     *  4. Cert info fields sanity
     *-----------------------------*/
    {
        json_t *info = ytls_get_cert_info(ytls);
        if(!info) {
            fprintf(stderr, "FAIL: ytls_get_cert_info returned NULL\n");
            result = 1;
            goto done;
        }
        json_int_t not_after  = kw_get_int(0, info, "not_after", 0, 0);
        json_int_t not_before = kw_get_int(0, info, "not_before", 0, 0);
        const char *serial    = kw_get_str(0, info, "serial", "", 0);

        if(not_after <= not_before) {
            fprintf(stderr, "FAIL: not_after (%lld) <= not_before (%lld)\n",
                (long long)not_after, (long long)not_before);
            result = 1;
        }
        if(not_after <= (json_int_t)time(NULL)) {
            fprintf(stderr, "FAIL: not_after (%lld) is in the past\n",
                (long long)not_after);
            result = 1;
        }
        if(!serial || !*serial) {
            fprintf(stderr, "FAIL: serial is empty\n");
            result = 1;
        }
        JSON_DECREF(info);
    }

    printf("%s: PASS\n", APP);

done:
    ytls_cleanup(ytls);
out:
    cleanup_tmp();
    gobj_end();
    return result;
}

/***************************************************************
 *              Local Methods
 ***************************************************************/

/***************************************************************************
 *  Shell out to `openssl req` to build a throw-away self-signed cert.
 *  Using the CLI keeps the test portable across OpenSSL major versions.
 ***************************************************************************/
PRIVATE int generate_self_signed(const char *cert_path, const char *key_path, const char *cn)
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "openssl req -x509 -newkey rsa:2048 -nodes -sha256 "
        "-days 365 "
        "-keyout '%s' -out '%s' "
        "-subj '/CN=%s' "
        ">/dev/null 2>&1",
        key_path, cert_path, cn
    );
    int rc = system(cmd);
    if(rc != 0) {
        return -1;
    }
    /* Sanity: files must exist and be non-empty. */
    struct stat st;
    if(stat(cert_path, &st) != 0 || st.st_size == 0) return -1;
    if(stat(key_path,  &st) != 0 || st.st_size == 0) return -1;
    return 0;
}

/***************************************************************************
 *  Fetch cert info and assert the subject DN contains `needle`.
 ***************************************************************************/
PRIVATE int expect_subject_contains(hytls ytls, const char *needle, const char *label)
{
    json_t *info = ytls_get_cert_info(ytls);
    if(!info) {
        fprintf(stderr, "FAIL (%s): ytls_get_cert_info returned NULL\n", label);
        return -1;
    }
    const char *subject = kw_get_str(0, info, "subject", "", 0);
    int ok = (subject && strstr(subject, needle) != NULL);
    if(!ok) {
        fprintf(stderr, "FAIL (%s): subject '%s' does not contain '%s'\n",
            label, subject ? subject : "(null)", needle);
    }
    JSON_DECREF(info);
    return ok ? 0 : -1;
}

/***************************************************************************
 *  Remove TMP_DIR and its files, ignoring errors.
 ***************************************************************************/
PRIVATE void cleanup_tmp(void)
{
    unlink(CERT_A);
    unlink(KEY_A);
    unlink(CERT_B);
    unlink(KEY_B);
    rmdir(TMP_DIR);
}
