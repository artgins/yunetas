/****************************************************************************
 *          test_sha256.c
 *
 *          sha256 of gobj-c: sha256_digest() / sha256_hex() and the
 *          incremental sha256_init() / sha256_update() / sha256_final() /
 *          sha256_final_hex().
 *          The vectors of FIPS 180-4 (and of its examples: "abc", the two
 *          block message, a million 'a'), the empty input, the lengths
 *          around the padding (55, 56, 63, 64, 65 bytes), and the same
 *          bytes given in parts of every size: the digest of the parts is
 *          the one of the whole.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>

#define APP "test_sha256"

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
 *  The vectors of FIPS 180-4
 ***************************************************************************/
PRIVATE void test_vectors(void)
{
    struct {
        const char *data;
        const char *sha256;
    } vectors[] = {
        {"",
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"abc",
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
        {"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
            "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1"},
    };

    for(size_t i = 0; i < ARRAY_SIZE(vectors); i++) {
        char name[256];
        char hex[SHA256_HEX_LEN + 1];
        size_t len = strlen(vectors[i].data);

        sha256_hex(vectors[i].data, len, hex, sizeof(hex));
        snprintf(name, sizeof(name), "sha256_hex() of %zu bytes is the FIPS vector", len);
        ok_or_fail(strcmp(hex, vectors[i].sha256) == 0, name);

        sha256_ctx_t ctx;
        sha256_init(&ctx);
        for(size_t j = 0; j < len; j++) {
            sha256_update(&ctx, vectors[i].data + j, 1);
        }
        sha256_final_hex(&ctx, hex, sizeof(hex));
        snprintf(name, sizeof(name), "sha256_update() a byte at a time, %zu bytes, is the FIPS vector", len);
        ok_or_fail(strcmp(hex, vectors[i].sha256) == 0, name);
    }

    /*
     *  A million 'a', in parts of 1000: the third example of FIPS 180-2
     */
    char part[1000];
    memset(part, 'a', sizeof(part));
    sha256_ctx_t ctx;
    char hex[SHA256_HEX_LEN + 1];
    sha256_init(&ctx);
    for(int i = 0; i < 1000; i++) {
        sha256_update(&ctx, part, sizeof(part));
    }
    sha256_final_hex(&ctx, hex, sizeof(hex));
    ok_or_fail(strcmp(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0,
        "a million 'a' in parts of 1000 is the FIPS vector");

    sha256_init(&ctx);
    sha256_update(&ctx, NULL, 0);
    sha256_final_hex(&ctx, hex, sizeof(hex));
    ok_or_fail(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0,
        "an update of 0 bytes (NULL) is the empty input");
}

/***************************************************************************
 *  Every length from 0 to 300 (the padding at 55, 56, 63, 64, 65 and their
 *  multiples), cut in two at every place and in parts of 1 to 70 bytes:
 *  the same digest as sha256_digest() of the whole
 ***************************************************************************/
PRIVATE void test_parts(void)
{
    uint8_t data[300];
    for(size_t i = 0; i < sizeof(data); i++) {
        data[i] = (uint8_t)(i * 7 + 3);
    }

    int bad_cuts = 0;
    int bad_parts = 0;
    for(size_t len = 0; len <= sizeof(data); len++) {
        uint8_t whole[SHA256_DIGEST_LEN];
        sha256_digest(data, len, whole);

        for(size_t cut = 0; cut <= len; cut++) {
            sha256_ctx_t ctx;
            uint8_t digest[SHA256_DIGEST_LEN];
            sha256_init(&ctx);
            sha256_update(&ctx, data, cut);
            sha256_update(&ctx, data + cut, len - cut);
            sha256_final(&ctx, digest);
            if(memcmp(digest, whole, sizeof(whole)) != 0) {
                bad_cuts++;
            }
        }
        for(size_t step = 1; step <= 70; step++) {
            sha256_ctx_t ctx;
            uint8_t digest[SHA256_DIGEST_LEN];
            sha256_init(&ctx);
            for(size_t off = 0; off < len; off += step) {
                sha256_update(&ctx, data + off, MIN(step, len - off));
            }
            sha256_final(&ctx, digest);
            if(memcmp(digest, whole, sizeof(whole)) != 0) {
                bad_parts++;
            }
        }
    }
    ok_or_fail(bad_cuts == 0, "0 to 300 bytes cut in two at every place: the digest of the whole");
    ok_or_fail(bad_parts == 0, "0 to 300 bytes in parts of 1 to 70 bytes: the digest of the whole");

    /*
     *  A context init'ed again hashes something else from the start
     */
    sha256_ctx_t ctx;
    uint8_t digest[SHA256_DIGEST_LEN];
    char hex[SHA256_HEX_LEN + 1];
    sha256_init(&ctx);
    sha256_update(&ctx, "xyz", 3);
    sha256_final(&ctx, digest);
    sha256_init(&ctx);
    sha256_update(&ctx, "abc", 3);
    sha256_final_hex(&ctx, hex, sizeof(hex));
    ok_or_fail(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0,
        "a context init'ed again starts from nothing");
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_bad_arguments(void)
{
    char small[SHA256_HEX_LEN];
    char hex[SHA256_HEX_LEN + 1];
    sha256_ctx_t ctx;
    printf("     (two ERRORs follow, on purpose)\n");
    ok_or_fail(sha256_hex("abc", 3, small, sizeof(small)) == -1, "sha256_hex() into 64 bytes answers -1");
    sha256_init(&ctx);
    sha256_update(&ctx, "abc", 3);
    ok_or_fail(sha256_final_hex(&ctx, small, sizeof(small)) == -1, "sha256_final_hex() into 64 bytes answers -1");
    sha256_final_hex(&ctx, hex, sizeof(hex));
    ok_or_fail(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0,
        "a sha256_final_hex() refused leaves the context to a final that works");
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

    test_vectors();
    test_parts();
    test_bad_arguments();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}
