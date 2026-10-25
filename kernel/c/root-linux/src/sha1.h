/****************************************************************************
 *          sha1.h
 *
 *          SHA-1 (FIPS 180-4) of a buffer in memory, for the handshake of
 *          a websocket (RFC 6455): the only use of it in Yuneta. SHA-1 is
 *          broken for signatures; never use it to protect anything.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHA1_DIGEST_SIZE 20

/*
 *  The 20 bytes of the SHA-1 of `len` bytes of `data`
 */
void sha1_digest(const void *data, size_t len, uint8_t digest[SHA1_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif
