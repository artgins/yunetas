/****************************************************************************
 *          MBEDTLS.H
 *
 *          Mbed-TLS specific code for the TLS/SSL layer
 *
 *          Copyright (c) 2025, ArtGins.
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
PUBLIC api_tls_t *mbed_api_tls(void);

#ifdef __cplusplus
}
#endif
