/****************************************************************************
 *          MBEDTLS.H
 *
 *          OpenSSL-specific code for the TLS/SSL layer
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
 *              Prototypes
 ***************************************************************/

/**rst**
   Get api_tls_t
**rst**/
PUBLIC api_tls_t *openssl_api_tls(void);

#ifdef __cplusplus
}
#endif
