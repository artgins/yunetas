/****************************************************************************
 *          test_peer_cert.c
 *
 *          Mutual TLS in ytls: a client that is given ssl_certificate
 *          presents it, and ytls_get_peer_cert_info() tells each end what
 *          the other presented, with "verified" only for a chain verified
 *          against the CA. On every backend compiled in (openssl, mbedtls).
 *
 *          A private CA signs the server (CN=localhost) and a client
 *          (CN=node1, DNS:node1.example). Another CA signs a rogue client
 *          (CN=node1). The handshake runs by pumping the encrypted bytes
 *          between a client filter and a server filter (no sockets).
 *
 *          Test 1: server "required", client with its CA-signed cert -> OK;
 *                  the server reads cn node1, dns node1.example, verified;
 *                  the client reads cn localhost, verified; the client's
 *                  ytls_get_cert_info() is its own (node1)
 *          Test 2: server "optional", the rogue client -> the handshake
 *                  completes, and the peer cert is NOT verified
 *          Test 3: server "required", a client without cert -> rejected
 *          Test 4: server without a CA asks no cert -> no peer cert
 *          Test 5: a client whose cert cannot be loaded -> refused (NULL),
 *                  the process goes on
 *          Test 6: a cn that is not a UTF8String -- a BMPString, a
 *                  T61String (Latin-1) -- is read as utf8 by both
 *                  backends, as the UTF8String one is (mbedTLS left a
 *                  cn of the first two out)
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#define APP "test_peer_cert"

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

/***************************************************************
 *              Constants
 ***************************************************************/
#define TMP_DIR         "/tmp/ytls_peer_cert"
#define CA_PEM          TMP_DIR "/ca.pem"
#define CA_KEY          TMP_DIR "/ca.key"
#define ROGUE_CA_PEM    TMP_DIR "/rogue_ca.pem"
#define ROGUE_CA_KEY    TMP_DIR "/rogue_ca.key"
#define SERVER_PEM      TMP_DIR "/server.pem"
#define SERVER_KEY      TMP_DIR "/server.key"
#define CLIENT_PEM      TMP_DIR "/client.pem"
#define CLIENT_KEY      TMP_DIR "/client.key"
#define ROGUE_PEM       TMP_DIR "/rogue.pem"
#define ROGUE_KEY       TMP_DIR "/rogue.key"

/*
 *  The cn "nodé1" in three ASN.1 string types, by the string_mask of
 *  openssl req: pkix writes a BMPString, nombstr a T61String, utf8only a
 *  UTF8String
 */
#define CN_NON_ASCII    "nod\xc3\xa9" "1"
PRIVATE const char *cn_string_masks[] = {"pkix", "nombstr", "utf8only"};

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

/*
 *  What each end read of its peer, after the handshake
 */
PRIVATE json_t *server_saw = NULL;
PRIVATE json_t *client_saw = NULL;
PRIVATE json_t *client_own = NULL;

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
        printf("ok   [%s] %s\n", library, name);
    } else {
        printf("FAIL [%s] %s\n", library, name);
        global_result += -1;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int run(const char *cmd)
{
    char line[6*PATH_MAX];
    snprintf(line, sizeof(line), "%s >/dev/null 2>&1", cmd);
    return system(line);
}

/***************************************************************************
 *  A CA, and a leaf signed by it with these extensions
 ***************************************************************************/
PRIVATE int make_ca(const char *pem, const char *key, const char *cn)
{
    char cmd[5*PATH_MAX];
    snprintf(cmd, sizeof(cmd),
        "openssl req -x509 -newkey rsa:2048 -nodes -sha256 -days 30 "
        "-keyout '%s' -out '%s' -subj '/CN=%s' "
        "-addext 'basicConstraints=critical,CA:TRUE' "
        "-addext 'keyUsage=critical,keyCertSign,cRLSign'",
        key, pem, cn
    );
    return run(cmd);
}

PRIVATE int make_leaf(
    const char *pem,
    const char *key,
    const char *cn,
    const char *ext,
    const char *ca_pem,
    const char *ca_key
)
{
    char ext_path[PATH_MAX];
    char csr_path[PATH_MAX];
    snprintf(ext_path, sizeof(ext_path), "%s.ext", pem);
    snprintf(csr_path, sizeof(csr_path), "%s.csr", pem);
    FILE *f = fopen(ext_path, "w");
    if(!f) {
        return -1;
    }
    fprintf(f, "basicConstraints=CA:FALSE\n%s\n", ext);
    fclose(f);

    char cmd[5*PATH_MAX];
    snprintf(cmd, sizeof(cmd),
        "openssl req -newkey rsa:2048 -nodes -sha256 -keyout '%s' -out '%s' -subj '/CN=%s'",
        key, csr_path, cn
    );
    if(run(cmd) != 0) {
        return -1;
    }
    snprintf(cmd, sizeof(cmd),
        "openssl x509 -req -in '%s' -CA '%s' -CAkey '%s' -CAcreateserial "
        "-days 30 -sha256 -extfile '%s' -out '%s'",
        csr_path, ca_pem, ca_key, ext_path, pem
    );
    return run(cmd);
}

/***************************************************************************
 *  A self-signed cert whose cn is CN_NON_ASCII, written as `string_mask`
 *  says
 ***************************************************************************/
PRIVATE int make_cn_cert(const char *string_mask)
{
    char cnf_path[PATH_MAX];
    snprintf(cnf_path, sizeof(cnf_path), "%s/cn_%s.cnf", TMP_DIR, string_mask);
    FILE *f = fopen(cnf_path, "w");
    if(!f) {
        return -1;
    }
    fprintf(f, "[req]\ndistinguished_name = dn\nstring_mask = %s\n[dn]\n", string_mask);
    fclose(f);

    char cmd[5*PATH_MAX];
    snprintf(cmd, sizeof(cmd),
        "openssl req -x509 -config '%s' -utf8 -newkey rsa:2048 -nodes -sha256 -days 30 "
        "-keyout '%s/cn_%s.key' -out '%s/cn_%s.pem' -subj '/CN=%s'",
        cnf_path, TMP_DIR, string_mask, TMP_DIR, string_mask, CN_NON_ASCII
    );
    return run(cmd);
}

PRIVATE int make_pki(void)
{
    if(mkdir(TMP_DIR, 0700) != 0) {
        return -1;
    }
    if(make_ca(CA_PEM, CA_KEY, "Test CA") != 0 ||
            make_ca(ROGUE_CA_PEM, ROGUE_CA_KEY, "Rogue CA") != 0 ||
            make_leaf(SERVER_PEM, SERVER_KEY, "localhost",
                "subjectAltName=DNS:localhost\nextendedKeyUsage=serverAuth",
                CA_PEM, CA_KEY) != 0 ||
            make_leaf(CLIENT_PEM, CLIENT_KEY, "node1",
                "subjectAltName=DNS:node1.example\nextendedKeyUsage=clientAuth",
                CA_PEM, CA_KEY) != 0 ||
            make_leaf(ROGUE_PEM, ROGUE_KEY, "node1",
                "subjectAltName=DNS:node1.example\nextendedKeyUsage=clientAuth",
                ROGUE_CA_PEM, ROGUE_CA_KEY) != 0) {
        return -1;
    }
    for(size_t i = 0; i < sizeof(cn_string_masks)/sizeof(cn_string_masks[0]); i++) {
        if(make_cn_cert(cn_string_masks[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
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

/***************************************************************************
 *  One handshake. Returns 0 when both ends finished it, -1 when one did not;
 *  what each end read of the other is left in server_saw / client_saw
 ***************************************************************************/
PRIVATE int attempt(json_t *server_cfg, json_t *client_cfg)
{
    int ret = -1;
    JSON_DECREF(server_saw)
    JSON_DECREF(client_saw)
    JSON_DECREF(client_own)

    hytls s_ytls = ytls_init(0, server_cfg, TRUE);
    hytls c_ytls = ytls_init(0, client_cfg, FALSE);
    JSON_DECREF(server_cfg)
    JSON_DECREF(client_cfg)
    if(!s_ytls || !c_ytls) {
        if(s_ytls) {
            ytls_cleanup(s_ytls);
        }
        if(c_ytls) {
            ytls_cleanup(c_ytls);
        }
        return -99;
    }

    hs_done[0] = hs_done[1] = 0;
    hs_error[0] = hs_error[1] = 0;
    cli_to_srv = gbuffer_create(64*1024, 64*1024);
    srv_to_cli = gbuffer_create(64*1024, 64*1024);

    hsskt s = ytls_new_secure_filter(s_ytls, on_handshake_done, on_clear_data, on_encrypted_data, &tag_server);
    hsskt c = ytls_new_secure_filter(c_ytls, on_handshake_done, on_clear_data, on_encrypted_data, &tag_client);
    if(s && c) {
        ytls_do_handshake(c_ytls, c);
        for(int i = 0; i < 50 && (!hs_done[0] || !hs_done[1]); i++) {
            drain(s_ytls, s, cli_to_srv);
            ytls_do_handshake(s_ytls, s);
            drain(c_ytls, c, srv_to_cli);
            ytls_do_handshake(c_ytls, c);
        }
        if(hs_done[0] && hs_done[1] && hs_error[0] == 0 && hs_error[1] == 0) {
            ret = 0;
        }
        server_saw = ytls_get_peer_cert_info(s_ytls, s);
        client_saw = ytls_get_peer_cert_info(c_ytls, c);
        client_own = ytls_get_cert_info(c_ytls);
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
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL has_dns(json_t *info, const char *name)
{
    size_t idx; json_t *jn;
    json_array_foreach(json_object_get(info, "dns_names"), idx, jn) {
        if(strcmp(json_string_value(jn), name) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

PRIVATE const char *cn_of(json_t *info)
{
    const char *cn = json_string_value(json_object_get(info, "cn"));
    return cn? cn : "";
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *server_config(const char *library, const char *verify_mode, BOOL with_ca)
{
    json_t *cfg = json_pack("{s:s, s:s, s:s}",
        "library",              library,
        "ssl_certificate",      SERVER_PEM,
        "ssl_certificate_key",  SERVER_KEY
    );
    if(with_ca) {
        json_object_set_new(cfg, "ssl_trusted_certificate", json_string(CA_PEM));
    }
    if(verify_mode) {
        json_object_set_new(cfg, "ssl_verify_mode", json_string(verify_mode));
    }
    return cfg;
}

PRIVATE json_t *client_config(const char *library, const char *pem, const char *key)
{
    json_t *cfg = json_pack("{s:s, s:s, s:s}",
        "library",                  library,
        "ssl_trusted_certificate",  CA_PEM,
        "ssl_server_name",          "localhost"
    );
    if(pem) {
        json_object_set_new(cfg, "ssl_certificate", json_string(pem));
        json_object_set_new(cfg, "ssl_certificate_key", json_string(key));
    }
    return cfg;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_library(const char *library)
{
    /*
     *  1: mutual TLS, both verified
     */
    int e = attempt(
        server_config(library, "required", TRUE),
        client_config(library, CLIENT_PEM, CLIENT_KEY)
    );
    ok_or_fail(e == 0, library, "1 required + a client cert of the CA: handshake OK");
    ok_or_fail(server_saw && strcmp(cn_of(server_saw), "node1") == 0,
        library, "1 the server reads the cn of the client: node1");
    ok_or_fail(server_saw && has_dns(server_saw, "node1.example"),
        library, "1 the server reads the DNS name of the client: node1.example");
    ok_or_fail(server_saw && json_is_true(json_object_get(server_saw, "verified")),
        library, "1 the client cert is verified");
    ok_or_fail(client_saw && strcmp(cn_of(client_saw), "localhost") == 0 &&
            json_is_true(json_object_get(client_saw, "verified")),
        library, "1 the client reads the server cert, verified: localhost");
    ok_or_fail(client_own && strcmp(cn_of(client_own), "node1") == 0,
        library, "1 ytls_get_cert_info() of the client is its own cert");

    /*
     *  2: optional, a cert of another CA: in, not verified
     */
    printf("     (a WARNING of a peer cert that did not verify follows, on purpose)\n");
    e = attempt(
        server_config(library, "optional", TRUE),
        client_config(library, ROGUE_PEM, ROGUE_KEY)
    );
    ok_or_fail(e == 0, library, "2 optional + a client cert of another CA: handshake completes");
    ok_or_fail(server_saw && strcmp(cn_of(server_saw), "node1") == 0 &&
            json_is_false(json_object_get(server_saw, "verified")),
        library, "2 the server reads cn node1 NOT verified");

    /*
     *  3: required, no client cert
     */
    printf("     (WARNINGs of a rejected handshake follow, on purpose)\n");
    e = attempt(
        server_config(library, "required", TRUE),
        client_config(library, NULL, NULL)
    );
    ok_or_fail(e != 0, library, "3 required + a client without cert: handshake rejected");
    ok_or_fail(!server_saw, library, "3 the server has no peer cert");

    /*
     *  4: a server without CA asks no cert
     */
    e = attempt(
        server_config(library, NULL, FALSE),
        client_config(library, CLIENT_PEM, CLIENT_KEY)
    );
    ok_or_fail(e == 0, library, "4 a server without CA: handshake OK");
    ok_or_fail(!server_saw, library, "4 a server without CA has no peer cert");

    /*
     *  5: a client cert that cannot be loaded: refused, not fatal
     */
    printf("     (an ERROR of a client cert not loaded follows, on purpose)\n");
    json_t *cfg = client_config(library, TMP_DIR "/missing.pem", CLIENT_KEY);
    hytls c = ytls_init(0, cfg, FALSE);
    JSON_DECREF(cfg)
    ok_or_fail(c == NULL, library, "5 a client cert that cannot be loaded: the client is refused");
    if(c) {
        ytls_cleanup(c);
    }

    /*
     *  6: a cn in a BMPString or a T61String reads as the UTF8String one
     */
    for(size_t i = 0; i < sizeof(cn_string_masks)/sizeof(cn_string_masks[0]); i++) {
        char pem[PATH_MAX];
        char key[PATH_MAX];
        snprintf(pem, sizeof(pem), "%s/cn_%s.pem", TMP_DIR, cn_string_masks[i]);
        snprintf(key, sizeof(key), "%s/cn_%s.key", TMP_DIR, cn_string_masks[i]);
        cfg = client_config(library, pem, key);
        c = ytls_init(0, cfg, FALSE);
        JSON_DECREF(cfg)
        json_t *own = c? ytls_get_cert_info(c) : NULL;
        char name[128];
        snprintf(name, sizeof(name), "6 string_mask %s: the cn is '%s' (got '%s')",
            cn_string_masks[i], CN_NON_ASCII, own? cn_of(own) : "(no cert info)");
        ok_or_fail(own && strcmp(cn_of(own), CN_NON_ASCII) == 0, library, name);
        JSON_DECREF(own)
        if(c) {
            ytls_cleanup(c);
        }
    }

    JSON_DECREF(server_saw)
    JSON_DECREF(client_saw)
    JSON_DECREF(client_own)
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
    if(make_pki() != 0) {
        printf("FAIL the certificates cannot be made (is `openssl` installed?)\n");
        global_result += -1;
    } else {
#ifdef CONFIG_HAVE_OPENSSL
        test_library("openssl");
#endif
#ifdef CONFIG_HAVE_MBEDTLS
        test_library("mbedtls");
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
