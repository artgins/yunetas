/****************************************************************************
 *          YTLS.H
 *
 *          TLS for Yuneta
 *
 *          Copyright (c) 2018 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
/* Compile-time TLS backend names — single source of truth.
 *
 *  TLS_LIBRARY_NAME    : preferred backend (OpenSSL wins when both)
 *  TLS_LIBRARIES_NAME  : every backend compiled in, joined with '+'
 *
 *  These strings are pushed into gobj's global variables pool by
 *  yunetas_register_c_core() so kw configs can substitute them via
 *  (^^__tls_library__^^) / (^^__tls_libraries__^^).
 */
#include <yuneta_config.h>
#if defined(CONFIG_HAVE_OPENSSL)
    #define TLS_LIBRARY_NAME "openssl"
#elif defined(CONFIG_HAVE_MBEDTLS)
    #define TLS_LIBRARY_NAME "mbedtls"
#else
    #define TLS_LIBRARY_NAME ""
#endif

#if defined(CONFIG_HAVE_OPENSSL) && defined(CONFIG_HAVE_MBEDTLS)
    #define TLS_LIBRARIES_NAME "openssl+mbedtls"
#elif defined(CONFIG_HAVE_OPENSSL)
    #define TLS_LIBRARIES_NAME "openssl"
#elif defined(CONFIG_HAVE_MBEDTLS)
    #define TLS_LIBRARIES_NAME "mbedtls"
#else
    #define TLS_LIBRARIES_NAME ""
#endif

/***************************************************************
 *              Structures
 ***************************************************************/
typedef void * hytls;
typedef void * hsskt;

/*
 *  Fields of jn_config
 *      - "library"                     str, default TLS_LIBRARY_NAME (compile-time backend)
 *
 *          Fields for library "openssl"
 *              - "trace"                       bool
 *              - "ssl_certificate"             str
 *              - "ssl_certificate_key"         str
 *              - "ssl_ciphers"                 str, default "HIGH:!aNULL:!kRSA:!PSK:!SRP:!MD5:!RC4"
 *              - "rx_buffer_size"              int, default 32*1024
 *              - "ssl_trusted_certificate"     str
 *              - "ssl_verify_depth"            int, default 1
 *
 *          Fields for library "mbedtls"
 *              - "trace"                       bool
 *              - "ssl_certificate"             str
 *              - "ssl_certificate_key"         str
 *              - "rx_buffer_size"              int, default 32*1024
 *              - "ssl_trusted_certificate"     str
 *              - "ssl_server_name"             str  (client only: hostname for SNI + cert verification)
 */

typedef struct api_tls_s {
    const char *name;
    hytls (*init)(
        hgobj gobj,
        json_t *jn_config,  // not owned
        BOOL server
    );
    void (*cleanup)(hytls ytls);
    int (*reload_certificates)(hytls ytls, json_t *jn_config /* not owned */);
    json_t *(*get_cert_info)(hytls ytls);
    const char * (*version)(hytls ytls);
    hsskt (*new_secure_filter)(
        hytls ytls,
        int (*on_handshake_done_cb)(void *user_data, int error),
        int (*on_clear_data_cb)(
            void *user_data,
            gbuffer_t *gbuf  // must be decref
        ),
        int (*on_encrypted_data_cb)(
            void *user_data,
            gbuffer_t *gbuf // must be decref
        ),
        void *user_data
    );
    void (*free_secure_filter)(hsskt sskt);
    int (*do_handshake)(hsskt sskt); // Must return 1 (done), 0 (in progress), -1 (failure)
    int (*encrypt_data)(
        hsskt sskt,
        gbuffer_t *gbuf  // owned
    );
    int (*decrypt_data)(
        hsskt sskt,
        gbuffer_t *gbuf  // owned
    );
    const char * (*get_last_error)(hsskt sskt);
    void (*set_trace)(hsskt sskt, BOOL set);
    int (*flush)(hsskt sskt); // flush clear and encrypted data
    void (*shutdown)(hsskt sskt);
} api_tls_t;

typedef struct { // Common to all ytls_t types
    api_tls_t *api_tls;     // HACK must be the first item in the ytls_t structures
} __ytls_t__;

/***************************************************************
 *              Prototypes
 ***************************************************************/

/**rst**
    Startup tls context

    "library"   library to use, default: TLS_LIBRARY_NAME (compile-time backend: openssl or mbedtls)
    "trace_tls" True to verbose trace.

    OPENSSL jn_config
    -----------------
        ssl_certificate         (string, required in server side)
        ssl_certificate_key     (string, required in server side)
        ssl_trusted_certificate (string, required in server side)
        ssl_verify_depth        (integer, default:1)
        ssl_ciphers             (string, default: "HIGH:!aNULL:!kRSA:!PSK:!SRP:!MD5:!RC4")
        rx_buffer_size          (integer, default: 32*1024)

**rst**/
PUBLIC hytls ytls_init(
    hgobj gobj,
    json_t *jn_config,  // not owned
    BOOL server
);

/**rst**
    Cleanup tls context
**rst**/
PUBLIC void ytls_cleanup(hytls ytls);

/**rst**
    Reload certificates without disrupting live connections.

    Builds a fresh TLS context from jn_config (same keys as ytls_init) and
    atomically swaps it into ytls. Existing secure filters (live connections)
    keep the old context alive via refcount until they close; new filters
    created after the call use the new context with the fresh certificates.

    Returns 0 on success, -1 on failure (the previous context is kept intact).
**rst**/
PUBLIC int ytls_reload_certificates(
    hytls ytls,
    json_t *jn_config   // not owned
);

/**rst**
    Return metadata about the currently loaded server certificate.

    Fields in the returned JSON (missing for client ytls / no cert loaded):
      - "subject"    (string)  X.509 subject DN, one-line form
      - "issuer"     (string)  X.509 issuer DN, one-line form
      - "not_before" (integer) Unix ts of certificate's notBefore
      - "not_after"  (integer) Unix ts of certificate's notAfter
      - "serial"     (string)  Serial number, uppercase hex

    Returns a new json object owned by the caller, or NULL if no cert info
    is available (e.g. client-side ytls, or backend that can't introspect).
**rst**/
PUBLIC json_t *ytls_get_cert_info(hytls ytls);

/**rst**
    Version tls
**rst**/
PUBLIC const char * ytls_version(hytls ytls);

/**rst**
    Get new secure filter
**rst**/
PUBLIC hsskt ytls_new_secure_filter(
    hytls ytls,
    int (*on_handshake_done_cb)(void *user_data, int error),
    int (*on_clear_data_cb)(
        void *user_data,
        gbuffer_t *gbuf  // must be decref
    ),
    int (*on_encrypted_data_cb)(
        void *user_data,
        gbuffer_t *gbuf  // must be decref
    ),
    void *user_data
);

/**rst**
    Shutdown secure connection
**rst**/
PUBLIC void ytls_shutdown(hytls ytls, hsskt sskt);

/**rst**
    Free secure filter
**rst**/
PUBLIC void ytls_free_secure_filter(hytls ytls, hsskt sskt);

/**rst**
    Do handshake
    Return
        1   (handshake done),
        0   (handshake in progress),
        -1  (handshake failure).
    Callback on_handshake_done_cb will be called once for successfully case, or more for failure case.
**rst**/
PUBLIC int ytls_do_handshake(hytls ytls, hsskt sskt);

/**rst**
    Use this function to encrypt clear data.
    The encrypted data will be returned in on_encrypted_data_cb callback.
**rst**/
PUBLIC int ytls_encrypt_data(
    hytls ytls,
    hsskt sskt,
    gbuffer_t *gbuf // owned
);

/**rst**
    Use this function decrypt encrypted data.
    The clear data will be returned in on_clear_data_cb callback.
**rst**/
PUBLIC int ytls_decrypt_data(
    hytls ytls,
    hsskt sskt,
    gbuffer_t *gbuf // owned
);

/**rst**
    Get last error
**rst**/
PUBLIC const char *ytls_get_last_error(hytls ytls, hsskt sskt);

/**rst**
    Set trace
**rst**/
PUBLIC void ytls_set_trace(hytls ytls, hsskt sskt, BOOL set);

/**rst**
    Flush data
**rst**/
PUBLIC int ytls_flush(hytls ytls, hsskt sskt);


#ifdef __cplusplus
}
#endif
