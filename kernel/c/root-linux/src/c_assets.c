/***********************************************************************
 *          C_ASSETS.C
 *          Assets GClass.
 *
 *          Binary assets of treedb nodes: the bytes on disk, the metadata
 *          in the treedb.
 *
 *          The design and the reason for it are in c_assets.h.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
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
    #include <mbedtls/sha256.h>
    #include <mbedtls/md5.h>
#endif
#endif

#if !defined(CONFIG_HAVE_OPENSSL) && !defined(CONFIG_HAVE_MBEDTLS)
    #error "C_ASSETS needs a TLS backend: sha256 for the asset id, md5 for the signed url"
#endif

#include "msg_ievent.h"
#include "yunetas_environment.h"
#include "c_assets.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define SHA256_HEX_LEN      64      /* sha256 as lowercase hex, without the nul */
#define MD5_BIN_LEN         16

/*
 *  The columns C_ASSETS writes and therefore needs the host's schema to
 *  declare. The host owns the schema -- this gclass never creates the
 *  topic, it REFUSES to work when the topic it was pointed at cannot hold
 *  what it is about to write. A silent partial write would be worse: the
 *  bytes would be on disk with no way to find them again.
 */
PRIVATE const char *required_cols[] = {
    "id", "content_type", "size", "t", "original_name", "source_path", "uploaded_by", 0
};

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    hgobj gobj;
    hgobj src;
    const char *root;       /* absolute import root, to cut the relative path */
    const char *uploaded_by;
    BOOL dry_run;
    json_t *stats;          /* imported / skipped / failed / bytes */
} import_ctx_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE BOOL treedb_is_master(hgobj gobj);
PRIVATE json_t *build_readonly_response(hgobj gobj, json_t *kw);
PRIVATE json_t *build_not_ready_response(hgobj gobj, json_t *kw);

PRIVATE int sha256_hex(hgobj gobj, const char *data, size_t len, char *bf, int bflen);
PRIVATE int md5_base64url(hgobj gobj, const char *data, size_t len, char *bf, int bflen);

PRIVATE const char *ext_of_content_type(const char *content_type);
PRIVATE const char *content_type_of_name(const char *name);
PRIVATE BOOL content_type_is_allowed(hgobj gobj, const char *content_type);

PRIVATE int build_blob_rel(hgobj gobj, char *bf, int bflen, const char *id, const char *ext);
PRIVATE gbuffer_t *read_whole_file(hgobj gobj, const char *path, size_t max_size);
PRIVATE json_t *sign_asset_url(hgobj gobj, const char *rel);
PRIVATE json_t *asset_hook_names(hgobj gobj);
PRIVATE BOOL node_is_linked(json_t *node, json_t *hook_names);

PRIVATE json_t *store_asset(
    hgobj gobj,
    gbuffer_t *gbuf,            /* owned */
    const char *content_type,
    const char *original_name,
    const char *source_path,
    const char *uploaded_by,
    hgobj src,
    char *comment,              /* on failure, why */
    int commentlen
);
PRIVATE BOOL import_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,
    char *fullpath,
    const char *directory,
    char *name,
    int level,
    wd_option opt
);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_put_asset(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_get_asset(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_assets(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_delete_asset(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_import_assets(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_gc_assets(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

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
PRIVATE sdata_desc_t pm_put_asset[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "content64",    0,              0,          "Asset content in base64. Use content64=$$(full-path)"),
SDATAPM (DTP_STRING,    "content_type", 0,              0,          "Mime type. Empty: guessed from original_name"),
SDATAPM (DTP_STRING,    "original_name",0,              0,          "Name the asset had where it came from"),
SDATAPM (DTP_STRING,    "source_path",  0,              0,          "Path the asset came from, the bridge a loader links by"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_get_asset[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Asset id (sha256 of its content)"),
SDATAPM (DTP_STRING,    "prefer",       0,              "url",      "'url' a signed url when it can, 'inline' always the bytes"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_list_assets[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_JSON,      "filter",       0,              0,          "Filter on the asset fields"),
SDATAPM (DTP_BOOLEAN,   "orphan",       0,              0,          "Only the assets no node links any more"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_delete_asset[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Asset id"),
SDATAPM (DTP_BOOLEAN,   "force",        0,              0,          "Delete it even if some node still links it"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_import_assets[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "source_dir",   0,              0,          "Directory to import, RELATIVE to the configured import_root"),
SDATAPM (DTP_BOOLEAN,   "dry_run",      0,              0,          "Say what it would do and write nothing"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_gc_assets[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_BOOLEAN,   "dry_run",      0,              0,          "Say what it would delete and delete nothing"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-----------json_fn-------------description--*/
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,        cmd_help,           "Command's help"),
SDATACM (DTP_SCHEMA,    "authzs",           0,          pm_authzs,      cmd_authzs,         "Authorization's help"),

/*-CMD2---type----------name----------------flag------------ali-items-----------json_fn-------------description--*/
SDATACM2 (DTP_SCHEMA,   "put-asset",        SDF_AUTHZ_X,    0,  pm_put_asset,   cmd_put_asset,      "Store one asset, return its id. Same bytes, same id: idempotent"),
SDATACM2 (DTP_SCHEMA,   "get-asset",        SDF_AUTHZ_X,    0,  pm_get_asset,   cmd_get_asset,      "Get one asset as a signed url, or inline when there is no web server in front"),
SDATACM2 (DTP_SCHEMA,   "list-assets",      SDF_AUTHZ_X,    0,  pm_list_assets, cmd_list_assets,    "List the asset metadata. NEVER the bytes"),
SDATACM2 (DTP_SCHEMA,   "delete-asset",     SDF_AUTHZ_X,    0,  pm_delete_asset,cmd_delete_asset,   "Delete one asset. Refused while a node links it"),
SDATACM2 (DTP_SCHEMA,   "import-assets",    SDF_AUTHZ_X,    0,  pm_import_assets,cmd_import_assets, "Import a whole directory already on this node. One command, N assets, no bytes on the wire"),
SDATACM2 (DTP_SCHEMA,   "gc-assets",        SDF_AUTHZ_X,    0,  pm_gc_assets,   cmd_gc_assets,      "Delete the assets no node links any more"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default---------description---------- */
SDATA (DTP_POINTER,     "treedb",           0,                  0,              "C_NODE gobj owning the treedb, EXTERNALLY set. Takes precedence over 'treedb_service'"),
SDATA (DTP_STRING,      "treedb_service",   SDF_RD,             "",             "Service name of the C_NODE that owns the treedb holding the asset topic"),
SDATA (DTP_STRING,      "topic_name",       SDF_RD,             "assets",       "Topic holding the asset metadata"),
SDATA (DTP_STRING,      "store_path",       SDF_RD,             "",             "Absolute blob directory. Empty: built under the realm store, like every other store of the realm"),
SDATA (DTP_STRING,      "store_service",    SDF_RD,             "",             "'service' segment of the realm store. Empty: the yuno role. Set it to the same one the treedb uses, so the blobs sit NEXT TO the treedb they belong to"),
SDATA (DTP_STRING,      "store_tenant",     SDF_RD,             "",             "'tenant' segment of the realm store"),
SDATA (DTP_STRING,      "store_dir",        SDF_RD,             "assets",       "Last segment of the blob directory"),
SDATA (DTP_STRING,      "import_root",      SDF_RD,             "",             "Root that 'import-assets' is confined to. Empty: 'import-assets' is refused"),
SDATA (DTP_STRING,      "public_url",       SDF_WR,             "",             "Url prefix a web server serves the blobs from, e.g. '/assets/'. Empty: 'get-asset' answers inline"),
SDATA (DTP_STRING,      "sign_secret",      SDF_WR,             "",             "Shared secret of the web server's secure_link_md5. Empty: 'get-asset' answers inline"),
SDATA (DTP_INTEGER,     "url_ttl",          SDF_WR,             "900",          "Seconds a signed url stays valid"),
SDATA (DTP_INTEGER,     "max_size",         SDF_RD,             "134217728",    "Largest asset accepted, in bytes (128M). It is a MEMORY limit as much as a policy one: an asset is hashed and written whole, so this much RAM is what one put-asset or one imported file can cost"),
SDATA (DTP_JSON,        "allowed_content_types",SDF_RD,         "[\"image/jpeg\",\"image/png\",\"image/webp\",\"image/gif\",\"application/pdf\",\"video/mp4\",\"video/webm\",\"video/quicktime\",\"video/ogg\",\"video/x-matroska\",\"audio/mpeg\",\"audio/mp4\",\"audio/ogg\",\"audio/wav\",\"audio/webm\",\"audio/flac\"]",
                                                                                "Mime types accepted. 'image/svg+xml' is NOT here on purpose: an svg served from the app's own origin runs script"),
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
{"messages",        "Trace assets stored, served and deleted"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---------------description--*/
SDATAAUTHZ (DTP_SCHEMA, "write",            0,      0,      0,                  "Permission to store and delete assets"),
SDATAAUTHZ (DTP_SCHEMA, "read",             0,      0,      0,                  "Permission to read assets"),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj gobj_treedb;              /* the C_NODE that owns the treedb */
    BOOL ready;                     /* the topic is there and can hold what we write */
    const char *treedb_service;
    const char *topic_name;
    const char *import_root;
    const char *public_url;
    const char *sign_secret;
    int32_t url_ttl;
    json_int_t max_size;
    char path_store[PATH_MAX];      /* absolute blob directory */
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
    SET_PRIV(topic_name,            gobj_read_str_attr)
    SET_PRIV(import_root,           gobj_read_str_attr)
    SET_PRIV(public_url,            gobj_read_str_attr)
    SET_PRIV(sign_secret,           gobj_read_str_attr)
    SET_PRIV(url_ttl,               gobj_read_integer_attr)
    SET_PRIV(max_size,              gobj_read_integer_attr)
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
    priv->path_store[0] = 0;

    /*--------------------------------------------*
     *      The treedb that holds the metadata
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

    /*--------------------------------------------*
     *      The topic must be able to hold what
     *      this gclass is about to write.
     *
     *  Fail closed: writing the bytes and failing
     *  to write the row leaves a blob nothing can
     *  ever find again.
     *--------------------------------------------*/
    json_t *desc = gobj_topic_desc(gobj_treedb, priv->topic_name);
    if(!desc) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_TREEDB,
            "msg",              "%s", "Asset topic not found in the treedb, C_ASSETS disabled",
            "treedb_service",   "%s", priv->treedb_service,
            "topic_name",       "%s", priv->topic_name,
            NULL
        );
        return 0;
    }
    json_t *cols = kw_get_list(gobj, desc, "cols", 0, 0);
    for(int i = 0; required_cols[i]; i++) {
        BOOL found = FALSE;
        size_t idx; json_t *col;
        json_array_foreach(cols, idx, col) {
            const char *col_id = kw_get_str(gobj, col, "id", "", 0);
            if(strcmp(col_id, required_cols[i])==0) {
                found = TRUE;
                break;
            }
        }
        if(!found) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB,
                "msg",          "%s", "Asset topic misses a required column, C_ASSETS disabled",
                "topic_name",   "%s", priv->topic_name,
                "column",       "%s", required_cols[i],
                NULL
            );
            JSON_DECREF(desc)
            return 0;
        }
    }
    JSON_DECREF(desc)

    /*--------------------------------------------*
     *      The directory holding the bytes
     *--------------------------------------------*/
    const char *store_path = gobj_read_str_attr(gobj, "store_path");
    if(!empty_string(store_path)) {
        snprintf(priv->path_store, sizeof(priv->path_store), "%s", store_path);
        if(!is_directory(priv->path_store)) {
            mkrdir(priv->path_store, yuneta_xpermission());
        }
    } else {
        const char *store_service = gobj_read_str_attr(gobj, "store_service");
        if(empty_string(store_service)) {
            store_service = gobj_yuno_role();
        }
        yuneta_realm_store_dir(
            priv->path_store,
            sizeof(priv->path_store),
            store_service,
            gobj_yuno_realm_owner(),
            gobj_yuno_realm_id(),
            gobj_read_str_attr(gobj, "store_tenant"),
            gobj_read_str_attr(gobj, "store_dir"),
            TRUE
        );
    }
    if(!is_directory(priv->path_store)) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_SYSTEM,
            "msg",              "%s", "Cannot create the asset store directory, C_ASSETS disabled",
            "path",             "%s", priv->path_store,
            NULL
        );
        priv->path_store[0] = 0;
        return 0;
    }

    priv->gobj_treedb = gobj_treedb;
    priv->ready = TRUE;

    gobj_log_info(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_STARTUP,
        "msg",              "%s", "Assets service ready",
        "treedb",           "%s", gobj_short_name(gobj_treedb),
        "topic_name",       "%s", priv->topic_name,
        "path",             "%s", priv->path_store,
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
 *  A replica must not be written: its tranger is not the master of the
 *  store, so an append here would be lost or would fight the master.
 ***************************************************************************/
PRIVATE BOOL treedb_is_master(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *tranger = gobj_read_pointer_attr(priv->gobj_treedb, "tranger");
    if(!tranger) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "treedb service without tranger",
            "treedb",           "%s", gobj_short_name(priv->gobj_treedb),
            NULL
        );
        return FALSE;
    }
    return kw_get_bool(gobj, tranger, "master", 0, 0);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *build_readonly_response(hgobj gobj, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return msg_iev_build_response(
        gobj,
        -1,
        json_sprintf("%s: treedb '%s' is READ-ONLY, this yuno is not the master of its tranger",
            gobj_yuno_role_plus_name(),
            gobj_short_name(priv->gobj_treedb)
        ),
        0,
        0,
        kw  // owned
    );
}

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
 *  sha256 of a buffer, as lowercase hex. This is the asset id.
 ***************************************************************************/
PRIVATE int sha256_hex(hgobj gobj, const char *data, size_t len, char *bf, int bflen)
{
    if(bflen < SHA256_HEX_LEN + 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "buffer too small for a sha256",
            "bflen",        "%d", bflen,
            NULL
        );
        return -1;
    }
    uint8_t digest[32];

#if defined(CONFIG_HAVE_OPENSSL)
    /* OpenSSL preferred when both backends are enabled, as in c_authz.c */
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
    if(EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
            EVP_DigestUpdate(ctx, data, len) != 1 ||
            EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "openssl sha256 FAILED",
            NULL
        );
        EVP_MD_CTX_free(ctx);
        return -1;
    }
    EVP_MD_CTX_free(ctx);
#elif defined(CONFIG_HAVE_MBEDTLS)
    if(mbedtls_sha256((const unsigned char *)data, len, digest, 0) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "mbedtls_sha256() FAILED",
            NULL
        );
        return -1;
    }
#endif

    for(int i = 0; i < 32; i++) {
        snprintf(bf + i*2, 3, "%02x", digest[i]);
    }
    bf[SHA256_HEX_LEN] = 0;

    return 0;
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
 *  The extension is what a web server reads to set the Content-Type, so it
 *  is derived from the type we stored, never from the name we were given.
 ***************************************************************************/
PRIVATE const char *ext_of_content_type(const char *content_type)
{
    if(empty_string(content_type)) {
        return "bin";
    }
    if(strcmp(content_type, "image/jpeg")==0) {
        return "jpg";
    }
    if(strcmp(content_type, "image/png")==0) {
        return "png";
    }
    if(strcmp(content_type, "image/webp")==0) {
        return "webp";
    }
    if(strcmp(content_type, "image/gif")==0) {
        return "gif";
    }
    if(strcmp(content_type, "application/pdf")==0) {
        return "pdf";
    }
    if(strcmp(content_type, "video/mp4")==0) {
        return "mp4";
    }
    if(strcmp(content_type, "video/webm")==0) {
        return "webm";
    }
    if(strcmp(content_type, "video/quicktime")==0) {
        return "mov";
    }
    if(strcmp(content_type, "video/ogg")==0) {
        return "ogv";
    }
    if(strcmp(content_type, "video/x-matroska")==0) {
        return "mkv";
    }
    if(strcmp(content_type, "audio/mpeg")==0) {
        return "mp3";
    }
    if(strcmp(content_type, "audio/mp4")==0) {
        return "m4a";
    }
    if(strcmp(content_type, "audio/ogg")==0) {
        return "ogg";
    }
    if(strcmp(content_type, "audio/wav")==0) {
        return "wav";
    }
    if(strcmp(content_type, "audio/webm")==0) {
        return "weba";
    }
    if(strcmp(content_type, "audio/flac")==0) {
        return "flac";
    }
    return "bin";
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *content_type_of_name(const char *name)
{
    if(empty_string(name)) {
        return "";
    }
    const char *dot = strrchr(name, '.');
    if(!dot || !dot[1]) {
        return "";
    }
    dot++;
    if(strcasecmp(dot, "jpg")==0 || strcasecmp(dot, "jpeg")==0) {
        return "image/jpeg";
    }
    if(strcasecmp(dot, "png")==0) {
        return "image/png";
    }
    if(strcasecmp(dot, "webp")==0) {
        return "image/webp";
    }
    if(strcasecmp(dot, "gif")==0) {
        return "image/gif";
    }
    if(strcasecmp(dot, "pdf")==0) {
        return "application/pdf";
    }
    /*
     *  Video and audio. The pairs that share a container are split by
     *  EXTENSION on purpose, because the extension is the only thing a web
     *  server reads to pick a Content-Type: '.webm' is video and '.weba'
     *  audio, '.mp4' video and '.m4a' audio, '.ogv' video and '.ogg' audio.
     */
    if(strcasecmp(dot, "mp4")==0) {
        return "video/mp4";
    }
    if(strcasecmp(dot, "webm")==0) {
        return "video/webm";
    }
    if(strcasecmp(dot, "mov")==0) {
        return "video/quicktime";
    }
    if(strcasecmp(dot, "ogv")==0) {
        return "video/ogg";
    }
    if(strcasecmp(dot, "mkv")==0) {
        return "video/x-matroska";
    }
    if(strcasecmp(dot, "mp3")==0) {
        return "audio/mpeg";
    }
    if(strcasecmp(dot, "m4a")==0) {
        return "audio/mp4";
    }
    if(strcasecmp(dot, "ogg")==0) {
        return "audio/ogg";
    }
    if(strcasecmp(dot, "wav")==0) {
        return "audio/wav";
    }
    if(strcasecmp(dot, "weba")==0) {
        return "audio/webm";
    }
    if(strcasecmp(dot, "flac")==0) {
        return "audio/flac";
    }
    return "";
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL content_type_is_allowed(hgobj gobj, const char *content_type)
{
    json_t *allowed = gobj_read_json_attr(gobj, "allowed_content_types");
    size_t idx; json_t *jn_v;
    json_array_foreach(allowed, idx, jn_v) {
        const char *s = json_string_value(jn_v);
        if(s && strcmp(s, content_type)==0) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  Blob layout: 'ab/cd/<id>.<ext>'.
 *
 *  Two levels of fanout because a flat directory of some ten thousand
 *  files is a directory nobody can look at, and this store is meant to be
 *  looked at when something does not add up.
 ***************************************************************************/
PRIVATE int build_blob_rel(hgobj gobj, char *bf, int bflen, const char *id, const char *ext)
{
    if(strlen(id) != SHA256_HEX_LEN) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Asset id is not a sha256",
            "id",           "%s", id,
            NULL
        );
        return -1;
    }
    for(int i = 0; i < SHA256_HEX_LEN; i++) {
        if(!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f'))) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset",   "%s", MSGSET_PARAMETER,
                "msg",      "%s", "Asset id is not lowercase hex",
                "id",       "%s", id,
                NULL
            );
            return -1;
        }
    }
    if(snprintf(bf, bflen, "%c%c/%c%c/%s.%s", id[0], id[1], id[2], id[3], id, ext) >= bflen) {
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
 *
 ***************************************************************************/
PRIVATE gbuffer_t *read_whole_file(hgobj gobj, const char *path, size_t max_size)
{
    /*
     *  filesize() answers 0 both for an empty file and for a stat that
     *  failed, so the emptiness test IS the failure test here.
     */
    off_t size = filesize(path);
    if(size <= 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "File is empty or cannot be stat'ed",
            "path",         "%s", path,
            NULL
        );
        return 0;
    }
    if((size_t)size > max_size) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "File over max_size",
            "path",         "%s", path,
            "size",         "%ld", (long)size,
            "max_size",     "%ld", (long)max_size,
            NULL
        );
        return 0;
    }
    int fd = open(path, O_RDONLY);
    if(fd < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot open file",
            "path",         "%s", path,
            "errno",        "%s", strerror(errno),
            NULL
        );
        return 0;
    }
    gbuffer_t *gbuf = gbuffer_create(size?size:1, size?size:1);
    if(!gbuf) {
        close(fd);
        return 0;   // Error already logged
    }
    char chunk[16*1024];
    off_t readed = 0;
    while(readed < size) {
        ssize_t ln = read(fd, chunk, sizeof(chunk));
        if(ln <= 0) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset",   "%s", MSGSET_SYSTEM,
                "msg",      "%s", "Cannot read file",
                "path",     "%s", path,
                "errno",    "%s", strerror(errno),
                NULL
            );
            close(fd);
            GBUFFER_DECREF(gbuf)
            return 0;
        }
        gbuffer_append(gbuf, chunk, ln);
        readed += ln;
    }
    close(fd);

    return gbuf;
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

/***************************************************************************
 *  Which fields of an asset node are hooks.
 *
 *  Asked to the SCHEMA and not guessed from the shape of the value: a hook
 *  renders as a dict, a list or -- with the 'hook_size' option -- a plain
 *  integer, so "it looks like a container" is not a test, and with
 *  'hook_size' it is not even distinguishable from 'size'.
 *
 *  Return a list (YOURS), or 0.
 ***************************************************************************/
PRIVATE json_t *asset_hook_names(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *desc = gobj_topic_desc(priv->gobj_treedb, priv->topic_name);
    if(!desc) {
        return 0;   // Error already logged
    }
    /*
     *  topic_desc_hook_names() walks a LIST OF COLS, while gobj_topic_desc()
     *  answers {topic_name, pkey, system_flag, topic_version, cols}. Handing
     *  it the dict does not fail -- json_array_foreach() over an object
     *  iterates nothing -- it answers "no hooks", and then every asset looks
     *  like an orphan.
     */
    json_t *hook_names = topic_desc_hook_names(
        json_incref(kw_get_list(gobj, desc, "cols", 0, KW_REQUIRED))    // owned
    );
    JSON_DECREF(desc)

    return hook_names;
}

/***************************************************************************
 *  An asset nothing links any more is garbage. The hooks are what say so:
 *  every parent of an asset lives in one of them.
 ***************************************************************************/
PRIVATE BOOL node_is_linked(json_t *node, json_t *hook_names)
{
    size_t idx; json_t *jn_name;
    json_array_foreach(hook_names, idx, jn_name) {
        json_t *v = json_object_get(node, json_string_value(jn_name));
        if(!v) {
            continue;
        }
        /*
         *  HACK 'hook_size' does not render a hook as a number: it renders
         *  it as [{"size": N}], one entry per hooked topic. So an EMPTY
         *  hook is a NON-EMPTY list -- [{"size": 0}] -- and "the list has
         *  elements" is exactly the wrong test: it makes every asset look
         *  linked, and gc-assets then deletes nothing at all.
         *
         *  Any other shape counts as LINKED. This answer decides what
         *  gc-assets removes, and not being able to prove that an asset is
         *  an orphan is a reason to keep it.
         */
        if(!json_is_array(v)) {
            return TRUE;
        }
        size_t i; json_t *e;
        json_array_foreach(v, i, e) {
            json_t *jn_size = json_is_object(e)? json_object_get(e, "size"): 0;
            if(!jn_size) {
                return TRUE;
            }
            if(json_integer_value(jn_size) > 0) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

/***************************************************************************
 *  Store one asset: the bytes on disk, one node in the treedb.
 *
 *  IDEMPOTENT by construction. The id is the sha256 of the content, so
 *  storing the same bytes twice is the same asset -- which is what makes a
 *  whole census reload create nothing at all.
 *
 *  Return the node (YOURS) or 0, with the reason in 'comment'.
 ***************************************************************************/
PRIVATE json_t *store_asset(
    hgobj gobj,
    gbuffer_t *gbuf,            /* owned */
    const char *content_type,
    const char *original_name,
    const char *source_path,
    const char *uploaded_by,
    hgobj src,
    char *comment,
    int commentlen
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t size = gbuffer_leftbytes(gbuf);
    if(size == 0) {
        snprintf(comment, commentlen, "asset is empty");
        GBUFFER_DECREF(gbuf)
        return 0;
    }
    if((json_int_t)size > priv->max_size) {
        snprintf(comment, commentlen,
            "asset of %ld bytes is over max_size (%ld)", (long)size, (long)priv->max_size
        );
        GBUFFER_DECREF(gbuf)
        return 0;
    }

    if(empty_string(content_type)) {
        content_type = content_type_of_name(original_name);
    }
    if(!content_type_is_allowed(gobj, content_type)) {
        snprintf(comment, commentlen,
            "content_type '%s' not in 'allowed_content_types'",
            empty_string(content_type)? "": content_type
        );
        GBUFFER_DECREF(gbuf)
        return 0;
    }

    /*--------------------------------------------*
     *      The id is the content
     *--------------------------------------------*/
    char id[SHA256_HEX_LEN + 1];
    if(sha256_hex(gobj, gbuffer_cur_rd_pointer(gbuf), size, id, sizeof(id)) < 0) {
        snprintf(comment, commentlen, "cannot hash the asset");
        GBUFFER_DECREF(gbuf)
        return 0;   // Error already logged
    }

    char rel[PATH_MAX];
    if(build_blob_rel(gobj, rel, sizeof(rel), id, ext_of_content_type(content_type)) < 0) {
        snprintf(comment, commentlen, "cannot build the blob path");
        GBUFFER_DECREF(gbuf)
        return 0;   // Error already logged
    }
    char full[PATH_MAX];
    if(build_path(full, sizeof(full), priv->path_store, rel, NULL) == NULL) {
        snprintf(comment, commentlen, "blob path too long");
        GBUFFER_DECREF(gbuf)
        return 0;   // Error already logged
    }

    /*--------------------------------------------*
     *      Write the bytes, unless they are
     *      already there with the same length.
     *
     *  The name IS the hash, so a file of the same
     *  size at that path is the same file.
     *--------------------------------------------*/
    if(!(is_regular_file(full) && filesize(full) == (off_t)size)) {
        char dir[PATH_MAX];
        snprintf(dir, sizeof(dir), "%s", full);
        char *p = strrchr(dir, '/');
        if(p) {
            *p = 0;
            if(!is_directory(dir)) {
                mkrdir(dir, yuneta_xpermission());
            }
        }
        if(gbuf2file(gobj, gbuf, full, yuneta_rpermission(), TRUE) < 0) {
            snprintf(comment, commentlen, "cannot write the blob");
            return 0;   // Error already logged, gbuf was owned by gbuf2file
        }
    } else {
        GBUFFER_DECREF(gbuf)
    }

    /*--------------------------------------------*
     *      One node, created or refreshed
     *--------------------------------------------*/
    json_t *record = json_pack("{s:s, s:s, s:I, s:s, s:s, s:s}",
        "id",               id,
        "content_type",     content_type,
        "size",             (json_int_t)size,
        "original_name",    empty_string(original_name)? "": original_name,
        "source_path",      empty_string(source_path)? "": source_path,
        "uploaded_by",      empty_string(uploaded_by)? "": uploaded_by
    );
    json_t *node = gobj_update_node(
        priv->gobj_treedb,
        priv->topic_name,
        record,     // owned
        json_pack("{s:b}", "create", 1),
        src
    );
    if(!node) {
        snprintf(comment, commentlen, "cannot write the asset node");
        return 0;   // Error already logged
    }

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "asset stored",
            "id",           "%s", id,
            "size",         "%ld", (long)size,
            "source_path",  "%s", empty_string(source_path)? "": source_path,
            NULL
        );
    }

    return node;
}

/***************************************************************************
 *  One file of the import walk.
 ***************************************************************************/
PRIVATE BOOL import_cb(
    hgobj gobj_,
    void *user_data,
    wd_found_type type,
    char *fullpath,
    const char *directory,
    char *name,
    int level,
    wd_option opt
)
{
    import_ctx_t *ctx = user_data;
    hgobj gobj = ctx->gobj;

    if(type != WD_TYPE_REGULAR_FILE) {
        return TRUE;
    }

    /*
     *  What the loader will link by: the path the asset came from,
     *  relative to the import root, which is the same string the census
     *  files already carry.
     */
    const char *source_path = fullpath;
    size_t root_len = strlen(ctx->root);
    if(strncmp(fullpath, ctx->root, root_len)==0) {
        source_path = fullpath + root_len;
        while(*source_path == '/') {
            source_path++;
        }
    }

    const char *content_type = content_type_of_name(name);
    if(empty_string(content_type) || !content_type_is_allowed(gobj, content_type)) {
        json_object_set_new(ctx->stats, "skipped",
            json_integer(kw_get_int(gobj, ctx->stats, "skipped", 0, 0) + 1)
        );
        return TRUE;
    }

    if(ctx->dry_run) {
        json_object_set_new(ctx->stats, "would_import",
            json_integer(kw_get_int(gobj, ctx->stats, "would_import", 0, 0) + 1)
        );
        return TRUE;
    }

    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = read_whole_file(gobj, fullpath, (size_t)priv->max_size);
    if(!gbuf) {
        json_object_set_new(ctx->stats, "failed",
            json_integer(kw_get_int(gobj, ctx->stats, "failed", 0, 0) + 1)
        );
        return TRUE;    // Error already logged; one bad file must not stop the import
    }
    size_t size = gbuffer_leftbytes(gbuf);

    char comment[120];
    comment[0] = 0;
    json_t *node = store_asset(
        gobj, gbuf, content_type, name, source_path, ctx->uploaded_by, ctx->src,
        comment, sizeof(comment)
    );
    if(!node) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL,
            "msg",          "%s", "cannot import asset",
            "path",         "%s", fullpath,
            "cause",        "%s", comment,
            NULL
        );
        json_object_set_new(ctx->stats, "failed",
            json_integer(kw_get_int(gobj, ctx->stats, "failed", 0, 0) + 1)
        );
        return TRUE;
    }
    JSON_DECREF(node)

    json_object_set_new(ctx->stats, "imported",
        json_integer(kw_get_int(gobj, ctx->stats, "imported", 0, 0) + 1)
    );
    json_object_set_new(ctx->stats, "bytes",
        json_integer(kw_get_int(gobj, ctx->stats, "bytes", 0, 0) + (json_int_t)size)
    );

    return TRUE;
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
 *
 ***************************************************************************/
PRIVATE json_t *cmd_put_asset(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->ready) {
        return build_not_ready_response(gobj, kw);
    }
    if(!treedb_is_master(gobj)) {
        return build_readonly_response(gobj, kw);
    }

    const char *permission = "write";
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

    const char *content64 = kw_get_str(gobj, kw, "content64", "", 0);
    if(empty_string(content64)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: no data in content64", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    gbuffer_t *gbuf = gbuffer_base64_to_binary(content64, strlen(content64));
    if(!gbuf) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: bad data in content64", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    char comment[120];
    comment[0] = 0;
    json_t *node = store_asset(
        gobj,
        gbuf,   // owned
        kw_get_str(gobj, kw, "content_type", "", 0),
        kw_get_str(gobj, kw, "original_name", "", 0),
        kw_get_str(gobj, kw, "source_path", "", 0),
        kw_get_str(gobj, kw, "__username__", "", 0),
        src,
        comment,
        sizeof(comment)
    );
    if(!node) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot store the asset: %s",
                gobj_yuno_role_plus_name(), comment
            ),
            0,
            0,
            kw  // owned
        );
    }

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: asset stored", gobj_yuno_role_plus_name()),
        gobj_topic_desc(priv->gobj_treedb, priv->topic_name),
        node,
        kw  // owned
    );
}

/***************************************************************************
 *  Two shapes of answer, and this service decides which:
 *
 *      {"mode":"url",    "url":"/assets/ab/cd/<id>.jpg?e=...&s=..."}
 *      {"mode":"inline", "content_type":"image/jpeg", "content64":"..."}
 *
 *  so a caller has ONE code path, and a node with no web server in front
 *  of it still shows its images instead of showing nothing.
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

    const char *id = kw_get_str(gobj, kw, "id", "", 0);
    if(empty_string(id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: what id?", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    json_t *node = gobj_get_node(
        priv->gobj_treedb,
        priv->topic_name,
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
    if(build_blob_rel(gobj, rel, sizeof(rel), id, ext_of_content_type(content_type)) < 0) {
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
    char full[PATH_MAX];
    if(build_path(full, sizeof(full), priv->path_store, rel, NULL) == NULL) {
        JSON_DECREF(answer)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: blob path too long", gobj_yuno_role_plus_name()),
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

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: asset inline", gobj_yuno_role_plus_name()),
        0,
        answer,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_assets(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
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

    BOOL orphan = kw_get_bool(gobj, kw, "orphan", 0, 0);

    /*
     *  'hook_size' instead of the hooks themselves: this list can be the
     *  whole store, and the parents of an asset are not what was asked for.
     */
    json_t *iter = gobj_list_nodes(
        priv->gobj_treedb,
        priv->topic_name,
        json_incref(kw_get_dict(gobj, kw, "filter", 0, 0)),
        json_pack("{s:b}", "hook_size", 1),
        src
    );

    if(orphan) {
        json_t *hook_names = asset_hook_names(gobj);
        json_t *orphans = json_array();
        size_t idx; json_t *node;
        json_array_foreach(iter, idx, node) {
            if(!node_is_linked(node, hook_names)) {
                json_array_append(orphans, node);
            }
        }
        JSON_DECREF(hook_names)
        JSON_DECREF(iter)
        iter = orphans;
    }

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: %d assets", gobj_yuno_role_plus_name(), (int)json_array_size(iter)),
        gobj_topic_desc(priv->gobj_treedb, priv->topic_name),
        iter,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_delete_asset(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->ready) {
        return build_not_ready_response(gobj, kw);
    }
    if(!treedb_is_master(gobj)) {
        return build_readonly_response(gobj, kw);
    }

    const char *permission = "write";
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

    const char *id = kw_get_str(gobj, kw, "id", "", 0);
    if(empty_string(id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: what id?", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    BOOL force = kw_get_bool(gobj, kw, "force", 0, 0);

    json_t *node = gobj_get_node(
        priv->gobj_treedb,
        priv->topic_name,
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
    int ret_rel = build_blob_rel(gobj, rel, sizeof(rel), id, ext_of_content_type(content_type));
    JSON_DECREF(node)

    /*
     *  The treedb decides whether it is still linked -- without 'force' it
     *  refuses a node that has parents, and that refusal is exactly the
     *  answer we want here.
     */
    if(gobj_delete_node(
        priv->gobj_treedb,
        priv->topic_name,
        json_pack("{s:s}", "id", id),
        force? json_pack("{s:b}", "force", 1): 0,
        src
    ) < 0) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot delete asset '%s': %s",
                gobj_yuno_role_plus_name(), id, gobj_log_last_message()
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  The row is gone; the bytes go with it. Order matters: a blob with
     *  no row is unreachable garbage, a row with no blob is a broken
     *  image -- so the row goes first and the file second.
     */
    if(ret_rel == 0) {
        char full[PATH_MAX];
        if(build_path(full, sizeof(full), priv->path_store, rel, NULL) != NULL) {
            if(is_regular_file(full) && unlink(full) < 0) {
                gobj_log_error(gobj, 0,
                    "function", "%s", __FUNCTION__,
                    "msgset",   "%s", MSGSET_SYSTEM,
                    "msg",      "%s", "Asset row deleted but its blob could not be removed",
                    "path",     "%s", full,
                    "errno",    "%s", strerror(errno),
                    NULL
                );
            }
        }
    }

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: asset '%s' deleted", gobj_yuno_role_plus_name(), id),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *  One command, N assets, and NOT ONE BYTE on the wire.
 *
 *  The bytes are already on this node -- that is the whole point. Sending
 *  a census of some hundreds of megabytes through the control plane, one
 *  base64 message per file, is the thing this command exists to avoid.
 ***************************************************************************/
PRIVATE json_t *cmd_import_assets(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->ready) {
        return build_not_ready_response(gobj, kw);
    }
    if(!treedb_is_master(gobj)) {
        return build_readonly_response(gobj, kw);
    }

    const char *permission = "write";
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

    /*----------------------------------------*
     *  This command READS AN ARBITRARY PATH,
     *  so it is confined to a configured root
     *  and refused outright when there is none.
     *
     *  Two things do the confining, and only one
     *  of them is visible here:
     *
     *  - the '..' guard below, which refuses
     *    instead of silently resolving somewhere
     *    else, so the caller learns what happened;
     *  - build_path(), which strips the leading '/'
     *    of every segment after the first and
     *    clamps '..' against it. That is what makes
     *    an ABSOLUTE source_dir land INSIDE the
     *    root ('/etc' -> '<import_root>/etc')
     *    rather than at '/etc'.
     *
     *  So do not "simplify" either one away on the
     *  grounds that the other covers it. And note
     *  walk_dir_tree() lstat()s: a symlink is
     *  neither a regular file nor a directory, so
     *  the walk cannot be led out by one either.
     *  tests/c/c_assets checks all three with
     *  hostile input.
     *----------------------------------------*/
    if(empty_string(priv->import_root)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: 'import-assets' is disabled, attribute 'import_root' is empty",
                gobj_yuno_role_plus_name()
            ),
            0,
            0,
            kw  // owned
        );
    }
    const char *source_dir = kw_get_str(gobj, kw, "source_dir", "", 0);
    if(strstr(source_dir, "..")) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: 'source_dir' cannot contain '..'", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    char root[PATH_MAX];
    if(build_path(root, sizeof(root), priv->import_root, source_dir, NULL) == NULL) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: import path too long", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    if(!is_directory(root)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: '%s' is not a directory", gobj_yuno_role_plus_name(), root),
            0,
            0,
            kw  // owned
        );
    }

    BOOL dry_run = kw_get_bool(gobj, kw, "dry_run", 0, 0);

    json_t *stats = json_pack("{s:s, s:b, s:i, s:i, s:i, s:i, s:i}",
        "source_dir",   root,
        "dry_run",      dry_run,
        "imported",     0,
        "would_import", 0,
        "skipped",      0,
        "failed",       0,
        "bytes",        0
    );
    import_ctx_t ctx = {
        .gobj = gobj,
        .src = src,
        .root = priv->import_root,
        .uploaded_by = kw_get_str(gobj, kw, "__username__", "", 0),
        .dry_run = dry_run,
        .stats = stats
    };

    if(walk_dir_tree(
        gobj,
        root,
        0,
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        import_cb,
        &ctx
    ) < 0) {
        JSON_DECREF(stats)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot walk '%s'", gobj_yuno_role_plus_name(), root),
            0,
            0,
            kw  // owned
        );
    }

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: %s %lld assets, %lld skipped, %lld failed",
            gobj_yuno_role_plus_name(),
            dry_run? "would import": "imported",
            (long long)kw_get_int(gobj, stats, dry_run? "would_import": "imported", 0, 0),
            (long long)kw_get_int(gobj, stats, "skipped", 0, 0),
            (long long)kw_get_int(gobj, stats, "failed", 0, 0)
        ),
        0,
        stats,
        kw  // owned
    );
}

/***************************************************************************
 *  Deleting orphans is NEVER automatic: an asset with no link today can be
 *  the asset a half-finished load links tomorrow. Somebody asks for this.
 ***************************************************************************/
PRIVATE json_t *cmd_gc_assets(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->ready) {
        return build_not_ready_response(gobj, kw);
    }
    if(!treedb_is_master(gobj)) {
        return build_readonly_response(gobj, kw);
    }

    const char *permission = "write";
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

    BOOL dry_run = kw_get_bool(gobj, kw, "dry_run", 0, 0);

    json_t *iter = gobj_list_nodes(
        priv->gobj_treedb,
        priv->topic_name,
        0,
        json_pack("{s:b}", "hook_size", 1),
        src
    );
    json_t *hook_names = asset_hook_names(gobj);
    json_t *deleted = json_array();
    int failed = 0;

    size_t idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        if(node_is_linked(node, hook_names)) {
            continue;
        }
        const char *id = kw_get_str(gobj, node, "id", "", 0);
        if(empty_string(id)) {
            continue;
        }
        if(dry_run) {
            json_array_append_new(deleted, json_string(id));
            continue;
        }
        const char *content_type = kw_get_str(gobj, node, "content_type", "", 0);
        char rel[PATH_MAX];
        int ret_rel = build_blob_rel(gobj, rel, sizeof(rel), id, ext_of_content_type(content_type));

        if(gobj_delete_node(
            priv->gobj_treedb,
            priv->topic_name,
            json_pack("{s:s}", "id", id),
            0,
            src
        ) < 0) {
            failed++;
            continue;   // Error already logged
        }
        if(ret_rel == 0) {
            char full[PATH_MAX];
            if(build_path(full, sizeof(full), priv->path_store, rel, NULL) != NULL) {
                if(is_regular_file(full) && unlink(full) < 0) {
                    gobj_log_error(gobj, 0,
                        "function", "%s", __FUNCTION__,
                        "msgset",   "%s", MSGSET_SYSTEM,
                        "msg",      "%s", "Asset row deleted but its blob could not be removed",
                        "path",     "%s", full,
                        "errno",    "%s", strerror(errno),
                        NULL
                    );
                }
            }
        }
        json_array_append_new(deleted, json_string(id));
    }
    JSON_DECREF(hook_names)
    JSON_DECREF(iter)

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: %s %d orphan assets, %d failed",
            gobj_yuno_role_plus_name(),
            dry_run? "would delete": "deleted",
            (int)json_array_size(deleted),
            failed
        ),
        0,
        deleted,
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
