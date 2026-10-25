/****************************************************************************
 *          test_ws_accept.c
 *
 *          The Sec-WebSocket-Accept of the handshake of C_WEBSOCKET: the
 *          SHA-1 of sha1.c (root-linux, private) and its base64 by
 *          gbuffer_binary_to_base64(). A browser refuses a connection whose
 *          accept is wrong, and the C client of C_WEBSOCKET does not check
 *          it, so no other test of the tree would see a broken one.
 *
 *          SHA-1 against the vectors of FIPS 180 and messages of every
 *          length around the padding (55/56 bytes need one more block, 64
 *          fill one); the accept against the example of RFC 6455 (1.3).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>
#include "sha1.h"

#define APP "test_ws_accept"

PRIVATE int global_result = 0;

PRIVATE void ok_or_fail(int cond, const char *name)
{
    if(cond) {
        printf("ok   %s\n", name);
    } else {
        printf("FAIL %s\n", name);
        global_result += -1;
    }
}

/***************************************************************************
 *  The SHA-1 of `len` bytes of `data` as 40 lowercase hex digits
 ***************************************************************************/
PRIVATE void sha1_hex(const void *data, size_t len, char hex[2*SHA1_DIGEST_SIZE + 1])
{
    uint8_t digest[SHA1_DIGEST_SIZE];
    sha1_digest(data, len, digest);
    for(int i = 0; i < SHA1_DIGEST_SIZE; i++) {
        snprintf(hex + i*2, 3, "%02x", digest[i]);
    }
}

/***************************************************************************
 *  FIPS 180 and the lengths around the padding
 ***************************************************************************/
PRIVATE void test_sha1(void)
{
    static const struct {
        const char *text;
        const char *sha1;
    } texts[] = {
        {"",    "da39a3ee5e6b4b0d3255bfef95601890afd80709"},
        {"abc", "a9993e364706816aba3e25717850c26c9cd0d89d"},
        {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                "84983e441c3bd26ebaae4aa1f95129e5e54670f1"},
    };
    for(size_t i = 0; i < ARRAY_SIZE(texts); i++) {
        char hex[2*SHA1_DIGEST_SIZE + 1];
        sha1_hex(texts[i].text, strlen(texts[i].text), hex);
        char name[160];
        snprintf(name, sizeof(name), "sha1(\"%.20s\")", texts[i].text);
        ok_or_fail(strcmp(hex, texts[i].sha1) == 0, name);
    }

    /*
     *  "a" repeated n times (hashlib.sha1(b'a'*n))
     */
    static const struct {
        size_t n;
        const char *sha1;
    } runs[] = {
        {1,       "86f7e437faa5a7fce15d1ddcb9eaeaea377667b8"},
        {55,      "c1c8bbdc22796e28c0e15163d20899b65621d65a"},
        {56,      "c2db330f6083854c99d4b5bfb6e8f29f201be699"},
        {57,      "f08f24908d682555111be7ff6f004e78283d989a"},
        {63,      "03f09f5b158a7a8cdad920bddc29b81c18a551f5"},
        {64,      "0098ba824b5c16427bd7a1122a5a442a25ec644d"},
        {65,      "11655326c708d70319be2610e8a57d9a5b959d3b"},
        {119,     "ee971065aaa017e0632a8ca6c77bb3bf8b1dfc56"},
        {120,     "f34c1488385346a55709ba056ddd08280dd4c6d6"},
        {128,     "ad5b3fdbcb526778c2839d2f151ea753995e26a0"},
        {1000,    "291e9a6c66994949b57ba5e650361e98fc36b1ba"},
        {1000000, "34aa973cd4c4daa4f61eeb2bdbad27316534016f"},
    };
    char *a = gbmem_malloc(1000000);
    if(!a) {
        ok_or_fail(0, "gbmem_malloc() of the runs of 'a'");
        return;
    }
    memset(a, 'a', 1000000);
    for(size_t i = 0; i < ARRAY_SIZE(runs); i++) {
        char hex[2*SHA1_DIGEST_SIZE + 1];
        sha1_hex(a, runs[i].n, hex);
        char name[80];
        snprintf(name, sizeof(name), "sha1('a' x %zu)", runs[i].n);
        ok_or_fail(strcmp(hex, runs[i].sha1) == 0, name);
    }
    gbmem_free(a);
}

/***************************************************************************
 *  RFC 6455 (1.3): the key "dGhlIHNhbXBsZSBub25jZQ==" is answered with
 *  "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="
 ***************************************************************************/
PRIVATE void test_accept(void)
{
    const char *concat = "dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    uint8_t digest[SHA1_DIGEST_SIZE];
    sha1_digest(concat, strlen(concat), digest);

    gbuffer_t *gbuf = gbuffer_binary_to_base64((const char *)digest, sizeof(digest));
    if(!gbuf) {
        ok_or_fail(0, "gbuffer_binary_to_base64() of the digest");
        return;
    }
    size_t len = gbuffer_leftbytes(gbuf);
    const char *b64 = gbuffer_cur_rd_pointer(gbuf);
    const char *expected = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
    ok_or_fail(
        len == strlen(expected) && memcmp(b64, expected, len) == 0,
        "Sec-WebSocket-Accept of the example of RFC 6455"
    );
    gbuffer_decref(gbuf);
}

/***************************************************************************
 *
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

    test_sha1();
    test_accept();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}
