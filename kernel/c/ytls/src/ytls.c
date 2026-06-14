/***********************************************************************
 *          YTLS.C
 *
 *          TLS for Yuneta
 *
 *          Copyright (c) 2018 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>
#include <unistd.h>

#include <kwid.h>
#include "ytls.h"
#include "tls/openssl.h"
#include "tls/mbedtls.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
    Locate a readable system CA bundle FILE, portable across Linux distros.

    The hashed-dir (OpenSSL CApath) layout is NOT portable: RHEL/Rocky ship
    only bundle files under /etc/pki/tls/certs with no <hash>.0 links, so a
    directory load finds nothing there. We probe well-known bundle files (the
    same list Go's crypto/x509 and curl use) and return the first readable one.

    Needed because a fully-static binary does not inherit the host's OPENSSLDIR
    or SSL_CERT_FILE, so OpenSSL's SSL_CTX_set_default_verify_paths() loads an
    empty store and verification fails on a perfectly valid public cert; and
    mbedTLS has no system trust store at all.
 ***************************************************************************/
PUBLIC const char *ytls_get_system_ca_bundle(void)
{
    static const char *candidates[] = {
        "/etc/ssl/certs/ca-certificates.crt",                  // Debian, Ubuntu, Alpine
        "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",   // RHEL7+/Rocky/Alma/Fedora
        "/etc/ssl/cert.pem",                                   // Rocky (symlink), Alpine, BSD
        "/etc/pki/tls/certs/ca-bundle.crt",                    // RHEL/Rocky/Fedora
        "/etc/ssl/ca-bundle.pem",                              // SUSE/SLES
        0
    };
    for(int i=0; candidates[i]; i++) {
        if(access(candidates[i], R_OK)==0) {
            return candidates[i];
        }
    }
    return NULL;
}

/***************************************************************************
    Startup tls
 ***************************************************************************/
PUBLIC hytls ytls_init(
    hgobj gobj,
    json_t *jn_config,  // not owned
    BOOL server
)
{
    /*--------------------------------*
     *      Load tls library
     *--------------------------------*/
    api_tls_t *api_tls = 0;

    const char *tls_library = kw_get_str(gobj, jn_config, "library", TLS_LIBRARY_NAME, 0);
    SWITCHS(tls_library) {
#ifdef CONFIG_HAVE_OPENSSL
        ICASES("openssl")
            api_tls = openssl_api_tls();
            break;
#endif

#ifdef CONFIG_HAVE_MBEDTLS
        ICASES("mbedtls")
            api_tls = mbed_api_tls();
        break;
#endif

        DEFAULTS
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_INTERNAL,
                "msg",              "%s", "tls_library NOT DEFINED",
                "library",          "%s", tls_library,
                NULL
            );
        break;
    } SWITCHS_END;

    if(!api_tls) {
        /*
         *  Exit without let re-starting
         */
        gobj_log_error(gobj, LOG_OPT_EXIT_ZERO,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "api_tls() FAILED",
            "library",          "%s", tls_library,
            NULL
        );
        return 0;
    }

    /*--------------------------------*
     *      Init tls
     *--------------------------------*/
    return api_tls->init(gobj, jn_config, server);
}

/***************************************************************************
    Cleanup tls
 ***************************************************************************/
PUBLIC void ytls_cleanup(hytls ytls)
{
    api_tls_t *api_tls = ((__ytls_t__ *)ytls)->api_tls;
    api_tls->cleanup(ytls);
}

/***************************************************************************
    Reload certificates (hot-reload)
 ***************************************************************************/
PUBLIC int ytls_reload_certificates(hytls ytls, json_t *jn_config)
{
    if(!ytls) {
        return -1;
    }
    api_tls_t *api_tls = ((__ytls_t__ *)ytls)->api_tls;
    if(!api_tls->reload_certificates) {
        return -1;
    }
    return api_tls->reload_certificates(ytls, jn_config);
}

/***************************************************************************
    Get cert info (subject, issuer, not_before, not_after, serial)
 ***************************************************************************/
PUBLIC json_t *ytls_get_cert_info(hytls ytls)
{
    if(!ytls) {
        return NULL;
    }
    api_tls_t *api_tls = ((__ytls_t__ *)ytls)->api_tls;
    if(!api_tls->get_cert_info) {
        return NULL;
    }
    return api_tls->get_cert_info(ytls);
}


/***************************************************************************
    Version tls
 ***************************************************************************/
PUBLIC const char * ytls_version(hytls ytls)
{
    api_tls_t *api_tls = ((__ytls_t__ *)ytls)->api_tls;
    return api_tls->version(ytls);
}

/***************************************************************************
    New secure filter
 ***************************************************************************/
PUBLIC hsskt ytls_new_secure_filter(
    hytls ytls,
    int (*on_handshake_done_cb)(void *user_data, int error),
    int (*on_clear_data_cb)(
        void *user_data,
        gbuffer_t *gbuf // must be decref
    ),
    int (*on_encrypted_data_cb)(
        void *user_data,
        gbuffer_t *gbuf // must be decref
    ),
    void *user_data
)
{
    api_tls_t *api_tls = ((__ytls_t__ *)ytls)->api_tls;
    return api_tls->new_secure_filter(
        ytls,
        on_handshake_done_cb,
        on_clear_data_cb,
        on_encrypted_data_cb,
        user_data
    );
}

/***************************************************************************
    Shutdown
 ***************************************************************************/
PUBLIC void ytls_shutdown(hytls ytls, hsskt sskt)
{
    api_tls_t *api_tls = ((__ytls_t__ *)ytls)->api_tls;
    api_tls->shutdown(sskt);
}

/***************************************************************************
    Free secure socket
 ***************************************************************************/
PUBLIC void ytls_free_secure_filter(hytls ytls, hsskt sskt)
{
    api_tls_t *api_tls = ((__ytls_t__ *)ytls)->api_tls;
    api_tls->free_secure_filter(sskt);
}

/***************************************************************************
    Do handshake
 ***************************************************************************/
PUBLIC int ytls_do_handshake(hytls ytls, hsskt sskt)
{
    api_tls_t *api_tls = ((__ytls_t__ *)ytls)->api_tls;
    return api_tls->do_handshake(sskt);
}

/***************************************************************************
    Use this function to encrypt clear data.
    The encrypted data will be returned in on_encrypted_data_cb callback.
 ***************************************************************************/
PUBLIC int ytls_encrypt_data(
    hytls ytls,
    hsskt sskt,
    gbuffer_t *gbuf // owned
)
{
    api_tls_t *api_tls = ((__ytls_t__ *)ytls)->api_tls;
    return api_tls->encrypt_data(sskt, gbuf);
}

/***************************************************************************
    Use this function decrypt encrypted data.
    The clear data will be returned in on_clear_data_cb callback.
 ***************************************************************************/
PUBLIC int ytls_decrypt_data(
    hytls ytls,
    hsskt sskt,
    gbuffer_t *gbuf // owned
)
{
    api_tls_t *api_tls = ((__ytls_t__ *)ytls)->api_tls;
    return api_tls->decrypt_data(sskt, gbuf);
}

/***************************************************************************
    Get error
 ***************************************************************************/
PUBLIC const char *ytls_get_last_error(hytls ytls, hsskt sskt)
{
    api_tls_t *api_tls = ((__ytls_t__ *)ytls)->api_tls;
    return api_tls->get_last_error(sskt);
}

/***************************************************************************
    Set trace
 ***************************************************************************/
PUBLIC void ytls_set_trace(hytls ytls, hsskt sskt, BOOL set)
{
    api_tls_t *api_tls = ((__ytls_t__ *)ytls)->api_tls;
    api_tls->set_trace(sskt, set);
}

/***************************************************************************
    Set peer/sock names (for self-contained TLS logging)
 ***************************************************************************/
PUBLIC void ytls_set_peer_name(hytls ytls, hsskt sskt, const char *peername, const char *sockname)
{
    api_tls_t *api_tls = ((__ytls_t__ *)ytls)->api_tls;
    api_tls->set_peer_name(sskt, peername, sockname);
}

/***************************************************************************
    Flush data
 ***************************************************************************/
PUBLIC int ytls_flush(hytls ytls, hsskt sskt)
{
    api_tls_t *api_tls = ((__ytls_t__ *)ytls)->api_tls;
    return api_tls->flush(sskt);
}
