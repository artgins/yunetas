/***********************************************************************
 *          YTLS.C
 *
 *          TLS for Yuneta
 *
 *          Copyright (c) 2018 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
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

    const char *tls_library = kw_get_str(gobj, jn_config, "library", "openssl", 0);
    SWITCHS(tls_library) {
        ICASES("openssl")
            api_tls = openssl_api_tls();
            break;
        ICASES("mbedtls")
            api_tls = mbed_api_tls();
        break;
        DEFAULTS
            break;
    } SWITCHS_END;

    if(!api_tls) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL_ERROR,
            "msg",              "%s", "tls_library NOT DEFINED",
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
    Flush data
 ***************************************************************************/
PUBLIC int ytls_flush(hytls ytls, hsskt sskt)
{
    api_tls_t *api_tls = ((__ytls_t__ *)ytls)->api_tls;
    return api_tls->flush(sskt);
}
