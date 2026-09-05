/***********************************************************************
 *          C_TEST_ASSETS.C
 *
 *          GClass to test the 'file' columns through C_NODE and the way
 *          out through C_ASSETS.
 *
 *          What it checks:
 *          - a record with a 'file' column arrives through `update-node`
 *            with its bytes BESIDE the record (`__files__`), the column
 *            leaves holding an fkey into __assets__, and the id IS the
 *            sha256 of the content -- checked against sha256sum, not
 *            against the code that produces it;
 *          - the same through the gbuffer door: the kw's one binary field
 *            and a manifest of slices, two files in one record;
 *          - `get-asset` answers INLINE with no web server configured and
 *            with a SIGNED URL once there is one; the token reproduces
 *            what nginx's secure_link_md5 hashes, computed independently;
 *          - an asset a node links cannot be deleted; the treedb's own
 *            guard says so;
 *          - `import-assets` turns a directory already on the node into
 *            N assets, skips what it must not serve, answers path -> id,
 *            and cannot be led out of its root ('..', absolute, symlink);
 *          - `gc-assets` removes exactly the orphans, row and bytes;
 *          - a denied principal is refused.
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
 *  The fixture: a buffer that starts like a png (the sniffer reads the
 *  first bytes) and its sha256, computed OUTSIDE this program with
 *  sha256sum, so the id is checked against an independent answer.
 *
 *      printf '\x89PNG\r\n\x1a\nyuneta-assets-test\n' | sha256sum
 */
#define PNG_FIXTURE     "\x89PNG\r\n\x1a\n" "yuneta-assets-test\n"
#define PNG_FIXTURE_LEN (sizeof(PNG_FIXTURE)-1)
#define PNG_SHA256      "afbcdae99c628ddb7b12813d4ffced63cb6a1c6f52dcdbcef4e641dbe96debb6"

#define JPG_FIXTURE     "\xff\xd8\xff\xe0" "yuneta-assets-test qr\n"
#define JPG_FIXTURE_LEN (sizeof(JPG_FIXTURE)-1)

#define PUBLIC_URL      "/media/"
#define SIGN_SECRET     "test-shared-secret"
#define URL_TTL         900

#define DENIED_USER     "denied@test"

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
    char path_import[PATH_MAX];
} PRIVATE_DATA;

/***************************************************************************
 *  Schema: one consumer topic whose 'foto' and 'qr' are files. Nothing
 *  else: __assets__ and its hooks are treedb's, derived at open.
 ***************************************************************************/
PRIVATE char schema_assets_test[] = "\
{                                                                   \n\
    'topics': [                                                     \n\
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
                    'flag': ['persistent','writable']               \n\
                },                                                  \n\
                'foto': {                                           \n\
                    'header': 'Photo',                              \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey','file']                         \n\
                },                                                  \n\
                'qr': {                                             \n\
                    'header': 'QR',                                 \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey','file']                         \n\
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

    const char *home = getenv("HOME");
    build_path(priv->path_root, sizeof(priv->path_root), home, "tests_yuneta", NULL);
    mkrdir(priv->path_root, 02770);

    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database), priv->path_root, DATABASE, NULL);
    rmrdir(path_database);

    build_path(priv->path_import, sizeof(priv->path_import), priv->path_root, DATABASE "_incoming", NULL);
    if(is_directory(priv->path_import)) {
        /*
         *  The symlink bait of a previous run points OUT of the root:
         *  take it away before the recursive remove follows it.
         */
        char link[PATH_MAX];
        build_path(link, sizeof(link), priv->path_import, "taller", "escape", NULL);
        unlink(link);
        rmrdir(priv->path_import);
    }
    mkrdir(priv->path_import, 02770);

    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", priv->path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    );
    priv->tranger = tranger2_startup(0, jn_tranger, 0);

    /*
     *  C_NODE: the treedb, with the import root the bulk door is confined to
     */
    helper_quote2doublequote(schema_assets_test);
    json_t *jn_schema = legalstring2json(schema_assets_test, TRUE);

    json_t *kw_node = json_pack("{s:I, s:s, s:o, s:i, s:s}",
        "tranger", (json_int_t)(uintptr_t)priv->tranger,
        "treedb_name", TREEDB_NAME,
        "treedb_schema", jn_schema,
        "exit_on_error", LOG_OPT_TRACE_STACK,
        "import_root", priv->path_import
    );
    priv->gobj_node = gobj_create_pure_child("test_node", C_NODE, kw_node, gobj);

    /*
     *  C_ASSETS, pointing at that C_NODE. No 'public_url'/'sign_secret'
     *  yet: the first checks are the INLINE fallback.
     */
    json_t *kw_assets = json_pack("{s:I, s:i}",
        "treedb", (json_int_t)(uintptr_t)priv->gobj_node,
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
PRIVATE json_t *ask_assets(hgobj gobj, const char *command, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    return gobj_command(priv->gobj_assets, command, kw, gobj);
}

PRIVATE json_t *ask_node(hgobj gobj, const char *command, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    return gobj_command(priv->gobj_node, command, kw, gobj);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int resp_result(json_t *resp)
{
    return (int)kw_get_int(0, resp, "result", -1, 0);
}

PRIVATE json_t *resp_data(json_t *resp)
{
    return kw_get_dict_value(0, resp, "data", 0, 0);
}

/***************************************************************************
 *  base64 of a buffer, as a new json string
 ***************************************************************************/
PRIVATE json_t *b64(const char *data, size_t len)
{
    gbuffer_t *gbuf = gbuffer_binary_to_base64(data, len);
    json_t *jn = json_string(gbuffer_cur_rd_pointer(gbuf));
    GBUFFER_DECREF(gbuf)
    return jn;
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

    /*
     *  A collapsed view answers an fkey in the 'list_dict' shape:
     *      [{"id": ..., "topic_name": ..., "hook_name": ...}]
     *  a pure node holds the reference string. Read both.
     */
    json_t *v = json_object_get(node, col);
    if(json_is_string(v)) {
        decode_parent_ref(json_string_value(v), topic, sizeof(topic), id, sizeof(id), hook, sizeof(hook));
    } else if(json_is_array(v) && json_array_size(v) > 0) {
        json_t *first = json_array_get(v, 0);
        if(json_is_object(first)) {
            snprintf(id, sizeof(id), "%s", kw_get_str(0, first, "id", "", 0));
        } else if(json_is_string(first)) {
            decode_parent_ref(json_string_value(first), topic, sizeof(topic), id, sizeof(id), hook, sizeof(hook));
        }
    } else if(json_is_object(v)) {
        const char *k; json_t *vv;
        json_object_foreach(v, k, vv) {
            snprintf(id, sizeof(id), "%s", kw_get_str(0, vv, "id", k, 0));
            break;
        }
    }
    return id;
}

/***************************************************************************
 *  Write one fixture file under the import root
 ***************************************************************************/
PRIVATE int write_fixture(hgobj gobj, const char *rel, const char *content, size_t len)
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
    fwrite(content, 1, len, f);
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
    const char *b64s = gbuffer_cur_rd_pointer(gbuf);
    int j = 0;
    for(int i = 0; b64s[i] && j < bflen - 1; i++) {
        char c = b64s[i];
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
     *  1: the json door. A record with its bytes
     *  beside it, through update-node create+autolink.
     *  The column leaves as an fkey whose id IS the
     *  sha256 of the content.
     *-----------------------------------------------*/
    resp = ask_node(gobj, "update-node",
        json_pack("{s:s, s:{s:s, s:s, s:s, s:s, s:{s:{s:o, s:s, s:s}}}, s:{s:b, s:b}}",
            "topic_name", "devices",
            "record",
                "id", "dev-1",
                "name", "first device",
                "foto", "",
                "qr", "",
                "__files__",
                    "foto",
                        "content64", b64(PNG_FIXTURE, PNG_FIXTURE_LEN),
                        "original_name", "dev-1.png",
                        "content_type", "image/png",
            "options",
                "create", 1,
                "autolink", 1
        )
    );
    if(resp_result(resp) != 0) {
        result += fail(gobj, "update-node with a file failed");
    } else {
        json_t *node = resp_data(resp);
        if(strcmp(fkey_id(node, "foto"), PNG_SHA256) != 0) {
            gobj_trace_json(gobj, node, "foto is not the sha256 of the bytes");
            result += fail(gobj, "the file column did not become an fkey to the sha256");
        }
        if(json_object_get(node, "__files__")) {
            result += fail(gobj, "the transport manifest reached the record");
        }
    }
    JSON_DECREF(resp)

    resp = ask_node(gobj, "node",
        json_pack("{s:s, s:s}", "topic_name", TREEDB_ASSETS_TOPIC, "node_id", PNG_SHA256)
    );
    if(resp_result(resp) != 0) {
        result += fail(gobj, "the asset index node was not created");
    } else {
        json_t *asset = resp_data(resp);
        if(kw_get_int(gobj, asset, "size", 0, 0) != (json_int_t)PNG_FIXTURE_LEN) {
            result += fail(gobj, "asset size is not the size of the bytes");
        }
        if(strcmp(kw_get_str(gobj, asset, "original_name", "", 0), "dev-1.png") != 0) {
            result += fail(gobj, "asset did not keep its original_name");
        }
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  2: get-asset, inline (no web server configured)
     *-----------------------------------------------*/
    resp = ask_assets(gobj, "get-asset", json_pack("{s:s}", "asset_id", PNG_SHA256));
    if(resp_result(resp) != 0) {
        result += fail(gobj, "get-asset failed");
    } else {
        json_t *data = resp_data(resp);
        if(strcmp(kw_get_str(gobj, data, "mode", "", 0), "inline") != 0) {
            result += fail(gobj, "get-asset did not fall back to inline");
        }
        json_t *expected = b64(PNG_FIXTURE, PNG_FIXTURE_LEN);
        if(strcmp(kw_get_str(gobj, data, "content64", "", 0), json_string_value(expected)) != 0) {
            result += fail(gobj, "get-asset inline did not answer the stored bytes");
        }
        JSON_DECREF(expected)
        if(strcmp(kw_get_str(gobj, data, "content_type", "", 0), "image/png") != 0) {
            result += fail(gobj, "get-asset inline lost the content_type");
        }
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  3: get-asset, a signed url once configured
     *-----------------------------------------------*/
    gobj_write_str_attr(priv->gobj_assets, "public_url", PUBLIC_URL);
    gobj_write_str_attr(priv->gobj_assets, "sign_secret", SIGN_SECRET);

    resp = ask_assets(gobj, "get-asset", json_pack("{s:s}", "asset_id", PNG_SHA256));
    if(resp_result(resp) != 0) {
        result += fail(gobj, "get-asset (url) failed");
    } else {
        json_t *data = resp_data(resp);
        const char *url = kw_get_str(gobj, data, "url", "", 0);
        if(strcmp(kw_get_str(gobj, data, "mode", "", 0), "url") != 0 || empty_string(url)) {
            result += fail(gobj, "get-asset did not sign a url");
        } else {
            char expected_uri[PATH_MAX];
            snprintf(expected_uri, sizeof(expected_uri), "%s%c%c/%c%c/%s.png",
                PUBLIC_URL,
                PNG_SHA256[0], PNG_SHA256[1], PNG_SHA256[2], PNG_SHA256[3],
                PNG_SHA256
            );
            char uri[PATH_MAX];
            long long expires = 0;
            char token[80];
            token[0] = 0;
            if(sscanf(url, "%[^?]?e=%lld&s=%79s", uri, &expires, token) != 3) {
                result += fail(gobj, "signed url has not the shape uri?e=..&s=..");
            } else {
                if(strcmp(uri, expected_uri) != 0) {
                    gobj_log_error(gobj, 0,
                        "function", "%s", __FUNCTION__,
                        "msgset",   "%s", MSGSET_INTERNAL,
                        "msg",      "%s", "TEST FAIL",
                        "what",     "%s", "signed url uri",
                        "uri",      "%s", uri,
                        "expected", "%s", expected_uri,
                        NULL
                    );
                    result += -1;
                }
                time_t now = time(NULL);
                if(expires < now + URL_TTL - 5 || expires > now + URL_TTL + 5) {
                    result += fail(gobj, "signed url expires outside url_ttl");
                }
                char expected[80];
                if(expected_token(gobj, expires, uri, expected, sizeof(expected)) == 0) {
                    if(strcmp(token, expected) != 0) {
                        result += fail(gobj, "signed url token is not what nginx would compute");
                    }
                }
            }
        }
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  4: the gbuffer door: one kw, two files
     *-----------------------------------------------*/
    {
        gbuffer_t *gbuf = gbuffer_create(PNG_FIXTURE_LEN + JPG_FIXTURE_LEN, PNG_FIXTURE_LEN + JPG_FIXTURE_LEN);
        gbuffer_append(gbuf, PNG_FIXTURE, PNG_FIXTURE_LEN);
        gbuffer_append(gbuf, JPG_FIXTURE, JPG_FIXTURE_LEN);

        char jpg_sha[SHA256_HEX_LEN + 1];
        sha256_hex(JPG_FIXTURE, JPG_FIXTURE_LEN, jpg_sha, sizeof(jpg_sha));

        resp = ask_node(gobj, "update-node",
            json_pack("{s:s, s:I, s:{s:s, s:s, s:s, s:{s:{s:I, s:I, s:s}, s:{s:I, s:I, s:s}}}, s:{s:b, s:b}}",
                "topic_name", "devices",
                "gbuffer", (json_int_t)(uintptr_t)gbuf,
                "record",
                    "id", "dev-2",
                    "foto", "",
                    "qr", "",
                    "__files__",
                        "foto",
                            "offset", (json_int_t)0,
                            "size", (json_int_t)PNG_FIXTURE_LEN,
                            "original_name", "dev-2.png",
                        "qr",
                            "offset", (json_int_t)PNG_FIXTURE_LEN,
                            "size", (json_int_t)JPG_FIXTURE_LEN,
                            "original_name", "dev-2.jpg",
                "options",
                    "create", 1,
                    "autolink", 1
            )
        );
        if(resp_result(resp) != 0) {
            result += fail(gobj, "update-node with a gbuffer of two slices failed");
        } else {
            json_t *node = resp_data(resp);
            if(strcmp(fkey_id(node, "foto"), PNG_SHA256) != 0) {
                result += fail(gobj, "the first slice did not become the png asset");
            }
            if(strcmp(fkey_id(node, "qr"), jpg_sha) != 0) {
                result += fail(gobj, "the second slice did not become the jpeg asset");
            }
        }
        JSON_DECREF(resp)

        /*  the same png from two devices is ONE asset with TWO parents  */
        resp = ask_node(gobj, "node",
            json_pack("{s:s, s:s}", "topic_name", TREEDB_ASSETS_TOPIC, "node_id", PNG_SHA256)
        );
        json_t *hook = json_object_get(resp_data(resp), "as_devices_foto");
        size_t parents = json_is_array(hook)? json_array_size(hook): json_object_size(hook);
        if(parents != 2) {
            gobj_trace_json(gobj, resp_data(resp), "the shared asset");
            result += fail(gobj, "the same bytes from two devices did not make one asset with two parents");
        }
        JSON_DECREF(resp)
    }

    /*-----------------------------------------------*
     *  4bis: a gbuffer on a write that FAILS
     *
     *  The binary field must be released EXACTLY once, and neither exit
     *  releases it twice: the command door (no topic_name, answered
     *  before the record is even looked at) and the write itself (an
     *  unknown topic, refused inside mt_update_node). A second release
     *  logs "BAD gbuf_decref()", which is an error and fails this test.
     *-----------------------------------------------*/
    {
        static const char *refused[] = {"", "no_such_topic", 0};
        for(int i = 0; refused[i]; i++) {
            gbuffer_t *gbuf = gbuffer_create(PNG_FIXTURE_LEN, PNG_FIXTURE_LEN);
            gbuffer_append(gbuf, PNG_FIXTURE, PNG_FIXTURE_LEN);
            resp = ask_node(gobj, "update-node",
                json_pack("{s:s, s:I, s:{s:s, s:{s:{s:I, s:I, s:s}}}, s:{s:b}}",
                    "topic_name", refused[i],
                    "gbuffer", (json_int_t)(uintptr_t)gbuf,
                    "record",
                        "id", "dev-refused",
                        "__files__",
                            "foto",
                                "offset", (json_int_t)0,
                                "size", (json_int_t)PNG_FIXTURE_LEN,
                                "original_name", "refused.png",
                    "options",
                        "create", 1
                )
            );
            if(resp_result(resp) == 0) {
                result += fail(gobj, "a write refused by its topic was accepted");
            }
            JSON_DECREF(resp)
        }
    }

    /*-----------------------------------------------*
     *  5: a linked asset cannot be deleted
     *-----------------------------------------------*/
    resp = ask_node(gobj, "delete-node",
        json_pack("{s:s, s:{s:s}}", "topic_name", TREEDB_ASSETS_TOPIC, "record", "id", PNG_SHA256)
    );
    if(resp_result(resp) == 0) {
        result += fail(gobj, "delete-node removed an asset a node still links");
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  6: import-assets, and it cannot be led out
     *-----------------------------------------------*/
    result += write_fixture(gobj, "taller/plano.png", PNG_FIXTURE "plan", PNG_FIXTURE_LEN + 4);
    result += write_fixture(gobj, "taller/notes.txt", "not an asset\n", 13);

    /*  '..' is refused by name, an absolute path lands INSIDE the root  */
    resp = ask_node(gobj, "import-assets", json_pack("{s:s}", "source_dir", "../"));
    if(resp_result(resp) == 0) {
        result += fail(gobj, "import-assets accepted '..'");
    }
    JSON_DECREF(resp)
    resp = ask_node(gobj, "import-assets", json_pack("{s:s}", "source_dir", "/etc"));
    if(resp_result(resp) == 0) {
        result += fail(gobj, "import-assets walked out of its import_root through an absolute path");
    }
    JSON_DECREF(resp)

    /*  a symlink out of the root is not followed  */
    {
        char link[PATH_MAX];
        build_path(link, sizeof(link), priv->path_import, "taller", "escape", NULL);
        unlink(link);
        if(symlink("/etc", link) < 0) {
            result += fail(gobj, "cannot create the symlink bait");
        }
    }

    resp = ask_node(gobj, "import-assets", json_pack("{s:s}", "source_dir", "taller"));
    if(resp_result(resp) != 0) {
        result += fail(gobj, "import-assets failed");
    } else {
        json_t *stats = resp_data(resp);
        if(kw_get_int(gobj, stats, "imported", 0, 0) != 1) {
            gobj_trace_json(gobj, stats, "import stats");
            result += fail(gobj, "import-assets did not import exactly the png");
        }
        if(kw_get_int(gobj, stats, "skipped", 0, 0) != 1) {
            result += fail(gobj, "import-assets did not skip what it must not serve");
        }
        char plan_sha[SHA256_HEX_LEN + 1];
        sha256_hex(PNG_FIXTURE "plan", PNG_FIXTURE_LEN + 4, plan_sha, sizeof(plan_sha));
        if(strcmp(kw_get_str(gobj, stats, "files`taller/plano.png", "", 0), plan_sha) != 0) {
            gobj_trace_json(gobj, stats, "import stats");
            result += fail(gobj, "import-assets did not answer path -> id");
        }
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  7: gc-assets removes exactly the orphan
     *-----------------------------------------------*/
    resp = ask_node(gobj, "gc-assets", json_pack("{s:s}", "dry_run", "1"));
    if(resp_result(resp) != 0 || json_array_size(resp_data(resp)) != 1) {
        result += fail(gobj, "gc-assets dry_run did not find exactly the imported orphan");
    }
    JSON_DECREF(resp)

    resp = ask_node(gobj, "nodes", json_pack("{s:s}", "topic_name", TREEDB_ASSETS_TOPIC));
    if(json_array_size(resp_data(resp)) != 3) {
        result += fail(gobj, "gc-assets dry_run deleted something");
    }
    JSON_DECREF(resp)

    resp = ask_node(gobj, "gc-assets", json_object());
    if(resp_result(resp) != 0 || json_array_size(resp_data(resp)) != 1) {
        result += fail(gobj, "gc-assets did not delete exactly the orphan");
    } else {
        const char *gone = json_string_value(json_array_get(resp_data(resp), 0));
        char path[PATH_MAX];
        if(treedb_blob_path(priv->tranger, gone, "image/png", path, sizeof(path)) == 0) {
            if(is_regular_file(path)) {
                result += fail(gobj, "gc-assets left the orphan's bytes on disk");
            }
        }
    }
    JSON_DECREF(resp)

    resp = ask_node(gobj, "nodes", json_pack("{s:s}", "topic_name", TREEDB_ASSETS_TOPIC));
    if(json_array_size(resp_data(resp)) != 2) {
        result += fail(gobj, "gc-assets did not leave the two linked assets");
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  8: a denied principal is refused
     *-----------------------------------------------*/
    resp = ask_assets(gobj, "get-asset",
        json_pack("{s:s, s:s}", "asset_id", PNG_SHA256, "__username__", DENIED_USER)
    );
    if(resp_result(resp) != -403) {
        result += fail(gobj, "a denied principal was allowed to read an asset");
    }
    JSON_DECREF(resp)

    if(result == 0) {
        gobj_log_info(gobj, 0,
            "msgset", "%s", MSGSET_INFO,
            "msg", "%s", "All c_assets tests PASSED",
            NULL
        );
    }
    return result;
}

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
