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

/***************************************************************
 *              Structures
 ***************************************************************/
typedef void * hytls;
typedef void * hsskt;

/*
 *  Fields of jn_config
 *      - "library"                     str, default "openssl"
 *
 *          Fields for library "openssl"
 *              - "trace"                       bool
 *              - "ssl_certificate"             str
 *              - "ssl_certificate_key"         str
 *              - "ssl_ciphers"                 str, default "HIGH:!aNULL:!kRSA:!PSK:!SRP:!MD5:!RC4"
 *              - "rx_buffer_size"              int, default 32*1024
 *              - "ssl_trusted_certificate"     str
 *              - "ssl_verify_depth"            int, default 1
 */

typedef struct api_tls_s {
    const char *name;
    hytls (*init)(
        hgobj gobj,
        json_t *jn_config,  // not owned
        BOOL server
    );
    void (*cleanup)(hytls ytls);
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

    "library"   library to use, defaul: "openssl"
    "trace"     True to verbose trace.

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
