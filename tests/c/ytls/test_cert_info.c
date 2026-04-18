/****************************************************************************
 *          test_cert_info.c
 *
 *          Edge-case coverage for ytls_get_cert_info() — the introspection
 *          API that feeds the c_yuno cert-expiry monitor.
 *
 *          Cases
 *          -----
 *          1. Short-validity cert (`-days 1`): days_remaining is 0 or 1,
 *             not_after is within ~26 hours of now, not_after > now.
 *          2. Long-validity cert (`-days 365`): days_remaining is >= 360
 *             (a touch less than 365 due to rounding and "seconds to midnight"
 *             semantics — the lower bound guards against a clearly wrong
 *             value while tolerating small clock skew).
 *          3. Self-signed: subject == issuer (one of the core properties of
 *             self-signed certs; tests that both fields are populated and
 *             that the backend renders them with the same format).
 *          4. Serial non-empty and hex-uppercase (matches the OpenSSL
 *             format; mbedtls backend strips colons and upper-cases to
 *             produce the same shape).
 *          5. Client-side ytls (no ssl_certificate configured): cert info
 *             must be NULL.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#define APP "test_cert_info"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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
#define TMP_DIR   "/tmp/ytls_cert_info_test"
#define CERT_1D  (TMP_DIR "/cert_1d.pem")
#define KEY_1D   (TMP_DIR "/key_1d.pem")
#define CERT_365 (TMP_DIR "/cert_365d.pem")
#define KEY_365  (TMP_DIR "/key_365d.pem")
#define CERT_EXP (TMP_DIR "/cert_expired.pem")
#define KEY_EXP  (TMP_DIR "/key_expired.pem")

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int generate_self_signed(const char *cert_path, const char *key_path,
                                 const char *cn, int days);
PRIVATE int generate_expired(const char *cert_path, const char *key_path, const char *cn);
PRIVATE void cleanup_tmp(void);

PRIVATE int test_short_validity(void);
PRIVATE int test_long_validity(void);
PRIVATE int test_client_no_cert(void);
PRIVATE int test_expired_cert(void);

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
        argc, argv,
        NULL, NULL, NULL, NULL, NULL, NULL
    );

    cleanup_tmp();
    if(mkdir(TMP_DIR, 0700) != 0) {
        fprintf(stderr, "mkdir(%s) failed\n", TMP_DIR);
        result = 1;
        goto out;
    }
    if(generate_self_signed(CERT_1D,  KEY_1D,  "ytls_info_1d",  1)   != 0 ||
       generate_self_signed(CERT_365, KEY_365, "ytls_info_365", 365) != 0 ||
       generate_expired(CERT_EXP, KEY_EXP, "ytls_info_expired")      != 0) {
        fprintf(stderr, "cert generation FAILED (is `openssl` installed?)\n");
        result = 1;
        goto out;
    }

    result += test_short_validity();
    result += test_long_validity();
    result += test_client_no_cert();
    result += test_expired_cert();

    if(result == 0) {
        printf("%s: PASS\n", APP);
    } else {
        printf("%s: FAIL (%d subtest(s) failed)\n", APP, result);
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
 *  Shell out to `openssl req` to build a throw-away self-signed cert
 *  with `-days` validity.
 ***************************************************************************/
PRIVATE int generate_self_signed(const char *cert_path, const char *key_path,
                                 const char *cn, int days)
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "openssl req -x509 -newkey rsa:2048 -nodes -sha256 "
        "-days %d "
        "-keyout '%s' -out '%s' "
        "-subj '/CN=%s' "
        ">/dev/null 2>&1",
        days, key_path, cert_path, cn
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
 *  Build an already-expired self-signed cert.
 *
 *  Trick: `openssl x509 -req -days -1` makes notAfter = notBefore - 1 day,
 *  so the cert is expired the moment it is issued. Accepted by OpenSSL's
 *  cert loader — we want ytls_init() to still load it and ytls_get_cert_info()
 *  to report a past not_after so the monitor can flag it.
 ***************************************************************************/
PRIVATE int generate_expired(const char *cert_path, const char *key_path, const char *cn)
{
    char csr_path[512];
    snprintf(csr_path, sizeof(csr_path), "%s.csr", cert_path);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "openssl genrsa -out '%s' 2048 >/dev/null 2>&1 && "
        "openssl req -new -key '%s' -subj '/CN=%s' -out '%s' >/dev/null 2>&1 && "
        "openssl x509 -req -in '%s' -signkey '%s' -days -1 -out '%s' >/dev/null 2>&1",
        key_path,
        key_path, cn, csr_path,
        csr_path, key_path, cert_path
    );
    int rc = system(cmd);
    unlink(csr_path);
    if(rc != 0) {
        return -1;
    }
    struct stat st;
    if(stat(cert_path, &st) != 0 || st.st_size == 0) return -1;
    if(stat(key_path,  &st) != 0 || st.st_size == 0) return -1;
    return 0;
}

/***************************************************************************
 *  Remove TMP_DIR and its files, ignoring errors.
 ***************************************************************************/
PRIVATE void cleanup_tmp(void)
{
    unlink(CERT_1D);
    unlink(KEY_1D);
    unlink(CERT_365);
    unlink(KEY_365);
    unlink(CERT_EXP);
    unlink(KEY_EXP);
    rmdir(TMP_DIR);
}

/***************************************************************************
 *  Case 1: short-validity cert.
 *  days_remaining computed at the call site should be 0 or 1 (depending
 *  on fractional seconds). not_after must be within ~26 hours of "now"
 *  to catch egregious time-zone / epoch mistakes.
 ***************************************************************************/
PRIVATE int test_short_validity(void)
{
    int fails = 0;

    json_t *cfg = json_pack("{s:s, s:s}",
        "ssl_certificate",     CERT_1D,
        "ssl_certificate_key", KEY_1D
    );
    hytls ytls = ytls_init(0, cfg, TRUE);
    JSON_DECREF(cfg);
    if(!ytls) {
        fprintf(stderr, "short_validity: ytls_init returned NULL\n");
        return 1;
    }

    json_t *info = ytls_get_cert_info(ytls);
    if(!info) {
        fprintf(stderr, "short_validity: get_cert_info returned NULL\n");
        fails++;
        goto done;
    }

    const char *subject = kw_get_str(0, info, "subject", "", 0);
    json_int_t not_after = kw_get_int(0, info, "not_after", 0, 0);
    time_t now = time(NULL);
    json_int_t delta = not_after - (json_int_t)now;

    if(!subject || !strstr(subject, "ytls_info_1d")) {
        fprintf(stderr, "short_validity: subject '%s' does not contain 'ytls_info_1d'\n",
            subject ? subject : "(null)");
        fails++;
    }
    if(delta <= 0) {
        fprintf(stderr, "short_validity: not_after (%lld) is not in the future of now (%lld)\n",
            (long long)not_after, (long long)now);
        fails++;
    }
    /* 1 day = 86400 s; allow +26h slack for system clock jitter. */
    if(delta > 26 * 3600) {
        fprintf(stderr, "short_validity: not_after delta %lld s is too large (> 26h)\n",
            (long long)delta);
        fails++;
    }

    JSON_DECREF(info);
done:
    ytls_cleanup(ytls);
    return fails;
}

/***************************************************************************
 *  Case 2: long-validity cert (365 days).
 *  days_remaining must be >= 360 (cert was just minted; tolerate a few
 *  days of slack for clock skew or certificate-rounding differences).
 *  Also asserts the self-signed invariant subject == issuer, and that
 *  serial is non-empty hex-uppercase.
 ***************************************************************************/
PRIVATE int test_long_validity(void)
{
    int fails = 0;

    json_t *cfg = json_pack("{s:s, s:s}",
        "ssl_certificate",     CERT_365,
        "ssl_certificate_key", KEY_365
    );
    hytls ytls = ytls_init(0, cfg, TRUE);
    JSON_DECREF(cfg);
    if(!ytls) {
        fprintf(stderr, "long_validity: ytls_init returned NULL\n");
        return 1;
    }

    json_t *info = ytls_get_cert_info(ytls);
    if(!info) {
        fprintf(stderr, "long_validity: get_cert_info returned NULL\n");
        fails++;
        goto done;
    }

    const char *subject = kw_get_str(0, info, "subject", "", 0);
    const char *issuer  = kw_get_str(0, info, "issuer",  "", 0);
    const char *serial  = kw_get_str(0, info, "serial",  "", 0);
    json_int_t not_after = kw_get_int(0, info, "not_after", 0, 0);

    time_t now = time(NULL);
    json_int_t days_remaining = (not_after - (json_int_t)now) / 86400;

    if(days_remaining < 360) {
        fprintf(stderr, "long_validity: days_remaining %lld is too small (expected >= 360)\n",
            (long long)days_remaining);
        fails++;
    }

    /* Self-signed: subject must equal issuer. */
    if(!subject || !issuer || strcmp(subject, issuer) != 0) {
        fprintf(stderr,
            "long_validity: self-signed cert should have subject == issuer\n"
            "  subject='%s'\n"
            "  issuer ='%s'\n",
            subject ? subject : "(null)",
            issuer  ? issuer  : "(null)");
        fails++;
    }

    /* Serial: non-empty, hex, uppercase. */
    if(!serial || !*serial) {
        fprintf(stderr, "long_validity: serial is empty\n");
        fails++;
    } else {
        for(const char *p = serial; *p; p++) {
            int c = (unsigned char)*p;
            BOOL is_upper_hex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
            if(!is_upper_hex) {
                fprintf(stderr,
                    "long_validity: serial '%s' contains non-uppercase-hex char '%c'\n",
                    serial, (char)c);
                fails++;
                break;
            }
        }
    }

    JSON_DECREF(info);
done:
    ytls_cleanup(ytls);
    return fails;
}

/***************************************************************************
 *  Case 3: client-side ytls (no ssl_certificate). get_cert_info must
 *  return NULL because there is nothing to introspect.
 ***************************************************************************/
PRIVATE int test_client_no_cert(void)
{
    json_t *cfg = json_object();  /* empty config: no cert */
    hytls ytls = ytls_init(0, cfg, FALSE /* server=FALSE */);
    JSON_DECREF(cfg);
    if(!ytls) {
        fprintf(stderr, "client_no_cert: ytls_init returned NULL\n");
        return 1;
    }

    int fails = 0;
    json_t *info = ytls_get_cert_info(ytls);
    if(info) {
        fprintf(stderr, "client_no_cert: get_cert_info unexpectedly returned a non-NULL object\n");
        JSON_DECREF(info);
        fails++;
    }

    ytls_cleanup(ytls);
    return fails;
}

/***************************************************************************
 *  Case 4: already-expired cert.
 *  ytls_init() must still succeed (it does not check cert validity at load
 *  time; clients do that during the TLS handshake). ytls_get_cert_info()
 *  must report not_after < now, i.e. the monitor's days_remaining computation
 *  yields a NEGATIVE value — which is the whole point of the expiry monitor.
 ***************************************************************************/
PRIVATE int test_expired_cert(void)
{
    int fails = 0;

    json_t *cfg = json_pack("{s:s, s:s}",
        "ssl_certificate",     CERT_EXP,
        "ssl_certificate_key", KEY_EXP
    );
    hytls ytls = ytls_init(0, cfg, TRUE);
    JSON_DECREF(cfg);
    if(!ytls) {
        fprintf(stderr, "expired_cert: ytls_init returned NULL (loader rejected expired cert?)\n");
        return 1;
    }

    json_t *info = ytls_get_cert_info(ytls);
    if(!info) {
        fprintf(stderr, "expired_cert: get_cert_info returned NULL\n");
        fails++;
        goto done;
    }

    json_int_t not_after = kw_get_int(0, info, "not_after", 0, 0);
    time_t now = time(NULL);

    if(not_after <= 0) {
        fprintf(stderr, "expired_cert: not_after is zero/invalid (%lld)\n",
            (long long)not_after);
        fails++;
    } else if(not_after >= (json_int_t)now) {
        fprintf(stderr,
            "expired_cert: not_after (%lld) is not in the past (now=%lld)\n",
            (long long)not_after, (long long)now);
        fails++;
    }

    /* The c_yuno monitor computes days_remaining = (not_after - now) / 86400.
     * For an expired cert that value MUST be <= 0 so the log_critical path
     * fires (cert_critical_days defaults to 2 in c_yuno).
     */
    json_int_t days_remaining = (not_after - (json_int_t)now) / 86400;
    if(days_remaining > 0) {
        fprintf(stderr, "expired_cert: days_remaining %lld > 0 (should be <= 0)\n",
            (long long)days_remaining);
        fails++;
    }

    JSON_DECREF(info);
done:
    ytls_cleanup(ytls);
    return fails;
}
