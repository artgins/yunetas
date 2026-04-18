/****************************************************************************
 *          test_cert_reload_mem.c
 *
 *          Memory-leak stress for the hot-reload path.
 *
 *          Rationale
 *          ---------
 *          When no TLS session is open, every call to
 *          ytls_reload_certificates() MUST:
 *            - build a fresh SSL_CTX / mbedtls_state
 *            - drop the ref on the old one
 *            - the old one's refcount becomes 0 AND it gets freed immediately
 *              (no sskt is holding it)
 *
 *          Running this loop many times and then asserting that
 *          get_cur_system_memory() returns to 0 after ytls_cleanup()
 *          catches a missed free in the reload path (the cheap smoke test).
 *          For the exhaustive check, run this binary under valgrind:
 *              valgrind --leak-check=full ./test_cert_reload_mem
 *
 *          Flow
 *          ----
 *          1. Generate two self-signed certs (A, B) via `openssl req`.
 *          2. ytls_init() with cert A.
 *          3. Loop N times: alternate reload to A <-> B.
 *          4. ytls_cleanup().
 *          5. Assert get_cur_system_memory() == 0 (only meaningful in debug
 *             builds with CONFIG_DEBUG_TRACK_MEMORY — a no-op otherwise, but
 *             the loop still exercises the code path for valgrind users).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#define APP "test_cert_reload_mem"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <kwid.h>
#include <gobj.h>
#include <ytls.h>

/***************************************************************
 *              Constants
 ***************************************************************/
#define TMP_DIR    "/tmp/ytls_reload_mem_test"
#define CERT_A     (TMP_DIR "/cert_A.pem")
#define KEY_A      (TMP_DIR "/key_A.pem")
#define CERT_B     (TMP_DIR "/cert_B.pem")
#define KEY_B      (TMP_DIR "/key_B.pem")

#define RELOAD_ITERS 1000

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int generate_self_signed(const char *cert_path, const char *key_path, const char *cn);
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
        argc, argv,
        NULL, NULL, NULL, NULL, NULL, NULL
    );

    /*-------------------------*
     *  Setup
     *-------------------------*/
    cleanup_tmp();
    if(mkdir(TMP_DIR, 0700) != 0) {
        fprintf(stderr, "mkdir(%s) failed\n", TMP_DIR);
        result = 1;
        goto out;
    }
    if(generate_self_signed(CERT_A, KEY_A, "ytls_mem_A") != 0 ||
       generate_self_signed(CERT_B, KEY_B, "ytls_mem_B") != 0) {
        fprintf(stderr, "cert generation FAILED\n");
        result = 1;
        goto out;
    }

    /*-------------------------*
     *  Init with cert A
     *-------------------------*/
    json_t *cfg_A = json_pack("{s:s, s:s}",
        "ssl_certificate",     CERT_A,
        "ssl_certificate_key", KEY_A
    );
    hytls ytls = ytls_init(0, cfg_A, TRUE);
    JSON_DECREF(cfg_A);
    if(!ytls) {
        fprintf(stderr, "ytls_init(A) FAILED\n");
        result = 1;
        goto out;
    }

    /*-------------------------*
     *  Reload loop
     *-------------------------*/
    for(int i = 0; i < RELOAD_ITERS; i++) {
        const char *cert = (i & 1) ? CERT_B : CERT_A;
        const char *key  = (i & 1) ? KEY_B  : KEY_A;

        json_t *cfg = json_pack("{s:s, s:s}",
            "ssl_certificate",     cert,
            "ssl_certificate_key", key
        );
        int rc = ytls_reload_certificates(ytls, cfg);
        JSON_DECREF(cfg);

        if(rc != 0) {
            fprintf(stderr, "FAIL: reload iter %d (%s) returned %d\n",
                i, (i & 1) ? "B" : "A", rc);
            result = 1;
            break;
        }
    }

    ytls_cleanup(ytls);

    if(result == 0) {
        printf("%s: %d reloads completed\n", APP, RELOAD_ITERS);
    }

out:
    cleanup_tmp();

    /*-------------------------*
     *  Memory accounting check
     *-------------------------*
     *  gobj_end() must run BEFORE get_cur_system_memory(): gobj_start_up()
     *  allocates a small amount of baseline state that is only freed by
     *  gobj_end(). Checking before that would count gobj's own baseline
     *  as a "leak" even when the reload path is clean.
     *
     *  Only effective in CONFIG_DEBUG_TRACK_MEMORY + DEBUG builds; in
     *  release builds get_cur_system_memory() returns 0 unconditionally.
     */
    gobj_end();

    size_t leaked = get_cur_system_memory();
    if(leaked != 0) {
        fprintf(stderr, "FAIL: %zu bytes still allocated after cleanup\n", leaked);
        print_track_mem();
        result = 1;
    }

    if(result == 0) {
        printf("%s: PASS\n", APP);
    } else {
        printf("%s: FAIL\n", APP);
    }
    return result;
}

/***************************************************************
 *              Local Methods
 ***************************************************************/

/***************************************************************************
 *  Shell out to `openssl req` to build a throw-away self-signed cert.
 ***************************************************************************/
PRIVATE int generate_self_signed(const char *cert_path, const char *key_path, const char *cn)
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "openssl req -x509 -newkey rsa:2048 -nodes -sha256 -days 365 "
        "-keyout '%s' -out '%s' -subj '/CN=%s' "
        ">/dev/null 2>&1",
        key_path, cert_path, cn
    );
    return system(cmd) == 0 ? 0 : -1;
}

/***************************************************************************
 *  Remove TMP_DIR and its files, ignoring errors.
 ***************************************************************************/
PRIVATE void cleanup_tmp(void)
{
    unlink(CERT_A); unlink(KEY_A);
    unlink(CERT_B); unlink(KEY_B);
    rmdir(TMP_DIR);
}
