/****************************************************************************
 *          test_server_init_not_fatal.c
 *
 *          A SERVER whose TLS context cannot be built gets NULL from
 *          ytls_init(), with the ERROR of its cause, and the process goes
 *          on: it is never fatal. Its owner degrades that listener (C_TCP_S
 *          does not listen, and tries again); the yuno keeps its other
 *          services. Up to 7.30.17 the OpenSSL backend exited the process
 *          with code 0 there (LOG_OPT_EXIT_ZERO), the mbedTLS backend since
 *          7.30.15, and an unknown `library` exited it for a client too.
 *          Up to 7.30.18 the mbedTLS backend gave a context for 2 and 3, and
 *          took the reload of 5: it did not check the pair, and took a
 *          server with no certificate. The server listened, said its TLS
 *          context was loaded, and every handshake failed; the reload
 *          dropped the good context.
 *
 *          On each backend compiled in:
 *          1. the certificate file does not exist  -> NULL
 *          2. the key does not match the certificate -> NULL
 *          3. no `ssl_certificate` at all -> NULL
 *          4. the trusted certificate does not exist -> NULL
 *          5. a good pair -> a context (the cases above were not "no
 *             server can be built here"); then a reload of a new
 *             certificate with the old key (the copy of a pair half done)
 *             -> -1, and the context of the good pair kept
 *          And `library` unknown -> NULL, for a server and for a client.
 *
 *          The red of this test is an EXIT with code 0, which ctest would
 *          take for a pass: an atexit() handler turns an exit before the
 *          end of the test into code 1.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#define APP "test_server_init_not_fatal"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <yuneta_config.h>
#include <kwid.h>
#include <gobj.h>
#include <ytls.h>

/***************************************************************
 *              Constants
 ***************************************************************/
#define TMP_DIR "/tmp/ytls_server_init_not_fatal"
#define CERT_A  (TMP_DIR "/cert_A.pem")
#define KEY_A   (TMP_DIR "/key_A.pem")
#define CERT_B  (TMP_DIR "/cert_B.pem")
#define KEY_B   (TMP_DIR "/key_B.pem")
#define MISSING (TMP_DIR "/does_not_exist.pem")

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int generate_self_signed(const char *cert_path, const char *key_path, const char *cn);
PRIVATE void expect_null(const char *library, const char *what, json_t *jn_cfg, BOOL server);
PRIVATE void cleanup_tmp(void);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int result = 0;
PRIVATE int reached_end = 0;

/***************************************************************************
 *  An exit before the end of the test (LOG_OPT_EXIT_ZERO) is a failure
 ***************************************************************************/
PRIVATE void check_reached_end(void)
{
    if(!reached_end) {
        printf("%s: FAIL, the process exited before the end of the test\n", APP);
        fflush(stdout);
        _exit(1);
    }
}

/***************************************************************************
 *              Test
 ***************************************************************************/
int main(int argc, char *argv[])
{
    atexit(check_reached_end);

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

    const char *libraries[] = {
#ifdef CONFIG_HAVE_OPENSSL
        "openssl",
#endif
#ifdef CONFIG_HAVE_MBEDTLS
        "mbedtls",
#endif
        0
    };

    cleanup_tmp();
    if(mkdir(TMP_DIR, 0700) != 0 ||
        generate_self_signed(CERT_A, KEY_A, "ytls_not_fatal_A") != 0 ||
        generate_self_signed(CERT_B, KEY_B, "ytls_not_fatal_B") != 0
    ) {
        printf("%s: FAIL, cannot make the test certificates in %s\n", APP, TMP_DIR);
        result = 1;
        goto out;
    }

    for(int i=0; libraries[i]; i++) {
        const char *library = libraries[i];

        expect_null(library, "certificate missing", json_pack("{s:s, s:s, s:s}",
            "library",              library,
            "ssl_certificate",      MISSING,
            "ssl_certificate_key",  KEY_A
        ), TRUE);

        expect_null(library, "key of another certificate", json_pack("{s:s, s:s, s:s}",
            "library",              library,
            "ssl_certificate",      CERT_A,
            "ssl_certificate_key",  KEY_B
        ), TRUE);

        expect_null(library, "no certificate", json_pack("{s:s, s:s}",
            "library",              library,
            "ssl_certificate_key",  KEY_A
        ), TRUE);

        expect_null(library, "trusted certificate missing", json_pack("{s:s, s:s, s:s, s:s}",
            "library",                  library,
            "ssl_certificate",          CERT_A,
            "ssl_certificate_key",      KEY_A,
            "ssl_trusted_certificate",  MISSING
        ), TRUE);

        json_t *jn_cfg = json_pack("{s:s, s:s, s:s}",
            "library",              library,
            "ssl_certificate",      CERT_A,
            "ssl_certificate_key",  KEY_A
        );
        hytls ytls = ytls_init(0, jn_cfg, TRUE);
        JSON_DECREF(jn_cfg)
        if(!ytls) {
            printf("%s: FAIL, %s: a good pair gives no server context\n", APP, library);
            result = 1;
        } else {
            /*
             *  A new certificate with the old key (a copy of the pair
             *  half done): refused, the context of A kept
             */
            jn_cfg = json_pack("{s:s, s:s, s:s}",
                "library",              library,
                "ssl_certificate",      CERT_B,
                "ssl_certificate_key",  KEY_A
            );
            if(ytls_reload_certificates(ytls, jn_cfg) == 0) {
                printf("%s: FAIL, %s: a reload with the key of another certificate answered 0\n",
                    APP, library);
                result = 1;
            }
            JSON_DECREF(jn_cfg)
            json_t *jn_info = ytls_get_cert_info(ytls);
            const char *subject = kw_get_str(0, jn_info, "subject", "", 0);
            if(!strstr(subject, "ytls_not_fatal_A")) {
                printf("%s: FAIL, %s: a refused reload did not keep the context: '%s'\n",
                    APP, library, subject);
                result = 1;
            }
            JSON_DECREF(jn_info)
            ytls_cleanup(ytls);
        }
    }

    expect_null("nope", "unknown library, server", json_pack("{s:s, s:s, s:s}",
        "library",              "nope",
        "ssl_certificate",      CERT_A,
        "ssl_certificate_key",  KEY_A
    ), TRUE);
    expect_null("nope", "unknown library, client", json_pack("{s:s, s:b}",
        "library",                      "nope",
        "ssl_allow_insecure_client",    1
    ), FALSE);

out:
    cleanup_tmp();
    gobj_end();
    reached_end = 1;
    printf("%s: %s\n", APP, result? "FAIL" : "PASS");
    return result;
}

/***************************************************************
 *              Local Methods
 ***************************************************************/

/***************************************************************************
 *  ytls_init() must answer NULL, and come back. jn_cfg is owned
 ***************************************************************************/
PRIVATE void expect_null(const char *library, const char *what, json_t *jn_cfg, BOOL server)
{
    hytls ytls = ytls_init(0, jn_cfg, server);
    JSON_DECREF(jn_cfg)
    if(ytls) {
        printf("%s: FAIL, %s: %s gave a context\n", APP, library, what);
        ytls_cleanup(ytls);
        result = 1;
    }
}

/***************************************************************************
 *  A throw-away self-signed certificate, with the openssl CLI
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
    if(system(cmd) != 0) {
        return -1;
    }
    struct stat st;
    if(stat(cert_path, &st) != 0 || st.st_size == 0) {
        return -1;
    }
    if(stat(key_path, &st) != 0 || st.st_size == 0) {
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Remove TMP_DIR and its files, ignoring errors
 ***************************************************************************/
PRIVATE void cleanup_tmp(void)
{
    unlink(CERT_A);
    unlink(KEY_A);
    unlink(CERT_B);
    unlink(KEY_B);
    rmdir(TMP_DIR);
}
