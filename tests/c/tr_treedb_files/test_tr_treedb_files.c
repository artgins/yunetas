/****************************************************************************
 *          test_tr_treedb_files.c
 *
 *          Regression coverage for the 'file' column and __assets__:
 *          the bytes of a node live on disk under the treedb, the index
 *          in memory, and the column holds an fkey into __assets__.
 *
 *          Each case nails a claim of DESIGN-treedb-files.md that, if it
 *          broke, would break QUIETLY:
 *            1. the bytes are not in the record
 *            2. the id is the hash of the BYTES (a lying client is refused)
 *            3. the ceiling is applied before decoding
 *            4. the type is read from the bytes
 *            5. the derived hooks persist nothing
 *            6. the gc, three ways (unlinked / live link / snapshot link)
 *            7. one kw, two files (a gbuffer and a manifest of slices)
 *            8. no leak
 *            9. one asset, ONE blob, and a snapshot refuses the delete
 *           10. the hooks follow the schema at RUN TIME, both ways
 *           11. the write path links a file column itself, autolink or not
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>

#include <gobj.h>
#include <timeranger2.h>
#include <tr_treedb.h>
#include <yev_loop.h>
#include <testing.h>
#include <helpers.h>

#include "schema_sample.c"

#define APP "test_tr_treedb_files"

/***************************************************************
 *              Constants
 ***************************************************************/
#define DATABASE    "tr_files"
#define TREEDB_NAME "treedb_files"

/*
 *  Fixtures: the first bytes are what the sniffer reads, the rest is
 *  filler that makes each one a different asset.
 */
#define PNG_A   "\x89PNG\r\n\x1a\n" "IHDR fixture A, not a real image but it starts like one"
#define PNG_B   "\x89PNG\r\n\x1a\n" "IHDR fixture B, a different photograph"
#define PNG_C   "\x89PNG\r\n\x1a\n" "IHDR fixture C, held only by a snapshot"
#define JPG_A   "\xff\xd8\xff\xe0" "JFIF fixture, a qr code"
#define PDF_A   "%PDF-1.4\n%fixture plan\n"
#define PDF_B   "%PDF-1.4\n%fixture plan of a run-time topic\n"
#define SVG_A   "<svg xmlns='http://www.w3.org/2000/svg'><script>alert(1)</script></svg>"

/*  One CONTAINER, two legal names for it: isobmff is 'video/mp4' by its
 *  bytes and 'audio/mp4' just as compatibly.  */
#define MP4_A   "\x00\x00\x00\x18" "ftypisom" "fixture, one container two names"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE char path_database[PATH_MAX];

/***************************************************************************
 *  base64 of a fixture, as a new json string
 ***************************************************************************/
PRIVATE json_t *b64(const char *data, size_t len)
{
    gbuffer_t *gbuf = gbuffer_binary_to_base64(data, len);
    json_t *jn = json_string(gbuffer_cur_rd_pointer(gbuf));
    GBUFFER_DECREF(gbuf)
    return jn;
}

/***************************************************************************
 *  sha256 of a fixture, static buffer
 ***************************************************************************/
PRIVATE const char *sha(const char *data, size_t len)
{
    static char bf[SHA256_HEX_LEN + 1];
    sha256_hex(data, len, bf, sizeof(bf));
    return bf;
}

/***************************************************************************
 *  The id part of a 'file' column value (full fkey reference)
 ***************************************************************************/
PRIVATE const char *fkey_id(json_t *node, const char *col)
{
    static char id[NAME_MAX];
    char topic[NAME_MAX];
    char hook[NAME_MAX];
    id[0] = 0;
    const char *ref = kw_get_str(0, node, col, "", 0);
    decode_parent_ref(ref, topic, sizeof(topic), id, sizeof(id), hook, sizeof(hook));
    return id;
}

/***************************************************************************
 *  Is the blob of an asset on disk?
 ***************************************************************************/
PRIVATE BOOL blob_exists(json_t *tranger, const char *id, const char *content_type)
{
    char path[PATH_MAX];
    if(treedb_blob_path(tranger, id, content_type, path, sizeof(path))<0) {
        return FALSE;
    }
    return is_regular_file(path);
}

/***************************************************************************
 *  Create a device whose 'foto' arrives as content64, and link it.
 *  This is the json door: the browser's shape.
 ***************************************************************************/
PRIVATE json_t *create_device_with_foto(
    json_t *tranger,
    const char *id,
    const char *data,
    size_t len,
    const char *content_type,
    const char *claimed_id
)
{
    json_t *kw = json_pack("{s:s, s:s, s:s, s:s, s:{s:{s:o, s:s, s:s}}}",
        "id", id,
        "name", id,
        "foto", claimed_id? claimed_id: "",
        "qr", "",
        "__files__",
            "foto",
                "content64", b64(data, len),
                "original_name", "foto.png",
                "content_type", content_type
    );
    /*  No autolink: the write path links a `file` column ITSELF  */
    return treedb_create_node(tranger, TREEDB_NAME, "devices", kw);
}

/***************************************************************************
 *  1. The bytes are not in the record
 ***************************************************************************/
PRIVATE int test_bytes_not_in_record(json_t *tranger)
{
    int result = 0;
    const char *test = "1. the bytes are not in the record";
    set_expected_results(test, NULL, NULL, NULL, 1);

    json_t *node = create_device_with_foto(tranger, "dev-1", PNG_A, sizeof(PNG_A)-1, "image/png", 0);
    if(!node) {
        printf("%s  FAIL: create with a file refused%s\n", On_Red BWhite, Color_Off);
        return -1;
    }
    const char *id = sha(PNG_A, sizeof(PNG_A)-1);

    if(json_object_get(node, "__files__") || json_object_get(node, "content64")) {
        printf("%s  FAIL: the record carries the transport keys%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(strcmp(fkey_id(node, "foto"), id)!=0) {
        printf("%s  FAIL: foto is not an fkey to the asset: %s%s\n",
            On_Red BWhite, kw_get_str(0, node, "foto", "", 0), Color_Off);
        result += -1;
    }
    json_t *asset = treedb_get_node(tranger, TREEDB_NAME, TREEDB_ASSETS_TOPIC, id);
    if(!asset) {
        printf("%s  FAIL: no index node in __assets__%s\n", On_Red BWhite, Color_Off);
        result += -1;
    } else {
        if(kw_get_int(0, asset, "size", 0, 0) != (json_int_t)(sizeof(PNG_A)-1)) {
            printf("%s  FAIL: asset size wrong%s\n", On_Red BWhite, Color_Off);
            result += -1;
        }
        if(strcmp(kw_get_str(0, asset, "content_type", "", 0), "image/png")!=0) {
            printf("%s  FAIL: asset content_type wrong%s\n", On_Red BWhite, Color_Off);
            result += -1;
        }
        json_t *hook = json_object_get(asset, "as_devices_foto");
        if(!json_is_object(hook) || !json_object_get(hook, "dev-1")) {
            printf("%s  FAIL: the derived hook does not hold the device%s\n", On_Red BWhite, Color_Off);
            result += -1;
        }
    }
    if(!blob_exists(tranger, id, "image/png")) {
        printf("%s  FAIL: the blob is not on disk%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    /*
     *  A bare id and no bytes links an asset that exists
     */
    json_t *kw = json_pack("{s:s, s:s, s:s}", "id", "dev-1b", "foto", id, "qr", "");
    json_t *node2 = treedb_create_node(tranger, TREEDB_NAME, "devices", kw);
    if(!node2 || strcmp(fkey_id(node2, "foto"), id)!=0) {
        printf("%s  FAIL: a bare id of an existing asset did not link%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  2. The id is the hash of the BYTES
 ***************************************************************************/
PRIVATE int test_id_is_the_hash(json_t *tranger)
{
    int result = 0;
    const char *test = "2. the id is the hash of the bytes";
    set_expected_results(
        test,
        json_pack("[{s:s},{s:s}]",
            "msg", "File REFUSED: the id does not match the bytes",
            "msg", "File REFUSED: asset not found and no bytes sent"
        ),
        NULL, NULL, 1
    );

    /*  good bytes, wrong id  */
    json_t *node = create_device_with_foto(
        tranger, "dev-liar", PNG_B, sizeof(PNG_B)-1, "image/png",
        sha(PNG_A, sizeof(PNG_A)-1)
    );
    if(node) {
        printf("%s  FAIL: a wrong id with good bytes was accepted%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(treedb_get_node(tranger, TREEDB_NAME, "devices", "dev-liar")) {
        printf("%s  FAIL: the lying record was created%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(blob_exists(tranger, sha(PNG_B, sizeof(PNG_B)-1), "image/png")) {
        printf("%s  FAIL: the refused bytes were written%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  a bare id nobody stored, and no bytes  */
    json_t *kw = json_pack("{s:s, s:s}", "id", "dev-ghost", "foto", sha(PNG_B, sizeof(PNG_B)-1));
    node = treedb_create_node(tranger, TREEDB_NAME, "devices", kw);
    if(node) {
        printf("%s  FAIL: a bare id of a missing asset was accepted%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  3. The ceiling is applied before decoding
 ***************************************************************************/
PRIVATE int test_ceiling_before_decode(json_t *tranger)
{
    int result = 0;
    const char *test = "3. the ceiling is applied before decoding";
    set_expected_results(
        test,
        json_pack("[{s:s},{s:s}]",
            "msg", "File REFUSED: over max_size",
            "msg", "File REFUSED: over max_size"
        ),
        NULL, NULL, 1
    );

    /*  the treedb ceiling  */
    treedb_set_files_limits(tranger, TREEDB_NAME, 40, NULL);
    json_t *node = create_device_with_foto(tranger, "dev-big", PNG_B, sizeof(PNG_B)-1, "image/png", 0);
    if(node) {
        printf("%s  FAIL: a file over the treedb ceiling was accepted%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    treedb_set_files_limits(tranger, TREEDB_NAME, 128*1024*1024, NULL);

    /*  the column's own limit (places.plano: 4096) under a generous ceiling  */
    char big_pdf[6000];
    memset(big_pdf, 'x', sizeof(big_pdf));
    memcpy(big_pdf, "%PDF-1.4\n", 9);
    json_t *kw = json_pack("{s:s, s:{s:{s:o, s:s}}}",
        "id", "place-big",
        "__files__", "plano",
            "content64", b64(big_pdf, sizeof(big_pdf)),
            "original_name", "big.pdf"
    );
    node = treedb_create_node(tranger, TREEDB_NAME, "places", kw);
    if(node) {
        printf("%s  FAIL: a file over the column limit was accepted%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  4. The type is read from the bytes
 ***************************************************************************/
PRIVATE int test_type_from_bytes(json_t *tranger)
{
    int result = 0;
    const char *test = "4. the type is read from the bytes";
    set_expected_results(
        test,
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "File REFUSED: content_type does not match the bytes",
            "msg", "File REFUSED: content_type does not match the bytes",
            "msg", "File REFUSED: content_type not allowed"
        ),
        NULL, NULL, 1
    );

    /*  a png called jpeg  */
    json_t *node = create_device_with_foto(tranger, "dev-lie-type", PNG_B, sizeof(PNG_B)-1, "image/jpeg", 0);
    if(node) {
        printf("%s  FAIL: a png called image/jpeg was accepted%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    /*  an svg called png: the case the allowlist exists for  */
    node = create_device_with_foto(tranger, "dev-svg", SVG_A, sizeof(SVG_A)-1, "image/png", 0);
    if(node) {
        printf("%s  FAIL: an svg called image/png was accepted%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    /*  a real png where the column takes only pdf  */
    json_t *kw = json_pack("{s:s, s:{s:{s:o, s:s}}}",
        "id", "place-png",
        "__files__", "plano",
            "content64", b64(PNG_B, sizeof(PNG_B)-1),
            "original_name", "plan.png"
    );
    node = treedb_create_node(tranger, TREEDB_NAME, "places", kw);
    if(node) {
        printf("%s  FAIL: a png was accepted by a pdf-only column%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    /*  and a real pdf, declared by nothing but its bytes, is taken  */
    kw = json_pack("{s:s, s:{s:{s:o}}}",
        "id", "place-1",
        "__files__", "plano",
            "content64", b64(PDF_A, sizeof(PDF_A)-1)
    );
    node = treedb_create_node(tranger, TREEDB_NAME, "places", kw);
    if(!node || strcmp(fkey_id(node, "plano"), sha(PDF_A, sizeof(PDF_A)-1))!=0) {
        printf("%s  FAIL: a pdf typed by its bytes alone was refused%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    json_t *asset = treedb_get_node(tranger, TREEDB_NAME, TREEDB_ASSETS_TOPIC, sha(PDF_A, sizeof(PDF_A)-1));
    if(!asset || strcmp(kw_get_str(0, asset, "content_type", "", 0), "application/pdf")!=0) {
        printf("%s  FAIL: the sniffed type was not stored%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  5. The derived hooks persist nothing
 ***************************************************************************/
PRIVATE int check_derived_hooks(json_t *tranger, const char *when)
{
    int result = 0;
    static const char *hooks[] = {"as_devices_foto", "as_devices_qr", "as_places_plano", 0};

    json_t *cols = tranger2_dict_topic_desc_cols(tranger, TREEDB_ASSETS_TOPIC);
    for(int i = 0; hooks[i]; i++) {
        json_t *col = json_object_get(cols, hooks[i]);
        if(!col || !kw_has_word(0, json_object_get(col, "flag"), "hook", 0)) {
            printf("%s  FAIL: %s: derived hook %s missing in memory%s\n",
                On_Red BWhite, when, hooks[i], Color_Off);
            result += -1;
        }
    }
    JSON_DECREF(cols)

    char path[PATH_MAX];
    build_path(path, sizeof(path), path_database, TREEDB_ASSETS_TOPIC, "topic_cols.json", NULL);
    json_t *stored = load_json_from_file(0, path, "", 0);
    if(!stored) {
        printf("%s  FAIL: %s: cannot read %s%s\n", On_Red BWhite, when, path, Color_Off);
        result += -1;
    } else {
        for(int i = 0; hooks[i]; i++) {
            if(json_object_get(stored, hooks[i])) {
                printf("%s  FAIL: %s: derived hook %s was PERSISTED%s\n",
                    On_Red BWhite, when, hooks[i], Color_Off);
                result += -1;
            }
        }
        JSON_DECREF(stored)
    }
    return result;
}

PRIVATE int test_derived_hooks_persist_nothing(json_t *tranger)
{
    int result = 0;
    const char *test = "5. the derived hooks persist nothing";
    set_expected_results(test, NULL, NULL, NULL, 1);

    result += check_derived_hooks(tranger, "before reopen");

    treedb_close_db(tranger, TREEDB_NAME);
    helper_quote2doublequote(schema_sample);
    json_t *jn_schema = legalstring2json(schema_sample, TRUE);
    if(!treedb_open_db(tranger, TREEDB_NAME, jn_schema, 0)) {
        printf("%s  FAIL: cannot reopen%s\n", On_Red BWhite, Color_Off);
        return -1;
    }
    result += check_derived_hooks(tranger, "after reopen");

    /*  and the links came back from the fkeys  */
    json_t *dev = treedb_get_node(tranger, TREEDB_NAME, "devices", "dev-1");
    json_t *asset = treedb_get_node(tranger, TREEDB_NAME, TREEDB_ASSETS_TOPIC, sha(PNG_A, sizeof(PNG_A)-1));
    if(!dev || !asset) {
        printf("%s  FAIL: nodes lost on reopen%s\n", On_Red BWhite, Color_Off);
        result += -1;
    } else {
        json_t *hook = json_object_get(asset, "as_devices_foto");
        if(!json_is_object(hook) || !json_object_get(hook, "dev-1")) {
            printf("%s  FAIL: the link was not rebuilt on reopen%s\n", On_Red BWhite, Color_Off);
            result += -1;
        }
    }

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  9. One asset, ONE blob: the first arrival names the file
 *
 *  A container that more than one mime type can claim used to be written
 *  twice, once per extension, and the row named only one of them: the
 *  other could never be served, never be seen by the gc and never be
 *  removed by the delete.
 ***************************************************************************/
PRIVATE int test_one_asset_one_blob(json_t *tranger)
{
    int result = 0;
    const char *test = "9. one asset, one blob";
    set_expected_results(
        test,
        json_pack("[{s:s}]",
            "msg", "asset already stored under another content_type, keeping the stored one"
        ),
        NULL, NULL, 1
    );

    size_t len = sizeof(MP4_A)-1;
    const char *id = sha(MP4_A, len);

    json_t *first = create_device_with_foto(tranger, "dev-mp4a", MP4_A, len, "video/mp4", 0);
    if(!first) {
        printf("%s  FAIL: the mp4 was refused%s\n", On_Red BWhite, Color_Off);
        return -1;
    }
    /*  the same bytes, the other name of the same container  */
    json_t *second = create_device_with_foto(tranger, "dev-mp4b", MP4_A, len, "audio/mp4", 0);
    if(!second) {
        printf("%s  FAIL: the second arrival was refused%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    if(!blob_exists(tranger, id, "video/mp4")) {
        printf("%s  FAIL: the blob of the first arrival is gone%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(blob_exists(tranger, id, "audio/mp4")) {
        printf("%s  FAIL: the second arrival wrote a SECOND blob for one asset%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    json_t *asset = treedb_get_node(tranger, TREEDB_NAME, TREEDB_ASSETS_TOPIC, id);
    if(!asset || strcmp(kw_get_str(0, asset, "content_type", "", 0), "video/mp4")!=0) {
        printf("%s  FAIL: the stored content_type changed under the served url%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    /*  and both devices name the one asset  */
    if(strcmp(fkey_id(first, "foto"), id)!=0 ||
            (second && strcmp(fkey_id(second, "foto"), id)!=0)) {
        printf("%s  FAIL: the two devices do not share the asset%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  10. The hooks follow the schema at RUN TIME
 *
 *  `create-topic` and `delete-topic` are live commands. Derived only at
 *  open, a topic added while the yuno runs had a `file` column whose hook
 *  did not exist, and a topic deleted at runtime left its hook behind
 *  holding children that are gone -- which the gc reads as "linked".
 ***************************************************************************/
PRIVATE int test_hooks_follow_the_schema(json_t *tranger)
{
    int result = 0;
    const char *test = "10. the hooks follow the schema at run time";
    set_expected_results(
        test,
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "Creating topic",
            /*  `works.plan` tampoco lleva `writable`: el aviso salta al
             *  crear el hook, que es una vez por topic y no por pasada.  */
            "msg", "a 'file' column without 'writable' cannot be filled by a person",
            "msg", "Deleting topic"
        ),
        NULL, NULL, 1
    );

    /*  A topic with a 'file' column, created with the yuno running  */
    json_t *cols = json_pack("{s:{s:s, s:s, s:i, s:s, s:[s,s]}, s:{s:s, s:s, s:i, s:s, s:[s,s]}}",
        "id",
            "id", "id", "header", "Id", "fillspace", 20, "type", "string",
            "flag", "persistent", "required",
        "plan",
            "id", "plan", "header", "Plan", "fillspace", 20, "type", "string",
            "flag", "fkey", "file"
    );
    if(!treedb_create_topic(
        tranger, TREEDB_NAME, "works", 1, "", 0, cols, 0, FALSE, FALSE
    )) {
        printf("%s  FAIL: cannot create the topic at run time%s\n", On_Red BWhite, Color_Off);
        return -1;
    }

    json_t *assets_cols = tranger2_dict_topic_desc_cols(tranger, TREEDB_ASSETS_TOPIC);
    BOOL derived = json_object_get(assets_cols, "as_works_plan")? TRUE: FALSE;
    JSON_DECREF(assets_cols)
    if(!derived) {
        printf("%s  FAIL: the hook of a topic created at run time was not derived%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  and it LINKS: the whole point of the hook existing  */
    json_t *kw = json_pack("{s:s, s:{s:{s:o, s:s}}}",
        "id", "work-1",
        "__files__", "plan",
            "content64", b64(PDF_B, sizeof(PDF_B)-1),
            "original_name", "work-1.pdf"
    );
    json_t *node = treedb_create_node(tranger, TREEDB_NAME, "works", kw);
    const char *pdf_id = sha(PDF_B, sizeof(PDF_B)-1);
    json_t *asset = treedb_get_node(tranger, TREEDB_NAME, TREEDB_ASSETS_TOPIC, pdf_id);
    json_t *hook = asset? json_object_get(asset, "as_works_plan"): 0;
    if(!node || !json_is_object(hook) || !json_object_get(hook, "work-1")) {
        printf("%s  FAIL: a file column of a run-time topic did not link%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  Delete the topic: the hook goes, and with it the children it held  */
    if(treedb_delete_topic(tranger, TREEDB_NAME, "works")<0) {
        printf("%s  FAIL: cannot delete the topic%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    assets_cols = tranger2_dict_topic_desc_cols(tranger, TREEDB_ASSETS_TOPIC);
    BOOL left_behind = json_object_get(assets_cols, "as_works_plan")? TRUE: FALSE;
    JSON_DECREF(assets_cols)
    if(left_behind) {
        printf("%s  FAIL: the hook of a deleted topic was left behind%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    asset = treedb_get_node(tranger, TREEDB_NAME, TREEDB_ASSETS_TOPIC, pdf_id);
    if(asset && json_object_get(asset, "as_works_plan")) {
        printf("%s  FAIL: the asset still answers a hook nothing declares%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    /*  and now the gc can see it: nothing links it any more  */
    json_t *would = treedb_gc_files(tranger, TREEDB_NAME, TRUE);
    if(!json_str_in_list(0, would, pdf_id, 0)) {
        printf("%s  FAIL: the gc cannot see the asset of the deleted topic%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    JSON_DECREF(would)

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  11. The write path links a `file` column ITSELF, autolink or not
 *
 *  treedb_update_node() skips every fkey in its field loop, which is right
 *  for a link a person edits by linking. Left there, an update WITHOUT
 *  autolink stored the bytes and the index node, answered success, and
 *  left the column as it was: an orphan asset and a device with no photo.
 *  And treedb_create_node() linked nothing at all, so a plain create-node
 *  did the same. Both link now; "" unlinks; a column the kw does not carry
 *  is left alone.
 ***************************************************************************/
PRIVATE int test_write_path_links(json_t *tranger)
{
    int result = 0;
    const char *test = "11. the write path links a file column itself";
    set_expected_results(test, NULL, NULL, NULL, 1);

    /*  sha() answers in ONE static buffer: copy what is kept  */
    char id_a[SHA256_HEX_LEN + 1];
    char id_b[SHA256_HEX_LEN + 1];
    char id_qr[SHA256_HEX_LEN + 1];
    snprintf(id_a, sizeof(id_a), "%s", sha(PNG_A, sizeof(PNG_A)-1));
    snprintf(id_b, sizeof(id_b), "%s", sha(PNG_B, sizeof(PNG_B)-1));
    snprintf(id_qr, sizeof(id_qr), "%s", sha(JPG_A, sizeof(JPG_A)-1));

    /*  a plain create, foto AND qr, no autolink anywhere  */
    json_t *kw = json_pack("{s:s, s:s, s:s, s:{s:{s:o, s:s}}}",
        "id", "dev-11",
        "foto", id_a,
        "qr", "",
        "__files__", "qr",
            "content64", b64(JPG_A, sizeof(JPG_A)-1),
            "original_name", "dev-11.jpg"
    );
    json_t *node = treedb_create_node(tranger, TREEDB_NAME, "devices", kw);
    if(!node) {
        printf("%s  FAIL: plain create refused%s\n", On_Red BWhite, Color_Off);
        return -1;
    }
    json_t *asset_a = treedb_get_node(tranger, TREEDB_NAME, TREEDB_ASSETS_TOPIC, id_a);
    json_t *hook = asset_a? json_object_get(asset_a, "as_devices_foto"): 0;
    if(!json_is_object(hook) || !json_object_get(hook, "dev-11")) {
        printf("%s  FAIL: create did not link foto (the asset's hook does not hold dev-11)%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    json_t *asset_qr = treedb_get_node(tranger, TREEDB_NAME, TREEDB_ASSETS_TOPIC, id_qr);
    hook = asset_qr? json_object_get(asset_qr, "as_devices_qr"): 0;
    if(!json_is_object(hook) || !json_object_get(hook, "dev-11")) {
        printf("%s  FAIL: create did not link qr%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  a plain update moves foto to B: A lets go, B takes it, qr untouched  */
    kw = json_pack("{s:s, s:s, s:{s:{s:o, s:s, s:s}}}",
        "id", "dev-11",
        "foto", "",
        "__files__", "foto",
            "content64", b64(PNG_B, sizeof(PNG_B)-1),
            "original_name", "dev-11.png",
            "content_type", "image/png"
    );
    if(!treedb_update_node(tranger, node, kw, TRUE)) {
        printf("%s  FAIL: plain update with a file refused%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(strcmp(fkey_id(node, "foto"), id_b)!=0) {
        printf("%s  FAIL: update did not move foto to B: %s%s\n",
            On_Red BWhite, kw_get_str(0, node, "foto", "", 0), Color_Off);
        result += -1;
    }
    hook = json_object_get(asset_a, "as_devices_foto");
    if(json_is_object(hook) && json_object_get(hook, "dev-11")) {
        printf("%s  FAIL: A still holds dev-11 after the move%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    json_t *asset_b = treedb_get_node(tranger, TREEDB_NAME, TREEDB_ASSETS_TOPIC, id_b);
    hook = asset_b? json_object_get(asset_b, "as_devices_foto"): 0;
    if(!json_is_object(hook) || !json_object_get(hook, "dev-11")) {
        printf("%s  FAIL: B does not hold dev-11 after the move%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    hook = json_object_get(asset_qr, "as_devices_qr");
    if(!json_is_object(hook) || !json_object_get(hook, "dev-11")) {
        printf("%s  FAIL: a column the kw did not carry was touched (qr unlinked)%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  "" unlinks, and the record says so  */
    kw = json_pack("{s:s, s:s}", "id", "dev-11", "foto", "");
    if(!treedb_update_node(tranger, node, kw, TRUE)) {
        printf("%s  FAIL: clearing update refused%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(!empty_string(kw_get_str(0, node, "foto", "", 0))) {
        printf("%s  FAIL: foto not cleared: %s%s\n",
            On_Red BWhite, kw_get_str(0, node, "foto", "", 0), Color_Off);
        result += -1;
    }
    hook = json_object_get(asset_b, "as_devices_foto");
    if(json_is_object(hook) && json_object_get(hook, "dev-11")) {
        printf("%s  FAIL: B still holds dev-11 after the clear%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  and it is on DISK, not only in memory: reopen and ask again  */
    treedb_close_db(tranger, TREEDB_NAME);
    helper_quote2doublequote(schema_sample);
    json_t *jn_schema = legalstring2json(schema_sample, TRUE);
    if(!treedb_open_db(tranger, TREEDB_NAME, jn_schema, 0)) {
        printf("%s  FAIL: cannot reopen%s\n", On_Red BWhite, Color_Off);
        return -1;
    }
    node = treedb_get_node(tranger, TREEDB_NAME, "devices", "dev-11");
    asset_qr = treedb_get_node(tranger, TREEDB_NAME, TREEDB_ASSETS_TOPIC, id_qr);
    hook = asset_qr? json_object_get(asset_qr, "as_devices_qr"): 0;
    if(!node || !empty_string(kw_get_str(0, node, "foto", "", 0)) ||
            !json_is_object(hook) || !json_object_get(hook, "dev-11")) {
        printf("%s  FAIL: the links of the plain writes did not survive a reopen%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  7. One kw, two files: a gbuffer and a manifest of two slices
 ***************************************************************************/
PRIVATE int test_one_kw_two_files(json_t *tranger)
{
    int result = 0;
    const char *test = "7. one kw, two files";
    set_expected_results(test, NULL, NULL, NULL, 1);

    size_t len_png = sizeof(PNG_B)-1;
    size_t len_jpg = sizeof(JPG_A)-1;
    gbuffer_t *gbuf = gbuffer_create(len_png + len_jpg, len_png + len_jpg);
    gbuffer_append(gbuf, PNG_B, len_png);
    gbuffer_append(gbuf, JPG_A, len_jpg);

    json_t *kw = json_pack("{s:s, s:I, s:{s:{s:I, s:I, s:s}, s:{s:I, s:I, s:s}}}",
        "id", "dev-2",
        "gbuffer", (json_int_t)(uintptr_t)gbuf,
        "__files__",
            "foto",
                "offset", (json_int_t)0,
                "size", (json_int_t)len_png,
                "original_name", "dev-2.png",
            "qr",
                "offset", (json_int_t)len_png,
                "size", (json_int_t)len_jpg,
                "original_name", "dev-2.jpg"
    );
    json_t *node = treedb_create_node(tranger, TREEDB_NAME, "devices", kw);
    if(!node) {
        printf("%s  FAIL: create with two slices refused%s\n", On_Red BWhite, Color_Off);
        return -1;
    }
    if(strcmp(fkey_id(node, "foto"), sha(PNG_B, len_png))!=0) {
        printf("%s  FAIL: foto slice not stored%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(strcmp(fkey_id(node, "qr"), sha(JPG_A, len_jpg))!=0) {
        printf("%s  FAIL: qr slice not stored%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(!blob_exists(tranger, sha(PNG_B, len_png), "image/png") ||
            !blob_exists(tranger, sha(JPG_A, len_jpg), "image/jpeg")) {
        printf("%s  FAIL: a slice is not on disk%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    json_t *qr = treedb_get_node(tranger, TREEDB_NAME, TREEDB_ASSETS_TOPIC, sha(JPG_A, len_jpg));
    if(!qr || strcmp(kw_get_str(0, qr, "content_type", "", 0), "image/jpeg")!=0) {
        printf("%s  FAIL: the jpeg slice was not typed from its bytes%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  6. The gc, three ways
 ***************************************************************************/
PRIVATE int test_gc_three_ways(json_t *tranger)
{
    int result = 0;
    const char *test = "6. the gc, three ways";
    set_expected_results(
        test,
        json_pack("[{s:s},{s:s}]",
            "msg", "cannot delete asset, a snapshot still links it",
            "msg", "cannot delete asset, a snapshot still links it"
        ),
        NULL, NULL, 1
    );

    /*
     *  C: linked by dev-3, then a snapshot, then dev-3 moves to A.
     *  After that nothing LIVE links C, and only the snapshot remembers it.
     */
    json_t *node = create_device_with_foto(tranger, "dev-3", PNG_C, sizeof(PNG_C)-1, "image/png", 0);
    if(!node) {
        printf("%s  FAIL: cannot create dev-3%s\n", On_Red BWhite, Color_Off);
        return -1;
    }
    if(treedb_shoot_snap(tranger, TREEDB_NAME, "snap_c", "dev-3 still holds C")<0) {
        printf("%s  FAIL: cannot shoot snap%s\n", On_Red BWhite, Color_Off);
        return -1;
    }
    json_t *kw = json_pack("{s:s, s:s}", "id", "dev-3", "foto", sha(PNG_A, sizeof(PNG_A)-1));
    if(!treedb_update_node(tranger, node, kw, TRUE)) {
        printf("%s  FAIL: plain update of dev-3 refused%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(strcmp(fkey_id(node, "foto"), sha(PNG_A, sizeof(PNG_A)-1))!=0) {
        printf("%s  FAIL: dev-3 did not move to A%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    /*
     *  An orphan nothing ever linked: imported bytes
     */
    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), path_database, "incoming", "taller", NULL);
    mkrdir(dir, 02770);
    char path[PATH_MAX];
    build_path(path, sizeof(path), dir, "orphan.pdf", NULL);
    const char *orphan = "%PDF-1.4\n%an orphan\n";
    FILE *f = fopen(path, "w");
    fwrite(orphan, 1, strlen(orphan), f);
    fclose(f);
    char import_root[PATH_MAX];
    build_path(import_root, sizeof(import_root), path_database, "incoming", NULL);
    json_t *imported = treedb_import_files(tranger, TREEDB_NAME, import_root, "taller", FALSE, "tester");
    if(!imported || kw_get_int(0, imported, "imported", 0, 0) != 1) {
        printf("%s  FAIL: import did not bring the orphan%s\n", On_Red BWhite, Color_Off);
        result += -1;
    } else {
        const char *mapped = kw_get_str(0, imported, "files`taller/orphan.pdf", "", 0);
        if(strcmp(mapped, sha(orphan, strlen(orphan)))!=0) {
            printf("%s  FAIL: import did not answer path -> id: '%s'%s\n", On_Red BWhite, mapped, Color_Off);
            result += -1;
        }
    }
    JSON_DECREF(imported)

    /*
     *  Dry run: the orphan yes, A (live) no, C (snapshot) no
     */
    json_t *would = treedb_gc_files(tranger, TREEDB_NAME, TRUE);
    if(!json_str_in_list(0, would, sha(orphan, strlen(orphan)), 0)) {
        printf("%s  FAIL: gc does not see the orphan%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(json_str_in_list(0, would, sha(PNG_A, sizeof(PNG_A)-1), 0)) {
        printf("%s  FAIL: gc would take an asset a live node links%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(json_str_in_list(0, would, sha(PNG_C, sizeof(PNG_C)-1), 0)) {
        printf("%s  FAIL: gc would take an asset only a SNAPSHOT links%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(json_array_size(would) != 1) {
        printf("%s  FAIL: gc dry run: expected 1 orphan, got %d%s\n",
            On_Red BWhite, (int)json_array_size(would), Color_Off);
        result += -1;
    }
    JSON_DECREF(would)
    if(!treedb_get_node(tranger, TREEDB_NAME, TREEDB_ASSETS_TOPIC, sha(orphan, strlen(orphan)))) {
        printf("%s  FAIL: the dry run deleted%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    /*
     *  For real: row and bytes of the orphan gone, the others intact
     */
    json_t *taken = treedb_gc_files(tranger, TREEDB_NAME, FALSE);
    if(json_array_size(taken) != 1) {
        printf("%s  FAIL: gc took %d%s\n", On_Red BWhite, (int)json_array_size(taken), Color_Off);
        result += -1;
    }
    JSON_DECREF(taken)
    if(treedb_get_node(tranger, TREEDB_NAME, TREEDB_ASSETS_TOPIC, sha(orphan, strlen(orphan)))) {
        printf("%s  FAIL: the orphan row survived the gc%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(blob_exists(tranger, sha(orphan, strlen(orphan)), "application/pdf")) {
        printf("%s  FAIL: the orphan blob survived the gc%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(!blob_exists(tranger, sha(PNG_C, sizeof(PNG_C)-1), "image/png") ||
            !treedb_get_node(tranger, TREEDB_NAME, TREEDB_ASSETS_TOPIC, sha(PNG_C, sizeof(PNG_C)-1))) {
        printf("%s  FAIL: the snapshot-held asset was taken%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    /*
     *  And a DELETE by hand is refused for the same reason the gc left it,
     *  with force too: the tag guard is inert for an asset (shoot_snap
     *  skips the __ topics), so the snapshot walk is the guard in its
     *  place, and force means "unlink the children", never "ignore what a
     *  snapshot needs".
     */
    json_t *held_asset = treedb_get_node(
        tranger, TREEDB_NAME, TREEDB_ASSETS_TOPIC, sha(PNG_C, sizeof(PNG_C)-1)
    );
    if(treedb_delete_node(tranger, held_asset, 0)==0) {
        printf("%s  FAIL: delete-node took an asset a SNAPSHOT still links%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(treedb_delete_node(tranger, held_asset, json_pack("{s:b}", "force", 1))==0) {
        printf("%s  FAIL: force took an asset a SNAPSHOT still links%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(!blob_exists(tranger, sha(PNG_C, sizeof(PNG_C)-1), "image/png")) {
        printf("%s  FAIL: the refused delete took the bytes anyway%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;

    char path_root[PATH_MAX];
    const char *home = getenv("HOME");
    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    json_t *tranger = 0;
    {
        const char *test = "create tranger";
        set_expected_results(
            test,
            json_pack("[{s:s}]", "msg", "Creating __timeranger2__.json"),
            NULL, NULL, 1
        );
        json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", LOG_OPT_TRACE_STACK
        );
        tranger = tranger2_startup(0, jn_tranger, 0);
        result += test_json(NULL);
    }

    {
        const char *test = "open treedb";
        /*  __snaps__ + __graphs__ + devices + places + __assets__ = 5 topics */
        /*  `places.plano` se queda SIN `writable` a proposito: una columna
         *  `file` que nadie puede llenar se dibuja igual que una llena y
         *  no se puede usar, asi que el open lo avisa. Las dos de
         *  `devices` si lo llevan, que es el caso normal.  */
        set_expected_results(
            test,
            json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s},{s:s}]",
                "msg", "Creating topic",
                "msg", "Creating topic",
                "msg", "Creating topic",
                "msg", "Creating topic",
                "msg", "Creating topic",
                "msg", "a 'file' column without 'writable' cannot be filled by a person"
            ),
            NULL, NULL, 1
        );
        helper_quote2doublequote(schema_sample);
        json_t *jn_schema = legalstring2json(schema_sample, TRUE);
        if(!jn_schema) {
            printf("Can't decode schema_sample json\n");
            exit(-1);
        }
        if(!treedb_open_db(tranger, TREEDB_NAME, jn_schema, 0)) {
            result += -1;
        }
        result += test_json(NULL);
    }

    result += test_bytes_not_in_record(tranger);
    result += test_id_is_the_hash(tranger);
    result += test_ceiling_before_decode(tranger);
    result += test_type_from_bytes(tranger);
    result += test_derived_hooks_persist_nothing(tranger);
    result += test_one_kw_two_files(tranger);
    result += test_write_path_links(tranger);
    result += test_one_asset_one_blob(tranger);
    result += test_gc_three_ways(tranger);
    /*  after the gc: it leaves an orphan of its own, and case 6 counts  */
    result += test_hooks_follow_the_schema(tranger);

    {
        const char *test = "close and shutdown";
        set_expected_results(test, NULL, NULL, NULL, 1);
        treedb_close_db(tranger, TREEDB_NAME);
        tranger2_shutdown(tranger);
        result += test_json(NULL);
    }

    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gbmem_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(
        argc,
        argv,
        NULL, NULL, NULL, NULL, NULL, NULL
    );

    yuno_catch_signals();

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    gobj_log_register_handler(
        "testing",
        0,
        capture_log_write,
        0
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    /*--------------------------------*
     *      Event loop
     *--------------------------------*/
    yev_loop_create(
        0,
        2024,
        10,
        NULL,
        &yev_loop
    );

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    int result = do_test();

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result < 0) {
        printf("%sTEST FAILED%s\n", On_Red BWhite, Color_Off);
    } else {
        printf("%sTEST PASSED%s\n", On_Green BWhite, Color_Off);
    }
    return result < 0 ? -1 : 0;
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int xtimes_once = 0;
    xtimes_once++;
    yev_loop_reset_running(yev_loop);
    if(xtimes_once > 1) {
        exit(-1);
    }
}

PRIVATE void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;

    signal(SIGPIPE, SIG_IGN);

    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);
    sigaction(SIGQUIT, &sigIntHandler, NULL);
}
