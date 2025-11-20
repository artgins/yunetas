/***********************************************************************
 *          c_authz.c
 *          Authz GClass.
 *
 *          Authentication and Authorization Manager
 *
 *          Maintain a JWKS built from json config or command line
 *          Built a CHECKER from keys of JWKS
 *          libjwt has been modified to use in yuneta environment:
 *              see in the jwt.h the functions marked with ArtGins
 *
 *          Copyright (c) 2020 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <unistd.h>
#include <grp.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <pwd.h>
#include <jwt.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>

#ifdef __linux__
#if defined(CONFIG_HAVE_OPENSSL)
    #include <openssl/ssl.h>
    #include <openssl/rand.h>
#elif defined(CONFIG_HAVE_MBEDTLS)
    #include <mbedtls/md.h>
    #include <mbedtls/ctr_drbg.h>
    #include <mbedtls/pkcs5.h>
#else
    #error "No crypto library defined"
#endif
#endif

#include <command_parser.h>
#include <timeranger2.h>
#include <tr_treedb.h>
#include "msg_ievent.h"
#include "yunetas_environment.h"
#include "c_yuno.h"
#include "c_tranger.h"
#include "c_node.h"
#include "c_authz.h"

#include "treedb_schema_authzs.c"
#include "../../libjwt/src/jwt-private.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int add_user_login(
    hgobj gobj,
    const char *username,
    json_t *jwt_payload,
    const char *peername
);

PRIVATE json_t *identify_system_user(
    hgobj gobj,
    const char **username,
    BOOL include_groups,
    BOOL verbose
);

PRIVATE json_t *get_user_roles(
    hgobj gobj,
    const char *dst_realm_id,
    const char *dst_service,
    const char *username,
    json_t *kw  // not owned
);

PRIVATE int create_validation_key(
    hgobj gobj,
    json_t *jn_jwk // owned
);
PRIVATE int destroy_validation_key(
    hgobj gobj,
    const char *kid
);

PRIVATE int create_jwt_validations(hgobj gobj);
PRIVATE int destroy_jwt_validations(hgobj gobj);
PRIVATE BOOL verify_token(hgobj gobj, const char *token, json_t **jwt_payload, const char **status);

PRIVATE json_t *hash_password(
    hgobj gobj,
    const char *password,
    const char *digest,
    unsigned int iterations
);
PRIVATE int check_password(
    hgobj gobj,
    const char *username,
    const char *password
);

/***************************************************************************
 *              Resources
 ***************************************************************************/
/*
 *  JWK Json Web Key
 */
static const json_desc_t jwk_desc[] = {
// Name             Type        Defaults    Fillspace
{"kid",             "string",   "",         "60"},  // First item is the pkey, same as iss
{"description",     "string",   "",         "20"},
{"use",             "string",   "sig",      "10"},
{"kty",             "string",   "RSA",      "10"},
{"alg",             "string",   "RS256",    "10"},
{"n",               "string",   "",         "20"},  // same as pkey
{"e",               "string",   "AQAB",     "10"},
{"x5c",             "string",   "",         "10"},
{0}
};

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_list_jwk(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_add_jwk(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_remove_jwk(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_users(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_accesses(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_create_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_enable_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_disable_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_delete_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_user_passw(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_check_user_passw(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_roles(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_user_roles(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_user_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag----default-----description---------- */
SDATAPM (DTP_STRING, "cmd",             0,      0,          "command about you want help."),
SDATAPM (DTP_INTEGER,  "level",         0,      0,          "level=1: search in bottoms, level=2: search in all childs"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_add_jwk[] = {
/*-PM----type-----------name------------flag----default-----description---------- */
SDATAPM (DTP_STRING,    "kid",          0,      0,          "Key ID – used to match JWT's kid to the correct key, same as 'iss'"),
SDATAPM (DTP_STRING,    "iss",          0,      0,          "Key ID – used to match JWT's kid to the correct key, same as 'kid'"),
SDATAPM (DTP_STRING,    "kty",          0,      "RSA",      "Key type (e.g. RSA, EC)"),
SDATAPM (DTP_STRING,    "use",          0,      "sig",      "Intended key use (sig = signature, enc = encryption)"),
SDATAPM (DTP_STRING,    "alg",          0,      "RS256",    "Algorithm used with the key (e.g. RS256)"),
SDATAPM (DTP_STRING,    "n",            0,      0,          "RSA modulus, base64url-encoded, same as 'pkey'"),
SDATAPM (DTP_STRING,    "pkey",         0,      0,          "RSA modulus, base64url-encoded, same as 'n'"),
SDATAPM (DTP_STRING,    "e",            0,      "AQAB",     "RSA exponent, usually 'AQAB' (65537), base64url-encoded"),
SDATAPM (DTP_STRING,    "x5c",          0,      0,          "(Optional) X.509 certificate chain"),

SDATAPM (DTP_STRING,    "description",  0,      0,          "Description"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_rm_jwk[] = {
/*-PM----type-----------name------------flag----default-----description---------- */
SDATAPM (DTP_STRING,    "kid",          0,      0,          "Key ID, same as 'iss'"),
SDATAPM (DTP_STRING,    "iss",          0,      0,          "Issuer, same as 'kid'"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag----default-----description---------- */
SDATAPM (DTP_STRING,    "authz",        0,      0,          "permission to search"),
SDATAPM (DTP_STRING,    "service",      0,      0,          "Service where to search the permission. If empty print all service's permissions"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_users[] = {
/*-PM----type-----------name------------flag----default-----description---------- */
SDATAPM (DTP_JSON,      "filter",       0,      0,          "Filter"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_create_user[] = {
/*-PM----type-----------name------------flag----default-----description---------- */
SDATAPM (DTP_STRING,    "username",     0,      0,          "Username"),
SDATAPM (DTP_STRING,    "role",         0,      0,          "ROLE format: roles^ROLE^users"),
SDATAPM (DTP_BOOLEAN,   "disabled",     0,      0,          "Disabled"),

SDATAPM (DTP_STRING,    "password",     0,      0,          "Password"),
SDATAPM (DTP_INTEGER,   "hashIterations",0,     "27500",    "Default To build a password"),
SDATAPM (DTP_STRING,    "algorithm",    0,      "sha256",   "Default To build a password"),

SDATA_END()
};
PRIVATE sdata_desc_t pm_user[] = {
/*-PM----type-----------name------------flag----default-----description---------- */
SDATAPM (DTP_STRING,    "username",     0,      0,          "Username"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_check_passw[] = {
/*-PM----type-----------name------------flag----default-----description---------- */
SDATAPM (DTP_STRING,    "username",     0,      0,          "Username"),
SDATAPM (DTP_STRING,    "password",     0,      0,          "Password"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_set_passw[] = {
/*-PM----type-----------name------------flag----default-----description---------- */
SDATAPM (DTP_STRING,    "username",     0,      0,          "Username"),
SDATAPM (DTP_STRING,    "password",     0,      0,          "Password"),
SDATAPM (DTP_INTEGER,   "hashIterations",0,     "27500",    "Default To build a password"),
SDATAPM (DTP_STRING,    "algorithm",    0,      "sha256",   "Default To build a password"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_roles[] = {
/*-PM----type-----------name------------flag----default-----description---------- */
SDATAPM (DTP_JSON,      "filter",       0,      0,          "Filter"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_user_roles[] = {
/*-PM----type-----------name------------flag----default-----description---------- */
SDATAPM (DTP_STRING,    "username",     0,      0,          "Username"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_user_authzs[] = {
/*-PM----type-----------name------------flag----default-----description---------- */
SDATAPM (DTP_STRING,    "username",     0,      0,          "Username"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};
PRIVATE const char *a_users[] = {"list-users", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help, pm_help,        cmd_help,       "Command's help"),
SDATACM (DTP_SCHEMA,    "authzs",           0,      pm_authzs,      cmd_authzs,     "Authorization's help"),

/*-CMD2---type----------name------------flag------------alias---items---------------json_fn-------------description--*/
SDATACM2(DTP_SCHEMA,    "list-jwk",     SDF_AUTHZ_X,    0,      0,              cmd_list_jwk,   "List OAuth2 JWKs"),
SDATACM2(DTP_SCHEMA,    "add-jwk",      SDF_AUTHZ_X,    0,      pm_add_jwk,     cmd_add_jwk,    "Add OAuth2 JWK"),
SDATACM2(DTP_SCHEMA,    "remove-jwk",   SDF_AUTHZ_X,    0,      pm_rm_jwk,      cmd_remove_jwk, "Remove OAuth2 JWK"),
SDATACM2(DTP_SCHEMA,    "users",        SDF_AUTHZ_X,    a_users,pm_users,       cmd_users,      "List users"),
SDATACM2(DTP_SCHEMA,    "accesses",     SDF_AUTHZ_X,    0,      pm_users,       cmd_accesses,   "List user accesses"),
SDATACM2(DTP_SCHEMA,    "create-user",  SDF_AUTHZ_X,    0,      pm_create_user, cmd_create_user,"Create or update user (see ROLE format)"),
SDATACM2(DTP_SCHEMA,    "enable-user",  SDF_AUTHZ_X,    0,      pm_user,        cmd_enable_user,"Enable user"),
SDATACM2(DTP_SCHEMA,    "disable-user", SDF_AUTHZ_X,    0,      pm_user,        cmd_disable_user,"Disable user"),
SDATACM2 (DTP_SCHEMA,   "delete-user",  SDF_AUTHZ_X,    0,      pm_user,        cmd_delete_user, "Delete user"),
SDATACM2 (DTP_SCHEMA,   "check-user-pwd",SDF_AUTHZ_X,   0,      pm_check_passw, cmd_check_user_passw, "Check user password"),
SDATACM2 (DTP_SCHEMA,   "set-user-pwd", SDF_AUTHZ_X,    0,      pm_set_passw,   cmd_set_user_passw, "Set user password"),

SDATACM2(DTP_SCHEMA,    "roles",        SDF_AUTHZ_X,    0,      pm_roles,       cmd_roles,      "List roles"),
SDATACM2(DTP_SCHEMA,    "user-roles",   SDF_AUTHZ_X,    0,      pm_user_roles,  cmd_user_roles, "Get roles of user"),
SDATACM2(DTP_SCHEMA,    "user-authzs",  SDF_AUTHZ_X,    0,      pm_user_authzs, cmd_user_authzs,"Get permissions of user"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type--------name----------------flag--------default-----description---------- */
/*
 *  HACK WARNING 2024-Nov-13: use of "tranger_path" to determine if this instance is master or not.
 *  If tranger_path is empty, then
 *      the class uses yuneta_realm_store_dir() to setup the tranger "authzs" as master TRUE
 *  if it's not empty:
 *      use master as set externally
 */
SDATA (DTP_STRING,  "tranger_path",     SDF_RD,     "",         "Tranger path, internal value (or not)"),
SDATA (DTP_STRING,  "authz_service",    SDF_RD,     "",         "If tranger_path is empty you can force the service where build the authz. If authz_service is empty then it will be the yuno_role"),
SDATA (DTP_STRING,  "authz_tenant",     SDF_RD,     "",         "Used for multi-tenant service"),
SDATA (DTP_BOOLEAN, "master",           SDF_RD,     "0",        "the master is the only that can write, if tranger_path is empty is set to TRUE internally"),

SDATA (DTP_INTEGER, "max_sessions_per_user",SDF_PERSIST,    "1",        "Max sessions per user"),
SDATA (DTP_JSON,    "jwks",                 SDF_WR|SDF_PERSIST, "[]",   "JWKS public keys, OLD jwt_public_keys, use the utility keycloak_pkey_to_jwks to create."),
SDATA (DTP_JSON,    "initial_load",         SDF_RD,         "{}",       "Initial data for treedb"),

SDATA (DTP_INTEGER, "hashIterations",   0,          "27500",    "Default To build a password"),
SDATA (DTP_STRING,  "algorithm",        0,          "sha256",   "Default To build a password"),

SDATA (DTP_POINTER, "user_data",        0,          0,          "user data"),
SDATA (DTP_POINTER, "user_data2",       0,          0,          "more user data"),
SDATA (DTP_POINTER, "subscriber",       0,          0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendant value!
 *  required paired correlative strings
 *  in s_user_trace_level
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

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int32_t max_sessions_per_user;

    hgobj gobj_tranger;
    hgobj gobj_treedb;
    json_t *tranger;
    BOOL master;

    json_t *jn_validations; // Set of keys with their checker
    jwk_set_t *jwks;        // Set of jwk
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

    helper_quote2doublequote(treedb_schema_authzs);

    /*---------------------------*
     *      OAuth
     *---------------------------*/
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

    jwt_set_alloc(
        malloc_func,
        free_func
    );

#if defined(__linux__)
#if defined(CONFIG_HAVE_OPENSSL)
    OpenSSL_add_all_digests();
    // #elif defined(CONFIG_HAVE_MBEDTLS)
#else
#error "No crypto library defined"
#endif
#endif

    /*--------------------------------*
     *      Tranger database
     *--------------------------------*/
    const char *path = gobj_read_str_attr(gobj, "tranger_path");
    const char *authz_service = gobj_read_str_attr(gobj, "authz_service");
    const char *authz_tenant = gobj_read_str_attr(gobj, "authz_tenant");
    BOOL master = gobj_read_bool_attr(gobj, "master");

    if(empty_string(path)) {
        /*--------------------------------------------*
         *  Without path, it must be master (or not)
         *  Set the path
         *--------------------------------------------*/
        char path_[PATH_MAX];
        if(empty_string(authz_service)) {
            authz_service = gobj_yuno_role();
        }
        yuneta_realm_store_dir(
            path_,
            sizeof(path_),
            authz_service,
            gobj_yuno_realm_owner(),
            gobj_yuno_realm_id(),
            authz_tenant,
            "authzs",
            master?TRUE:FALSE
        );
        gobj_write_str_attr(gobj, "tranger_path", path_);
        path = gobj_read_str_attr(gobj, "tranger_path");

    } else {
        /*------------------------------------*
         *  With path, can be master or not
         *------------------------------------*/
    }

    /*--------------------------------------------------------*
     *  If path not exist check then authz only local access
     *--------------------------------------------------------*/
    if(!is_directory(path)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "No authz db, authz only to local access",
            NULL
        );
        return;
    }

    /*
     *  Create validators
     */
    create_jwt_validations(gobj);

    /*
     *  Check schema, exit if fails
     */
    json_t *jn_treedb_schema = legalstring2json(treedb_schema_authzs, TRUE);
    if(parse_schema(jn_treedb_schema)<0) {
        /*
         *  Exit if schema fails
         */
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP_ERROR,
            "msg",          "%s", "Parse schema fails",
            NULL
        );
        exit(-1);
    }

    /*------------------------------------*
     *  Open treedb
     *------------------------------------*/
    json_t *kw_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path,
        "filename_mask", "%Y",
        "master", master,
        "on_critical_error", (int)(LOG_OPT_EXIT_ZERO)
    );
    priv->gobj_tranger = gobj_create_service(
        "tranger_authz",
        C_TRANGER,
        kw_tranger,
        gobj
    );
    priv->tranger = gobj_read_pointer_attr(priv->gobj_tranger, "tranger");

    /*----------------------*
     *  Create Treedb
     *----------------------*/
    const char *treedb_name = "treedb_authzs"; // HACK hardcoded service name
    json_t *kw_resource = json_pack("{s:I, s:s, s:o, s:i}",
        "tranger", (json_int_t)(uintptr_t)priv->tranger,
        "treedb_name", treedb_name,
        "treedb_schema", jn_treedb_schema,
        "exit_on_error", LOG_OPT_EXIT_ZERO
    );

    priv->gobj_treedb = gobj_create_service(
        treedb_name,
        C_NODE,
        kw_resource,
        gobj
    );

    /*
     *  HACK pipe inheritance
     */
    gobj_set_bottom_gobj(priv->gobj_treedb, priv->gobj_tranger);
    gobj_set_bottom_gobj(gobj, priv->gobj_treedb);

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    } else if(gobj_is_pure_child(gobj)) {
        subscriber = gobj_parent(gobj);
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(max_sessions_per_user, gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(max_sessions_per_user,       gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(master,                    gobj_read_bool_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    destroy_jwt_validations(gobj);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gobj_tranger && !gobj_is_running(priv->gobj_tranger)) {
        gobj_start(priv->gobj_tranger);
    }
    if(priv->gobj_treedb && !gobj_is_running(priv->gobj_treedb)) {
        gobj_start(priv->gobj_treedb);
    }

    if(priv->gobj_treedb &&
        gobj_topic_size(priv->gobj_treedb, "roles", "")==0 &&
        gobj_topic_size(priv->gobj_treedb, "users", "")==0
    ) {
        /*------------------------------------*
         *  Empty treedb? initialize treedb
         *-----------------------------------*/
        json_t *initial_load = gobj_read_json_attr(gobj, "initial_load");
        time_t t;
        time(&t);
        const char *topic_name;
        json_t *topic_records;
        json_object_foreach(initial_load, topic_name, topic_records) {
            int idx; json_t *record;
            json_array_foreach(topic_records, idx, record) {
                json_object_set_new(record, "time", json_integer((json_int_t)t));

                json_t *kw_update_node = json_pack("{s:s, s:O, s:{s:b, s:b}}",
                    "topic_name", topic_name,
                    "record", record,
                    "options",
                        "create", 1,
                        "autolink", 1
                );
                gobj_send_event(
                    priv->gobj_treedb,
                    EV_TREEDB_UPDATE_NODE,
                    kw_update_node,
                    gobj
                );
            }
        }
    }

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gobj_treedb) {
        gobj_stop(priv->gobj_treedb);
    }
    if(priv->gobj_tranger) {
        gobj_stop(priv->gobj_tranger);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method mt_authenticate
    Ejemplo keycloak:  {
            "acr": "1",
            "allowed-origins": [],
            "aud": ["realm-management", "account"],
            "azp": "yunetacontrol",
            "email": "",
            "email_verified": TRUE,
            "exp": 1666336696,
            "family_name": "",
            "given_name": "",
            "iat": 1666336576,
            "iss": "https://localhost:8641/auth/realms/kkk",
            "jti": "96b65a56c",
            "locale": "en",
            "name": "",
            "preferred_username": "xxx@xx.com",
            "realm_access": {},
            "resource_access": {},
            "scope": "profile email",
            "session_state": "aa4fb7ce-...",
            "sid": "aa4fb7ce-...",
            "sub": "0a1e5c27-...",
            "typ": "Bearer"
        }
    Ejemplo de jwt dado por google  {
            "aud": "apps.googleusercontent.com",
            "azp": "apps.googleusercontent.com",
            "email": "xxx@gmail.com",
            "email_verified": TRUE,
            "exp": 1666341427,
            "given_name": "ZZZ",
            "iat": 1666337827,
            "iss": "https://accounts.google.com",
            "jti": "b2a",
            "name": "Xxx",
            "nbf": 1337527,
            "picture": "",
            "sub": "1094087..."
        }
 ***************************************************************************/
PRIVATE json_t *mt_authenticate(hgobj gobj, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *jwt = kw_get_str(gobj, kw, "jwt", NULL, 0);
    json_t *jwt_payload = NULL;
    char *comment = "";
    BOOL yuneta_by_local_ip = FALSE;
    const char *username = kw_get_str(gobj, kw, "username", "", 0);
    const char *password = kw_get_str(gobj, kw, "password", "", 0);
    const char *peername;
    if(gobj_has_bottom_attr(src, "peername")) {
        peername = gobj_read_str_attr(src, "peername");
    } else {
        peername = kw_get_str(gobj, kw, "peername", "", 0);
    }

    /*-----------------------------*
     *  peername is required
     *-----------------------------*/
    if(empty_string(peername)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "Destination service not found",
            "user",         "%s", username,
            NULL
        );
        json_t *jn_resp = json_pack("{s:i, s:s, s:s}",
            "result", -1,
            "comment", "Peername is required",
            "user",  username
        );
        KW_DECREF(kw)
        return jn_resp;
    }

    /*-----------------------------*
     *  Get destination service
     *-----------------------------*/
    const char *dst_service = kw_get_str(gobj,
        kw,
        "__md_iev__`ievent_gate_stack`0`dst_service",
        kw_get_str(gobj, kw, "dst_service", "", 0),
        0
    );
    if(!gobj_find_service(dst_service, FALSE)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "Destination service not found",
            "user",         "%s", username,
            "service",      "%s", dst_service,
            NULL
        );
        json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s}",
            "result", -1,
            "comment", "Destination service not found",
            "user",  username,
            "service", dst_service
        );
        KW_DECREF(kw)
        return jn_resp;
    }

    if(is_ip_denied(peername)) {
        /*
         *  IP not authorized
         */
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "Ip denied",
            "user",         "%s", username,
            "service",      "%s", dst_service,
            NULL
        );
        json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s}",
            "result", -1,
            "comment", "Ip denied",
            "user",  username,
            "service", dst_service
        );
        KW_DECREF(kw)
        return jn_resp;
    }

    if(empty_string(jwt)) {
        /*-----------------------------------------------*
         *  Without JWT, check user password,yuneta,ip
         *-----------------------------------------------*/
        do {
            if(!empty_string(password) && priv->gobj_treedb) {
                /*-----------------------------*
                 *      User with password
                 *-----------------------------*/
                int authorization = check_password(gobj, username, password);
                if(authorization < 0) {
                    /*
                     *  Only localhost is allowed without jwt
                     */
                    json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s}",
                        "result", -1,
                        "comment", "Wrong user/pwd",
                        "user",  username,
                        "service", dst_service
                    );
                    KW_DECREF(kw)
                    return jn_resp;
                }

                comment = "User authenticated by password";
                break;
            }

            if(strcmp(username, "yuneta")!=0) {
                /*
                 *  Only yuneta is allowed without jwt/passw
                 */
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_AUTH,
                    "msg",          "%s", "Without JWT/passw only yuneta is allowed",
                    "user",         "%s", username,
                    "service",      "%s", dst_service,
                    NULL
                );
                json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s}",
                    "result", -1,
                    "comment", "Without JWT/passw only yuneta is allowed",
                    "user",  username,
                    "service", dst_service
                );
                KW_DECREF(kw)
                return jn_resp;
            }

            if(is_ip_allowed(peername)) {
                /*
                 *  IP autorizada sin user/passw, usa logged user
                 */
                comment = "User authenticated by Ip allowed";
                break;
            }

            const char *localhost = "127.0.0.";
            if(strncmp(peername, localhost, strlen(localhost))!=0) {
                /*
                 *  Only localhost is allowed without jwt
                 */
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_AUTH,
                    "msg",          "%s", "Without JWT/passw only localhost is allowed",
                    "user",         "%s", username,
                    "service",      "%s", dst_service,
                    NULL
                );
                json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s}",
                    "result", -1,
                    "comment", "Without JWT/passw only localhost is allowed",
                    "user",  username,
                    "service", dst_service
                );
                KW_DECREF(kw)
                return jn_resp;
            }

            yuneta_by_local_ip = TRUE;

            /*
             *  If local ip, is yuneta, and is not treedb, let it.
             */
            comment = "User yuneta authenticated by local ip";
            break;

        } while(0);

    } else {
        /*-------------------------------*
         *      HERE user with JWT
         *-------------------------------*/
        const char *status = NULL;

        if(!verify_token(gobj, jwt, &jwt_payload, &status)) {
            char temp[512];
            snprintf(temp, sizeof(temp), "%s", status);
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_AUTH,
                "msg",          "%s", "Authentication failed: Invalid token",
                "status",       "%s", temp,
                "user",         "%s", username,
                "service",      "%s", dst_service,
                "jwt",          "%s", jwt,
                NULL
            );
            json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s}",
                "result", -1,
                "comment", "Authentication failed: Invalid token",
                "username", username,
                "service", dst_service
            );
            JSON_DECREF(jwt_payload);
            KW_DECREF(kw)
            return jn_resp;
        }

        /*-------------------------------------------------*
         *  Get username and validate against our system
         *-------------------------------------------------*/
        username = kw_get_str(gobj, jwt_payload, "email", "", KW_REQUIRED);
        BOOL email_verified = kw_get_bool(gobj, jwt_payload, "email_verified", FALSE, KW_REQUIRED);
        if(!email_verified) {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_AUTH,
                "msg",          "%s", "Email not verified",
                "user",         "%s", username,
                "service",      "%s", dst_service,
                NULL
            );
            json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s}",
                "result", -1,
                "comment", "Email not verified",
                "username", username,
                "service", dst_service
            );
            JSON_DECREF(jwt_payload);
            KW_DECREF(kw)
            return jn_resp;
        }
        if(!strchr(username, '@')) {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_AUTH,
                "msg",          "%s", "Username must be an email address",
                "user",         "%s", username,
                "service",      "%s", dst_service,
                NULL
            );
            json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s}",
                "result", -1,
                "comment", "Username must be an email address",
                "username", username,
                "service", dst_service
            );
            JSON_DECREF(jwt_payload);
            KW_DECREF(kw)
            return jn_resp;
        }

        comment = "User authenticated by jwt";
    }

    /*----------------------------------------------------------*
     *  HACK save username, jwt_payload in src (IEvent_srv)
     *----------------------------------------------------------*/
    if(gobj_has_bottom_attr(src, "__username__")) {
        gobj_write_str_attr(src, "__username__", username);
    }
    if(gobj_has_bottom_attr(src, "jwt_payload")) {
        gobj_write_json_attr(src, "jwt_payload", jwt_payload?jwt_payload:json_null());
    }

    /*------------------------------*
     *  Let it if no treedb
     *------------------------------*/
    if(!priv->gobj_treedb) {
        json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s}",
            "result", 0,
            "comment", comment,
            "username", username,
            "dst_service", dst_service
        );
        JSON_DECREF(jwt_payload);
        KW_DECREF(kw)
        return jn_resp;
    }

    /*------------------------------*
     *      yuneta
     *------------------------------*/
    if(yuneta_by_local_ip) {
        // TODO add the services_roles???
        json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s}",
            "result", 0,
            "comment", comment,
            "username", username,
            "dst_service", dst_service
        );
        JSON_DECREF(jwt_payload);
        KW_DECREF(kw)
        return jn_resp;
    }

    /*------------------------------*
     *      Check user
     *------------------------------*/
    json_t *user = gobj_get_node(
        priv->gobj_treedb,
        "users",
        json_pack("{s:s}",
            "id", username
        ),
        json_pack("{s:b}",
            "with_metadata", 1
        ),
        gobj
    );
    if(!user) {
        /*--------------------------------*
         *      Publish
         *--------------------------------*/
        gobj_publish_event(
            gobj,
            EV_AUTHZ_USER_NEW,
            json_pack("{s:s, s:s}",
                "username", username,
                "dst_service", dst_service
            )
        );

        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "User not exist",
            "user",         "%s", username,
            "service",      "%s", dst_service,
            NULL
        );
        json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s}",
            "result", -1,
            "comment", "User not exist",
            "username", username,
            "dst_service", dst_service
        );
        JSON_DECREF(jwt_payload);
        KW_DECREF(kw)
        return jn_resp;
    }

    /*------------------------------*
     *      Get user roles
     *------------------------------*/
    json_t *services_roles = get_user_roles(
        gobj,
        gobj_yuno_realm_id(),
        dst_service,
        username,
        kw
    );

print_json2("XXX services_roles", services_roles); // TODO TEST

    if(!kw_has_key(services_roles, dst_service)) {
        /*
         *  No authorized in dst service
         */
        gobj_log_warning(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_AUTH,
            "msg",              "%s", "User has not authz in service",
            "comment",          "%s", comment,
            "user",             "%s", username,
            "service",          "%s", dst_service,
            "services_roles",   "%j", services_roles?services_roles:json_null(),
            NULL
        );
        json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s}",
            "result", -1,
            "comment", "User has not authz in service",
            "username", username,
            "dst_service", dst_service
        );
        JSON_DECREF(user);
        JSON_DECREF(services_roles);
        JSON_DECREF(jwt_payload);
        KW_DECREF(kw)
        return jn_resp;
    }

    BOOL disabled = kw_get_bool(gobj, user, "disabled", 0, KW_REQUIRED);
    if(disabled) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "User disabled",
            "username",     "%s", username,
            "service",      "%s", dst_service,
            NULL
        );
        json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s}",
            "result", -1,
            "comment", "User disabled",
            "username", username,
            "dst_service", dst_service
        );
        JSON_DECREF(user);
        JSON_DECREF(services_roles);
        JSON_DECREF(jwt_payload);
        KW_DECREF(kw)
        return jn_resp;
    }

    /*------------------------------*
     *      Get session id
     *------------------------------*/
    char uuid[60];
    const char *session_id;

    if(jwt_payload) {
        // WARNING "session_state" is from keycloak!!!
        session_id = kw_get_str(
            gobj,
            jwt_payload,
            "sid",
            kw_get_str(gobj, jwt_payload, "session_state", "", 0),
            0
        );
    } else {
        create_random_uuid(uuid, sizeof(uuid));
        session_id = uuid;
    }

    /*--------------------------------------------*
     *  Get sessions, check max sessions allowed
     *--------------------------------------------*/
    json_int_t max_sessions = kw_get_int(
        gobj,
        user,
        "max_sessions",
        0,
        KW_REQUIRED
    );
    if(max_sessions < 0) {
        max_sessions = priv->max_sessions_per_user;
    }

    json_t *sessions = kw_get_dict(gobj, user, "__sessions", 0, KW_REQUIRED);
    if(!sessions) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH_ERROR,
            "msg",          "%s", "__sessions NULL",
            "username",     "%s", username,
            "service",      "%s", dst_service,
            NULL
        );
    }
    json_t *session;
    void *n; const char *k;
    json_object_foreach_safe(sessions, n, k, session) {
        if(json_object_size(sessions) < max_sessions) {
            break;
        }
        /*-------------------------------*
         *  Check max sessions allowed
         *  Drop the old sessions
         *-------------------------------*/
        hgobj prev_channel_gobj = (hgobj)(uintptr_t)kw_get_int(
            gobj, session, "channel_gobj", 0, KW_REQUIRED
        );
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "Drop session, max sessions reached",
            "username",     "%s", username,
            "service",      "%s", dst_service,
            NULL
        );
        gobj_send_event(prev_channel_gobj, EV_DROP, 0, gobj);
        json_object_del(sessions, k);
    }

    /*-------------------------------*
     *      Save session
     *-------------------------------*/
    session = json_pack("{s:s, s:I}",
        "id", session_id,
        "channel_gobj", (json_int_t)(uintptr_t)src
    );
    json_object_set(sessions, session_id, session);

    user = gobj_update_node(
        priv->gobj_treedb,
        "users",
        user,
        json_pack("{s:b, s:b}",
            "volatil", 1,
            "with_metadata", 1
        ),
        src
    );

    /*------------------------------*
     *      Save user access
     *------------------------------*/
    add_user_login(gobj, username, jwt_payload, peername);

    /*------------------------------------*
     *  Subscribe to know close session
     *------------------------------------*/
    gobj_subscribe_event(src, EV_ON_CLOSE, 0, gobj);

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_AUTH,
        "msg",          "%s", "User authenticated",
        "username",     "%s", username,
        "service",      "%s", dst_service,
        "roles",        "%j", services_roles,
        NULL
    );

    json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:s, s:O}",
        "result", 0,
        "comment", "User authenticated",
        "username", username,
        "dst_service", dst_service,
        "services_roles", services_roles
    );
    if(jwt_payload) {
        json_object_set(jn_resp, "jwt_payload", jwt_payload);
    }

    /*--------------------------------*
     *      Publish
     *--------------------------------*/
    gobj_publish_event(
        gobj,
        EV_AUTHZ_USER_LOGIN,
        json_pack("{s:s, s:s, s:o, s:o, s:o, s:o}",
            "username", username,
            "dst_service", dst_service,
            "user", user,
            "session", session,
            "services_roles", services_roles,
            "jwt_payload", jwt_payload
        )
    );

    KW_DECREF(kw)
    return jn_resp;
}




                    /***************************
                     *      Commands
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw)
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
 *  List iss Issuers of OAuth2
 ***************************************************************************/
PRIVATE json_t *cmd_list_jwk(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jwks = gobj_read_json_attr(gobj, "jwks");
    json_t *jn_schema = json_desc_to_schema(jwk_desc);

    return msg_iev_build_response(
        gobj,
        0,
        0,
        jn_schema,
        json_incref(jwks),
        kw  // owned
    );
}

/***************************************************************************
 *  Add OAuth2 Issuer
 ***************************************************************************/
PRIVATE json_t *cmd_add_jwk(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *kid = kw_get_str(
        gobj,
        kw,
        "kid",
        kw_get_str(gobj, kw, "iss", "", 0),
        0
    );
    const char *n = kw_get_str(
        gobj,
        kw,
        "n",
        kw_get_str(gobj, kw, "pkey", "", 0),
        0
    );

    if(empty_string(kid)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What kid (iss)?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(n)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What n (pkey)?"),
            0,
            0,
            kw  // owned
        );
    }

    const char *description = kw_get_str(gobj, kw, "description", "", 0);
    const char *alg = kw_get_str(gobj, kw, "alg", "RS256", 0);
    const char *use = kw_get_str(gobj, kw, "use", "sig", 0);
    const char *kty = kw_get_str(gobj, kw, "kty", "RSA", 0);
    const char *e = kw_get_str(gobj, kw, "e", "AQAB", 0);
    const char *x5c = kw_get_str(gobj, kw, "x5c", "", 0);

    json_t *jwks = gobj_read_json_attr(gobj, "jwks");

    /*
     *  Check if the record already exists
     */
    json_t *jn_record = kwjr_get( // Return is NOT yours, unless use of KW_EXTRACT
        gobj,
        jwks,       // kw, NOT owned
        kid,        // id
        0,          // new_record, owned
        jwk_desc,   // json_desc
        NULL,       // idx pointer
        0           // flag
    );
    if(jn_record) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Kid (iss) '%s' already exists", kid),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Create new record
     */
    jn_record = create_json_record(gobj, jwk_desc);
    json_object_update_new(
        jn_record,
        json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
            "kid", kid,
            "use", use,
            "kty", kty,
            "alg", alg,
            "e", e,
            "description", description,
            "n", n,
            "x5c", x5c
        )
    );

    /*
     *  Add the new record
     */
    jn_record = kwjr_get(  // Return is NOT yours, unless use of KW_EXTRACT
        gobj,
        jwks,       // kw, NOT owned
        kid,        // id
        jn_record,  // new_record, owned
        jwk_desc,   // json_desc
        NULL,       // idx pointer
        KW_CREATE   // flag
    );

    /*
     *  Save new record in persistent attrs
     */
    gobj_save_persistent_attrs(gobj, json_string("jwks"));

    /*
     *  Create validation key
     */
    json_t *jn_jwk = create_json_record(gobj, jwk_desc);
    json_object_update_new(jn_jwk, json_deep_copy(jn_record));

    json_t *jn_comment = NULL;
    int status = create_validation_key(gobj, jn_jwk);
    if(status != 0) {
        jn_comment = json_sprintf("Cannot create validation key");
    }

    return msg_iev_build_response(
        gobj,
        status,
        jn_comment,
        json_desc_to_schema(jwk_desc),
        json_incref(jn_record),
        kw  // owned
    );
}

/***************************************************************************
 *  Remove OAuth2 Issuer
 ***************************************************************************/
PRIVATE json_t *cmd_remove_jwk(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *kid = kw_get_str(
        gobj,
        kw,
        "kid",
        kw_get_str(gobj, kw, "iss", "", 0),
        0
    );
    if(empty_string(kid)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What kid (iss)?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *jwks = gobj_read_json_attr(gobj, "jwks");

    /*
     *  Check if the record already exists
     */
    json_t *jn_record = kwjr_get( // Return is NOT yours, unless use of KW_EXTRACT
        gobj,
        jwks,       // kw, NOT owned
        kid,        // id
        0,          // new_record, owned
        jwk_desc,   // json_desc
        NULL,       // idx pointer
        0           // flag
    );
    if(!jn_record) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("kid (iss) '%s' NOT exist", kid),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Remove the record
     */
    jn_record = kwjr_get(  // Return is NOT yours, unless use of KW_EXTRACT
        gobj,
        jwks,       // kw, NOT owned
        kid,        // id
        0,          // new_record, owned
        jwk_desc,   // json_desc
        NULL,       // idx pointer
        KW_EXTRACT  // flag
    );

    /*
     *  Save new record in persistent attrs
     */
    gobj_save_persistent_attrs(gobj, json_string("jwks"));

    /*
     *  Destroy validation key
     */
    destroy_validation_key(gobj, kid);

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("kid (iss) '%s' deleted", kid),
        json_desc_to_schema(jwk_desc),
        jn_record, // owned
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
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_users(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *jn_filter = kw_get_dict(gobj, kw, "filter", 0, KW_EXTRACT);
    json_t *jn_users = gobj_list_nodes(
        priv->gobj_treedb,
        "users",
        jn_filter,
        json_pack("{s:b}",
            "with_metadata", 1
        ),
        gobj
    );

    return msg_iev_build_response(
        gobj,
        0,
        0,
        tranger2_list_topic_desc_cols(priv->tranger, "users"),
        jn_users,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_accesses(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *jn_filter = kw_get_dict(gobj, kw, "filter", 0, KW_EXTRACT);
    json_t *jn_users = gobj_list_nodes(
        priv->gobj_treedb,
        "users_accesses",
        jn_filter,
        json_pack("{s:b}",
            "with_metadata", 1
        ),
        gobj
    );

    return msg_iev_build_response(
        gobj,
        0,
        0,
        tranger2_list_topic_desc_cols(priv->tranger, "users_accesses"),
        jn_users,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_create_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*--------------------------*
     *      Get parameters
     *--------------------------*/
    const char *username = kw_get_str(gobj, kw, "username", "", 0);
    if(empty_string(username)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What username?"),
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------*
     *  Check if username exists
     *-----------------------------*/
    json_t *user = gobj_get_node(
        priv->gobj_treedb,
        "users",
        json_pack("{s:s}",
            "id", username
        ),
        0,
        gobj
    );
    if(user) {
        JSON_DECREF(user)
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("User already exists: %s", username),
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------*
     *      Has password?
     *-----------------------------*/
    const char *password = kw_get_str(gobj, kw, "password", "", 0);
    if(empty_string(password)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What password?"),
            0,
            0,
            kw  // owned
        );
    }

    int hashIterations = (int)kw_get_int(
        gobj,
        kw,
        "hashIterations",
        gobj_read_integer_attr(gobj, "hashIterations"),
        KW_WILD_NUMBER
    );
    const char *algorithm = kw_get_str(
        gobj,
        kw,
        "algorithm",
        gobj_read_str_attr(gobj, "algorithm"),
        0
    );
    json_t *credentials = hash_password(
        gobj,
        password,
        algorithm,
        hashIterations
    );
    if(!credentials) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Error creating credentials: %s", gobj_log_last_message()),
            0,
            0,
            kw  // owned
        );
    }
    json_object_set_new(kw, "credentials", credentials);

    gobj_send_event(gobj, EV_ADD_USER, json_incref(kw), src);

    user = gobj_get_node(
        priv->gobj_treedb,
        "users",
        json_pack("{s:s}", "id", username),
        json_pack("{s:b}",
            "with_metadata", 1
        ),
        gobj
    );
    if(!user) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Can't create user: %s", username),
            0,
            0,
            kw  // owned
        );
    } else {
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("User created or updated: %s", username),
            tranger2_list_topic_desc_cols(priv->tranger, "users"),
            user,
            kw  // owned
        );
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_enable_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *username = kw_get_str(gobj, kw, "username", "", 0);

    if(empty_string(username)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What username?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *user = gobj_get_node(
        priv->gobj_treedb,
        "users",
        json_pack("{s:s}", "id", username),
        json_pack("{s:b}",
            "with_metadata", 1
        ),
        gobj
    );
    if(!user) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("User not found: '%s'", username),
            0,
            0,
            kw  // owned
        );
    }

    json_object_set_new(user, "disabled", json_false());

    user = gobj_update_node(
        priv->gobj_treedb,
        "users",
        user,
        0,
        src
    );

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("User enabled: %s", username),
        tranger2_list_topic_desc_cols(priv->tranger, "users"),
        user,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_disable_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *username = kw_get_str(gobj, kw, "username", "", 0);

    if(empty_string(username)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What username?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *user = gobj_get_node(
        priv->gobj_treedb,
        "users",
        json_pack("{s:s}", "id", username),
        json_pack("{s:b}",
            "with_metadata", 1
        ),
        gobj
    );
    if(!user) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("User not found: '%s'", username),
            0,
            0,
            kw  // owned
        );
    }

    json_object_set_new(user, "disabled", json_true());

    user = gobj_update_node(
        priv->gobj_treedb,
        "users",
        user,
        0,
        src
    );
    gobj_send_event(gobj, EV_REJECT_USER, user, src);

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("User disabled: %s", username),
        tranger2_list_topic_desc_cols(priv->tranger, "users"),
        user,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_delete_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *username = kw_get_str(gobj, kw, "username", "", 0);

    if(empty_string(username)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What username?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *user = gobj_get_node(
        priv->gobj_treedb,
        "users",
        json_pack("{s:s}", "id", username),
        json_pack("{s:b}",
            "with_metadata", 1
        ),
        gobj
    );
    if(!user) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("User not found: '%s'", username),
            0,
            0,
            kw  // owned
        );
    }

    gobj_send_event(gobj, EV_REJECT_USER, user, src);

    // TODO force to delete links?
    int ret = gobj_delete_node(
        priv->gobj_treedb,
        "users",
        user,
        0,
        gobj
    );

    return msg_iev_build_response(
        gobj,
        ret,
        json_sprintf("User deleted: %s", username),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_check_user_passw(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*--------------------------*
     *      Get parameters
     *--------------------------*/
    const char *username = kw_get_str(gobj, kw, "username", "", 0);
    const char *password = kw_get_str(gobj, kw, "password", "", 0);
    if(empty_string(username)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What username?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(password)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What password?"),
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------*
     *  Get username
     *-----------------------------*/
    json_t *user = gobj_get_node(
        priv->gobj_treedb,
        "users",
        json_pack("{s:s}",
            "id", username
        ),
        0,
        src
    );
    if(!user) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("User not exist: %s", username),
            0,
            0,
            kw  // owned
        );
    }

    int authorization = check_password(gobj, username, password);

    JSON_DECREF(user)
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Password match: %s", authorization==0?"Yes":"No"),
        0,
        0, // owned
        kw  // owned
    );

}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_set_user_passw(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*--------------------------*
     *      Get parameters
     *--------------------------*/
    const char *username = kw_get_str(gobj, kw, "username", "", 0);
    const char *password = kw_get_str(gobj, kw, "password", "", 0);
    if(empty_string(username)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What username?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(password)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What password?"),
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------*
     *  Get username
     *-----------------------------*/
    json_t *user = gobj_get_node(
        priv->gobj_treedb,
        "users",
        json_pack("{s:s}",
            "id", username
        ),
        0,
        src
    );
    if(!user) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("User not exist: %s", username),
            0,
            0,
            kw  // owned
        );
    }

    int hashIterations = (int)kw_get_int(
        gobj,
        kw,
        "hashIterations",
        gobj_read_integer_attr(gobj, "hashIterations"),
        KW_WILD_NUMBER
    );
    const char *algorithm = kw_get_str(
        gobj,
        kw,
        "algorithm",
        gobj_read_str_attr(gobj, "algorithm"),
        0
    );

    /*-----------------------------*
     *      Update user
     *-----------------------------*/
    json_t *credentials = hash_password(
        gobj,
        password,
        algorithm,
        hashIterations
    );
    if(!credentials) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Error creating credentials: %s", gobj_log_last_message()),
            0,
            0,
            kw  // owned
        );
    }
    JSON_DECREF(user)

    user = gobj_update_node(
        priv->gobj_treedb,
        "users",
        json_pack("{s:s, s:o}",
            "id", username,
            "credentials", credentials
        ),
        0,
        gobj
    );
    if(!user) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Cannot update user password: %s", gobj_log_last_message()),
            0,
            0,
            kw  // owned
        );
    }
    JSON_DECREF(user)

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Updated user password: %s", username),
        0,
        0, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_roles(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *jn_filter = kw_get_dict(gobj, kw, "filter", 0, KW_EXTRACT);
    json_t *jn_roles = gobj_list_nodes(
        priv->gobj_treedb,
        "roles",
        jn_filter,
        json_pack("{s:b}",
            "with_metadata", 1
        ),
        gobj
    );

    json_t *jn_data = jn_roles;

    return msg_iev_build_response(
        gobj,
        0,
        0,
        tranger2_list_topic_desc_cols(priv->tranger, "roles"),
        jn_data,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_user_roles(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *username = kw_get_str(gobj, kw, "username", "", 0);

    if(empty_string(username)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What username?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *user = gobj_get_node(
        priv->gobj_treedb,
        "users",
        json_pack("{s:s}", "id", username),
        json_pack("{s:b}",
            "with_metadata", 1
        ),
        gobj
    );
    if(!user) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("User not found: '%s'", username),
            0,
            0,
            kw  // owned
        );
    }

    json_t *roles = gobj_node_parents(
        priv->gobj_treedb,
        "users", // topic_name
        json_pack("{s:s}",
            "id", username
        ),
        "roles", // link
        json_pack("{s:b, s:b}",
            "refs", 1,
            "with_metadata", 1
        ),
        gobj
    );

    JSON_DECREF(user)

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        roles,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_user_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *username = kw_get_str(gobj, kw, "username", "", 0);

    if(empty_string(username)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What username?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *user = gobj_get_node(
        priv->gobj_treedb,
        "users",
        json_pack("{s:s}", "id", username),
        json_pack("{s:b}",
            "with_metadata", 1
        ),
        gobj
    );
    if(!user) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("User not found: '%s'", username),
            0,
            0,
            kw  // owned
        );
    }

    json_t *services_roles = json_array();

    json_t *roles_refs = gobj_node_parents(
        priv->gobj_treedb,
        "users", // topic_name
        json_pack("{s:s}",
            "id", username
        ),
        "roles", // link
        json_pack("{s:b, s:b}",
            "list_dict", 1,
            "with_metadata", 1
        ),
        gobj
    );

    int idx; json_t *role_ref;
    json_array_foreach(roles_refs, idx, role_ref) {
        json_t *role = gobj_get_node(
            priv->gobj_treedb,
            "roles", // topic_name
            json_incref(role_ref),
            json_pack("{s:b, s:b}",
                "list_dict", 1,
                "with_metadata", 1
            ),
            gobj
        );

        json_array_append(services_roles, role);

        json_t *tree_roles = gobj_node_children(
            priv->gobj_treedb,
            "roles", // topic_name
            role,    // 'id' and pkey2s fields are used to find the node
            "roles",
            json_pack("{s:b}", // filter to children tree
                "disabled", 0
            ),
            json_pack("{s:b, s:b, s:b}",
                "list_dict", 1,
                "with_metadata", 1,
                "recursive", 1
            ),
            gobj
        );

        json_t *child;
        int idx3;
        json_array_foreach(tree_roles, idx3, child) {
            json_array_append_new(services_roles, json_incref(child));
        }
        json_decref(tree_roles);
    }
    json_decref(roles_refs);
    JSON_DECREF(user)

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        services_roles,
        kw  // owned
    );
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Create validations for public keys
 ***************************************************************************/
PRIVATE int create_jwt_validations(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jwks = gobj_read_json_attr(gobj, "jwks");

    priv->jwks = jwks_create(NULL);
    priv->jn_validations = json_array();

    int idx; json_t *jn_record;
    json_array_foreach(jwks, idx, jn_record) {
        json_t *jn_jwk = create_json_record(gobj, jwk_desc);
        json_object_update_new(jn_jwk, json_deep_copy(jn_record));
        create_validation_key(gobj, jn_jwk);
    }

    return 0;
}

/***************************************************************************
 *  Destroy validations
 ***************************************************************************/
PRIVATE int destroy_jwt_validations(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int idx; json_t *jn_validation;
    json_array_foreach(priv->jn_validations, idx, jn_validation) {
        jwt_checker_t *jwt_checker = (jwt_checker_t *)(uintptr_t)kw_get_int(
            gobj,
            jn_validation,
            "jwt_checker",
            0,
            KW_REQUIRED
        );
        jwt_checker_free(jwt_checker);

    }
    JSON_DECREF(priv->jn_validations)

    jwks_free(priv->jwks);
    priv->jwks = NULL;

    return 0;
}

/***************************************************************************
 *  jn_jwt is duplicate json of a entry in jwks
 ***************************************************************************/
PRIVATE int create_validation_key(
    hgobj gobj,
    json_t *jn_jwk // owned
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *kid = kw_get_str(gobj, jn_jwk, "kid", "", KW_REQUIRED);
    const char *algorithm = kw_get_str(gobj, jn_jwk, "alg", "", KW_REQUIRED);

    /*
     *  Convert Encryption algorithm string to enum
     */
    jwt_alg_t alg = jwt_str_alg(algorithm);
    if(alg == JWT_ALG_INVAL) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_CONFIGURATION_ERROR,
            "msg",              "%s", "JWT Algorithm UNKNOWN",
            "msg",              "%s", "jwt_str_alg() FAILED",
            "kid",              "%s", kid,
            "algorithm",        "%s", algorithm,
            NULL
        );
        JSON_DECREF(jn_jwk)
        return -1;
    }

    /*
     *  Create the jwk_item
     */
    jwk_item_t *jwk_item = jwk_process_one(priv->jwks, jn_jwk);
    if(!jwk_item) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL_ERROR,
            "msg",              "%s", "jwk_process_one() FAILED",
            "kid",              "%s", kid,
            "algorithm",        "%s", algorithm,
            NULL
        );
        JSON_DECREF(jn_jwk)
        return -1;
    }
    jwks_item_add(priv->jwks, jwk_item);

    /*
     *  Create the jwk_checker
     */
    jwt_checker_t *jwt_checker = jwt_checker_new();
    if(!jwt_checker) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL_ERROR,
            "msg",              "%s", "jwk_process_one() FAILED",
            "kid",              "%s", kid,
            "algorithm",        "%s", algorithm,
            NULL
        );
        jwks_item_free2(priv->jwks, jwk_item);
        JSON_DECREF(jn_jwk)

        return -1;
    }

    if(jwt_checker_setkey(jwt_checker, alg, jwk_item)!=0) {
        const char *serror = jwt_checker_error_msg(jwt_checker);
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL_ERROR,
            "msg",              "%s", "jwt_checker_setkey() FAILED",
            "serror",           "%s", serror,
            "kid",              "%s", kid,
            "algorithm",        "%s", algorithm,
            NULL
        );
        jwks_item_free2(priv->jwks, jwk_item);
        jwt_checker_free(jwt_checker);
        JSON_DECREF(jn_jwk)
        return -1;
    }

    jwt_checker_time_leeway(jwt_checker, JWT_CLAIM_NBF, 0);
    jwt_checker_time_leeway(jwt_checker, JWT_CLAIM_EXP, 0);
    if(!empty_string(kid) && strcmp(kid, "*")!=0) {
        jwt_checker_claim_set(jwt_checker, JWT_CLAIM_ISS, kid);
    }
    if(jwt_checker_error(jwt_checker) != 0) {
        const char *serror = jwt_checker_error_msg(jwt_checker);
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL_ERROR,
            "msg",              "%s", "jwt_checker_* FAILED",
            "serror",           "%s", serror,
            "kid",              "%s", kid,
            "algorithm",        "%s", algorithm,
            NULL
        );
        jwt_checker_free(jwt_checker);
        JSON_DECREF(jn_jwk)
        return -1;
    }

    json_object_set_new(jn_jwk, "jwt_checker", json_integer((json_int_t)jwt_checker));
    json_array_append_new(priv->jn_validations, jn_jwk);

    return 0;
}

/***************************************************************************
 *  Destroy validations
 ***************************************************************************/
PRIVATE int destroy_validation_key(
    hgobj gobj,
    const char *kid
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Delete validation
     */
    json_t *jn_jwt = kwjr_get(  // Return is NOT yours, unless use of KW_EXTRACT
        gobj,
        priv->jn_validations,   // kw, NOT owned
        kid,                    // id
        0,                      // new_record, owned
        jwk_desc,               // json_desc
        NULL,                   // idx pointer
        KW_EXTRACT              // flag
    );

    jwt_checker_t *jwt_checker = (jwt_checker_t *)(uintptr_t)kw_get_int(
        gobj,
        jn_jwt,
        "jwt_checker",
        0,
        KW_REQUIRED
    );
    jwt_checker_free(jwt_checker);

    jwk_item_t *jwk_item = jwks_find_bykid(priv->jwks, kid);
    if(jwk_item) {
        jwks_item_free2(priv->jwks, jwk_item);
    } else {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_CONFIGURATION_ERROR,
            "msg",              "%s", "kid NOT FOUND",
            "kid",              "%s", kid,
            NULL
        );
    }

    JSON_DECREF(jn_jwt)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL verify_token(
    hgobj gobj,
    const char *token,
    json_t **jwt_payload_,
    const char **status
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    BOOL validated = FALSE;
    *jwt_payload_ = NULL;
    *status = "No OAuth2 Issuer found";

    int idx; json_t *jn_validation;
    json_array_foreach(priv->jn_validations, idx, jn_validation) {
        const char *jn_validation_iss = kw_get_str(gobj, jn_validation, "kid", "", KW_REQUIRED);

        jwt_checker_t *jwt_checker = (jwt_checker_t *)(uintptr_t)kw_get_int(
            gobj, jn_validation, "jwt_checker", 0, KW_REQUIRED
        );

        json_t *payload = jwt_checker_verify2(jwt_checker, token);
        if(!payload) {
            continue;
        }

        const char *jn_jwt_iss = kw_get_str(gobj, payload, "iss", "", KW_REQUIRED);
        if(empty_string(jn_jwt_iss) || strcmp(jn_validation_iss, jn_jwt_iss)!=0) {
            JSON_DECREF(payload)
            continue;
        }

        *jwt_payload_ = payload;

        if(!jwt_checker->error) {
            validated = TRUE;
        }
        *status = jwt_checker_error_msg(jwt_checker);
        break;
    }

    return validated;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *identify_system_user(
    hgobj gobj,
    const char **username,
    BOOL include_groups,
    BOOL verbose
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *user = gobj_get_node(
        priv->gobj_treedb,
        "users",
        json_pack("{s:s, s:b}",
            "id", *username,
            "disabled", 0
        ),
        json_pack("{s:b}",
            "with_metadata", 1
        ),
        gobj
    );
    if(user) {
        return user;
    }

    if(include_groups) {
        /*-------------------------------*
         *  HACK user's group is valid
         *-------------------------------*/
        static gid_t groups[30]; // HACK to use outside
        int ngroups = sizeof(groups)/sizeof(groups[0]);
        getgrouplist(*username, 0, groups, &ngroups);
        for(int i=0; i<ngroups; i++) {
            struct group *gr = getgrgid(groups[i]);
            if(gr) {
                user = gobj_get_node(
                    priv->gobj_treedb,
                    "users",
                    json_pack("{s:s}", "id", gr->gr_name),
                    json_pack("{s:b}",
                        "with_metadata", 1
                    ),
                    gobj
                );
                if(user) {
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INFO,
                        "msg",          "%s", "Using groupname instead of username",
                        "username",     "%s", *username,
                        "groupname",    "%s", gr->gr_name,
                        NULL
                    );
                    *username = gr->gr_name;
                    return user;
                }
            }
        }
    }

    if(verbose) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "username not found in system",
            "username",     "%s", *username,
            NULL
        );
    }
    return 0; // username as user or group not found
}

/***************************************************************************
 *  Constant-time comparison
 *      Always compares all bytes, takes constant time.
 *      ✅ Yes — resistant to timing attacks.
 ***************************************************************************/
PRIVATE int secure_eq(const uint8_t *a, const uint8_t *b, size_t n)
{
    if(!a || !b) {
        return -1;
    }
    unsigned int diff = 0;
    for(size_t i = 0; i < n; i++) {
        diff |= (unsigned int)(a[i] ^ b[i]);
    }
    return diff == 0 ? 0 : -1;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int gen_salt(hgobj gobj, uint8_t *salt, size_t salt_len)
{
#if defined(__linux__)
#if defined(CONFIG_HAVE_OPENSSL)
    if(RAND_bytes(salt, (int)salt_len) != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "RAND_bytes() FAILED",
            NULL
        );
        return -1;
    }
#elif defined(CONFIG_HAVE_MBEDTLS)
#else
#error "No crypto library defined"
#endif
#endif
    return 0;
}

/***************************************************************************
 *  PBKDF2-HMAC with arbitrary digest
 *
 *  password    : NUL-terminated string
 *  salt        : salt bytes
 *  salt_len    : length of salt
 *  iterations  : cost factor (>=1)
 *  digest_name : e.g. "sha256", "sha3-512", "sm3"
 *  out_key     : output buffer
 *  out_len     : desired key length
 *
 *  Returns 0 on success, −1 on failure ***************************************************************************/
PRIVATE int pbkdf2_any(
    hgobj gobj,
    const char *password,
    const uint8_t *salt,
    size_t salt_len,
    unsigned int iterations,
    const char *digest_name,
    uint8_t *out_key,
    size_t out_len
) {
    int ret = 0;

#if defined(__linux__)
#if defined(CONFIG_HAVE_OPENSSL)

    EVP_MD *md = EVP_MD_fetch(NULL, digest_name, NULL);
    if(!md) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Unable to get openssl digest",
            "digest",       "%s", digest_name,
            NULL
        );
        return -1;
    }

    int md_size = EVP_MD_get_size(md);
    if(md_size <= 0) { /* Should not happen for HMAC-capable digests */
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "EVP_MD_get_size() failed",
            "digest",       "%s", digest_name,
            NULL
        );
        EVP_MD_free(md);
        return -1;
    }

    if(md_size > out_len) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "out_key size too small",
            "digest",       "%s", digest_name,
            "md_size",      "%d", (int)md_size,
            "out_len",      "%d", (int)out_len,
            NULL
        );
        EVP_MD_free(md);
        return -1;
    }

    if(PKCS5_PBKDF2_HMAC(
        password, (int)strlen(password),
        salt, (int)salt_len,
        (int)iterations, md,
        (int)md_size, out_key
    ) != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "PKCS5_PBKDF2_HMAC() failed",
            "digest",       "%s", digest_name,
            NULL
        );
        ret = -1;
    }

    EVP_MD_free(md);
    ret = md_size;

#elif defined(CONFIG_HAVE_MBEDTLS)
#else
#error "No crypto library defined"
#endif
#endif

    return ret;
}

/***************************************************************************
 *  Return

    "credentials" : [
        {
            "type": "password",
            "secretData": {
                "value": "???",
                "salt": "???=="
            },
            "credentialData" : {
                "hashIterations": 27500,
                "algorithm": "sha512",
                "additionalParameters": {
                }
            }
        }
    ]

 ***************************************************************************/
PRIVATE json_t *hash_password(
    hgobj gobj,
    const char *password,
    const char *digest,
    unsigned int iterations
)
{
    if(empty_string(digest)) {
        digest = "sha512";
    }
    if(iterations < 1) {
        iterations = 27500;
    }

    uint8_t salt[16];
    if(gen_salt(gobj, salt, sizeof(salt)) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "RAND_bytes() FAILED",
            NULL
        );
        return NULL;
    }

    uint8_t hash[EVP_MAX_MD_SIZE];

    int hash_len = pbkdf2_any(
        gobj,
        password,
        salt, sizeof(salt),
        iterations, digest,
        hash, sizeof(hash)
    );
    if(hash_len <= 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "pbkdf2_any() failed",
            "digest",       "%s", digest,
            NULL
        );
        return NULL;
    }

    gbuffer_t *gbuf_hash = gbuffer_string_to_base64((const char *)hash, hash_len);
    gbuffer_t *gbuf_salt = gbuffer_string_to_base64((const char *)salt, sizeof(salt));
    char *hash_b64 = gbuffer_cur_rd_pointer(gbuf_hash);
    char *salt_b64 = gbuffer_cur_rd_pointer(gbuf_salt);

    json_t *credential_list = json_array();
    json_t *credential = json_pack("{s:s, s:{s:s, s:s}, s:{s:I, s:s, s:{}}}",
        "type", "password",
        "secretData",
            "value", hash_b64,
            "salt", salt_b64,
        "credentialData",
            "hashIterations", (json_int_t)iterations,
            "algorithm", digest,
            "additionalParameters"
    );
    json_array_append_new(credential_list, credential);

    GBUFFER_DECREF(gbuf_hash);
    GBUFFER_DECREF(gbuf_salt);

    return credential_list;
}

/***************************************************************************
 *  Verify a PBKDF2-HMAC derived key matches the expected value
 *
 *  password     : NUL-terminated password string
 *  salt         : salt bytes
 *  salt_len     : length of salt
 *  iterations   : cost factor (>=1)
 *  digest_name  : e.g., "sha256", "sha3-512", "sm3"
 *  expected_dk  : expected derived key bytes
 *  expected_len : length of expected_dk (and of recomputed dk)
 *
 *  Returns 0 on match, -1 on error or mismatch
 ***************************************************************************/
PRIVATE int pbkdf2_verify_any(
    hgobj gobj,
    const char *password,
    const uint8_t *salt,
    size_t salt_len,
    unsigned int iterations,
    const char *digest,
    const uint8_t *expected_dk,
    size_t expected_len
)
{
    uint8_t hash[EVP_MAX_MD_SIZE];

    int hash_len = pbkdf2_any(
        gobj,
        password,
        salt, salt_len,
        iterations, digest,
        hash, sizeof(hash)
    );

    if(hash_len <= 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "pbkdf2_any() failed",
            "digest",       "%s", digest,
            NULL
        );
        return -1;
    }
    if(hash_len != expected_len) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "expected_len don't match",
            "digest",       "%s", digest,
            "hash_len",     "%d", (int)hash_len,
            "expected_len", "%d", (int)expected_len,
            NULL
        );
        return -1;
    }

    return secure_eq(hash, expected_dk, expected_len); /* 0 = match, -1 = mismatch/error */
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int match_hash(
    hgobj gobj,
    const char *password,
    const char *hash_b64,
    const char *salt_b64,
    const char *digest,
    json_int_t iterations
)
{
    gbuffer_t *gbuf_hash = gbuffer_base64_to_string((const char *)hash_b64, strlen(hash_b64));
    gbuffer_t *gbuf_salt = gbuffer_base64_to_string((const char *)salt_b64, strlen(salt_b64));
    uint8_t *hash = gbuffer_cur_rd_pointer(gbuf_hash);
    uint8_t *salt = gbuffer_cur_rd_pointer(gbuf_salt);
    size_t hash_len = gbuffer_leftbytes(gbuf_hash);
    size_t salt_len = gbuffer_leftbytes(gbuf_salt);

    int ret = pbkdf2_verify_any(
        gobj,
        password,
        salt, salt_len,
        iterations, digest,
        hash, hash_len
    );
    GBUFFER_DECREF(gbuf_hash);
    GBUFFER_DECREF(gbuf_salt);

    return ret;
}

/***************************************************************************
 *  Return -2 if username is not authorized
 *  Return 0 if password matches
 ***************************************************************************/
PRIVATE int check_password(
    hgobj gobj,
    const char *username,
    const char *password
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // TODO a client_id can have permitted usernames in blank

    if(empty_string(username)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH_ERROR,
            "msg",          "%s", "No username given to check password",
            NULL
        );
        return -2;
    }
    json_t *user = gobj_get_node(
        priv->gobj_treedb,
        "users",
        json_pack("{s:s}",
            "id", username
        ),
        json_pack("{s:b}",
            "show_hidden", 1
        ),
        gobj
    );
    if(!user) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH_ERROR,
            "msg",          "%s", "User not exist",
            "username",     "%s", username,
            NULL
        );
        return -2;
    }

    json_t *credentials = kw_get_list(gobj, user, "credentials", 0, KW_REQUIRED);

    int idx; json_t *credential;
    json_array_foreach(credentials, idx, credential) {
        const char *hash_saved = kw_get_str(
            gobj,
            credential,
            "secretData`value",
            "",
            KW_REQUIRED
        );
        const char *salt = kw_get_str(
            gobj,
            credential,
            "secretData`salt",
            "",
            KW_REQUIRED
        );
        json_int_t hashIterations = kw_get_int(
            gobj,
            credential,
            "credentialData`hashIterations",
            0,
            KW_REQUIRED
        );
        const char *algorithm = kw_get_str(
            gobj,
            credential,
            "credentialData`algorithm",
            "",
            KW_REQUIRED
        );

        if(match_hash(
            gobj,
            password,
            hash_saved,
            salt,
            algorithm,
            hashIterations
        )==0) {
            JSON_DECREF(user)
            return 0;
        }
    }

    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_AUTH_ERROR,
        "msg",          "%s", "User pwd not matched",
        "username",     "%s", username,
        NULL
    );

    JSON_DECREF(user)
    return -2;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *append_role(
    hgobj gobj,
    json_t *services_roles, // not owned
    json_t *role,       // not owned
    const char *dst_realm_id,
    const char *dst_service
)
{
    BOOL disabled = kw_get_bool(gobj, role, "disabled", 0, KW_REQUIRED|KW_WILD_NUMBER);
    if(!disabled) {
        const char *service = kw_get_str(gobj, role, "service", "", KW_REQUIRED);
        const char *realm_id = kw_get_str(gobj, role, "realm_id", "", KW_REQUIRED);
        if((strcmp(realm_id, dst_realm_id)==0 || strcmp(realm_id, "*")==0)) {
            if(strcmp(service, dst_service)==0 || strcmp(service, "*")==0
            ) {
                json_t *srv_roles = kw_get_list(
                    gobj,
                    services_roles,
                    dst_service,
                    json_array(),
                    KW_CREATE
                );
                json_array_append_new(
                    srv_roles,
                    json_string(kw_get_str(gobj, role, "id", "", KW_REQUIRED))
                );
            }
        }
    }

    return services_roles;
}

/***************************************************************************
 *
    Ejemplo de respuesta:

    "services_roles": {
        "treedb_controlcenter": [
            "manage-controlcenter",
            "owner"
        ],
        "treedb_authzs": [
            "manage-authzs",
            "write-authzs",
            "read-authzs",
            "owner"
        ]
    }

 ***************************************************************************/
PRIVATE json_t *get_user_roles(
    hgobj gobj,
    const char *dst_realm_id,
    const char *dst_service,
    const char *username,
    json_t *kw // not owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *services_roles = json_object();

    json_t *roles_refs = gobj_node_parents(
        priv->gobj_treedb,
        "users", // topic_name
        json_pack("{s:s}",
            "id", username
        ),
        "roles", // link
        json_pack("{s:b, s:b}",
            "list_dict", 1,
            "with_metadata", 1
        ),
        gobj
    );
    if(!roles_refs) {
        return services_roles;
    }

    json_t *required_services = kw_get_list(gobj, kw, "required_services", 0, 0);

    int idx; json_t *role_ref;
    json_array_foreach(roles_refs, idx, role_ref) {
        json_t *role = gobj_get_node(
            priv->gobj_treedb,
            "roles", // topic_name
            json_incref(role_ref),
            json_pack("{s:b, s:b}",
                "list_dict", 1,
                "with_metadata", 1
            ),
            gobj
        );
        if(kw_get_bool(gobj, role, "disabled", 0, KW_REQUIRED|KW_WILD_NUMBER)) {
            json_decref(role);
            continue;
        }

        append_role(
            gobj,
            services_roles,
            role,
            dst_realm_id,
            dst_service
        );

        int idx2; json_t *required_service;
        json_array_foreach(required_services, idx2, required_service) {
            const char *service = json_string_value(required_service);
            if(service) {
                append_role(
                    gobj,
                    services_roles,
                    role,
                    dst_realm_id,
                    service
                );
            }
        }

        json_t *tree_roles = gobj_node_children(
            priv->gobj_treedb,
            "roles", // topic_name
            json_incref(role),    // 'id' and pkey2s fields are used to find the node
            "roles",
            json_pack("{s:b}", // filter to children tree
                "disabled", 0
            ),
            json_pack("{s:b, s:b, s:b}",
                "list_dict", 1,
                "with_metadata", 1,
                "recursive", 1
            ),
            gobj
        );
        json_decref(role);

        json_t *child;
        int idx3;
        json_array_foreach(tree_roles, idx3, child) {
            append_role(
                gobj,
                services_roles,
                child,
                dst_realm_id,
                dst_service
            );
            int idx4;
            json_array_foreach(required_services, idx4, required_service) {
                const char *service = json_string_value(required_service);
                if(service) {
                    append_role(
                        gobj,
                        services_roles,
                        child,
                        dst_realm_id,
                        service
                    );
                }
            }
        }
        json_decref(tree_roles);
    }
    json_decref(roles_refs);

    return services_roles;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *append_permission(
    hgobj gobj,
    json_t *services_roles, // not owned
    json_t *role,       // not owned
    const char *dst_realm_id,
    const char *dst_service
)
{
    BOOL disabled = kw_get_bool(gobj, role, "disabled", 0, KW_REQUIRED|KW_WILD_NUMBER);
    if(!disabled) {
        const char *service = kw_get_str(gobj, role, "service", "", KW_REQUIRED);
        const char *realm_id = kw_get_str(gobj, role, "realm_id", "", KW_REQUIRED);
        if((strcmp(realm_id, dst_realm_id)==0 || strcmp(realm_id, "*")==0)) {
            if(strcmp(service, dst_service)==0 || strcmp(service, "*")==0
            ) {
                const char *permission = kw_get_str(gobj, role, "permission", "", KW_REQUIRED);
                BOOL deny = kw_get_bool(gobj, role, "deny", FALSE, KW_REQUIRED);
                if(!empty_string(permission)) {
                    json_object_set_new(
                        services_roles,
                        permission,
                        deny?json_false():json_true()
                    );
                }

                json_t *permissions = kw_get_list(gobj, role, "permissions", 0, KW_REQUIRED);
                int idx; json_t *jn_permission;
                json_array_foreach(permissions, idx, jn_permission) {
                    permission = kw_get_str(gobj, jn_permission, "permission", "", KW_REQUIRED);
                    deny = kw_get_bool(gobj, jn_permission, "deny", FALSE, KW_REQUIRED);
                    if(!empty_string(permission)) {
                        json_object_set_new(
                            services_roles,
                            permission,
                            deny?json_false():json_true()
                        );
                    }
                }
            }
        }
    }

    return services_roles;
}

/***************************************************************************
 *
    Ejemplo de respuesta:
 ***************************************************************************/
PRIVATE json_t *get_user_permissions(
    hgobj gobj,
    const char *dst_realm_id,
    const char *dst_service,
    const char *username,
    json_t *kw // not owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *services_roles = json_object();

    json_t *user = gobj_get_node(
        priv->gobj_treedb,
        "users", // topic_name
        json_pack("{s:s}",
            "id", username
        ),
        NULL,
        gobj
    );
    if(!user) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "User not found",
            "user",         "%s", username,
            NULL
        );
        return services_roles;
    }
    JSON_DECREF(user)

    json_t *roles_refs = gobj_node_parents(
        priv->gobj_treedb,
        "users", // topic_name
        json_pack("{s:s}",
            "id", username
        ),
        "roles", // link
        json_pack("{s:b, s:b}",
            "list_dict", 1,
            "with_metadata", 1
        ),
        gobj
    );
    if(!roles_refs) {
        return services_roles;
    }

    json_t *required_services = kw_get_list(gobj, kw, "required_services", 0, 0);

    int idx; json_t *role_ref;
    json_array_foreach(roles_refs, idx, role_ref) {
        json_t *role = gobj_get_node(
            priv->gobj_treedb,
            "roles", // topic_name
            json_incref(role_ref),
            json_pack("{s:b, s:b}",
                "list_dict", 1,
                "with_metadata", 1
            ),
            gobj
        );
        if(kw_get_bool(gobj, role, "disabled", 0, KW_REQUIRED|KW_WILD_NUMBER)) {
            json_decref(role);
            continue;
        }

        append_permission(
            gobj,
            services_roles,
            role,
            dst_realm_id,
            dst_service
        );

        int idx2; json_t *required_service;
        json_array_foreach(required_services, idx2, required_service) {
            const char *service = json_string_value(required_service);
            if(service) {
                append_role(
                    gobj,
                    services_roles,
                    role,
                    dst_realm_id,
                    service
                );
            }
        }

        json_t *tree_roles = gobj_node_children(
            priv->gobj_treedb,
            "roles", // topic_name
            json_incref(role),    // 'id' and pkey2s fields are used to find the node
            "roles",
            json_pack("{s:b}", // filter to children tree
                "disabled", 0
            ),
            json_pack("{s:b, s:b, s:b}",
                "list_dict", 1,
                "with_metadata", 1,
                "recursive", 1
            ),
            gobj
        );
        json_decref(role);

        json_t *child;
        int idx3;
        json_array_foreach(tree_roles, idx3, child) {
            append_permission(
                gobj,
                services_roles,
                child,
                dst_realm_id,
                dst_service
            );
            int idx4;
            json_array_foreach(required_services, idx4, required_service) {
                const char *service = json_string_value(required_service);
                if(service) {
                    append_permission(
                        gobj,
                        services_roles,
                        child,
                        dst_realm_id,
                        service
                    );
                }
            }
        }
        json_decref(tree_roles);
    }
    json_decref(roles_refs);

    return services_roles;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int add_user_login(
    hgobj gobj,
    const char *username,
    json_t *jwt_payload, // not owned
    const char *peername
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->master) {
        /*
         *  Crea user en users_accesses
         */
        json_t *accesso = json_pack("{s:s, s:s, s:I, s:s}",
            "id", username,
            "ev", "login",
            "tm", (json_int_t)time_in_seconds(),
            "ip", peername
        );
        if(jwt_payload) {
            json_object_set(accesso, "jwt_payload", jwt_payload);
        }

        json_decref(gobj_update_node(
            priv->gobj_treedb,
            "users_accesses",
            accesso, // owned
            json_pack("{s:b}",
                "create", 1
            ),
            gobj
        ));
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int add_user_logout(hgobj gobj, const char *username)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->master) {
        /*
         *  Crea user en users_accesses
         */
        json_t *access = json_pack("{s:s, s:s, s:I}",
            "id", username,
            "ev", "logout",
            "tm", (json_int_t)time_in_seconds()
        );

        json_decref(gobj_update_node(
            priv->gobj_treedb,
            "users_accesses",
            access, // owned
            json_pack("{s:b}",
                "create", 1
            ),
            gobj
        ));
    }

    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Identity_card off from
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*------------------------------*
    *      Get jwt info
    *------------------------------*/
    json_t *jwt_payload = gobj_read_json_attr(src, "jwt_payload");
    if(!jwt_payload) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "close without jwt_payload",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }

    if(gobj_is_shutdowning()) {
        KW_DECREF(kw)
        return 0;
    }

    /*--------------------------------------------*
     *  Add logout user
     *--------------------------------------------*/
    if(priv->gobj_treedb) { // Si han pasado a pause es 0
        const char *session_id = kw_get_str(
            gobj,
            jwt_payload,
            "sid",
            kw_get_str(gobj, jwt_payload, "session_state", "", 0),
            0
        );
        if(empty_string(session_id)) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_PARAMETER_ERROR,
                "msg",              "%s", "sid or session_state not found",
                "msg",              "%s", "jwt_str_alg() FAILED",
                NULL
            );
        }
        const char *username = kw_get_str(gobj, jwt_payload, "preferred_username", 0, KW_REQUIRED);

        json_t *user = gobj_get_node(
            priv->gobj_treedb,
            "users",
            json_pack("{s:s}", "id", username),
            json_pack("{s:b}",
                "with_metadata", 1
            ),
            gobj
        );
        if(!user) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "User not found",
                "username",     "%s", username,
                NULL
            );
        } else {
            json_t *sessions = kw_get_dict(gobj, user, "__sessions", 0, KW_REQUIRED);
            json_t *session = kw_get_dict(gobj, sessions, session_id, 0, KW_EXTRACT); // Remove session

            add_user_logout(gobj, username);

            gobj_publish_event(
                gobj,
                EV_AUTHZ_USER_LOGOUT,
                json_pack("{s:s, s:O, s:o}",
                    "username", username,
                    "user", user,
                    "session", session
                )
            );

            json_decref(gobj_update_node(
                priv->gobj_treedb,
                "users",
                user,
                json_pack("{s:b, s:b}",
                    "volatil", 1,
                    "with_metadata", 1
                ),
                src
            ));

            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_AUTH,
                "msg",          "%s", "Logout",
                "user",         "%s", username,
                NULL
            );
        }
    }

    gobj_unsubscribe_event(src, EV_ON_CLOSE, 0, gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Create or update a user
 ***************************************************************************/
PRIVATE int ac_create_user(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *username = kw_get_str(gobj, kw, "username", "", KW_REQUIRED);
    const char *role = kw_get_str(gobj, kw, "role", "", 0);
    BOOL disabled = kw_get_bool(gobj, kw, "disabled", 0, 0);
    json_t *credentials = kw_get_dict_value(gobj, kw, "credentials", 0, 0);
    json_t *properties = kw_get_dict_value(gobj, kw, "properties", 0, 0);

    time_t t;
    time(&t);

    json_t *user = gobj_get_node(
        priv->gobj_treedb,
        "users",
        json_pack("{s:s}",
            "id", username
        ),
        0,
        gobj
    );
    BOOL new_user = user?TRUE:FALSE;
    JSON_DECREF(user)

    if(empty_string(role)) {
        json_t *record = json_pack("{s:s, s:b}",
            "id", username,
            "disabled", disabled
        );

        if(new_user) {
            json_object_set_new(record, "time", json_integer((json_int_t)t));
        }
        if(credentials) {
            json_object_set(record, "credentials", credentials);
        }
        if(properties) {
            json_object_set(record, "properties", properties);
        }

        json_decref(gobj_update_node(
            priv->gobj_treedb,
            "users",
            record,
            json_pack("{s:b, s:b}",
                "create", 1,
                "autolink", 0
            ),
            src
        ));
    } else {
        json_t *record = json_pack("{s:s, s:s, s:b}",
            "id", username,
            "roles", role,
            "disabled", disabled
        );

        if(new_user) {
            json_object_set_new(record, "time", json_integer((json_int_t)t));
        }
        if(credentials) {
            json_object_set(record, "credentials", credentials);
        }
        if(properties) {
            json_object_set(record, "properties", properties);
        }

        json_decref(gobj_update_node(
            priv->gobj_treedb,
            "users",
            record,
            json_pack("{s:b, s:b}",
                "create", 1,
                "autolink", 1
            ),
            src
        ));
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_reject_user(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *username = kw_get_str(gobj, kw, "username", "", KW_REQUIRED);

    json_t *user = gobj_get_node(
        priv->gobj_treedb,
        "users",
        json_pack("{s:s}", "id", username),
        json_pack("{s:b}",
            "with_metadata", 1
        ),
        gobj
    );
    if(!user) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "User not found",
            "username",     "%s", username,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }


    if(kw_has_key(kw, "disabled")) {
        BOOL disabled = kw_get_bool(gobj, kw, "disabled", 0, 0);
        json_object_set_new(user, "disabled", disabled?json_true():json_false());
        user = gobj_update_node(
            priv->gobj_treedb,
            "users",
            user,
            json_pack("{s:b}",
                "with_metadata", 1
            ),
            src
        );
    }

    /*-----------------*
     *  Get sessions
     *-----------------*/
    json_t *sessions = kw_get_dict(gobj, user, "__sessions", 0, KW_REQUIRED);
    if(!sessions) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "__sessions NULL",
            NULL
        );
    }
    json_t *session;
    void *n; const char *k;
    int ret = 0;
    json_object_foreach_safe(sessions, n, k, session) {
        /*-------------------------------*
         *  Drop sessions
         *-------------------------------*/
        hgobj prev_channel_gobj = (hgobj)(uintptr_t)kw_get_int(gobj, session, "channel_gobj", 0, KW_REQUIRED);
        gobj_send_event(prev_channel_gobj, EV_DROP, 0, gobj);
        json_object_del(sessions, k);
        ret++;
    }

    json_decref(gobj_update_node(
        priv->gobj_treedb,
        "users",
        user,
        json_pack("{s:b, s:b}",
            "volatil", 1,
            "with_metadata", 1
        ),
        src
    ));

    KW_DECREF(kw)
    return ret;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_authenticate = mt_authenticate,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_AUTHZ);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_ADD_USER);
GOBJ_DEFINE_EVENT(EV_REJECT_USER);
GOBJ_DEFINE_EVENT(EV_AUTHZ_USER_LOGIN);
GOBJ_DEFINE_EVENT(EV_AUTHZ_USER_LOGOUT);
GOBJ_DEFINE_EVENT(EV_AUTHZ_USER_NEW);

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_idle[] = {
        {EV_ADD_USER,             ac_create_user,           0},
        {EV_REJECT_USER,          ac_reject_user,           0},
        {EV_ON_CLOSE,             ac_on_close,              0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    event_type_t event_types[] = { // HACK System gclass, not public events
        {EV_AUTHZ_USER_LOGIN,   EVF_OUTPUT_EVENT},
        {EV_AUTHZ_USER_LOGOUT,  EVF_OUTPUT_EVENT},
        {EV_AUTHZ_USER_NEW,     EVF_OUTPUT_EVENT},
        {EV_ADD_USER,           0},
        {EV_REJECT_USER,        0},
        {EV_ON_CLOSE,           0},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  //lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        command_table,  // command_table,
        s_user_trace_level,  // s_user_trace_level
        0   // gclass_flag
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_authz(void)
{
    return create_gclass(C_AUTHZ);
}


/***************************************************************************
   Check user authz
 ***************************************************************************/
PUBLIC BOOL authz_checker(hgobj gobj_to_check, const char *authz, json_t *kw, hgobj src)
{
    hgobj gobj = gobj_find_service_by_gclass(C_AUTHZ, TRUE);
    if(!gobj) {
        /*
         *  HACK if this function is called is because the authz system is configured in setup.
         *  If the service is not found then deny all.
         */
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "No gclass authz found",
            NULL
        );
        KW_DECREF(kw)
        return FALSE;
    }

    const char *__username__ = kw_get_str(gobj, kw, "__username__", 0, 0);
    if(empty_string(__username__)) {
        __username__ = gobj_read_str_attr(src, "__username__");
        if(empty_string(__username__)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "__username__ not found in kw nor src",
                "src",          "%s", gobj_full_name(src),
                NULL
            );
            KW_DECREF(kw)
            return FALSE;
        }
    }

    /*
     *  Check if the authz exists in gobj_to_check
     */
    json_t *jn_authz_desc = gobj_authz(gobj_to_check, authz);
    if(!jn_authz_desc) {
        // Error already logged
        KW_DECREF(kw)
        return FALSE;
    }

    json_t *user_authzs = get_user_permissions(
        gobj,
        gobj_yuno_realm_id(),       // dst_realm_id
        gobj_name(gobj_to_check),   // dst_service
        __username__,
        kw // not owned
    );

trace_msg0("username: %s", __username__);
print_json2("XXX user_authzs", user_authzs); // TODO TEST

    BOOL allow = FALSE;
    const char *authz_; json_t *jn_allow;
    json_object_foreach(user_authzs, authz_, jn_allow) {
        if(strcmp(authz_, "*")==0 || strcmp(authz_, authz)==0) {
            allow = json_boolean_value(jn_allow)?1:0;
            break;
        }
    }

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(gobj, user_authzs, "user '%s', authz '%s', allow -> %s",
            __username__,
            authz,
            allow?"YES":"NO"
        );
    }

    JSON_DECREF(user_authzs)
    JSON_DECREF(jn_authz_desc)
    KW_DECREF(kw)
    return allow;
}

/***************************************************************************
   Authenticate user
   If we are here is because gobj_service has no authentication parser,
   then use this global parser

    Return json response
        {
            "result": 0,        // 0 successful authentication, -1 error
            "comment": "",
            "username": ""      // username authenticated
        }
***************************************************************************/
PUBLIC json_t *authentication_parser(hgobj gobj_service, json_t *kw, hgobj src)
{
    hgobj gobj = gobj_find_service_by_gclass(C_AUTHZ, TRUE);
    if(!gobj) {
        /*
         *  HACK if this function is called is because the authz system is configured in setup.
         *  If the service is not found, deny all.
         */
        KW_DECREF(kw)
        return json_pack("{s:i, s:s, s:s}",
            "result", -1,
            "username", kw_get_str(0, kw, "username", "", 0),
            "comment", "Authz gclass not found"
        );
    }

    return gobj_authenticate(gobj, kw, src);
}
