/***********************************************************************
 *          C_TEST_ASSETS.C
 *
 *          GClass to test C_ASSETS
 *
 *          What it checks:
 *          - the asset id IS the sha256 of the content, and storing the
 *            same bytes twice is one asset, not two;
 *          - `get-asset` answers INLINE with no web server configured and
 *            with a SIGNED URL once there is one -- the fallback is the
 *            whole reason a caller has a single code path;
 *          - the signed url reproduces what nginx's secure_link_md5
 *            hashes, computed here independently;
 *          - an asset is an orphan until a node links it, and linked is
 *            read from the SCHEMA's hooks, not guessed from the shape of
 *            a value;
 *          - a linked asset cannot be deleted, an unlinked one can, and
 *            its bytes go with the row;
 *          - `import-assets` turns a directory already on the node into N
 *            assets, skips what it must not serve, and a second run
 *            creates nothing;
 *          - `gc-assets` removes exactly the orphans.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "c_test_assets.h"

#if defined(CONFIG_HAVE_OPENSSL)
    #include <openssl/evp.h>
#endif

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TREEDB_NAME     "treedb_assets_test"
#define DATABASE        "c_assets"

/*
 *  The content of the asset the test stores, and its sha256 -- computed
 *  outside this program (sha256sum), so the id is checked against an
 *  independent answer and not against the code that produces it.
 */
#define ASSET_CONTENT   "yuneta-assets-test\n"
#define ASSET_B64       "eXVuZXRhLWFzc2V0cy10ZXN0Cg=="
#define ASSET_SHA256    "d094cd29da2b205e47a356887a38b966b18c3ed607851dc66e42fa7e36d6ffaa"

#define PUBLIC_URL      "/assets/"
#define SIGN_SECRET     "test-shared-secret"
#define URL_TTL         900

/*
 *  The principal the test yuno's authz checker refuses.
 */
#define DENIED_USER     "denied@test"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE int run_tests(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "level of help"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-------json_fn---------description--*/
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,    cmd_help,       "Command's help"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type--------name----------------flag----------------default-----description---------- */
SDATA (DTP_POINTER, "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER, "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER, "subscriber",       0,                  0,          "subscriber of output-events"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
SDATA_END()
};

/*---------------------------------------------*
 *      Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj gobj_node;
    hgobj gobj_assets;
    hgobj timer;
    json_t *tranger;
    char path_root[PATH_MAX];
    char path_store[PATH_MAX];
    char path_import[PATH_MAX];
} PRIVATE_DATA;

/***************************************************************************
 *  Schema: the canonical `assets` topic of c_assets.h, plus one consumer
 *  topic whose `foto` column is an fkey into it.
 ***************************************************************************/
PRIVATE char schema_assets_test[] = "\
{                                                                   \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'topic_name': 'assets',                                 \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 32,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'content_type': {                                   \n\
                    'header': 'Type',                               \n\
                    'fillspace': 12,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'size': {                                           \n\
                    'header': 'Size',                               \n\
                    'fillspace': 8,                                 \n\
                    'type': 'integer',                              \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                't': {                                              \n\
                    'header': 'Time',                               \n\
                    'fillspace': 20,                                \n\
                    'type': 'integer',                              \n\
                    'flag': ['persistent','time','now']             \n\
                },                                                  \n\
                'original_name': {                                  \n\
                    'header': 'Name',                               \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'source_path': {                                    \n\
                    'header': 'Source',                             \n\
                    'fillspace': 30,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'uploaded_by': {                                    \n\
                    'header': 'By',                                 \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'as_foto': {                                        \n\
                    'header': 'Photo of',                           \n\
                    'fillspace': 20,                                \n\
                    'type': 'dict',                                 \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'devices': 'foto'                           \n\
                    }                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'topic_name': 'devices',                                \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'name': {                                           \n\
                    'header': 'Name',                               \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'foto': {                                           \n\
                    'header': 'Photo',                              \n\
                    'fillspace': 32,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey']                                \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Prepare paths
     */
    const char *home = getenv("HOME");
    build_path(priv->path_root, sizeof(priv->path_root), home, "tests_yuneta", NULL);
    mkrdir(priv->path_root, 02770);

    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database), priv->path_root, DATABASE, NULL);
    rmrdir(path_database);

    build_path(priv->path_store, sizeof(priv->path_store), path_database, "blobs", NULL);
    build_path(priv->path_import, sizeof(priv->path_import), path_database, "incoming", NULL);

    /*
     *  Start tranger
     */
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", priv->path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    );
    priv->tranger = tranger2_startup(0, jn_tranger, 0);

    /*
     *  Create C_NODE child
     */
    helper_quote2doublequote(schema_assets_test);
    json_t *jn_schema = legalstring2json(schema_assets_test, TRUE);

    json_t *kw_node = json_pack("{s:I, s:s, s:o, s:i}",
        "tranger", (json_int_t)(uintptr_t)priv->tranger,
        "treedb_name", TREEDB_NAME,
        "treedb_schema", jn_schema,
        "exit_on_error", LOG_OPT_TRACE_STACK
    );
    priv->gobj_node = gobj_create_pure_child("test_node", C_NODE, kw_node, gobj);

    /*
     *  Create C_ASSETS child, pointing at that C_NODE.
     *
     *  No 'public_url'/'sign_secret' yet: the first checks are the INLINE
     *  fallback, which is what a node with no web server in front of it
     *  answers.
     */
    json_t *kw_assets = json_pack("{s:I, s:s, s:s, s:s, s:i}",
        "treedb", (json_int_t)(uintptr_t)priv->gobj_node,
        "topic_name", "assets",
        "store_path", priv->path_store,
        "import_root", priv->path_import,
        "url_ttl", URL_TTL
    );
    priv->gobj_assets = gobj_create_pure_child("test_assets", C_ASSETS, kw_assets, gobj);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->gobj_node);
    gobj_start(priv->gobj_assets);
    gobj_start(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->gobj_assets);
    gobj_stop(priv->gobj_node);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_play(priv->gobj_node);
    gobj_play(priv->gobj_assets);

    set_timeout(priv->timer, 100);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_pause(priv->gobj_assets);
    gobj_pause(priv->gobj_node);

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->tranger) {
        // NOTE: treedb is already closed by C_NODE's mt_stop
        tranger2_shutdown(priv->tranger);
        priv->tranger = NULL;
    }
}




                    /***************************
                     *      Helpers
                     ***************************/




/***************************************************************************
 *  One failed check
 ***************************************************************************/
PRIVATE int fail(hgobj gobj, const char *what)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "TEST FAIL",
        "what",         "%s", what,
        NULL
    );
    return -1;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *ask(hgobj gobj, const char *command, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return gobj_command(priv->gobj_assets, command, kw, gobj);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int resp_result(json_t *resp)
{
    return (int)kw_get_int(0, resp, "result", -1, 0);
}

/***************************************************************************
 *  Write one fixture file under the import root
 ***************************************************************************/
PRIVATE int write_fixture(hgobj gobj, const char *rel, const char *content)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char path[PATH_MAX];
    if(build_path(path, sizeof(path), priv->path_import, rel, NULL) == NULL) {
        return fail(gobj, "fixture path too long");
    }
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    char *p = strrchr(dir, '/');
    if(p) {
        *p = 0;
        mkrdir(dir, 02770);
    }
    FILE *f = fopen(path, "w");
    if(!f) {
        return fail(gobj, "cannot write fixture");
    }
    fwrite(content, 1, strlen(content), f);
    fclose(f);

    return 0;
}

/***************************************************************************
 *  The token nginx would compute for this url, worked out here from the
 *  directive's own template and NOT from the code under test:
 *
 *      secure_link_md5 "$secure_link_expires$uri <secret>"
 ***************************************************************************/
PRIVATE int expected_token(
    hgobj gobj,
    json_int_t expires,
    const char *uri,
    char *bf,
    int bflen
) {
    char message[PATH_MAX + 128];
    snprintf(message, sizeof(message), "%lld%s %s", (long long)expires, uri, SIGN_SECRET);

    uint8_t digest[16];
    unsigned int digest_len = 0;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if(!ctx) {
        return fail(gobj, "EVP_MD_CTX_new");
    }
    if(EVP_DigestInit_ex(ctx, EVP_md5(), NULL) != 1 ||
            EVP_DigestUpdate(ctx, message, strlen(message)) != 1 ||
            EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return fail(gobj, "md5 of the expected token");
    }
    EVP_MD_CTX_free(ctx);

    gbuffer_t *gbuf = gbuffer_binary_to_base64((const char *)digest, sizeof(digest));
    if(!gbuf) {
        return fail(gobj, "base64 of the expected token");
    }
    const char *b64 = gbuffer_cur_rd_pointer(gbuf);
    int j = 0;
    for(int i = 0; b64[i] && j < bflen - 1; i++) {
        char c = b64[i];
        if(c == '=' || c == '\n' || c == '\r') {
            continue;
        }
        if(c == '+') {
            c = '-';
        } else if(c == '/') {
            c = '_';
        }
        bf[j++] = c;
    }
    bf[j] = 0;
    GBUFFER_DECREF(gbuf)

    return 0;
}




                    /***************************
                     *      The tests
                     ***************************/




/***************************************************************************
 *  Run all tests -- called from the timer action, inside the event loop
 ***************************************************************************/
PRIVATE int run_tests(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int result = 0;
    json_t *resp;

    /*-----------------------------------------------*
     *  1: the id IS the content
     *-----------------------------------------------*/
    resp = ask(gobj, "put-asset",
        json_pack("{s:s, s:s, s:s}",
            "content64",     ASSET_B64,
            "content_type",  "image/png",
            "original_name", "one.png"
        )
    );
    if(resp_result(resp) != 0) {
        result += fail(gobj, "put-asset failed");
    } else {
        const char *id = kw_get_str(gobj, kw_get_dict(gobj, resp, "data", 0, 0), "id", "", 0);
        if(strcmp(id, ASSET_SHA256) != 0) {
            result += fail(gobj, "asset id is not the sha256 of its content");
        }
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  2: the same bytes are the same asset
     *-----------------------------------------------*/
    resp = ask(gobj, "put-asset",
        json_pack("{s:s, s:s, s:s}",
            "content64",     ASSET_B64,
            "content_type",  "image/png",
            "original_name", "again.png"
        )
    );
    JSON_DECREF(resp)

    resp = ask(gobj, "list-assets", json_object());
    if(json_array_size(kw_get_list(gobj, resp, "data", 0, 0)) != 1) {
        result += fail(gobj, "storing the same bytes twice made two assets");
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  3: with no web server configured, INLINE
     *-----------------------------------------------*/
    resp = ask(gobj, "get-asset", json_pack("{s:s}", "id", ASSET_SHA256));
    if(resp_result(resp) != 0) {
        result += fail(gobj, "get-asset failed");
    } else {
        json_t *data = kw_get_dict(gobj, resp, "data", 0, 0);
        if(strcmp(kw_get_str(gobj, data, "mode", "", 0), "inline") != 0) {
            result += fail(gobj, "get-asset did not fall back to inline");
        }
        if(strcmp(kw_get_str(gobj, data, "content64", "", 0), ASSET_B64) != 0) {
            result += fail(gobj, "inline content64 does not round-trip");
        }
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  4: with one configured, a SIGNED URL that
     *     nginx would accept
     *-----------------------------------------------*/
    gobj_write_str_attr(priv->gobj_assets, "public_url", PUBLIC_URL);
    gobj_write_str_attr(priv->gobj_assets, "sign_secret", SIGN_SECRET);

    time_t before = time(NULL);
    resp = ask(gobj, "get-asset", json_pack("{s:s}", "id", ASSET_SHA256));
    time_t after = time(NULL);
    if(resp_result(resp) != 0) {
        result += fail(gobj, "get-asset (url) failed");
    } else {
        json_t *data = kw_get_dict(gobj, resp, "data", 0, 0);
        if(strcmp(kw_get_str(gobj, data, "mode", "", 0), "url") != 0) {
            result += fail(gobj, "get-asset did not sign a url");
        }
        const char *url = kw_get_str(gobj, data, "url", "", 0);

        char expected_uri[PATH_MAX];
        snprintf(expected_uri, sizeof(expected_uri), "%s%c%c/%c%c/%s.png",
            PUBLIC_URL,
            ASSET_SHA256[0], ASSET_SHA256[1], ASSET_SHA256[2], ASSET_SHA256[3],
            ASSET_SHA256
        );
        if(strncmp(url, expected_uri, strlen(expected_uri)) != 0) {
            result += fail(gobj, "the signed url does not address the blob");
        } else {
            /*
             *  Pull '?e=<expires>&s=<token>' apart and check both halves.
             */
            const char *q = url + strlen(expected_uri);
            json_int_t expires = 0;
            char token[64];
            token[0] = 0;
            if(sscanf(q, "?e=%lld&s=%63s", (long long *)&expires, token) != 2) {
                result += fail(gobj, "the signed url has no e/s parameters");
            } else {
                if(expires < (json_int_t)before + URL_TTL ||
                        expires > (json_int_t)after + URL_TTL) {
                    result += fail(gobj, "the signed url does not expire in url_ttl seconds");
                }
                char want[64];
                if(expected_token(gobj, expires, expected_uri, want, sizeof(want)) < 0) {
                    result += -1;   // Error already logged
                } else if(strcmp(token, want) != 0) {
                    result += fail(gobj, "the signature is not what nginx would compute");
                }
            }
        }
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  5: nothing links it yet -> orphan
     *-----------------------------------------------*/
    resp = ask(gobj, "list-assets", json_pack("{s:b}", "orphan", 1));
    if(json_array_size(kw_get_list(gobj, resp, "data", 0, 0)) != 1) {
        result += fail(gobj, "an asset no node links is not reported as orphan");
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  6: linked -> not an orphan any more
     *-----------------------------------------------*/
    json_t *device = gobj_create_node(
        priv->gobj_node,
        "devices",
        json_pack("{s:s, s:s}", "id", "dev1", "name", "Device one"),
        0,
        gobj
    );
    if(!device) {
        result += fail(gobj, "cannot create the consumer node");
    }
    JSON_DECREF(device)

    if(gobj_link_nodes(
        priv->gobj_node,
        "as_foto",
        "assets",
        json_pack("{s:s}", "id", ASSET_SHA256),
        "devices",
        json_pack("{s:s}", "id", "dev1"),
        gobj
    ) < 0) {
        result += fail(gobj, "cannot link the asset to the consumer node");
    }

    resp = ask(gobj, "list-assets", json_pack("{s:b}", "orphan", 1));
    if(json_array_size(kw_get_list(gobj, resp, "data", 0, 0)) != 0) {
        result += fail(gobj, "a linked asset is still reported as orphan");
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  7: a linked asset cannot be deleted
     *-----------------------------------------------*/
    resp = ask(gobj, "delete-asset", json_pack("{s:s}", "id", ASSET_SHA256));
    if(resp_result(resp) == 0) {
        result += fail(gobj, "delete-asset removed an asset a node still links");
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  8: unlinked, it goes -- and its bytes with it
     *-----------------------------------------------*/
    char blob_rel[NAME_MAX];
    snprintf(blob_rel, sizeof(blob_rel), "%c%c/%c%c/%s.png",
        ASSET_SHA256[0], ASSET_SHA256[1], ASSET_SHA256[2], ASSET_SHA256[3],
        ASSET_SHA256
    );
    char blob[PATH_MAX];
    build_path(blob, sizeof(blob), priv->path_store, blob_rel, NULL);
    if(!is_regular_file(blob)) {
        result += fail(gobj, "the blob is not on disk where the url points");
    }

    if(gobj_unlink_nodes(
        priv->gobj_node,
        "as_foto",
        "assets",
        json_pack("{s:s}", "id", ASSET_SHA256),
        "devices",
        json_pack("{s:s}", "id", "dev1"),
        gobj
    ) < 0) {
        result += fail(gobj, "cannot unlink the asset");
    }

    resp = ask(gobj, "delete-asset", json_pack("{s:s}", "id", ASSET_SHA256));
    if(resp_result(resp) != 0) {
        result += fail(gobj, "cannot delete an unlinked asset");
    }
    JSON_DECREF(resp)

    if(is_regular_file(blob)) {
        result += fail(gobj, "the row is gone but the bytes stayed behind");
    }

    /*-----------------------------------------------*
     *  9: import a directory that is already here
     *-----------------------------------------------*/
    result += write_fixture(gobj, "taller/fotos/alpha.png", "import-alpha\n");
    result += write_fixture(gobj, "taller/fotos/bravo.jpg", "import-bravo\n");
    result += write_fixture(gobj, "taller/notes.txt", "not an image\n");

    resp = ask(gobj, "import-assets", json_pack("{s:s}", "source_dir", "taller"));
    if(resp_result(resp) != 0) {
        result += fail(gobj, "import-assets failed");
    } else {
        json_t *data = kw_get_dict(gobj, resp, "data", 0, 0);
        if(kw_get_int(gobj, data, "imported", 0, 0) != 2) {
            result += fail(gobj, "import-assets did not import both images");
        }
        if(kw_get_int(gobj, data, "skipped", 0, 0) != 1) {
            result += fail(gobj, "import-assets did not skip what it must not serve");
        }
    }
    JSON_DECREF(resp)

    /*
     *  The bridge a loader links by: the path the asset came from,
     *  relative to the import root.
     */
    resp = ask(gobj, "list-assets",
        json_pack("{s:{s:s}}", "filter", "source_path", "taller/fotos/alpha.png")
    );
    if(json_array_size(kw_get_list(gobj, resp, "data", 0, 0)) != 1) {
        result += fail(gobj, "an imported asset does not carry its source_path");
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  10: importing twice creates nothing
     *-----------------------------------------------*/
    resp = ask(gobj, "import-assets", json_pack("{s:s}", "source_dir", "taller"));
    JSON_DECREF(resp)

    resp = ask(gobj, "list-assets", json_object());
    if(json_array_size(kw_get_list(gobj, resp, "data", 0, 0)) != 2) {
        result += fail(gobj, "a second import duplicated the assets");
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  11: gc takes the orphans, and only those
     *-----------------------------------------------*/
    resp = ask(gobj, "gc-assets", json_pack("{s:b}", "dry_run", 1));
    if(json_array_size(kw_get_list(gobj, resp, "data", 0, 0)) != 2) {
        result += fail(gobj, "gc-assets dry_run did not find both orphans");
    }
    JSON_DECREF(resp)

    resp = ask(gobj, "list-assets", json_object());
    if(json_array_size(kw_get_list(gobj, resp, "data", 0, 0)) != 2) {
        result += fail(gobj, "gc-assets dry_run deleted something");
    }
    JSON_DECREF(resp)

    resp = ask(gobj, "gc-assets", json_object());
    JSON_DECREF(resp)

    resp = ask(gobj, "list-assets", json_object());
    if(json_array_size(kw_get_list(gobj, resp, "data", 0, 0)) != 0) {
        result += fail(gobj, "gc-assets did not delete the orphans");
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  12: the authz gate answers -403
     *
     *  The test yuno installs a checker that denies
     *  exactly one principal, so this exercises the
     *  gate instead of stepping around it.
     *-----------------------------------------------*/
    resp = ask(gobj, "put-asset",
        json_pack("{s:s, s:s, s:s}",
            "content64",    ASSET_B64,
            "content_type", "image/png",
            "__username__", DENIED_USER
        )
    );
    if(resp_result(resp) != -403) {
        result += fail(gobj, "a denied principal was allowed to store an asset");
    }
    JSON_DECREF(resp)

    resp = ask(gobj, "list-assets", json_pack("{s:s}", "__username__", DENIED_USER));
    if(resp_result(resp) != -403) {
        result += fail(gobj, "a denied principal was allowed to list assets");
    }
    JSON_DECREF(resp)

    if(result == 0) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "All c_assets tests PASSED",
            NULL
        );
    }

    return result;
}




                    /***************************
                     *      Commands
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw);
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}




                    /***************************
                     *      Local Methods
                     ***************************/




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_treedb_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    run_tests(gobj);

    gobj_log_info(gobj, 0,
        "msgset", "%s", MSGSET_INFO,
        "msg", "%s", "Exit to die",
        NULL
    );
    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_play    = mt_play,
    .mt_pause   = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_ASSETS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_TIMEOUT,                ac_timeout,             0},
        {EV_TREEDB_NODE_CREATED,    ac_treedb_event,        0},
        {EV_TREEDB_NODE_UPDATED,    ac_treedb_event,        0},
        {EV_TREEDB_NODE_DELETED,    ac_treedb_event,        0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_TREEDB_NODE_CREATED,    EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
        {EV_TREEDB_NODE_UPDATED,    EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
        {EV_TREEDB_NODE_DELETED,    EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
        {NULL, 0}
    };

    /*----------------------------------------*
     *          Register GClass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0, // local methods
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
        command_table,
        s_user_trace_level,
        0 // gcflags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_test_assets(void)
{
    return create_gclass(C_TEST_ASSETS);
}
