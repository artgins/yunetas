/****************************************************************************
 *          sha1.c
 *
 *          SHA-1 as FIPS 180-4 (sections 5.1.1, 5.3.1, 6.1) describes it:
 *          the message is padded with a 1 bit, zeros and its length in bits
 *          (64 bits, big endian) to a multiple of 512 bits, and each block
 *          of 512 bits goes through 80 rounds that update five 32-bit
 *          words. The digest is those five words, big endian.
 *
 *          Written for Yuneta: up to 7.28.4 this file was the sha1.c of
 *          Paul E. Jones, with no license that allowed it here.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include "sha1.h"

/***************************************************************
 *              Constants
 ***************************************************************/
#define SHA1_BLOCK_SIZE 64

/***************************************************************
 *              Prototypes
 ***************************************************************/
static void sha1_process_block(uint32_t h[5], const uint8_t block[SHA1_BLOCK_SIZE]);

/***************************************************************************
 *  Left rotation of a 32-bit word
 ***************************************************************************/
static inline uint32_t rotl32(uint32_t x, unsigned n)
{
    return (x << n) | (x >> (32 - n));
}

/***************************************************************************
 *  One block of 512 bits into the five words of the hash
 ***************************************************************************/
static void sha1_process_block(uint32_t h[5], const uint8_t block[SHA1_BLOCK_SIZE])
{
    uint32_t w[80];

    for(int t = 0; t < 16; t++) {
        w[t] = ((uint32_t)block[t*4] << 24) |
               ((uint32_t)block[t*4 + 1] << 16) |
               ((uint32_t)block[t*4 + 2] << 8) |
               ((uint32_t)block[t*4 + 3]);
    }
    for(int t = 16; t < 80; t++) {
        w[t] = rotl32(w[t-3] ^ w[t-8] ^ w[t-14] ^ w[t-16], 1);
    }

    uint32_t a = h[0];
    uint32_t b = h[1];
    uint32_t c = h[2];
    uint32_t d = h[3];
    uint32_t e = h[4];

    for(int t = 0; t < 80; t++) {
        uint32_t f;
        uint32_t k;
        if(t < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999;
        } else if(t < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if(t < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        uint32_t temp = rotl32(a, 5) + f + e + k + w[t];
        e = d;
        d = c;
        c = rotl32(b, 30);
        b = a;
        a = temp;
    }

    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
}

/***************************************************************************
 *  The 20 bytes of the SHA-1 of `len` bytes of `data`
 ***************************************************************************/
void sha1_digest(const void *data, size_t len, uint8_t digest[SHA1_DIGEST_SIZE])
{
    uint32_t h[5] = {
        0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0
    };
    const uint8_t *p = data;
    size_t left = len;

    while(left >= SHA1_BLOCK_SIZE) {
        sha1_process_block(h, p);
        p += SHA1_BLOCK_SIZE;
        left -= SHA1_BLOCK_SIZE;
    }

    /*
     *  The tail, the 1 bit, the zeros and the length: one block, or two
     *  when the tail leaves no room for the 8 bytes of the length
     */
    uint8_t tail[SHA1_BLOCK_SIZE * 2];
    memset(tail, 0, sizeof(tail));
    if(left > 0) {
        memcpy(tail, p, left);
    }
    tail[left] = 0x80;
    size_t tail_len = (left < SHA1_BLOCK_SIZE - 8)? SHA1_BLOCK_SIZE : SHA1_BLOCK_SIZE * 2;

    uint64_t bits = (uint64_t)len * 8;
    for(int i = 0; i < 8; i++) {
        tail[tail_len - 1 - i] = (uint8_t)(bits >> (i * 8));
    }

    for(size_t off = 0; off < tail_len; off += SHA1_BLOCK_SIZE) {
        sha1_process_block(h, tail + off);
    }

    for(int i = 0; i < 5; i++) {
        digest[i*4]     = (uint8_t)(h[i] >> 24);
        digest[i*4 + 1] = (uint8_t)(h[i] >> 16);
        digest[i*4 + 2] = (uint8_t)(h[i] >> 8);
        digest[i*4 + 3] = (uint8_t)(h[i]);
    }
}
