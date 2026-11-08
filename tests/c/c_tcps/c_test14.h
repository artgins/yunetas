/****************************************************************************
 *          C_TEST14.H
 *
 *          A class to test C_TCP / C_TCP_S
 *          Test: a TLS server whose certificate cannot be loaded does
 *          not listen, the yuno goes on, and it listens when the
 *          certificate is there
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifndef TEST14_TLS_LIBRARY
    #define TEST14_TLS_LIBRARY  TLS_LIBRARY_NAME
#endif
#ifndef TEST14_PORT_TLS
    #define TEST14_PORT_TLS     "7881"
#endif
#ifndef TEST14_PORT_TLS2
    #define TEST14_PORT_TLS2    "7883"
#endif
#ifndef TEST14_PORT_PLAIN
    #define TEST14_PORT_PLAIN   "7882"
#endif
#ifndef TEST14_CERT_ERROR   // what the backend says of a certificate file that is not there
    #ifdef CONFIG_HAVE_OPENSSL  // TLS_LIBRARY_NAME prefers it
        #define TEST14_CERT_ERROR   "SSL_CTX_use_certificate_chain_file() FAILED"
    #else
        #define TEST14_CERT_ERROR   "mbedtls_x509_crt_parse_file() FAILED"
    #endif
#endif
#ifndef TEST14_APP_NAME
    #define TEST14_APP_NAME     "test_tcps_test14"  // must be the executable's name
#endif

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_TEST14);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test14(void);

extern int test14_reached_end;   // the test ran to its end (the yuno did not exit)
extern int test14_failed;        // checks that failed

#ifdef __cplusplus
}
#endif
