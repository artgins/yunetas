/***********************************************************************
 *          C_ASSETS.C
 *          Assets GClass.
 *
 *          The way OUT of the bytes a treedb keeps for its 'file' columns:
 *          a signed url a web server checks by itself, or the bytes inline
 *          when there is no web server in front.
 *
 *          Storing is treedb's (tr_treedb: `file` columns, `__assets__`,
 *          `import-assets`, `gc-assets` on C_NODE). This gclass only
 *          publishes what is stored. The design and the reason for it are
 *          in c_assets.h and kernel/c/timeranger2/DESIGN-treedb-files.md.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include <command_parser.h>
#include <kwid.h>
#include <tr_treedb.h>

/*
 *  AFTER gobj.h: the CONFIG_HAVE_* macros come from yuneta_config.h, which
 *  gtypes.h pulls in. Testing them any earlier tests nothing.
 */
#ifdef __linux__
#if defined(CONFIG_HAVE_OPENSSL)
    #include <openssl/evp.h>
#endif
#if defined(CONFIG_HAVE_MBEDTLS)
    #include <mbedtls/md5.h>
#endif
#endif

#if !defined(CONFIG_HAVE_OPENSSL) && !defined(CONFIG_HAVE_MBEDTLS)
    #error "C_ASSETS needs a TLS backend: md5 for the signed url"
#endif

#include "msg_ievent.h"
#include "yunetas_environment.h"
#include "c_assets.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define MD5_BIN_LEN         16

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE json_t *build_not_ready_response(hgobj gobj, json_t *kw);
PRIVATE const char *asked_asset_id(hgobj gobj, json_t *kw);
PRIVATE int md5_base64url(hgobj gobj, const char *data, size_t len, char *bf, int bflen);
PRIVATE int build_blob_rel(hgobj gobj, char *bf, int bflen, const char *id, const char *content_type);
PRIVATE json_t *sign_asset_url(hgobj gobj, const char *rel);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_get_asset(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "authz",        0,              0,          "permission to search"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service where to search the permission. If empty print all service's permissions"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_get_asset[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "asset_id",     0,              0,          "Asset id (sha256 of its content)"),
SDATAPM (DTP_STRING,    "prefer",       0,              "url",      "'url' a signed url when it can, 'inline' always the bytes"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-----------json_fn-------------description--*/
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,        cmd_help,           "Command's help"),
SDATACM (DTP_SCHEMA,    "authzs",           0,          pm_authzs,      cmd_authzs,         "Authorization's help"),

/*-CMD2---type----------name----------------flag------------ali-items-----------json_fn-------------description--*/
SDATACM2 (DTP_SCHEMA,   "get-asset",        SDF_AUTHZ_X,    0,  pm_get_asset,   cmd_get_asset,      "Get one asset as a signed url, or inline when there is no web server in front"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default---------description---------- */
SDATA (DTP_POINTER,     "treedb",           0,                  0,              "C_NODE gobj owning the treedb, EXTERNALLY set. Takes precedence over 'treedb_service'"),
SDATA (DTP_STRING,      "treedb_service",   SDF_RD,             "",             "Service name of the C_NODE that owns the treedb whose __assets__ is served"),
SDATA (DTP_STRING,      "public_url",       SDF_WR,             "",             "Url prefix a web server serves the blobs from, e.g. '/media/'. Empty: 'get-asset' answers inline"),
SDATA (DTP_STRING,      "sign_secret",      SDF_WR,             "",             "Shared secret of the web server's secure_link_md5. Empty: 'get-asset' answers inline"),
SDATA (DTP_INTEGER,     "url_ttl",          SDF_WR,             "900",          "Seconds a signed url stays valid"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,              "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,              "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace assets served"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---------------description--*/
SDATAAUTHZ (DTP_SCHEMA, "read",             0,      0,      0,                  "Permission to read assets"),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj gobj_treedb;              /* the C_NODE that owns the treedb */
    BOOL ready;
    const char *treedb_service;
    const char *public_url;
    const char *sign_secret;
    int32_t url_ttl;
} PRIVATE_DATA;




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
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(treedb_service,        gobj_read_str_attr)
    SET_PRIV(public_url,            gobj_read_str_attr)
    SET_PRIV(sign_secret,           gobj_read_str_attr)
    SET_PRIV(url_ttl,               gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  A cached 'const char *' is a pointer INTO the attribute, so an attr
     *  written at runtime and not repeated here leaves the copy dangling.
     */
    IF_EQ_SET_PRIV(url_ttl,             gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(public_url,        gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(sign_secret,       gobj_read_str_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method play
 *
 *  Everything this service needs belongs to ANOTHER service, so it is
 *  resolved here and not in mt_create: at create time the treedb service
 *  may not exist yet.
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->ready = FALSE;
    priv->gobj_treedb = 0;

    /*--------------------------------------------*
     *      The treedb that holds __assets__
     *
     *  By pointer when the host built it itself
     *  (a hosted C_NODE is no service and
     *  gobj_find_service() will never see it),
     *  by name otherwise.
     *--------------------------------------------*/
    hgobj gobj_treedb = (hgobj)gobj_read_pointer_attr(gobj, "treedb");
    if(!gobj_treedb) {
        if(empty_string(priv->treedb_service)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Neither 'treedb' nor 'treedb_service' is set, C_ASSETS disabled",
                NULL
            );
            return 0;
        }
        gobj_treedb = gobj_find_service(priv->treedb_service, FALSE);
        if(!gobj_treedb) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_PARAMETER,
                "msg",              "%s", "treedb service not found, C_ASSETS disabled",
                "treedb_service",   "%s", priv->treedb_service,
                NULL
            );
            return 0;
        }
    }

    json_t *desc = gobj_topic_desc(gobj_treedb, TREEDB_ASSETS_TOPIC);
    if(!desc) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_TREEDB,
            "msg",              "%s", "__assets__ topic not found in the treedb, C_ASSETS disabled",
            "treedb",           "%s", gobj_short_name(gobj_treedb),
            NULL
        );
        return 0;
    }
    JSON_DECREF(desc)

    priv->gobj_treedb = gobj_treedb;
    priv->ready = TRUE;

    gobj_log_info(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_STARTUP,
        "msg",              "%s", "Assets service ready",
        "treedb",           "%s", gobj_short_name(gobj_treedb),
        "serving",          "%s", empty_string(priv->public_url) || empty_string(priv->sign_secret)?
                                    "inline":"signed url",
        NULL
    );

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->ready = FALSE;
    priv->gobj_treedb = 0;

    return 0;
}




                    /***************************
                     *      Helpers
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *build_not_ready_response(hgobj gobj, json_t *kw)
{
    return msg_iev_build_response(
        gobj,
        -1,
        json_sprintf("%s: assets service is not configured, see the errors of its mt_play",
            gobj_yuno_role_plus_name()
        ),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *  Which asset the caller is asking about.
 *
 *  The parameter is `asset_id` and NOT `id`, and that is not cosmetic:
 *  `command-yuno` hands its WHOLE kw to gobj_list_nodes() as the filter that
 *  picks the yuno, so a parameter named like a field of the yuno record
 *  becomes a filter on that field. An `id` of a sha256 matches no yuno, and
 *  the answer is *"Yuno not found"* -- which names the yuno and never the
 *  parameter, so it reads as the service being missing.
 *
 *  `id` is still accepted, for a caller that reaches this service directly
 *  and never crosses the agent.
 ***************************************************************************/
PRIVATE const char *asked_asset_id(hgobj gobj, json_t *kw)
{
    const char *id = kw_get_str(gobj, kw, "asset_id", "", 0);
    if(empty_string(id)) {
        id = kw_get_str(gobj, kw, "id", "", 0);
    }
    return id;
}

/***************************************************************************
 *  md5 of a buffer, as base64url without padding.
 *
 *  This is nginx's own token format for secure_link_md5: the raw 16 bytes
 *  of the digest, base64'd, with '+' and '/' replaced and '=' dropped.
 *  md5 is not a choice -- it is the only digest that directive computes.
 ***************************************************************************/
PRIVATE int md5_base64url(hgobj gobj, const char *data, size_t len, char *bf, int bflen)
{
    uint8_t digest[MD5_BIN_LEN];

#if defined(CONFIG_HAVE_OPENSSL)
    unsigned int digest_len = 0;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if(!ctx) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "EVP_MD_CTX_new() FAILED",
            NULL
        );
        return -1;
    }
    if(EVP_DigestInit_ex(ctx, EVP_md5(), NULL) != 1 ||
            EVP_DigestUpdate(ctx, data, len) != 1 ||
            EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "openssl md5 FAILED",
            NULL
        );
        EVP_MD_CTX_free(ctx);
        return -1;
    }
    EVP_MD_CTX_free(ctx);
#elif defined(CONFIG_HAVE_MBEDTLS)
    if(mbedtls_md5((const unsigned char *)data, len, digest) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "mbedtls_md5() FAILED",
            NULL
        );
        return -1;
    }
#endif

    gbuffer_t *gbuf_b64 = gbuffer_binary_to_base64((const char *)digest, sizeof(digest));
    if(!gbuf_b64) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "gbuffer_binary_to_base64() FAILED",
            NULL
        );
        return -1;
    }
    const char *b64 = gbuffer_cur_rd_pointer(gbuf_b64);
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
    GBUFFER_DECREF(gbuf_b64)

    return 0;
}

/***************************************************************************
 *  The path of a blob RELATIVE to the blob directory, 'ab/cd/<id>.<ext>':
 *  what goes after `public_url` in a served url. Same layout treedb
 *  writes (treedb_blob_path), so the web server's `alias` is `.blobs/`.
 ***************************************************************************/
PRIVATE int build_blob_rel(hgobj gobj, char *bf, int bflen, const char *id, const char *content_type)
{
    /*
     *  The id goes into a served path, so it is checked the way
     *  treedb_blob_path() checks it -- a length alone would let anything
     *  of 64 characters through, and the two must not disagree about
     *  what an asset id is.
     */
    BOOL is_sha256 = (strlen(id) == SHA256_HEX_LEN);
    for(int i = 0; is_sha256 && i < SHA256_HEX_LEN; i++) {
        if(!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f'))) {
            is_sha256 = FALSE;
        }
    }
    if(!is_sha256) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Asset id is not a lowercase sha256",
            "id",           "%s", id,
            NULL
        );
        return -1;
    }
    if(snprintf(bf, bflen, "%c%c/%c%c/%s.%s",
        id[0], id[1], id[2], id[3], id, treedb_file_ext(content_type)) >= bflen) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Blob relative path too long",
            "id",           "%s", id,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Build the signed url a web server can check on its own.
 *
 *  It MUST reproduce, byte for byte, what this nginx directive hashes:
 *
 *      secure_link       $arg_s,$arg_e;
 *      secure_link_md5   "$secure_link_expires$uri <sign_secret>";
 *
 *  so the message is  <expires><uri><space><secret>  and the token is the
 *  md5 of it in base64url. The client's address is deliberately NOT in it:
 *  it would tie the url to one ip and break every phone that changes
 *  network mid-session. The short lifetime is what limits a leaked url.
 *
 *  Return a json string (yours) or 0 when this node serves inline.
 ***************************************************************************/
PRIVATE json_t *sign_asset_url(hgobj gobj, const char *rel)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(empty_string(priv->public_url) || empty_string(priv->sign_secret)) {
        return 0;
    }

    char uri[PATH_MAX];
    if(build_path(uri, sizeof(uri), priv->public_url, rel, NULL) == NULL) {
        return 0;   // Error already logged
    }

    time_t expires = time(NULL) + (priv->url_ttl > 0? priv->url_ttl: 900);

    char *message = gbmem_malloc(strlen(uri) + strlen(priv->sign_secret) + 64);
    if(!message) {
        return 0;   // Error already logged
    }
    sprintf(message, "%lld%s %s", (long long)expires, uri, priv->sign_secret);

    char token[64];
    int ret = md5_base64url(gobj, message, strlen(message), token, sizeof(token));
    GBMEM_FREE(message)
    if(ret < 0) {
        return 0;   // Error already logged
    }

    return json_sprintf("%s?e=%lld&s=%s", uri, (long long)expires, token);
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

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw)
    json_t *jn_resp = gobj_build_authzs_doc(gobj, cmd, kw);
    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_resp,
        kw  // owned
    );
}

/***************************************************************************
 *  Two shapes of answer, and this service decides which:
 *
 *      {"mode":"url",    "url":"/media/ab/cd/<id>.jpg?e=...&s=..."}
 *      {"mode":"inline", "content_type":"image/jpeg", "content64":"..."}
 *
 *  so a caller has ONE code path, and a node with no web server in front
 *  of it still shows its images instead of showing nothing.
 *
 *  It takes the asset ID and nothing else -- not a node plus a column: the
 *  id is the sha256 of the content, so the url can be cached for ever.
 ***************************************************************************/
PRIVATE json_t *cmd_get_asset(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->ready) {
        return build_not_ready_response(gobj, kw);
    }

    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("%s: no permission to '%s' in service '%s'",
                gobj_yuno_role_plus_name(), permission, gobj_name(gobj)
            ),
            0,
            0,
            kw  // owned
        );
    }

    const char *id = asked_asset_id(gobj, kw);
    if(empty_string(id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: what asset_id?", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    json_t *node = gobj_get_node(
        priv->gobj_treedb,
        TREEDB_ASSETS_TOPIC,
        json_pack("{s:s}", "id", id),
        0,
        src
    );
    if(!node) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: asset '%s' not found", gobj_yuno_role_plus_name(), id),
            0,
            0,
            kw  // owned
        );
    }

    const char *content_type = kw_get_str(gobj, node, "content_type", "", 0);
    char rel[PATH_MAX];
    if(build_blob_rel(gobj, rel, sizeof(rel), id, content_type) < 0) {
        JSON_DECREF(node)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: asset '%s' has an unusable id", gobj_yuno_role_plus_name(), id),
            0,
            0,
            kw  // owned
        );
    }

    json_t *answer = json_pack("{s:s, s:s, s:I}",
        "id",           id,
        "content_type", content_type,
        "size",         kw_get_int(gobj, node, "size", 0, 0)
    );
    JSON_DECREF(node)

    /*----------------------------------------*
     *      A signed url when we can
     *----------------------------------------*/
    const char *prefer = kw_get_str(gobj, kw, "prefer", "url", 0);
    if(strcmp(prefer, "inline") != 0) {
        json_t *jn_url = sign_asset_url(gobj, rel);
        if(jn_url) {
            json_object_set_new(answer, "mode", json_string("url"));
            json_object_set_new(answer, "url", jn_url);
            if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INFO,
                    "msg",          "%s", "asset url signed",
                    "id",           "%s", id,
                    NULL
                );
            }
            return msg_iev_build_response(
                gobj,
                0,
                json_sprintf("%s: asset url signed", gobj_yuno_role_plus_name()),
                0,
                answer,
                kw  // owned
            );
        }
    }

    /*----------------------------------------*
     *      Otherwise the bytes themselves
     *----------------------------------------*/
    json_t *tranger = gobj_read_pointer_attr(priv->gobj_treedb, "tranger");
    char full[PATH_MAX];
    if(!tranger || treedb_blob_path(tranger, id, content_type, full, sizeof(full)) < 0) {
        JSON_DECREF(answer)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot build the blob path of '%s'", gobj_yuno_role_plus_name(), id),
            0,
            0,
            kw  // owned
        );
    }
    gbuffer_t *gbuf_b64 = gbuffer_file2base64(full);
    if(!gbuf_b64) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Asset node exists but its blob does not",
            "id",           "%s", id,
            "path",         "%s", full,
            NULL
        );
        JSON_DECREF(answer)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: asset '%s' has no bytes on disk",
                gobj_yuno_role_plus_name(), id
            ),
            0,
            0,
            kw  // owned
        );
    }
    json_object_set_new(answer, "mode", json_string("inline"));
    json_object_set_new(answer, "content64",
        json_string(gbuffer_cur_rd_pointer(gbuf_b64))
    );
    GBUFFER_DECREF(gbuf_b64)

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "asset served inline",
            "id",           "%s", id,
            NULL
        );
    }

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: asset inline", gobj_yuno_role_plus_name()),
        0,
        answer,
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
    .mt_writing = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_ASSETS);

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
PUBLIC int register_c_assets(void)
{
    return create_gclass(C_ASSETS);
}
