/****************************************************************************
 *          gen_frame64_fixture.c
 *
 *  Fixture generator for the emu_device end-to-end runtime check.
 *
 *  emu_device replays the base64 "frame64" field of a timeranger2 topic to a
 *  TCP sink. The review left it compile-verified only, "needs a timeranger2
 *  topic whose records carry a base64 frame64 field" — this builds exactly
 *  that: a master timeranger2 database with one topic of N records, each
 *  { "id":1, "tm":<t>, "frame64": base64("FRAME-<j>\n") }, appended as N
 *  instances of a single key so emu_device replays them in order.
 *
 *  Build (standalone, against the installed yuneta libs):
 *    gcc -I$Y/outputs/include gen_frame64_fixture.c -L$Y/outputs/lib \
 *      -lyunetas-core-linux -largp-standalone -ltimeranger2 -lyev_loop -lytls \
 *      -lyunetas-gobj -ljansson -luring -lpcre2-8 -ljwt-y -lssl -lcrypto \
 *      -lpthread -ldl -lbacktrace -o gen_frame64_fixture
 *
 *  Usage: gen_frame64_fixture <path> <database> <topic> <n_frames>
 *
 *          Copyright (c) 2026, ArtGins.
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gobj.h>
#include <timeranger2.h>
#include <helpers.h>

#define BASE_T 1700000000ULL  /* fixed epoch so the fixture is deterministic */

/* Minimal standard base64 encoder (no newlines) — keeps the fixture
 * self-contained and the expected frame64 values trivially reproducible. */
static char *b64(const unsigned char *in, size_t n)
{
    static const char *T =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char *out = gbmem_malloc(((n + 2) / 3) * 4 + 1);
    size_t o = 0;
    for(size_t i = 0; i < n; i += 3) {
        unsigned v = in[i] << 16;
        int rem = (int)(n - i);
        if(rem > 1) {
            v |= in[i+1] << 8;
        }
        if(rem > 2) {
            v |= in[i+2];
        }
        out[o++] = T[(v >> 18) & 0x3F];
        out[o++] = T[(v >> 12) & 0x3F];
        out[o++] = (rem > 1) ? T[(v >> 6) & 0x3F] : '=';
        out[o++] = (rem > 2) ? T[v & 0x3F] : '=';
    }
    out[o] = 0;
    return out;
}

int main(int argc, char *argv[])
{
    if(argc < 5) {
        fprintf(stderr, "usage: %s <path> <database> <topic> <n_frames>\n", argv[0]);
        return 2;
    }
    const char *path = argv[1];
    const char *database = argv[2];
    const char *topic = argv[3];
    int n = atoi(argv[4]);

    sys_malloc_fn_t mf; sys_realloc_fn_t rf; sys_calloc_fn_t cf; sys_free_fn_t ff;
    gbmem_get_allocators(&mf, &rf, &cf, &ff);
    json_set_alloc_funcs(mf, ff);
    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:s}",
        "path", path,
        "database", database,
        "master", 1,
        "filename_mask", "%Y"
    );
    json_t *tranger = tranger2_startup(0, jn_tranger, 0);
    if(!tranger) {
        fprintf(stderr, "tranger2_startup FAILED for %s/%s\n", path, database);
        return 1;
    }

    tranger2_create_topic(
        tranger,
        topic,
        "id",       // pkey
        "tm",       // tkey
        json_pack("{s:i, s:s}", "on_critical_error", 4, "filename_mask", "%Y"),
        sf_int_key,
        0,          // jn_cols
        0           // jn_var
    );

    for(int j = 0; j < n; j++) {
        char payload[32];
        int plen = snprintf(payload, sizeof(payload), "FRAME-%d\n", j);
        char *frame64 = b64((const unsigned char *)payload, (size_t)plen);

        json_t *jn_record = json_pack("{s:i, s:I, s:s}",
            "id", 1,                                /* one key -> ordered replay */
            "tm", (json_int_t)(BASE_T + (unsigned)j),
            "frame64", frame64
        );
        gbmem_free(frame64);

        md2_record_ex_t md;
        if(tranger2_append_record(tranger, topic, BASE_T + (unsigned)j, 0, &md, jn_record) < 0) {
            fprintf(stderr, "tranger2_append_record FAILED at j=%d\n", j);
            return 1;
        }
    }

    tranger2_stop(tranger);
    tranger2_shutdown(tranger);

    printf("OK: wrote %d frame64 records to %s/%s topic '%s'\n", n, path, database, topic);
    return 0;
}
